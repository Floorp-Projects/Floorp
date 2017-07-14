/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{BuiltDisplayList, ColorF, ComplexClipRegion, DeviceIntRect, DeviceIntSize, DevicePoint};
use api::{ExtendMode, FontKey, FontRenderMode, GlyphInstance, GlyphOptions, GradientStop};
use api::{ImageKey, ImageRendering, ItemRange, LayerPoint, LayerRect, LayerSize, TextShadow};
use api::{LayerToWorldTransform, TileOffset, WebGLContextId, YuvColorSpace, YuvFormat};
use api::device_length;
use app_units::Au;
use border::BorderCornerInstance;
use euclid::{Size2D};
use gpu_cache::{GpuCacheAddress, GpuBlockData, GpuCache, GpuCacheHandle, GpuDataRequest, ToGpuBlocks};
use mask_cache::{ClipMode, ClipRegion, ClipSource, MaskCacheInfo};
use renderer::MAX_VERTEX_TEXTURE_WIDTH;
use render_task::{RenderTask, RenderTaskLocation};
use resource_cache::{ImageProperties, ResourceCache};
use std::{mem, usize};
use util::{TransformedRect, recycle_vec};


pub const CLIP_DATA_GPU_BLOCKS: usize = 10;

#[derive(Debug, Copy, Clone)]
pub struct PrimitiveOpacity {
    pub is_opaque: bool,
}

impl PrimitiveOpacity {
    pub fn opaque() -> PrimitiveOpacity {
        PrimitiveOpacity {
            is_opaque: true,
        }
    }

    pub fn translucent() -> PrimitiveOpacity {
        PrimitiveOpacity {
            is_opaque: false,
        }
    }

    pub fn from_alpha(alpha: f32) -> PrimitiveOpacity {
        PrimitiveOpacity {
            is_opaque: alpha == 1.0,
        }
    }

    pub fn accumulate(&mut self, alpha: f32) {
        self.is_opaque = self.is_opaque && alpha == 1.0;
    }
}

/// Stores two coordinates in texel space. The coordinates
/// are stored in texel coordinates because the texture atlas
/// may grow. Storing them as texel coords and normalizing
/// the UVs in the vertex shader means nothing needs to be
/// updated on the CPU when the texture size changes.
#[derive(Copy, Clone, Debug)]
pub struct TexelRect {
    pub uv0: DevicePoint,
    pub uv1: DevicePoint,
}

impl TexelRect {
    pub fn new(u0: u32, v0: u32, u1: u32, v1: u32) -> TexelRect {
        TexelRect {
            uv0: DevicePoint::new(u0 as f32, v0 as f32),
            uv1: DevicePoint::new(u1 as f32, v1 as f32),
        }
    }

    pub fn invalid() -> TexelRect {
        TexelRect {
            uv0: DevicePoint::new(-1.0, -1.0),
            uv1: DevicePoint::new(-1.0, -1.0),
        }
    }
}

impl Into<GpuBlockData> for TexelRect {
    fn into(self) -> GpuBlockData {
        GpuBlockData {
            data: [self.uv0.x, self.uv0.y, self.uv1.x, self.uv1.y],
        }
    }
}

/// For external images, it's not possible to know the
/// UV coords of the image (or the image data itself)
/// until the render thread receives the frame and issues
/// callbacks to the client application. For external
/// images that are visible, a DeferredResolve is created
/// that is stored in the frame. This allows the render
/// thread to iterate this list and update any changed
/// texture data and update the UV rect.
pub struct DeferredResolve {
    pub address: GpuCacheAddress,
    pub image_properties: ImageProperties,
}

#[derive(Debug, Copy, Clone, Eq, PartialEq, Hash, Ord, PartialOrd)]
pub struct SpecificPrimitiveIndex(pub usize);

#[derive(Debug, Copy, Clone, Eq, PartialEq, Hash, Ord, PartialOrd)]
pub struct PrimitiveIndex(pub usize);

#[derive(Debug, Copy, Clone, Eq, PartialEq)]
pub enum PrimitiveKind {
    Rectangle,
    TextRun,
    Image,
    YuvImage,
    Border,
    AlignedGradient,
    AngleGradient,
    RadialGradient,
    BoxShadow,
    TextShadow,
}

#[derive(Debug, Copy, Clone, Eq, PartialEq, Hash)]
pub enum PrimitiveCacheKey {
    BoxShadow(BoxShadowPrimitiveCacheKey),
    TextShadow(PrimitiveIndex),
}

impl GpuCacheHandle {
    pub fn as_int(&self, gpu_cache: &GpuCache) -> i32 {
        let address = gpu_cache.get_address(self);

        // TODO(gw): Temporarily encode GPU Cache addresses as a single int.
        //           In the future, we can change the PrimitiveInstance struct
        //           to use 2x u16 for the vertex attribute instead of an i32.
        address.v as i32 * MAX_VERTEX_TEXTURE_WIDTH as i32 + address.u as i32
    }
}

// TODO(gw): Pack the fields here better!
#[derive(Debug)]
pub struct PrimitiveMetadata {
    pub opacity: PrimitiveOpacity,
    pub clips: Vec<ClipSource>,
    pub clip_cache_info: Option<MaskCacheInfo>,
    pub prim_kind: PrimitiveKind,
    pub cpu_prim_index: SpecificPrimitiveIndex,
    pub gpu_location: GpuCacheHandle,
    // An optional render task that is a dependency of
    // drawing this primitive. For instance, box shadows
    // use this to draw a portion of the box shadow to
    // a render target to reduce the number of pixels
    // that the box-shadow shader needs to run on. For
    // text-shadow, this creates a render task chain
    // that implements a 2-pass separable blur on a
    // text run.
    pub render_task: Option<RenderTask>,
    pub clip_task: Option<RenderTask>,

    // TODO(gw): In the future, we should just pull these
    //           directly from the DL item, instead of
    //           storing them here.
    pub local_rect: LayerRect,
    pub local_clip_rect: LayerRect,
}

