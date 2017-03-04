/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use app_units::Au;
use fnv::FnvHasher;
use internal_types::{ANGLE_FLOAT_TO_FIXED, AxisDirection};
use internal_types::{LowLevelFilterOp};
use internal_types::{RendererFrame};
use frame_builder::{FrameBuilder, FrameBuilderConfig};
use clip_scroll_node::ClipScrollNode;
use clip_scroll_tree::{ClipScrollTree, ScrollStates};
use profiler::TextureCacheProfileCounters;
use resource_cache::ResourceCache;
use scene::{Scene, SceneProperties};
use std::collections::HashMap;
use std::hash::BuildHasherDefault;
use tiling::{AuxiliaryListsMap, CompositeOps, PrimitiveFlags};
use webrender_traits::{AuxiliaryLists, ClipRegion, ColorF, DisplayItem, Epoch, FilterOp};
use webrender_traits::{LayerPoint, LayerRect, LayerSize, LayerToScrollTransform, LayoutTransform, TileOffset};
use webrender_traits::{MixBlendMode, PipelineId, ScrollEventPhase, ScrollLayerId, ScrollLayerState};
use webrender_traits::{ScrollLocation, ScrollPolicy, ServoScrollRootId, SpecificDisplayItem};
use webrender_traits::{StackingContext, WorldPoint, ImageDisplayItem, DeviceUintSize};

#[derive(Copy, Clone, PartialEq, PartialOrd, Debug)]
pub struct FrameId(pub u32);

static DEFAULT_SCROLLBAR_COLOR: ColorF = ColorF { r: 0.3, g: 0.3, b: 0.3, a: 0.6 };

struct FlattenContext<'a> {
    scene: &'a Scene,
    builder: &'a mut FrameBuilder,
    resource_cache: &'a mut ResourceCache,
}

// TODO: doc
pub struct Frame {
    pub clip_scroll_tree: ClipScrollTree,
    pub pipeline_epoch_map: HashMap<PipelineId, Epoch, BuildHasherDefault<FnvHasher>>,
    pub pipeline_auxiliary_lists: AuxiliaryListsMap,
    id: FrameId,
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
    fn mix_blend_mode_for_compositing(&self) -> Option<MixBlendMode>;
    fn filter_ops_for_compositing(&self,
                                  auxiliary_lists: &AuxiliaryLists,
                                  properties: &SceneProperties) -> Vec<LowLevelFilterOp>;
}

impl StackingContextHelpers for StackingContext {
    fn mix_blend_mode_for_compositing(&self) -> Option<MixBlendMode> {
        match self.mix_blend_mode {
            MixBlendMode::Normal => None,
            _ => Some(self.mix_blend_mode),
        }
    }

