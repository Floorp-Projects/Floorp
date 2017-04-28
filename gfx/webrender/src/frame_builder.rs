/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use app_units::Au;
use frame::FrameId;
use gpu_store::GpuStoreAddress;
use internal_types::{HardwareCompositeOp, SourceTexture};
use mask_cache::{ClipMode, ClipSource, MaskCacheInfo, RegionMode};
use prim_store::{GradientPrimitiveCpu, GradientPrimitiveGpu, ImagePrimitiveCpu, ImagePrimitiveGpu};
use prim_store::{ImagePrimitiveKind, PrimitiveContainer, PrimitiveGeometry, PrimitiveIndex};
use prim_store::{PrimitiveStore, RadialGradientPrimitiveCpu, RadialGradientPrimitiveGpu};
use prim_store::{RectanglePrimitive, TextRunPrimitiveCpu, TextRunPrimitiveGpu};
use prim_store::{BoxShadowPrimitiveGpu, TexelRect, YuvImagePrimitiveCpu, YuvImagePrimitiveGpu};
use profiler::{FrameProfileCounters, TextureCacheProfileCounters};
use render_task::{AlphaRenderItem, MaskCacheKey, MaskResult, RenderTask, RenderTaskIndex};
use render_task::RenderTaskLocation;
use resource_cache::ResourceCache;
use clip_scroll_node::{ClipInfo, ClipScrollNode, NodeType};
use clip_scroll_tree::ClipScrollTree;
use std::{cmp, f32, i32, mem, usize};
use euclid::SideOffsets2D;
use tiling::StackingContextIndex;
use tiling::{AuxiliaryListsMap, ClipScrollGroup, ClipScrollGroupIndex, CompositeOps, Frame};
use tiling::{PackedLayer, PackedLayerIndex, PrimitiveFlags, PrimitiveRunCmd, RenderPass};
use tiling::{RenderTargetContext, RenderTaskCollection, ScrollbarPrimitive, StackingContext};
use util::{self, pack_as_float, subtract_rect, recycle_vec};
use util::RectHelpers;
use webrender_traits::{BorderDetails, BorderDisplayItem, BoxShadowClipMode, ClipAndScrollInfo};
use webrender_traits::{ClipId, ClipRegion, ColorF, DeviceIntPoint, DeviceIntRect, DeviceIntSize};
use webrender_traits::{DeviceUintRect, DeviceUintSize, ExtendMode, FontKey, FontRenderMode};
use webrender_traits::{GlyphOptions, ImageKey, ImageRendering, ItemRange, LayerPoint, LayerRect};
use webrender_traits::{LayerSize, LayerToScrollTransform, PipelineId, RepeatMode, TileOffset};
use webrender_traits::{TransformStyle, WebGLContextId, YuvColorSpace, YuvData};

#[derive(Debug, Clone)]
struct ImageBorderSegment {
    geom_rect: LayerRect,
    sub_rect: TexelRect,
    stretch_size: LayerSize,
    tile_spacing: LayerSize,
}

impl ImageBorderSegment {
    fn new(rect: LayerRect,
           sub_rect: TexelRect,
           repeat_horizontal: RepeatMode,
           repeat_vertical: RepeatMode) -> ImageBorderSegment {
        let tile_spacing = LayerSize::zero();

        debug_assert!(sub_rect.uv1.x >= sub_rect.uv0.x);
        debug_assert!(sub_rect.uv1.y >= sub_rect.uv0.y);

        let image_size = LayerSize::new(sub_rect.uv1.x - sub_rect.uv0.x,
                                        sub_rect.uv1.y - sub_rect.uv0.y);

        let stretch_size_x = match repeat_horizontal {
            RepeatMode::Stretch => rect.size.width,
            RepeatMode::Repeat => image_size.width,
            RepeatMode::Round | RepeatMode::Space => {
                println!("Round/Space not supported yet!");
                rect.size.width
            }
        };

        let stretch_size_y = match repeat_vertical {
            RepeatMode::Stretch => rect.size.height,
            RepeatMode::Repeat => image_size.height,
            RepeatMode::Round | RepeatMode::Space => {
                println!("Round/Space not supported yet!");
                rect.size.height
            }
        };

        ImageBorderSegment {
            geom_rect: rect,
            sub_rect: sub_rect,
            stretch_size: LayerSize::new(stretch_size_x, stretch_size_y),
            tile_spacing: tile_spacing,
        }
    }
}

#[derive(Clone, Copy)]
pub struct FrameBuilderConfig {
    pub enable_scrollbars: bool,
    pub enable_subpixel_aa: bool,
    pub debug: bool,
}

impl FrameBuilderConfig {
    pub fn new(enable_scrollbars: bool,
               enable_subpixel_aa: bool,
               debug: bool)
               -> FrameBuilderConfig {
        FrameBuilderConfig {
            enable_scrollbars: enable_scrollbars,
            enable_subpixel_aa: enable_subpixel_aa,
            debug: debug,
        }
    }
}

pub struct FrameBuilder {
    screen_size: DeviceUintSize,
    background_color: Option<ColorF>,
    prim_store: PrimitiveStore,
    cmds: Vec<PrimitiveRunCmd>,
    config: FrameBuilderConfig,

    stacking_context_store: Vec<StackingContext>,
    clip_scroll_group_store: Vec<ClipScrollGroup>,
    packed_layers: Vec<PackedLayer>,

    scrollbar_prims: Vec<ScrollbarPrimitive>,

    /// A stack of scroll nodes used during display list processing to properly
    /// parent new scroll nodes.
    reference_frame_stack: Vec<ClipId>,

    /// A stack of stacking contexts used for creating ClipScrollGroups as
    /// primitives are added to the frame.
    stacking_context_stack: Vec<StackingContextIndex>,
}

impl FrameBuilder {
    pub fn new(previous: Option<FrameBuilder>,
               screen_size: DeviceUintSize,
               background_color: Option<ColorF>,
               config: FrameBuilderConfig) -> FrameBuilder {
        match previous {
            Some(prev) => {
                FrameBuilder {
                    stacking_context_store: recycle_vec(prev.stacking_context_store),
                    clip_scroll_group_store: recycle_vec(prev.clip_scroll_group_store),
                    cmds: recycle_vec(prev.cmds),
                    packed_layers: recycle_vec(prev.packed_layers),
                    scrollbar_prims: recycle_vec(prev.scrollbar_prims),
                    reference_frame_stack: recycle_vec(prev.reference_frame_stack),
                    stacking_context_stack: recycle_vec(prev.stacking_context_stack),
                    prim_store: prev.prim_store.recycle(),
                    screen_size: screen_size,
                    background_color: background_color,
                    config: config,
                }
            }
            None => {
                FrameBuilder {
                    stacking_context_store: Vec::new(),
                    clip_scroll_group_store: Vec::new(),
                    cmds: Vec::new(),
                    packed_layers: Vec::new(),
                    scrollbar_prims: Vec::new(),
                    reference_frame_stack: Vec::new(),
                    stacking_context_stack: Vec::new(),
                    prim_store: PrimitiveStore::new(),
                    screen_size: screen_size,
                    background_color: background_color,
                    config: config,
                }
            }
        }
    }

    pub fn create_clip_scroll_group_if_necessary(&mut self,
                                                 stacking_context_index: StackingContextIndex,
                                                 info: ClipAndScrollInfo) {
        if self.stacking_context_store[stacking_context_index.0].has_clip_scroll_group(info) {
            return;
        }

        let group_index = self.create_clip_scroll_group(stacking_context_index, info);
        let stacking_context = &mut self.stacking_context_store[stacking_context_index.0];
        stacking_context.clip_scroll_groups.push(group_index);
    }

