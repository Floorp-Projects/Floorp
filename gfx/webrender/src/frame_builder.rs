/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use app_units::Au;
use batch_builder::BorderSideHelpers;
use frame::FrameId;
use gpu_store::GpuStoreAddress;
use internal_types::{HardwareCompositeOp, SourceTexture};
use mask_cache::{ClipSource, MaskCacheInfo};
use prim_store::{BorderPrimitiveCpu, BorderPrimitiveGpu, BoxShadowPrimitiveGpu};
use prim_store::{GradientPrimitiveCpu, GradientPrimitiveGpu, ImagePrimitiveCpu, ImagePrimitiveGpu};
use prim_store::{ImagePrimitiveKind, PrimitiveContainer, PrimitiveGeometry, PrimitiveIndex};
use prim_store::{PrimitiveStore, RadialGradientPrimitiveCpu, RadialGradientPrimitiveGpu};
use prim_store::{RectanglePrimitive, TextRunPrimitiveCpu, TextRunPrimitiveGpu};
use prim_store::{TexelRect, YuvImagePrimitiveCpu, YuvImagePrimitiveGpu};
use profiler::{FrameProfileCounters, TextureCacheProfileCounters};
use render_task::{AlphaRenderItem, MaskCacheKey, MaskResult, RenderTask, RenderTaskIndex};
use render_task::RenderTaskLocation;
use resource_cache::ResourceCache;
use clip_scroll_tree::ClipScrollTree;
use std::{cmp, f32, i32, mem, usize};
use tiling::{AuxiliaryListsMap, ClipScrollGroup, ClipScrollGroupIndex, CompositeOps, Frame};
use tiling::{PackedLayer, PackedLayerIndex, PrimitiveFlags, PrimitiveRunCmd, RenderPass};
use tiling::{RenderTargetContext, RenderTaskCollection, ScrollbarPrimitive, ScrollLayer};
use tiling::{ScrollLayerIndex, StackingContext, StackingContextIndex};
use util::{self, pack_as_float, rect_from_points_f, subtract_rect, TransformedRect};
use util::{RectHelpers, TransformedRectKind};
use webrender_traits::{as_scroll_parent_rect, BorderDetails, BorderDisplayItem, BorderSide, BorderStyle};
use webrender_traits::{BoxShadowClipMode, ClipRegion, ColorF, device_length, DeviceIntPoint};
use webrender_traits::{DeviceIntRect, DeviceIntSize, DeviceUintSize, ExtendMode, FontKey, TileOffset};
use webrender_traits::{FontRenderMode, GlyphOptions, ImageKey, ImageRendering, ItemRange};
use webrender_traits::{LayerPoint, LayerRect, LayerSize, LayerToScrollTransform, PipelineId};
use webrender_traits::{RepeatMode, ScrollLayerId, ScrollLayerPixel, WebGLContextId, YuvColorSpace};

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
    screen_rect: LayerRect,
    background_color: Option<ColorF>,
    prim_store: PrimitiveStore,
    cmds: Vec<PrimitiveRunCmd>,
    config: FrameBuilderConfig,

    stacking_context_store: Vec<StackingContext>,
    scroll_layer_store: Vec<ScrollLayer>,
    clip_scroll_group_store: Vec<ClipScrollGroup>,
    packed_layers: Vec<PackedLayer>,

    scrollbar_prims: Vec<ScrollbarPrimitive>,

    /// A stack of scroll nodes used during display list processing to properly
    /// parent new scroll nodes.
    clip_scroll_node_stack: Vec<ScrollLayerIndex>,
}

impl FrameBuilder {
    pub fn new(viewport_size: LayerSize,
               background_color: Option<ColorF>,
               config: FrameBuilderConfig) -> FrameBuilder {
        FrameBuilder {
            screen_rect: LayerRect::new(LayerPoint::zero(), viewport_size),
            background_color: background_color,
            stacking_context_store: Vec::new(),
            scroll_layer_store: Vec::new(),
            clip_scroll_group_store: Vec::new(),
            prim_store: PrimitiveStore::new(),
            cmds: Vec::new(),
            packed_layers: Vec::new(),
            scrollbar_prims: Vec::new(),
            config: config,
            clip_scroll_node_stack: Vec::new(),
        }
    }

