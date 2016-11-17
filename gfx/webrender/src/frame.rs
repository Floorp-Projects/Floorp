/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use app_units::Au;
use euclid::{Matrix4D, Point2D, Point3D, Point4D, Rect, Size2D};
use fnv::FnvHasher;
use geometry::ray_intersects_rect;
use internal_types::{ANGLE_FLOAT_TO_FIXED, AxisDirection};
use internal_types::{CompositionOp};
use internal_types::{LowLevelFilterOp};
use internal_types::{RendererFrame};
use layer::{Layer, ScrollingState};
use resource_cache::ResourceCache;
use scene::Scene;
use std::collections::{HashMap, HashSet};
use std::hash::BuildHasherDefault;
use tiling::{AuxiliaryListsMap, FrameBuilder, FrameBuilderConfig, PrimitiveFlags};
use util::MatrixHelpers;
use webrender_traits::{AuxiliaryLists, PipelineId, Epoch, ScrollPolicy, ScrollLayerId};
use webrender_traits::{ClipRegion, ColorF, DisplayItem, StackingContext, FilterOp, MixBlendMode};
use webrender_traits::{ScrollEventPhase, ScrollLayerInfo, SpecificDisplayItem, ScrollLayerState};

#[cfg(target_os = "macos")]
const CAN_OVERSCROLL: bool = true;

#[cfg(not(target_os = "macos"))]
const CAN_OVERSCROLL: bool = false;

#[derive(Copy, Clone, PartialEq, PartialOrd, Debug)]
pub struct FrameId(pub u32);

static DEFAULT_SCROLLBAR_COLOR: ColorF = ColorF { r: 0.3, g: 0.3, b: 0.3, a: 0.6 };

struct FlattenContext<'a> {
    scene: &'a Scene,
    pipeline_sizes: &'a mut HashMap<PipelineId, Size2D<f32>>,
    builder: &'a mut FrameBuilder,
}

pub type LayerMap = HashMap<ScrollLayerId, Layer, BuildHasherDefault<FnvHasher>>;