    pub fn add_primitive(&mut self,
                         clip_and_scroll: ClipAndScrollInfo,
                         rect: &LayerRect,
                         clip_region: &ClipRegion,
                         extra_clips: &[ClipSource],
                         container: PrimitiveContainer)
                         -> PrimitiveIndex {
        let stacking_context_index = *self.stacking_context_stack.last().unwrap();

        self.create_clip_scroll_group_if_necessary(stacking_context_index, clip_and_scroll);

        let geometry = PrimitiveGeometry {
            local_rect: *rect,
            local_clip_rect: clip_region.main,
        };
        let mut clip_sources = Vec::new();
        if clip_region.is_complex() {
            clip_sources.push(ClipSource::Region(clip_region.clone(), RegionMode::ExcludeRect));
        }

        clip_sources.extend(extra_clips.iter().cloned());

        let clip_info = MaskCacheInfo::new(&clip_sources,
                                           &mut self.prim_store.gpu_data32);

        let prim_index = self.prim_store.add_primitive(geometry,
                                                       clip_sources,
                                                       clip_info,
                                                       container);

        match self.cmds.last_mut().unwrap() {
            &mut PrimitiveRunCmd::PrimitiveRun(_run_prim_index, ref mut count, run_clip_and_scroll)
                if run_clip_and_scroll == clip_and_scroll => {
                    debug_assert!(_run_prim_index.0 + *count == prim_index.0);
                    *count += 1;
                    return prim_index;
            }
            &mut PrimitiveRunCmd::PrimitiveRun(..) |
            &mut PrimitiveRunCmd::PushStackingContext(..) |
            &mut PrimitiveRunCmd::PopStackingContext => {}
        }

        self.cmds.push(PrimitiveRunCmd::PrimitiveRun(prim_index, 1, clip_and_scroll));
        prim_index
    }

    pub fn create_clip_scroll_group(&mut self,
                                    stacking_context_index: StackingContextIndex,
                                    info: ClipAndScrollInfo)
                                    -> ClipScrollGroupIndex {
        let packed_layer_index = PackedLayerIndex(self.packed_layers.len());
        self.packed_layers.push(PackedLayer::empty());

        self.clip_scroll_group_store.push(ClipScrollGroup {
            stacking_context_index: stacking_context_index,
            scroll_node_id: info.scroll_node_id,
            clip_node_id: info.clip_node_id(),
            packed_layer_index: packed_layer_index,
            screen_bounding_rect: None,
         });

        ClipScrollGroupIndex(self.clip_scroll_group_store.len() - 1, info)
    }

    pub fn push_stacking_context(&mut self,
                                 reference_frame_offset: &LayerPoint,
                                 pipeline_id: PipelineId,
                                 is_page_root: bool,
                                 composite_ops: CompositeOps,
                                 transform_style: TransformStyle) {
        if let Some(parent_index) = self.stacking_context_stack.last() {
            let parent_is_root = self.stacking_context_store[parent_index.0].is_page_root;

            if composite_ops.mix_blend_mode.is_some() && !parent_is_root {
                // the parent stacking context of a stacking context with mix-blend-mode
                // must be drawn with a transparent background, unless the parent stacking context
                // is the root of the page
                self.stacking_context_store[parent_index.0].should_isolate = true;
            }
        }

        let stacking_context_index = StackingContextIndex(self.stacking_context_store.len());
        self.stacking_context_store.push(StackingContext::new(pipeline_id,
                                                              *reference_frame_offset,
                                                              is_page_root,
                                                              transform_style,
                                                              composite_ops));
        self.cmds.push(PrimitiveRunCmd::PushStackingContext(stacking_context_index));
        self.stacking_context_stack.push(stacking_context_index);
    }

    pub fn pop_stacking_context(&mut self) {
        self.cmds.push(PrimitiveRunCmd::PopStackingContext);
        self.stacking_context_stack.pop();
    }

    pub fn push_reference_frame(&mut self,
                                parent_id: Option<ClipId>,
                                pipeline_id: PipelineId,
                                rect: &LayerRect,
                                transform: &LayerToScrollTransform,
                                clip_scroll_tree: &mut ClipScrollTree)
                                -> ClipId {
        let new_id = clip_scroll_tree.add_reference_frame(rect, transform, pipeline_id, parent_id);
        self.reference_frame_stack.push(new_id);
        new_id
    }

    pub fn current_reference_frame_id(&self) -> ClipId {
        *self.reference_frame_stack.last().unwrap()
    }

    pub fn setup_viewport_offset(&mut self,
                                 window_size: DeviceUintSize,
                                 inner_rect: DeviceUintRect,
                                 device_pixel_ratio: f32,
                                 clip_scroll_tree: &mut ClipScrollTree) {
        let inner_origin = inner_rect.origin.to_f32();
        let viewport_offset = LayerPoint::new((inner_origin.x / device_pixel_ratio).round(),
                                              (inner_origin.y / device_pixel_ratio).round());
        let outer_size = window_size.to_f32();
        let outer_size = LayerSize::new((outer_size.width / device_pixel_ratio).round(),
                                        (outer_size.height / device_pixel_ratio).round());
        let clip_size = LayerSize::new(outer_size.width + 2.0 * viewport_offset.x,
                                       outer_size.height + 2.0 * viewport_offset.y);

        let viewport_clip = LayerRect::new(LayerPoint::new(-viewport_offset.x, -viewport_offset.y),
                                           LayerSize::new(clip_size.width, clip_size.height));

        let root_id = clip_scroll_tree.root_reference_frame_id();
        if let Some(root_node) = clip_scroll_tree.nodes.get_mut(&root_id) {
            if let NodeType::ReferenceFrame(ref mut transform) = root_node.node_type {
                *transform = LayerToScrollTransform::create_translation(viewport_offset.x,
                                                                        viewport_offset.y,
                                                                        0.0);
            }
            root_node.local_clip_rect = viewport_clip;
        }

        let clip_id = clip_scroll_tree.topmost_scrolling_node_id();
        if let Some(root_node) = clip_scroll_tree.nodes.get_mut(&clip_id) {
            root_node.local_clip_rect = viewport_clip;
        }
    }

    pub fn push_root(&mut self,
                     pipeline_id: PipelineId,
                     viewport_size: &LayerSize,
                     content_size: &LayerSize,
                     clip_scroll_tree: &mut ClipScrollTree)
                     -> ClipId {
        let viewport_rect = LayerRect::new(LayerPoint::zero(), *viewport_size);
        let identity = &LayerToScrollTransform::identity();
        self.push_reference_frame(None, pipeline_id, &viewport_rect, identity, clip_scroll_tree);

        let topmost_scrolling_node_id = ClipId::root_scroll_node(pipeline_id);
        clip_scroll_tree.topmost_scrolling_node_id = topmost_scrolling_node_id;
        self.add_clip_scroll_node(topmost_scrolling_node_id,
                                   clip_scroll_tree.root_reference_frame_id,
                                   pipeline_id,
                                   &LayerRect::new(LayerPoint::zero(), *content_size),
                                   &ClipRegion::simple(&viewport_rect),
                                   clip_scroll_tree);
        topmost_scrolling_node_id
    }

    pub fn add_clip_scroll_node(&mut self,
                                new_node_id: ClipId,
                                parent_id: ClipId,
                                pipeline_id: PipelineId,
                                content_rect: &LayerRect,
                                clip_region: &ClipRegion,
                                clip_scroll_tree: &mut ClipScrollTree) {
        let clip_info = ClipInfo::new(clip_region,
                                      &mut self.prim_store.gpu_data32,
                                      PackedLayerIndex(self.packed_layers.len()));
        let node = ClipScrollNode::new(pipeline_id,
                                       parent_id,
                                       content_rect,
                                       &clip_region.main,
                                       clip_info);

        clip_scroll_tree.add_node(node, new_node_id);
        self.packed_layers.push(PackedLayer::empty());
    }

    pub fn pop_reference_frame(&mut self) {
        self.reference_frame_stack.pop();
    }

    pub fn add_solid_rectangle(&mut self,
                               clip_and_scroll: ClipAndScrollInfo,
                               rect: &LayerRect,
                               clip_region: &ClipRegion,
                               color: &ColorF,
                               flags: PrimitiveFlags) {
        if color.a == 0.0 {
            return;
        }

        let prim = RectanglePrimitive {
            color: *color,
        };

        let prim_index = self.add_primitive(clip_and_scroll,
                                            rect,
                                            clip_region,
                                            &[],
                                            PrimitiveContainer::Rectangle(prim));

        match flags {
            PrimitiveFlags::None => {}
            PrimitiveFlags::Scrollbar(clip_id, border_radius) => {
                self.scrollbar_prims.push(ScrollbarPrimitive {
                    prim_index: prim_index,
                    clip_id: clip_id,
                    border_radius: border_radius,
                });
            }
        }
    }

