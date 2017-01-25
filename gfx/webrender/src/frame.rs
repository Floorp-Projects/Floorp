/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use app_units::Au;
use fnv::FnvHasher;
use internal_types::{ANGLE_FLOAT_TO_FIXED, AxisDirection};
use internal_types::{CompositionOp};
use internal_types::{LowLevelFilterOp};
use internal_types::{RendererFrame};
use layer::Layer;
use resource_cache::ResourceCache;
use scene::Scene;
use scroll_tree::{ScrollTree, ScrollStates};
use std::collections::HashMap;
use std::hash::BuildHasherDefault;
use tiling::{AuxiliaryListsMap, FrameBuilder, FrameBuilderConfig, PrimitiveFlags};
use webrender_traits::{AuxiliaryLists, ClipRegion, ColorF, DisplayItem, Epoch, FilterOp};
use webrender_traits::{LayerPoint, LayerRect, LayerSize, LayerToScrollTransform};
use webrender_traits::{MixBlendMode, PipelineId, ScrollEventPhase, ScrollLayerId, ScrollLayerState};
use webrender_traits::{ScrollLocation, ScrollPolicy, ServoScrollRootId, SpecificDisplayItem};
use webrender_traits::{StackingContext, WorldPoint};

#[derive(Copy, Clone, PartialEq, PartialOrd, Debug)]
pub struct FrameId(pub u32);

static DEFAULT_SCROLLBAR_COLOR: ColorF = ColorF { r: 0.3, g: 0.3, b: 0.3, a: 0.6 };

struct FlattenContext<'a> {
    scene: &'a Scene,
    pipeline_sizes: &'a mut HashMap<PipelineId, LayerSize>,
    builder: &'a mut FrameBuilder,
}

// TODO: doc
pub struct Frame {
    pub scroll_tree: ScrollTree,
    pub pipeline_epoch_map: HashMap<PipelineId, Epoch, BuildHasherDefault<FnvHasher>>,
    pub pipeline_auxiliary_lists: AuxiliaryListsMap,
    id: FrameId,
    debug: bool,
    frame_builder_config: FrameBuilderConfig,
    frame_builder: Option<FrameBuilder>,
}

trait DisplayListHelpers {
    fn starting_stacking_context<'a>(&'a self) -> Option<(&'a StackingContext, &'a ClipRegion)>;
}

impl DisplayListHelpers for Vec<DisplayItem> {
    fn starting_stacking_context<'a>(&'a self) -> Option<(&'a StackingContext, &'a ClipRegion)> {
        self.first().and_then(|item| match item.item {
            SpecificDisplayItem::PushStackingContext(ref specific_item) => {
                Some((&specific_item.stacking_context, &item.clip))
            },
            _ => None,
        })
    }
}

trait StackingContextHelpers {
    fn needs_composition_operation_for_mix_blend_mode(&self) -> bool;
    fn composition_operations(&self, auxiliary_lists: &AuxiliaryLists) -> Vec<CompositionOp>;
}

impl StackingContextHelpers for StackingContext {
    fn needs_composition_operation_for_mix_blend_mode(&self) -> bool {
        match self.mix_blend_mode {
            MixBlendMode::Normal => false,
            MixBlendMode::Multiply |
            MixBlendMode::Screen |
            MixBlendMode::Overlay |
            MixBlendMode::Darken |
            MixBlendMode::Lighten |
            MixBlendMode::ColorDodge |
            MixBlendMode::ColorBurn |
            MixBlendMode::HardLight |
            MixBlendMode::SoftLight |
            MixBlendMode::Difference |
            MixBlendMode::Exclusion |
            MixBlendMode::Hue |
            MixBlendMode::Saturation |
            MixBlendMode::Color |
            MixBlendMode::Luminosity => true,
        }
    }

