/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{BuiltDisplayList, BuiltDisplayListIter, ClipAndScrollInfo, ClipId, ColorF};
use api::{ComplexClipRegion, DeviceUintRect, DeviceUintSize, DisplayItemRef, Epoch, FilterOp};
use api::{ImageDisplayItem, ItemRange, LayerPoint, LayerRect, LayerSize, LayerToScrollTransform};
use api::{LayerVector2D, LayoutSize, LayoutTransform, LocalClip, MixBlendMode, PipelineId};
use api::{PropertyBinding, ScrollClamping, ScrollEventPhase, ScrollLayerState, ScrollLocation};
use api::{ScrollPolicy, ScrollSensitivity, SpecificDisplayItem, StackingContext, TileOffset};
use api::{TransformStyle, WorldPoint};
use clip_scroll_tree::{ClipScrollTree, ScrollStates};
use euclid::rect;
use fnv::FnvHasher;
use gpu_cache::GpuCache;
use internal_types::{RendererFrame};
use frame_builder::{FrameBuilder, FrameBuilderConfig};
use mask_cache::ClipRegion;
use profiler::{GpuCacheProfileCounters, TextureCacheProfileCounters};
use resource_cache::ResourceCache;
use scene::{Scene, SceneProperties};
use std::cmp;
use std::collections::HashMap;
use std::hash::BuildHasherDefault;
use tiling::{CompositeOps, DisplayListMap, PrimitiveFlags};
use util::{ComplexClipRegionHelpers, subtract_rect};

#[derive(Copy, Clone, PartialEq, PartialOrd, Debug)]
pub struct FrameId(pub u32);

static DEFAULT_SCROLLBAR_COLOR: ColorF = ColorF { r: 0.3, g: 0.3, b: 0.3, a: 0.6 };

/// Nested display lists cause two types of replacements to ClipIds inside the nesting:
///     1. References to the root scroll frame are replaced by the ClipIds that
///        contained the nested display list.
///     2. Other ClipIds (that aren't custom or reference frames) are assumed to be
///        local to the nested display list and are converted to an id that is unique
///        outside of the nested display list as well.
///
/// This structure keeps track of what ids are the "root" for one particular level of
/// nesting as well as keeping and index, which can make ClipIds used internally unique
/// in the full ClipScrollTree.
struct NestedDisplayListInfo {
    /// The index of this nested display list, which is used to generate
    /// new ClipIds for clips that are defined inside it.
    nest_index: u64,

    /// The ClipId of the scroll frame node which contains this nested
    /// display list. This is used to replace references to the root with
    /// the proper ClipId.
    scroll_node_id: ClipId,

    /// The ClipId of the clip node which contains this nested display list.
    /// This is used to replace references to the root with the proper ClipId.
    clip_node_id: ClipId,
}

impl NestedDisplayListInfo {
    fn convert_id_to_nested(&self, id: &ClipId) -> ClipId {
        match *id {
            ClipId::Clip(id, _, pipeline_id) => ClipId::Clip(id, self.nest_index, pipeline_id),
            _ => *id,
        }
    }

    fn convert_scroll_id_to_nested(&self, id: &ClipId) -> ClipId {
        if id.pipeline_id() != self.scroll_node_id.pipeline_id() {
            return *id;
        }

        if id.is_root_scroll_node() {
            self.scroll_node_id
        } else {
            self.convert_id_to_nested(id)
        }
    }

    fn convert_clip_id_to_nested(&self, id: &ClipId) -> ClipId {
        if id.pipeline_id() != self.clip_node_id.pipeline_id() {
            return *id;
        }

        if id.is_root_scroll_node() {
            self.clip_node_id
        } else {
            self.convert_id_to_nested(id)
        }
    }
}

struct FlattenContext<'a> {
    scene: &'a Scene,
    builder: &'a mut FrameBuilder,
    resource_cache: &'a mut ResourceCache,
    replacements: Vec<(ClipId, ClipId)>,
    nested_display_list_info: Vec<NestedDisplayListInfo>,
    current_nested_display_list_index: u64,
}

impl<'a> FlattenContext<'a> {
    fn new(scene: &'a Scene,
           builder: &'a mut FrameBuilder,
           resource_cache: &'a mut ResourceCache)
           -> FlattenContext<'a> {
        FlattenContext {
            scene,
            builder,
            resource_cache,
            replacements: Vec::new(),
            nested_display_list_info: Vec::new(),
            current_nested_display_list_index: 0,
        }
    }

    fn push_nested_display_list_ids(&mut self, info: ClipAndScrollInfo) {
        self.current_nested_display_list_index += 1;
        self.nested_display_list_info.push(NestedDisplayListInfo {
            nest_index: self.current_nested_display_list_index,
            scroll_node_id: info.scroll_node_id,
            clip_node_id: info.clip_node_id(),
        });
    }

    fn pop_nested_display_list_ids(&mut self) {
        self.nested_display_list_info.pop();
    }