impl PrimitiveMetadata {
    pub fn needs_clipping(&self) -> bool {
        self.clip_task.is_some()
    }
}

#[derive(Debug, Clone)]
#[repr(C)]
pub struct RectanglePrimitive {
    pub color: ColorF,
}

impl ToGpuBlocks for RectanglePrimitive {
    fn write_gpu_blocks(&self, mut request: GpuDataRequest) {
        request.push(self.color);
    }
}

#[derive(Debug)]
pub enum ImagePrimitiveKind {
    Image(ImageKey, ImageRendering, Option<TileOffset>, LayerSize),
    WebGL(WebGLContextId),
}

#[derive(Debug)]
pub struct ImagePrimitiveCpu {
    pub kind: ImagePrimitiveKind,
    // TODO(gw): Build on demand
    pub gpu_blocks: [GpuBlockData; 2],
}

impl ToGpuBlocks for ImagePrimitiveCpu {
    fn write_gpu_blocks(&self, mut request: GpuDataRequest) {
        request.extend_from_slice(&self.gpu_blocks);
    }
}

#[derive(Debug)]
pub struct YuvImagePrimitiveCpu {
    pub yuv_key: [ImageKey; 3],
    pub format: YuvFormat,
    pub color_space: YuvColorSpace,

    pub image_rendering: ImageRendering,

    // TODO(gw): Generate on demand
    pub gpu_block: GpuBlockData,
}

impl ToGpuBlocks for YuvImagePrimitiveCpu {
    fn write_gpu_blocks(&self, mut request: GpuDataRequest) {
        request.push(self.gpu_block);
    }
}

#[derive(Debug, Clone)]
pub struct BorderPrimitiveCpu {
    pub corner_instances: [BorderCornerInstance; 4],
    pub gpu_blocks: [GpuBlockData; 8],
}

impl ToGpuBlocks for BorderPrimitiveCpu {
    fn write_gpu_blocks(&self, mut request: GpuDataRequest) {
        request.extend_from_slice(&self.gpu_blocks);
    }
}

#[derive(Debug, Copy, Clone, Eq, PartialEq, Hash)]
pub struct BoxShadowPrimitiveCacheKey {
    pub shadow_rect_size: Size2D<Au>,
    pub border_radius: Au,
    pub blur_radius: Au,
    pub inverted: bool,
}

#[derive(Debug, Clone)]
pub struct BoxShadowPrimitiveCpu {
    // todo(gw): generate on demand
    // gpu data
    pub src_rect: LayerRect,
    pub bs_rect: LayerRect,
    pub color: ColorF,
    pub border_radius: f32,
    pub edge_size: f32,
    pub blur_radius: f32,
    pub inverted: f32,
    pub rects: Vec<LayerRect>,
}

impl ToGpuBlocks for BoxShadowPrimitiveCpu {
    fn write_gpu_blocks(&self, mut request: GpuDataRequest) {
        request.push(self.src_rect);
        request.push(self.bs_rect);
        request.push(self.color);
        request.push([self.border_radius,
                      self.edge_size,
                      self.blur_radius,
                      self.inverted]);
        for &rect in &self.rects {
            request.push(rect);
        }
    }
}

#[derive(Debug)]
pub struct GradientPrimitiveCpu {
    pub stops_range: ItemRange<GradientStop>,
    pub stops_count: usize,
    pub extend_mode: ExtendMode,
    pub reverse_stops: bool,
    pub gpu_blocks: [GpuBlockData; 3],
}

impl GradientPrimitiveCpu {
    fn build_gpu_blocks_for_aligned(&self,
                                    display_list: &BuiltDisplayList,
                                    mut request: GpuDataRequest) -> PrimitiveOpacity {
        let mut opacity = PrimitiveOpacity::opaque();
        request.extend_from_slice(&self.gpu_blocks);
        let src_stops = display_list.get(self.stops_range);

        for src in src_stops {
            request.push(src.color.premultiplied());
            request.push([src.offset, 0.0, 0.0, 0.0]);
            opacity.accumulate(src.color.a);
        }

        opacity
    }

    fn build_gpu_blocks_for_angle_radial(&self,
                                         display_list: &BuiltDisplayList,
                                         mut request: GpuDataRequest) {
        request.extend_from_slice(&self.gpu_blocks);

        let gradient_builder = GradientGpuBlockBuilder::new(self.stops_range,
                                                            display_list);
        gradient_builder.build(self.reverse_stops, &mut request);
    }
}

// The gradient entry index for the first color stop
pub const GRADIENT_DATA_FIRST_STOP: usize = 0;
// The gradient entry index for the last color stop
pub const GRADIENT_DATA_LAST_STOP: usize = GRADIENT_DATA_SIZE - 1;

// The start of the gradient data table
pub const GRADIENT_DATA_TABLE_BEGIN: usize = GRADIENT_DATA_FIRST_STOP + 1;
// The exclusive bound of the gradient data table
pub const GRADIENT_DATA_TABLE_END: usize = GRADIENT_DATA_LAST_STOP;
// The number of entries in the gradient data table.
pub const GRADIENT_DATA_TABLE_SIZE: usize = 128;

// The number of entries in a gradient data: GRADIENT_DATA_TABLE_SIZE + first stop entry + last stop entry
pub const GRADIENT_DATA_SIZE: usize = GRADIENT_DATA_TABLE_SIZE + 2;

#[derive(Debug, Clone, Copy)]
#[repr(C)]
// An entry in a gradient data table representing a segment of the gradient color space.
pub struct GradientDataEntry {
    pub start_color: ColorF,
    pub end_color: ColorF,
}

struct GradientGpuBlockBuilder<'a> {
    stops_range: ItemRange<GradientStop>,
    display_list: &'a BuiltDisplayList,
}