    fn composition_operations(&self, auxiliary_lists: &AuxiliaryLists) -> Vec<CompositionOp> {
        let mut composition_operations = vec![];
        if self.needs_composition_operation_for_mix_blend_mode() {
            composition_operations.push(CompositionOp::MixBlend(self.mix_blend_mode));
        }
        for filter in auxiliary_lists.filters(&self.filters) {
            match *filter {
                FilterOp::Blur(radius) => {
                    composition_operations.push(CompositionOp::Filter(LowLevelFilterOp::Blur(
                        radius,
                        AxisDirection::Horizontal)));
                    composition_operations.push(CompositionOp::Filter(LowLevelFilterOp::Blur(
                        radius,
                        AxisDirection::Vertical)));
                }
                FilterOp::Brightness(amount) => {
                    composition_operations.push(CompositionOp::Filter(
                            LowLevelFilterOp::Brightness(Au::from_f32_px(amount))));
                }
                FilterOp::Contrast(amount) => {
                    composition_operations.push(CompositionOp::Filter(
                            LowLevelFilterOp::Contrast(Au::from_f32_px(amount))));
                }
                FilterOp::Grayscale(amount) => {
                    composition_operations.push(CompositionOp::Filter(
                            LowLevelFilterOp::Grayscale(Au::from_f32_px(amount))));
                }
                FilterOp::HueRotate(angle) => {
                    composition_operations.push(CompositionOp::Filter(
                            LowLevelFilterOp::HueRotate(f32::round(
                                    angle * ANGLE_FLOAT_TO_FIXED) as i32)));
                }
                FilterOp::Invert(amount) => {
                    composition_operations.push(CompositionOp::Filter(
                            LowLevelFilterOp::Invert(Au::from_f32_px(amount))));
                }
                FilterOp::Opacity(amount) => {
                    composition_operations.push(CompositionOp::Filter(
                            LowLevelFilterOp::Opacity(Au::from_f32_px(amount))));
                }
                FilterOp::Saturate(amount) => {
                    composition_operations.push(CompositionOp::Filter(
                            LowLevelFilterOp::Saturate(Au::from_f32_px(amount))));
                }
                FilterOp::Sepia(amount) => {
                    composition_operations.push(CompositionOp::Filter(
                            LowLevelFilterOp::Sepia(Au::from_f32_px(amount))));
                }
            }
        }

        composition_operations
    }
}

struct DisplayListTraversal<'a> {
    pub display_list: &'a [DisplayItem],
    pub next_item_index: usize,
}

impl<'a> DisplayListTraversal<'a> {
    pub fn new_skipping_first(display_list: &'a Vec<DisplayItem>) -> DisplayListTraversal {
        DisplayListTraversal {
            display_list: display_list,
            next_item_index: 1,
        }
    }

    pub fn skip_current_stacking_context(&mut self) {
        for item in self {
            if item.item == SpecificDisplayItem::PopStackingContext {
                return;
            }
        }
    }

    pub fn current_stacking_context_empty(&self) -> bool {
        match self.peek() {
            Some(item) => item.item == SpecificDisplayItem::PopStackingContext,
            None => true,
        }
    }

    fn peek(&self) -> Option<&'a DisplayItem> {
        if self.next_item_index >= self.display_list.len() {
            return None
        }
        Some(&self.display_list[self.next_item_index])
    }
}

impl<'a> Iterator for DisplayListTraversal<'a> {
    type Item = &'a DisplayItem;

    fn next(&mut self) -> Option<&'a DisplayItem> {
        if self.next_item_index >= self.display_list.len() {
            return None
        }

        let item = &self.display_list[self.next_item_index];
        self.next_item_index += 1;
        Some(item)
    }
}

impl Frame {
    pub fn new(debug: bool, config: FrameBuilderConfig) -> Frame {
        Frame {
            pipeline_epoch_map: HashMap::with_hasher(Default::default()),
            pipeline_auxiliary_lists: HashMap::with_hasher(Default::default()),
            scroll_tree: ScrollTree::new(),
            id: FrameId(0),
            debug: debug,
            frame_builder: None,
            frame_builder_config: config,
        }
    }

    pub fn reset(&mut self) -> ScrollStates {
        self.pipeline_epoch_map.clear();

        // Advance to the next frame.
        self.id.0 += 1;

        self.scroll_tree.drain()
    }