    fn add_primitive(&mut self,
                     rect: &LayerRect,
                     clip_region: &ClipRegion,
                     container: PrimitiveContainer) -> PrimitiveIndex {

        let geometry = PrimitiveGeometry {
            local_rect: *rect,
            local_clip_rect: clip_region.main,
        };
        let clip_source = if clip_region.is_complex() {
            ClipSource::Region(clip_region.clone())
        } else {
            ClipSource::NoClip
        };
        let clip_info = MaskCacheInfo::new(&clip_source,
                                           false,
                                           &mut self.prim_store.gpu_data32);

        let prim_index = self.prim_store.add_primitive(geometry,
                                                       Box::new(clip_source),
                                                       clip_info,
                                                       container);

        match self.cmds.last_mut().unwrap() {
            &mut PrimitiveRunCmd::PrimitiveRun(_run_prim_index, ref mut count) => {
                debug_assert!(_run_prim_index.0 + *count == prim_index.0);
                *count += 1;
                return prim_index;
            }
            &mut PrimitiveRunCmd::PushStackingContext(..) |
            &mut PrimitiveRunCmd::PopStackingContext |
            &mut PrimitiveRunCmd::PushScrollLayer(..) |
            &mut PrimitiveRunCmd::PopScrollLayer => {}
        }

        self.cmds.push(PrimitiveRunCmd::PrimitiveRun(prim_index, 1));

        prim_index
    }

    pub fn create_clip_scroll_group(&mut self,
                                    stacking_context_index: StackingContextIndex,
                                    scroll_layer_id: ScrollLayerId,
                                    pipeline_id: PipelineId)
                                    -> ClipScrollGroupIndex {
        let packed_layer_index = PackedLayerIndex(self.packed_layers.len());
        self.packed_layers.push(PackedLayer::empty());

        self.clip_scroll_group_store.push(ClipScrollGroup {
            stacking_context_index: stacking_context_index,
            scroll_layer_id: scroll_layer_id,
            packed_layer_index: packed_layer_index,
            pipeline_id: pipeline_id,
            xf_rect: None,
         });

        ClipScrollGroupIndex(self.clip_scroll_group_store.len() - 1)
    }

    pub fn push_stacking_context(&mut self,
                                 rect: LayerRect,
                                 transform: LayerToScrollTransform,
                                 pipeline_id: PipelineId,
                                 scroll_layer_id: ScrollLayerId,
                                 composite_ops: CompositeOps) {
        let stacking_context_index = StackingContextIndex(self.stacking_context_store.len());
        let group_index = self.create_clip_scroll_group(stacking_context_index,
                                                        scroll_layer_id,
                                                        pipeline_id);
        self.stacking_context_store.push(StackingContext::new(pipeline_id,
                                                              transform,
                                                              rect,
                                                              composite_ops,
                                                              group_index));
        self.cmds.push(PrimitiveRunCmd::PushStackingContext(stacking_context_index));
    }

    pub fn pop_stacking_context(&mut self) {
        self.cmds.push(PrimitiveRunCmd::PopStackingContext);
    }

    pub fn push_clip_scroll_node(&mut self,
                                 scroll_layer_id: ScrollLayerId,
                                 clip_region: &ClipRegion,
                                 node_origin: &LayerPoint,
                                 content_size: &LayerSize) {
        let scroll_layer_index = ScrollLayerIndex(self.scroll_layer_store.len());
        let parent_index = *self.clip_scroll_node_stack.last().unwrap_or(&scroll_layer_index);
        self.clip_scroll_node_stack.push(scroll_layer_index);

        let packed_layer_index = PackedLayerIndex(self.packed_layers.len());

        let clip_source = ClipSource::Region(clip_region.clone());
        let clip_info = MaskCacheInfo::new(&clip_source,
                                           true, // needs an extra clip for the clip rectangle
                                           &mut self.prim_store.gpu_data32);

        self.scroll_layer_store.push(ScrollLayer {
            scroll_layer_id: scroll_layer_id,
            parent_index: parent_index,
            clip_source: clip_source,
            clip_cache_info: clip_info,
            xf_rect: None,
            packed_layer_index: packed_layer_index,
        });
        self.packed_layers.push(PackedLayer::empty());
        self.cmds.push(PrimitiveRunCmd::PushScrollLayer(scroll_layer_index));


        // We need to push a fake stacking context here, because primitives that are
        // direct children of this stacking context, need to be adjusted by the scroll
        // offset of this layer. Eventually we should be able to remove this.
        let rect = LayerRect::new(LayerPoint::zero(),
                                  LayerSize::new(content_size.width + node_origin.x,
                                                 content_size.height + node_origin.y));
        self.push_stacking_context(rect,
                                   LayerToScrollTransform::identity(),
                                   scroll_layer_id.pipeline_id,
                                   scroll_layer_id,
                                   CompositeOps::empty());

    }