    fn convert_new_id_to_nested(&self, id: &ClipId) -> ClipId {
        if let Some(nested_info) = self.nested_display_list_info.last() {
            nested_info.convert_id_to_nested(id)
        } else {
            *id
        }
    }

    fn convert_clip_scroll_info_to_nested(&self, info: &mut ClipAndScrollInfo) {
        if let Some(nested_info) = self.nested_display_list_info.last() {
            info.scroll_node_id = nested_info.convert_scroll_id_to_nested(&info.scroll_node_id);
            info.clip_node_id =
                info.clip_node_id.map(|ref id| nested_info.convert_clip_id_to_nested(id));
        }

        // We only want to produce nested ClipIds if we are in a nested display
        // list situation.
        debug_assert!(!info.scroll_node_id.is_nested() ||
                      !self.nested_display_list_info.is_empty());
        debug_assert!(!info.clip_node_id().is_nested() ||
                      !self.nested_display_list_info.is_empty());
    }

    /// Since WebRender still handles fixed position and reference frame content internally
    /// we need to apply this table of id replacements only to the id that affects the
    /// position of a node. We can eventually remove this when clients start handling
    /// reference frames themselves. This method applies these replacements.
    fn apply_scroll_frame_id_replacement(&self, id: ClipId) -> ClipId {
        match self.replacements.last() {
            Some(&(to_replace, replacement)) if to_replace == id => replacement,
            _ => id,
        }
    }

    fn get_complex_clips(&self,
                         pipeline_id: PipelineId,
                         complex_clips: ItemRange<ComplexClipRegion>)
                         -> Vec<ComplexClipRegion> {
        if complex_clips.is_empty() {
            return vec![];
        }

        self.scene.display_lists.get(&pipeline_id)
                                .expect("No display list?")
                                .get(complex_clips)
                                .collect()
    }
}

// TODO: doc
pub struct Frame {
    pub clip_scroll_tree: ClipScrollTree,
    pub pipeline_epoch_map: HashMap<PipelineId, Epoch, BuildHasherDefault<FnvHasher>>,
    id: FrameId,
    frame_builder_config: FrameBuilderConfig,
    frame_builder: Option<FrameBuilder>,
}

trait StackingContextHelpers {
    fn mix_blend_mode_for_compositing(&self) -> Option<MixBlendMode>;
    fn filter_ops_for_compositing(&self,
                                  display_list: &BuiltDisplayList,
                                  input_filters: ItemRange<FilterOp>,
                                  properties: &SceneProperties) -> Vec<FilterOp>;
}

impl StackingContextHelpers for StackingContext {
    fn mix_blend_mode_for_compositing(&self) -> Option<MixBlendMode> {
        match self.mix_blend_mode {
            MixBlendMode::Normal => None,
            _ => Some(self.mix_blend_mode),
        }
    }

    fn filter_ops_for_compositing(&self,
                                  display_list: &BuiltDisplayList,
                                  input_filters: ItemRange<FilterOp>,
                                  properties: &SceneProperties) -> Vec<FilterOp> {
        let mut filters = vec![];
        for filter in display_list.get(input_filters) {
            if filter.is_noop() {
                continue;
            }
            if let FilterOp::Opacity(ref value) = filter {
                let amount = properties.resolve_float(value, 1.0);
                filters.push(FilterOp::Opacity(PropertyBinding::Value(amount)));
            } else {
                filters.push(filter);
            }
        }
        filters
    }
}

