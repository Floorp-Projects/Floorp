/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{AlphaType, BorderRadius, BuiltDisplayList, ClipAndScrollInfo, ClipId, ClipMode};
use api::{ColorF, ColorU, DeviceIntRect, DeviceIntSize, DevicePixelScale, Epoch};
use api::{ComplexClipRegion, ExtendMode, FontRenderMode};
use api::{GlyphInstance, GlyphKey, GradientStop, ImageKey, ImageRendering, ItemRange, ItemTag};
use api::{LayerPoint, LayerRect, LayerSize, LayerToWorldTransform, LayerVector2D, LineOrientation};
use api::{LineStyle, PipelineId, PremultipliedColorF, TileOffset};
use api::{WorldToLayerTransform, YuvColorSpace, YuvFormat};
use border::{BorderCornerInstance, BorderEdgeKind};
use clip_scroll_tree::{CoordinateSystemId, ClipScrollTree};
use clip_scroll_node::ClipScrollNode;
use clip::{ClipSource, ClipSourcesHandle, ClipStore};
use frame_builder::PrimitiveContext;
use glyph_rasterizer::{FontInstance, FontTransform};
use internal_types::{FastHashMap};
use gpu_cache::{GpuBlockData, GpuCache, GpuCacheAddress, GpuCacheHandle, GpuDataRequest,
                ToGpuBlocks};
use gpu_types::{ClipChainRectIndex, ClipScrollNodeData};
use picture::{PictureKind, PicturePrimitive};
use profiler::FrameProfileCounters;
use render_task::{BlitSource, ClipChain, ClipChainNode, ClipChainNodeIter, ClipChainNodeRef, ClipWorkItem};
use render_task::{RenderTask, RenderTaskCacheKey, RenderTaskCacheKeyKind, RenderTaskId, RenderTaskTree};
use renderer::{MAX_VERTEX_TEXTURE_WIDTH};
use resource_cache::{CacheItem, ImageProperties, ResourceCache};
use scene::{ScenePipeline, SceneProperties};
use segment::SegmentBuilder;
use std::{mem, usize};
use std::rc::Rc;
use util::{MatrixHelpers, calculate_screen_bounding_rect, pack_as_float};
use util::recycle_vec;


const MIN_BRUSH_SPLIT_AREA: f32 = 128.0 * 128.0;

#[derive(Debug)]
pub struct PrimitiveRun {
    pub base_prim_index: PrimitiveIndex,
    pub count: usize,
    pub clip_and_scroll: ClipAndScrollInfo,
}

#[derive(Debug, Copy, Clone)]
pub struct PrimitiveOpacity {
    pub is_opaque: bool,
}

impl PrimitiveOpacity {
    pub fn opaque() -> PrimitiveOpacity {
        PrimitiveOpacity { is_opaque: true }
    }

