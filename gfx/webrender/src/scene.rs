/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use euclid::Size2D;
use fnv::FnvHasher;
use internal_types::DrawListId;
use resource_cache::ResourceCache;
use std::collections::HashMap;
use std::hash::BuildHasherDefault;
use tiling::AuxiliaryListsMap;
use webrender_traits::{AuxiliaryLists, BuiltDisplayList, PipelineId, Epoch};
use webrender_traits::{ColorF, DisplayListId, StackingContext, StackingContextId};
use webrender_traits::{SpecificDisplayListItem};
use webrender_traits::{IframeInfo};
use webrender_traits::{RectangleDisplayItem, ClipRegion, DisplayItem, SpecificDisplayItem};

/// A representation of the layout within the display port for a given document or iframe.
#[derive(Debug)]
pub struct ScenePipeline {
    pub pipeline_id: PipelineId,
    pub epoch: Epoch,
    pub background_draw_list: Option<DrawListId>,
    pub root_stacking_context_id: StackingContextId,
    pub viewport_size: Size2D<f32>,
}

/// A complete representation of the layout bundling visible pipelines together.
pub struct Scene {
    pub root_pipeline_id: Option<PipelineId>,
    pub pipeline_map: HashMap<PipelineId, ScenePipeline, BuildHasherDefault<FnvHasher>>,
    pub pipeline_sizes: HashMap<PipelineId, Size2D<f32>>,
    pub pipeline_auxiliary_lists: AuxiliaryListsMap,
    pub display_list_map: HashMap<DisplayListId, SceneDisplayList, BuildHasherDefault<FnvHasher>>,
    pub stacking_context_map: HashMap<StackingContextId, SceneStackingContext, BuildHasherDefault<FnvHasher>>,
}

#[derive(Clone, Debug)]
pub enum SpecificSceneItem {
    DrawList(DrawListId),
    StackingContext(StackingContextId, PipelineId),
    Iframe(IframeInfo),
}

#[derive(Clone, Debug)]
pub struct SceneItem {
    pub specific: SpecificSceneItem,
}

/// Similar to webrender_traits::DisplayList internal to WebRender.
pub struct SceneDisplayList {
    pub pipeline_id: PipelineId,
    pub epoch: Epoch,
    pub items: Vec<SceneItem>,
}

pub struct SceneStackingContext {
    pub pipeline_id: PipelineId,
    pub epoch: Epoch,
    pub stacking_context: StackingContext,
    pub stacking_context_id: StackingContextId,
}

impl Scene {
    pub fn new() -> Scene {
        Scene {
            root_pipeline_id: None,
            pipeline_sizes: HashMap::new(),
            pipeline_map: HashMap::with_hasher(Default::default()),
            pipeline_auxiliary_lists: HashMap::with_hasher(Default::default()),
            display_list_map: HashMap::with_hasher(Default::default()),
            stacking_context_map: HashMap::with_hasher(Default::default()),
        }
    }

    pub fn add_display_list(&mut self,
                            id: DisplayListId,
                            pipeline_id: PipelineId,
                            epoch: Epoch,
                            built_display_list: BuiltDisplayList,
                            resource_cache: &mut ResourceCache) {
        let items = built_display_list.display_list_items().iter().map(|item| {
            match item.specific {
                SpecificDisplayListItem::DrawList(ref info) => {
                    let draw_list_id = resource_cache.add_draw_list(
                        built_display_list.display_items(&info.items).to_vec(),
                        pipeline_id);
                    SceneItem {
                        specific: SpecificSceneItem::DrawList(draw_list_id)
                    }
                }
                SpecificDisplayListItem::StackingContext(ref info) => {
                    SceneItem {
                        specific: SpecificSceneItem::StackingContext(info.id, pipeline_id)
                    }
                }
                SpecificDisplayListItem::Iframe(ref info) => {
                    SceneItem {
                        specific: SpecificSceneItem::Iframe(*info)
                    }
                }
            }
        }).collect();

        let display_list = SceneDisplayList {
            pipeline_id: pipeline_id,
            epoch: epoch,
            items: items,
        };

        self.display_list_map.insert(id, display_list);
    }

    pub fn add_stacking_context(&mut self,
                                id: StackingContextId,
                                pipeline_id: PipelineId,
                                epoch: Epoch,
                                stacking_context: StackingContext) {
        let stacking_context = SceneStackingContext {
            pipeline_id: pipeline_id,
            epoch: epoch,
            stacking_context: stacking_context,
            stacking_context_id: id,
        };

        self.stacking_context_map.insert(id, stacking_context);
    }

    pub fn set_root_pipeline_id(&mut self, pipeline_id: PipelineId) {
        self.root_pipeline_id = Some(pipeline_id);
    }

    pub fn set_root_stacking_context(&mut self,
                                     pipeline_id: PipelineId,
                                     epoch: Epoch,
                                     stacking_context_id: StackingContextId,
                                     background_color: ColorF,
                                     viewport_size: Size2D<f32>,
                                     resource_cache: &mut ResourceCache,
                                     auxiliary_lists: AuxiliaryLists) {
        self.pipeline_auxiliary_lists.insert(pipeline_id, auxiliary_lists);

        let old_display_list_keys: Vec<_> = self.display_list_map.iter()
                                                .filter(|&(_, ref v)| {
                                                    v.pipeline_id == pipeline_id &&
                                                    v.epoch < epoch
                                                })
                                                .map(|(k, _)| k.clone())
                                                .collect();

        // Remove any old draw lists and display lists for this pipeline
        for key in old_display_list_keys {
            let display_list = self.display_list_map.remove(&key).unwrap();
            for item in display_list.items {
                match item.specific {
                    SpecificSceneItem::DrawList(draw_list_id) => {
                        resource_cache.remove_draw_list(draw_list_id);
                    }
                    SpecificSceneItem::StackingContext(..) |
                    SpecificSceneItem::Iframe(..) => {}
                }
            }
        }

        let old_stacking_context_keys: Vec<_> = self.stacking_context_map.iter()
                                                                         .filter(|&(_, ref v)| {
                                                                             v.pipeline_id == pipeline_id &&
                                                                             v.epoch < epoch
                                                                         })
                                                                         .map(|(k, _)| k.clone())
                                                                         .collect();

        // Remove any old draw lists and display lists for this pipeline
        for key in old_stacking_context_keys {
            self.stacking_context_map.remove(&key).unwrap();

            // TODO: Could remove all associated DLs here,
            //       and then the above code could just be a debug assert check...
        }

        let background_draw_list = if background_color.a > 0.0 {
            let overflow = self.stacking_context_map[&stacking_context_id].stacking_context.overflow;

            let rectangle_item = RectangleDisplayItem {
                color: background_color,
            };
            let root_bg_color_item = DisplayItem {
                item: SpecificDisplayItem::Rectangle(rectangle_item),
                rect: overflow,
                clip: ClipRegion::simple(&overflow),
            };

            let draw_list_id = resource_cache.add_draw_list(vec![root_bg_color_item], pipeline_id);
            Some(draw_list_id)
        } else {
            None
        };

        let new_pipeline = ScenePipeline {
            pipeline_id: pipeline_id,
            epoch: epoch,
            background_draw_list: background_draw_list,
            root_stacking_context_id: stacking_context_id,
            viewport_size: viewport_size,
        };

        if let Some(old_pipeline) = self.pipeline_map.insert(pipeline_id, new_pipeline) {
            if let Some(background_draw_list) = old_pipeline.background_draw_list {
                resource_cache.remove_draw_list(background_draw_list);
            }
        }
    }
}