    fn filter_ops_for_compositing(&self,
                                  auxiliary_lists: &AuxiliaryLists,
                                  properties: &SceneProperties) -> Vec<LowLevelFilterOp> {
        let mut filters = vec![];
        for filter in auxiliary_lists.filters(&self.filters) {
            match *filter {
                FilterOp::Blur(radius) => {
                    filters.push(LowLevelFilterOp::Blur(
                        radius,
                        AxisDirection::Horizontal));
                    filters.push(LowLevelFilterOp::Blur(
                        radius,
                        AxisDirection::Vertical));
                }
                FilterOp::Brightness(amount) => {
                    filters.push(
                            LowLevelFilterOp::Brightness(Au::from_f32_px(amount)));
                }
                FilterOp::Contrast(amount) => {
                    filters.push(
                            LowLevelFilterOp::Contrast(Au::from_f32_px(amount)));
                }
                FilterOp::Grayscale(amount) => {
                    filters.push(
                            LowLevelFilterOp::Grayscale(Au::from_f32_px(amount)));
                }
                FilterOp::HueRotate(angle) => {
                    filters.push(
                            LowLevelFilterOp::HueRotate(f32::round(
                                    angle * ANGLE_FLOAT_TO_FIXED) as i32));
                }
                FilterOp::Invert(amount) => {
                    filters.push(
                            LowLevelFilterOp::Invert(Au::from_f32_px(amount)));
                }
                FilterOp::Opacity(ref value) => {
                    let amount = properties.resolve_float(value, 1.0);
                    filters.push(
                            LowLevelFilterOp::Opacity(Au::from_f32_px(amount)));
                }
                FilterOp::Saturate(amount) => {
                    filters.push(
                            LowLevelFilterOp::Saturate(Au::from_f32_px(amount)));
                }
                FilterOp::Sepia(amount) => {
                    filters.push(
                            LowLevelFilterOp::Sepia(Au::from_f32_px(amount)));
                }
            }
        }
        filters
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
    pub fn new(config: FrameBuilderConfig) -> Frame {
        Frame {
            pipeline_epoch_map: HashMap::with_hasher(Default::default()),
            pipeline_auxiliary_lists: HashMap::with_hasher(Default::default()),
            clip_scroll_tree: ClipScrollTree::new(),
            id: FrameId(0),
            frame_builder: None,
            frame_builder_config: config,
        }
    }

    pub fn reset(&mut self) -> ScrollStates {
        self.pipeline_epoch_map.clear();

        // Advance to the next frame.
        self.id.0 += 1;

        self.clip_scroll_tree.drain()
    }

    pub fn get_scroll_node_state(&self) -> Vec<ScrollLayerState> {
        self.clip_scroll_tree.get_scroll_node_state()
    }

    /// Returns true if any nodes actually changed position or false otherwise.
    pub fn scroll_nodes(&mut self,
                        origin: LayerPoint,
                        pipeline_id: PipelineId,
                        scroll_root_id: ServoScrollRootId)
                         -> bool {
        self.clip_scroll_tree.scroll_nodes(origin, pipeline_id, scroll_root_id)
    }

    /// Returns true if any nodes actually changed position or false otherwise.
    pub fn scroll(&mut self,
                  scroll_location: ScrollLocation,
                  cursor: WorldPoint,
                  phase: ScrollEventPhase)
                  -> bool {
        self.clip_scroll_tree.scroll(scroll_location, cursor, phase,)
    }

    pub fn tick_scrolling_bounce_animations(&mut self) {
        self.clip_scroll_tree.tick_scrolling_bounce_animations();
    }

    pub fn discard_frame_state_for_pipeline(&mut self, pipeline_id: PipelineId) {
        self.clip_scroll_tree.discard_frame_state_for_pipeline(pipeline_id);
    }

    pub fn create(&mut self, scene: &Scene, resource_cache: &mut ResourceCache) {
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

        self.clip_scroll_tree.establish_root(root_pipeline_id,
                                             &root_pipeline.viewport_size,
                                             &root_clip.main.size);

        let background_color = root_pipeline.background_color.and_then(|color| {
            if color.a > 0.0 {
                Some(color)
            } else {
                None
            }
        });

        let mut frame_builder = FrameBuilder::new(root_pipeline.viewport_size,
                                                  background_color,
                                                  self.frame_builder_config);

        {
            let mut context = FlattenContext {
                scene: scene,
                builder: &mut frame_builder,
                resource_cache: resource_cache
            };

            let mut traversal = DisplayListTraversal::new_skipping_first(display_list);
            let reference_frame_id = self.clip_scroll_tree.root_reference_frame_id();
            let topmost_scroll_layer_id = self.clip_scroll_tree.topmost_scroll_layer_id();
            debug_assert!(reference_frame_id != topmost_scroll_layer_id);

            let viewport_rect = LayerRect::new(LayerPoint::zero(), root_pipeline.viewport_size);
            let clip = ClipRegion::simple(&viewport_rect);
            context.builder.push_clip_scroll_node(reference_frame_id,
                                                  &clip,
                                                  &LayerPoint::zero(),
                                                  &root_pipeline.viewport_size);
            context.builder.push_clip_scroll_node(topmost_scroll_layer_id,
                                                  &clip,
                                                  &LayerPoint::zero(),
                                                  &root_clip.main.size);

            self.flatten_stacking_context(&mut traversal,
                                          root_pipeline_id,
                                          &mut context,
                                          reference_frame_id,
                                          topmost_scroll_layer_id,
                                          LayerToScrollTransform::identity(),
                                          0,
                                          &root_stacking_context,
                                          root_clip);

            context.builder.pop_clip_scroll_node();
            context.builder.pop_clip_scroll_node();
        }

        self.frame_builder = Some(frame_builder);
        self.clip_scroll_tree.finalize_and_apply_pending_scroll_offsets(old_scrolling_states);
    }

    fn flatten_scroll_layer<'a>(&mut self,
                                traversal: &mut DisplayListTraversal<'a>,
                                pipeline_id: PipelineId,
                                context: &mut FlattenContext,
                                current_reference_frame_id: ScrollLayerId,
                                parent_scroll_layer_id: ScrollLayerId,
                                layer_relative_transform: LayerToScrollTransform,
                                level: i32,
                                clip: &ClipRegion,
                                content_size: &LayerSize,
                                new_scroll_layer_id: ScrollLayerId) {
        // Avoid doing unnecessary work for empty stacking contexts.
        if traversal.current_stacking_context_empty() {
            traversal.skip_current_stacking_context();
            return;
        }

        let clip_rect = clip.main;
        let node = ClipScrollNode::new(&clip_rect,
                                       *content_size,
                                       &layer_relative_transform,
                                       pipeline_id);
        self.clip_scroll_tree.add_node(node, new_scroll_layer_id, parent_scroll_layer_id);
        context.builder.push_clip_scroll_node(new_scroll_layer_id,
                                              clip,
                                              &clip_rect.origin,
                                              &content_size);

        self.flatten_items(traversal,
                           pipeline_id,
                           context,
                           current_reference_frame_id,
                           new_scroll_layer_id,
                           LayerToScrollTransform::identity(),
                           level);

        context.builder.pop_clip_scroll_node();
    }