    pub fn add_border(&mut self,
                      clip_and_scroll: ClipAndScrollInfo,
                      rect: LayerRect,
                      clip_region: &ClipRegion,
                      border_item: &BorderDisplayItem) {
        let create_segments = |outset: SideOffsets2D<f32>| {
            // Calculate the modified rect as specific by border-image-outset
            let origin = LayerPoint::new(rect.origin.x - outset.left,
                                         rect.origin.y - outset.top);
            let size = LayerSize::new(rect.size.width + outset.left + outset.right,
                                      rect.size.height + outset.top + outset.bottom);
            let rect = LayerRect::new(origin, size);

            let tl_outer = LayerPoint::new(rect.origin.x, rect.origin.y);
            let tl_inner = tl_outer + LayerPoint::new(border_item.widths.left, border_item.widths.top);

            let tr_outer = LayerPoint::new(rect.origin.x + rect.size.width, rect.origin.y);
            let tr_inner = tr_outer + LayerPoint::new(-border_item.widths.right, border_item.widths.top);

            let bl_outer = LayerPoint::new(rect.origin.x, rect.origin.y + rect.size.height);
            let bl_inner = bl_outer + LayerPoint::new(border_item.widths.left, -border_item.widths.bottom);

            let br_outer = LayerPoint::new(rect.origin.x + rect.size.width,
                                           rect.origin.y + rect.size.height);
            let br_inner = br_outer - LayerPoint::new(border_item.widths.right, border_item.widths.bottom);

            // Build the list of gradient segments
            vec![
                // Top left
                LayerRect::from_floats(tl_outer.x, tl_outer.y, tl_inner.x, tl_inner.y),
                // Top right
                LayerRect::from_floats(tr_inner.x, tr_outer.y, tr_outer.x, tr_inner.y),
                // Bottom right
                LayerRect::from_floats(br_inner.x, br_inner.y, br_outer.x, br_outer.y),
                // Bottom left
                LayerRect::from_floats(bl_outer.x, bl_inner.y, bl_inner.x, bl_outer.y),
                // Top
                LayerRect::from_floats(tl_inner.x, tl_outer.y, tr_inner.x, tl_inner.y),
                // Bottom
                LayerRect::from_floats(bl_inner.x, bl_inner.y, br_inner.x, bl_outer.y),
                // Left
                LayerRect::from_floats(tl_outer.x, tl_inner.y, tl_inner.x, bl_inner.y),
                // Right
                LayerRect::from_floats(tr_inner.x, tr_inner.y, br_outer.x, br_inner.y),
            ]
        };

        match border_item.details {
            BorderDetails::Image(ref border) => {
                // Calculate the modified rect as specific by border-image-outset
                let origin = LayerPoint::new(rect.origin.x - border.outset.left,
                                             rect.origin.y - border.outset.top);
                let size = LayerSize::new(rect.size.width + border.outset.left + border.outset.right,
                                          rect.size.height + border.outset.top + border.outset.bottom);
                let rect = LayerRect::new(origin, size);

                // Calculate the local texel coords of the slices.
                let px0 = 0;
                let px1 = border.patch.slice.left;
                let px2 = border.patch.width - border.patch.slice.right;
                let px3 = border.patch.width;

                let py0 = 0;
                let py1 = border.patch.slice.top;
                let py2 = border.patch.height - border.patch.slice.bottom;
                let py3 = border.patch.height;

                let tl_outer = LayerPoint::new(rect.origin.x, rect.origin.y);
                let tl_inner = tl_outer + LayerPoint::new(border_item.widths.left, border_item.widths.top);

                let tr_outer = LayerPoint::new(rect.origin.x + rect.size.width, rect.origin.y);
                let tr_inner = tr_outer + LayerPoint::new(-border_item.widths.right, border_item.widths.top);

                let bl_outer = LayerPoint::new(rect.origin.x, rect.origin.y + rect.size.height);
                let bl_inner = bl_outer + LayerPoint::new(border_item.widths.left, -border_item.widths.bottom);

                let br_outer = LayerPoint::new(rect.origin.x + rect.size.width,
                                               rect.origin.y + rect.size.height);
                let br_inner = br_outer - LayerPoint::new(border_item.widths.right, border_item.widths.bottom);

                // Build the list of image segments
                let mut segments = vec![
                    // Top left
                    ImageBorderSegment::new(LayerRect::from_floats(tl_outer.x, tl_outer.y, tl_inner.x, tl_inner.y),
                                            TexelRect::new(px0, py0, px1, py1),
                                            RepeatMode::Stretch,
                                            RepeatMode::Stretch),

                    // Top right
                    ImageBorderSegment::new(LayerRect::from_floats(tr_inner.x, tr_outer.y, tr_outer.x, tr_inner.y),
                                            TexelRect::new(px2, py0, px3, py1),
                                            RepeatMode::Stretch,
                                            RepeatMode::Stretch),

                    // Bottom right
                    ImageBorderSegment::new(LayerRect::from_floats(br_inner.x, br_inner.y, br_outer.x, br_outer.y),
                                            TexelRect::new(px2, py2, px3, py3),
                                            RepeatMode::Stretch,
                                            RepeatMode::Stretch),

                    // Bottom left
                    ImageBorderSegment::new(LayerRect::from_floats(bl_outer.x, bl_inner.y, bl_inner.x, bl_outer.y),
                                            TexelRect::new(px0, py2, px1, py3),
                                            RepeatMode::Stretch,
                                            RepeatMode::Stretch),
                ];

                // Add edge segments if valid size.
                if px1 < px2 && py1 < py2 {
                    segments.extend_from_slice(&[
                        // Top
                        ImageBorderSegment::new(LayerRect::from_floats(tl_inner.x, tl_outer.y, tr_inner.x, tl_inner.y),
                                                TexelRect::new(px1, py0, px2, py1),
                                                border.repeat_horizontal,
                                                RepeatMode::Stretch),

                        // Bottom
                        ImageBorderSegment::new(LayerRect::from_floats(bl_inner.x, bl_inner.y, br_inner.x, bl_outer.y),
                                                TexelRect::new(px1, py2, px2, py3),
                                                border.repeat_horizontal,
                                                RepeatMode::Stretch),

                        // Left
                        ImageBorderSegment::new(LayerRect::from_floats(tl_outer.x, tl_inner.y, tl_inner.x, bl_inner.y),
                                                TexelRect::new(px0, py1, px1, py2),
                                                RepeatMode::Stretch,
                                                border.repeat_vertical),

                        // Right
                        ImageBorderSegment::new(LayerRect::from_floats(tr_inner.x, tr_inner.y, br_outer.x, br_inner.y),
                                                TexelRect::new(px2, py1, px3, py2),
                                                RepeatMode::Stretch,
                                                border.repeat_vertical),
                    ]);
                }

                for segment in segments {
                    self.add_image(clip_and_scroll,
                                   segment.geom_rect,
                                   clip_region,
                                   &segment.stretch_size,
                                   &segment.tile_spacing,
                                   Some(segment.sub_rect),
                                   border.image_key,
                                   ImageRendering::Auto,
                                   None);
                }
            }
            BorderDetails::Normal(ref border) => {
                self.add_normal_border(&rect,
                                       border,
                                       &border_item.widths,
                                       clip_and_scroll,
                                       clip_region);
            }
            BorderDetails::Gradient(ref border) => {
                for segment in create_segments(border.outset) {
                    let segment_rel = segment.origin - rect.origin;

                    self.add_gradient(clip_and_scroll,
                                      segment,
                                      clip_region,
                                      border.gradient.start_point - segment_rel,
                                      border.gradient.end_point - segment_rel,
                                      border.gradient.stops,
                                      border.gradient.extend_mode,
                                      segment.size,
                                      LayerSize::zero());
                }
            }
            BorderDetails::RadialGradient(ref border) => {
                for segment in create_segments(border.outset) {
                    let segment_rel = segment.origin - rect.origin;

                    self.add_radial_gradient(clip_and_scroll,
                                             segment,
                                             clip_region,
                                             border.gradient.start_center - segment_rel,
                                             border.gradient.start_radius,
                                             border.gradient.end_center - segment_rel,
                                             border.gradient.end_radius,
                                             border.gradient.ratio_xy,
                                             border.gradient.stops,
                                             border.gradient.extend_mode,
                                             segment.size,
                                             LayerSize::zero());
                }
            }
        }
    }