    pub fn pop_clip_scroll_node(&mut self) {
        self.pop_stacking_context();
        self.cmds.push(PrimitiveRunCmd::PopScrollLayer);
        self.clip_scroll_node_stack.pop();
    }

    pub fn add_solid_rectangle(&mut self,
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

        let prim_index = self.add_primitive(rect,
                                            clip_region,
                                            PrimitiveContainer::Rectangle(prim));

        match flags {
            PrimitiveFlags::None => {}
            PrimitiveFlags::Scrollbar(scroll_layer_id, border_radius) => {
                self.scrollbar_prims.push(ScrollbarPrimitive {
                    prim_index: prim_index,
                    scroll_layer_id: scroll_layer_id,
                    border_radius: border_radius,
                });
            }
        }
    }

    pub fn supported_style(&mut self, border: &BorderSide) -> bool {
        match border.style {
            BorderStyle::Solid |
            BorderStyle::None |
            BorderStyle::Dotted |
            BorderStyle::Dashed |
            BorderStyle::Inset |
            BorderStyle::Ridge |
            BorderStyle::Groove |
            BorderStyle::Outset |
            BorderStyle::Double => {
                return true;
            }
            _ => {
                println!("TODO: Other border styles {:?}", border.style);
                return false;
            }
        }
    }

