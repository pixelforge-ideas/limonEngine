//
// Created by engin on 31.03.2019.
//

#ifndef LIMONENGINE_GRAPHICSPIPELINE_H
#define LIMONENGINE_GRAPHICSPIPELINE_H


#include <utility>
#include <vector>
#include <functional>
#include <memory>
#include <Assets/AssetManager.h>

#include "GraphicsPipelineStage.h"
#include "../GameObjects/Light.h"
#include "RenderMethods.h"

class World;
class GraphicsProgram;
class RenderMethodInterface;

class GraphicsPipeline {
public:

    struct StageInfo {
        uint32_t highestPriority = 999;//keeps the highest priority, used for render ordering, which is lower. priority 1 is higher priority then priority 10.
        std::shared_ptr<GraphicsPipelineStage> stage;
        bool clear = false;
        std::vector<RenderMethods::RenderMethod> renderMethods;
        std::unordered_map<std::string, RenderMethodInterface*> externalRenderMethods;
        std::vector<std::string> cameraTags;
        std::vector<std::string> renderTags;

        void addRenderMethod(RenderMethods::RenderMethod method) {
            for (auto iterator = renderMethods.begin(); iterator != renderMethods.end();++iterator) {
                if(iterator->getPriority() > method.getPriority()) {
                    renderMethods.insert(iterator, method);
                    return;
                }
            }
            highestPriority = method.getPriority();
            renderMethods.emplace_back(method);
        }

        void addExternalRenderMethod(const std::string& methodName, RenderMethodInterface* externalMethod) {
            externalRenderMethods[methodName] = externalMethod;
        }

        uint32_t getHighestPriority() const {
            return highestPriority;
        }

        void setHighestPriority(uint32_t highestPriority) {
            //the value set by this implies a dependency of this stage has higher priority, so it raises this stages priority with it
            StageInfo::highestPriority = highestPriority;
        }

        bool serialize(tinyxml2::XMLDocument &document, tinyxml2::XMLElement *parentNode, OptionsUtil::Options *options);
        static bool
        deserialize(tinyxml2::XMLElement *stageInfoElement,
                    std::shared_ptr<AssetManager> assetManager,
                    std::unique_ptr<GraphicsPipeline> &pipeline,
                    const std::vector<std::shared_ptr<Texture>> &textures,
                    GraphicsPipeline::StageInfo &newStageInfo);
        std::vector<std::shared_ptr<GraphicsProgram>> programs;
    };
private:
    friend class SDL2Helper;
    GraphicsPipeline() = default;//used for deserialize
    GraphicsPipeline::StageInfo* lastStageInfo = nullptr;

public:
    static std::vector<std::string> renderMethodNames;//This is not array, because custom effects might be loaded on runtime as extensions.

    explicit GraphicsPipeline(RenderMethods renderMethods) : renderMethods(std::move(renderMethods)) {
        auto tempNameVector = RenderMethodInterface::getRenderMethodNames();
        renderMethodNames.insert(renderMethodNames.end(),
                                 tempNameVector.begin(),
                                 tempNameVector.end());
    }

    const RenderMethods &getRenderMethods() const {
        return renderMethods;
    }


    static const std::vector<std::string> &getRenderMethodNames() {
        return renderMethodNames;
    }

    void addNewStage(const StageInfo& stageInformation) {
        for (const std::string &cameraTag: stageInformation.cameraTags) {
            std::map<std::string, std::vector<std::set<std::string>>>::iterator cameraEntry = cameraTagToRenderTagMap.find(cameraTag);
            if(cameraEntry == cameraTagToRenderTagMap.end() || cameraEntry->second.empty()) {
                std::vector<std::set<std::string>> newTagStruct;
                std::set<std::string> tempRenderTags;
                for(const std::string& renderTag:stageInformation.renderTags) {
                    tempRenderTags.insert(renderTag);
                }
                newTagStruct.push_back(tempRenderTags);
                cameraTagToRenderTagMap[cameraTag] = newTagStruct;
            } else {
                //there is already a camera entry
                std::set<std::string> tempRenderTags;
                for(const std::string& renderTag:stageInformation.renderTags) {
                    tempRenderTags.insert(renderTag);
                }
                cameraTagToRenderTagMap[cameraTag].push_back(tempRenderTags);
            }
        }
        pipelineStages.push_back(stageInformation);
    }

    void addTexture(const std::shared_ptr<Texture>& texture) {
        textures.emplace_back(texture);
    }

    std::vector<StageInfo>& getStages() {
        return pipelineStages;
    };

    std::shared_ptr<Texture> getTexture(uint32_t serializeID) {
        for(std::shared_ptr<Texture> texture:this->textures) {
            if(texture->getSerializeID() == serializeID) {
                return texture;
            }
        }
        return nullptr;
    }

    void initialize();

    inline void render() {
        for(auto& stageInfo:pipelineStages) {
            lastStageInfo = &stageInfo;
            stageInfo.stage->activate(stageInfo.clear);
            for(auto& renderMethod:stageInfo.renderMethods) {
                renderMethod();
            }
        }
    }

    void reActivateLastStage() {
        //if something injected some state changes (Like editor) this will be need
        lastStageInfo->stage->activate(false);
    }

    void finalize();

    bool serialize(const std::string& renderPipelineFileName, OptionsUtil::Options *options);

    static std::unique_ptr<GraphicsPipeline> deserialize(const std::string &graphicsPipelineFileName, GraphicsInterface *graphicsWrapper,  std::shared_ptr<AssetManager>, OptionsUtil::Options *options, RenderMethods renderMethods);

    const std::vector<std::shared_ptr<Texture>> &getTextures() {
        return textures;
    };

    /**
     * Key is the camera tag
     * Value is a list, with each element is one stage
     * In that list, there is a set of tags, that are active for the stage
     * @return
     */
    const std::map<std::string, std::vector<std::set<std::string>>>& getCameraTagToRenderTagSetMap() const {
        return cameraTagToRenderTagMap;
    }

private:
    std::map<std::string, std::vector<std::set<std::string>>> cameraTagToRenderTagMap;//Per stage, we configure camera name(tag) and renderTags(objects to render). We should combine them and make accessible so culling can use the info.
    RenderMethods renderMethods;
    std::vector<StageInfo> pipelineStages;
    std::vector<std::shared_ptr<Texture>> textures;
};

#endif //LIMONENGINE_GRAPHICSPIPELINE_H
