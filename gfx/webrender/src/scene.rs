/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use fnv::FnvHasher;
use std::collections::HashMap;
use std::hash::BuildHasherDefault;
use tiling::AuxiliaryListsMap;
use webrender_traits::{AuxiliaryLists, BuiltDisplayList, PipelineId, Epoch, ColorF};
use webrender_traits::{DisplayItem, DynamicProperties, LayerSize, LayoutTransform};
use webrender_traits::{PropertyBinding, PropertyBindingId};

/// Stores a map of the animated property bindings for the current display list. These
/// can be used to animate the transform and/or opacity of a display list without
/// re-submitting the display list itself.
pub struct SceneProperties {
    transform_properties: HashMap<PropertyBindingId, LayoutTransform>,
    float_properties: HashMap<PropertyBindingId, f32>,
}

impl SceneProperties {
    pub fn new() -> SceneProperties {
        SceneProperties {
            transform_properties: HashMap::default(),
            float_properties: HashMap::default(),
        }
    }

    /// Set the current property list for this display list.
    pub fn set_properties(&mut self, properties: DynamicProperties) {
        self.transform_properties.clear();
        self.float_properties.clear();

        for property in properties.transforms {
            self.transform_properties.insert(property.key.id, property.value);
        }

        for property in properties.floats {
            self.float_properties.insert(property.key.id, property.value);
        }
    }

    /// Get the current value for a transform property.
    pub fn resolve_layout_transform(&self,
                                    property: Option<&PropertyBinding<LayoutTransform>>)
                                    -> LayoutTransform {
        let property = match property {
            Some(property) => property,
            None => return LayoutTransform::identity(),
        };

        match *property {
            PropertyBinding::Value(matrix) => matrix,
            PropertyBinding::Binding(ref key) => {
                self.transform_properties
                    .get(&key.id)
                    .cloned()
                    .unwrap_or_else(|| {
                        warn!("Property binding {:?} has an invalid value.", key);
                        LayoutTransform::identity()
                    })
            }
        }
    }

    /// Get the current value for a float property.
    pub fn resolve_float(&self, property: &PropertyBinding<f32>, default_value: f32) -> f32 {
        match *property {
            PropertyBinding::Value(value) => value,
            PropertyBinding::Binding(ref key) => {
                self.float_properties
                    .get(&key.id)
                    .cloned()
                    .unwrap_or_else(|| {
                        warn!("Property binding {:?} has an invalid value.", key);
                        default_value
                    })
            }
        }
    }
}

/// A representation of the layout within the display port for a given document or iframe.
#[derive(Debug)]
pub struct ScenePipeline {
    pub pipeline_id: PipelineId,
    pub epoch: Epoch,
    pub viewport_size: LayerSize,
    pub background_color: Option<ColorF>,
}

/// A complete representation of the layout bundling visible pipelines together.
pub struct Scene {
    pub root_pipeline_id: Option<PipelineId>,
    pub pipeline_map: HashMap<PipelineId, ScenePipeline, BuildHasherDefault<FnvHasher>>,
    pub pipeline_auxiliary_lists: AuxiliaryListsMap,
    pub display_lists: HashMap<PipelineId, Vec<DisplayItem>, BuildHasherDefault<FnvHasher>>,
    pub properties: SceneProperties,
}

impl Scene {
    pub fn new() -> Scene {
        Scene {
            root_pipeline_id: None,
            pipeline_map: HashMap::default(),
            pipeline_auxiliary_lists: HashMap::default(),
            display_lists: HashMap::default(),
            properties: SceneProperties::new(),
        }
    }

    pub fn set_root_pipeline_id(&mut self, pipeline_id: PipelineId) {
        self.root_pipeline_id = Some(pipeline_id);
    }

    pub fn set_display_list(&mut self,
                            pipeline_id: PipelineId,
                            epoch: Epoch,
                            built_display_list: BuiltDisplayList,
                            background_color: Option<ColorF>,
                            viewport_size: LayerSize,
                            auxiliary_lists: AuxiliaryLists) {

        self.pipeline_auxiliary_lists.insert(pipeline_id, auxiliary_lists);
        self.display_lists.insert(pipeline_id, built_display_list.into_display_items());
        
        let new_pipeline = ScenePipeline {
            pipeline_id: pipeline_id,
            epoch: epoch,
            viewport_size: viewport_size,
            background_color: background_color,
        };

        self.pipeline_map.insert(pipeline_id, new_pipeline);
    }
}