    pub fn add_gradient(&mut self,
                        clip_and_scroll: ClipAndScrollInfo,
                        rect: LayerRect,
                        clip_region: &ClipRegion,
                        start_point: LayerPoint,
                        end_point: LayerPoint,
                        stops: ItemRange,
                        extend_mode: ExtendMode,
                        tile_size: LayerSize,
                        tile_spacing: LayerSize) {
        let tile_repeat = tile_size + tile_spacing;
        let is_not_tiled = tile_repeat.width >= rect.size.width &&
                           tile_repeat.height >= rect.size.height;

        let aligned_and_fills_rect = (start_point.x == end_point.x &&
                                      start_point.y.min(end_point.y) <= 0.0 &&
                                      start_point.y.max(end_point.y) >= rect.size.height) ||
                                     (start_point.y == end_point.y &&
                                      start_point.x.min(end_point.x) <= 0.0 &&
                                      start_point.x.max(end_point.x) >= rect.size.width);

        // Fast path for clamped, axis-aligned gradients, with gradient lines intersecting all of rect:
        let aligned = extend_mode == ExtendMode::Clamp && is_not_tiled && aligned_and_fills_rect;

        // Try to ensure that if the gradient is specified in reverse, then so long as the stops
        // are also supplied in reverse that the rendered result will be equivalent. To do this,
        // a reference orientation for the gradient line must be chosen, somewhat arbitrarily, so
        // just designate the reference orientation as start < end. Aligned gradient rendering
        // manages to produce the same result regardless of orientation, so don't worry about
        // reversing in that case.
        let reverse_stops = !aligned &&
                            (start_point.x > end_point.x ||
                             (start_point.x == end_point.x &&
                              start_point.y > end_point.y));

        let gradient_cpu = GradientPrimitiveCpu {
            stops_range: stops,
            extend_mode: extend_mode,
            reverse_stops: reverse_stops,
            cache_dirty: true,
        };

        // To get reftests exactly matching with reverse start/end
        // points, it's necessary to reverse the gradient
        // line in some cases.
        let (sp, ep) = if reverse_stops {
            (end_point, start_point)
        } else {
            (start_point, end_point)
        };

        let gradient_gpu = GradientPrimitiveGpu {
            start_point: sp,
            end_point: ep,
            extend_mode: pack_as_float(extend_mode as u32),
            tile_size: tile_size,
            tile_repeat: tile_repeat,
            padding: [0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0],
        };

        let prim = if aligned {
            PrimitiveContainer::AlignedGradient(gradient_cpu, gradient_gpu)
        } else {
            PrimitiveContainer::AngleGradient(gradient_cpu, gradient_gpu)
        };

        self.add_primitive(clip_and_scroll, &rect, clip_region, &[], prim);
    }

    pub fn add_radial_gradient(&mut self,
                               clip_and_scroll: ClipAndScrollInfo,
                               rect: LayerRect,
                               clip_region: &ClipRegion,
                               start_center: LayerPoint,
                               start_radius: f32,
                               end_center: LayerPoint,
                               end_radius: f32,
                               ratio_xy: f32,
                               stops: ItemRange,
                               extend_mode: ExtendMode,
                               tile_size: LayerSize,
                               tile_spacing: LayerSize) {
        let radial_gradient_cpu = RadialGradientPrimitiveCpu {
            stops_range: stops,
            extend_mode: extend_mode,
            cache_dirty: true,
        };

        let radial_gradient_gpu = RadialGradientPrimitiveGpu {
            start_center: start_center,
            end_center: end_center,
            start_radius: start_radius,
            end_radius: end_radius,
            ratio_xy: ratio_xy,
            extend_mode: pack_as_float(extend_mode as u32),
            tile_size: tile_size,
            tile_repeat: tile_size + tile_spacing,
            padding: [0.0, 0.0, 0.0, 0.0],
        };

        self.add_primitive(clip_and_scroll,
                           &rect,
                           clip_region,
                           &[],
                           PrimitiveContainer::RadialGradient(radial_gradient_cpu, radial_gradient_gpu));
    }

    pub fn add_text(&mut self,
                    clip_and_scroll: ClipAndScrollInfo,
                    rect: LayerRect,
                    clip_region: &ClipRegion,
                    font_key: FontKey,
                    size: Au,
                    blur_radius: Au,
                    color: &ColorF,
                    glyph_range: ItemRange,
                    glyph_options: Option<GlyphOptions>) {
        if color.a == 0.0 {
            return
        }

        if size.0 <= 0 {
            return
        }

        // TODO(gw): Use a proper algorithm to select
        // whether this item should be rendered with
        // subpixel AA!
        let render_mode = if blur_radius == Au(0) &&
                             self.config.enable_subpixel_aa {
            FontRenderMode::Subpixel
        } else {
            FontRenderMode::Alpha
        };

        let prim_cpu = TextRunPrimitiveCpu {
            font_key: font_key,
            logical_font_size: size,
            blur_radius: blur_radius,
            glyph_range: glyph_range,
            cache_dirty: true,
            glyph_instances: Vec::new(),
            color_texture_id: SourceTexture::Invalid,
            color: *color,
            render_mode: render_mode,
            glyph_options: glyph_options,
            resource_address: GpuStoreAddress(0),
        };

        let prim_gpu = TextRunPrimitiveGpu {
            color: *color,
        };

        self.add_primitive(clip_and_scroll,
                           &rect,
                           clip_region,
                           &[],
                           PrimitiveContainer::TextRun(prim_cpu, prim_gpu));
    }

    pub fn fill_box_shadow_rect(&mut self,
                                clip_and_scroll: ClipAndScrollInfo,
                                box_bounds: &LayerRect,
                                bs_rect: LayerRect,
                                clip_region: &ClipRegion,
                                color: &ColorF,
                                border_radius: f32,
                                clip_mode: BoxShadowClipMode) {
        // We can draw a rectangle instead with the proper border radius clipping.
        let (bs_clip_mode, rect_to_draw) = match clip_mode {
            BoxShadowClipMode::Outset |
            BoxShadowClipMode::None => (ClipMode::Clip, bs_rect),
            BoxShadowClipMode::Inset => (ClipMode::ClipOut, *box_bounds),
        };

        let box_clip_mode = !bs_clip_mode;

        // Clip the inside
        let extra_clips = [ClipSource::Complex(bs_rect,
                                               border_radius,
                                               bs_clip_mode),
                           // Clip the outside of the box
                           ClipSource::Complex(*box_bounds,
                                             border_radius,
                                             box_clip_mode)];

        let prim = RectanglePrimitive {
            color: *color,
        };

        self.add_primitive(clip_and_scroll,
                           &rect_to_draw,
                           clip_region,
                           &extra_clips,
                           PrimitiveContainer::Rectangle(prim));
    }