    fn flatten_stacking_context<'a>(&mut self,
                                    traversal: &mut DisplayListTraversal<'a>,
                                    pipeline_id: PipelineId,
                                    context: &mut FlattenContext,
                                    current_reference_frame_id: ScrollLayerId,
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
            CompositeOps::new(
                stacking_context.filter_ops_for_compositing(auxiliary_lists, &context.scene.properties),
                stacking_context.mix_blend_mode_for_compositing())
        };

        if composition_operations.will_make_invisible() {
            traversal.skip_current_stacking_context();
            return;
        }

        let stacking_context_transform = context.scene
                                                .properties
                                                .resolve_layout_transform(&stacking_context.transform);

        let mut transform =
            layer_relative_transform.pre_translated(stacking_context.bounds.origin.x,
                                                    stacking_context.bounds.origin.y,
                                                    0.0)
                                     .pre_mul(&stacking_context_transform)
                                     .pre_mul(&stacking_context.perspective);

        let mut reference_frame_id = current_reference_frame_id;
        let mut scroll_layer_id = match stacking_context.scroll_policy {
            ScrollPolicy::Fixed => current_reference_frame_id,
            ScrollPolicy::Scrollable => current_scroll_layer_id,
        };

        // If we have a transformation, we establish a new reference frame. This means
        // that fixed position stacking contexts are positioned relative to us.
        if stacking_context_transform != LayoutTransform::identity() ||
           stacking_context.perspective != LayoutTransform::identity() {
            scroll_layer_id = self.clip_scroll_tree.add_reference_frame(clip_region.main,
                                                                        transform,
                                                                        pipeline_id,
                                                                        scroll_layer_id);
            reference_frame_id = scroll_layer_id;
            transform = LayerToScrollTransform::identity();
        }

        if level == 0 {
            if let Some(pipeline) = context.scene.pipeline_map.get(&pipeline_id) {
                if let Some(bg_color) = pipeline.background_color {

                    // Adding a dummy layer for this rectangle in order to disable clipping.
                    let no_clip = ClipRegion::simple(&clip_region.main);
                    context.builder.push_stacking_context(clip_region.main,
                                                          transform,
                                                          pipeline_id,
                                                          scroll_layer_id,
                                                          CompositeOps::empty());

                    //Note: we don't use the original clip region here,
                    // it's already processed by the node we just pushed.
                    context.builder.add_solid_rectangle(&clip_region.main,
                                                        &no_clip,
                                                        &bg_color,
                                                        PrimitiveFlags::None);

                    context.builder.pop_stacking_context();
                }
            }
        }

         // TODO(gw): Int with overflow etc
        context.builder.push_stacking_context(clip_region.main,
                                              transform,
                                              pipeline_id,
                                              scroll_layer_id,
                                              composition_operations);

        self.flatten_items(traversal,
                           pipeline_id,
                           context,
                           reference_frame_id,
                           scroll_layer_id,
                           transform,
                           level);

        if level == 0 && self.frame_builder_config.enable_scrollbars {
            let scrollbar_rect = LayerRect::new(LayerPoint::zero(), LayerSize::new(10.0, 70.0));
            context.builder.add_solid_rectangle(
                &scrollbar_rect,
                &ClipRegion::simple(&scrollbar_rect),
                &DEFAULT_SCROLLBAR_COLOR,
                PrimitiveFlags::Scrollbar(self.clip_scroll_tree.topmost_scroll_layer_id(), 4.0));
        }

        context.builder.pop_stacking_context();
    }

    fn flatten_iframe<'a>(&mut self,
                          pipeline_id: PipelineId,
                          bounds: &LayerRect,
                          context: &mut FlattenContext,
                          current_scroll_layer_id: ScrollLayerId,
                          layer_relative_transform: LayerToScrollTransform) {

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

        let iframe_rect = LayerRect::new(LayerPoint::zero(), bounds.size);
        let transform = layer_relative_transform.pre_translated(bounds.origin.x,
                                                                bounds.origin.y,
                                                                0.0);
        let iframe_reference_frame_id =
            self.clip_scroll_tree.add_reference_frame(iframe_rect,
                                                      transform,
                                                      pipeline_id,
                                                      current_scroll_layer_id);
        let iframe_scroll_layer_id = ScrollLayerId::root_scroll_layer(pipeline_id);
        let node = ClipScrollNode::new(&LayerRect::new(LayerPoint::zero(), iframe_rect.size),
                                       iframe_clip.main.size,
                                       &LayerToScrollTransform::identity(),
                                       pipeline_id);
        self.clip_scroll_tree.add_node(node.clone(),
                                       iframe_scroll_layer_id,
                                       iframe_reference_frame_id);

        context.builder.push_clip_scroll_node(iframe_reference_frame_id,
                                              iframe_clip,
                                              &LayerPoint::zero(),
                                              &iframe_rect.size);
        context.builder.push_clip_scroll_node(iframe_scroll_layer_id,
                                              iframe_clip,
                                              &LayerPoint::zero(),
                                              &iframe_clip.main.size);

        let mut traversal = DisplayListTraversal::new_skipping_first(display_list);

        self.flatten_stacking_context(&mut traversal,
                                      pipeline_id,
                                      context,
                                      iframe_reference_frame_id,
                                      iframe_scroll_layer_id,
                                      LayerToScrollTransform::identity(),
                                      0,
                                      &iframe_stacking_context,
                                      iframe_clip);

        context.builder.pop_clip_scroll_node();
        context.builder.pop_clip_scroll_node();
    }

    fn flatten_items<'a>(&mut self,
                         traversal: &mut DisplayListTraversal<'a>,
                         pipeline_id: PipelineId,
                         context: &mut FlattenContext,
                         current_reference_frame_id: ScrollLayerId,
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
                    let image = context.resource_cache.get_image_properties(info.image_key);
                    if let Some(tile_size) = image.tiling {
                        // The image resource is tiled. We have to generate an image primitive
                        // for each tile.
                        let image_size = DeviceUintSize::new(image.descriptor.width, image.descriptor.height);
                        self.decompose_tiled_image(context, &item, info, image_size, tile_size as u32);
                    } else {
                        context.builder.add_image(item.rect,
                                                  &item.clip,
                                                  &info.stretch_size,
                                                  &info.tile_spacing,
                                                  None,
                                                  info.image_key,
                                                  info.image_rendering,
                                                  None);
                    }
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
                                             text_info.glyphs,
                                             text_info.glyph_options);
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
                                                 info.stops,
                                                 info.extend_mode);
                }
                SpecificDisplayItem::RadialGradient(ref info) => {
                    context.builder.add_radial_gradient(item.rect,
                                                        &item.clip,
                                                        info.start_center,
                                                        info.start_radius,
                                                        info.end_center,
                                                        info.end_radius,
                                                        info.stops,
                                                        info.extend_mode);
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
                                                  current_reference_frame_id,
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
                                              current_reference_frame_id,
                                              current_scroll_layer_id,
                                              layer_relative_transform,
                                              level,
                                              &item.clip,
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

    fn decompose_tiled_image(&mut self,
                             context: &mut FlattenContext,
                             item: &DisplayItem,
                             info: &ImageDisplayItem,
                             image_size: DeviceUintSize,
                             tile_size: u32) {
        // The image resource is tiled. We have to generate an image primitive
        // for each tile.
        // We need to do this because the image is broken up into smaller tiles in the texture
        // cache and the image shader is not able to work with this type of sparse representation.

        // The tiling logic works as follows:
        //
        //  ###################-+  -+
        //  #    |    |    |//# |   | image size
        //  #    |    |    |//# |   |
        //  #----+----+----+--#-+   |  -+ 
        //  #    |    |    |//# |   |   | regular tile size
        //  #    |    |    |//# |   |   |
        //  #----+----+----+--#-+   |  -+-+
        //  #////|////|////|//# |   |     | "leftover" height
        //  ################### |  -+  ---+
        //  #----+----+----+----+
        //
        // In the ascii diagram above, a large image is plit into tiles of almost regular size.
        // The tiles on the right and bottom edges (hatched in the diagram) are smaller than
        // the regular tiles and are handled separately in the code see leftover_width/height.
        // each generated image primitive corresponds to a tile in the texture cache, with the
        // assumption that the smaller tiles with leftover sizes are sized to fit their own
        // irregular size in the texture cache.

        // TODO(nical) supporting tiled repeated images isn't implemented yet.
        // One way to implement this is to have another level of decomposition on top of this one,
        // and generate a set of image primitive per repetition just like we have a primitive
        // per tile here.
        //
        // For the case where we don't tile along an axis, we can still perform the repetition in
        // the shader (for this particular axis), and it is worth special-casing for this to avoid
        // generating many primitives.
        // This can happen with very tall and thin images used as a repeating background.
        // Apparently web authors do that...

        let mut stretch_size = info.stretch_size;

        let mut repeat_x = false;
        let mut repeat_y = false;

        if stretch_size.width < item.rect.size.width {
            if image_size.width < tile_size {
                // we don't actually tile in this dimmension so repeating can be done in the shader.
                repeat_x = true;
            } else {
                println!("Unimplemented! repeating a tiled image (x axis)");
                stretch_size.width = item.rect.size.width;
            }
        }

        if stretch_size.height < item.rect.size.height {
                // we don't actually tile in this dimmension so repeating can be done in the shader.
            if image_size.height < tile_size {
                repeat_y = true;
            } else {
                println!("Unimplemented! repeating a tiled image (y axis)");
                stretch_size.height = item.rect.size.height;
            }
        }

        let tile_size_f32 = tile_size as f32;

        // Note: this rounds down so it excludes the partially filled tiles on the right and
        // bottom edges (we handle them separately below).
        let num_tiles_x = (image_size.width / tile_size) as u16;
        let num_tiles_y = (image_size.height / tile_size) as u16;

        // Ratio between (image space) tile size and image size.
        let img_dw = tile_size_f32 / (image_size.width as f32);
        let img_dh = tile_size_f32 / (image_size.height as f32);

        // Strected size of the tile in layout space.
        let stretched_tile_size = LayerSize::new(
            img_dw * stretch_size.width,
            img_dh * stretch_size.height
        );

        // The size in pixels of the tiles on the right and bottom edges, smaller
        // than the regular tile size if the image is not a multiple of the tile size.
        // Zero means the image size is a multiple of the tile size.
        let leftover = DeviceUintSize::new(image_size.width % tile_size, image_size.height % tile_size);

        for ty in 0..num_tiles_y {
            for tx in 0..num_tiles_x {
                self.add_tile_primitive(context, item, info,
                                        TileOffset::new(tx, ty),
                                        stretched_tile_size,
                                        1.0, 1.0,
                                        repeat_x, repeat_y);
            }
            if leftover.width != 0 {
                // Tiles on the right edge that are smaller than the tile size.
                self.add_tile_primitive(context, item, info,
                                        TileOffset::new(num_tiles_x, ty),
                                        stretched_tile_size,
                                        (leftover.width as f32) / tile_size_f32,
                                        1.0,
                                        repeat_x, repeat_y);
            }
        }

        if leftover.height != 0 {
            for tx in 0..num_tiles_x {
                // Tiles on the bottom edge that are smaller than the tile size.
                self.add_tile_primitive(context, item, info,
                                        TileOffset::new(tx, num_tiles_y),
                                        stretched_tile_size,
                                        1.0,
                                        (leftover.height as f32) / tile_size_f32,
                                        repeat_x, repeat_y);
            }

            if leftover.width != 0 {
                // Finally, the bottom-right tile with a "leftover" size.
                self.add_tile_primitive(context, item, info,
                                        TileOffset::new(num_tiles_x, num_tiles_y),
                                        stretched_tile_size,
                                        (leftover.width as f32) / tile_size_f32,
                                        (leftover.height as f32) / tile_size_f32,
                                        repeat_x, repeat_y);
            }
        }
    }

    fn add_tile_primitive(&mut self,
                          context: &mut FlattenContext,
                          item: &DisplayItem,
                          info: &ImageDisplayItem,
                          tile_offset: TileOffset,
                          stretched_tile_size: LayerSize,
                          tile_ratio_width: f32,
                          tile_ratio_height: f32,
                          repeat_x: bool,
                          repeat_y: bool) {
        // If the the image is tiled along a given axis, we can't have the shader compute
        // the image repetition pattern. In this case we base the primitive's rectangle size
        // on the stretched tile size which effectively cancels the repetion (and repetition
        // has to be emulated by generating more primitives).
        // If the image is not tiling along this axis, we can perform the repetition in the
        // shader. in this case we use the item's size in the primitive (on that particular
        // axis).
        // See the repeat_x/y code below.

        let stretched_size = LayerSize::new(
            stretched_tile_size.width * tile_ratio_width,
            stretched_tile_size.height * tile_ratio_height,
        );

        let mut prim_rect = LayerRect::new(
            item.rect.origin + LayerPoint::new(
                tile_offset.x as f32 * stretched_tile_size.width,
                tile_offset.y as f32 * stretched_tile_size.height,
            ),
            stretched_size,
        );

        if repeat_x {
            assert_eq!(tile_offset.x, 0);
            prim_rect.size.width = item.rect.size.width;
        }

        if repeat_y {
            assert_eq!(tile_offset.y, 0);
            prim_rect.size.height = item.rect.size.height;
        }

        // Fix up the primitive's rect if it overflows the original item rect.
        if let Some(prim_rect) = prim_rect.intersection(&item.rect) {
            context.builder.add_image(prim_rect,
                                      &item.clip,
                                      &stretched_size,
                                      &info.tile_spacing,
                                      None,
                                      info.image_key,
                                      info.image_rendering,
                                      Some(tile_offset));
        }
    }

    pub fn build(&mut self,
                 resource_cache: &mut ResourceCache,
                 auxiliary_lists_map: &AuxiliaryListsMap,
                 device_pixel_ratio: f32,
                 texture_cache_profile: &mut TextureCacheProfileCounters)
                 -> RendererFrame {
        self.clip_scroll_tree.update_all_node_transforms();
        let frame = self.build_frame(resource_cache,
                                     auxiliary_lists_map,
                                     device_pixel_ratio,
                                     texture_cache_profile);
        resource_cache.expire_old_resources(self.id);
        frame
    }

    fn build_frame(&mut self,
                   resource_cache: &mut ResourceCache,
                   auxiliary_lists_map: &AuxiliaryListsMap,
                   device_pixel_ratio: f32,
                   texture_cache_profile: &mut TextureCacheProfileCounters)
                   -> RendererFrame {
        let mut frame_builder = self.frame_builder.take();
        let frame = frame_builder.as_mut().map(|builder|
            builder.build(resource_cache,
                          self.id,
                          &self.clip_scroll_tree,
                          auxiliary_lists_map,
                          device_pixel_ratio,
                          texture_cache_profile)
        );
        self.frame_builder = frame_builder;

        let nodes_bouncing_back = self.clip_scroll_tree.collect_nodes_bouncing_back();
        RendererFrame::new(self.pipeline_epoch_map.clone(), nodes_bouncing_back, frame)
    }
}