    pub fn translucent() -> PrimitiveOpacity {
        PrimitiveOpacity { is_opaque: false }
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

// Represents the local space rect of a list of
// primitive runs. For most primitive runs, the
// primitive runs are attached to the parent they
// are declared in. However, when a primitive run
// is part of a 3d rendering context, it may get
// hoisted to a higher level in the picture tree.
// When this happens, we need to also calculate the
// local space rects in the original space. This
// allows constructing the true world space polygons
// for the primitive, to enable the plane splitting
// logic to work correctly.
// TODO(gw) In the future, we can probably simplify
//          this - perhaps calculate the world space
//          polygons directly and store internally
//          in the picture structure.
#[derive(Debug)]
pub struct PrimitiveRunLocalRect {
    pub local_rect_in_actual_parent_space: LayerRect,
    pub local_rect_in_original_parent_space: LayerRect,
}

/// For external images, it's not possible to know the
/// UV coords of the image (or the image data itself)
/// until the render thread receives the frame and issues
/// callbacks to the client application. For external
/// images that are visible, a DeferredResolve is created
/// that is stored in the frame. This allows the render
/// thread to iterate this list and update any changed
/// texture data and update the UV rect.
#[cfg_attr(feature = "capture", derive(Deserialize, Serialize))]
pub struct DeferredResolve {
    pub address: GpuCacheAddress,
    pub image_properties: ImageProperties,
}

#[derive(Debug, Copy, Clone, Eq, PartialEq, Hash, Ord, PartialOrd)]
pub struct SpecificPrimitiveIndex(pub usize);

#[derive(Debug, Copy, Clone, Eq, PartialEq, Hash, Ord, PartialOrd)]
#[cfg_attr(feature = "capture", derive(Deserialize, Serialize))]
pub struct PrimitiveIndex(pub usize);

#[derive(Debug, Copy, Clone, Eq, PartialEq)]
pub enum PrimitiveKind {
    TextRun,
    Image,
    YuvImage,
    Border,
    AlignedGradient,
    AngleGradient,
    RadialGradient,
    Picture,
    Brush,
}

impl GpuCacheHandle {
    pub fn as_int(&self, gpu_cache: &GpuCache) -> i32 {
        gpu_cache.get_address(self).as_int()
    }
}

impl GpuCacheAddress {
    pub fn as_int(&self) -> i32 {
        // TODO(gw): Temporarily encode GPU Cache addresses as a single int.
        //           In the future, we can change the PrimitiveInstance struct
        //           to use 2x u16 for the vertex attribute instead of an i32.
        self.v as i32 * MAX_VERTEX_TEXTURE_WIDTH as i32 + self.u as i32
    }
}

// TODO(gw): Pack the fields here better!
#[derive(Debug)]
pub struct PrimitiveMetadata {
    pub opacity: PrimitiveOpacity,
    pub clip_sources: ClipSourcesHandle,
    pub prim_kind: PrimitiveKind,
    pub cpu_prim_index: SpecificPrimitiveIndex,
    pub gpu_location: GpuCacheHandle,
    pub clip_task_id: Option<RenderTaskId>,

    // TODO(gw): In the future, we should just pull these
    //           directly from the DL item, instead of
    //           storing them here.
    pub local_rect: LayerRect,
    pub local_clip_rect: LayerRect,
    pub clip_chain_rect_index: ClipChainRectIndex,
    pub is_backface_visible: bool,
    pub screen_rect: Option<DeviceIntRect>,

    /// A tag used to identify this primitive outside of WebRender. This is
    /// used for returning useful data during hit testing.
    pub tag: Option<ItemTag>,
}

#[derive(Debug)]
pub enum BrushMaskKind {
    //Rect,         // TODO(gw): Optimization opportunity for masks with 0 border radii.
    Corner(LayerSize),
    RoundedRect(LayerRect, BorderRadius),
}

#[derive(Debug)]
pub enum BrushKind {
    Mask {
        clip_mode: ClipMode,
        kind: BrushMaskKind,
    },
    Solid {
        color: ColorF,
    },
    Clear,
    Line {
        color: PremultipliedColorF,
        wavy_line_thickness: f32,
        style: LineStyle,
        orientation: LineOrientation,
    },
    Picture,
}

impl BrushKind {
    fn supports_segments(&self) -> bool {
        match *self {
            BrushKind::Solid { .. } |
            BrushKind::Picture => true,
            _ => false,
        }
    }
}

bitflags! {
    /// Each bit of the edge AA mask is:
    /// 0, when the edge of the primitive needs to be considered for AA
    /// 1, when the edge of the segment needs to be considered for AA
    ///
    /// *Note*: the bit values have to match the shader logic in
    /// `write_transform_vertex()` function.
    pub struct EdgeAaSegmentMask: u8 {
        const LEFT = 0x1;
        const TOP = 0x2;
        const RIGHT = 0x4;
        const BOTTOM = 0x8;
    }
}

#[derive(Debug)]
pub struct BrushSegment {
    pub local_rect: LayerRect,
    pub clip_task_id: Option<RenderTaskId>,
    pub may_need_clip_mask: bool,
    pub edge_flags: EdgeAaSegmentMask,
}

impl BrushSegment {
    pub fn new(
        origin: LayerPoint,
        size: LayerSize,
        may_need_clip_mask: bool,
        edge_flags: EdgeAaSegmentMask,
    ) -> BrushSegment {
        BrushSegment {
            local_rect: LayerRect::new(origin, size),
            clip_task_id: None,
            may_need_clip_mask,
            edge_flags,
        }
    }
}

#[derive(Copy, Clone, Debug, PartialEq)]
pub enum BrushClipMaskKind {
    Unknown,
    Individual,
    Global,
}

#[derive(Debug)]
pub struct BrushSegmentDescriptor {
    pub segments: Vec<BrushSegment>,
    pub clip_mask_kind: BrushClipMaskKind,
}

#[derive(Debug)]
pub struct BrushPrimitive {
    pub kind: BrushKind,
    pub segment_desc: Option<BrushSegmentDescriptor>,
}

impl BrushPrimitive {
    pub fn new(
        kind: BrushKind,
        segment_desc: Option<BrushSegmentDescriptor>,
    ) -> BrushPrimitive {
        BrushPrimitive {
            kind,
            segment_desc,
        }
    }

    fn write_gpu_blocks(&self, request: &mut GpuDataRequest) {
        // has to match VECS_PER_SPECIFIC_BRUSH
        match self.kind {
            BrushKind::Picture => {
            }
            BrushKind::Solid { color } => {
                request.push(color.premultiplied());
            }
            BrushKind::Clear => {
                // Opaque black with operator dest out
                request.push(PremultipliedColorF::BLACK);
            }
            BrushKind::Mask { clip_mode, kind: BrushMaskKind::Corner(radius) } => {
                request.push([
                    radius.width,
                    radius.height,
                    clip_mode as u32 as f32,
                    0.0,
                ]);
            }
            BrushKind::Mask { clip_mode, kind: BrushMaskKind::RoundedRect(rect, radii) } => {
                request.push([
                    clip_mode as u32 as f32,
                    0.0,
                    0.0,
                    0.0
                ]);
                request.push(rect);
                request.push([
                    radii.top_left.width,
                    radii.top_left.height,
                    radii.top_right.width,
                    radii.top_right.height,
                ]);
                request.push([
                    radii.bottom_right.width,
                    radii.bottom_right.height,
                    radii.bottom_left.width,
                    radii.bottom_left.height,
                ]);
            }
            BrushKind::Line { color, wavy_line_thickness, style, orientation } => {
                request.push(color);
                request.push([
                    wavy_line_thickness,
                    pack_as_float(style as u32),
                    pack_as_float(orientation as u32),
                    0.0,
                ]);
            }
        }
    }
}

// Key that identifies a unique (partial) image that is being
// stored in the render task cache.
#[derive(Debug, Copy, Clone, Eq, Hash, PartialEq)]
#[cfg_attr(feature = "capture", derive(Deserialize, Serialize))]
pub struct ImageCacheKey {
    // TODO(gw): Consider introducing a struct that collectively
    //           identifies an image in the resource cache
    //           uniquely. We pass this around to a few places.
    pub image_key: ImageKey,
    pub image_rendering: ImageRendering,
    pub tile_offset: Option<TileOffset>,
    pub texel_rect: Option<DeviceIntRect>,
}

// Where to find the texture data for an image primitive.
#[derive(Debug)]
pub enum ImageSource {
    // A normal image - just reference the texture cache.
    Default,
    // An image that is pre-rendered into the texture cache
    // via a render task.
    Cache {
        size: DeviceIntSize,
        item: CacheItem,
    },
}

#[derive(Debug)]
pub struct ImagePrimitiveCpu {
    pub tile_spacing: LayerSize,
    pub alpha_type: AlphaType,
    pub stretch_size: LayerSize,
    pub current_epoch: Epoch,
    pub source: ImageSource,
    pub key: ImageCacheKey,
}

impl ToGpuBlocks for ImagePrimitiveCpu {
    fn write_gpu_blocks(&self, mut request: GpuDataRequest) {
        request.push([
            self.stretch_size.width, self.stretch_size.height,
            self.tile_spacing.width, self.tile_spacing.height,
        ]);
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

#[derive(Debug)]
pub struct BorderPrimitiveCpu {
    pub corner_instances: [BorderCornerInstance; 4],
    pub edges: [BorderEdgeKind; 4],
    pub gpu_blocks: [GpuBlockData; 8],
}

impl ToGpuBlocks for BorderPrimitiveCpu {
    fn write_gpu_blocks(&self, mut request: GpuDataRequest) {
        request.extend_from_slice(&self.gpu_blocks);
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
    fn build_gpu_blocks_for_aligned(
        &self,
        display_list: &BuiltDisplayList,
        mut request: GpuDataRequest,
    ) -> PrimitiveOpacity {
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

    fn build_gpu_blocks_for_angle_radial(
        &self,
        display_list: &BuiltDisplayList,
        mut request: GpuDataRequest,
    ) {
        request.extend_from_slice(&self.gpu_blocks);

        let gradient_builder = GradientGpuBlockBuilder::new(self.stops_range, display_list);
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

#[derive(Debug)]
#[repr(C)]
// An entry in a gradient data table representing a segment of the gradient color space.
pub struct GradientDataEntry {
    pub start_color: PremultipliedColorF,
    pub end_color: PremultipliedColorF,
}

struct GradientGpuBlockBuilder<'a> {
    stops_range: ItemRange<GradientStop>,
    display_list: &'a BuiltDisplayList,
}

impl<'a> GradientGpuBlockBuilder<'a> {
    fn new(
        stops_range: ItemRange<GradientStop>,
        display_list: &'a BuiltDisplayList,
    ) -> Self {
        GradientGpuBlockBuilder {
            stops_range,
            display_list,
        }
    }

    /// Generate a color ramp filling the indices in [start_idx, end_idx) and interpolating
    /// from start_color to end_color.
    fn fill_colors(
        &self,
        start_idx: usize,
        end_idx: usize,
        start_color: &PremultipliedColorF,
        end_color: &PremultipliedColorF,
        entries: &mut [GradientDataEntry; GRADIENT_DATA_SIZE],
    ) {
        // Calculate the color difference for individual steps in the ramp.
        let inv_steps = 1.0 / (end_idx - start_idx) as f32;
        let step_r = (end_color.r - start_color.r) * inv_steps;
        let step_g = (end_color.g - start_color.g) * inv_steps;
        let step_b = (end_color.b - start_color.b) * inv_steps;
        let step_a = (end_color.a - start_color.a) * inv_steps;

        let mut cur_color = *start_color;

        // Walk the ramp writing start and end colors for each entry.
        for index in start_idx .. end_idx {
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
        (offset.max(0.0).min(1.0) * GRADIENT_DATA_TABLE_SIZE as f32 +
            GRADIENT_DATA_TABLE_BEGIN as f32)
            .round() as usize
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
            self.fill_colors(
                GRADIENT_DATA_LAST_STOP,
                GRADIENT_DATA_LAST_STOP + 1,
                &cur_color,
                &cur_color,
                &mut entries,
            );

            // Fill in the center of the gradient table, generating a color ramp between each consecutive pair
            // of gradient stops. Each iteration of a loop will fill the indices in [next_idx, cur_idx). The
            // loop will then fill indices in [GRADIENT_DATA_TABLE_BEGIN, GRADIENT_DATA_TABLE_END).
            let mut cur_idx = GRADIENT_DATA_TABLE_END;
            for next in src_stops {
                let next_color = next.color.premultiplied();
                let next_idx = Self::get_index(1.0 - next.offset);

                if next_idx < cur_idx {
                    self.fill_colors(next_idx, cur_idx, &next_color, &cur_color, &mut entries);
                    cur_idx = next_idx;
                }

                cur_color = next_color;
            }
            debug_assert_eq!(cur_idx, GRADIENT_DATA_TABLE_BEGIN);

            // Fill in the last entry (for reversed stops) with the last color stop
            self.fill_colors(
                GRADIENT_DATA_FIRST_STOP,
                GRADIENT_DATA_FIRST_STOP + 1,
                &cur_color,
                &cur_color,
                &mut entries,
            );
        } else {
            // Fill in the first entry with the first color stop
            self.fill_colors(
                GRADIENT_DATA_FIRST_STOP,
                GRADIENT_DATA_FIRST_STOP + 1,
                &cur_color,
                &cur_color,
                &mut entries,
            );

            // Fill in the center of the gradient table, generating a color ramp between each consecutive pair
            // of gradient stops. Each iteration of a loop will fill the indices in [cur_idx, next_idx). The
            // loop will then fill indices in [GRADIENT_DATA_TABLE_BEGIN, GRADIENT_DATA_TABLE_END).
            let mut cur_idx = GRADIENT_DATA_TABLE_BEGIN;
            for next in src_stops {
                let next_color = next.color.premultiplied();
                let next_idx = Self::get_index(next.offset);

                if next_idx > cur_idx {
                    self.fill_colors(cur_idx, next_idx, &cur_color, &next_color, &mut entries);
                    cur_idx = next_idx;
                }

                cur_color = next_color;
            }
            debug_assert_eq!(cur_idx, GRADIENT_DATA_TABLE_END);

            // Fill in the last entry with the last color stop
            self.fill_colors(
                GRADIENT_DATA_LAST_STOP,
                GRADIENT_DATA_LAST_STOP + 1,
                &cur_color,
                &cur_color,
                &mut entries,
            );
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
    fn build_gpu_blocks_for_angle_radial(
        &self,
        display_list: &BuiltDisplayList,
        mut request: GpuDataRequest,
    ) {
        request.extend_from_slice(&self.gpu_blocks);

        let gradient_builder = GradientGpuBlockBuilder::new(self.stops_range, display_list);
        gradient_builder.build(false, &mut request);
    }
}

#[derive(Debug, Clone)]
pub struct TextRunPrimitiveCpu {
    pub font: FontInstance,
    pub offset: LayerVector2D,
    pub glyph_range: ItemRange<GlyphInstance>,
    pub glyph_count: usize,
    pub glyph_keys: Vec<GlyphKey>,
    pub glyph_gpu_blocks: Vec<GpuBlockData>,
    pub shadow_color: ColorU,
}

impl TextRunPrimitiveCpu {
    pub fn get_font(
        &self,
        device_pixel_scale: DevicePixelScale,
        transform: Option<&LayerToWorldTransform>,
    ) -> FontInstance {
        let mut font = self.font.clone();
        font.size = font.size.scale_by(device_pixel_scale.0);
        if let Some(transform) = transform {
            if transform.has_perspective_component() || !transform.has_2d_inverse() {
                font.render_mode = font.render_mode.limit_by(FontRenderMode::Alpha);
            } else {
                font.transform = FontTransform::from(transform).quantize();
            }
        }
        font
    }

    pub fn is_shadow(&self) -> bool {
        self.shadow_color.a != 0
    }

    pub fn get_color(&self) -> ColorF {
        ColorF::from(if self.is_shadow() {
            self.shadow_color
        } else {
            self.font.color
        })
    }

    fn prepare_for_render(
        &mut self,
        resource_cache: &mut ResourceCache,
        device_pixel_scale: DevicePixelScale,
        transform: Option<&LayerToWorldTransform>,
        display_list: &BuiltDisplayList,
        gpu_cache: &mut GpuCache,
    ) {
        let font = self.get_font(device_pixel_scale, transform);

        // Cache the glyph positions, if not in the cache already.
        // TODO(gw): In the future, remove `glyph_instances`
        //           completely, and just reference the glyphs
        //           directly from the display list.
        if self.glyph_keys.is_empty() {
            let subpx_dir = font.subpx_dir.limit_by(font.render_mode);
            let src_glyphs = display_list.get(self.glyph_range);

            // TODO(gw): If we support chunks() on AuxIter
            //           in the future, this code below could
            //           be much simpler...
            let mut gpu_block = [0.0; 4];
            for (i, src) in src_glyphs.enumerate() {
                let key = GlyphKey::new(src.index, src.point, font.render_mode, subpx_dir);
                self.glyph_keys.push(key);

                // Two glyphs are packed per GPU block.

                if (i & 1) == 0 {
                    gpu_block[0] = src.point.x;
                    gpu_block[1] = src.point.y;
                } else {
                    gpu_block[2] = src.point.x;
                    gpu_block[3] = src.point.y;
                    self.glyph_gpu_blocks.push(gpu_block.into());
                }
            }

            // Ensure the last block is added in the case
            // of an odd number of glyphs.
            if (self.glyph_keys.len() & 1) != 0 {
                self.glyph_gpu_blocks.push(gpu_block.into());
            }
        }

        resource_cache.request_glyphs(font, &self.glyph_keys, gpu_cache);
    }

    fn write_gpu_blocks(&self, request: &mut GpuDataRequest) {
        request.push(self.get_color().premultiplied());
        // this is the only case where we need to provide plain color to GPU
        let bg_color = ColorF::from(self.font.bg_color);
        request.push([bg_color.r, bg_color.g, bg_color.b, 1.0]);
        request.push([
            self.offset.x,
            self.offset.y,
            0.0,
            0.0,
        ]);
        request.extend_from_slice(&self.glyph_gpu_blocks);

        assert!(request.current_used_block_num() <= MAX_VERTEX_TEXTURE_WIDTH);
    }
}

#[derive(Debug)]
#[repr(C)]
struct ClipRect {
    rect: LayerRect,
    mode: f32,
}

#[derive(Debug)]
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
        request.push([
            self.outer_radius_x,
            self.outer_radius_y,
            self.inner_radius_x,
            self.inner_radius_y,
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

#[derive(Debug)]
#[repr(C)]
pub struct ImageMaskData {
    pub local_rect: LayerRect,
}

impl ToGpuBlocks for ImageMaskData {
    fn write_gpu_blocks(&self, mut request: GpuDataRequest) {
        request.push(self.local_rect);
    }
}

#[derive(Debug)]
pub struct ClipData {
    rect: ClipRect,
    top_left: ClipCorner,
    top_right: ClipCorner,
    bottom_left: ClipCorner,
    bottom_right: ClipCorner,
}

impl ClipData {
    pub fn rounded_rect(rect: &LayerRect, radii: &BorderRadius, mode: ClipMode) -> ClipData {
        ClipData {
            rect: ClipRect {
                rect: *rect,
                mode: mode as u32 as f32,
            },
            top_left: ClipCorner {
                rect: LayerRect::new(
                    LayerPoint::new(rect.origin.x, rect.origin.y),
                    LayerSize::new(radii.top_left.width, radii.top_left.height),
                ),
                outer_radius_x: radii.top_left.width,
                outer_radius_y: radii.top_left.height,
                inner_radius_x: 0.0,
                inner_radius_y: 0.0,
            },
            top_right: ClipCorner {
                rect: LayerRect::new(
                    LayerPoint::new(
                        rect.origin.x + rect.size.width - radii.top_right.width,
                        rect.origin.y,
                    ),
                    LayerSize::new(radii.top_right.width, radii.top_right.height),
                ),
                outer_radius_x: radii.top_right.width,
                outer_radius_y: radii.top_right.height,
                inner_radius_x: 0.0,
                inner_radius_y: 0.0,
            },
            bottom_left: ClipCorner {
                rect: LayerRect::new(
                    LayerPoint::new(
                        rect.origin.x,
                        rect.origin.y + rect.size.height - radii.bottom_left.height,
                    ),
                    LayerSize::new(radii.bottom_left.width, radii.bottom_left.height),
                ),
                outer_radius_x: radii.bottom_left.width,
                outer_radius_y: radii.bottom_left.height,
                inner_radius_x: 0.0,
                inner_radius_y: 0.0,
            },
            bottom_right: ClipCorner {
                rect: LayerRect::new(
                    LayerPoint::new(
                        rect.origin.x + rect.size.width - radii.bottom_right.width,
                        rect.origin.y + rect.size.height - radii.bottom_right.height,
                    ),
                    LayerSize::new(radii.bottom_right.width, radii.bottom_right.height),
                ),
                outer_radius_x: radii.bottom_right.width,
                outer_radius_y: radii.bottom_right.height,
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
                    LayerSize::new(radius, radius),
                ),
                radius,
                0.0,
            ),
            top_right: ClipCorner::uniform(
                LayerRect::new(
                    LayerPoint::new(rect.origin.x + rect.size.width - radius, rect.origin.y),
                    LayerSize::new(radius, radius),
                ),
                radius,
                0.0,
            ),
            bottom_left: ClipCorner::uniform(
                LayerRect::new(
                    LayerPoint::new(rect.origin.x, rect.origin.y + rect.size.height - radius),
                    LayerSize::new(radius, radius),
                ),
                radius,
                0.0,
            ),
            bottom_right: ClipCorner::uniform(
                LayerRect::new(
                    LayerPoint::new(
                        rect.origin.x + rect.size.width - radius,
                        rect.origin.y + rect.size.height - radius,
                    ),
                    LayerSize::new(radius, radius),
                ),
                radius,
                0.0,
            ),
        }
    }

    pub fn write(&self, request: &mut GpuDataRequest) {
        request.push(self.rect.rect);
        request.push([self.rect.mode, 0.0, 0.0, 0.0]);
        for corner in &[
            &self.top_left,
            &self.top_right,
            &self.bottom_left,
            &self.bottom_right,
        ] {
            corner.write(request);
        }
    }
}

#[derive(Debug)]
pub enum PrimitiveContainer {
    TextRun(TextRunPrimitiveCpu),
    Image(ImagePrimitiveCpu),
    YuvImage(YuvImagePrimitiveCpu),
    Border(BorderPrimitiveCpu),
    AlignedGradient(GradientPrimitiveCpu),
    AngleGradient(GradientPrimitiveCpu),
    RadialGradient(RadialGradientPrimitiveCpu),
    Picture(PicturePrimitive),
    Brush(BrushPrimitive),
}

pub struct PrimitiveStore {
    /// CPU side information only.
    pub cpu_brushes: Vec<BrushPrimitive>,
    pub cpu_text_runs: Vec<TextRunPrimitiveCpu>,
    pub cpu_pictures: Vec<PicturePrimitive>,
    pub cpu_images: Vec<ImagePrimitiveCpu>,
    pub cpu_yuv_images: Vec<YuvImagePrimitiveCpu>,
    pub cpu_gradients: Vec<GradientPrimitiveCpu>,
    pub cpu_radial_gradients: Vec<RadialGradientPrimitiveCpu>,
    pub cpu_metadata: Vec<PrimitiveMetadata>,
    pub cpu_borders: Vec<BorderPrimitiveCpu>,
}

impl PrimitiveStore {
    pub fn new() -> PrimitiveStore {
        PrimitiveStore {
            cpu_metadata: Vec::new(),
            cpu_brushes: Vec::new(),
            cpu_text_runs: Vec::new(),
            cpu_pictures: Vec::new(),
            cpu_images: Vec::new(),
            cpu_yuv_images: Vec::new(),
            cpu_gradients: Vec::new(),
            cpu_radial_gradients: Vec::new(),
            cpu_borders: Vec::new(),
        }
    }

    pub fn recycle(self) -> Self {
        PrimitiveStore {
            cpu_metadata: recycle_vec(self.cpu_metadata),
            cpu_brushes: recycle_vec(self.cpu_brushes),
            cpu_text_runs: recycle_vec(self.cpu_text_runs),
            cpu_pictures: recycle_vec(self.cpu_pictures),
            cpu_images: recycle_vec(self.cpu_images),
            cpu_yuv_images: recycle_vec(self.cpu_yuv_images),
            cpu_gradients: recycle_vec(self.cpu_gradients),
            cpu_radial_gradients: recycle_vec(self.cpu_radial_gradients),
            cpu_borders: recycle_vec(self.cpu_borders),
        }
    }

    pub fn add_primitive(
        &mut self,
        local_rect: &LayerRect,
        local_clip_rect: &LayerRect,
        is_backface_visible: bool,
        clip_sources: ClipSourcesHandle,
        tag: Option<ItemTag>,
        container: PrimitiveContainer,
    ) -> PrimitiveIndex {
        let prim_index = self.cpu_metadata.len();

        let base_metadata = PrimitiveMetadata {
            clip_sources,
            gpu_location: GpuCacheHandle::new(),
            clip_task_id: None,
            local_rect: *local_rect,
            local_clip_rect: *local_clip_rect,
            clip_chain_rect_index: ClipChainRectIndex(0),
            is_backface_visible: is_backface_visible,
            screen_rect: None,
            tag,
            opacity: PrimitiveOpacity::translucent(),
            prim_kind: PrimitiveKind::Brush,
            cpu_prim_index: SpecificPrimitiveIndex(0),
        };

        let metadata = match container {
            PrimitiveContainer::Brush(brush) => {
                let opacity = match brush.kind {
                    BrushKind::Clear => PrimitiveOpacity::translucent(),
                    BrushKind::Solid { ref color } => PrimitiveOpacity::from_alpha(color.a),
                    BrushKind::Mask { .. } => PrimitiveOpacity::translucent(),
                    BrushKind::Line { .. } => PrimitiveOpacity::translucent(),
                    BrushKind::Picture => {
                        // TODO(gw): This is not currently used. In the future
                        //           we should detect opaque pictures.
                        unreachable!();
                    }
                };

                let metadata = PrimitiveMetadata {
                    opacity,
                    prim_kind: PrimitiveKind::Brush,
                    cpu_prim_index: SpecificPrimitiveIndex(self.cpu_brushes.len()),
                    ..base_metadata
                };

                self.cpu_brushes.push(brush);

                metadata
            }
            PrimitiveContainer::TextRun(text_cpu) => {
                let metadata = PrimitiveMetadata {
                    opacity: PrimitiveOpacity::translucent(),
                    prim_kind: PrimitiveKind::TextRun,
                    cpu_prim_index: SpecificPrimitiveIndex(self.cpu_text_runs.len()),
                    ..base_metadata
                };

                self.cpu_text_runs.push(text_cpu);
                metadata
            }
            PrimitiveContainer::Picture(picture) => {
                let metadata = PrimitiveMetadata {
                    opacity: PrimitiveOpacity::translucent(),
                    prim_kind: PrimitiveKind::Picture,
                    cpu_prim_index: SpecificPrimitiveIndex(self.cpu_pictures.len()),
                    ..base_metadata
                };

                self.cpu_pictures.push(picture);
                metadata
            }
            PrimitiveContainer::Image(image_cpu) => {
                let metadata = PrimitiveMetadata {
                    opacity: PrimitiveOpacity::translucent(),
                    prim_kind: PrimitiveKind::Image,
                    cpu_prim_index: SpecificPrimitiveIndex(self.cpu_images.len()),
                    ..base_metadata
                };

                self.cpu_images.push(image_cpu);
                metadata
            }
            PrimitiveContainer::YuvImage(image_cpu) => {
                let metadata = PrimitiveMetadata {
                    opacity: PrimitiveOpacity::opaque(),
                    prim_kind: PrimitiveKind::YuvImage,
                    cpu_prim_index: SpecificPrimitiveIndex(self.cpu_yuv_images.len()),
                    ..base_metadata
                };

                self.cpu_yuv_images.push(image_cpu);
                metadata
            }
            PrimitiveContainer::Border(border_cpu) => {
                let metadata = PrimitiveMetadata {
                    opacity: PrimitiveOpacity::translucent(),
                    prim_kind: PrimitiveKind::Border,
                    cpu_prim_index: SpecificPrimitiveIndex(self.cpu_borders.len()),
                    ..base_metadata
                };

                self.cpu_borders.push(border_cpu);
                metadata
            }
            PrimitiveContainer::AlignedGradient(gradient_cpu) => {
                let metadata = PrimitiveMetadata {
                    opacity: PrimitiveOpacity::translucent(),
                    prim_kind: PrimitiveKind::AlignedGradient,
                    cpu_prim_index: SpecificPrimitiveIndex(self.cpu_gradients.len()),
                    ..base_metadata
                };

                self.cpu_gradients.push(gradient_cpu);
                metadata
            }
            PrimitiveContainer::AngleGradient(gradient_cpu) => {
                let metadata = PrimitiveMetadata {
                    // TODO: calculate if the gradient is actually opaque
                    opacity: PrimitiveOpacity::translucent(),
                    prim_kind: PrimitiveKind::AngleGradient,
                    cpu_prim_index: SpecificPrimitiveIndex(self.cpu_gradients.len()),
                    ..base_metadata
                };

                self.cpu_gradients.push(gradient_cpu);
                metadata
            }
            PrimitiveContainer::RadialGradient(radial_gradient_cpu) => {
                let metadata = PrimitiveMetadata {
                    // TODO: calculate if the gradient is actually opaque
                    opacity: PrimitiveOpacity::translucent(),
                    prim_kind: PrimitiveKind::RadialGradient,
                    cpu_prim_index: SpecificPrimitiveIndex(self.cpu_radial_gradients.len()),
                    ..base_metadata
                };

                self.cpu_radial_gradients.push(radial_gradient_cpu);
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

    fn prepare_prim_for_render_inner(
        &mut self,
        prim_index: PrimitiveIndex,
        prim_context: &PrimitiveContext,
        resource_cache: &mut ResourceCache,
        gpu_cache: &mut GpuCache,
        render_tasks: &mut RenderTaskTree,
        child_tasks: Vec<RenderTaskId>,
        parent_tasks: &mut Vec<RenderTaskId>,
        pic_index: SpecificPrimitiveIndex,
    ) {
        let metadata = &mut self.cpu_metadata[prim_index.0];
        match metadata.prim_kind {
            PrimitiveKind::Border => {}
            PrimitiveKind::Picture => {
                self.cpu_pictures[metadata.cpu_prim_index.0]
                    .prepare_for_render(
                        prim_index,
                        prim_context,
                        render_tasks,
                        metadata.screen_rect.as_ref().expect("bug: trying to draw an off-screen picture!?"),
                        &metadata.local_rect,
                        child_tasks,
                        parent_tasks,
                        resource_cache,
                        gpu_cache,
                    );
            }
            PrimitiveKind::TextRun => {
                let pic = &self.cpu_pictures[pic_index.0];
                let text = &mut self.cpu_text_runs[metadata.cpu_prim_index.0];
                // The transform only makes sense for screen space rasterization
                let transform = match pic.kind {
                    PictureKind::BoxShadow { .. } => None,
                    PictureKind::TextShadow { .. } => None,
                    PictureKind::Image { .. } => {
                        Some(&prim_context.scroll_node.world_content_transform)
                    },
                };
                text.prepare_for_render(
                    resource_cache,
                    prim_context.device_pixel_scale,
                    transform,
                    prim_context.display_list,
                    gpu_cache,
                );
            }
            PrimitiveKind::Image => {
                let image_cpu = &mut self.cpu_images[metadata.cpu_prim_index.0];
                let image_properties = resource_cache.get_image_properties(image_cpu.key.image_key);

                // TODO(gw): Add image.rs and move this code out to a separate
                //           source file as it gets more complicated, and we
                //           start pre-rendering images for other reasons.

                if let Some(image_properties) = image_properties {
                    // See if this image has been updated since we last hit this code path.
                    // If so, we need to (at least) update the opacity, and also rebuild
                    // and render task cached portions of this image.
                    if image_properties.epoch != image_cpu.current_epoch {
                        image_cpu.current_epoch = image_properties.epoch;

                        // Update the opacity.
                        metadata.opacity.is_opaque = image_properties.descriptor.is_opaque &&
                            image_cpu.tile_spacing.width == 0.0 &&
                            image_cpu.tile_spacing.height == 0.0;

                        // Work out whether this image is a normal / simple type, or if
                        // we need to pre-render it to the render task cache.
                        image_cpu.source = match image_cpu.key.texel_rect {
                            Some(texel_rect) => {
                                ImageSource::Cache {
                                    // Size in device-pixels we need to allocate in render task cache.
                                    size: texel_rect.size,
                                    item: CacheItem::invalid(),
                                }
                            }
                            None => {
                                // Simple image - just use a normal texture cache entry.
                                ImageSource::Default
                            }
                        };
                    }

                    // TODO(gw): Don't actually need this in cached source mode if
                    //           the cache item is still valid...
                    resource_cache.request_image(
                        image_cpu.key.image_key,
                        image_cpu.key.image_rendering,
                        image_cpu.key.tile_offset,
                        gpu_cache,
                    );

                    // Every frame, for cached items, we need to request the render
                    // task cache item. The closure will be invoked on the first
                    // time through, and any time the render task output has been
                    // evicted from the texture cache.
                    if let ImageSource::Cache { size, ref mut item } = image_cpu.source {
                        let key = image_cpu.key;

                        // Request a pre-rendered image task.
                        *item = resource_cache.request_render_task(
                            RenderTaskCacheKey {
                                size,
                                kind: RenderTaskCacheKeyKind::Image(key),
                            },
                            gpu_cache,
                            render_tasks,
                            |render_tasks| {
                                // Create a task to blit from the texture cache to
                                // a normal transient render task surface. This will
                                // copy only the sub-rect, if specified.
                                let cache_to_target_task = RenderTask::new_blit(
                                    size,
                                    BlitSource::Image {
                                        key,
                                    },
                                );
                                let cache_to_target_task_id = render_tasks.add(cache_to_target_task);

                                // Create a task to blit the rect from the child render
                                // task above back into the right spot in the persistent
                                // render target cache.
                                let target_to_cache_task = RenderTask::new_blit(
                                    size,
                                    BlitSource::RenderTask {
                                        task_id: cache_to_target_task_id,
                                    },
                                );
                                let target_to_cache_task_id = render_tasks.add(target_to_cache_task);

                                // Hook this into the render task tree at the right spot.
                                parent_tasks.push(target_to_cache_task_id);

                                // Pass the image opacity, so that the cached render task
                                // item inherits the same opacity properties.
                                (target_to_cache_task_id, [0.0; 3], image_properties.descriptor.is_opaque)
                            }
                        );
                    }
                }
            }
            PrimitiveKind::YuvImage => {
                let image_cpu = &mut self.cpu_yuv_images[metadata.cpu_prim_index.0];

                let channel_num = image_cpu.format.get_plane_num();
                debug_assert!(channel_num <= 3);
                for channel in 0 .. channel_num {
                    resource_cache.request_image(
                        image_cpu.yuv_key[channel],
                        image_cpu.image_rendering,
                        None,
                        gpu_cache,
                    );
                }
            }
            PrimitiveKind::Brush |
            PrimitiveKind::AlignedGradient |
            PrimitiveKind::AngleGradient |
            PrimitiveKind::RadialGradient => {}
        }

        // Mark this GPU resource as required for this frame.
        if let Some(mut request) = gpu_cache.request(&mut metadata.gpu_location) {
            // has to match VECS_PER_BRUSH_PRIM
            request.push(metadata.local_rect);
            request.push(metadata.local_clip_rect);

            match metadata.prim_kind {
                PrimitiveKind::Border => {
                    let border = &self.cpu_borders[metadata.cpu_prim_index.0];
                    border.write_gpu_blocks(request);
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
                    metadata.opacity = gradient.build_gpu_blocks_for_aligned(prim_context.display_list, request);
                }
                PrimitiveKind::AngleGradient => {
                    let gradient = &self.cpu_gradients[metadata.cpu_prim_index.0];
                    gradient.build_gpu_blocks_for_angle_radial(prim_context.display_list, request);
                }
                PrimitiveKind::RadialGradient => {
                    let gradient = &self.cpu_radial_gradients[metadata.cpu_prim_index.0];
                    gradient.build_gpu_blocks_for_angle_radial(prim_context.display_list, request);
                }
                PrimitiveKind::TextRun => {
                    let text = &self.cpu_text_runs[metadata.cpu_prim_index.0];
                    text.write_gpu_blocks(&mut request);
                }
                PrimitiveKind::Picture => {
                    let pic = &self.cpu_pictures[metadata.cpu_prim_index.0];
                    pic.write_gpu_blocks(&mut request);

                    let brush = &pic.brush;
                    brush.write_gpu_blocks(&mut request);
                    match brush.segment_desc {
                        Some(ref segment_desc) => {
                            for segment in &segment_desc.segments {
                                // has to match VECS_PER_SEGMENT
                                request.write_segment(segment.local_rect);
                            }
                        }
                        None => {
                            request.write_segment(metadata.local_rect);
                        }
                    }
                }
                PrimitiveKind::Brush => {
                    let brush = &self.cpu_brushes[metadata.cpu_prim_index.0];
                    brush.write_gpu_blocks(&mut request);
                    match brush.segment_desc {
                        Some(ref segment_desc) => {
                            for segment in &segment_desc.segments {
                                // has to match VECS_PER_SEGMENT
                                request.write_segment(segment.local_rect);
                            }
                        }
                        None => {
                            request.write_segment(metadata.local_rect);
                        }
                    }
                }
            }
        }
    }

    fn write_brush_segment_description(
        brush: &mut BrushPrimitive,
        metadata: &PrimitiveMetadata,
        prim_context: &PrimitiveContext,
        clip_store: &mut ClipStore,
        node_data: &[ClipScrollNodeData],
        clips: &Vec<ClipWorkItem>,
        has_clips_from_other_coordinate_systems: bool,
    ) {
        match brush.segment_desc {
            Some(ref segment_desc) => {
                // If we already have a segment descriptor, only run through the
                // clips list if we haven't already determined the mask kind.
                if segment_desc.clip_mask_kind != BrushClipMaskKind::Unknown {
                    return;
                }
            }
            None => {
                // If no segment descriptor built yet, see if it is a brush
                // type that wants to be segmented.
                if !brush.kind.supports_segments() {
                    return;
                }
                if metadata.local_rect.size.area() <= MIN_BRUSH_SPLIT_AREA {
                    return;
                }
            }
        }

        let mut segment_builder = SegmentBuilder::new(
            metadata.local_rect,
            None,
            metadata.local_clip_rect
        );

        // If this primitive is clipped by clips from a different coordinate system, then we
        // need to apply a clip mask for the entire primitive.
        let mut clip_mask_kind = match has_clips_from_other_coordinate_systems {
            true => BrushClipMaskKind::Global,
            false => BrushClipMaskKind::Individual,
        };

        // Segment the primitive on all the local-space clip sources that we can.
        for clip_item in clips {
            if clip_item.coordinate_system_id != prim_context.scroll_node.coordinate_system_id {
                continue;
            }

            let local_clips = clip_store.get_opt(&clip_item.clip_sources).expect("bug");
            for &(ref clip, _) in &local_clips.clips {
                let (local_clip_rect, radius, mode) = match *clip {
                    ClipSource::RoundedRectangle(rect, radii, clip_mode) => {
                        (rect, Some(radii), clip_mode)
                    }
                    ClipSource::Rectangle(rect) => {
                        (rect, None, ClipMode::Clip)
                    }
                    ClipSource::BorderCorner(..) |
                    ClipSource::Image(..) => {
                        // TODO(gw): We can easily extend the segment builder
                        //           to support these clip sources in the
                        //           future, but they are rarely used.
                        clip_mask_kind = BrushClipMaskKind::Global;
                        continue;
                    }
                };

                // If the scroll node transforms are different between the clip
                // node and the primitive, we need to get the clip rect in the
                // local space of the primitive, in order to generate correct
                // local segments.
                let local_clip_rect = if clip_item.scroll_node_data_index == prim_context.scroll_node.node_data_index {
                    local_clip_rect
                } else {
                    let clip_transform_data = &node_data[clip_item.scroll_node_data_index.0 as usize];
                    let prim_transform = &prim_context.scroll_node.world_content_transform;

                    let relative_transform = prim_transform
                        .inverse()
                        .unwrap_or(WorldToLayerTransform::identity())
                        .pre_mul(&clip_transform_data.transform);

                    relative_transform.transform_rect(&local_clip_rect)
                };

                segment_builder.push_rect(local_clip_rect, radius, mode);
            }
        }

        match brush.segment_desc {
            Some(ref mut segment_desc) => {
                segment_desc.clip_mask_kind = clip_mask_kind;
            }
            None => {
                // TODO(gw): We can probably make the allocation
                //           patterns of this and the segment
                //           builder significantly better, by
                //           retaining it across primitives.
                let mut segments = Vec::new();

                segment_builder.build(|segment| {
                    segments.push(
                        BrushSegment::new(
                            segment.rect.origin,
                            segment.rect.size,
                            segment.has_mask,
                            segment.edge_flags,
                        ),
                    );
                });

                brush.segment_desc = Some(BrushSegmentDescriptor {
                    segments,
                    clip_mask_kind,
                });
            }
        }
    }

    fn update_clip_task_for_brush(
        &mut self,
        prim_context: &PrimitiveContext,
        prim_index: PrimitiveIndex,
        render_tasks: &mut RenderTaskTree,
        clip_store: &mut ClipStore,
        tasks: &mut Vec<RenderTaskId>,
        node_data: &[ClipScrollNodeData],
        clips: &Vec<ClipWorkItem>,
        combined_outer_rect: &DeviceIntRect,
        has_clips_from_other_coordinate_systems: bool,
    ) -> bool {
        let metadata = &self.cpu_metadata[prim_index.0];
        let brush = match metadata.prim_kind {
            PrimitiveKind::Brush => {
                &mut self.cpu_brushes[metadata.cpu_prim_index.0]
            }
            PrimitiveKind::Picture => {
                &mut self.cpu_pictures[metadata.cpu_prim_index.0].brush
            }
            _ => {
                return false;
            }
        };

        PrimitiveStore::write_brush_segment_description(
            brush,
            metadata,
            prim_context,
            clip_store,
            node_data,
            clips,
            has_clips_from_other_coordinate_systems,
        );

        let segment_desc = match brush.segment_desc {
            Some(ref mut description) => description,
            None => return false,
        };
        let clip_mask_kind = segment_desc.clip_mask_kind;

        for segment in &mut segment_desc.segments {
            if !segment.may_need_clip_mask && clip_mask_kind != BrushClipMaskKind::Global {
                segment.clip_task_id = None;
                continue;
            }

            let segment_screen_rect = calculate_screen_bounding_rect(
                &prim_context.scroll_node.world_content_transform,
                &segment.local_rect,
                prim_context.device_pixel_scale,
            );

            let intersected_rect = combined_outer_rect.intersection(&segment_screen_rect);
            segment.clip_task_id = intersected_rect.map(|bounds| {
                let clip_task = RenderTask::new_mask(
                    bounds,
                    clips.clone(),
                    prim_context.scroll_node.coordinate_system_id,
                );

                let clip_task_id = render_tasks.add(clip_task);
                tasks.push(clip_task_id);

                clip_task_id
            })
        }

        true
    }

    fn update_clip_task(
        &mut self,
        prim_index: PrimitiveIndex,
        prim_context: &PrimitiveContext,
        prim_screen_rect: &DeviceIntRect,
        screen_rect: &DeviceIntRect,
        resource_cache: &mut ResourceCache,
        gpu_cache: &mut GpuCache,
        render_tasks: &mut RenderTaskTree,
        clip_store: &mut ClipStore,
        tasks: &mut Vec<RenderTaskId>,
        node_data: &[ClipScrollNodeData],
    ) -> bool {
        self.cpu_metadata[prim_index.0].clip_task_id = None;

        let prim_screen_rect = match prim_screen_rect.intersection(screen_rect) {
            Some(rect) => rect,
            None => {
                self.cpu_metadata[prim_index.0].screen_rect = None;
                return false;
            }
        };

        let mut combined_outer_rect = match prim_context.clip_chain {
            Some(ref chain) => prim_screen_rect.intersection(&chain.combined_outer_screen_rect),
            None => Some(prim_screen_rect),
        };

        let clip_chain = prim_context.clip_chain.map_or(None, |x| x.nodes.clone());

        let prim_coordinate_system_id = prim_context.scroll_node.coordinate_system_id;
        let transform = &prim_context.scroll_node.world_content_transform;
        let extra_clip =  {
            let metadata = &self.cpu_metadata[prim_index.0];
            let prim_clips = clip_store.get_mut(&metadata.clip_sources);
            if prim_clips.has_clips() {
                prim_clips.update(gpu_cache, resource_cache);
                let (screen_inner_rect, screen_outer_rect) =
                    prim_clips.get_screen_bounds(transform, prim_context.device_pixel_scale);

                if let Some(outer) = screen_outer_rect {
                    combined_outer_rect = combined_outer_rect.and_then(|r| r.intersection(&outer));
                }

                Some(Rc::new(ClipChainNode {
                    work_item: ClipWorkItem {
                        scroll_node_data_index: prim_context.scroll_node.node_data_index,
                        clip_sources: metadata.clip_sources.weak(),
                        coordinate_system_id: prim_coordinate_system_id,
                    },
                    // The local_clip_rect a property of ClipChain nodes that are ClipScrollNodes.
                    // It's used to calculate a local clipping rectangle before we reach this
                    // point, so we can set it to zero here. It should be unused from this point
                    // on.
                    local_clip_rect: LayerRect::zero(),
                    screen_inner_rect,
                    screen_outer_rect: screen_outer_rect.unwrap_or(prim_screen_rect),
                    prev: None,
                }))
            } else {
                None
            }
        };

        // If everything is clipped out, then we don't need to render this primitive.
        let combined_outer_rect = match combined_outer_rect {
            Some(rect) if !rect.is_empty() => rect,
            _ => {
                self.cpu_metadata[prim_index.0].screen_rect = None;
                return false;
            }
        };

        let mut has_clips_from_other_coordinate_systems = false;
        let mut combined_inner_rect = *screen_rect;
        let clips = convert_clip_chain_to_clip_vector(
            clip_chain,
            extra_clip,
            &combined_outer_rect,
            &mut combined_inner_rect,
            prim_context.scroll_node.coordinate_system_id,
            &mut has_clips_from_other_coordinate_systems
        );

        // This can happen if we had no clips or if all the clips were optimized away. In
        // some cases we still need to create a clip mask in order to create a rectangular
        // clip in screen space coordinates.
        if clips.is_empty() {
            // If we don't have any clips from other coordinate systems, the local clip
            // calculated from the clip chain should be sufficient to ensure proper clipping.
            if !has_clips_from_other_coordinate_systems {
                return true;
            }

            // If we have filtered all clips and the screen rect isn't any smaller, we can just
            // skip masking entirely.
            if combined_outer_rect == prim_screen_rect {
                return true;
            }
            // Otherwise we create an empty mask, but with an empty inner rect to avoid further
            // optimization of the empty mask.
            combined_inner_rect = DeviceIntRect::zero();
        }

        if combined_inner_rect.contains_rect(&prim_screen_rect) {
           return true;
        }

        // First try to  render this primitive's mask using optimized brush rendering.
        if self.update_clip_task_for_brush(
            prim_context,
            prim_index,
            render_tasks,
            clip_store,
            tasks,
            node_data,
            &clips,
            &combined_outer_rect,
            has_clips_from_other_coordinate_systems,
        ) {
            return true;
        }

        let clip_task = RenderTask::new_mask(
            combined_outer_rect,
            clips,
            prim_coordinate_system_id,
        );

        let clip_task_id = render_tasks.add(clip_task);
        self.cpu_metadata[prim_index.0].clip_task_id = Some(clip_task_id);
        tasks.push(clip_task_id);

        true
    }

    pub fn prepare_prim_for_render(
        &mut self,
        prim_index: PrimitiveIndex,
        prim_context: &PrimitiveContext,
        resource_cache: &mut ResourceCache,
        gpu_cache: &mut GpuCache,
        render_tasks: &mut RenderTaskTree,
        clip_store: &mut ClipStore,
        clip_scroll_tree: &ClipScrollTree,
        pipelines: &FastHashMap<PipelineId, ScenePipeline>,
        perform_culling: bool,
        parent_tasks: &mut Vec<RenderTaskId>,
        scene_properties: &SceneProperties,
        profile_counters: &mut FrameProfileCounters,
        pic_index: SpecificPrimitiveIndex,
        screen_rect: &DeviceIntRect,
        clip_chain_rect_index: ClipChainRectIndex,
        node_data: &[ClipScrollNodeData],
        local_rects: &mut Vec<LayerRect>,
    ) -> Option<LayerRect> {
        // Reset the visibility of this primitive.
        // Do some basic checks first, that can early out
        // without even knowing the local rect.
        let (cpu_prim_index, dependencies, cull_children, may_need_clip_mask) = {
            let metadata = &mut self.cpu_metadata[prim_index.0];
            metadata.screen_rect = None;

            if perform_culling &&
               !metadata.is_backface_visible &&
               prim_context.scroll_node.world_content_transform.is_backface_visible() {
                return None;
            }

            let (dependencies, cull_children, may_need_clip_mask) = match metadata.prim_kind {
                PrimitiveKind::Picture => {
                    let pic = &mut self.cpu_pictures[metadata.cpu_prim_index.0];

                    if !pic.resolve_scene_properties(scene_properties) {
                        return None;
                    }

                    let (rfid, may_need_clip_mask) = match pic.kind {
                        PictureKind::Image { reference_frame_id, .. } => {
                            (Some(reference_frame_id), false)
                        }
                        _ => {
                            (None, true)
                        }
                    };
                    (Some((pic.pipeline_id, mem::replace(&mut pic.runs, Vec::new()), rfid)),
                     pic.cull_children,
                     may_need_clip_mask)
                }
                _ => {
                    (None, true, true)
                }
            };

            (metadata.cpu_prim_index, dependencies, cull_children, may_need_clip_mask)
        };

        // If we have dependencies, we need to prepare them first, in order
        // to know the actual rect of this primitive.
        // For example, scrolling may affect the location of an item in
        // local space, which may force us to render this item on a larger
        // picture target, if being composited.
        let mut child_tasks = Vec::new();
        if let Some((pipeline_id, dependencies, rfid)) = dependencies {
            let result = self.prepare_prim_runs(
                &dependencies,
                pipeline_id,
                gpu_cache,
                resource_cache,
                render_tasks,
                clip_store,
                clip_scroll_tree,
                pipelines,
                prim_context,
                cull_children,
                &mut child_tasks,
                profile_counters,
                rfid,
                scene_properties,
                cpu_prim_index,
                screen_rect,
                node_data,
                local_rects,
            );

            let metadata = &mut self.cpu_metadata[prim_index.0];

            // Restore the dependencies (borrow check dance)
            let pic = &mut self.cpu_pictures[cpu_prim_index.0];
            pic.runs = dependencies;

            metadata.local_rect = pic.update_local_rect(
                metadata.local_rect,
                result,
            );
        }

        let (local_rect, unclipped_device_rect) = {
            let metadata = &mut self.cpu_metadata[prim_index.0];
            if metadata.local_rect.size.width <= 0.0 ||
               metadata.local_rect.size.height <= 0.0 {
                //warn!("invalid primitive rect {:?}", metadata.local_rect);
                return None;
            }

            let local_rect = metadata.local_clip_rect.intersection(&metadata.local_rect);
            let local_rect = match local_rect {
                Some(local_rect) => local_rect,
                None if perform_culling => return None,
                None => LayerRect::zero(),
            };

            let screen_bounding_rect = calculate_screen_bounding_rect(
                &prim_context.scroll_node.world_content_transform,
                &local_rect,
                prim_context.device_pixel_scale,
            );

            let clip_bounds = match prim_context.clip_chain {
                Some(ref node) => node.combined_outer_screen_rect,
                None => *screen_rect,
            };
            metadata.screen_rect = screen_bounding_rect.intersection(&clip_bounds);

            if metadata.screen_rect.is_none() && perform_culling {
                return None;
            }

            metadata.clip_chain_rect_index = clip_chain_rect_index;

            (local_rect, screen_bounding_rect)
        };

        if perform_culling && may_need_clip_mask && !self.update_clip_task(
            prim_index,
            prim_context,
            &unclipped_device_rect,
            screen_rect,
            resource_cache,
            gpu_cache,
            render_tasks,
            clip_store,
            parent_tasks,
            node_data,
        ) {
            return None;
        }

        self.prepare_prim_for_render_inner(
            prim_index,
            prim_context,
            resource_cache,
            gpu_cache,
            render_tasks,
            child_tasks,
            parent_tasks,
            pic_index,
        );

        Some(local_rect)
    }

    // TODO(gw): Make this simpler / more efficient by tidying
    //           up the logic that early outs from prepare_prim_for_render.
    pub fn reset_prim_visibility(&mut self) {
        for md in &mut self.cpu_metadata {
            md.screen_rect = None;
        }
    }

    pub fn prepare_prim_runs(
        &mut self,
        runs: &[PrimitiveRun],
        pipeline_id: PipelineId,
        gpu_cache: &mut GpuCache,
        resource_cache: &mut ResourceCache,
        render_tasks: &mut RenderTaskTree,
        clip_store: &mut ClipStore,
        clip_scroll_tree: &ClipScrollTree,
        pipelines: &FastHashMap<PipelineId, ScenePipeline>,
        parent_prim_context: &PrimitiveContext,
        perform_culling: bool,
        parent_tasks: &mut Vec<RenderTaskId>,
        profile_counters: &mut FrameProfileCounters,
        original_reference_frame_id: Option<ClipId>,
        scene_properties: &SceneProperties,
        pic_index: SpecificPrimitiveIndex,
        screen_rect: &DeviceIntRect,
        node_data: &[ClipScrollNodeData],
        local_rects: &mut Vec<LayerRect>,
    ) -> PrimitiveRunLocalRect {
        let mut result = PrimitiveRunLocalRect {
            local_rect_in_actual_parent_space: LayerRect::zero(),
            local_rect_in_original_parent_space: LayerRect::zero(),
        };

        for run in runs {
            // TODO(gw): Perhaps we can restructure this to not need to create
            //           a new primitive context for every run (if the hash
            //           lookups ever show up in a profile).
            let scroll_node = &clip_scroll_tree.nodes[&run.clip_and_scroll.scroll_node_id];
            let clip_chain = clip_scroll_tree.get_clip_chain(&run.clip_and_scroll.clip_node_id());

            if perform_culling {
                if !scroll_node.invertible {
                    debug!("{:?} {:?}: position not invertible", run.base_prim_index, pipeline_id);
                    continue;
                }

                match clip_chain {
                     Some(ref chain) if chain.combined_outer_screen_rect.is_empty() => {
                        debug!("{:?} {:?}: clipped out", run.base_prim_index, pipeline_id);
                        continue;
                    }
                    _ => {},
                }
            }


            let parent_relative_transform = parent_prim_context
                .scroll_node
                .world_content_transform
                .inverse()
                .map(|inv_parent| {
                    inv_parent.pre_mul(&scroll_node.world_content_transform)
                });

            let original_relative_transform = original_reference_frame_id
                .and_then(|original_reference_frame_id| {
                    let parent = clip_scroll_tree
                        .nodes[&original_reference_frame_id]
                        .world_content_transform;
                    parent.inverse()
                        .map(|inv_parent| {
                            inv_parent.pre_mul(&scroll_node.world_content_transform)
                        })
                });

            let display_list = &pipelines
                .get(&pipeline_id)
                .expect("No display list?")
                .display_list;

            let child_prim_context = PrimitiveContext::new(
                parent_prim_context.device_pixel_scale,
                display_list,
                clip_chain,
                scroll_node,
            );


            let clip_chain_rect = match perform_culling {
                true => get_local_clip_rect_for_nodes(scroll_node, clip_chain),
                false => None,
            };

            let clip_chain_rect_index = match clip_chain_rect {
                Some(rect) if rect.is_empty() => continue,
                Some(rect) => {
                    local_rects.push(rect);
                    ClipChainRectIndex(local_rects.len() - 1)
                }
                None => ClipChainRectIndex(0), // This is no clipping.
            };


            for i in 0 .. run.count {
                let prim_index = PrimitiveIndex(run.base_prim_index.0 + i);

                if let Some(prim_local_rect) = self.prepare_prim_for_render(
                    prim_index,
                    &child_prim_context,
                    resource_cache,
                    gpu_cache,
                    render_tasks,
                    clip_store,
                    clip_scroll_tree,
                    pipelines,
                    perform_culling,
                    parent_tasks,
                    scene_properties,
                    profile_counters,
                    pic_index,
                    screen_rect,
                    clip_chain_rect_index,
                    node_data,
                    local_rects,
                ) {
                    profile_counters.visible_primitives.inc();

                    if let Some(ref matrix) = original_relative_transform {
                        let bounds = matrix.transform_rect(&prim_local_rect);
                        result.local_rect_in_original_parent_space =
                            result.local_rect_in_original_parent_space.union(&bounds);
                    }

                    if let Some(ref matrix) = parent_relative_transform {
                        let bounds = matrix.transform_rect(&prim_local_rect);
                        result.local_rect_in_actual_parent_space =
                            result.local_rect_in_actual_parent_space.union(&bounds);
                    }
                }
            }
        }

        result
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

        delta_left >= 0f32 && delta_top >= 0f32 && delta_right >= 0f32 && delta_bottom >= 0f32 &&
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

fn convert_clip_chain_to_clip_vector(
    clip_chain_nodes: ClipChainNodeRef,
    extra_clip: ClipChainNodeRef,
    combined_outer_rect: &DeviceIntRect,
    combined_inner_rect: &mut DeviceIntRect,
    prim_coordinate_system: CoordinateSystemId,
    has_clips_from_other_coordinate_systems: &mut bool,
) -> Vec<ClipWorkItem> {
    // Filter out all the clip instances that don't contribute to the result.
    ClipChainNodeIter { current: extra_clip }
        .chain(ClipChainNodeIter { current: clip_chain_nodes })
        .filter_map(|node| {
            *has_clips_from_other_coordinate_systems |=
                prim_coordinate_system != node.work_item.coordinate_system_id;

            *combined_inner_rect = if !node.screen_inner_rect.is_empty() {
                // If this clip's inner area contains the area of the primitive clipped
                // by previous clips, then it's not going to affect rendering in any way.
                if node.screen_inner_rect.contains_rect(&combined_outer_rect) {
                    return None;
                }
                combined_inner_rect.intersection(&node.screen_inner_rect)
                    .unwrap_or_else(DeviceIntRect::zero)
            } else {
                DeviceIntRect::zero()
            };

            Some(node.work_item.clone())
        })
        .collect()
}

fn get_local_clip_rect_for_nodes(
    scroll_node: &ClipScrollNode,
    clip_chain: Option<&ClipChain>,
) -> Option<LayerRect> {
    let clip_chain_nodes = match clip_chain {
        Some(ref clip_chain) => clip_chain.nodes.clone(),
        None => return None,
    };

    let local_rect = ClipChainNodeIter { current: clip_chain_nodes }.fold(
        None,
        |combined_local_clip_rect: Option<LayerRect>, node| {
            if node.work_item.coordinate_system_id != scroll_node.coordinate_system_id {
                return combined_local_clip_rect;
            }

            Some(match combined_local_clip_rect {
                Some(combined_rect) =>
                    combined_rect.intersection(&node.local_clip_rect).unwrap_or_else(LayerRect::zero),
                None => node.local_clip_rect,
            })
        }
    );

    match local_rect {
        Some(local_rect) =>
            Some(scroll_node.coordinate_system_relative_transform.unapply(&local_rect)),
        None => None,
    }
}

impl<'a> GpuDataRequest<'a> {
    // Write the GPU cache data for an individual segment.
    // TODO(gw): The second block is currently unused. In
    //           the future, it will be used to store a
    //           UV rect, allowing segments to reference
    //           part of an image.
    fn write_segment(
        &mut self,
        local_rect: LayerRect,
    ) {
        self.push(local_rect);
        self.push([
            0.0,
            0.0,
            0.0,
            0.0
        ]);
    }
}