    pub fn add_box_shadow(&mut self,
                          clip_and_scroll: ClipAndScrollInfo,
                          box_bounds: &LayerRect,
                          clip_region: &ClipRegion,
                          box_offset: &LayerPoint,
                          color: &ColorF,
                          blur_radius: f32,
                          spread_radius: f32,
                          border_radius: f32,
                          clip_mode: BoxShadowClipMode) {
        if color.a == 0.0 {
            return
        }

        // The local space box shadow rect. It is the element rect
        // translated by the box shadow offset and inflated by the
        // box shadow spread.
        let inflate_amount = match clip_mode {
            BoxShadowClipMode::Outset | BoxShadowClipMode::None => spread_radius,
            BoxShadowClipMode::Inset => -spread_radius,
        };

        let bs_rect = box_bounds.translate(box_offset)
                                .inflate(inflate_amount, inflate_amount);
        // If we have negative inflate amounts.
        // Have to explicitly check this since euclid::TypedRect relies on negative rects
        let bs_rect_empty = bs_rect.size.width <= 0.0 || bs_rect.size.height <= 0.0;

        // Just draw a rectangle
        if (blur_radius == 0.0 && spread_radius == 0.0 && clip_mode == BoxShadowClipMode::None)
           || bs_rect_empty {
            self.add_solid_rectangle(clip_and_scroll,
                                     box_bounds,
                                     clip_region,
                                     color,
                                     PrimitiveFlags::None);
            return;
        }

        if blur_radius == 0.0 && spread_radius == 0.0 && border_radius != 0.0 {
            self.fill_box_shadow_rect(clip_and_scroll,
                                      box_bounds,
                                      bs_rect,
                                      clip_region,
                                      color,
                                      border_radius,
                                      clip_mode);
            return;
        }

        // Get the outer rectangle, based on the blur radius.
        let outside_edge_size = 2.0 * blur_radius;
        let inside_edge_size = outside_edge_size.max(border_radius);
        let edge_size = outside_edge_size + inside_edge_size;
        let outer_rect = bs_rect.inflate(outside_edge_size, outside_edge_size);

        // Box shadows are often used for things like text underline and other
        // simple primitives, so we want to draw these simple cases with the
        // solid rectangle shader wherever possible, to avoid invoking the
        // expensive box-shadow shader.
        enum BoxShadowKind {
            Simple(Vec<LayerRect>),     // Can be drawn via simple rectangles only
            Shadow(Vec<LayerRect>),     // Requires the full box-shadow code path
        }

        let shadow_kind = match clip_mode {
            BoxShadowClipMode::Outset | BoxShadowClipMode::None => {
                // If a border radius is set, we need to draw inside
                // the original box in order to draw where the border
                // corners are. A clip-out mask applied below will
                // ensure that we don't draw on the box itself.
                let inner_box_bounds = box_bounds.inflate(-border_radius,
                                                          -border_radius);
                // For outset shadows, subtracting the element rectangle
                // from the outer rectangle gives the rectangles we need
                // to draw. In the simple case (no blur radius), we can
                // just draw these as solid colors.
                let mut rects = Vec::new();
                subtract_rect(&outer_rect, &inner_box_bounds, &mut rects);
                if edge_size == 0.0 {
                    BoxShadowKind::Simple(rects)
                } else {
                    BoxShadowKind::Shadow(rects)
                }
            }
            BoxShadowClipMode::Inset => {
                // For inset shadows, in the simple case (no blur) we
                // can draw the shadow area by subtracting the box
                // shadow rect from the element rect (since inset box
                // shadows never extend past the element rect). However,
                // in the case of an inset box shadow with blur, we
                // currently just draw the box shadow over the entire
                // rect. The opaque parts of the shadow (past the outside
                // edge of the box-shadow) are handled by the shadow
                // shader.
                // TODO(gw): We should be able to optimize the complex
                //           inset shadow case to touch fewer pixels. We
                //           can probably calculate the inner rect that
                //           can't be affected, and subtract that from
                //           the element rect?
                let mut rects = Vec::new();
                if edge_size == 0.0 {
                    subtract_rect(box_bounds, &bs_rect, &mut rects);
                    BoxShadowKind::Simple(rects)
                } else {
                    rects.push(*box_bounds);
                    BoxShadowKind::Shadow(rects)
                }
            }
        };

        match shadow_kind {
            BoxShadowKind::Simple(rects) => {
                for rect in &rects {
                    self.add_solid_rectangle(clip_and_scroll,
                                             rect,
                                             clip_region,
                                             color,
                                             PrimitiveFlags::None)
                }
            }
            BoxShadowKind::Shadow(rects) => {
                if clip_mode == BoxShadowClipMode::Inset {
                    self.fill_box_shadow_rect(clip_and_scroll,
                                              box_bounds,
                                              bs_rect,
                                              clip_region,
                                              color,
                                              border_radius,
                                              clip_mode);
                }

                let inverted = match clip_mode {
                    BoxShadowClipMode::Outset | BoxShadowClipMode::None => 0.0,
                    BoxShadowClipMode::Inset => 1.0,
                };

                // Outset box shadows with border radius
                // need a clip out of the center box.
                let extra_clip_mode = match clip_mode {
                    BoxShadowClipMode::Outset | BoxShadowClipMode::None => ClipMode::ClipOut,
                    BoxShadowClipMode::Inset => ClipMode::Clip,
                };

                let mut extra_clips = Vec::new();
                if border_radius >= 0.0 {
                    extra_clips.push(ClipSource::Complex(*box_bounds,
                                                border_radius,
                                                extra_clip_mode));
                }

                let prim_gpu = BoxShadowPrimitiveGpu {
                    src_rect: *box_bounds,
                    bs_rect: bs_rect,
                    color: *color,
                    blur_radius: blur_radius,
                    border_radius: border_radius,
                    edge_size: edge_size,
                    inverted: inverted,
                };

                self.add_primitive(clip_and_scroll,
                                   &outer_rect,
                                   clip_region,
                                   extra_clips.as_slice(),
                                   PrimitiveContainer::BoxShadow(prim_gpu, rects));
            }
        }
    }

    pub fn add_webgl_rectangle(&mut self,
                               clip_and_scroll: ClipAndScrollInfo,
                               rect: LayerRect,
                               clip_region: &ClipRegion,
                               context_id: WebGLContextId) {
        let prim_cpu = ImagePrimitiveCpu {
            kind: ImagePrimitiveKind::WebGL(context_id),
            color_texture_id: SourceTexture::Invalid,
            resource_address: GpuStoreAddress(0),
            sub_rect: None,
        };

        let prim_gpu = ImagePrimitiveGpu {
            stretch_size: rect.size,
            tile_spacing: LayerSize::zero(),
        };

        self.add_primitive(clip_and_scroll,
                           &rect,
                           clip_region,
                           &[],
                           PrimitiveContainer::Image(prim_cpu, prim_gpu));
    }

    pub fn add_image(&mut self,
                     clip_and_scroll: ClipAndScrollInfo,
                     rect: LayerRect,
                     clip_region: &ClipRegion,
                     stretch_size: &LayerSize,
                     tile_spacing: &LayerSize,
                     sub_rect: Option<TexelRect>,
                     image_key: ImageKey,
                     image_rendering: ImageRendering,
                     tile: Option<TileOffset>) {
        let prim_cpu = ImagePrimitiveCpu {
            kind: ImagePrimitiveKind::Image(image_key,
                                            image_rendering,
                                            tile,
                                            *tile_spacing),
            color_texture_id: SourceTexture::Invalid,
            resource_address: GpuStoreAddress(0),
            sub_rect: sub_rect,
        };

        let prim_gpu = ImagePrimitiveGpu {
            stretch_size: *stretch_size,
            tile_spacing: *tile_spacing,
        };

        self.add_primitive(clip_and_scroll,
                           &rect,
                           clip_region,
                           &[],
                           PrimitiveContainer::Image(prim_cpu, prim_gpu));
    }

    pub fn add_yuv_image(&mut self,
                         clip_and_scroll: ClipAndScrollInfo,
                         rect: LayerRect,
                         clip_region: &ClipRegion,
                         yuv_data: YuvData,
                         color_space: YuvColorSpace) {
        let format = yuv_data.get_format();
        let yuv_key = match yuv_data {
            YuvData::NV12(plane_0, plane_1) => [plane_0, plane_1, ImageKey::new(0, 0)],
            YuvData::PlanarYCbCr(plane_0, plane_1, plane_2) =>
                [plane_0, plane_1, plane_2],
        };

        let prim_cpu = YuvImagePrimitiveCpu {
            yuv_key: yuv_key,
            yuv_texture_id: [SourceTexture::Invalid, SourceTexture::Invalid, SourceTexture::Invalid],
            yuv_resource_address: GpuStoreAddress(0),
            format: format,
            color_space: color_space,
        };

        let prim_gpu = YuvImagePrimitiveGpu::new(rect.size);

        self.add_primitive(clip_and_scroll,
                           &rect,
                           clip_region,
                           &[],
                           PrimitiveContainer::YuvImage(prim_cpu, prim_gpu));
    }

    /// Compute the contribution (bounding rectangles, and resources) of layers and their
    /// primitives in screen space.
    fn build_layer_screen_rects_and_cull_layers(&mut self,
                                                screen_rect: &DeviceIntRect,
                                                clip_scroll_tree: &mut ClipScrollTree,
                                                auxiliary_lists_map: &AuxiliaryListsMap,
                                                resource_cache: &mut ResourceCache,
                                                profile_counters: &mut FrameProfileCounters,
                                                device_pixel_ratio: f32) {
        profile_scope!("cull");
        LayerRectCalculationAndCullingPass::create_and_run(self,
                                                           screen_rect,
                                                           clip_scroll_tree,
                                                           auxiliary_lists_map,
                                                           resource_cache,
                                                           profile_counters,
                                                           device_pixel_ratio);
    }