impl<'a> GradientGpuBlockBuilder<'a> {
    fn new(stops_range: ItemRange<GradientStop>,
           display_list: &'a BuiltDisplayList) -> GradientGpuBlockBuilder<'a> {
        GradientGpuBlockBuilder {
            stops_range,
            display_list,
        }
    }

    /// Generate a color ramp filling the indices in [start_idx, end_idx) and interpolating
    /// from start_color to end_color.
    fn fill_colors(&self,
                   start_idx: usize,
                   end_idx: usize,
                   start_color: &ColorF,
                   end_color: &ColorF,
                   entries: &mut [GradientDataEntry; GRADIENT_DATA_SIZE]) {
        // Calculate the color difference for individual steps in the ramp.
        let inv_steps = 1.0 / (end_idx - start_idx) as f32;
        let step_r = (end_color.r - start_color.r) * inv_steps;
        let step_g = (end_color.g - start_color.g) * inv_steps;
        let step_b = (end_color.b - start_color.b) * inv_steps;
        let step_a = (end_color.a - start_color.a) * inv_steps;

        let mut cur_color = *start_color;

        // Walk the ramp writing start and end colors for each entry.
        for index in start_idx..end_idx {
            let entry = &mut entries[index];
            entry.start_color = cur_color;
            cur_color.r += step_r;
            cur_color.g += step_g;
            cur_color.b += step_b;
            cur_color.a += step_a;
            entry.end_color = cur_color;
        }
    }

    /// Compute an index into the gradient entry table based on a gradient stop offset. This
    /// function maps offsets from [0, 1] to indices in [GRADIENT_DATA_TABLE_BEGIN, GRADIENT_DATA_TABLE_END].
    #[inline]
    fn get_index(offset: f32) -> usize {
        (offset.max(0.0).min(1.0)
            * GRADIENT_DATA_TABLE_SIZE as f32
            + GRADIENT_DATA_TABLE_BEGIN as f32).round() as usize
    }

    // Build the gradient data from the supplied stops, reversing them if necessary.
    fn build(&self, reverse_stops: bool, request: &mut GpuDataRequest) {
        let src_stops = self.display_list.get(self.stops_range);

        // Preconditions (should be ensured by DisplayListBuilder):
        // * we have at least two stops
        // * first stop has offset 0.0
        // * last stop has offset 1.0

        let mut src_stops = src_stops.into_iter();
        let first = src_stops.next().unwrap();
        let mut cur_color = first.color.premultiplied();
        debug_assert_eq!(first.offset, 0.0);

        // A table of gradient entries, with two colors per entry, that specify the start and end color
        // within the segment of the gradient space represented by that entry. To lookup a gradient result,
        // first the entry index is calculated to determine which two colors to interpolate between, then
        // the offset within that entry bucket is used to interpolate between the two colors in that entry.
        // This layout preserves hard stops, as the end color for a given entry can differ from the start
        // color for the following entry, despite them being adjacent. Colors are stored within in BGRA8
        // format for texture upload. This table requires the gradient color stops to be normalized to the
        // range [0, 1]. The first and last entries hold the first and last color stop colors respectively,
        // while the entries in between hold the interpolated color stop values for the range [0, 1].
        let mut entries: [GradientDataEntry; GRADIENT_DATA_SIZE] = unsafe { mem::uninitialized() };

        if reverse_stops {
            // Fill in the first entry (for reversed stops) with the first color stop
            self.fill_colors(GRADIENT_DATA_LAST_STOP, GRADIENT_DATA_LAST_STOP + 1, &cur_color, &cur_color, &mut entries);

            // Fill in the center of the gradient table, generating a color ramp between each consecutive pair
            // of gradient stops. Each iteration of a loop will fill the indices in [next_idx, cur_idx). The
            // loop will then fill indices in [GRADIENT_DATA_TABLE_BEGIN, GRADIENT_DATA_TABLE_END).
            let mut cur_idx = GRADIENT_DATA_TABLE_END;
            for next in src_stops {
                let next_color = next.color.premultiplied();
                let next_idx = Self::get_index(1.0 - next.offset);

                if next_idx < cur_idx {
                    self.fill_colors(next_idx, cur_idx,
                                     &next_color, &cur_color, &mut entries);
                    cur_idx = next_idx;
                }

                cur_color = next_color;
            }
            debug_assert_eq!(cur_idx, GRADIENT_DATA_TABLE_BEGIN);

            // Fill in the last entry (for reversed stops) with the last color stop
            self.fill_colors(GRADIENT_DATA_FIRST_STOP, GRADIENT_DATA_FIRST_STOP + 1, &cur_color, &cur_color, &mut entries);
        } else {
            // Fill in the first entry with the first color stop
            self.fill_colors(GRADIENT_DATA_FIRST_STOP, GRADIENT_DATA_FIRST_STOP + 1, &cur_color, &cur_color, &mut entries);

            // Fill in the center of the gradient table, generating a color ramp between each consecutive pair
            // of gradient stops. Each iteration of a loop will fill the indices in [cur_idx, next_idx). The
            // loop will then fill indices in [GRADIENT_DATA_TABLE_BEGIN, GRADIENT_DATA_TABLE_END).
            let mut cur_idx = GRADIENT_DATA_TABLE_BEGIN;
            for next in src_stops {
                let next_color = next.color.premultiplied();
                let next_idx = Self::get_index(next.offset);

                if next_idx > cur_idx {
                    self.fill_colors(cur_idx, next_idx,
                                     &cur_color, &next_color, &mut entries);
                    cur_idx = next_idx;
                }

                cur_color = next_color;
            }
            debug_assert_eq!(cur_idx, GRADIENT_DATA_TABLE_END);

            // Fill in the last entry with the last color stop
            self.fill_colors(GRADIENT_DATA_LAST_STOP, GRADIENT_DATA_LAST_STOP + 1, &cur_color, &cur_color, &mut entries);
        }

        for entry in entries.iter() {
            request.push(entry.start_color);
            request.push(entry.end_color);
        }
    }
}

#[derive(Debug)]
pub struct RadialGradientPrimitiveCpu {
    pub stops_range: ItemRange<GradientStop>,
    pub extend_mode: ExtendMode,
    pub gpu_data_count: i32,
    pub gpu_blocks: [GpuBlockData; 3],
}

impl RadialGradientPrimitiveCpu {
    fn build_gpu_blocks_for_angle_radial(&self,
                                         display_list: &BuiltDisplayList,
                                         mut request: GpuDataRequest) {
        request.extend_from_slice(&self.gpu_blocks);

        let gradient_builder = GradientGpuBlockBuilder::new(self.stops_range,
                                                            display_list);
        gradient_builder.build(false, &mut request);
    }
}

#[derive(Debug, Clone)]
pub struct TextShadowPrimitiveCpu {
    pub text_primitives: Vec<TextRunPrimitiveCpu>,
    pub shadow: TextShadow,
}

#[derive(Debug, Clone)]
pub struct TextRunPrimitiveCpu {
    pub font_key: FontKey,
    pub logical_font_size: Au,
    pub glyph_range: ItemRange<GlyphInstance>,
    pub glyph_count: usize,
    // TODO(gw): Maybe make this an Arc for sharing with resource cache
    pub glyph_instances: Vec<GlyphInstance>,
    pub color: ColorF,
    pub render_mode: FontRenderMode,
    pub glyph_options: Option<GlyphOptions>,
}

impl TextRunPrimitiveCpu {
    fn prepare_for_render(&mut self,
                          resource_cache: &mut ResourceCache,
                          device_pixel_ratio: f32,
                          display_list: &BuiltDisplayList) {
        // Cache the glyph positions, if not in the cache already.
        // TODO(gw): In the future, remove `glyph_instances`
        //           completely, and just reference the glyphs
        //           directly from the displaty list.
        if self.glyph_instances.is_empty() {
            let src_glyphs = display_list.get(self.glyph_range);
            for src in src_glyphs {
                self.glyph_instances.push(GlyphInstance {
                    index: src.index,
                    point: src.point,
                });
            }
        }

        let font_size_dp = self.logical_font_size.scale_by(device_pixel_ratio);

        resource_cache.request_glyphs(self.font_key,
                                      font_size_dp,
                                      self.color,
                                      &self.glyph_instances,
                                      self.render_mode,
                                      self.glyph_options);
    }

