/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use euclid::Size2D;
use fnv::FnvHasher;
use std::collections::HashMap;
use std::hash::BuildHasherDefault;
use tiling::AuxiliaryListsMap;
use webrender_traits::{AuxiliaryLists, BuiltDisplayList, PipelineId, Epoch, ColorF};
use webrender_traits::{DisplayItem, SpecificDisplayItem, StackingContext};

trait DisplayListHelpers {
    fn starting_stacking_context<'a>(&'a self) -> Option<&'a StackingContext>;
}

impl DisplayListHelpers for Vec<DisplayItem> {
    fn starting_stacking_context<'a>(&'a self) -> Option<&'a StackingContext> {
        self.first().and_then(|item| match item.item {
            SpecificDisplayItem::PushStackingContext(ref item) => Some(&item.stacking_context),
            _ => None,
        })
    }
}

/// A representation of the layout within the display port for a given document or iframe.
#[derive(Debug)]
pub struct ScenePipeline {
    pub pipeline_id: PipelineId,
    pub epoch: Epoch,
    pub viewport_size: Size2D<f32>,
    pub background_color: ColorF,
}

/// A complete representation of the layout bundling visible pipelines together.
pub struct Scene {
    pub root_pipeline_id: Option<PipelineId>,
    pub pipeline_map: HashMap<PipelineId, ScenePipeline, BuildHasherDefault<FnvHasher>>,
    pub pipeline_sizes: HashMap<PipelineId, Size2D<f32>>,
    pub pipeline_auxiliary_lists: AuxiliaryListsMap,
    pub display_lists: HashMap<PipelineId, Vec<DisplayItem>, BuildHasherDefault<FnvHasher>>,
}

impl Scene {
    pub fn new() -> Scene {
        Scene {
            root_pipeline_id: None,
            pipeline_sizes: HashMap::new(),
            pipeline_map: HashMap::with_hasher(Default::default()),
            pipeline_auxiliary_lists: HashMap::with_hasher(Default::default()),
            display_lists: HashMap::with_hasher(Default::default()),
        }
    }

    pub fn set_root_pipeline_id(&mut self, pipeline_id: PipelineId) {
        self.root_pipeline_id = Some(pipeline_id);
    }

    pub fn set_root_display_list(&mut self,
                                 pipeline_id: PipelineId,
                                 epoch: Epoch,
                                 built_display_list: BuiltDisplayList,
                                 background_color: ColorF,
                                 viewport_size: Size2D<f32>,
                                 auxiliary_lists: AuxiliaryLists) {
        self.pipeline_auxiliary_lists.insert(pipeline_id, auxiliary_lists);
        self.display_lists.insert(pipeline_id, built_display_list.all_display_items().to_vec());

        let new_pipeline = ScenePipeline {
            pipeline_id: pipeline_id,
            epoch: epoch,
            viewport_size: viewport_size,
            background_color: background_color,
        };

        self.pipeline_map.insert(pipeline_id, new_pipeline);
    }
}