    fn update_scroll_bars(&mut self, clip_scroll_tree: &ClipScrollTree) {
        let distance_from_edge = 8.0;

        for scrollbar_prim in &self.scrollbar_prims {
            let mut geom = (*self.prim_store.gpu_geometry.get(GpuStoreAddress(scrollbar_prim.prim_index.0 as i32))).clone();
            let clip_scroll_node = &clip_scroll_tree.nodes[&scrollbar_prim.clip_id];

            let scrollable_distance = clip_scroll_node.scrollable_height();

            if scrollable_distance <= 0.0 {
                geom.local_clip_rect.size = LayerSize::zero();
                *self.prim_store.gpu_geometry.get_mut(GpuStoreAddress(scrollbar_prim.prim_index.0 as i32)) = geom;
                continue;
            }

            let f = -clip_scroll_node.scrolling.offset.y / scrollable_distance;

            let min_y = clip_scroll_node.local_viewport_rect.origin.y -
                        clip_scroll_node.scrolling.offset.y +
                        distance_from_edge;

            let max_y = clip_scroll_node.local_viewport_rect.origin.y +
                        clip_scroll_node.local_viewport_rect.size.height -
                        clip_scroll_node.scrolling.offset.y -
                        geom.local_rect.size.height -
                        distance_from_edge;

            geom.local_rect.origin.x = clip_scroll_node.local_viewport_rect.origin.x +
                                       clip_scroll_node.local_viewport_rect.size.width -
                                       geom.local_rect.size.width -
                                       distance_from_edge;

            geom.local_rect.origin.y = util::lerp(min_y, max_y, f);
            geom.local_clip_rect = geom.local_rect;

            let clip_source = if scrollbar_prim.border_radius > 0.0 {
                Some(ClipSource::Complex(geom.local_rect, scrollbar_prim.border_radius, ClipMode::Clip))
            } else {
                None
            };
            self.prim_store.set_clip_source(scrollbar_prim.prim_index, clip_source);
            *self.prim_store.gpu_geometry.get_mut(GpuStoreAddress(scrollbar_prim.prim_index.0 as i32)) = geom;
        }
    }

    fn build_render_task(&self) -> (RenderTask, usize) {
        profile_scope!("build_render_task");

        let mut next_z = 0;
        let mut next_task_index = RenderTaskIndex(0);

        let mut sc_stack = Vec::new();
        let mut current_task = RenderTask::new_alpha_batch(next_task_index,
                                                           DeviceIntPoint::zero(),
                                                           RenderTaskLocation::Fixed);
        next_task_index.0 += 1;
        let mut alpha_task_stack = Vec::new();

        for cmd in &self.cmds {
            match *cmd {
                PrimitiveRunCmd::PushStackingContext(stacking_context_index) => {
                    let stacking_context = &self.stacking_context_store[stacking_context_index.0];
                    sc_stack.push(stacking_context_index);

                    if !stacking_context.is_visible {
                        continue;
                    }

                    let stacking_context_rect = &stacking_context.bounding_rect;
                    let composite_count = stacking_context.composite_ops.count();

                    if composite_count == 0 && stacking_context.should_isolate {
                        let location = RenderTaskLocation::Dynamic(None, stacking_context_rect.size);
                        let new_task = RenderTask::new_alpha_batch(next_task_index,
                                                                   stacking_context_rect.origin,
                                                                   location);
                        next_task_index.0 += 1;
                        let prev_task = mem::replace(&mut current_task, new_task);
                        alpha_task_stack.push(prev_task);
                    }

                    for _ in 0..composite_count {
                        let location = RenderTaskLocation::Dynamic(None, stacking_context_rect.size);
                        let new_task = RenderTask::new_alpha_batch(next_task_index,
                                                                   stacking_context_rect.origin,
                                                                   location);
                        next_task_index.0 += 1;
                        let prev_task = mem::replace(&mut current_task, new_task);
                        alpha_task_stack.push(prev_task);
                    }
                }
                PrimitiveRunCmd::PopStackingContext => {
                    let stacking_context_index = sc_stack.pop().unwrap();
                    let stacking_context = &self.stacking_context_store[stacking_context_index.0];

                    if !stacking_context.is_visible {
                        continue;
                    }

                    let composite_count = stacking_context.composite_ops.count();

                    if composite_count == 0 && stacking_context.should_isolate {
                        let mut prev_task = alpha_task_stack.pop().unwrap();
                        let item = AlphaRenderItem::HardwareComposite(stacking_context_index,
                                                                      current_task.id,
                                                                      HardwareCompositeOp::PremultipliedAlpha,
                                                                      next_z);
                        next_z += 1;
                        prev_task.as_alpha_batch().items.push(item);
                        prev_task.children.push(current_task);
                        current_task = prev_task;
                    }

                    for filter in &stacking_context.composite_ops.filters {
                        let mut prev_task = alpha_task_stack.pop().unwrap();
                        let item = AlphaRenderItem::Blend(stacking_context_index,
                                                          current_task.id,
                                                          *filter,
                                                          next_z);
                        next_z += 1;
                        prev_task.as_alpha_batch().items.push(item);
                        prev_task.children.push(current_task);
                        current_task = prev_task;
                    }
                    if let Some(mix_blend_mode) = stacking_context.composite_ops.mix_blend_mode {
                        let readback_task =
                            RenderTask::new_readback(stacking_context_index,
                                                     stacking_context.bounding_rect);

                        let mut prev_task = alpha_task_stack.pop().unwrap();
                        let item = AlphaRenderItem::Composite(stacking_context_index,
                                                              readback_task.id,
                                                              current_task.id,
                                                              mix_blend_mode,
                                                              next_z);
                        next_z += 1;
                        prev_task.as_alpha_batch().items.push(item);
                        prev_task.children.push(current_task);
                        prev_task.children.push(readback_task);
                        current_task = prev_task;
                    }
                }
                PrimitiveRunCmd::PrimitiveRun(first_prim_index, prim_count, clip_and_scroll) => {
                    let stacking_context_index = *sc_stack.last().unwrap();
                    let stacking_context = &self.stacking_context_store[stacking_context_index.0];

                    if !stacking_context.is_visible {
                        continue;
                    }

                    let stacking_context_index = *sc_stack.last().unwrap();
                    let group_index = self.stacking_context_store[stacking_context_index.0]
                                          .clip_scroll_group(clip_and_scroll);
                    if self.clip_scroll_group_store[group_index.0].screen_bounding_rect.is_none() {
                        continue
                    }

                    for i in 0..prim_count {
                        let prim_index = PrimitiveIndex(first_prim_index.0 + i);

                        if self.prim_store.cpu_bounding_rects[prim_index.0].is_some() {
                            let prim_metadata = self.prim_store.get_metadata(prim_index);

                            // Add any dynamic render tasks needed to render this primitive
                            if let Some(ref render_task) = prim_metadata.render_task {
                                current_task.children.push(render_task.clone());
                            }
                            if let Some(ref clip_task) = prim_metadata.clip_task {
                                current_task.children.push(clip_task.clone());
                            }

                            let item = AlphaRenderItem::Primitive(group_index, prim_index, next_z);
                            current_task.as_alpha_batch().items.push(item);
                            next_z += 1;
                        }
                    }
                }
            }
        }

        debug_assert!(alpha_task_stack.is_empty());
        (current_task, next_task_index.0)
    }