    fn write_gpu_blocks(&self, request: &mut GpuDataRequest) {
        // Two glyphs are packed per GPU block.
        for glyph_chunk in self.glyph_instances.chunks(2) {
            // In the case of an odd number of glyphs, the
            // last glyph will get duplicated in the final
            // GPU block.
            let first_glyph = glyph_chunk.first().unwrap();
            let second_glyph = glyph_chunk.last().unwrap();
            let data = match self.render_mode {
                FontRenderMode::Mono |
                FontRenderMode::Alpha => [
                    first_glyph.point.x,
                    first_glyph.point.y,
                    second_glyph.point.x,
                    second_glyph.point.y,
                ],
                // The sub-pixel offset has already been taken into account
                // by the glyph rasterizer, thus the truncating here.
                FontRenderMode::Subpixel => [
                    first_glyph.point.x.trunc(),
                    first_glyph.point.y.trunc(),
                    second_glyph.point.x.trunc(),
                    second_glyph.point.y.trunc(),
                ],
            };
            request.push(data);
        }
    }
}

#[derive(Debug, Clone)]
#[repr(C)]
struct GlyphPrimitive {
    offset: LayerPoint,
    padding: LayerPoint,
}

#[derive(Debug, Clone)]
#[repr(C)]
struct ClipRect {
    rect: LayerRect,
    mode: f32,
}

#[derive(Debug, Clone)]
#[repr(C)]
struct ClipCorner {
    rect: LayerRect,
    outer_radius_x: f32,
    outer_radius_y: f32,
    inner_radius_x: f32,
    inner_radius_y: f32,
}

impl ToGpuBlocks for ClipCorner {
    fn write_gpu_blocks(&self, mut request: GpuDataRequest) {
        self.write(&mut request)
    }
}

impl ClipCorner {
    fn write(&self, request: &mut GpuDataRequest) {
        request.push(self.rect);
        request.push([self.outer_radius_x, self.outer_radius_y,
                      self.inner_radius_x, self.inner_radius_y,
                     ]);
    }

    fn uniform(rect: LayerRect, outer_radius: f32, inner_radius: f32) -> ClipCorner {
        ClipCorner {
            rect,
            outer_radius_x: outer_radius,
            outer_radius_y: outer_radius,
            inner_radius_x: inner_radius,
            inner_radius_y: inner_radius,
        }
    }
}

#[derive(Debug, Clone)]
#[repr(C)]
pub struct ImageMaskData {
    pub local_rect: LayerRect,
}

impl ToGpuBlocks for ImageMaskData {
    fn write_gpu_blocks(&self, mut request: GpuDataRequest) {
        request.push(self.local_rect);
    }
}

#[derive(Debug, Clone)]
pub struct ClipData {
    rect: ClipRect,
    top_left: ClipCorner,
    top_right: ClipCorner,
    bottom_left: ClipCorner,
    bottom_right: ClipCorner,
}

impl ClipData {
    pub fn from_clip_region(clip: &ComplexClipRegion) -> ClipData {
        ClipData {
            rect: ClipRect {
                rect: clip.rect,
                // TODO(gw): Support other clip modes for regions?
                mode: ClipMode::Clip as u32 as f32,
            },
            top_left: ClipCorner {
                rect: LayerRect::new(
                    LayerPoint::new(clip.rect.origin.x, clip.rect.origin.y),
                    LayerSize::new(clip.radii.top_left.width, clip.radii.top_left.height)),
                outer_radius_x: clip.radii.top_left.width,
                outer_radius_y: clip.radii.top_left.height,
                inner_radius_x: 0.0,
                inner_radius_y: 0.0,
            },
            top_right: ClipCorner {
                rect: LayerRect::new(
                    LayerPoint::new(clip.rect.origin.x + clip.rect.size.width - clip.radii.top_right.width, clip.rect.origin.y),
                    LayerSize::new(clip.radii.top_right.width, clip.radii.top_right.height)),
                outer_radius_x: clip.radii.top_right.width,
                outer_radius_y: clip.radii.top_right.height,
                inner_radius_x: 0.0,
                inner_radius_y: 0.0,
            },
            bottom_left: ClipCorner {
                rect: LayerRect::new(
                    LayerPoint::new(clip.rect.origin.x, clip.rect.origin.y + clip.rect.size.height - clip.radii.bottom_left.height),
                    LayerSize::new(clip.radii.bottom_left.width, clip.radii.bottom_left.height)),
                outer_radius_x: clip.radii.bottom_left.width,
                outer_radius_y: clip.radii.bottom_left.height,
                inner_radius_x: 0.0,
                inner_radius_y: 0.0,
            },
            bottom_right: ClipCorner {
                rect: LayerRect::new(
                    LayerPoint::new(clip.rect.origin.x + clip.rect.size.width - clip.radii.bottom_right.width,
                                    clip.rect.origin.y + clip.rect.size.height - clip.radii.bottom_right.height),
                    LayerSize::new(clip.radii.bottom_right.width, clip.radii.bottom_right.height)),
                outer_radius_x: clip.radii.bottom_right.width,
                outer_radius_y: clip.radii.bottom_right.height,
                inner_radius_x: 0.0,
                inner_radius_y: 0.0,
            },
        }
    }