    pub fn get_scroll_layer_state(&self) -> Vec<ScrollLayerState> {
        self.scroll_tree.get_scroll_layer_state()
    }

    /// Returns true if any layers actually changed position or false otherwise.
    pub fn scroll_layers(&mut self,
                         origin: LayerPoint,
                         pipeline_id: PipelineId,
                         scroll_root_id: ServoScrollRootId)
                          -> bool {
        self.scroll_tree.scroll_layers(origin, pipeline_id, scroll_root_id)
    }

    /// Returns true if any layers actually changed position or false otherwise.
    pub fn scroll(&mut self,
                  scroll_location: ScrollLocation,
                  cursor: WorldPoint,
                  phase: ScrollEventPhase)
                  -> bool {
        self.scroll_tree.scroll(scroll_location, cursor, phase,)
    }

    pub fn tick_scrolling_bounce_animations(&mut self) {
        self.scroll_tree.tick_scrolling_bounce_animations();
    }

    pub fn create(&mut self,
                  scene: &Scene,
                  pipeline_sizes: &mut HashMap<PipelineId, LayerSize>) {
        let root_pipeline_id = match scene.root_pipeline_id {
            Some(root_pipeline_id) => root_pipeline_id,
            None => return,
        };

        let root_pipeline = match scene.pipeline_map.get(&root_pipeline_id) {
            Some(root_pipeline) => root_pipeline,
            None => return,
        };

        let display_list = scene.display_lists.get(&root_pipeline_id);
        let display_list = match display_list {
            Some(display_list) => display_list,
            None => return,
        };

        let old_scrolling_states = self.reset();
        self.pipeline_auxiliary_lists = scene.pipeline_auxiliary_lists.clone();

        self.pipeline_epoch_map.insert(root_pipeline_id, root_pipeline.epoch);

        let (root_stacking_context, root_clip) = match display_list.starting_stacking_context() {
            Some(some) => some,
            None => {
                warn!("Pipeline display list does not start with a stacking context.");
                return;
            }
        };

        // Insert global position: fixed elements layer
        debug_assert!(self.scroll_tree.layers.is_empty());
        let root_scroll_layer_id = ScrollLayerId::root(root_pipeline_id);
        let root_fixed_layer_id = ScrollLayerId::create_fixed(root_pipeline_id);
        let root_viewport = LayerRect::new(LayerPoint::zero(), root_pipeline.viewport_size);
        let layer = Layer::new(&root_viewport,
                               root_clip.main.size,
                               &LayerToScrollTransform::identity(),
                               root_pipeline_id);
        self.scroll_tree.add_layer(layer.clone(), root_fixed_layer_id, None);
        self.scroll_tree.add_layer(layer, root_scroll_layer_id, None);
        self.scroll_tree.root_scroll_layer_id = Some(root_scroll_layer_id);

        let background_color = root_pipeline.background_color.and_then(|color| {
            if color.a > 0.0 {
                Some(color)
            } else {
                None
            }
        });

        let mut frame_builder = FrameBuilder::new(root_pipeline.viewport_size,
                                                  background_color,
                                                  self.debug,
                                                  self.frame_builder_config);

        {
            let mut context = FlattenContext {
                scene: scene,
                pipeline_sizes: pipeline_sizes,
                builder: &mut frame_builder,
            };

            let mut traversal = DisplayListTraversal::new_skipping_first(display_list);
            self.flatten_stacking_context(&mut traversal,
                                          root_pipeline_id,
                                          &mut context,
                                          root_fixed_layer_id,
                                          root_scroll_layer_id,
                                          LayerToScrollTransform::identity(),
                                          0,
                                          &root_stacking_context,
                                          root_clip);
        }

        self.frame_builder = Some(frame_builder);
        self.scroll_tree.finalize_and_apply_pending_scroll_offsets(old_scrolling_states);
    }