impl Frame {
    pub fn new(config: FrameBuilderConfig) -> Frame {
        Frame {
            pipeline_epoch_map: HashMap::default(),
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

    /// Returns true if the node actually changed position or false otherwise.
    pub fn scroll_node(&mut self, origin: LayerPoint, id: ClipId, clamp: ScrollClamping) -> bool {
        self.clip_scroll_tree.scroll_node(origin, id, clamp)
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

    pub fn create(&mut self,
                  scene: &Scene,
                  resource_cache: &mut ResourceCache,
                  window_size: DeviceUintSize,
                  inner_rect: DeviceUintRect,
                  device_pixel_ratio: f32) {
        let root_pipeline_id = match scene.root_pipeline_id {
            Some(root_pipeline_id) => root_pipeline_id,
            None => return,
        };

        let root_pipeline = match scene.pipeline_map.get(&root_pipeline_id) {
            Some(root_pipeline) => root_pipeline,
            None => return,
        };

        let display_list = match scene.display_lists.get(&root_pipeline_id) {
            Some(display_list) => display_list,
            None => return,
        };

        if window_size.width == 0 || window_size.height == 0 {
            error!("ERROR: Invalid window dimensions! Please call api.set_window_size()");
        }

        let old_scrolling_states = self.reset();

        self.pipeline_epoch_map.insert(root_pipeline_id, root_pipeline.epoch);

        let background_color = root_pipeline.background_color.and_then(|color| {
            if color.a > 0.0 {
                Some(color)
            } else {
                None
            }
        });

        let mut frame_builder = FrameBuilder::new(self.frame_builder.take(),
                                                  window_size,
                                                  background_color,
                                                  self.frame_builder_config);

        {
            let mut context = FlattenContext::new(scene, &mut frame_builder, resource_cache);

            context.builder.push_root(root_pipeline_id,
                                      &root_pipeline.viewport_size,
                                      &root_pipeline.content_size,
                                      &mut self.clip_scroll_tree);

            context.builder.setup_viewport_offset(window_size,
                                                  inner_rect,
                                                  device_pixel_ratio,
                                                  &mut self.clip_scroll_tree);

            self.flatten_root(&mut display_list.iter(),
                              root_pipeline_id,
                              &mut context,
                              &root_pipeline.content_size);
        }

        self.frame_builder = Some(frame_builder);
        self.clip_scroll_tree.finalize_and_apply_pending_scroll_offsets(old_scrolling_states);
    }

    fn flatten_clip<'a>(&mut self,
                        context: &mut FlattenContext,
                        pipeline_id: PipelineId,
                        parent_id: &ClipId,
                        new_clip_id: &ClipId,
                        clip_region: ClipRegion) {
        let new_clip_id = context.convert_new_id_to_nested(new_clip_id);
        context.builder.add_clip_node(new_clip_id,
                                      *parent_id,
                                      pipeline_id,
                                      clip_region,
                                      &mut self.clip_scroll_tree);
    }

    fn flatten_scroll_frame<'a>(&mut self,
                                context: &mut FlattenContext,
                                pipeline_id: PipelineId,
                                parent_id: &ClipId,
                                new_scroll_frame_id: &ClipId,
                                frame_rect: &LayerRect,
                                content_rect: &LayerRect,
                                clip_region: ClipRegion,
                                scroll_sensitivity: ScrollSensitivity) {
        let clip_id = self.clip_scroll_tree.generate_new_clip_id(pipeline_id);
        context.builder.add_clip_node(clip_id,
                                      *parent_id,
                                      pipeline_id,
                                      clip_region,
                                      &mut self.clip_scroll_tree);

        let new_scroll_frame_id = context.convert_new_id_to_nested(new_scroll_frame_id);
        context.builder.add_scroll_frame(new_scroll_frame_id,
                                         clip_id,
                                         pipeline_id,
                                         &frame_rect,
                                         &content_rect.size,
                                         scroll_sensitivity,
                                         &mut self.clip_scroll_tree);
    }

    fn flatten_stacking_context<'a>(&mut self,
                                    traversal: &mut BuiltDisplayListIter<'a>,
                                    pipeline_id: PipelineId,
                                    context: &mut FlattenContext,
                                    context_scroll_node_id: ClipId,
                                    mut reference_frame_relative_offset: LayerVector2D,
                                    bounds: &LayerRect,
                                    stacking_context: &StackingContext,
                                    filters: ItemRange<FilterOp>) {
        // Avoid doing unnecessary work for empty stacking contexts.
        if traversal.current_stacking_context_empty() {
            traversal.skip_current_stacking_context();
            return;
        }

        let composition_operations = {
            // TODO(optimization?): self.traversal.display_list()
            let display_list = context.scene.display_lists
                                      .get(&pipeline_id)
                                      .expect("No display list?!");
            CompositeOps::new(
                stacking_context.filter_ops_for_compositing(display_list, filters, &context.scene.properties),
                stacking_context.mix_blend_mode_for_compositing())
        };

        if composition_operations.will_make_invisible() {
            traversal.skip_current_stacking_context();
            return;
        }

        if stacking_context.scroll_policy == ScrollPolicy::Fixed {
            context.replacements.push((context_scroll_node_id,
                                       context.builder.current_reference_frame_id()));
        }

        // If we have a transformation, we establish a new reference frame. This means
        // that fixed position stacking contexts are positioned relative to us.
        let is_reference_frame = stacking_context.transform.is_some() ||
                                 stacking_context.perspective.is_some();
        if is_reference_frame {
            let transform = stacking_context.transform.as_ref();
            let transform = context.scene.properties.resolve_layout_transform(transform);
            let perspective =
                stacking_context.perspective.unwrap_or_else(LayoutTransform::identity);
            let transform =
                LayerToScrollTransform::create_translation(reference_frame_relative_offset.x,
                                                           reference_frame_relative_offset.y,
                                                           0.0)
                                        .pre_translate(bounds.origin.to_vector().to_3d())
                                        .pre_mul(&transform)
                                        .pre_mul(&perspective);

            let reference_frame_bounds = LayerRect::new(LayerPoint::zero(), bounds.size);
            let mut clip_id = context.apply_scroll_frame_id_replacement(context_scroll_node_id);
            clip_id = context.builder.push_reference_frame(Some(clip_id),
                                                           pipeline_id,
                                                           &reference_frame_bounds,
                                                           &transform,
                                                           &mut self.clip_scroll_tree);
            context.replacements.push((context_scroll_node_id, clip_id));
            reference_frame_relative_offset = LayerVector2D::zero();
        } else {
            reference_frame_relative_offset = LayerVector2D::new(
                reference_frame_relative_offset.x + bounds.origin.x,
                reference_frame_relative_offset.y + bounds.origin.y);
        }

        context.builder.push_stacking_context(&reference_frame_relative_offset,
                                              pipeline_id,
                                              composition_operations,
                                              stacking_context.transform_style);

        self.flatten_items(traversal,
                           pipeline_id,
                           context,
                           reference_frame_relative_offset);

        if stacking_context.scroll_policy == ScrollPolicy::Fixed {
            context.replacements.pop();
        }

        if is_reference_frame {
            context.replacements.pop();
            context.builder.pop_reference_frame();
        }

        context.builder.pop_stacking_context();
    }