    pub fn uniform(rect: LayerRect, radius: f32, mode: ClipMode) -> ClipData {
        ClipData {
            rect: ClipRect {
                rect,
                mode: mode as u32 as f32,
            },
            top_left: ClipCorner::uniform(
                LayerRect::new(
                    LayerPoint::new(rect.origin.x, rect.origin.y),
                    LayerSize::new(radius, radius)),
                radius, 0.0),
            top_right: ClipCorner::uniform(
                LayerRect::new(
                    LayerPoint::new(rect.origin.x + rect.size.width - radius, rect.origin.y),
                    LayerSize::new(radius, radius)),
                radius, 0.0),
            bottom_left: ClipCorner::uniform(
                LayerRect::new(
                    LayerPoint::new(rect.origin.x, rect.origin.y + rect.size.height - radius),
                    LayerSize::new(radius, radius)),
                radius, 0.0),
            bottom_right: ClipCorner::uniform(
                LayerRect::new(
                    LayerPoint::new(rect.origin.x + rect.size.width - radius, rect.origin.y + rect.size.height - radius),
                    LayerSize::new(radius, radius)),
                radius, 0.0),
        }
    }

    pub fn write(&self, request: &mut GpuDataRequest) {
        request.push(self.rect.rect);
        request.push([self.rect.mode, 0.0, 0.0, 0.0]);
        for corner in &[&self.top_left, &self.top_right, &self.bottom_left, &self.bottom_right] {
            corner.write(request);
        }
    }
}

#[derive(Debug)]
pub enum PrimitiveContainer {
    Rectangle(RectanglePrimitive),
    TextRun(TextRunPrimitiveCpu),
    Image(ImagePrimitiveCpu),
    YuvImage(YuvImagePrimitiveCpu),
    Border(BorderPrimitiveCpu),
    AlignedGradient(GradientPrimitiveCpu),
    AngleGradient(GradientPrimitiveCpu),
    RadialGradient(RadialGradientPrimitiveCpu),
    BoxShadow(BoxShadowPrimitiveCpu),
    TextShadow(TextShadowPrimitiveCpu),
}

pub struct PrimitiveStore {
    /// CPU side information only.
    pub cpu_bounding_rects: Vec<Option<DeviceIntRect>>,
    pub cpu_rectangles: Vec<RectanglePrimitive>,
    pub cpu_text_runs: Vec<TextRunPrimitiveCpu>,
    pub cpu_text_shadows: Vec<TextShadowPrimitiveCpu>,
    pub cpu_images: Vec<ImagePrimitiveCpu>,
    pub cpu_yuv_images: Vec<YuvImagePrimitiveCpu>,
    pub cpu_gradients: Vec<GradientPrimitiveCpu>,
    pub cpu_radial_gradients: Vec<RadialGradientPrimitiveCpu>,
    pub cpu_metadata: Vec<PrimitiveMetadata>,
    pub cpu_borders: Vec<BorderPrimitiveCpu>,
    pub cpu_box_shadows: Vec<BoxShadowPrimitiveCpu>,
}

impl PrimitiveStore {
    pub fn new() -> PrimitiveStore {
        PrimitiveStore {
            cpu_metadata: Vec::new(),
            cpu_rectangles: Vec::new(),
            cpu_bounding_rects: Vec::new(),
            cpu_text_runs: Vec::new(),
            cpu_text_shadows: Vec::new(),
            cpu_images: Vec::new(),
            cpu_yuv_images: Vec::new(),
            cpu_gradients: Vec::new(),
            cpu_radial_gradients: Vec::new(),
            cpu_borders: Vec::new(),
            cpu_box_shadows: Vec::new(),
        }
    }

    pub fn recycle(self) -> Self {
        PrimitiveStore {
            cpu_metadata: recycle_vec(self.cpu_metadata),
            cpu_rectangles: recycle_vec(self.cpu_rectangles),
            cpu_bounding_rects: recycle_vec(self.cpu_bounding_rects),
            cpu_text_runs: recycle_vec(self.cpu_text_runs),
            cpu_text_shadows: recycle_vec(self.cpu_text_shadows),
            cpu_images: recycle_vec(self.cpu_images),
            cpu_yuv_images: recycle_vec(self.cpu_yuv_images),
            cpu_gradients: recycle_vec(self.cpu_gradients),
            cpu_radial_gradients: recycle_vec(self.cpu_radial_gradients),
            cpu_borders: recycle_vec(self.cpu_borders),
            cpu_box_shadows: recycle_vec(self.cpu_box_shadows),
        }
    }