    pub fn build(&mut self,
                 resource_cache: &mut ResourceCache,
                 frame_id: FrameId,
                 clip_scroll_tree: &mut ClipScrollTree,
                 auxiliary_lists_map: &AuxiliaryListsMap,
                 device_pixel_ratio: f32,
                 texture_cache_profile: &mut TextureCacheProfileCounters)
                 -> Frame {
        profile_scope!("build");

        let mut profile_counters = FrameProfileCounters::new();
        profile_counters.total_primitives.set(self.prim_store.prim_count());

        resource_cache.begin_frame(frame_id);

        let screen_rect = DeviceIntRect::new(
            DeviceIntPoint::zero(),
            DeviceIntSize::new(self.screen_size.width as i32,
                               self.screen_size.height as i32));

        // Pick a size for the cache render targets to be. The main requirement is that it
        // has to be at least as large as the framebuffer size. This ensures that it will
        // always be able to allocate the worst case render task (such as a clip mask that
        // covers the entire screen).
        let cache_size = DeviceUintSize::new(cmp::max(1024, screen_rect.size.width as u32),
                                             cmp::max(1024, screen_rect.size.height as u32));

        self.update_scroll_bars(clip_scroll_tree);

        self.build_layer_screen_rects_and_cull_layers(&screen_rect,
                                                      clip_scroll_tree,
                                                      auxiliary_lists_map,
                                                      resource_cache,
                                                      &mut profile_counters,
                                                      device_pixel_ratio);

        let (main_render_task, static_render_task_count) = self.build_render_task();
        let mut render_tasks = RenderTaskCollection::new(static_render_task_count);

        let mut required_pass_count = 0;
        main_render_task.max_depth(0, &mut required_pass_count);

        resource_cache.block_until_all_resources_added(texture_cache_profile);

        for node in clip_scroll_tree.nodes.values() {
            if let NodeType::Clip(ref clip_info) = node.node_type {
                if let Some(ref mask_info) = clip_info.mask_cache_info {
                    self.prim_store.resolve_clip_cache(mask_info, resource_cache);
                }
            }
        }

        let deferred_resolves = self.prim_store.resolve_primitives(resource_cache,
                                                                   device_pixel_ratio);

        let mut passes = Vec::new();

        // Do the allocations now, assigning each tile's tasks to a render
        // pass and target as required.
        for index in 0..required_pass_count {
            passes.push(RenderPass::new(index as isize,
                                        index == required_pass_count-1,
                                        cache_size));
        }

        main_render_task.assign_to_passes(passes.len() - 1, &mut passes);

        for pass in &mut passes {
            let ctx = RenderTargetContext {
                stacking_context_store: &self.stacking_context_store,
                clip_scroll_group_store: &self.clip_scroll_group_store,
                prim_store: &self.prim_store,
                resource_cache: resource_cache,
            };

            pass.build(&ctx, &mut render_tasks);

            profile_counters.passes.inc();
            profile_counters.color_targets.add(pass.color_targets.target_count());
            profile_counters.alpha_targets.add(pass.alpha_targets.target_count());
        }

        resource_cache.end_frame();

        Frame {
            device_pixel_ratio: device_pixel_ratio,
            background_color: self.background_color,
            window_size: self.screen_size,
            profile_counters: profile_counters,
            passes: passes,
            cache_size: cache_size,
            layer_texture_data: self.packed_layers.clone(),
            render_task_data: render_tasks.render_task_data,
            gpu_data16: self.prim_store.gpu_data16.build(),
            gpu_data32: self.prim_store.gpu_data32.build(),
            gpu_data64: self.prim_store.gpu_data64.build(),
            gpu_data128: self.prim_store.gpu_data128.build(),
            gpu_geometry: self.prim_store.gpu_geometry.build(),
            gpu_gradient_data: self.prim_store.gpu_gradient_data.build(),
            gpu_resource_rects: self.prim_store.gpu_resource_rects.build(),
            deferred_resolves: deferred_resolves,
        }
    }

}

struct LayerRectCalculationAndCullingPass<'a> {
    frame_builder: &'a mut FrameBuilder,
    screen_rect: &'a DeviceIntRect,
    clip_scroll_tree: &'a mut ClipScrollTree,
    auxiliary_lists_map: &'a AuxiliaryListsMap,
    resource_cache: &'a mut ResourceCache,
    profile_counters: &'a mut FrameProfileCounters,
    device_pixel_ratio: f32,
    stacking_context_stack: Vec<StackingContextIndex>,

    /// A cached clip info stack, which should handle the most common situation,
    /// which is that we are using the same clip info stack that we were using
    /// previously.
    current_clip_stack: Vec<(PackedLayerIndex, MaskCacheInfo)>,

    /// Information about the cached clip stack, which is used to avoid having
    /// to recalculate it for every primitive.
    current_clip_info: Option<(ClipId, Option<DeviceIntRect>)>
}

impl<'a> LayerRectCalculationAndCullingPass<'a> {
    fn create_and_run(frame_builder: &'a mut FrameBuilder,
                      screen_rect: &'a DeviceIntRect,
                      clip_scroll_tree: &'a mut ClipScrollTree,
                      auxiliary_lists_map: &'a AuxiliaryListsMap,
                      resource_cache: &'a mut ResourceCache,
                      profile_counters: &'a mut FrameProfileCounters,
                      device_pixel_ratio: f32) {

        let mut pass = LayerRectCalculationAndCullingPass {
            frame_builder: frame_builder,
            screen_rect: screen_rect,
            clip_scroll_tree: clip_scroll_tree,
            auxiliary_lists_map: auxiliary_lists_map,
            resource_cache: resource_cache,
            profile_counters: profile_counters,
            device_pixel_ratio: device_pixel_ratio,
            stacking_context_stack: Vec::new(),
            current_clip_stack: Vec::new(),
            current_clip_info: None,
        };
        pass.run();
    }

    fn run(&mut self) {
        self.recalculate_clip_scroll_groups();
        self.recalculate_clip_scroll_nodes();
        self.compute_stacking_context_visibility();

        let commands = mem::replace(&mut self.frame_builder.cmds, Vec::new());
        for cmd in &commands {
            match *cmd {
                PrimitiveRunCmd::PushStackingContext(stacking_context_index) =>
                    self.handle_push_stacking_context(stacking_context_index),
                PrimitiveRunCmd::PrimitiveRun(prim_index, prim_count, clip_and_scroll) =>
                    self.handle_primitive_run(prim_index, prim_count, clip_and_scroll),
                PrimitiveRunCmd::PopStackingContext => self.handle_pop_stacking_context(),
            }
        }

        mem::replace(&mut self.frame_builder.cmds, commands);
    }

    fn recalculate_clip_scroll_nodes(&mut self) {
        for (_, ref mut node) in self.clip_scroll_tree.nodes.iter_mut() {
            let node_clip_info = match node.node_type {
                NodeType::Clip(ref mut clip_info) => clip_info,
                NodeType::ReferenceFrame(_) => continue,
            };

            let packed_layer_index = node_clip_info.packed_layer_index;
            let packed_layer = &mut self.frame_builder.packed_layers[packed_layer_index.0];

            // The coordinates of the mask are relative to the origin of the node itself,
            // so we need to account for that origin in the transformation we assign to
            // the packed layer.
            let transform = node.world_viewport_transform
                                .pre_translated(node.local_viewport_rect.origin.x,
                                                node.local_viewport_rect.origin.y,
                                                0.0);
            packed_layer.set_transform(transform);

            // Meanwhile, the combined viewport rect is relative to the reference frame, so
            // we move it into the local coordinate system of the node.
            let local_viewport_rect =
                node.combined_local_viewport_rect.translate(&-node.local_viewport_rect.origin);

            node_clip_info.screen_bounding_rect = packed_layer.set_rect(&local_viewport_rect,
                                                                        self.screen_rect,
                                                                        self.device_pixel_ratio);

            let mask_info = match node_clip_info.mask_cache_info {
                Some(ref mut mask_info) => mask_info,
                _ => continue,
            };

            let auxiliary_lists = self.auxiliary_lists_map.get(&node.pipeline_id)
                                                          .expect("No auxiliary lists?");

            mask_info.update(&node_clip_info.clip_sources,
                             &packed_layer.transform,
                             &mut self.frame_builder.prim_store.gpu_data32,
                             self.device_pixel_ratio,
                             auxiliary_lists);

            for clip_source in &node_clip_info.clip_sources {
                if let Some(mask) = clip_source.image_mask() {
                    // We don't add the image mask for resolution, because
                    // layer masks are resolved later.
                    self.resource_cache.request_image(mask.image, ImageRendering::Auto, None);
                }
            }
        }
    }

