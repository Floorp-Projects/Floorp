/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::collections::HashMap;
use webrender::api::{BuiltDisplayList, ColorF, Epoch};
use webrender::api::{LayoutSize, PipelineId};
use webrender::api::{PropertyBinding, PropertyBindingId, LayoutTransform, DynamicProperties};

/// Stores a map of the animated property bindings for the current display list. These
/// can be used to animate the transform and/or opacity of a display list without
/// re-submitting the display list itself.
#[derive(Default)]
pub struct SceneProperties {
    transform_properties: HashMap<PropertyBindingId, LayoutTransform>,
    float_properties: HashMap<PropertyBindingId, f32>,
}

impl SceneProperties {
    /// Set the current property list for this display list.
    pub fn set_properties(&mut self, properties: &DynamicProperties) {
        self.transform_properties.clear();
        self.float_properties.clear();

        for property in &properties.transforms {
            self.transform_properties
                .insert(property.key.id, property.value);
        }

        for property in &properties.floats {
            self.float_properties
                .insert(property.key.id, property.value);
        }
    }

    /// Get the current value for a transform property.
    pub fn resolve_layout_transform(
        &self,
        property: &Option<PropertyBinding<LayoutTransform>>,
    ) -> LayoutTransform {
        let property = match *property {
            Some(property) => property,
            None => return LayoutTransform::identity(),
        };

        match property {
            PropertyBinding::Value(matrix) => matrix,
            PropertyBinding::Binding(ref key) => self.transform_properties
                .get(&key.id)
                .cloned()
                .unwrap_or_else(|| {
                    println!("Property binding {:?} has an invalid value.", key);
                    LayoutTransform::identity()
                }),
        }
    }

    /// Get the current value for a float property.
    pub fn resolve_float(&self, property: &PropertyBinding<f32>, default_value: f32) -> f32 {
        match *property {
            PropertyBinding::Value(value) => value,
            PropertyBinding::Binding(ref key) => self.float_properties
                .get(&key.id)
                .cloned()
                .unwrap_or_else(|| {
                    println!("Property binding {:?} has an invalid value.", key);
                    default_value
                }),
        }
    }
}

/// A representation of the layout within the display port for a given document or iframe.
#[derive(Debug)]
pub struct ScenePipeline {
    pub epoch: Epoch,
    pub viewport_size: LayoutSize,
    pub background_color: Option<ColorF>,
}

/// A complete representation of the layout bundling visible pipelines together.
pub struct Scene {
    pub properties: SceneProperties,
    pub root_pipeline_id: Option<PipelineId>,
    pub pipeline_map: HashMap<PipelineId, ScenePipeline>,
    pub display_lists: HashMap<PipelineId, BuiltDisplayList>,
}

impl Scene {
    pub fn new() -> Scene {
        Scene {
            properties: SceneProperties::default(),
            root_pipeline_id: None,
            pipeline_map: HashMap::default(),
            display_lists: HashMap::default(),
        }
    }

    pub fn set_root_pipeline_id(&mut self, pipeline_id: PipelineId) {
        self.root_pipeline_id = Some(pipeline_id);
    }

    pub fn remove_pipeline(&mut self, pipeline_id: &PipelineId) {
        if self.root_pipeline_id == Some(*pipeline_id) {
            self.root_pipeline_id = None;
        }
        self.pipeline_map.remove(pipeline_id);
        self.display_lists.remove(pipeline_id);
    }

    pub fn begin_display_list(
        &mut self,
        pipeline_id: &PipelineId,
        epoch: &Epoch,
        background_color: &Option<ColorF>,
        viewport_size: &LayoutSize,
    ) {
        let new_pipeline = ScenePipeline {
            epoch: epoch.clone(),
            viewport_size: viewport_size.clone(),
            background_color: background_color.clone(),
        };

        self.pipeline_map.insert(pipeline_id.clone(), new_pipeline);
    }

    pub fn finish_display_list(
        &mut self,
        pipeline_id: PipelineId,
        built_display_list: BuiltDisplayList,
    ) {
        self.display_lists.insert(pipeline_id, built_display_list);
    }
}