    pub fn add_primitive(&mut self,
                         local_rect: &LayerRect,
                         local_clip_rect: &LayerRect,
                         clips: Vec<ClipSource>,
                         clip_info: Option<MaskCacheInfo>,
                         container: PrimitiveContainer) -> PrimitiveIndex {
        let prim_index = self.cpu_metadata.len();
        self.cpu_bounding_rects.push(None);

        let metadata = match container {
            PrimitiveContainer::Rectangle(rect) => {
                let metadata = PrimitiveMetadata {
                    opacity: PrimitiveOpacity::from_alpha(rect.color.a),
                    clips,
                    clip_cache_info: clip_info,
                    prim_kind: PrimitiveKind::Rectangle,
                    cpu_prim_index: SpecificPrimitiveIndex(self.cpu_rectangles.len()),
                    gpu_location: GpuCacheHandle::new(),
                    render_task: None,
                    clip_task: None,
                    local_rect: *local_rect,
                    local_clip_rect: *local_clip_rect,
                };

                self.cpu_rectangles.push(rect);

                metadata
            }
            PrimitiveContainer::TextRun(text_cpu) => {
                let metadata = PrimitiveMetadata {
                    opacity: PrimitiveOpacity::translucent(),
                    clips,
                    clip_cache_info: clip_info,
                    prim_kind: PrimitiveKind::TextRun,
                    cpu_prim_index: SpecificPrimitiveIndex(self.cpu_text_runs.len()),
                    gpu_location: GpuCacheHandle::new(),
                    render_task: None,
                    clip_task: None,
                    local_rect: *local_rect,
                    local_clip_rect: *local_clip_rect,
                };

                self.cpu_text_runs.push(text_cpu);
                metadata
            }
            PrimitiveContainer::TextShadow(text_shadow) => {
                let metadata = PrimitiveMetadata {
                    opacity: PrimitiveOpacity::translucent(),
                    clips,
                    clip_cache_info: clip_info,
                    prim_kind: PrimitiveKind::TextShadow,
                    cpu_prim_index: SpecificPrimitiveIndex(self.cpu_text_shadows.len()),
                    gpu_location: GpuCacheHandle::new(),
                    render_task: None,
                    clip_task: None,
                    local_rect: *local_rect,
                    local_clip_rect: *local_clip_rect,
                };

                self.cpu_text_shadows.push(text_shadow);
                metadata
            }
            PrimitiveContainer::Image(image_cpu) => {
                let metadata = PrimitiveMetadata {
                    opacity: PrimitiveOpacity::translucent(),
                    clips,
                    clip_cache_info: clip_info,
                    prim_kind: PrimitiveKind::Image,
                    cpu_prim_index: SpecificPrimitiveIndex(self.cpu_images.len()),
                    gpu_location: GpuCacheHandle::new(),
                    render_task: None,
                    clip_task: None,
                    local_rect: *local_rect,
                    local_clip_rect: *local_clip_rect,
                };

                self.cpu_images.push(image_cpu);
                metadata
            }
            PrimitiveContainer::YuvImage(image_cpu) => {
                let metadata = PrimitiveMetadata {
                    opacity: PrimitiveOpacity::opaque(),
                    clips,
                    clip_cache_info: clip_info,
                    prim_kind: PrimitiveKind::YuvImage,
                    cpu_prim_index: SpecificPrimitiveIndex(self.cpu_yuv_images.len()),
                    gpu_location: GpuCacheHandle::new(),
                    render_task: None,
                    clip_task: None,
                    local_rect: *local_rect,
                    local_clip_rect: *local_clip_rect,
                };

                self.cpu_yuv_images.push(image_cpu);
                metadata
            }
            PrimitiveContainer::Border(border_cpu) => {
                let metadata = PrimitiveMetadata {
                    opacity: PrimitiveOpacity::translucent(),
                    clips,
                    clip_cache_info: clip_info,
                    prim_kind: PrimitiveKind::Border,
                    cpu_prim_index: SpecificPrimitiveIndex(self.cpu_borders.len()),
                    gpu_location: GpuCacheHandle::new(),
                    render_task: None,
                    clip_task: None,
                    local_rect: *local_rect,
                    local_clip_rect: *local_clip_rect,
                };

                self.cpu_borders.push(border_cpu);
                metadata
            }
            PrimitiveContainer::AlignedGradient(gradient_cpu) => {
                let metadata = PrimitiveMetadata {
                    opacity: PrimitiveOpacity::translucent(),
                    clips,
                    clip_cache_info: clip_info,
                    prim_kind: PrimitiveKind::AlignedGradient,
                    cpu_prim_index: SpecificPrimitiveIndex(self.cpu_gradients.len()),
                    gpu_location: GpuCacheHandle::new(),
                    render_task: None,
                    clip_task: None,
                    local_rect: *local_rect,
                    local_clip_rect: *local_clip_rect,
                };

                self.cpu_gradients.push(gradient_cpu);
                metadata
            }
            PrimitiveContainer::AngleGradient(gradient_cpu) => {
                let metadata = PrimitiveMetadata {
                    // TODO: calculate if the gradient is actually opaque
                    opacity: PrimitiveOpacity::translucent(),
                    clips,
                    clip_cache_info: clip_info,
                    prim_kind: PrimitiveKind::AngleGradient,
                    cpu_prim_index: SpecificPrimitiveIndex(self.cpu_gradients.len()),
                    gpu_location: GpuCacheHandle::new(),
                    render_task: None,
                    clip_task: None,
                    local_rect: *local_rect,
                    local_clip_rect: *local_clip_rect,
                };

                self.cpu_gradients.push(gradient_cpu);
                metadata
            }
            PrimitiveContainer::RadialGradient(radial_gradient_cpu) => {
                let metadata = PrimitiveMetadata {
                    // TODO: calculate if the gradient is actually opaque
                    opacity: PrimitiveOpacity::translucent(),
                    clips,
                    clip_cache_info: clip_info,
                    prim_kind: PrimitiveKind::RadialGradient,
                    cpu_prim_index: SpecificPrimitiveIndex(self.cpu_radial_gradients.len()),
                    gpu_location: GpuCacheHandle::new(),
                    render_task: None,
                    clip_task: None,
                    local_rect: *local_rect,
                    local_clip_rect: *local_clip_rect,
                };

                self.cpu_radial_gradients.push(radial_gradient_cpu);
                metadata
            }
            PrimitiveContainer::BoxShadow(box_shadow) => {
                let cache_key = PrimitiveCacheKey::BoxShadow(BoxShadowPrimitiveCacheKey {
                    blur_radius: Au::from_f32_px(box_shadow.blur_radius),
                    border_radius: Au::from_f32_px(box_shadow.border_radius),
                    inverted: box_shadow.inverted != 0.0,
                    shadow_rect_size: Size2D::new(Au::from_f32_px(box_shadow.bs_rect.size.width),
                                                  Au::from_f32_px(box_shadow.bs_rect.size.height)),
                });

                // The actual cache size is calculated during prepare_prim_for_render().
                // This is necessary since the size may change depending on the device
                // pixel ratio (for example, during zoom or moving the window to a
                // monitor with a different device pixel ratio).
                let cache_size = DeviceIntSize::zero();

                // Create a render task for this box shadow primitive. This renders a small
                // portion of the box shadow to a render target. That portion is then
                // stretched over the actual primitive rect by the box shadow primitive
                // shader, to reduce the number of pixels that the expensive box
                // shadow shader needs to run on.
                // TODO(gw): In the future, we can probably merge the box shadow
                // primitive (stretch) shader with the generic cached primitive shader.
                let render_task = RenderTask::new_prim_cache(cache_key,
                                                             cache_size,
                                                             PrimitiveIndex(prim_index));

                let metadata = PrimitiveMetadata {
                    opacity: PrimitiveOpacity::translucent(),
                    clips,
                    clip_cache_info: clip_info,
                    prim_kind: PrimitiveKind::BoxShadow,
                    cpu_prim_index: SpecificPrimitiveIndex(self.cpu_box_shadows.len()),
                    gpu_location: GpuCacheHandle::new(),
                    render_task: Some(render_task),
                    clip_task: None,
                    local_rect: *local_rect,
                    local_clip_rect: *local_clip_rect,
                };

                self.cpu_box_shadows.push(box_shadow);
                metadata
            }
        };

        self.cpu_metadata.push(metadata);

        PrimitiveIndex(prim_index)
    }