    fn recalculate_clip_scroll_groups(&mut self) {
        for ref mut group in &mut self.frame_builder.clip_scroll_group_store {
            let stacking_context_index = group.stacking_context_index;
            let stacking_context = &mut self.frame_builder
                                            .stacking_context_store[stacking_context_index.0];

            let scroll_node = &self.clip_scroll_tree.nodes[&group.scroll_node_id];
            let clip_node = &self.clip_scroll_tree.nodes[&group.clip_node_id];
            let packed_layer = &mut self.frame_builder.packed_layers[group.packed_layer_index.0];

            // The world content transform is relative to the containing reference frame,
            // so we translate into the origin of the stacking context itself.
            let transform = scroll_node.world_content_transform
                                       .pre_translated(stacking_context.reference_frame_offset.x,
                                                       stacking_context.reference_frame_offset.y,
                                                       0.0);
            packed_layer.set_transform(transform);

            if !stacking_context.can_contribute_to_scene() {
                return;
            }

            // Here we move the viewport rectangle into the coordinate system
            // of the stacking context content.
            let viewport_rect =
                &clip_node.combined_local_viewport_rect
                     .translate(&clip_node.reference_frame_relative_scroll_offset)
                     .translate(&-scroll_node.reference_frame_relative_scroll_offset)
                     .translate(&-stacking_context.reference_frame_offset)
                     .translate(&-scroll_node.scrolling.offset);
            group.screen_bounding_rect = packed_layer.set_rect(viewport_rect,
                                                               self.screen_rect,
                                                               self.device_pixel_ratio);
        }
    }

    fn compute_stacking_context_visibility(&mut self) {
        for context_index in 0..self.frame_builder.stacking_context_store.len() {
            let is_visible = {
                // We don't take into account visibility of children here, so we must
                // do that later.
                let stacking_context = &self.frame_builder.stacking_context_store[context_index];
                stacking_context.clip_scroll_groups.iter().any(|group_index| {
                    self.frame_builder.clip_scroll_group_store[group_index.0].is_visible()
                })
            };
            self.frame_builder.stacking_context_store[context_index].is_visible = is_visible;
        }
    }

    fn handle_pop_stacking_context(&mut self) {
        let stacking_context_index = self.stacking_context_stack.pop().unwrap();

        let (bounding_rect, is_visible) = {
            let stacking_context =
                &mut self.frame_builder.stacking_context_store[stacking_context_index.0];
            stacking_context.bounding_rect = stacking_context.bounding_rect
                                                             .intersection(self.screen_rect)
                                                             .unwrap_or(DeviceIntRect::zero());
            (stacking_context.bounding_rect.clone(), stacking_context.is_visible)
        };

        if let Some(ref mut parent_index) = self.stacking_context_stack.last_mut() {
            let parent = &mut self.frame_builder.stacking_context_store[parent_index.0];
            parent.bounding_rect = parent.bounding_rect.union(&bounding_rect);

            // The previous compute_stacking_context_visibility pass did not take into
            // account visibility of children, so we do that now.
            parent.is_visible = parent.is_visible || is_visible;
        }
    }

    fn handle_push_stacking_context(&mut self, stacking_context_index: StackingContextIndex) {
        self.stacking_context_stack.push(stacking_context_index);

        // Reset bounding rect to zero. We will calculate it as we collect primitives
        // from various scroll layers. In handle_pop_stacking_context , we use this to
        // calculate the device bounding rect. In the future, we could cache this during
        // the initial adding of items for the common case (where there is only a single
        // scroll layer for items in a stacking context).
        let stacking_context = &mut self.frame_builder
                                        .stacking_context_store[stacking_context_index.0];
        stacking_context.bounding_rect = DeviceIntRect::zero();
    }

    fn rebuild_clip_info_stack_if_necessary(&mut self, id: ClipId) -> Option<DeviceIntRect> {
        if let Some((current_scroll_id, bounding_rect)) = self.current_clip_info {
            if current_scroll_id == id {
                return bounding_rect;
            }
        }

        // TODO(mrobinson): If we notice that this process is expensive, we can special-case
        // more common situations, such as moving from a child or a parent.
        self.current_clip_stack.clear();
        let mut bounding_rect = None;

        let mut current_id = Some(id);
        while let Some(id) = current_id {
            let node = &self.clip_scroll_tree.nodes.get(&id).unwrap();
            current_id = node.parent;

            let clip_info = match node.node_type {
                NodeType::Clip(ref clip) if clip.is_masking() => clip,
                _ => continue,
            };

            if bounding_rect.is_none() {
                bounding_rect = Some(match clip_info.screen_bounding_rect {
                    Some((_kind, rect)) => rect,
                    None => DeviceIntRect::zero(),
                });
            }
            self.current_clip_stack.push((clip_info.packed_layer_index,
                                          clip_info.mask_cache_info.clone().unwrap()))
        }
        self.current_clip_stack.reverse();

        self.current_clip_info = Some((id, bounding_rect));
        bounding_rect
    }

    fn handle_primitive_run(&mut self,
                            prim_index: PrimitiveIndex,
                            prim_count: usize,
                            clip_and_scroll: ClipAndScrollInfo) {
        let stacking_context_index = *self.stacking_context_stack.last().unwrap();
        let (packed_layer_index, pipeline_id) = {
            let stacking_context =
                &self.frame_builder.stacking_context_store[stacking_context_index.0];

            if !stacking_context.is_visible {
                return;
            }

            let group_index = stacking_context.clip_scroll_group(clip_and_scroll);
            let clip_scroll_group = &self.frame_builder.clip_scroll_group_store[group_index.0];
            (clip_scroll_group.packed_layer_index, stacking_context.pipeline_id)
        };

        let clip_bounds =
            self.rebuild_clip_info_stack_if_necessary(clip_and_scroll.clip_node_id());
        if clip_bounds.map_or(false, |bounds| bounds.is_empty()) {
            return;
        }

        let stacking_context =
            &mut self.frame_builder.stacking_context_store[stacking_context_index.0];
        let packed_layer = &self.frame_builder.packed_layers[packed_layer_index.0];
        let auxiliary_lists = self.auxiliary_lists_map.get(&pipeline_id)
                                                      .expect("No auxiliary lists?");
        for i in 0..prim_count {
            let prim_index = PrimitiveIndex(prim_index.0 + i);
            if self.frame_builder.prim_store.build_bounding_rect(prim_index,
                                                                 self.screen_rect,
                                                                 &packed_layer.transform,
                                                                 &packed_layer.local_clip_rect,
                                                                 self.device_pixel_ratio) {
                if self.frame_builder.prim_store.prepare_prim_for_render(prim_index,
                                                                         self.resource_cache,
                                                                         &packed_layer.transform,
                                                                         self.device_pixel_ratio,
                                                                         auxiliary_lists) {
                    self.frame_builder.prim_store.build_bounding_rect(prim_index,
                                                                      self.screen_rect,
                                                                      &packed_layer.transform,
                                                                      &packed_layer.local_clip_rect,
                                                                      self.device_pixel_ratio);
                }

                // If the primitive is visible, consider culling it via clip rect(s).
                // If it is visible but has clips, create the clip task for it.
                let prim_bounding_rect =
                    match self.frame_builder.prim_store.cpu_bounding_rects[prim_index.0] {
                    Some(rect) => rect,
                    _ => continue,
                };

                let prim_metadata = &mut self.frame_builder.prim_store.cpu_metadata[prim_index.0];
                let prim_clip_info = prim_metadata.clip_cache_info.as_ref();
                let mut visible = true;

                stacking_context.bounding_rect =
                    stacking_context.bounding_rect.union(&prim_bounding_rect);

                if let Some(info) = prim_clip_info {
                    self.current_clip_stack.push((packed_layer_index, info.clone()));
                }

                // Try to create a mask if we may need to.
                if !self.current_clip_stack.is_empty() {
                    // If the primitive doesn't have a specific clip, key the task ID off the
                    // stacking context. This means that two primitives which are only clipped
                    // by the stacking context stack can share clip masks during render task
                    // assignment to targets.
                    let clip_bounds = clip_bounds.unwrap_or_else(DeviceIntRect::zero);
                    let (mask_key, mask_rect) = match prim_clip_info {
                        Some(..) => (MaskCacheKey::Primitive(prim_index), prim_bounding_rect),
                        None => (MaskCacheKey::ClipNode(clip_and_scroll.clip_node_id()),
                                                        clip_bounds)
                    };
                    let mask_opt =
                        RenderTask::new_mask(mask_rect, mask_key, &self.current_clip_stack);
                    match mask_opt {
                        MaskResult::Outside => { // Primitive is completely clipped out.
                            prim_metadata.clip_task = None;
                            self.frame_builder.prim_store.cpu_bounding_rects[prim_index.0] = None;
                            visible = false;
                        }
                        MaskResult::Inside(task) => prim_metadata.clip_task = Some(task),
                    }
                }

                if prim_clip_info.is_some() {
                    self.current_clip_stack.pop();
                }

                if visible {
                    self.profile_counters.visible_primitives.inc();
                }
            }
        }
    }
}