// TODO: doc
pub struct Frame {
    pub layers: LayerMap,
    pub pipeline_epoch_map: HashMap<PipelineId, Epoch, BuildHasherDefault<FnvHasher>>,
    pub pipeline_auxiliary_lists: HashMap<PipelineId,
                                          AuxiliaryLists,
                                          BuildHasherDefault<FnvHasher>>,
    pub root_scroll_layer_id: Option<ScrollLayerId>,
    id: FrameId,
    debug: bool,
    frame_builder_config: FrameBuilderConfig,
    frame_builder: Option<FrameBuilder>,
}

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
            layers: HashMap::with_hasher(Default::default()),
            root_scroll_layer_id: None,
            id: FrameId(0),
            debug: debug,
            frame_builder: None,
            frame_builder_config: config,
        }
    }

    pub fn reset(&mut self)
                 -> HashMap<ScrollLayerId, ScrollingState, BuildHasherDefault<FnvHasher>> {
        self.pipeline_epoch_map.clear();

        // Free any render targets from last frame.
        // TODO: This should really re-use existing targets here...
        let mut old_layer_scrolling_states = HashMap::with_hasher(Default::default());
        for (layer_id, old_layer) in &mut self.layers.drain() {
            old_layer_scrolling_states.insert(layer_id, old_layer.scrolling);
        }

        // Advance to the next frame.
        self.id.0 += 1;

        old_layer_scrolling_states
    }

    pub fn get_scroll_layer(&self, cursor: &Point2D<f32>, scroll_layer_id: ScrollLayerId)
                            -> Option<ScrollLayerId> {
        self.layers.get(&scroll_layer_id).and_then(|layer| {
            for child_layer_id in layer.children.iter().rev() {
                if let Some(layer_id) = self.get_scroll_layer(cursor, *child_layer_id) {
                    return Some(layer_id);
                }
            }

            match scroll_layer_id.info {
                ScrollLayerInfo::Fixed => {
                    None
                }
                ScrollLayerInfo::Scrollable(..) => {
                    let inv = layer.world_viewport_transform.inverse().unwrap();
                    let z0 = -10000.0;
                    let z1 =  10000.0;

                    let p0 = inv.transform_point4d(&Point4D::new(cursor.x, cursor.y, z0, 1.0));
                    let p0 = Point3D::new(p0.x / p0.w,
                                          p0.y / p0.w,
                                          p0.z / p0.w);
                    let p1 = inv.transform_point4d(&Point4D::new(cursor.x, cursor.y, z1, 1.0));
                    let p1 = Point3D::new(p1.x / p1.w,
                                          p1.y / p1.w,
                                          p1.z / p1.w);

                    let is_unscrollable = layer.content_size.width <= layer.local_viewport_rect.size.width &&
                        layer.content_size.height <= layer.local_viewport_rect.size.height;
                    if is_unscrollable {
                        None
                    } else {
                        let result = ray_intersects_rect(p0, p1, layer.local_viewport_rect);
                        if result {
                            Some(scroll_layer_id)
                        } else {
                            None
                        }
                    }
                }
            }
        })
    }

    pub fn get_scroll_layer_state(&self) -> Vec<ScrollLayerState> {
        let mut result = vec![];
        for (scroll_layer_id, scroll_layer) in &self.layers {
            match scroll_layer_id.info {
                ScrollLayerInfo::Scrollable(_, servo_scroll_root_id) => {
                    result.push(ScrollLayerState {
                        pipeline_id: scroll_layer.pipeline_id,
                        scroll_root_id: servo_scroll_root_id,
                        scroll_offset: scroll_layer.scrolling.offset,
                    })
                }
                ScrollLayerInfo::Fixed => {}
            }
        }
        result
    }

    /// Returns true if any layers actually changed position or false otherwise.
    pub fn scroll(&mut self,
                  mut delta: Point2D<f32>,
                  cursor: Point2D<f32>,
                  phase: ScrollEventPhase)
                  -> bool {
        let root_scroll_layer_id = match self.root_scroll_layer_id {
            Some(root_scroll_layer_id) => root_scroll_layer_id,
            None => return false,
        };

        let scroll_layer_id = match self.get_scroll_layer(&cursor, root_scroll_layer_id) {
            Some(scroll_layer_id) => scroll_layer_id,
            None => return false,
        };

        let scroll_root_id = match scroll_layer_id.info {
            ScrollLayerInfo::Scrollable(_, scroll_root_id) => scroll_root_id,
            ScrollLayerInfo::Fixed => unreachable!("Tried to scroll a fixed position layer."),
        };

        let mut scrolled_a_layer = false;
        for (layer_id, layer) in self.layers.iter_mut() {
            if layer_id.pipeline_id != scroll_layer_id.pipeline_id {
                continue;
            }

            match layer_id.info {
                ScrollLayerInfo::Scrollable(_, id) if id != scroll_root_id => continue,
                ScrollLayerInfo::Fixed => continue,
                _ => {}
            }

            if layer.scrolling.started_bouncing_back && phase == ScrollEventPhase::Move(false) {
                continue;
            }

            let overscroll_amount = layer.overscroll_amount();
            let overscrolling = CAN_OVERSCROLL && (overscroll_amount.width != 0.0 ||
                                                   overscroll_amount.height != 0.0);
            if overscrolling {
                if overscroll_amount.width != 0.0 {
                    delta.x /= overscroll_amount.width.abs()
                }
                if overscroll_amount.height != 0.0 {
                    delta.y /= overscroll_amount.height.abs()
                }
            }

            let is_unscrollable =
                layer.content_size.width <= layer.local_viewport_rect.size.width &&
                layer.content_size.height <= layer.local_viewport_rect.size.height;

            let original_layer_scroll_offset = layer.scrolling.offset;

            if layer.content_size.width > layer.local_viewport_rect.size.width {
                layer.scrolling.offset.x = layer.scrolling.offset.x + delta.x;
                if is_unscrollable || !CAN_OVERSCROLL {
                    layer.scrolling.offset.x = layer.scrolling.offset.x.min(0.0);
                    layer.scrolling.offset.x =
                        layer.scrolling.offset.x.max(-layer.content_size.width +
                                                     layer.local_viewport_rect.size.width);
                }
            }

            if layer.content_size.height > layer.local_viewport_rect.size.height {
                layer.scrolling.offset.y = layer.scrolling.offset.y + delta.y;
                if is_unscrollable || !CAN_OVERSCROLL {
                    layer.scrolling.offset.y = layer.scrolling.offset.y.min(0.0);
                    layer.scrolling.offset.y =
                        layer.scrolling.offset.y.max(-layer.content_size.height +
                                                     layer.local_viewport_rect.size.height);
                }
            }

            if phase == ScrollEventPhase::Start || phase == ScrollEventPhase::Move(true) {
                layer.scrolling.started_bouncing_back = false
            } else if overscrolling &&
                    ((delta.x < 1.0 && delta.y < 1.0) || phase == ScrollEventPhase::End) {
                layer.scrolling.started_bouncing_back = true;
                layer.scrolling.bouncing_back = true
            }

            layer.scrolling.offset.x = layer.scrolling.offset.x.round();
            layer.scrolling.offset.y = layer.scrolling.offset.y.round();

            if CAN_OVERSCROLL {
                layer.stretch_overscroll_spring();
            }

            scrolled_a_layer = scrolled_a_layer ||
                layer.scrolling.offset != original_layer_scroll_offset ||
                layer.scrolling.started_bouncing_back;
        }

        scrolled_a_layer
    }

    pub fn tick_scrolling_bounce_animations(&mut self) {
        for (_, layer) in &mut self.layers {
            layer.tick_scrolling_bounce_animation()
        }
    }

    pub fn create(&mut self,
                  scene: &Scene,
                  pipeline_sizes: &mut HashMap<PipelineId, Size2D<f32>>,
                  device_pixel_ratio: f32) {
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

        let old_layer_scrolling_states = self.reset();
        self.pipeline_auxiliary_lists = scene.pipeline_auxiliary_lists.clone();

        self.pipeline_epoch_map.insert(root_pipeline_id, root_pipeline.epoch);

        let root_stacking_context = match display_list.starting_stacking_context() {
            Some(stacking_context) => stacking_context,
            None => {
                warn!("Pipeline display list does not start with a stacking context.");
                return;
            }
        };

        let root_scroll_layer_id =
            root_stacking_context.scroll_layer_id
                                 .expect("root layer must be a scroll layer!");
        self.root_scroll_layer_id = Some(root_scroll_layer_id);

        // Insert global position: fixed elements layer
        debug_assert!(self.layers.is_empty());
        let root_fixed_layer_id = ScrollLayerId::create_fixed(root_pipeline_id);
        let root_viewport = Rect::new(Point2D::zero(), root_pipeline.viewport_size);
        let layer = Layer::new(&root_viewport,
                               root_stacking_context.overflow.size,
                               &Matrix4D::identity(),
                               root_pipeline_id);
        self.layers.insert(root_fixed_layer_id, layer.clone());
        self.layers.insert(root_scroll_layer_id, layer);

        let mut frame_builder = FrameBuilder::new(root_pipeline.viewport_size,
                                                  device_pixel_ratio,
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
                                          Matrix4D::identity(),
                                          0,
                                          &root_stacking_context);
        }

        self.frame_builder = Some(frame_builder);

        // TODO(gw): These are all independent - can be run through thread pool if it shows up
        // in the profile!
        for (scroll_layer_id, layer) in &mut self.layers {
            let scrolling_state = match old_layer_scrolling_states.get(&scroll_layer_id) {
                Some(old_scrolling_state) => *old_scrolling_state,
                None => ScrollingState::new(),
            };

            layer.finalize(&scrolling_state);
        }
    }

    fn flatten_stacking_context<'a>(&mut self,
                                    traversal: &mut DisplayListTraversal<'a>,
                                    pipeline_id: PipelineId,
                                    context: &mut FlattenContext,
                                    current_fixed_layer_id: ScrollLayerId,
                                    mut current_scroll_layer_id: ScrollLayerId,
                                    mut layer_relative_transform: Matrix4D<f32>,
                                    level: i32,
                                    stacking_context: &StackingContext) {
        // Avoid doing unnecessary work for empty stacking contexts.
        if traversal.current_stacking_context_empty() {
            traversal.skip_current_stacking_context();
            return;
        }

        match (stacking_context.scroll_policy, stacking_context.scroll_layer_id) {
            (ScrollPolicy::Scrollable, Some(inner_scroll_layer_id)) if level != 0 => {
                debug_assert!(!self.layers.contains_key(&inner_scroll_layer_id));

                let layer = Layer::new(&stacking_context.bounds,
                                       stacking_context.overflow.size,
                                       &layer_relative_transform,
                                       pipeline_id);

                debug_assert!(current_scroll_layer_id != inner_scroll_layer_id);
                self.layers
                    .get_mut(&current_scroll_layer_id)
                    .unwrap()
                    .add_child(inner_scroll_layer_id);

                self.layers.insert(inner_scroll_layer_id, layer);
                current_scroll_layer_id = inner_scroll_layer_id;
                layer_relative_transform = Matrix4D::identity();
            }
            _ => {}
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

        // Stacking contexts with scroll roots are currently not "real" stacking contexts,
        // but are currently represented as stacking contexts in the display list until they
        // can get their own display item to represent them. Thus we do not adjust the transform
        // to account for them and we expand the overflow region to include the area above
        // their origin in the parent context.
        let (transform, overflow) = if stacking_context.scroll_layer_id.is_none() {
            (layer_relative_transform.pre_translated(stacking_context.bounds.origin.x,
                                                     stacking_context.bounds.origin.y,
                                                     0.0)
                                     .pre_mul(&stacking_context.transform)
                                     .pre_mul(&stacking_context.perspective),
             stacking_context.overflow)
        } else {
            let mut overflow = stacking_context.overflow;
            overflow.size.width += stacking_context.bounds.origin.x;
            overflow.size.height += stacking_context.bounds.origin.y;
            (layer_relative_transform, overflow)
        };

        // Build world space transform
        let scroll_layer_id = match (stacking_context.scroll_policy,
                                     stacking_context.scroll_layer_id) {
            (ScrollPolicy::Fixed, _scroll_layer_id) => {
                debug_assert!(_scroll_layer_id.is_none());
                current_fixed_layer_id
            }
            (ScrollPolicy::Scrollable, _) => current_scroll_layer_id
        };

        // TODO(gw): Int with overflow etc
        context.builder.push_layer(overflow,
                                   overflow,
                                   transform,
                                   pipeline_id,
                                   scroll_layer_id,
                                   &composition_operations);

        if level == 0 {
            // Add a large white rectangle as the root display item if there is no root stacking
            // context background color. This is removed by the occlusion culling for most tiles,
            // and means that it's no longer necessary to clear the framebuffer.
            //
            // TODO(nical) Should painting a white background be optional if there is no stacking
            // context background color? On deferred GPUs we probably still want to clear the
            // framebuffer and Gecko currently supports semi-transparent windows.
            //
            // If we do need this, does it make sense to keep Frame::clear_tiles?
            let mut root_background_color = match context.scene.pipeline_map.get(&pipeline_id) {
                Some(pipeline) => pipeline.background_color,
                None => ColorF::new(1.0, 1.0, 1.0, 1.0),
            };

            if root_background_color.a == 0.0 {
                root_background_color = ColorF::new(1.0, 1.0, 1.0, 1.0);
            }

            context.builder.add_solid_rectangle(&stacking_context.bounds,
                                                &ClipRegion::simple(&stacking_context.bounds),
                                                &root_background_color,
                                                PrimitiveFlags::None);
        }

        self.flatten_items(traversal,
                           pipeline_id,
                           context,
                           current_fixed_layer_id,
                           current_scroll_layer_id,
                           transform,
                           level);

        if level == 0 && self.frame_builder_config.enable_scrollbars {
            let scrollbar_rect = Rect::new(Point2D::zero(), Size2D::new(10.0, 70.0));
            context.builder.add_solid_rectangle(&scrollbar_rect,
                                                &ClipRegion::simple(&scrollbar_rect),
                                                &DEFAULT_SCROLLBAR_COLOR,
                                                PrimitiveFlags::Scrollbar(self.root_scroll_layer_id.unwrap(),
                                                                          4.0));
        }

        context.builder.pop_layer();
    }

    fn flatten_iframe<'a>(&mut self,
                          pipeline_id: PipelineId,
                          bounds: &Rect<f32>,
                          context: &mut FlattenContext,
                          current_scroll_layer_id: ScrollLayerId,
                          layer_relative_transform: Matrix4D<f32>) {
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

        let iframe_stacking_context = match display_list.starting_stacking_context() {
            Some(stacking_context) => stacking_context,
            None => {
                warn!("Pipeline display list does not start with a stacking context.");
                return;
            }
        };

        self.pipeline_epoch_map.insert(pipeline_id, pipeline.epoch);

        let iframe_rect = &Rect::new(Point2D::zero(), bounds.size);
        let transform = layer_relative_transform.pre_translated(bounds.origin.x,
                                                                bounds.origin.y,
                                                                0.0);

        let iframe_fixed_layer_id = ScrollLayerId::create_fixed(pipeline_id);
        let iframe_scroll_layer_id = iframe_stacking_context.scroll_layer_id.unwrap();

        let layer = Layer::new(&iframe_rect,
                               iframe_stacking_context.overflow.size,
                               &transform,
                               pipeline_id);
        self.layers.insert(iframe_fixed_layer_id, layer.clone());
        self.layers.insert(iframe_scroll_layer_id, layer);
        self.layers.get_mut(&current_scroll_layer_id).unwrap().add_child(iframe_scroll_layer_id);

        let mut traversal = DisplayListTraversal::new_skipping_first(display_list);

        self.flatten_stacking_context(&mut traversal,
                                      pipeline_id,
                                      context,
                                      iframe_fixed_layer_id,
                                      iframe_scroll_layer_id,
                                      Matrix4D::identity(),
                                      0,
                                      &iframe_stacking_context);
    }

    fn flatten_items<'a>(&mut self,
                         traversal: &mut DisplayListTraversal<'a>,
                         pipeline_id: PipelineId,
                         context: &mut FlattenContext,
                         current_fixed_layer_id: ScrollLayerId,
                         current_scroll_layer_id: ScrollLayerId,
                         layer_relative_transform: Matrix4D<f32>,
                         level: i32) {
        while let Some(item) = traversal.next() {
            match item.item {
                SpecificDisplayItem::WebGL(ref info) => {
                    context.builder.add_webgl_rectangle(item.rect, &item.clip, info.context_id);
                }
                SpecificDisplayItem::Image(ref info) => {
                    context.builder.add_image(item.rect,
                                              &item.clip,
                                              &info.stretch_size,
                                              &info.tile_spacing,
                                              info.image_key,
                                              info.image_rendering);
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
                                                  &info.stacking_context);
                }
                SpecificDisplayItem::Iframe(ref info) => {
                    self.flatten_iframe(info.pipeline_id,
                                        &item.rect,
                                        context,
                                        current_scroll_layer_id,
                                        layer_relative_transform);
                }
                SpecificDisplayItem::PopStackingContext => return,
            }
        }
    }

    pub fn build(&mut self,
                 resource_cache: &mut ResourceCache,
                 auxiliary_lists_map: &AuxiliaryListsMap,
                 device_pixel_ratio: f32)
                 -> RendererFrame {
        self.update_layer_transforms(device_pixel_ratio);
        let frame = self.build_frame(resource_cache, auxiliary_lists_map);
        resource_cache.expire_old_resources(self.id);
        frame
    }

    fn update_layer_transform(&mut self,
                              layer_id: ScrollLayerId,
                              parent_world_transform: &Matrix4D<f32>,
                              parent_viewport_rect: &Rect<f32>,
                              device_pixel_ratio: f32) {
        // TODO(gw): This is an ugly borrow check workaround to clone these.
        //           Restructure this to avoid the clones!
        let (layer_transform_for_children, viewport_rect, layer_children) = {
            match self.layers.get_mut(&layer_id) {
                Some(layer) => {
                    let inv_transform = layer.local_transform.inverse().unwrap();
                    let parent_viewport_rect_in_local_space = inv_transform.transform_rect(parent_viewport_rect)
                                                                           .translate(&-layer.scrolling.offset);
                    let local_viewport_rect = layer.local_viewport_rect
                                                   .translate(&-layer.scrolling.offset);
                    let viewport_rect = parent_viewport_rect_in_local_space.intersection(&local_viewport_rect)
                                                                           .unwrap_or(Rect::new(Point2D::zero(), Size2D::zero()));

                    layer.combined_local_viewport_rect = viewport_rect;
                    layer.world_viewport_transform = parent_world_transform.pre_mul(&layer.local_transform);
                    layer.world_content_transform = layer.world_viewport_transform
                                                         .pre_translated(layer.scrolling.offset.x,
                                                                         layer.scrolling.offset.y,
                                                                         0.0);

                    (layer.world_content_transform,
                     viewport_rect,
                     layer.children.clone())
                }
                None => return,
            }
        };

        for child_layer_id in layer_children {
            self.update_layer_transform(child_layer_id,
                                        &layer_transform_for_children,
                                        &viewport_rect,
                                        device_pixel_ratio);
        }
    }

    fn update_layer_transforms(&mut self, device_pixel_ratio: f32) {
        if let Some(root_scroll_layer_id) = self.root_scroll_layer_id {
            let root_viewport = self.layers[&root_scroll_layer_id].local_viewport_rect;

            self.update_layer_transform(root_scroll_layer_id,
                                        &Matrix4D::identity(),
                                        &root_viewport,
                                        device_pixel_ratio);

            // Update any fixed layers
            let mut fixed_layers = Vec::new();
            for (layer_id, _) in &self.layers {
                match layer_id.info {
                    ScrollLayerInfo::Scrollable(..) => {}
                    ScrollLayerInfo::Fixed => {
                        fixed_layers.push(*layer_id);
                    }
                }
            }

            for layer_id in fixed_layers {
                self.update_layer_transform(layer_id,
                                            &Matrix4D::identity(),
                                            &root_viewport,
                                            device_pixel_ratio);
            }
        }
    }

    fn build_frame(&mut self,
                   resource_cache: &mut ResourceCache,
                   auxiliary_lists_map: &AuxiliaryListsMap) -> RendererFrame {
        let mut frame_builder = self.frame_builder.take();
        let frame = frame_builder.as_mut().map(|builder|
            builder.build(resource_cache, self.id, &self.layers, auxiliary_lists_map)
        );
        self.frame_builder = frame_builder;

        let layers_bouncing_back = self.collect_layers_bouncing_back();
        RendererFrame::new(self.pipeline_epoch_map.clone(),
                           layers_bouncing_back,
                           frame)
    }

    fn collect_layers_bouncing_back(&self)
                                    -> HashSet<ScrollLayerId, BuildHasherDefault<FnvHasher>> {
        let mut layers_bouncing_back = HashSet::with_hasher(Default::default());
        for (scroll_layer_id, layer) in &self.layers {
            if layer.scrolling.bouncing_back {
                layers_bouncing_back.insert(*scroll_layer_id);
            }
        }
        layers_bouncing_back
    }
}
