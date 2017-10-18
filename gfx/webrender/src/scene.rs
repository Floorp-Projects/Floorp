/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{BuiltDisplayList, ColorF, DynamicProperties, Epoch, LayerSize, LayoutSize};
use api::{FilterOp, LayoutTransform, PipelineId, PropertyBinding, PropertyBindingId};
use api::{ItemRange, MixBlendMode, StackingContext};
use internal_types::FastHashMap;

/// Stores a map of the animated property bindings for the current display list. These
/// can be used to animate the transform and/or opacity of a display list without
/// re-submitting the display list itself.
pub struct SceneProperties {
    transform_properties: FastHashMap<PropertyBindingId, LayoutTransform>,
    float_properties: FastHashMap<PropertyBindingId, f32>,
}

impl SceneProperties {
    pub fn new() -> SceneProperties {
        SceneProperties {
            transform_properties: FastHashMap::default(),
            float_properties: FastHashMap::default(),
        }
    }

    /// Set the current property list for this display list.
    pub fn set_properties(&mut self, properties: DynamicProperties) {
        self.transform_properties.clear();
        self.float_properties.clear();

        for property in properties.transforms {
            self.transform_properties
                .insert(property.key.id, property.value);
        }

        for property in properties.floats {
            self.float_properties
                .insert(property.key.id, property.value);
        }
    }

    /// Get the current value for a transform property.
    pub fn resolve_layout_transform(
        &self,
        property: Option<&PropertyBinding<LayoutTransform>>,
    ) -> LayoutTransform {
        let property = match property {
            Some(property) => property,
            None => return LayoutTransform::identity(),
        };

        match *property {
            PropertyBinding::Value(matrix) => matrix,
            PropertyBinding::Binding(ref key) => self.transform_properties
                .get(&key.id)
                .cloned()
                .unwrap_or_else(|| {
                    warn!("Property binding {:?} has an invalid value.", key);
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
                    warn!("Property binding {:?} has an invalid value.", key);
                    default_value
                }),
        }
    }
}

/// A representation of the layout within the display port for a given document or iframe.
pub struct ScenePipeline {
    pub pipeline_id: PipelineId,
    pub epoch: Epoch,
    pub viewport_size: LayerSize,
    pub content_size: LayoutSize,
    pub background_color: Option<ColorF>,
    pub display_list: BuiltDisplayList,
}

/// A complete representation of the layout bundling visible pipelines together.
pub struct Scene {
    pub root_pipeline_id: Option<PipelineId>,
    pub pipelines: FastHashMap<PipelineId, ScenePipeline>,
    pub properties: SceneProperties,
}

impl Scene {
    pub fn new() -> Scene {
        Scene {
            root_pipeline_id: None,
            pipelines: FastHashMap::default(),
            properties: SceneProperties::new(),
        }
    }

    pub fn set_root_pipeline_id(&mut self, pipeline_id: PipelineId) {
        self.root_pipeline_id = Some(pipeline_id);
    }

    pub fn set_display_list(
        &mut self,
        pipeline_id: PipelineId,
        epoch: Epoch,
        display_list: BuiltDisplayList,
        background_color: Option<ColorF>,
        viewport_size: LayerSize,
        content_size: LayoutSize,
    ) {
        let new_pipeline = ScenePipeline {
            pipeline_id,
            epoch,
            viewport_size,
            content_size,
            background_color,
            display_list,
        };

        self.pipelines.insert(pipeline_id, new_pipeline);
    }

    pub fn remove_pipeline(&mut self, pipeline_id: PipelineId) {
        if self.root_pipeline_id == Some(pipeline_id) {
            self.root_pipeline_id = None;
        }
        self.pipelines.remove(&pipeline_id);
    }
}

pub trait FilterOpHelpers {
    fn resolve(self, properties: &SceneProperties) -> FilterOp;
    fn is_noop(&self) -> bool;
}

impl FilterOpHelpers for FilterOp {
    fn resolve(self, properties: &SceneProperties) -> FilterOp {
        match self {
            FilterOp::Opacity(ref value) => {
                let amount = properties.resolve_float(value, 1.0);
                FilterOp::Opacity(PropertyBinding::Value(amount))
            }
            _ => self,
        }
    }

    fn is_noop(&self) -> bool {
        match *self {
            FilterOp::Blur(length) => length == 0.0,
            FilterOp::Brightness(amount) => amount == 1.0,
            FilterOp::Contrast(amount) => amount == 1.0,
            FilterOp::Grayscale(amount) => amount == 0.0,
            FilterOp::HueRotate(amount) => amount == 0.0,
            FilterOp::Invert(amount) => amount == 0.0,
            FilterOp::Opacity(value) => match value {
                PropertyBinding::Value(amount) => amount == 1.0,
                PropertyBinding::Binding(..) => {
                    panic!("bug: binding value should be resolved");
                }
            },
            FilterOp::Saturate(amount) => amount == 1.0,
            FilterOp::Sepia(amount) => amount == 0.0,
        }
    }
}

pub trait StackingContextHelpers {
    fn mix_blend_mode_for_compositing(&self) -> Option<MixBlendMode>;
    fn filter_ops_for_compositing(
        &self,
        display_list: &BuiltDisplayList,
        input_filters: ItemRange<FilterOp>,
        properties: &SceneProperties,
    ) -> Vec<FilterOp>;
}

impl StackingContextHelpers for StackingContext {
    fn mix_blend_mode_for_compositing(&self) -> Option<MixBlendMode> {
        match self.mix_blend_mode {
            MixBlendMode::Normal => None,
            _ => Some(self.mix_blend_mode),
        }
    }

    fn filter_ops_for_compositing(
        &self,
        display_list: &BuiltDisplayList,
        input_filters: ItemRange<FilterOp>,
        properties: &SceneProperties,
    ) -> Vec<FilterOp> {
        let mut filters = vec![];
        for filter in display_list.get(input_filters) {
            let filter = filter.resolve(properties);
            if filter.is_noop() {
                continue;
            }
            filters.push(filter);
        }
        filters
    }
}