    fn flatten_iframe<'a>(&mut self,
                          pipeline_id: PipelineId,
                          parent_id: ClipId,
                          bounds: &LayerRect,
                          local_clip: &LocalClip,
                          context: &mut FlattenContext,
                          reference_frame_relative_offset: LayerVector2D) {
        let pipeline = match context.scene.pipeline_map.get(&pipeline_id) {
            Some(pipeline) => pipeline,
            None => return,
        };

        let display_list = match context.scene.display_lists.get(&pipeline_id) {
            Some(display_list) => display_list,
            None => return,
        };

        let mut clip_region = ClipRegion::create_for_clip_node_with_local_clip(local_clip);
        clip_region.origin += reference_frame_relative_offset;
        let parent_pipeline_id = parent_id.pipeline_id();
        let clip_id = self.clip_scroll_tree.generate_new_clip_id(parent_pipeline_id);
        context.builder.add_clip_node(clip_id,
                                      parent_id,
                                      parent_pipeline_id,
                                      clip_region,
                                      &mut self.clip_scroll_tree);

        self.pipeline_epoch_map.insert(pipeline_id, pipeline.epoch);

        let iframe_rect = LayerRect::new(LayerPoint::zero(), bounds.size);
        let transform = LayerToScrollTransform::create_translation(
            reference_frame_relative_offset.x + bounds.origin.x,
            reference_frame_relative_offset.y + bounds.origin.y,
            0.0);

        let iframe_reference_frame_id =
            context.builder.push_reference_frame(Some(clip_id),
                                                 pipeline_id,
                                                 &iframe_rect,
                                                 &transform,
                                                 &mut self.clip_scroll_tree);

        context.builder.add_scroll_frame(
            ClipId::root_scroll_node(pipeline_id),
            iframe_reference_frame_id,
            pipeline_id,
            &iframe_rect,
            &pipeline.content_size,
            ScrollSensitivity::ScriptAndInputEvents,
            &mut self.clip_scroll_tree);

        self.flatten_root(&mut display_list.iter(), pipeline_id, context, &pipeline.content_size);

        context.builder.pop_reference_frame();
    }

    fn flatten_item<'a, 'b>(&mut self,
                            item: DisplayItemRef<'a, 'b>,
                            pipeline_id: PipelineId,
                            context: &mut FlattenContext,
                            reference_frame_relative_offset: LayerVector2D)
                            -> Option<BuiltDisplayListIter<'a>> {
        let mut clip_and_scroll = item.clip_and_scroll();
        context.convert_clip_scroll_info_to_nested(&mut clip_and_scroll);

        let unreplaced_scroll_id = clip_and_scroll.scroll_node_id;
        clip_and_scroll.scroll_node_id =
            context.apply_scroll_frame_id_replacement(clip_and_scroll.scroll_node_id);


        let item_rect_with_offset = item.rect().translate(&reference_frame_relative_offset);
        let clip_with_offset = item.local_clip_with_offset(&reference_frame_relative_offset);
        match *item.item() {
            SpecificDisplayItem::WebGL(ref info) => {
                context.builder.add_webgl_rectangle(clip_and_scroll,
                                                    item_rect_with_offset,
                                                    &clip_with_offset,
                                                    info.context_id);
            }
            SpecificDisplayItem::Image(ref info) => {
                let image = context.resource_cache.get_image_properties(info.image_key);
                if let Some(tile_size) = image.tiling {
                    // The image resource is tiled. We have to generate an image primitive
                    // for each tile.
                    let image_size = DeviceUintSize::new(image.descriptor.width,
                                                         image.descriptor.height);
                    self.decompose_image(clip_and_scroll,
                                         context,
                                         &item_rect_with_offset,
                                         &clip_with_offset,
                                         info,
                                         image_size,
                                         tile_size as u32);
                } else {
                    context.builder.add_image(clip_and_scroll,
                                              item_rect_with_offset,
                                              &clip_with_offset,
                                              &info.stretch_size,
                                              &info.tile_spacing,
                                              None,
                                              info.image_key,
                                              info.image_rendering,
                                              None);
                }
            }
            SpecificDisplayItem::YuvImage(ref info) => {
                context.builder.add_yuv_image(clip_and_scroll,
                                              item_rect_with_offset,
                                              &clip_with_offset,
                                              info.yuv_data,
                                              info.color_space,
                                              info.image_rendering);
            }
            SpecificDisplayItem::Text(ref text_info) => {
                context.builder.add_text(clip_and_scroll,
                                         reference_frame_relative_offset,
                                         item_rect_with_offset,
                                         &clip_with_offset,
                                         text_info.font_key,
                                         text_info.size,
                                         &text_info.color,
                                         item.glyphs(),
                                         item.display_list().get(item.glyphs()).count(),
                                         text_info.glyph_options);
            }
            SpecificDisplayItem::Rectangle(ref info) => {
                if !self.try_to_add_rectangle_splitting_on_clip(context,
                                                                &item_rect_with_offset,
                                                                &clip_with_offset,
                                                                &info.color,
                                                                &clip_and_scroll) {
                    context.builder.add_solid_rectangle(clip_and_scroll,
                                                        &item_rect_with_offset,
                                                        &clip_with_offset,
                                                        &info.color,
                                                        PrimitiveFlags::None);

                }
            }
            SpecificDisplayItem::Line(ref info) => {
                context.builder.add_line(clip_and_scroll,
                                         item.local_clip(),
                                         info.baseline,
                                         info.start,
                                         info.end,
                                         info.orientation,
                                         info.width,
                                         &info.color,
                                         info.style);
            }
            SpecificDisplayItem::Gradient(ref info) => {
                context.builder.add_gradient(clip_and_scroll,
                                             item_rect_with_offset,
                                             &clip_with_offset,
                                             info.gradient.start_point,
                                             info.gradient.end_point,
                                             item.gradient_stops(),
                                             item.display_list()
                                                 .get(item.gradient_stops()).count(),
                                             info.gradient.extend_mode,
                                             info.tile_size,
                                             info.tile_spacing);
            }
            SpecificDisplayItem::RadialGradient(ref info) => {
                context.builder.add_radial_gradient(clip_and_scroll,
                                                    item_rect_with_offset,
                                                    &clip_with_offset,
                                                    info.gradient.start_center,
                                                    info.gradient.start_radius,
                                                    info.gradient.end_center,
                                                    info.gradient.end_radius,
                                                    info.gradient.ratio_xy,
                                                    item.gradient_stops(),
                                                    info.gradient.extend_mode,
                                                    info.tile_size,
                                                    info.tile_spacing);
            }
            SpecificDisplayItem::BoxShadow(ref box_shadow_info) => {
                let bounds = box_shadow_info.box_bounds.translate(&reference_frame_relative_offset);
                context.builder.add_box_shadow(clip_and_scroll,
                                               &bounds,
                                               &clip_with_offset,
                                               &box_shadow_info.offset,
                                               &box_shadow_info.color,
                                               box_shadow_info.blur_radius,
                                               box_shadow_info.spread_radius,
                                               box_shadow_info.border_radius,
                                               box_shadow_info.clip_mode);
            }
            SpecificDisplayItem::Border(ref info) => {
                context.builder.add_border(clip_and_scroll,
                                           item_rect_with_offset,
                                           &clip_with_offset,
                                           info,
                                           item.gradient_stops(),
                                           item.display_list()
                                               .get(item.gradient_stops()).count());
            }
            SpecificDisplayItem::PushStackingContext(ref info) => {
                let mut subtraversal = item.sub_iter();
                self.flatten_stacking_context(&mut subtraversal,
                                              pipeline_id,
                                              context,
                                              unreplaced_scroll_id,
                                              reference_frame_relative_offset,
                                              &item.rect(),
                                              &info.stacking_context,
                                              item.filters());
                return Some(subtraversal);
            }
            SpecificDisplayItem::Iframe(ref info) => {
                self.flatten_iframe(info.pipeline_id,
                                    clip_and_scroll.scroll_node_id,
                                    &item.rect(),
                                    &item.local_clip(),
                                    context,
                                    reference_frame_relative_offset);
            }
            SpecificDisplayItem::Clip(ref info) => {
                let complex_clips = context.get_complex_clips(pipeline_id, item.complex_clip().0);
                let mut clip_region =
                    ClipRegion::create_for_clip_node(*item.local_clip().clip_rect(),
                                                     complex_clips,
                                                     info.image_mask);
                clip_region.origin += reference_frame_relative_offset;

                self.flatten_clip(context,
                                  pipeline_id,
                                  &clip_and_scroll.scroll_node_id,
                                  &info.id,
                                  clip_region);
            }
            SpecificDisplayItem::ScrollFrame(ref info) => {
                let complex_clips = context.get_complex_clips(pipeline_id, item.complex_clip().0);
                let mut clip_region =
                    ClipRegion::create_for_clip_node(*item.local_clip().clip_rect(),
                                                     complex_clips,
                                                     info.image_mask);
                clip_region.origin += reference_frame_relative_offset;

                // Just use clip rectangle as the frame rect for this scroll frame.
                // This is only interesting when calculating scroll extents for the
                // ClipScrollNode::scroll(..) API
                let frame_rect = item.local_clip()
                                     .clip_rect()
                                     .translate(&reference_frame_relative_offset);
                let content_rect = item.rect().translate(&reference_frame_relative_offset);
                self.flatten_scroll_frame(context,
                                          pipeline_id,
                                          &clip_and_scroll.scroll_node_id,
                                          &info.id,
                                          &frame_rect,
                                          &content_rect,
                                          clip_region,
                                          info.scroll_sensitivity);
            }
            SpecificDisplayItem::PushNestedDisplayList => {
                // Using the clip and scroll already processed for nesting here
                // means that in the case of multiple nested display lists, we
                // will enter the outermost ids into the table and avoid having
                // to do a replacement for every level of nesting.
                context.push_nested_display_list_ids(clip_and_scroll);
            }
            SpecificDisplayItem::PopNestedDisplayList => context.pop_nested_display_list_ids(),

            // Do nothing; these are dummy items for the display list parser
            SpecificDisplayItem::SetGradientStops => { }

            SpecificDisplayItem::PopStackingContext =>
                unreachable!("Should have returned in parent method."),
            SpecificDisplayItem::PushTextShadow(shadow) => {
                context.builder.push_text_shadow(shadow,
                                                 clip_and_scroll,
                                                 &clip_with_offset);
            }
            SpecificDisplayItem::PopTextShadow => {
                context.builder.pop_text_shadow();
            }
        }
        None
    }

    /// Try to optimize the rendering of a solid rectangle that is clipped by a single
    /// rounded rectangle, by only masking the parts of the rectangle that intersect
    /// the rounded parts of the clip. This is pretty simple now, so has a lot of
    /// potential for further optimizations.
    fn try_to_add_rectangle_splitting_on_clip(&mut self,
                                              context: &mut FlattenContext,
                                              rect: &LayerRect,
                                              local_clip: &LocalClip,
                                              color: &ColorF,
                                              clip_and_scroll: &ClipAndScrollInfo)
                                              -> bool {
        // If this rectangle is not opaque, splitting the rectangle up
        // into an inner opaque region just ends up hurting batching and
        // doing more work than necessary.
        if color.a != 1.0 {
            return false;
        }

        let inner_unclipped_rect = match local_clip {
            &LocalClip::Rect(_) => return false,
            &LocalClip::RoundedRect(_, ref region) => region.get_inner_rect_full(),
        };
        let inner_unclipped_rect = match inner_unclipped_rect {
            Some(rect) => rect,
            None => return false,
        };

        // The inner rectangle is not clipped by its assigned clipping node, so we can
        // let it be clipped by the parent of the clipping node, which may result in
        // less masking some cases.
        let mut clipped_rects = Vec::new();
        subtract_rect(rect, &inner_unclipped_rect, &mut clipped_rects);

        context.builder.add_solid_rectangle(*clip_and_scroll,
                                            &inner_unclipped_rect,
                                            &LocalClip::from(*local_clip.clip_rect()),
                                            color,
                                            PrimitiveFlags::None);

        for clipped_rect in &clipped_rects {
            context.builder.add_solid_rectangle(*clip_and_scroll,
                                                clipped_rect,
                                                local_clip,
                                                color,
                                                PrimitiveFlags::None);
        }
        true
    }

    fn flatten_root<'a>(&mut self,
                        traversal: &mut BuiltDisplayListIter<'a>,
                        pipeline_id: PipelineId,
                        context: &mut FlattenContext,
                        content_size: &LayoutSize) {
        context.builder.push_stacking_context(&LayerVector2D::zero(),
                                              pipeline_id,
                                              CompositeOps::default(),
                                              TransformStyle::Flat);

        // We do this here, rather than above because we want any of the top-level
        // stacking contexts in the display list to be treated like root stacking contexts.
        // FIXME(mrobinson): Currently only the first one will, which for the moment is
        // sufficient for all our use cases.
        context.builder.notify_waiting_for_root_stacking_context();

        // For the root pipeline, there's no need to add a full screen rectangle
        // here, as it's handled by the framebuffer clear.
        let clip_id = ClipId::root_scroll_node(pipeline_id);
        if context.scene.root_pipeline_id != Some(pipeline_id) {
            if let Some(pipeline) = context.scene.pipeline_map.get(&pipeline_id) {
                if let Some(bg_color) = pipeline.background_color {
                    let root_bounds = LayerRect::new(LayerPoint::zero(), *content_size);
                    context.builder.add_solid_rectangle(ClipAndScrollInfo::simple(clip_id),
                                                        &root_bounds,
                                                        &LocalClip::from(root_bounds),
                                                        &bg_color,
                                                        PrimitiveFlags::None);
                }
            }
        }


        self.flatten_items(traversal, pipeline_id, context, LayerVector2D::zero());

        if self.frame_builder_config.enable_scrollbars {
            let scrollbar_rect = LayerRect::new(LayerPoint::zero(), LayerSize::new(10.0, 70.0));
            context.builder.add_solid_rectangle(
                ClipAndScrollInfo::simple(clip_id),
                &scrollbar_rect,
                &LocalClip::from(scrollbar_rect),
                &DEFAULT_SCROLLBAR_COLOR,
                PrimitiveFlags::Scrollbar(self.clip_scroll_tree.topmost_scrolling_node_id(), 4.0));
        }

        context.builder.pop_stacking_context();
    }

    fn flatten_items<'a>(&mut self,
                         traversal: &mut BuiltDisplayListIter<'a>,
                         pipeline_id: PipelineId,
                         context: &mut FlattenContext,
                         reference_frame_relative_offset: LayerVector2D) {
        loop {
            let subtraversal = {
                let item = match traversal.next() {
                    Some(item) => item,
                    None => break,
                };

                if SpecificDisplayItem::PopStackingContext == *item.item() {
                    return;
                }

                self.flatten_item(item, pipeline_id, context, reference_frame_relative_offset)
            };

            // If flatten_item created a sub-traversal, we need `traversal` to have the
            // same state as the completed subtraversal, so we reinitialize it here.
            if let Some(subtraversal) = subtraversal {
                *traversal = subtraversal;
            }
        }
    }

    /// Decomposes an image display item that is repeated into an image per individual repetition.
    /// We need to do this when we are unable to perform the repetition in the shader,
    /// for example if the image is tiled.
    ///
    /// In all of the "decompose" methods below, we independently handle horizontal and vertical
    /// decomposition. This lets us generate the minimum amount of primitives by, for  example,
    /// decompositing the repetition horizontally while repeating vertically in the shader (for
    /// an image where the width is too bug but the height is not).
    ///
    /// decompose_image and decompose_image_row handle image repetitions while decompose_tiled_image
    /// takes care of the decomposition required by the internal tiling of the image.
    fn decompose_image(&mut self,
                       clip_and_scroll: ClipAndScrollInfo,
                       context: &mut FlattenContext,
                       item_rect: &LayerRect,
                       item_local_clip: &LocalClip,
                       info: &ImageDisplayItem,
                       image_size: DeviceUintSize,
                       tile_size: u32) {
        let no_vertical_tiling = image_size.height <= tile_size;
        let no_vertical_spacing = info.tile_spacing.height == 0.0;
        if no_vertical_tiling && no_vertical_spacing {
            self.decompose_image_row(clip_and_scroll,
                                     context,
                                     item_rect,
                                     item_local_clip,
                                     info,
                                     image_size,
                                     tile_size);
            return;
        }

        // Decompose each vertical repetition into rows.
        let layout_stride = info.stretch_size.height + info.tile_spacing.height;
        let num_repetitions = (item_rect.size.height / layout_stride).ceil() as u32;
        for i in 0..num_repetitions {
            if let Some(row_rect) = rect(
                item_rect.origin.x,
                item_rect.origin.y + (i as f32) * layout_stride,
                item_rect.size.width,
                info.stretch_size.height
            ).intersection(item_rect) {
                self.decompose_image_row(clip_and_scroll,
                                         context,
                                         &row_rect,
                                         item_local_clip,
                                         info,
                                         image_size,
                                         tile_size);
            }
        }
    }

    fn decompose_image_row(&mut self,
                           clip_and_scroll: ClipAndScrollInfo,
                           context: &mut FlattenContext,
                           item_rect: &LayerRect,
                           item_local_clip: &LocalClip,
                           info: &ImageDisplayItem,
                           image_size: DeviceUintSize,
                           tile_size: u32) {
        let no_horizontal_tiling = image_size.width <= tile_size;
        let no_horizontal_spacing = info.tile_spacing.width == 0.0;
        if no_horizontal_tiling && no_horizontal_spacing {
            self.decompose_tiled_image(clip_and_scroll,
                                       context,
                                       item_rect,
                                       item_local_clip,
                                       info,
                                       image_size,
                                       tile_size);
            return;
        }

        // Decompose each horizontal repetition.
        let layout_stride = info.stretch_size.width + info.tile_spacing.width;
        let num_repetitions = (item_rect.size.width / layout_stride).ceil() as u32;
        for i in 0..num_repetitions {
            if let Some(decomposed_rect) = rect(
                item_rect.origin.x + (i as f32) * layout_stride,
                item_rect.origin.y,
                info.stretch_size.width,
                item_rect.size.height,
            ).intersection(item_rect) {
                self.decompose_tiled_image(clip_and_scroll,
                                           context,
                                           &decomposed_rect,
                                           item_local_clip,
                                           info,
                                           image_size,
                                           tile_size);
            }
        }
    }

    fn decompose_tiled_image(&mut self,
                             clip_and_scroll: ClipAndScrollInfo,
                             context: &mut FlattenContext,
                             item_rect: &LayerRect,
                             item_local_clip: &LocalClip,
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
        //
        // For the case where we don't tile along an axis, we can still perform the repetition in
        // the shader (for this particular axis), and it is worth special-casing for this to avoid
        // generating many primitives.
        // This can happen with very tall and thin images used as a repeating background.
        // Apparently web authors do that...

        let mut repeat_x = false;
        let mut repeat_y = false;

        if info.stretch_size.width < item_rect.size.width {
            // If this assert blows up it means we haven't properly decomposed the image in decompose_image_row.
            debug_assert!(image_size.width <= tile_size);
            // we don't actually tile in this dimmension so repeating can be done in the shader.
            repeat_x = true;
        }

        if info.stretch_size.height < item_rect.size.height {
            // If this assert blows up it means we haven't properly decomposed the image in decompose_image.
            debug_assert!(image_size.height <= tile_size);
            // we don't actually tile in this dimmension so repeating can be done in the shader.
            repeat_y = true;
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
            img_dw * info.stretch_size.width,
            img_dh * info.stretch_size.height
        );

        // The size in pixels of the tiles on the right and bottom edges, smaller
        // than the regular tile size if the image is not a multiple of the tile size.
        // Zero means the image size is a multiple of the tile size.
        let leftover = DeviceUintSize::new(image_size.width % tile_size, image_size.height % tile_size);

        for ty in 0..num_tiles_y {
            for tx in 0..num_tiles_x {
                self.add_tile_primitive(clip_and_scroll,
                                        context,
                                        item_rect,
                                        item_local_clip,
                                        info,
                                        TileOffset::new(tx, ty),
                                        stretched_tile_size,
                                        1.0, 1.0,
                                        repeat_x, repeat_y);
            }
            if leftover.width != 0 {
                // Tiles on the right edge that are smaller than the tile size.
                self.add_tile_primitive(clip_and_scroll,
                                        context,
                                        item_rect,
                                        item_local_clip,
                                        info,
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
                self.add_tile_primitive(clip_and_scroll,
                                        context,
                                        item_rect,
                                        item_local_clip,
                                        info,
                                        TileOffset::new(tx, num_tiles_y),
                                        stretched_tile_size,
                                        1.0,
                                        (leftover.height as f32) / tile_size_f32,
                                        repeat_x,
                                        repeat_y);
            }

            if leftover.width != 0 {
                // Finally, the bottom-right tile with a "leftover" size.
                self.add_tile_primitive(clip_and_scroll,
                                        context,
                                        item_rect,
                                        item_local_clip,
                                        info,
                                        TileOffset::new(num_tiles_x, num_tiles_y),
                                        stretched_tile_size,
                                        (leftover.width as f32) / tile_size_f32,
                                        (leftover.height as f32) / tile_size_f32,
                                        repeat_x,
                                        repeat_y);
            }
        }
    }

    fn add_tile_primitive(&mut self,
                          clip_and_scroll: ClipAndScrollInfo,
                          context: &mut FlattenContext,
                          item_rect: &LayerRect,
                          item_local_clip: &LocalClip,
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
            item_rect.origin + LayerVector2D::new(
                tile_offset.x as f32 * stretched_tile_size.width,
                tile_offset.y as f32 * stretched_tile_size.height,
            ),
            stretched_size,
        );

        if repeat_x {
            assert_eq!(tile_offset.x, 0);
            prim_rect.size.width = item_rect.size.width;
        }

        if repeat_y {
            assert_eq!(tile_offset.y, 0);
            prim_rect.size.height = item_rect.size.height;
        }

        // Fix up the primitive's rect if it overflows the original item rect.
        if let Some(prim_rect) = prim_rect.intersection(item_rect) {
            context.builder.add_image(clip_and_scroll,
                                      prim_rect,
                                      item_local_clip,
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
                 gpu_cache: &mut GpuCache,
                 display_lists: &DisplayListMap,
                 device_pixel_ratio: f32,
                 pan: LayerPoint,
                 texture_cache_profile: &mut TextureCacheProfileCounters,
                 gpu_cache_profile: &mut GpuCacheProfileCounters)
                 -> RendererFrame {
        self.clip_scroll_tree.update_all_node_transforms(pan);
        let frame = self.build_frame(resource_cache,
                                     gpu_cache,
                                     display_lists,
                                     device_pixel_ratio,
                                     texture_cache_profile,
                                     gpu_cache_profile);
        // Expire any resources that haven't been used for `cache_expiry_frames`.
        let num_frames_back = self.frame_builder_config.cache_expiry_frames;
        let expiry_frame = FrameId(cmp::max(num_frames_back, self.id.0) - num_frames_back);
        resource_cache.expire_old_resources(expiry_frame);
        frame
    }

    fn build_frame(&mut self,
                   resource_cache: &mut ResourceCache,
                   gpu_cache: &mut GpuCache,
                   display_lists: &DisplayListMap,
                   device_pixel_ratio: f32,
                   texture_cache_profile: &mut TextureCacheProfileCounters,
                   gpu_cache_profile: &mut GpuCacheProfileCounters)
                   -> RendererFrame {
        let mut frame_builder = self.frame_builder.take();
        let frame = frame_builder.as_mut().map(|builder|
            builder.build(resource_cache,
                          gpu_cache,
                          self.id,
                          &mut self.clip_scroll_tree,
                          display_lists,
                          device_pixel_ratio,
                          texture_cache_profile,
                          gpu_cache_profile)
        );
        self.frame_builder = frame_builder;

        let nodes_bouncing_back = self.clip_scroll_tree.collect_nodes_bouncing_back();
        RendererFrame::new(self.pipeline_epoch_map.clone(), nodes_bouncing_back, frame)
    }
}