    pub fn get_metadata(&self, index: PrimitiveIndex) -> &PrimitiveMetadata {
        &self.cpu_metadata[index.0]
    }

    pub fn prim_count(&self) -> usize {
        self.cpu_metadata.len()
    }

    pub fn build_bounding_rect(&mut self,
                               prim_index: PrimitiveIndex,
                               screen_rect: &DeviceIntRect,
                               layer_transform: &LayerToWorldTransform,
                               layer_combined_local_clip_rect: &LayerRect,
                               device_pixel_ratio: f32) -> Option<(LayerRect, DeviceIntRect)> {
        let metadata = &self.cpu_metadata[prim_index.0];
        let local_rect = metadata.local_rect
                                 .intersection(&metadata.local_clip_rect)
                                 .and_then(|rect| rect.intersection(layer_combined_local_clip_rect));

        let bounding_rect = local_rect.and_then(|local_rect| {
            let xf_rect = TransformedRect::new(&local_rect,
                                               layer_transform,
                                               device_pixel_ratio);
            xf_rect.bounding_rect.intersection(screen_rect)
        });

        self.cpu_bounding_rects[prim_index.0] = bounding_rect;
        bounding_rect.map(|screen_bound| (local_rect.unwrap(), screen_bound))
    }

    /// Returns true if the bounding box needs to be updated.
    pub fn prepare_prim_for_render(&mut self,
                                   prim_index: PrimitiveIndex,
                                   resource_cache: &mut ResourceCache,
                                   gpu_cache: &mut GpuCache,
                                   layer_transform: &LayerToWorldTransform,
                                   device_pixel_ratio: f32,
                                   display_list: &BuiltDisplayList)
                                   -> &mut PrimitiveMetadata {

        let metadata = &mut self.cpu_metadata[prim_index.0];

        if let Some(ref mut clip_info) = metadata.clip_cache_info {
            clip_info.update(&metadata.clips, layer_transform, gpu_cache, device_pixel_ratio);

            //TODO-LCCR: we could tighten up the `local_clip_rect` here
            // but that would require invalidating the whole GPU block

            for clip in &metadata.clips {
                if let ClipSource::Region(ClipRegion{ image_mask: Some(ref mask), .. }, ..) = *clip {
                    resource_cache.request_image(mask.image, ImageRendering::Auto, None);
                }
            }
        }

        match metadata.prim_kind {
            PrimitiveKind::Rectangle |
            PrimitiveKind::Border  => {}
            PrimitiveKind::BoxShadow => {
                // TODO(gw): Account for zoom factor!
                // Here, we calculate the size of the patch required in order
                // to create the box shadow corner. First, scale it by the
                // device pixel ratio since the cache shader expects vertices
                // in device space. The shader adds a 1-pixel border around
                // the patch, in order to prevent bilinear filter artifacts as
                // the patch is clamped / mirrored across the box shadow rect.
                let box_shadow_cpu = &self.cpu_box_shadows[metadata.cpu_prim_index.0];
                let edge_size = box_shadow_cpu.edge_size.ceil() * device_pixel_ratio;
                let edge_size = edge_size as i32 + 2;   // Account for bilinear filtering
                let cache_size = DeviceIntSize::new(edge_size, edge_size);
                let location = RenderTaskLocation::Dynamic(None, cache_size);
                metadata.render_task.as_mut().unwrap().location = location;
            }
            PrimitiveKind::TextShadow => {
                let shadow = &mut self.cpu_text_shadows[metadata.cpu_prim_index.0];
                for text in &mut shadow.text_primitives {
                    text.prepare_for_render(resource_cache,
                                            device_pixel_ratio,
                                            display_list);
                }

                // This is a text-shadow element. Create a render task that will
                // render the text run to a target, and then apply a gaussian
                // blur to that text run in order to build the actual primitive
                // which will be blitted to the framebuffer.
                let cache_width = (metadata.local_rect.size.width * device_pixel_ratio).ceil() as i32;
                let cache_height = (metadata.local_rect.size.height * device_pixel_ratio).ceil() as i32;
                let cache_size = DeviceIntSize::new(cache_width, cache_height);
                let cache_key = PrimitiveCacheKey::TextShadow(prim_index);
                let blur_radius = device_length(shadow.shadow.blur_radius,
                                                device_pixel_ratio);
                metadata.render_task = Some(RenderTask::new_blur(cache_key,
                                                                 cache_size,
                                                                 blur_radius,
                                                                 prim_index));
            }
            PrimitiveKind::TextRun => {
                let text = &mut self.cpu_text_runs[metadata.cpu_prim_index.0];
                text.prepare_for_render(resource_cache,
                                        device_pixel_ratio,
                                        display_list);
            }
            PrimitiveKind::Image => {
                let image_cpu = &mut self.cpu_images[metadata.cpu_prim_index.0];

                match image_cpu.kind {
                    ImagePrimitiveKind::Image(image_key, image_rendering, tile_offset, tile_spacing) => {
                        resource_cache.request_image(image_key, image_rendering, tile_offset);

                        // TODO(gw): This doesn't actually need to be calculated each frame.
                        // It's cheap enough that it's not worth introducing a cache for images
                        // right now, but if we introduce a cache for images for some other
                        // reason then we might as well cache this with it.
                        let image_properties = resource_cache.get_image_properties(image_key);
                        metadata.opacity.is_opaque = image_properties.descriptor.is_opaque &&
                                                     tile_spacing.width == 0.0 &&
                                                     tile_spacing.height == 0.0;
                    }
                    ImagePrimitiveKind::WebGL(..) => {}
                }
            }
            PrimitiveKind::YuvImage => {
                let image_cpu = &mut self.cpu_yuv_images[metadata.cpu_prim_index.0];

                let channel_num = image_cpu.format.get_plane_num();
                debug_assert!(channel_num <= 3);
                for channel in 0..channel_num {
                    resource_cache.request_image(image_cpu.yuv_key[channel], image_cpu.image_rendering, None);
                }
            }
            PrimitiveKind::AlignedGradient |
            PrimitiveKind::AngleGradient |
            PrimitiveKind::RadialGradient => {}
        }

        // Mark this GPU resource as required for this frame.
        if let Some(mut request) = gpu_cache.request(&mut metadata.gpu_location) {
            request.push(metadata.local_rect);
            request.push(metadata.local_clip_rect);

            match metadata.prim_kind {
                PrimitiveKind::Rectangle => {
                    let rect = &self.cpu_rectangles[metadata.cpu_prim_index.0];
                    rect.write_gpu_blocks(request);
                }
                PrimitiveKind::Border => {
                    let border = &self.cpu_borders[metadata.cpu_prim_index.0];
                    border.write_gpu_blocks(request);
                }
                PrimitiveKind::BoxShadow => {
                    let box_shadow = &self.cpu_box_shadows[metadata.cpu_prim_index.0];
                    box_shadow.write_gpu_blocks(request);
                }
                PrimitiveKind::Image => {
                    let image = &self.cpu_images[metadata.cpu_prim_index.0];
                    image.write_gpu_blocks(request);
                }
                PrimitiveKind::YuvImage => {
                    let yuv_image = &self.cpu_yuv_images[metadata.cpu_prim_index.0];
                    yuv_image.write_gpu_blocks(request);
                }
                PrimitiveKind::AlignedGradient => {
                    let gradient = &self.cpu_gradients[metadata.cpu_prim_index.0];
                    metadata.opacity = gradient.build_gpu_blocks_for_aligned(display_list,
                                                                             request);
                }
                PrimitiveKind::AngleGradient => {
                    let gradient = &self.cpu_gradients[metadata.cpu_prim_index.0];
                    gradient.build_gpu_blocks_for_angle_radial(display_list,
                                                               request);
                }
                PrimitiveKind::RadialGradient => {
                    let gradient = &self.cpu_radial_gradients[metadata.cpu_prim_index.0];
                    gradient.build_gpu_blocks_for_angle_radial(display_list,
                                                               request);
                }
                PrimitiveKind::TextRun => {
                    let text = &self.cpu_text_runs[metadata.cpu_prim_index.0];
                    request.push(text.color);
                    text.write_gpu_blocks(&mut request);
                }
                PrimitiveKind::TextShadow => {
                    let prim = &self.cpu_text_shadows[metadata.cpu_prim_index.0];
                    request.push(prim.shadow.color);
                    for text in &prim.text_primitives {
                        text.write_gpu_blocks(&mut request);
                    }
                }
            }
        }

        metadata
    }
}


//Test for one clip region contains another
trait InsideTest<T> {
    fn might_contain(&self, clip: &T) -> bool;
}

impl InsideTest<ComplexClipRegion> for ComplexClipRegion {
    // Returns true if clip is inside self, can return false negative
    fn might_contain(&self, clip: &ComplexClipRegion) -> bool {
        let delta_left = clip.rect.origin.x - self.rect.origin.x;
        let delta_top = clip.rect.origin.y - self.rect.origin.y;
        let delta_right = self.rect.max_x() - clip.rect.max_x();
        let delta_bottom = self.rect.max_y() - clip.rect.max_y();

        delta_left >= 0f32 &&
        delta_top >= 0f32 &&
        delta_right >= 0f32 &&
        delta_bottom >= 0f32 &&
        clip.radii.top_left.width >= self.radii.top_left.width - delta_left &&
        clip.radii.top_left.height >= self.radii.top_left.height - delta_top &&
        clip.radii.top_right.width >= self.radii.top_right.width - delta_right &&
        clip.radii.top_right.height >= self.radii.top_right.height - delta_top &&
        clip.radii.bottom_left.width >= self.radii.bottom_left.width - delta_left &&
        clip.radii.bottom_left.height >= self.radii.bottom_left.height - delta_bottom &&
        clip.radii.bottom_right.width >= self.radii.bottom_right.width - delta_right &&
        clip.radii.bottom_right.height >= self.radii.bottom_right.height - delta_bottom
    }
}