    pub fn add_border(&mut self,
                      rect: LayerRect,
                      clip_region: &ClipRegion,
                      border_item: &BorderDisplayItem) {
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
                    self.add_image(segment.geom_rect,
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
                let radius = &border.radius;
                let left = &border.left;
                let right = &border.right;
                let top = &border.top;
                let bottom = &border.bottom;

                if !self.supported_style(left) || !self.supported_style(right) ||
                   !self.supported_style(top) || !self.supported_style(bottom) {
                    println!("Unsupported border style, not rendering border");
                    return;
                }

                // These colors are used during inset/outset scaling.
                let left_color      = left.border_color(1.0, 2.0/3.0, 0.3, 0.7);
                let top_color       = top.border_color(1.0, 2.0/3.0, 0.3, 0.7);
                let right_color     = right.border_color(2.0/3.0, 1.0, 0.7, 0.3);
                let bottom_color    = bottom.border_color(2.0/3.0, 1.0, 0.7, 0.3);

                let tl_outer = LayerPoint::new(rect.origin.x, rect.origin.y);
                let tl_inner = tl_outer + LayerPoint::new(radius.top_left.width.max(border_item.widths.left),
                                                          radius.top_left.height.max(border_item.widths.top));

                let tr_outer = LayerPoint::new(rect.origin.x + rect.size.width, rect.origin.y);
                let tr_inner = tr_outer + LayerPoint::new(-radius.top_right.width.max(border_item.widths.right),
                                                          radius.top_right.height.max(border_item.widths.top));

                let bl_outer = LayerPoint::new(rect.origin.x, rect.origin.y + rect.size.height);
                let bl_inner = bl_outer + LayerPoint::new(radius.bottom_left.width.max(border_item.widths.left),
                                                          -radius.bottom_left.height.max(border_item.widths.bottom));

                let br_outer = LayerPoint::new(rect.origin.x + rect.size.width,
                                               rect.origin.y + rect.size.height);
                let br_inner = br_outer - LayerPoint::new(radius.bottom_right.width.max(border_item.widths.right),
                                                          radius.bottom_right.height.max(border_item.widths.bottom));

                // The border shader is quite expensive. For simple borders, we can just draw
                // the border with a few rectangles. This generally gives better batching, and
                // a GPU win in fragment shader time.
                // More importantly, the software (OSMesa) implementation we run tests on is
                // particularly slow at running our complex border shader, compared to the
                // rectangle shader. This has the effect of making some of our tests time
                // out more often on CI (the actual cause is simply too many Servo processes and
                // threads being run on CI at once).
                // TODO(gw): Detect some more simple cases and handle those with simpler shaders too.
                // TODO(gw): Consider whether it's only worth doing this for large rectangles (since
                //           it takes a little more CPU time to handle multiple rectangles compared
                //           to a single border primitive).
                if left.style == BorderStyle::Solid {
                    let same_color = left_color == top_color &&
                                     left_color == right_color &&
                                     left_color == bottom_color;
                    let same_style = left.style == top.style &&
                                     left.style == right.style &&
                                     left.style == bottom.style;

                    if same_color && same_style && radius.is_zero() {
                        let rects = [
                            LayerRect::new(rect.origin,
                                           LayerSize::new(rect.size.width, border_item.widths.top)),
                            LayerRect::new(LayerPoint::new(tl_outer.x, tl_inner.y),
                                           LayerSize::new(border_item.widths.left,
                                                          rect.size.height - border_item.widths.top - border_item.widths.bottom)),
                            LayerRect::new(tr_inner,
                                           LayerSize::new(border_item.widths.right,
                                                          rect.size.height - border_item.widths.top - border_item.widths.bottom)),
                            LayerRect::new(LayerPoint::new(bl_outer.x, bl_inner.y),
                                           LayerSize::new(rect.size.width, border_item.widths.bottom))
                        ];

                        for rect in &rects {
                            self.add_solid_rectangle(rect,
                                                     clip_region,
                                                     &top_color,
                                                     PrimitiveFlags::None);
                        }

                        return;
                    }
                }

                //Note: while similar to `ComplexClipRegion::get_inner_rect()` in spirit,
                // this code is a bit more complex and can not there for be merged.
                let inner_rect = rect_from_points_f(tl_inner.x.max(bl_inner.x),
                                                    tl_inner.y.max(tr_inner.y),
                                                    tr_inner.x.min(br_inner.x),
                                                    bl_inner.y.min(br_inner.y));

                let prim_cpu = BorderPrimitiveCpu {
                    inner_rect: LayerRect::from_untyped(&inner_rect),
                };

                let prim_gpu = BorderPrimitiveGpu {
                    colors: [ left_color, top_color, right_color, bottom_color ],
                    widths: [ border_item.widths.left,
                              border_item.widths.top,
                              border_item.widths.right,
                              border_item.widths.bottom ],
                    style: [
                        pack_as_float(left.style as u32),
                        pack_as_float(top.style as u32),
                        pack_as_float(right.style as u32),
                        pack_as_float(bottom.style as u32),
                    ],
                    radii: [
                        radius.top_left,
                        radius.top_right,
                        radius.bottom_right,
                        radius.bottom_left,
                    ],
                };

                self.add_primitive(&rect,
                                   clip_region,
                                   PrimitiveContainer::Border(prim_cpu, prim_gpu));
            }
        }
    }

    pub fn add_gradient(&mut self,
                        rect: LayerRect,
                        clip_region: &ClipRegion,
                        start_point: LayerPoint,
                        end_point: LayerPoint,
                        stops: ItemRange,
                        extend_mode: ExtendMode) {
        // Fast path for clamped, axis-aligned gradients:
        let aligned = extend_mode == ExtendMode::Clamp &&
                      (start_point.x == end_point.x ||
                       start_point.y == end_point.y);
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
            padding: [0.0, 0.0, 0.0],
        };

        let prim = if aligned {
            PrimitiveContainer::AlignedGradient(gradient_cpu, gradient_gpu)
        } else {
            PrimitiveContainer::AngleGradient(gradient_cpu, gradient_gpu)
        };

        self.add_primitive(&rect, clip_region, prim);
    }

    pub fn add_radial_gradient(&mut self,
                               rect: LayerRect,
                               clip_region: &ClipRegion,
                               start_center: LayerPoint,
                               start_radius: f32,
                               end_center: LayerPoint,
                               end_radius: f32,
                               stops: ItemRange,
                               extend_mode: ExtendMode) {
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
            extend_mode: pack_as_float(extend_mode as u32),
            padding: [0.0],
        };

        self.add_primitive(&rect,
                           clip_region,
                           PrimitiveContainer::RadialGradient(radial_gradient_cpu, radial_gradient_gpu));
    }

    pub fn add_text(&mut self,
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

        let (render_mode, glyphs_per_run) = if blur_radius == Au(0) {
            // TODO(gw): Use a proper algorithm to select
            // whether this item should be rendered with
            // subpixel AA!
            let render_mode = if self.config.enable_subpixel_aa {
                FontRenderMode::Subpixel
            } else {
                FontRenderMode::Alpha
            };

            (render_mode, 8)
        } else {
            // TODO(gw): Support breaking up text shadow when
            // the size of the text run exceeds the dimensions
            // of the render target texture.
            (FontRenderMode::Alpha, glyph_range.length)
        };

        let text_run_count = (glyph_range.length + glyphs_per_run - 1) / glyphs_per_run;
        for run_index in 0..text_run_count {
            let start = run_index * glyphs_per_run;
            let end = cmp::min(start + glyphs_per_run, glyph_range.length);
            let sub_range = ItemRange {
                start: glyph_range.start + start,
                length: end - start,
            };

            let prim_cpu = TextRunPrimitiveCpu {
                font_key: font_key,
                logical_font_size: size,
                blur_radius: blur_radius,
                glyph_range: sub_range,
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

            self.add_primitive(&rect,
                               clip_region,
                               PrimitiveContainer::TextRun(prim_cpu, prim_gpu));
        }
    }

    pub fn add_box_shadow(&mut self,
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

        // Fast path.
        if blur_radius == 0.0 && spread_radius == 0.0 && clip_mode == BoxShadowClipMode::None {
            self.add_solid_rectangle(&box_bounds,
                                     clip_region,
                                     color,
                                     PrimitiveFlags::None);
            return;
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
                // For outset shadows, subtracting the element rectangle
                // from the outer rectangle gives the rectangles we need
                // to draw. In the simple case (no blur radius), we can
                // just draw these as solid colors.
                let mut rects = Vec::new();
                subtract_rect(&outer_rect, box_bounds, &mut rects);
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
                    self.add_solid_rectangle(rect,
                                             clip_region,
                                             color,
                                             PrimitiveFlags::None)
                }
            }
            BoxShadowKind::Shadow(rects) => {
                let inverted = match clip_mode {
                    BoxShadowClipMode::Outset | BoxShadowClipMode::None => 0.0,
                    BoxShadowClipMode::Inset => 1.0,
                };

                let prim_gpu = BoxShadowPrimitiveGpu {
                    src_rect: *box_bounds,
                    bs_rect: bs_rect,
                    color: *color,
                    blur_radius: blur_radius,
                    border_radius: border_radius,
                    edge_size: edge_size,
                    inverted: inverted,
                };

                self.add_primitive(&outer_rect,
                                   clip_region,
                                   PrimitiveContainer::BoxShadow(prim_gpu, rects));
            }
        }
    }

    pub fn add_webgl_rectangle(&mut self,
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

        self.add_primitive(&rect,
                           clip_region,
                           PrimitiveContainer::Image(prim_cpu, prim_gpu));
    }

    pub fn add_image(&mut self,
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

        self.add_primitive(&rect,
                           clip_region,
                           PrimitiveContainer::Image(prim_cpu, prim_gpu));
    }

    pub fn add_yuv_image(&mut self,
                         rect: LayerRect,
                         clip_region: &ClipRegion,
                         y_image_key: ImageKey,
                         u_image_key: ImageKey,
                         v_image_key: ImageKey,
                         color_space: YuvColorSpace) {

        let prim_cpu = YuvImagePrimitiveCpu {
            y_key: y_image_key,
            u_key: u_image_key,
            v_key: v_image_key,
            y_texture_id: SourceTexture::Invalid,
            u_texture_id: SourceTexture::Invalid,
            v_texture_id: SourceTexture::Invalid,
        };

        let prim_gpu = YuvImagePrimitiveGpu::new(rect.size, color_space);

        self.add_primitive(&rect,
                           clip_region,
                           PrimitiveContainer::YuvImage(prim_cpu, prim_gpu));
    }

    /// Compute the contribution (bounding rectangles, and resources) of layers and their
    /// primitives in screen space.
    fn build_layer_screen_rects_and_cull_layers(&mut self,
                                                screen_rect: &DeviceIntRect,
                                                clip_scroll_tree: &ClipScrollTree,
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
            let clip_scroll_node = &clip_scroll_tree.nodes[&scrollbar_prim.scroll_layer_id];

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

            let clip_source = if scrollbar_prim.border_radius == 0.0 {
                ClipSource::NoClip
            } else {
                ClipSource::Complex(geom.local_rect, scrollbar_prim.border_radius)
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

                    for filter in &stacking_context.composite_ops.filters {
                        let mut prev_task = alpha_task_stack.pop().unwrap();
                        let item = AlphaRenderItem::Blend(stacking_context_index,
                                                          current_task.id,
                                                          *filter,
                                                          next_z);
                        next_z += 1;
                        prev_task.as_alpha_batch().alpha_items.push(item);
                        prev_task.children.push(current_task);
                        current_task = prev_task;
                    }
                    if let Some(mix_blend_mode) = stacking_context.composite_ops.mix_blend_mode {
                        match HardwareCompositeOp::from_mix_blend_mode(mix_blend_mode) {
                            Some(op) => {
                                let mut prev_task = alpha_task_stack.pop().unwrap();
                                let item = AlphaRenderItem::HardwareComposite(stacking_context_index,
                                                                              current_task.id,
                                                                              op,
                                                                              next_z);
                                next_z += 1;
                                prev_task.as_alpha_batch().alpha_items.push(item);
                                prev_task.children.push(current_task);
                                current_task = prev_task;
                            }
                            None => {
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
                                prev_task.as_alpha_batch().alpha_items.push(item);
                                prev_task.children.push(current_task);
                                prev_task.children.push(readback_task);
                                current_task = prev_task;
                            }
                        }
                    }
                }
                PrimitiveRunCmd::PrimitiveRun(first_prim_index, prim_count) => {
                    let stacking_context_index = *sc_stack.last().unwrap();
                    let stacking_context = &self.stacking_context_store[stacking_context_index.0];

                    if !stacking_context.is_visible {
                        continue;
                    }

                    let stacking_context_index = *sc_stack.last().unwrap();
                    let group_index =
                        self.stacking_context_store[stacking_context_index.0].clip_scroll_group();
                    let clip_scroll_group = &self.clip_scroll_group_store[group_index.0];

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

                            let transform_kind = clip_scroll_group.xf_rect.as_ref().unwrap().kind;
                            let needs_clipping = prim_metadata.clip_task.is_some();
                            let needs_blending = transform_kind == TransformedRectKind::Complex ||
                                                 !prim_metadata.is_opaque ||
                                                 needs_clipping;

                            let items = if needs_blending {
                                &mut current_task.as_alpha_batch().alpha_items
                            } else {
                                &mut current_task.as_alpha_batch().opaque_items
                            };
                            items.push(AlphaRenderItem::Primitive(group_index, prim_index, next_z));
                            next_z += 1;
                        }
                    }
                }
                PrimitiveRunCmd::PushScrollLayer(_) | PrimitiveRunCmd::PopScrollLayer => { }
            }
        }

        debug_assert!(alpha_task_stack.is_empty());
        (current_task, next_task_index.0)
    }

    pub fn build(&mut self,
                 resource_cache: &mut ResourceCache,
                 frame_id: FrameId,
                 clip_scroll_tree: &ClipScrollTree,
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
            DeviceIntSize::from_lengths(device_length(self.screen_rect.size.width as f32,
                                                      device_pixel_ratio),
                                        device_length(self.screen_rect.size.height as f32,
                                                      device_pixel_ratio)));

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

        for scroll_layer in self.scroll_layer_store.iter() {
            if let Some(ref clip_info) = scroll_layer.clip_cache_info {
                self.prim_store.resolve_clip_cache(clip_info, resource_cache);
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
            profile_counters.targets.add(pass.targets.len());
        }

        resource_cache.end_frame();

        Frame {
            device_pixel_ratio: device_pixel_ratio,
            background_color: self.background_color,
            viewport_size: self.screen_rect.size,
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
    clip_scroll_tree: &'a ClipScrollTree,
    auxiliary_lists_map: &'a AuxiliaryListsMap,
    resource_cache: &'a mut ResourceCache,
    profile_counters: &'a mut FrameProfileCounters,
    device_pixel_ratio: f32,
    stacking_context_stack: Vec<StackingContextIndex>,
    scroll_layer_stack: Vec<ScrollLayerIndex>,

    /// A cached clip info stack, which should handle the most common situation,
    /// which is that we are using the same clip info stack that we were using
    /// previously.
    current_clip_stack: Vec<(PackedLayerIndex, MaskCacheInfo)>,

    /// The scroll layer that defines the previous scroll layer info stack.
    current_clip_stack_scroll_layer: Option<ScrollLayerIndex>
}

impl<'a> LayerRectCalculationAndCullingPass<'a> {
    fn create_and_run(frame_builder: &'a mut FrameBuilder,
                      screen_rect: &'a DeviceIntRect,
                      clip_scroll_tree: &'a ClipScrollTree,
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
            scroll_layer_stack: Vec::new(),
            current_clip_stack: Vec::new(),
            current_clip_stack_scroll_layer: None,
        };
        pass.run();
    }

    fn run(&mut self) {
        self.recalculate_clip_scroll_groups();
        self.compute_stacking_context_visibility();

        let commands = mem::replace(&mut self.frame_builder.cmds, Vec::new());
        for cmd in &commands {
            match cmd {
                &PrimitiveRunCmd::PushStackingContext(stacking_context_index) =>
                    self.handle_push_stacking_context(stacking_context_index),
                &PrimitiveRunCmd::PushScrollLayer(scroll_layer_index) =>
                    self.handle_push_scroll_layer(scroll_layer_index),
                &PrimitiveRunCmd::PrimitiveRun(prim_index, prim_count) =>
                    self.handle_primitive_run(prim_index, prim_count),
                &PrimitiveRunCmd::PopStackingContext => self.handle_pop_stacking_context(),
                &PrimitiveRunCmd::PopScrollLayer => self.handle_pop_scroll_layer(),
            }
        }

        mem::replace(&mut self.frame_builder.cmds, commands);
    }

    fn recalculate_clip_scroll_groups(&mut self) {
        for ref mut group in &mut self.frame_builder.clip_scroll_group_store {
            let stacking_context_index = group.stacking_context_index;
            let stacking_context = &mut self.frame_builder
                                            .stacking_context_store[stacking_context_index.0];

            let scroll_tree_layer = &self.clip_scroll_tree.nodes[&group.scroll_layer_id];
            let packed_layer = &mut self.frame_builder.packed_layers[group.packed_layer_index.0];
            packed_layer.transform = scroll_tree_layer.world_content_transform
                                                      .with_source::<ScrollLayerPixel>()
                                                      .pre_mul(&stacking_context.local_transform);
            packed_layer.inv_transform = packed_layer.transform.inverse().unwrap();

            if !stacking_context.can_contribute_to_scene() {
                return;
            }

            let inv_layer_transform = stacking_context.local_transform.inverse().unwrap();
            let local_viewport_rect =
                as_scroll_parent_rect(&scroll_tree_layer.combined_local_viewport_rect);
            let viewport_rect = inv_layer_transform.transform_rect(&local_viewport_rect);
            let layer_local_rect = stacking_context.local_rect.intersection(&viewport_rect);

            group.xf_rect = None;

            let layer_local_rect = match layer_local_rect {
                Some(layer_local_rect) if !layer_local_rect.is_empty() => layer_local_rect,
                _ => continue,
            };

            let layer_xf_rect = TransformedRect::new(&layer_local_rect,
                                                     &packed_layer.transform,
                                                     self.device_pixel_ratio);

            if layer_xf_rect.bounding_rect.intersects(&self.screen_rect) {
                packed_layer.screen_vertices = layer_xf_rect.vertices.clone();
                packed_layer.local_clip_rect = layer_local_rect;
                group.xf_rect = Some(layer_xf_rect);
            }
        }
    }

    fn compute_stacking_context_visibility(&mut self) {
        for context_index in 0..self.frame_builder.stacking_context_store.len() {
            let is_visible = {
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

        let bounding_rect = {
            let stacking_context =
                &mut self.frame_builder.stacking_context_store[stacking_context_index.0];
            stacking_context.bounding_rect = stacking_context.bounding_rect
                                                             .intersection(self.screen_rect)
                                                             .unwrap_or(DeviceIntRect::zero());
            stacking_context.bounding_rect.clone()
        };

        if let Some(ref mut parent_index) = self.stacking_context_stack.last_mut() {
            let parent = &mut self.frame_builder.stacking_context_store[parent_index.0];
            parent.bounding_rect = parent.bounding_rect.union(&bounding_rect);
        }
    }

    fn handle_push_scroll_layer(&mut self, scroll_layer_index: ScrollLayerIndex) {
        self.scroll_layer_stack.push(scroll_layer_index);

        let scroll_layer = &mut self.frame_builder.scroll_layer_store[scroll_layer_index.0];
        let packed_layer_index = scroll_layer.packed_layer_index;
        let scroll_tree_layer = &self.clip_scroll_tree.nodes[&scroll_layer.scroll_layer_id];
        let packed_layer = &mut self.frame_builder.packed_layers[packed_layer_index.0];

        packed_layer.transform = scroll_tree_layer.world_viewport_transform;
        packed_layer.inv_transform = packed_layer.transform.inverse().unwrap();

        let local_rect = &scroll_tree_layer.combined_local_viewport_rect
                                           .translate(&scroll_tree_layer.scrolling.offset);
        if !local_rect.is_empty() {
            let layer_xf_rect = TransformedRect::new(local_rect,
                                                     &packed_layer.transform,
                                                     self.device_pixel_ratio);

            if layer_xf_rect.bounding_rect.intersects(&self.screen_rect) {
                packed_layer.screen_vertices = layer_xf_rect.vertices.clone();
                packed_layer.local_clip_rect = *local_rect;
                scroll_layer.xf_rect = Some(layer_xf_rect);
            }
        }

        let clip_info = match scroll_layer.clip_cache_info {
            Some(ref mut clip_info) => clip_info,
            None => return,
        };

        let pipeline_id = scroll_layer.scroll_layer_id.pipeline_id;
        let auxiliary_lists = self.auxiliary_lists_map.get(&pipeline_id)
                                                       .expect("No auxiliary lists?");
        clip_info.update(&scroll_layer.clip_source,
                         &packed_layer.transform,
                         &mut self.frame_builder.prim_store.gpu_data32,
                         self.device_pixel_ratio,
                         auxiliary_lists);

        if let Some(mask) = scroll_layer.clip_source.image_mask() {
            // We don't add the image mask for resolution, because layer masks are resolved later.
            self.resource_cache.request_image(mask.image, ImageRendering::Auto, None);
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

    fn rebuild_clip_info_stack_if_necessary(&mut self, mut scroll_layer_index: ScrollLayerIndex) {
        if let Some(previous_scroll_layer) = self.current_clip_stack_scroll_layer {
            if previous_scroll_layer == scroll_layer_index {
                return;
            }
        }

        // TODO(mrobinson): If we notice that this process is expensive, we can special-case
        // more common situations, such as moving from a child or a parent.
        self.current_clip_stack_scroll_layer = Some(scroll_layer_index);
        self.current_clip_stack.clear();
        loop {
            let scroll_layer = &self.frame_builder.scroll_layer_store[scroll_layer_index.0];
            match scroll_layer.clip_cache_info {
                Some(ref clip_info) if clip_info.is_masking() =>
                    self.current_clip_stack.push((scroll_layer.packed_layer_index,
                                                  clip_info.clone())),
                _ => {},
            };

            if scroll_layer.parent_index == scroll_layer_index {
                break;
            }
            scroll_layer_index = scroll_layer.parent_index;
        }

        self.current_clip_stack.reverse();
    }

    fn handle_primitive_run(&mut self, prim_index: PrimitiveIndex, prim_count: usize) {
        let stacking_context_index = *self.stacking_context_stack.last().unwrap();
        let (packed_layer_index, pipeline_id) = {
            let stacking_context =
                &self.frame_builder.stacking_context_store[stacking_context_index.0];

            if !stacking_context.is_visible {
                return;
            }

            let group_index = stacking_context.clip_scroll_group();
            let clip_scroll_group = &self.frame_builder.clip_scroll_group_store[group_index.0];
            (clip_scroll_group.packed_layer_index, stacking_context.pipeline_id)
        };

        let scroll_layer_index = *self.scroll_layer_stack.last().unwrap();
        self.rebuild_clip_info_stack_if_necessary(scroll_layer_index);

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
                    let (mask_key, mask_rect) = match prim_clip_info {
                        Some(..) => (MaskCacheKey::Primitive(prim_index), prim_bounding_rect),
                        None => {
                            let scroll_layer =
                                &self.frame_builder.scroll_layer_store[scroll_layer_index.0];
                            (MaskCacheKey::ScrollLayer(scroll_layer_index),
                             scroll_layer.xf_rect.as_ref().unwrap().bounding_rect)
                        }
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

    fn handle_pop_scroll_layer(&mut self) {
        self.scroll_layer_stack.pop();
    }
}