    fn flatten_scroll_layer<'a>(&mut self,
                                traversal: &mut DisplayListTraversal<'a>,
                                pipeline_id: PipelineId,
                                context: &mut FlattenContext,
                                current_fixed_layer_id: ScrollLayerId,
                                mut current_scroll_layer_id: ScrollLayerId,
                                layer_relative_transform: LayerToScrollTransform,
                                level: i32,
                                clip: &LayerRect,
                                content_size: &LayerSize,
                                new_scroll_layer_id: ScrollLayerId) {
        // Avoid doing unnecessary work for empty stacking contexts.
        if traversal.current_stacking_context_empty() {
            traversal.skip_current_stacking_context();
            return;
        }

        let layer = Layer::new(&clip, *content_size, &layer_relative_transform, pipeline_id);
        self.scroll_tree.add_layer(layer, new_scroll_layer_id, Some(current_scroll_layer_id));
        current_scroll_layer_id = new_scroll_layer_id;

        let layer_rect = LayerRect::new(LayerPoint::zero(),
                                        LayerSize::new(content_size.width + clip.origin.x,
                                                       content_size.height + clip.origin.y));
        context.builder.push_layer(layer_rect,
                                   &ClipRegion::simple(&layer_rect),
                                   LayerToScrollTransform::identity(),
                                   pipeline_id,
                                   current_scroll_layer_id,
                                   &[]);

        self.flatten_items(traversal,
                           pipeline_id,
                           context,
                           current_fixed_layer_id,
                           current_scroll_layer_id,
                           LayerToScrollTransform::identity(),
                           level);

        context.builder.pop_layer();
    }

    fn flatten_stacking_context<'a>(&mut self,
                                    traversal: &mut DisplayListTraversal<'a>,
                                    pipeline_id: PipelineId,
                                    context: &mut FlattenContext,
                                    current_fixed_layer_id: ScrollLayerId,
                                    current_scroll_layer_id: ScrollLayerId,
                                    layer_relative_transform: LayerToScrollTransform,
                                    level: i32,
                                    stacking_context: &StackingContext,
                                    clip_region: &ClipRegion) {
        // Avoid doing unnecessary work for empty stacking contexts.
        if traversal.current_stacking_context_empty() {
            traversal.skip_current_stacking_context();
            return;
        }

        let composition_operations = {
            let auxiliary_lists = self.pipeline_auxiliary_lists
                                      .get(&pipeline_id)
                                      .expect("No auxiliary lists?!");
            stacking_context.composition_operations(auxiliary_lists)
        };

        // Detect composition operations that will make us invisible.
        for composition_operation in &composition_operations {
            match *composition_operation {
                CompositionOp::Filter(LowLevelFilterOp::Opacity(Au(0))) => {
                    traversal.skip_current_stacking_context();
                    return;
                }
                _ => {}
            }
        }

        let transform = layer_relative_transform.pre_translated(stacking_context.bounds.origin.x,
                                                                stacking_context.bounds.origin.y,
                                                                0.0)
                                                .pre_mul(&stacking_context.transform)
                                                .pre_mul(&stacking_context.perspective);

        // Build world space transform
        let scroll_layer_id = match stacking_context.scroll_policy {
            ScrollPolicy::Fixed => current_fixed_layer_id,
            ScrollPolicy::Scrollable => current_scroll_layer_id,
        };

        if level == 0 {
            if let Some(pipeline) = context.scene.pipeline_map.get(&pipeline_id) {
                if let Some(bg_color) = pipeline.background_color {

                    // Adding a dummy layer for this rectangle in order to disable clipping.
                    let no_clip = ClipRegion::simple(&clip_region.main);
                    context.builder.push_layer(clip_region.main,
                                               &no_clip,
                                               transform,
                                               pipeline_id,
                                               scroll_layer_id,
                                               &composition_operations);

                    //Note: we don't use the original clip region here,
                    // it's already processed by the layer we just pushed.
                    context.builder.add_solid_rectangle(&clip_region.main,
                                                        &no_clip,
                                                        &bg_color,
                                                        PrimitiveFlags::None);

                    context.builder.pop_layer();
                }
            }
        }

         // TODO(gw): Int with overflow etc
        context.builder.push_layer(clip_region.main,
                                   &clip_region,
                                   transform,
                                   pipeline_id,
                                   scroll_layer_id,
                                   &composition_operations);

        self.flatten_items(traversal,
                           pipeline_id,
                           context,
                           current_fixed_layer_id,
                           current_scroll_layer_id,
                           transform,
                           level);

        if level == 0 && self.frame_builder_config.enable_scrollbars {
            let scrollbar_rect = LayerRect::new(LayerPoint::zero(), LayerSize::new(10.0, 70.0));
            context.builder.add_solid_rectangle(
                &scrollbar_rect,
                &ClipRegion::simple(&scrollbar_rect),
                &DEFAULT_SCROLLBAR_COLOR,
                PrimitiveFlags::Scrollbar(self.scroll_tree.root_scroll_layer_id.unwrap(), 4.0));
        }

        context.builder.pop_layer();
    }

    fn flatten_iframe<'a>(&mut self,
                          pipeline_id: PipelineId,
                          bounds: &LayerRect,
                          context: &mut FlattenContext,
                          current_scroll_layer_id: ScrollLayerId,
                          layer_relative_transform: LayerToScrollTransform) {
        context.pipeline_sizes.insert(pipeline_id, bounds.size);

        let pipeline = match context.scene.pipeline_map.get(&pipeline_id) {
            Some(pipeline) => pipeline,
            None => return,
        };

        let display_list = context.scene.display_lists.get(&pipeline_id);
        let display_list = match display_list {
            Some(display_list) => display_list,
            None => return,
        };

        let (iframe_stacking_context, iframe_clip) = match display_list.starting_stacking_context() {
            Some(some) => some,
            None => {
                warn!("Pipeline display list does not start with a stacking context.");
                return;
            }
        };

        self.pipeline_epoch_map.insert(pipeline_id, pipeline.epoch);

        let iframe_rect = &LayerRect::new(LayerPoint::zero(), bounds.size);
        let transform = layer_relative_transform.pre_translated(bounds.origin.x,
                                                                bounds.origin.y,
                                                                0.0);

        let iframe_fixed_layer_id = ScrollLayerId::create_fixed(pipeline_id);
        let iframe_scroll_layer_id = ScrollLayerId::root(pipeline_id);

        let layer = Layer::new(iframe_rect,
                               iframe_clip.main.size,
                               &transform,
                               pipeline_id);
        self.scroll_tree.add_layer(layer.clone(), iframe_fixed_layer_id, None);
        self.scroll_tree.add_layer(layer, iframe_scroll_layer_id, Some(current_scroll_layer_id));

        let mut traversal = DisplayListTraversal::new_skipping_first(display_list);

        self.flatten_stacking_context(&mut traversal,
                                      pipeline_id,
                                      context,
                                      iframe_fixed_layer_id,
                                      iframe_scroll_layer_id,
                                      LayerToScrollTransform::identity(),
                                      0,
                                      &iframe_stacking_context,
                                      iframe_clip);
    }

    fn flatten_items<'a>(&mut self,
                         traversal: &mut DisplayListTraversal<'a>,
                         pipeline_id: PipelineId,
                         context: &mut FlattenContext,
                         current_fixed_layer_id: ScrollLayerId,
                         current_scroll_layer_id: ScrollLayerId,
                         layer_relative_transform: LayerToScrollTransform,
                         level: i32) {
        while let Some(item) = traversal.next() {
            match item.item {
                SpecificDisplayItem::WebGL(ref info) => {
                    context.builder.add_webgl_rectangle(item.rect,
                                                        &item.clip, info.context_id);
                }
                SpecificDisplayItem::Image(ref info) => {
                    context.builder.add_image(item.rect,
                                              &item.clip,
                                              &info.stretch_size,
                                              &info.tile_spacing,
                                              info.image_key,
                                              info.image_rendering);
                }
                SpecificDisplayItem::YuvImage(ref info) => {
                    context.builder.add_yuv_image(item.rect,
                                                  &item.clip,
                                                  info.y_image_key,
                                                  info.u_image_key,
                                                  info.v_image_key,
                                                  info.color_space);
                }
                SpecificDisplayItem::Text(ref text_info) => {
                    context.builder.add_text(item.rect,
                                             &item.clip,
                                             text_info.font_key,
                                             text_info.size,
                                             text_info.blur_radius,
                                             &text_info.color,
                                             text_info.glyphs);
                }
                SpecificDisplayItem::Rectangle(ref info) => {
                    context.builder.add_solid_rectangle(&item.rect,
                                                        &item.clip,
                                                        &info.color,
                                                        PrimitiveFlags::None);
                }
                SpecificDisplayItem::Gradient(ref info) => {
                    context.builder.add_gradient(item.rect,
                                                 &item.clip,
                                                 info.start_point,
                                                 info.end_point,
                                                 info.stops);
                }
                SpecificDisplayItem::RadialGradient(ref info) => {
                    context.builder.add_radial_gradient(item.rect,
                                                        &item.clip,
                                                        info.start_center,
                                                        info.start_radius,
                                                        info.end_center,
                                                        info.end_radius,
                                                        info.stops);
                }
                SpecificDisplayItem::BoxShadow(ref box_shadow_info) => {
                    context.builder.add_box_shadow(&box_shadow_info.box_bounds,
                                                   &item.clip,
                                                   &box_shadow_info.offset,
                                                   &box_shadow_info.color,
                                                   box_shadow_info.blur_radius,
                                                   box_shadow_info.spread_radius,
                                                   box_shadow_info.border_radius,
                                                   box_shadow_info.clip_mode);
                }
                SpecificDisplayItem::Border(ref info) => {
                    context.builder.add_border(item.rect, &item.clip, info);
                }
                SpecificDisplayItem::PushStackingContext(ref info) => {
                    self.flatten_stacking_context(traversal,
                                                  pipeline_id,
                                                  context,
                                                  current_fixed_layer_id,
                                                  current_scroll_layer_id,
                                                  layer_relative_transform,
                                                  level + 1,
                                                  &info.stacking_context,
                                                  &item.clip);
                }
                SpecificDisplayItem::PushScrollLayer(ref info) => {
                    self.flatten_scroll_layer(traversal,
                                              pipeline_id,
                                              context,
                                              current_fixed_layer_id,
                                              current_scroll_layer_id,
                                              layer_relative_transform,
                                              level,
                                              &item.rect,
                                              &info.content_size,
                                              info.id);
                }
                SpecificDisplayItem::Iframe(ref info) => {
                    self.flatten_iframe(info.pipeline_id,
                                        &item.rect,
                                        context,
                                        current_scroll_layer_id,
                                        layer_relative_transform);
                }
                SpecificDisplayItem::PopStackingContext |
                SpecificDisplayItem::PopScrollLayer => return,
            }
        }
    }

    pub fn build(&mut self,
                 resource_cache: &mut ResourceCache,
                 auxiliary_lists_map: &AuxiliaryListsMap,
                 device_pixel_ratio: f32)
                 -> RendererFrame {
        self.scroll_tree.update_all_layer_transforms();
        let frame = self.build_frame(resource_cache,
                                     auxiliary_lists_map,
                                     device_pixel_ratio);
        resource_cache.expire_old_resources(self.id);
        frame
    }

    fn build_frame(&mut self,
                   resource_cache: &mut ResourceCache,
                   auxiliary_lists_map: &AuxiliaryListsMap,
                   device_pixel_ratio: f32) -> RendererFrame {
        let mut frame_builder = self.frame_builder.take();
        let frame = frame_builder.as_mut().map(|builder|
            builder.build(resource_cache,
                          self.id,
                          &self.scroll_tree,
                          auxiliary_lists_map,
                          device_pixel_ratio)
        );
        self.frame_builder = frame_builder;

        let layers_bouncing_back = self.scroll_tree.collect_layers_bouncing_back();
        RendererFrame::new(self.pipeline_epoch_map.clone(), layers_bouncing_back, frame)
    }
}
