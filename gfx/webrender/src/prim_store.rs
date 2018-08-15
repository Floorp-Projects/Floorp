/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{AlphaType, BorderRadius, BuiltDisplayList, ClipMode, ColorF};
use api::{DeviceIntRect, DeviceIntSize, DevicePixelScale, ExtendMode, LayoutTransform};
use api::{FilterOp, GlyphInstance, GradientStop, ImageKey, ImageRendering, ItemRange, ItemTag, TileOffset};
use api::{GlyphRasterSpace, LayoutPoint, LayoutRect, LayoutSize, LayoutToWorldTransform, LayoutVector2D};
use api::{PremultipliedColorF, PropertyBinding, Shadow, YuvColorSpace, YuvFormat, DeviceIntSideOffsets};
use api::{BorderWidths, BoxShadowClipMode, LayoutToWorldScale, NormalBorder};
use app_units::Au;
use border::{BorderCacheKey, BorderRenderTaskInfo};
use box_shadow::BLUR_SAMPLE_SCALE;
use clip_scroll_tree::{CoordinateSystemId, SpatialNodeIndex};
use clip::{ClipNodeFlags, ClipChainId, ClipChainInstance, ClipItem};
use frame_builder::{FrameBuildingContext, FrameBuildingState, PictureContext, PictureState};
use frame_builder::PrimitiveRunContext;
use glyph_rasterizer::{FontInstance, FontTransform, GlyphKey, FONT_SIZE_LIMIT};
use gpu_cache::{GpuBlockData, GpuCache, GpuCacheAddress, GpuCacheHandle, GpuDataRequest,
                ToGpuBlocks};
use gpu_types::BrushFlags;
use image::{for_each_tile, for_each_repetition};
use picture::{PictureCompositeMode, PicturePrimitive};
#[cfg(debug_assertions)]
use render_backend::FrameId;
use render_task::{BlitSource, RenderTask, RenderTaskCacheKey};
use render_task::{RenderTaskCacheKeyKind, RenderTaskId, RenderTaskCacheEntryHandle};
use renderer::{MAX_VERTEX_TEXTURE_WIDTH};
use resource_cache::{ImageProperties, ImageRequest, ResourceCache};
use scene::SceneProperties;
use segment::SegmentBuilder;
use spatial_node::SpatialNode;
use std::{mem, usize};
use util::{MatrixHelpers, calculate_screen_bounding_rect};
use util::{pack_as_float, recycle_vec, TransformedRectKind};


const MIN_BRUSH_SPLIT_AREA: f32 = 256.0 * 256.0;
pub const VECS_PER_SEGMENT: usize = 2;

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct ScrollNodeAndClipChain {
    pub spatial_node_index: SpatialNodeIndex,
    pub clip_chain_id: ClipChainId,
}

impl ScrollNodeAndClipChain {
    pub fn new(
        spatial_node_index: SpatialNodeIndex,
        clip_chain_id: ClipChainId
    ) -> Self {
        ScrollNodeAndClipChain {
            spatial_node_index,
            clip_chain_id,
        }
    }
}

// This is CPU-side information about a transform, that is relevant
// during culling and primitive prep pass. Often it is the same as
// the information in the clip-scroll tree. However, if we decide
// to rasterize a picture in local space, then this will be the
// transform relative to that picture's coordinate system.
pub struct Transform<'a> {
    pub m: &'a LayoutToWorldTransform,
    pub backface_is_visible: bool,
    pub transform_kind: TransformedRectKind,
}

#[derive(Debug)]
pub struct PrimitiveRun {
    pub base_prim_index: PrimitiveIndex,
    pub count: usize,
    pub spatial_node_index: SpatialNodeIndex,
}

impl PrimitiveRun {
    pub fn is_chasing(&self, index: Option<PrimitiveIndex>) -> bool {
        match index {
            Some(id) if cfg!(debug_assertions) => {
                self.base_prim_index <= id && id.0 < self.base_prim_index.0 + self.count
            }
            _ => false,
        }
    }
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
}

#[derive(Debug)]
pub enum CoordinateSpaceMappingKind {
    Local,
    Offset(LayoutVector2D),
    Transform(Option<LayoutTransform>),
}

#[derive(Debug)]
pub struct CoordinateSpaceMapping {
    kind: CoordinateSpaceMappingKind,
    pub local_rect: LayoutRect,
    ref_spatial_node_index: SpatialNodeIndex,
}

impl CoordinateSpaceMapping {
    fn new(
        ref_spatial_node_index: SpatialNodeIndex,
    ) -> Self {
        CoordinateSpaceMapping {
            kind: CoordinateSpaceMappingKind::Local,
            local_rect: LayoutRect::zero(),
            ref_spatial_node_index,
        }
    }

    pub fn set_target_spatial_node(
        &mut self,
        target_node_index: SpatialNodeIndex,
        spatial_nodes: &[SpatialNode],
    ) {
        let ref_spatial_node = &spatial_nodes[self.ref_spatial_node_index.0];
        let target_spatial_node = &spatial_nodes[target_node_index.0];

        self.kind = if self.ref_spatial_node_index == target_node_index {
            CoordinateSpaceMappingKind::Local
        } else if ref_spatial_node.coordinate_system_id == target_spatial_node.coordinate_system_id {
            let offset = target_spatial_node.coordinate_system_relative_offset -
                         ref_spatial_node.coordinate_system_relative_offset;
            CoordinateSpaceMappingKind::Offset(offset)
        } else {
            let relative_transform = ref_spatial_node
                .world_content_transform
                .inverse()
                .map(|inv_parent| {
                    inv_parent.pre_mul(&target_spatial_node.world_content_transform)
                })
                .map(|transform| {
                    *transform.to_transform()
                });
            CoordinateSpaceMappingKind::Transform(relative_transform)
        }
    }

    pub fn accumulate(&mut self, rect: &LayoutRect) {
        match self.kind {
            CoordinateSpaceMappingKind::Local => {
                self.local_rect = self.local_rect.union(rect);
            }
            CoordinateSpaceMappingKind::Offset(ref offset) => {
                let rect = rect.translate(offset);
                self.local_rect = self.local_rect.union(&rect);
            }
            CoordinateSpaceMappingKind::Transform(ref transform) => {
                if let Some(ref matrix) = transform {
                    match matrix.transform_rect(rect) {
                        Some(bounds) => {
                            self.local_rect = self.local_rect.union(&bounds);
                        }
                        None => {
                            warn!("parent relative transform can't transform the primitive rect for {:?}", rect);
                        }
                    }
                }
            }
        }
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
    pub mapping: CoordinateSpaceMapping,
    pub original_mapping: Option<CoordinateSpaceMapping>,
}

impl PrimitiveRunLocalRect {
    pub fn new(
        spatial_node_index: SpatialNodeIndex,
        original_spatial_node_index: SpatialNodeIndex,
    ) -> Self {
        let mapping = CoordinateSpaceMapping::new(spatial_node_index);

        let original_mapping = if spatial_node_index == original_spatial_node_index {
            None
        } else {
            Some(CoordinateSpaceMapping::new(original_spatial_node_index))
        };

        PrimitiveRunLocalRect {
            mapping,
            original_mapping,
        }
    }

    pub fn set_target_spatial_node(
        &mut self,
        target_node_index: SpatialNodeIndex,
        spatial_nodes: &[SpatialNode],
    ) {
        self.mapping
            .set_target_spatial_node(target_node_index, spatial_nodes);

        if let Some(ref mut mapping) = self.original_mapping {
            mapping.set_target_spatial_node(target_node_index, spatial_nodes);
        }
    }

    pub fn accumulate(&mut self, rect: &LayoutRect) {
        self.mapping.accumulate(rect);

        if let Some(ref mut mapping) = self.original_mapping {
            mapping.accumulate(rect);
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
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct DeferredResolve {
    pub address: GpuCacheAddress,
    pub image_properties: ImageProperties,
}

#[derive(Debug, Copy, Clone, Eq, PartialEq, Hash, Ord, PartialOrd)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct PrimitiveIndex(pub usize);

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

#[derive(Debug, Copy, Clone)]
pub struct ScreenRect {
    pub clipped: DeviceIntRect,
    pub unclipped: DeviceIntRect,
}

// TODO(gw): Pack the fields here better!
#[derive(Debug)]
pub struct PrimitiveMetadata {
    pub opacity: PrimitiveOpacity,
    pub clip_chain_id: ClipChainId,
    pub gpu_location: GpuCacheHandle,
    pub clip_task_id: Option<RenderTaskId>,

    // TODO(gw): In the future, we should just pull these
    //           directly from the DL item, instead of
    //           storing them here.
    pub local_rect: LayoutRect,
    pub local_clip_rect: LayoutRect,

    // The current combined local clip for this primitive, from
    // the primitive local clip above and the current clip chain.
    pub combined_local_clip_rect: LayoutRect,

    pub is_backface_visible: bool,
    pub screen_rect: Option<ScreenRect>,

    /// A tag used to identify this primitive outside of WebRender. This is
    /// used for returning useful data during hit testing.
    pub tag: Option<ItemTag>,

    /// The last frame ID (of the `RenderTaskTree`) this primitive
    /// was prepared for rendering in.
    #[cfg(debug_assertions)]
    pub prepared_frame_id: FrameId,
}

// Maintains a list of opacity bindings that have been collapsed into
// the color of a single primitive. This is an important optimization
// that avoids allocating an intermediate surface for most common
// uses of opacity filters.
#[derive(Debug)]
pub struct OpacityBinding {
    bindings: Vec<PropertyBinding<f32>>,
    current: f32,
}

impl OpacityBinding {
    pub fn new() -> OpacityBinding {
        OpacityBinding {
            bindings: Vec::new(),
            current: 1.0,
        }
    }

    // Add a new opacity value / binding to the list
    pub fn push(&mut self, binding: PropertyBinding<f32>) {
        self.bindings.push(binding);
    }

    // Resolve the current value of each opacity binding, and
    // store that as a single combined opacity. Returns true
    // if the opacity value changed from last time.
    pub fn update(&mut self, scene_properties: &SceneProperties) -> bool {
        let mut new_opacity = 1.0;

        for binding in &self.bindings {
            let opacity = scene_properties.resolve_float(binding);
            new_opacity = new_opacity * opacity;
        }

        let changed = new_opacity != self.current;
        self.current = new_opacity;

        changed
    }
}

#[derive(Debug)]
pub struct VisibleImageTile {
    pub tile_offset: TileOffset,
    pub handle: GpuCacheHandle,
    pub edge_flags: EdgeAaSegmentMask,
    pub local_rect: LayoutRect,
    pub local_clip_rect: LayoutRect,
}

#[derive(Debug)]
pub struct VisibleGradientTile {
    pub handle: GpuCacheHandle,
    pub local_rect: LayoutRect,
    pub local_clip_rect: LayoutRect,
}

#[derive(Debug)]
pub enum BorderSource {
    Image(ImageRequest),
    Border {
        handle: Option<RenderTaskCacheEntryHandle>,
        cache_key: BorderCacheKey,
        task_info: Option<BorderRenderTaskInfo>,
        border: NormalBorder,
        widths: BorderWidths,
    },
}

#[derive(Debug)]
pub enum BrushKind {
    Solid {
        color: ColorF,
        opacity_binding: OpacityBinding,
    },
    Clear,
    Picture(PicturePrimitive),
    Image {
        request: ImageRequest,
        alpha_type: AlphaType,
        stretch_size: LayoutSize,
        tile_spacing: LayoutSize,
        source: ImageSource,
        sub_rect: Option<DeviceIntRect>,
        opacity_binding: OpacityBinding,
        visible_tiles: Vec<VisibleImageTile>,
    },
    YuvImage {
        yuv_key: [ImageKey; 3],
        format: YuvFormat,
        color_space: YuvColorSpace,
        image_rendering: ImageRendering,
    },
    RadialGradient {
        stops_handle: GpuCacheHandle,
        stops_range: ItemRange<GradientStop>,
        extend_mode: ExtendMode,
        center: LayoutPoint,
        start_radius: f32,
        end_radius: f32,
        ratio_xy: f32,
        stretch_size: LayoutSize,
        tile_spacing: LayoutSize,
        visible_tiles: Vec<VisibleGradientTile>,
    },
    LinearGradient {
        stops_handle: GpuCacheHandle,
        stops_range: ItemRange<GradientStop>,
        extend_mode: ExtendMode,
        reverse_stops: bool,
        start_point: LayoutPoint,
        end_point: LayoutPoint,
        stretch_size: LayoutSize,
        tile_spacing: LayoutSize,
        visible_tiles: Vec<VisibleGradientTile>,
    },
    Border {
        source: BorderSource,
    },
}

impl BrushKind {
    fn supports_segments(&self, resource_cache: &ResourceCache) -> bool {
        match *self {
            BrushKind::Image { ref request, .. } => {
                // tiled images don't support segmentation
                resource_cache
                    .get_image_properties(request.key)
                    .and_then(|properties| properties.tiling)
                    .is_none()
            }

            BrushKind::Solid { .. } |
            BrushKind::YuvImage { .. } |
            BrushKind::RadialGradient { .. } |
            BrushKind::Border { .. } |
            BrushKind::LinearGradient { .. } => true,

            // TODO(gw): Allow batch.rs to add segment instances
            //           for Picture primitives.
            BrushKind::Picture { .. } => false,

            BrushKind::Clear => false,
        }
    }

    // Construct a brush that is a solid color rectangle.
    pub fn new_solid(color: ColorF) -> BrushKind {
        BrushKind::Solid {
            color,
            opacity_binding: OpacityBinding::new(),
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

#[derive(Debug, Clone)]
pub enum BrushSegmentTaskId {
    RenderTaskId(RenderTaskId),
    Opaque,
    Empty,
}

impl BrushSegmentTaskId {
    pub fn needs_blending(&self) -> bool {
        match *self {
            BrushSegmentTaskId::RenderTaskId(..) => true,
            BrushSegmentTaskId::Opaque | BrushSegmentTaskId::Empty => false,
        }
    }
}

#[derive(Debug, Clone)]
pub struct BrushSegment {
    pub local_rect: LayoutRect,
    pub clip_task_id: BrushSegmentTaskId,
    pub may_need_clip_mask: bool,
    pub edge_flags: EdgeAaSegmentMask,
    pub extra_data: [f32; 4],
    pub brush_flags: BrushFlags,
}

impl BrushSegment {
    pub fn new(
        rect: LayoutRect,
        may_need_clip_mask: bool,
        edge_flags: EdgeAaSegmentMask,
        extra_data: [f32; 4],
        brush_flags: BrushFlags,
    ) -> BrushSegment {
        BrushSegment {
            local_rect: rect,
            clip_task_id: BrushSegmentTaskId::Opaque,
            may_need_clip_mask,
            edge_flags,
            extra_data,
            brush_flags,
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
    ) -> Self {
        BrushPrimitive {
            kind,
            segment_desc,
        }
    }

    pub fn new_picture(prim: PicturePrimitive) -> Self {
        BrushPrimitive {
            kind: BrushKind::Picture(prim),
            segment_desc: None,
        }
    }

    fn write_gpu_blocks(
        &self,
        request: &mut GpuDataRequest,
        local_rect: LayoutRect,
    ) {
        // has to match VECS_PER_SPECIFIC_BRUSH
        match self.kind {
            BrushKind::Border { .. } => {
                // Border primitives currently used for
                // image borders, and run through the
                // normal brush_image shader.
                request.push(PremultipliedColorF::WHITE);
                request.push(PremultipliedColorF::WHITE);
                request.push([
                    local_rect.size.width,
                    local_rect.size.height,
                    0.0,
                    0.0,
                ]);
            }
            BrushKind::YuvImage { .. } => {}
            BrushKind::Picture { .. } => {
                request.push(PremultipliedColorF::WHITE);
                request.push(PremultipliedColorF::WHITE);
                request.push([
                    local_rect.size.width,
                    local_rect.size.height,
                    0.0,
                    0.0,
                ]);
            }
            // Images are drawn as a white color, modulated by the total
            // opacity coming from any collapsed property bindings.
            BrushKind::Image { stretch_size, tile_spacing, ref opacity_binding, .. } => {
                request.push(ColorF::new(1.0, 1.0, 1.0, opacity_binding.current).premultiplied());
                request.push(PremultipliedColorF::WHITE);
                request.push([
                    stretch_size.width + tile_spacing.width,
                    stretch_size.height + tile_spacing.height,
                    0.0,
                    0.0,
                ]);
            }
            // Solid rects also support opacity collapsing.
            BrushKind::Solid { color, ref opacity_binding, .. } => {
                request.push(color.scale_alpha(opacity_binding.current).premultiplied());
            }
            BrushKind::Clear => {
                // Opaque black with operator dest out
                request.push(PremultipliedColorF::BLACK);
            }
            BrushKind::LinearGradient { stretch_size, start_point, end_point, extend_mode, .. } => {
                request.push([
                    start_point.x,
                    start_point.y,
                    end_point.x,
                    end_point.y,
                ]);
                request.push([
                    pack_as_float(extend_mode as u32),
                    stretch_size.width,
                    stretch_size.height,
                    0.0,
                ]);
            }
            BrushKind::RadialGradient { stretch_size, center, start_radius, end_radius, ratio_xy, extend_mode, .. } => {
                request.push([
                    center.x,
                    center.y,
                    start_radius,
                    end_radius,
                ]);
                request.push([
                    ratio_xy,
                    pack_as_float(extend_mode as u32),
                    stretch_size.width,
                    stretch_size.height,
                ]);
            }
        }
    }
}

// Key that identifies a unique (partial) image that is being
// stored in the render task cache.
#[derive(Debug, Copy, Clone, Eq, Hash, PartialEq)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct ImageCacheKey {
    pub request: ImageRequest,
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
        handle: Option<RenderTaskCacheEntryHandle>,
    },
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
        let mut cur_color = match src_stops.next() {
            Some(stop) => {
                debug_assert_eq!(stop.offset, 0.0);
                stop.color.premultiplied()
            }
            None => {
                error!("Zero gradient stops found!");
                PremultipliedColorF::BLACK
            }
        };

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
            if cur_idx != GRADIENT_DATA_TABLE_BEGIN {
                error!("Gradient stops abruptly at {}, auto-completing to white", cur_idx);
                self.fill_colors(GRADIENT_DATA_TABLE_BEGIN, cur_idx, &PremultipliedColorF::WHITE, &cur_color, &mut entries);
            }

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
            if cur_idx != GRADIENT_DATA_TABLE_END {
                error!("Gradient stops abruptly at {}, auto-completing to white", cur_idx);
                self.fill_colors(cur_idx, GRADIENT_DATA_TABLE_END, &PremultipliedColorF::WHITE, &cur_color, &mut entries);
            }

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

#[derive(Debug, Clone)]
pub struct TextRunPrimitive {
    pub specified_font: FontInstance,
    pub used_font: FontInstance,
    pub offset: LayoutVector2D,
    pub glyph_range: ItemRange<GlyphInstance>,
    pub glyph_keys: Vec<GlyphKey>,
    pub glyph_gpu_blocks: Vec<GpuBlockData>,
    pub shadow: bool,
    pub glyph_raster_space: GlyphRasterSpace,
}

impl TextRunPrimitive {
    pub fn new(
        font: FontInstance,
        offset: LayoutVector2D,
        glyph_range: ItemRange<GlyphInstance>,
        glyph_keys: Vec<GlyphKey>,
        shadow: bool,
        glyph_raster_space: GlyphRasterSpace,
    ) -> Self {
        TextRunPrimitive {
            specified_font: font.clone(),
            used_font: font,
            offset,
            glyph_range,
            glyph_keys,
            glyph_gpu_blocks: Vec::new(),
            shadow,
            glyph_raster_space,
        }
    }

    pub fn update_font_instance(
        &mut self,
        device_pixel_scale: DevicePixelScale,
        transform: &LayoutToWorldTransform,
        allow_subpixel_aa: bool,
    ) -> bool {
        // Get the current font size in device pixels
        let device_font_size = self.specified_font.size.scale_by(device_pixel_scale.0);

        // Determine if rasterizing glyphs in local or screen space.
        // Only support transforms that can be coerced to simple 2D transforms.
        let transform_glyphs = if transform.has_perspective_component() ||
           !transform.has_2d_inverse() ||
           // Font sizes larger than the limit need to be scaled, thus can't use subpixels.
           transform.exceeds_2d_scale(FONT_SIZE_LIMIT / device_font_size.to_f64_px()) ||
           // Otherwise, ensure the font is rasterized in screen-space.
           self.glyph_raster_space != GlyphRasterSpace::Screen {
            false
        } else {
            true
        };

        // Get the font transform matrix (skew / scale) from the complete transform.
        let font_transform = if transform_glyphs {
            // Quantize the transform to minimize thrashing of the glyph cache.
            FontTransform::from(transform).quantize()
        } else {
            FontTransform::identity()
        };

        // If the transform or device size is different, then the caller of
        // this method needs to know to rebuild the glyphs.
        let cache_dirty =
            self.used_font.transform != font_transform ||
            self.used_font.size != device_font_size;

        // Construct used font instance from the specified font instance
        self.used_font = FontInstance {
            transform: font_transform,
            size: device_font_size,
            ..self.specified_font.clone()
        };

        // If subpixel AA is disabled due to the backing surface the glyphs
        // are being drawn onto, disable it (unless we are using the
        // specifial subpixel mode that estimates background color).
        if !allow_subpixel_aa && self.specified_font.bg_color.a == 0 {
            self.used_font.disable_subpixel_aa();
        }

        // If using local space glyphs, we don't want subpixel AA
        // or positioning.
        if !transform_glyphs {
            self.used_font.disable_subpixel_aa();
            self.used_font.disable_subpixel_position();
        }

        cache_dirty
    }

    fn prepare_for_render(
        &mut self,
        device_pixel_scale: DevicePixelScale,
        transform: &LayoutToWorldTransform,
        allow_subpixel_aa: bool,
        display_list: &BuiltDisplayList,
        frame_building_state: &mut FrameBuildingState,
    ) {
        let cache_dirty = self.update_font_instance(
            device_pixel_scale,
            transform,
            allow_subpixel_aa,
        );

        // Cache the glyph positions, if not in the cache already.
        // TODO(gw): In the future, remove `glyph_instances`
        //           completely, and just reference the glyphs
        //           directly from the display list.
        if self.glyph_keys.is_empty() || cache_dirty {
            let subpx_dir = self.used_font.get_subpx_dir();
            let src_glyphs = display_list.get(self.glyph_range);

            // TODO(gw): If we support chunks() on AuxIter
            //           in the future, this code below could
            //           be much simpler...
            let mut gpu_block = [0.0; 4];
            for (i, src) in src_glyphs.enumerate() {
                let world_offset = self.used_font.transform.transform(&src.point);
                let device_offset = device_pixel_scale.transform_point(&world_offset);
                let key = GlyphKey::new(src.index, device_offset, subpx_dir);
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

        frame_building_state.resource_cache
                            .request_glyphs(self.used_font.clone(),
                                            &self.glyph_keys,
                                            frame_building_state.gpu_cache,
                                            frame_building_state.render_tasks,
                                            frame_building_state.special_render_passes);
    }

    fn write_gpu_blocks(&self, request: &mut GpuDataRequest) {
        request.push(ColorF::from(self.used_font.color).premultiplied());
        // this is the only case where we need to provide plain color to GPU
        let bg_color = ColorF::from(self.used_font.bg_color);
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
    rect: LayoutRect,
    mode: f32,
}

#[derive(Debug)]
#[repr(C)]
struct ClipCorner {
    rect: LayoutRect,
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

    fn uniform(rect: LayoutRect, outer_radius: f32, inner_radius: f32) -> ClipCorner {
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
    pub local_rect: LayoutRect,
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
    pub fn rounded_rect(rect: &LayoutRect, radii: &BorderRadius, mode: ClipMode) -> ClipData {
        ClipData {
            rect: ClipRect {
                rect: *rect,
                mode: mode as u32 as f32,
            },
            top_left: ClipCorner {
                rect: LayoutRect::new(
                    LayoutPoint::new(rect.origin.x, rect.origin.y),
                    LayoutSize::new(radii.top_left.width, radii.top_left.height),
                ),
                outer_radius_x: radii.top_left.width,
                outer_radius_y: radii.top_left.height,
                inner_radius_x: 0.0,
                inner_radius_y: 0.0,
            },
            top_right: ClipCorner {
                rect: LayoutRect::new(
                    LayoutPoint::new(
                        rect.origin.x + rect.size.width - radii.top_right.width,
                        rect.origin.y,
                    ),
                    LayoutSize::new(radii.top_right.width, radii.top_right.height),
                ),
                outer_radius_x: radii.top_right.width,
                outer_radius_y: radii.top_right.height,
                inner_radius_x: 0.0,
                inner_radius_y: 0.0,
            },
            bottom_left: ClipCorner {
                rect: LayoutRect::new(
                    LayoutPoint::new(
                        rect.origin.x,
                        rect.origin.y + rect.size.height - radii.bottom_left.height,
                    ),
                    LayoutSize::new(radii.bottom_left.width, radii.bottom_left.height),
                ),
                outer_radius_x: radii.bottom_left.width,
                outer_radius_y: radii.bottom_left.height,
                inner_radius_x: 0.0,
                inner_radius_y: 0.0,
            },
            bottom_right: ClipCorner {
                rect: LayoutRect::new(
                    LayoutPoint::new(
                        rect.origin.x + rect.size.width - radii.bottom_right.width,
                        rect.origin.y + rect.size.height - radii.bottom_right.height,
                    ),
                    LayoutSize::new(radii.bottom_right.width, radii.bottom_right.height),
                ),
                outer_radius_x: radii.bottom_right.width,
                outer_radius_y: radii.bottom_right.height,
                inner_radius_x: 0.0,
                inner_radius_y: 0.0,
            },
        }
    }

    pub fn uniform(rect: LayoutRect, radius: f32, mode: ClipMode) -> ClipData {
        ClipData {
            rect: ClipRect {
                rect,
                mode: mode as u32 as f32,
            },
            top_left: ClipCorner::uniform(
                LayoutRect::new(
                    LayoutPoint::new(rect.origin.x, rect.origin.y),
                    LayoutSize::new(radius, radius),
                ),
                radius,
                0.0,
            ),
            top_right: ClipCorner::uniform(
                LayoutRect::new(
                    LayoutPoint::new(rect.origin.x + rect.size.width - radius, rect.origin.y),
                    LayoutSize::new(radius, radius),
                ),
                radius,
                0.0,
            ),
            bottom_left: ClipCorner::uniform(
                LayoutRect::new(
                    LayoutPoint::new(rect.origin.x, rect.origin.y + rect.size.height - radius),
                    LayoutSize::new(radius, radius),
                ),
                radius,
                0.0,
            ),
            bottom_right: ClipCorner::uniform(
                LayoutRect::new(
                    LayoutPoint::new(
                        rect.origin.x + rect.size.width - radius,
                        rect.origin.y + rect.size.height - radius,
                    ),
                    LayoutSize::new(radius, radius),
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
    TextRun(TextRunPrimitive),
    Brush(BrushPrimitive),
}

impl PrimitiveContainer {
    // Return true if the primary primitive is visible.
    // Used to trivially reject non-visible primitives.
    // TODO(gw): Currently, primitives other than those
    //           listed here are handled before the
    //           add_primitive() call. In the future
    //           we should move the logic for all other
    //           primitive types to use this.
    pub fn is_visible(&self) -> bool {
        match *self {
            PrimitiveContainer::TextRun(ref info) => {
                info.specified_font.color.a > 0
            }
            PrimitiveContainer::Brush(ref brush) => {
                match brush.kind {
                    BrushKind::Solid { ref color, .. } => {
                        color.a > 0.0
                    }
                    BrushKind::Clear |
                    BrushKind::Picture { .. } |
                    BrushKind::Image { .. } |
                    BrushKind::YuvImage { .. } |
                    BrushKind::RadialGradient { .. } |
                    BrushKind::Border { .. } |
                    BrushKind::LinearGradient { .. } => {
                        true
                    }
                }
            }
        }
    }

    // Create a clone of this PrimitiveContainer, applying whatever
    // changes are necessary to the primitive to support rendering
    // it as part of the supplied shadow.
    pub fn create_shadow(&self, shadow: &Shadow) -> PrimitiveContainer {
        match *self {
            PrimitiveContainer::TextRun(ref info) => {
                let mut font = FontInstance {
                    color: shadow.color.into(),
                    ..info.specified_font.clone()
                };
                if shadow.blur_radius > 0.0 {
                    font.disable_subpixel_aa();
                }

                PrimitiveContainer::TextRun(TextRunPrimitive::new(
                    font,
                    info.offset + shadow.offset,
                    info.glyph_range,
                    info.glyph_keys.clone(),
                    true,
                    info.glyph_raster_space,
                ))
            }
            PrimitiveContainer::Brush(ref brush) => {
                match brush.kind {
                    BrushKind::Solid { .. } => {
                        PrimitiveContainer::Brush(BrushPrimitive::new(
                            BrushKind::new_solid(shadow.color),
                            None,
                        ))
                    }
                    BrushKind::Clear |
                    BrushKind::Picture { .. } |
                    BrushKind::Image { .. } |
                    BrushKind::YuvImage { .. } |
                    BrushKind::Border { .. } |
                    BrushKind::RadialGradient { .. } |
                    BrushKind::LinearGradient { .. } => {
                        panic!("bug: other brush kinds not expected here yet");
                    }
                }
            }
        }
    }
}

pub enum PrimitiveDetails {
    Brush(BrushPrimitive),
    TextRun(TextRunPrimitive),
}

pub struct Primitive {
    pub metadata: PrimitiveMetadata,
    pub details: PrimitiveDetails,
}

impl Primitive {
    pub fn as_pic(&self) -> &PicturePrimitive {
        match self.details {
            PrimitiveDetails::Brush(BrushPrimitive { kind: BrushKind::Picture(ref pic), .. }) => pic,
            _ => {
                panic!("bug: not a picture!");
            }
        }
    }

    pub fn as_pic_mut(&mut self) -> &mut PicturePrimitive {
        match self.details {
            PrimitiveDetails::Brush(BrushPrimitive { kind: BrushKind::Picture(ref mut pic), .. }) => pic,
            _ => {
                panic!("bug: not a picture!");
            }
        }
    }
}

pub struct PrimitiveStore {
    pub primitives: Vec<Primitive>,

    /// A primitive index to chase through debugging.
    pub chase_id: Option<PrimitiveIndex>,
}

impl PrimitiveStore {
    pub fn new() -> PrimitiveStore {
        PrimitiveStore {
            primitives: Vec::new(),
            chase_id: None,
        }
    }

    pub fn recycle(self) -> Self {
        PrimitiveStore {
            primitives: recycle_vec(self.primitives),
            chase_id: self.chase_id,
        }
    }

    pub fn get_pic(&self, index: PrimitiveIndex) -> &PicturePrimitive {
        self.primitives[index.0].as_pic()
    }

    pub fn get_pic_mut(&mut self, index: PrimitiveIndex) -> &mut PicturePrimitive {
        self.primitives[index.0].as_pic_mut()
    }

    pub fn add_primitive(
        &mut self,
        local_rect: &LayoutRect,
        local_clip_rect: &LayoutRect,
        is_backface_visible: bool,
        clip_chain_id: ClipChainId,
        tag: Option<ItemTag>,
        container: PrimitiveContainer,
    ) -> PrimitiveIndex {
        let prim_index = self.primitives.len();

        let base_metadata = PrimitiveMetadata {
            clip_chain_id,
            gpu_location: GpuCacheHandle::new(),
            clip_task_id: None,
            local_rect: *local_rect,
            local_clip_rect: *local_clip_rect,
            combined_local_clip_rect: *local_clip_rect,
            is_backface_visible,
            screen_rect: None,
            tag,
            opacity: PrimitiveOpacity::translucent(),
            #[cfg(debug_assertions)]
            prepared_frame_id: FrameId(0),
        };

        let prim = match container {
            PrimitiveContainer::Brush(brush) => {
                let opacity = match brush.kind {
                    BrushKind::Clear => PrimitiveOpacity::translucent(),
                    BrushKind::Solid { ref color, .. } => PrimitiveOpacity::from_alpha(color.a),
                    BrushKind::Image { .. } => PrimitiveOpacity::translucent(),
                    BrushKind::YuvImage { .. } => PrimitiveOpacity::opaque(),
                    BrushKind::RadialGradient { .. } => PrimitiveOpacity::translucent(),
                    BrushKind::LinearGradient { .. } => PrimitiveOpacity::translucent(),
                    BrushKind::Picture { .. } => PrimitiveOpacity::translucent(),
                    BrushKind::Border { .. } => PrimitiveOpacity::translucent(),
                };

                let metadata = PrimitiveMetadata {
                    opacity,
                    ..base_metadata
                };

                Primitive {
                    metadata,
                    details: PrimitiveDetails::Brush(brush),
                }
            }
            PrimitiveContainer::TextRun(text_cpu) => {
                let metadata = PrimitiveMetadata {
                    opacity: PrimitiveOpacity::translucent(),
                    ..base_metadata
                };

                Primitive {
                    metadata,
                    details: PrimitiveDetails::TextRun(text_cpu),
                }
            }
        };

        self.primitives.push(prim);

        PrimitiveIndex(prim_index)
    }

    // Internal method that retrieves the primitive index of a primitive
    // that can be the target for collapsing parent opacity filters into.
    fn get_opacity_collapse_prim(
        &self,
        prim_index: PrimitiveIndex,
    ) -> Option<PrimitiveIndex> {
        let pic = self.get_pic(prim_index);

        // We can only collapse opacity if there is a single primitive, otherwise
        // the opacity needs to be applied to the primitives as a group.
        if pic.runs.len() != 1 {
            return None;
        }

        let run = &pic.runs[0];
        if run.count != 1 {
            return None;
        }

        let prim = &self.primitives[run.base_prim_index.0];

        // For now, we only support opacity collapse on solid rects and images.
        // This covers the most common types of opacity filters that can be
        // handled by this optimization. In the future, we can easily extend
        // this to other primitives, such as text runs and gradients.
        match prim.details {
            PrimitiveDetails::Brush(ref brush) => {
                match brush.kind {
                    BrushKind::Picture(ref pic) => {
                        // If we encounter a picture that is a pass-through
                        // (i.e. no composite mode), then we can recurse into
                        // that to try and find a primitive to collapse to.
                        if pic.composite_mode.is_none() {
                            return self.get_opacity_collapse_prim(run.base_prim_index);
                        }
                    }
                    // If we find a single rect or image, we can use that
                    // as the primitive to collapse the opacity into.
                    BrushKind::Solid { .. } | BrushKind::Image { .. } => {
                        return Some(run.base_prim_index)
                    }
                    BrushKind::Border { .. } |
                    BrushKind::YuvImage { .. } |
                    BrushKind::LinearGradient { .. } |
                    BrushKind::RadialGradient { .. } |
                    BrushKind::Clear => {}
                }
            }
            PrimitiveDetails::TextRun(..) => {}
        }

        None
    }

    // Apply any optimizations to drawing this picture. Currently,
    // we just support collapsing pictures with an opacity filter
    // by pushing that opacity value into the color of a primitive
    // if that picture contains one compatible primitive.
    pub fn optimize_picture_if_possible(
        &mut self,
        pic_prim_index: PrimitiveIndex,
    ) {
        // Only handle opacity filters for now.
        let binding = match self.get_pic(pic_prim_index).composite_mode {
            Some(PictureCompositeMode::Filter(FilterOp::Opacity(binding, _))) => {
                binding
            }
            _ => {
                return;
            }
        };

        // See if this picture contains a single primitive that supports
        // opacity collapse.
        match self.get_opacity_collapse_prim(pic_prim_index) {
            Some(prim_index) => {
                let prim = &mut self.primitives[prim_index.0];
                match prim.details {
                    PrimitiveDetails::Brush(ref mut brush) => {
                        // By this point, we know we should only have found a primitive
                        // that supports opacity collapse.
                        match brush.kind {
                            BrushKind::Solid { ref mut opacity_binding, .. } |
                            BrushKind::Image { ref mut opacity_binding, .. } => {
                                opacity_binding.push(binding);
                            }
                            BrushKind::Clear { .. } |
                            BrushKind::Picture { .. } |
                            BrushKind::YuvImage { .. } |
                            BrushKind::Border { .. } |
                            BrushKind::LinearGradient { .. } |
                            BrushKind::RadialGradient { .. } => {
                                unreachable!("bug: invalid prim type for opacity collapse");
                            }
                        }
                    }
                    PrimitiveDetails::TextRun(..) => {
                        unreachable!("bug: invalid prim type for opacity collapse");
                    }
                }
            }
            None => {
                return;
            }
        }

        // The opacity filter has been collapsed, so mark this picture
        // as a pass though. This means it will no longer allocate an
        // intermediate surface or incur an extra blend / blit. Instead,
        // the collapsed primitive will be drawn directly into the
        // parent picture.
        self.get_pic_mut(pic_prim_index).composite_mode = None;
    }

    pub fn prim_count(&self) -> usize {
        self.primitives.len()
    }

    pub fn prepare_prim_for_render(
        &mut self,
        prim_index: PrimitiveIndex,
        prim_run_context: &PrimitiveRunContext,
        pic_context: &PictureContext,
        pic_state: &mut PictureState,
        frame_context: &FrameBuildingContext,
        frame_state: &mut FrameBuildingState,
        display_list: &BuiltDisplayList,
    ) -> Option<LayoutRect> {
        let mut may_need_clip_mask = true;
        let mut pic_state_for_children = PictureState::new();
        let is_chased = Some(prim_index) == self.chase_id;

        // If we have dependencies, we need to prepare them first, in order
        // to know the actual rect of this primitive.
        // For example, scrolling may affect the location of an item in
        // local space, which may force us to render this item on a larger
        // picture target, if being composited.
        let pic_context_for_children = {
            let prim = &mut self.primitives[prim_index.0];

            // Do some basic checks first, that can early out
            // without even knowing the local rect.
            if !prim.metadata.is_backface_visible && prim_run_context.transform.backface_is_visible {
                if cfg!(debug_assertions) && is_chased {
                    println!("\tculled for not having visible back faces");
                }
                return None;
            }

            match prim.details {
                PrimitiveDetails::Brush(ref mut brush) => {
                    match brush.kind {
                        BrushKind::Picture(ref mut pic) => {
                            if !pic.resolve_scene_properties(frame_context.scene_properties) {
                                if cfg!(debug_assertions) && is_chased {
                                    println!("\tculled for carrying an invisible composite filter");
                                }
                                return None;
                            }

                            may_need_clip_mask = pic.composite_mode.is_some();

                            let inflation_factor = match pic.composite_mode {
                                Some(PictureCompositeMode::Filter(FilterOp::Blur(blur_radius))) => {
                                    // The amount of extra space needed for primitives inside
                                    // this picture to ensure the visibility check is correct.
                                    BLUR_SAMPLE_SCALE * blur_radius
                                }
                                _ => {
                                    0.0
                                }
                            };

                            // Mark whether this picture has a complex coordinate system.
                            pic_state_for_children.has_non_root_coord_system |=
                                prim_run_context.scroll_node.coordinate_system_id != CoordinateSystemId::root();

                            Some(PictureContext {
                                pipeline_id: pic.pipeline_id,
                                prim_runs: mem::replace(&mut pic.runs, Vec::new()),
                                spatial_node_index: prim_run_context.spatial_node_index,
                                original_spatial_node_index: pic.original_spatial_node_index,
                                apply_local_clip_rect: pic.apply_local_clip_rect,
                                inflation_factor,
                                // TODO(lsalzman): allow overriding parent if intermediate surface is opaque
                                allow_subpixel_aa: pic_context.allow_subpixel_aa && pic.allow_subpixel_aa(),
                            })
                        }
                        _ => {
                            None
                        }
                    }
                }
                PrimitiveDetails::TextRun(..) => {
                    None
                }
            }
        };

        if let Some(pic_context_for_children) = pic_context_for_children {
            let result = self.prepare_prim_runs(
                &pic_context_for_children,
                &mut pic_state_for_children,
                frame_context,
                frame_state,
            );

            // Restore the dependencies (borrow check dance)
            let prim = &mut self.primitives[prim_index.0];
            let new_local_rect = prim
                .as_pic_mut()
                .update_local_rect_and_set_runs(
                    result,
                    pic_context_for_children.prim_runs
                );

            if new_local_rect != prim.metadata.local_rect {
                prim.metadata.local_rect = new_local_rect;
                frame_state.gpu_cache.invalidate(&mut prim.metadata.gpu_location);
                pic_state.local_rect_changed = true;
            }
        }

        let prim = &mut self.primitives[prim_index.0];
        prim.metadata.screen_rect = None;

        if prim.metadata.local_rect.size.width <= 0.0 ||
           prim.metadata.local_rect.size.height <= 0.0 {
            if cfg!(debug_assertions) && is_chased {
                println!("\tculled for zero local rectangle");
            }
            return None;
        }

        // Inflate the local rect for this primitive by the inflation factor of
        // the picture context. This ensures that even if the primitive itself
        // is not visible, any effects from the blur radius will be correctly
        // taken into account.
        let local_rect = prim
            .metadata
            .local_rect
            .inflate(pic_context.inflation_factor, pic_context.inflation_factor)
            .intersection(&prim.metadata.local_clip_rect);
        let local_rect = match local_rect {
            Some(local_rect) => local_rect,
            None => {
                if cfg!(debug_assertions) && is_chased {
                    println!("\tculled for being out of the local clip rectangle: {:?}",
                        prim.metadata.local_clip_rect);
                }
                return None
            }
        };

        let clip_chain = frame_state
            .clip_store
            .build_clip_chain_instance(
                prim.metadata.clip_chain_id,
                local_rect,
                prim.metadata.local_clip_rect,
                prim_run_context.spatial_node_index,
                &frame_context.clip_scroll_tree.spatial_nodes,
                frame_state.gpu_cache,
                frame_state.resource_cache,
                frame_context.device_pixel_scale,
            );

        let clip_chain = match clip_chain {
            Some(clip_chain) => clip_chain,
            None => {
                prim.metadata.screen_rect = None;
                return None;
            }
        };

        if cfg!(debug_assertions) && is_chased {
            println!("\teffective clip chain from {:?} {}",
                clip_chain.clips_range,
                if pic_context.apply_local_clip_rect { "(applied)" } else { "" },
            );
        }

        pic_state.has_non_root_coord_system |= clip_chain.has_non_root_coord_system;

        let unclipped_device_rect = match calculate_screen_bounding_rect(
            &prim_run_context.scroll_node.world_content_transform,
            &local_rect,
            frame_context.device_pixel_scale,
            None, //TODO: inflate `frame_context.screen_rect` appropriately
        ) {
            Some(rect) => rect,
            None => {
                if cfg!(debug_assertions) && is_chased {
                    println!("\tculled for being behind the near plane of transform: {:?}",
                        prim_run_context.scroll_node.world_content_transform);
                }
                return None
            }
        };

        let clipped_device_rect = match calculate_screen_bounding_rect(
            &prim_run_context.scroll_node.world_content_transform,
            &clip_chain.local_bounding_rect,
            frame_context.device_pixel_scale,
            None,
        ) {
            Some(rect) => rect,
            None => {
                if cfg!(debug_assertions) && is_chased {
                    println!("\tculled for being behind the near plane of transform: {:?}",
                        prim_run_context.scroll_node.world_content_transform);
                }
                return None
            }
        };

        let clipped_device_rect = match clipped_device_rect.intersection(&frame_context.screen_rect) {
            Some(clipped_device_rect) => clipped_device_rect,
            None => return None,
        };

        prim.metadata.screen_rect = Some(ScreenRect {
            clipped: clipped_device_rect,
            unclipped: unclipped_device_rect,
        });

        prim.metadata.combined_local_clip_rect = if pic_context.apply_local_clip_rect {
            clip_chain.local_clip_rect
        } else {
            prim.metadata.local_clip_rect
        };

        let ccr = match prim.metadata.combined_local_clip_rect.intersection(&local_rect) {
            Some(ccr) => ccr,
            None => return None,
        };

        prim.build_prim_segments_if_needed(
            pic_state,
            frame_state,
            frame_context,
        );

        if may_need_clip_mask && !prim.update_clip_task(
            prim_run_context,
            &clipped_device_rect,
            &clip_chain,
            pic_state,
            frame_context,
            frame_state,
            is_chased,
        ) {
            return None;
        }

        if cfg!(debug_assertions) && is_chased {
            println!("\tconsidered visible and ready with local rect {:?}", local_rect);
        }

        prim.prepare_prim_for_render_inner(
            prim_index,
            prim_run_context,
            pic_state_for_children,
            pic_context,
            pic_state,
            frame_context,
            frame_state,
            display_list,
            is_chased,
        );

        Some(ccr)
    }

    // TODO(gw): Make this simpler / more efficient by tidying
    //           up the logic that early outs from prepare_prim_for_render.
    pub fn reset_prim_visibility(&mut self) {
        for prim in &mut self.primitives {
            prim.metadata.screen_rect = None;
        }
    }

    pub fn prepare_prim_runs(
        &mut self,
        pic_context: &PictureContext,
        pic_state: &mut PictureState,
        frame_context: &FrameBuildingContext,
        frame_state: &mut FrameBuildingState,
    ) -> PrimitiveRunLocalRect {
        let mut result = PrimitiveRunLocalRect::new(
            pic_context.spatial_node_index,
            pic_context.original_spatial_node_index,
        );

        let display_list = &frame_context
            .pipelines
            .get(&pic_context.pipeline_id)
            .expect("No display list?")
            .display_list;

        for run in &pic_context.prim_runs {
            // TODO(gw): Perhaps we can restructure this to not need to create
            //           a new primitive context for every run (if the hash
            //           lookups ever show up in a profile).
            let scroll_node = &frame_context
                .clip_scroll_tree
                .spatial_nodes[run.spatial_node_index.0];

            if run.is_chasing(self.chase_id) {
                println!("\tpreparing a run of length {} in pipeline {:?}",
                    run.count, pic_context.pipeline_id);
                println!("\trun {:?}", run.spatial_node_index);
                println!("\ttransform {:?}", scroll_node.world_content_transform.to_transform());
            }

            // Mark whether this picture contains any complex coordinate
            // systems, due to either the scroll node or the clip-chain.
            pic_state.has_non_root_coord_system |=
                scroll_node.coordinate_system_id != CoordinateSystemId::root();

            if !scroll_node.invertible {
                if run.is_chasing(self.chase_id) {
                    println!("\tculled for the scroll node transform being invertible");
                }
                continue;
            }

            result.set_target_spatial_node(
                run.spatial_node_index,
                &frame_context.clip_scroll_tree.spatial_nodes,
            );

            let transform = frame_context
                .transforms
                .get_transform(run.spatial_node_index);

            let child_prim_run_context = PrimitiveRunContext::new(
                scroll_node,
                run.spatial_node_index,
                transform,
            );

            for i in 0 .. run.count {
                let prim_index = PrimitiveIndex(run.base_prim_index.0 + i);

                if let Some(prim_local_rect) = self.prepare_prim_for_render(
                    prim_index,
                    &child_prim_run_context,
                    pic_context,
                    pic_state,
                    frame_context,
                    frame_state,
                    display_list,
                ) {
                    frame_state.profile_counters.visible_primitives.inc();
                    result.accumulate(&prim_local_rect);
                }
            }
        }

        result
    }
}

fn build_gradient_stops_request(
    stops_handle: &mut GpuCacheHandle,
    stops_range: ItemRange<GradientStop>,
    reverse_stops: bool,
    frame_state: &mut FrameBuildingState,
    display_list: &BuiltDisplayList,
) {
    if let Some(mut request) = frame_state.gpu_cache.request(stops_handle) {
        let gradient_builder = GradientGpuBlockBuilder::new(
            stops_range,
            display_list,
        );
        gradient_builder.build(
            reverse_stops,
            &mut request,
        );
    }
}

fn decompose_repeated_primitive(
    visible_tiles: &mut Vec<VisibleGradientTile>,
    metadata: &mut PrimitiveMetadata,
    stretch_size: &LayoutSize,
    tile_spacing: &LayoutSize,
    prim_run_context: &PrimitiveRunContext,
    frame_context: &FrameBuildingContext,
    frame_state: &mut FrameBuildingState,
    callback: &mut FnMut(&LayoutRect, GpuDataRequest),
) {
    visible_tiles.clear();

    // Tighten the clip rect because decomposing the repeated image can
    // produce primitives that are partially covering the original image
    // rect and we want to clip these extra parts out.
    let tight_clip_rect = metadata
        .combined_local_clip_rect
        .intersection(&metadata.local_rect).unwrap();

    let unclipped_device_rect = &metadata
        .screen_rect
        .unwrap()
        .unclipped;

    let visible_rect = compute_conservative_visible_rect(
        prim_run_context,
        frame_context,
        unclipped_device_rect,
        &tight_clip_rect
    );
    let stride = *stretch_size + *tile_spacing;

    for_each_repetition(
        &metadata.local_rect,
        &visible_rect,
        &stride,
        &mut |origin, _| {

            let mut handle = GpuCacheHandle::new();
            let rect = LayoutRect {
                origin: *origin,
                size: *stretch_size,
            };
            if let Some(request) = frame_state.gpu_cache.request(&mut handle) {
                callback(&rect, request);
            }

            visible_tiles.push(VisibleGradientTile {
                local_rect: rect,
                local_clip_rect: tight_clip_rect,
                handle
            });
        }
    );

    if visible_tiles.is_empty() {
        // At this point if we don't have tiles to show it means we could probably
        // have done a better a job at culling during an earlier stage.
        // Clearing the screen rect has the effect of "culling out" the primitive
        // from the point of view of the batch builder, and ensures we don't hit
        // assertions later on because we didn't request any image.
        metadata.screen_rect = None;
    }
}

fn compute_conservative_visible_rect(
    prim_run_context: &PrimitiveRunContext,
    frame_context: &FrameBuildingContext,
    clipped_device_rect: &DeviceIntRect,
    local_clip_rect: &LayoutRect,
) -> LayoutRect {
    let world_screen_rect = clipped_device_rect
        .to_f32() / frame_context.device_pixel_scale;

    if let Some(layer_screen_rect) = prim_run_context
        .scroll_node
        .world_content_transform
        .unapply(&world_screen_rect) {

        return local_clip_rect.intersection(&layer_screen_rect).unwrap_or(LayoutRect::zero());
    }

    *local_clip_rect
}

fn edge_flags_for_tile_spacing(tile_spacing: &LayoutSize) -> EdgeAaSegmentMask {
    let mut flags = EdgeAaSegmentMask::empty();

    if tile_spacing.width > 0.0 {
        flags |= EdgeAaSegmentMask::LEFT | EdgeAaSegmentMask::RIGHT;
    }
    if tile_spacing.height > 0.0 {
        flags |= EdgeAaSegmentMask::TOP | EdgeAaSegmentMask::BOTTOM;
    }

    flags
}

impl<'a> GpuDataRequest<'a> {
    // Write the GPU cache data for an individual segment.
    fn write_segment(
        &mut self,
        local_rect: LayoutRect,
        extra_data: [f32; 4],
    ) {
        let _ = VECS_PER_SEGMENT;
        self.push(local_rect);
        self.push(extra_data);
    }
}

fn write_brush_segment_description(
    brush: &mut BrushPrimitive,
    metadata: &PrimitiveMetadata,
    clip_chain: &ClipChainInstance,
    frame_state: &mut FrameBuildingState,
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
            if !brush.kind.supports_segments(frame_state.resource_cache) {
                return;
            }
        }
    }

    // If the brush is small, we generally want to skip building segments
    // and just draw it as a single primitive with clip mask. However,
    // if the clips are purely rectangles that have no per-fragment
    // clip masks, we will segment anyway. This allows us to completely
    // skip allocating a clip mask in these cases.
    let is_large = metadata.local_rect.size.area() > MIN_BRUSH_SPLIT_AREA;

    // TODO(gw): We should probably detect and store this on each
    //           ClipSources instance, to avoid having to iterate
    //           the clip sources here.
    let mut rect_clips_only = true;

    let mut segment_builder = SegmentBuilder::new(
        metadata.local_rect,
        None,
        metadata.local_clip_rect
    );

    // If this primitive is clipped by clips from a different coordinate system, then we
    // need to apply a clip mask for the entire primitive.
    let mut clip_mask_kind = if clip_chain.has_clips_from_other_coordinate_systems {
        BrushClipMaskKind::Global
    } else {
        BrushClipMaskKind::Individual
    };

    // Segment the primitive on all the local-space clip sources that we can.
    for i in 0 .. clip_chain.clips_range.count {
        let (clip_node, flags) = frame_state.clip_store.get_node_from_range(&clip_chain.clips_range, i);

        if !flags.contains(ClipNodeFlags::SAME_COORD_SYSTEM) {
            continue;
        }

        // TODO(gw): We can easily extend the segment builder to support these clip sources in
        // the future, but they are rarely used.
        // We must do this check here in case we continue early below.
        if clip_node.item.is_image_or_line_decoration_clip() {
            clip_mask_kind = BrushClipMaskKind::Global;
        }

        // If this clip item is positioned by another positioning node, its relative position
        // could change during scrolling. This means that we would need to resegment. Instead
        // of doing that, only segment with clips that have the same positioning node.
        // TODO(mrobinson, #2858): It may make sense to include these nodes, resegmenting only
        // when necessary while scrolling.
        if !flags.contains(ClipNodeFlags::SAME_SPATIAL_NODE) {
            // We don't need to generate a global clip mask for rectangle clips because we are
            // in the same coordinate system and rectangular clips are handled by the local
            // clip chain rectangle.
            if !clip_node.item.is_rect() {
                clip_mask_kind = BrushClipMaskKind::Global;
            }
            continue;
        }

        let (local_clip_rect, radius, mode) = match clip_node.item {
            ClipItem::RoundedRectangle(rect, radii, clip_mode) => {
                rect_clips_only = false;
                (rect, Some(radii), clip_mode)
            }
            ClipItem::Rectangle(rect, mode) => {
                (rect, None, mode)
            }
            ClipItem::BoxShadow(ref info) => {
                rect_clips_only = false;

                // For inset box shadows, we can clip out any
                // pixels that are inside the shadow region
                // and are beyond the inner rect, as they can't
                // be affected by the blur radius.
                let inner_clip_mode = match info.clip_mode {
                    BoxShadowClipMode::Outset => None,
                    BoxShadowClipMode::Inset => Some(ClipMode::ClipOut),
                };

                // Push a region into the segment builder where the
                // box-shadow can have an effect on the result. This
                // ensures clip-mask tasks get allocated for these
                // pixel regions, even if no other clips affect them.
                segment_builder.push_mask_region(
                    info.prim_shadow_rect,
                    info.prim_shadow_rect.inflate(
                        -0.5 * info.shadow_rect_alloc_size.width,
                        -0.5 * info.shadow_rect_alloc_size.height,
                    ),
                    inner_clip_mode,
                );

                continue;
            }
            ClipItem::LineDecoration(..) | ClipItem::Image(..) => {
                rect_clips_only = false;
                continue;
            }
        };

        segment_builder.push_clip_rect(local_clip_rect, radius, mode);
    }

    if is_large || rect_clips_only {
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
                            segment.rect,
                            segment.has_mask,
                            segment.edge_flags,
                            [0.0; 4],
                            BrushFlags::empty(),
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
}

impl Primitive {
    fn update_clip_task_for_brush(
        &mut self,
        prim_run_context: &PrimitiveRunContext,
        clip_chain: &ClipChainInstance,
        combined_outer_rect: &DeviceIntRect,
        pic_state: &mut PictureState,
        frame_context: &FrameBuildingContext,
        frame_state: &mut FrameBuildingState,
    ) -> bool {
        debug_assert!(frame_context.screen_rect.contains_rect(combined_outer_rect));

        let brush = match self.details {
            PrimitiveDetails::Brush(ref mut brush) => brush,
            PrimitiveDetails::TextRun(..) => return false,
        };

        write_brush_segment_description(
            brush,
            &self.metadata,
            clip_chain,
            frame_state,
        );

        let segment_desc = match brush.segment_desc {
            Some(ref mut description) => description,
            None => return false,
        };
        let clip_mask_kind = segment_desc.clip_mask_kind;

        for segment in &mut segment_desc.segments {
            if !segment.may_need_clip_mask && clip_mask_kind != BrushClipMaskKind::Global {
                segment.clip_task_id = BrushSegmentTaskId::Opaque;
                continue;
            }

            let intersected_rect = calculate_screen_bounding_rect(
                &prim_run_context.scroll_node.world_content_transform,
                &segment.local_rect,
                frame_context.device_pixel_scale,
                Some(&combined_outer_rect),
            );

            let bounds = match intersected_rect {
                Some(bounds) => bounds,
                None => {
                    segment.clip_task_id = BrushSegmentTaskId::Empty;
                    continue;
                }
            };

            if clip_chain.clips_range.count > 0 {
                let clip_task = RenderTask::new_mask(
                    bounds,
                    clip_chain.clips_range,
                    frame_state.clip_store,
                    frame_state.gpu_cache,
                    frame_state.resource_cache,
                    frame_state.render_tasks,
                );

                let clip_task_id = frame_state.render_tasks.add(clip_task);
                pic_state.tasks.push(clip_task_id);
                segment.clip_task_id = BrushSegmentTaskId::RenderTaskId(clip_task_id);
            }
        }

        true
    }

    fn reset_clip_task(&mut self) {
        self.metadata.clip_task_id = None;
        if let PrimitiveDetails::Brush(ref mut brush) = self.details {
            if let Some(ref mut desc) = brush.segment_desc {
                for segment in &mut desc.segments {
                    segment.clip_task_id = BrushSegmentTaskId::Opaque;
                }
            }
        }
    }

    fn prepare_prim_for_render_inner(
        &mut self,
        prim_index: PrimitiveIndex,
        prim_run_context: &PrimitiveRunContext,
        pic_state_for_children: PictureState,
        pic_context: &PictureContext,
        pic_state: &mut PictureState,
        frame_context: &FrameBuildingContext,
        frame_state: &mut FrameBuildingState,
        display_list: &BuiltDisplayList,
        is_chased: bool,
    ) {
        let mut is_tiled = false;
        let metadata = &mut self.metadata;
        #[cfg(debug_assertions)]
        {
            metadata.prepared_frame_id = frame_state.render_tasks.frame_id();
        }

        match self.details {
            PrimitiveDetails::TextRun(ref mut text) => {
                // The transform only makes sense for screen space rasterization
                let transform = prim_run_context.scroll_node.world_content_transform.to_transform();
                text.prepare_for_render(
                    frame_context.device_pixel_scale,
                    &transform,
                    pic_context.allow_subpixel_aa,
                    display_list,
                    frame_state,
                );
            }
            PrimitiveDetails::Brush(ref mut brush) => {
                match brush.kind {
                    BrushKind::Image {
                        request,
                        sub_rect,
                        stretch_size,
                        ref mut tile_spacing,
                        ref mut source,
                        ref mut opacity_binding,
                        ref mut visible_tiles,
                        ..
                    } => {
                        let image_properties = frame_state
                            .resource_cache
                            .get_image_properties(request.key);


                        // Set if we need to request the source image from the cache this frame.
                        if let Some(image_properties) = image_properties {
                            is_tiled = image_properties.tiling.is_some();

                            // If the opacity changed, invalidate the GPU cache so that
                            // the new color for this primitive gets uploaded.
                            if opacity_binding.update(frame_context.scene_properties) {
                                frame_state.gpu_cache.invalidate(&mut metadata.gpu_location);
                            }

                            // Update opacity for this primitive to ensure the correct
                            // batching parameters are used.
                            metadata.opacity.is_opaque =
                                image_properties.descriptor.is_opaque &&
                                opacity_binding.current == 1.0;

                            if *tile_spacing != LayoutSize::zero() && !is_tiled {
                                *source = ImageSource::Cache {
                                    // Size in device-pixels we need to allocate in render task cache.
                                    size: image_properties.descriptor.size.to_i32(),
                                    handle: None,
                                };
                            }

                            // Work out whether this image is a normal / simple type, or if
                            // we need to pre-render it to the render task cache.
                            if let Some(rect) = sub_rect {
                                // We don't properly support this right now.
                                debug_assert!(!is_tiled);
                                *source = ImageSource::Cache {
                                    // Size in device-pixels we need to allocate in render task cache.
                                    size: rect.size,
                                    handle: None,
                                };
                            }

                            let mut request_source_image = false;

                            // Every frame, for cached items, we need to request the render
                            // task cache item. The closure will be invoked on the first
                            // time through, and any time the render task output has been
                            // evicted from the texture cache.
                            match *source {
                                ImageSource::Cache { ref mut size, ref mut handle } => {
                                    let padding = DeviceIntSideOffsets::new(
                                        0,
                                        (tile_spacing.width * size.width as f32 / stretch_size.width) as i32,
                                        (tile_spacing.height * size.height as f32 / stretch_size.height) as i32,
                                        0,
                                    );

                                    let inner_size = *size;
                                    size.width += padding.horizontal();
                                    size.height += padding.vertical();

                                    if padding != DeviceIntSideOffsets::zero() {
                                        metadata.opacity.is_opaque = false;
                                    }

                                    let image_cache_key = ImageCacheKey {
                                        request,
                                        texel_rect: sub_rect,
                                    };

                                    // Request a pre-rendered image task.
                                    *handle = Some(frame_state.resource_cache.request_render_task(
                                        RenderTaskCacheKey {
                                            size: *size,
                                            kind: RenderTaskCacheKeyKind::Image(image_cache_key),
                                        },
                                        frame_state.gpu_cache,
                                        frame_state.render_tasks,
                                        None,
                                        image_properties.descriptor.is_opaque,
                                        |render_tasks| {
                                            // We need to render the image cache this frame,
                                            // so will need access to the source texture.
                                            request_source_image = true;

                                            // Create a task to blit from the texture cache to
                                            // a normal transient render task surface. This will
                                            // copy only the sub-rect, if specified.
                                            let cache_to_target_task = RenderTask::new_blit_with_padding(
                                                inner_size,
                                                &padding,
                                                BlitSource::Image { key: image_cache_key },
                                            );
                                            let cache_to_target_task_id = render_tasks.add(cache_to_target_task);

                                            // Create a task to blit the rect from the child render
                                            // task above back into the right spot in the persistent
                                            // render target cache.
                                            let target_to_cache_task = RenderTask::new_blit(
                                                *size,
                                                BlitSource::RenderTask {
                                                    task_id: cache_to_target_task_id,
                                                },
                                            );
                                            let target_to_cache_task_id = render_tasks.add(target_to_cache_task);

                                            // Hook this into the render task tree at the right spot.
                                            pic_state.tasks.push(target_to_cache_task_id);

                                            // Pass the image opacity, so that the cached render task
                                            // item inherits the same opacity properties.
                                            target_to_cache_task_id
                                        }
                                    ));
                                }
                                ImageSource::Default => {
                                    // Normal images just reference the source texture each frame.
                                    request_source_image = true;
                                }
                            }

                            if let Some(tile_size) = image_properties.tiling {

                                let device_image_size = image_properties.descriptor.size;

                                // Tighten the clip rect because decomposing the repeated image can
                                // produce primitives that are partially covering the original image
                                // rect and we want to clip these extra parts out.
                                let tight_clip_rect = metadata
                                    .combined_local_clip_rect
                                    .intersection(&metadata.local_rect).unwrap();

                                let visible_rect = compute_conservative_visible_rect(
                                    prim_run_context,
                                    frame_context,
                                    &metadata.screen_rect.unwrap().clipped,
                                    &tight_clip_rect
                                );

                                let base_edge_flags = edge_flags_for_tile_spacing(tile_spacing);

                                let stride = stretch_size + *tile_spacing;

                                visible_tiles.clear();

                                for_each_repetition(
                                    &metadata.local_rect,
                                    &visible_rect,
                                    &stride,
                                    &mut |origin, edge_flags| {
                                        let edge_flags = base_edge_flags | edge_flags;

                                        let image_rect = LayoutRect {
                                            origin: *origin,
                                            size: stretch_size,
                                        };

                                        for_each_tile(
                                            &image_rect,
                                            &visible_rect,
                                            &device_image_size,
                                            tile_size as u32,
                                            &mut |tile_rect, tile_offset, tile_flags| {

                                                frame_state.resource_cache.request_image(
                                                    request.with_tile(tile_offset),
                                                    frame_state.gpu_cache,
                                                );

                                                let mut handle = GpuCacheHandle::new();
                                                if let Some(mut request) = frame_state.gpu_cache.request(&mut handle) {
                                                    request.push(ColorF::new(1.0, 1.0, 1.0, opacity_binding.current).premultiplied());
                                                    request.push(PremultipliedColorF::WHITE);
                                                    request.push([tile_rect.size.width, tile_rect.size.height, 0.0, 0.0]);
                                                    request.write_segment(*tile_rect, [0.0; 4]);
                                                }

                                                visible_tiles.push(VisibleImageTile {
                                                    tile_offset,
                                                    handle,
                                                    edge_flags: tile_flags & edge_flags,
                                                    local_rect: *tile_rect,
                                                    local_clip_rect: tight_clip_rect,
                                                });
                                            }
                                        );
                                    }
                                );

                                if visible_tiles.is_empty() {
                                    // At this point if we don't have tiles to show it means we could probably
                                    // have done a better a job at culling during an earlier stage.
                                    // Clearing the screen rect has the effect of "culling out" the primitive
                                    // from the point of view of the batch builder, and ensures we don't hit
                                    // assertions later on because we didn't request any image.
                                    metadata.screen_rect = None;
                                }
                            } else if request_source_image {
                                frame_state.resource_cache.request_image(
                                    request,
                                    frame_state.gpu_cache,
                                );
                            }
                        }
                    }
                    BrushKind::YuvImage { format, yuv_key, image_rendering, .. } => {
                        let channel_num = format.get_plane_num();
                        debug_assert!(channel_num <= 3);
                        for channel in 0 .. channel_num {
                            frame_state.resource_cache.request_image(
                                ImageRequest {
                                    key: yuv_key[channel],
                                    rendering: image_rendering,
                                    tile: None,
                                },
                                frame_state.gpu_cache,
                            );
                        }
                    }
                    BrushKind::Border { ref mut source, .. } => {
                        match *source {
                            BorderSource::Image(request) => {
                                let image_properties = frame_state
                                    .resource_cache
                                    .get_image_properties(request.key);

                                if let Some(image_properties) = image_properties {
                                    // Update opacity for this primitive to ensure the correct
                                    // batching parameters are used.
                                    metadata.opacity.is_opaque =
                                        image_properties.descriptor.is_opaque;

                                    frame_state.resource_cache.request_image(
                                        request,
                                        frame_state.gpu_cache,
                                    );
                                }
                            }
                            BorderSource::Border { .. } => {
                                // Handled earlier since we need to update the segment
                                // descriptor *before* update_clip_task() is called.
                            }
                        }
                    }
                    BrushKind::RadialGradient {
                        stops_range,
                        center,
                        start_radius,
                        end_radius,
                        ratio_xy,
                        extend_mode,
                        stretch_size,
                        tile_spacing,
                        ref mut stops_handle,
                        ref mut visible_tiles,
                        ..
                    } => {
                        build_gradient_stops_request(
                            stops_handle,
                            stops_range,
                            false,
                            frame_state,
                            display_list,
                        );

                        if tile_spacing != LayoutSize::zero() {
                            is_tiled = true;

                            decompose_repeated_primitive(
                                visible_tiles,
                                metadata,
                                &stretch_size,
                                &tile_spacing,
                                prim_run_context,
                                frame_context,
                                frame_state,
                                &mut |rect, mut request| {
                                    request.push([
                                        center.x,
                                        center.y,
                                        start_radius,
                                        end_radius,
                                    ]);
                                    request.push([
                                        ratio_xy,
                                        pack_as_float(extend_mode as u32),
                                        stretch_size.width,
                                        stretch_size.height,
                                    ]);
                                    request.write_segment(*rect, [0.0; 4]);
                                },
                            );
                        }
                    }
                    BrushKind::LinearGradient {
                        stops_range,
                        reverse_stops,
                        start_point,
                        end_point,
                        extend_mode,
                        stretch_size,
                        tile_spacing,
                        ref mut stops_handle,
                        ref mut visible_tiles,
                        ..
                    } => {

                        build_gradient_stops_request(
                            stops_handle,
                            stops_range,
                            reverse_stops,
                            frame_state,
                            display_list,
                        );

                        if tile_spacing != LayoutSize::zero() {
                            is_tiled = true;

                            decompose_repeated_primitive(
                                visible_tiles,
                                metadata,
                                &stretch_size,
                                &tile_spacing,
                                prim_run_context,
                                frame_context,
                                frame_state,
                                &mut |rect, mut request| {
                                    request.push([
                                        start_point.x,
                                        start_point.y,
                                        end_point.x,
                                        end_point.y,
                                    ]);
                                    request.push([
                                        pack_as_float(extend_mode as u32),
                                        stretch_size.width,
                                        stretch_size.height,
                                        0.0,
                                    ]);
                                    request.write_segment(*rect, [0.0; 4]);
                                }
                            );
                        }
                    }
                    BrushKind::Picture(ref mut pic) => {
                        pic.prepare_for_render(
                            prim_index,
                            metadata,
                            prim_run_context,
                            pic_state_for_children,
                            pic_state,
                            frame_context,
                            frame_state,
                        );
                    }
                    BrushKind::Solid { ref color, ref mut opacity_binding, .. } => {
                        // If the opacity changed, invalidate the GPU cache so that
                        // the new color for this primitive gets uploaded. Also update
                        // the opacity field that controls which batches this primitive
                        // will be added to.
                        if opacity_binding.update(frame_context.scene_properties) {
                            metadata.opacity = PrimitiveOpacity::from_alpha(opacity_binding.current * color.a);
                            frame_state.gpu_cache.invalidate(&mut metadata.gpu_location);
                        }
                    }
                    BrushKind::Clear => {}
                }
            }
        }

        if is_tiled {
            // we already requested each tile's gpu data.
            return;
        }

        // Mark this GPU resource as required for this frame.
        if let Some(mut request) = frame_state.gpu_cache.request(&mut metadata.gpu_location) {
            match self.details {
                PrimitiveDetails::TextRun(ref mut text) => {
                    text.write_gpu_blocks(&mut request);
                }
                PrimitiveDetails::Brush(ref mut brush) => {
                    brush.write_gpu_blocks(&mut request, metadata.local_rect);

                    match brush.segment_desc {
                        Some(ref segment_desc) => {
                            for segment in &segment_desc.segments {
                                if cfg!(debug_assertions) && is_chased {
                                    println!("\t\t{:?}", segment);
                                }
                                // has to match VECS_PER_SEGMENT
                                request.write_segment(
                                    segment.local_rect,
                                    segment.extra_data,
                                );
                            }
                        }
                        None => {
                            request.write_segment(
                                metadata.local_rect,
                                [0.0; 4],
                            );
                        }
                    }
                }
            }
        }
    }

    fn update_clip_task(
        &mut self,
        prim_run_context: &PrimitiveRunContext,
        prim_screen_rect: &DeviceIntRect,
        clip_chain: &ClipChainInstance,
        pic_state: &mut PictureState,
        frame_context: &FrameBuildingContext,
        frame_state: &mut FrameBuildingState,
        is_chased: bool,
    ) -> bool {
        if cfg!(debug_assertions) && is_chased {
            println!("\tupdating clip task with screen rect {:?}", prim_screen_rect);
        }
        // Reset clips from previous frames since we may clip differently each frame.
        self.reset_clip_task();

        // First try to  render this primitive's mask using optimized brush rendering.
        if self.update_clip_task_for_brush(
            prim_run_context,
            &clip_chain,
            prim_screen_rect,
            pic_state,
            frame_context,
            frame_state,
        ) {
            if cfg!(debug_assertions) && is_chased {
                println!("\tsegment tasks have been created for clipping");
            }
            return true;
        }

        if clip_chain.clips_range.count > 0 {
            let clip_task = RenderTask::new_mask(
                *prim_screen_rect,
                clip_chain.clips_range,
                frame_state.clip_store,
                frame_state.gpu_cache,
                frame_state.resource_cache,
                frame_state.render_tasks,
            );

            let clip_task_id = frame_state.render_tasks.add(clip_task);
            if cfg!(debug_assertions) && is_chased {
                println!("\tcreated task {:?} with combined outer rect {:?}",
                    clip_task_id, prim_screen_rect);
            }
            self.metadata.clip_task_id = Some(clip_task_id);
            pic_state.tasks.push(clip_task_id);
        }

        true
    }

    fn build_prim_segments_if_needed(
        &mut self,
        pic_state: &mut PictureState,
        frame_state: &mut FrameBuildingState,
        frame_context: &FrameBuildingContext,
    ) {
        let brush = match self.details {
            PrimitiveDetails::Brush(ref mut brush) => brush,
            PrimitiveDetails::TextRun(..) => return,
        };

        if let BrushKind::Border {
            source: BorderSource::Border {
                ref border,
                ref mut cache_key,
                ref widths,
                ref mut handle,
                ref mut task_info,
                ..
            }
        } = brush.kind {
            // TODO(gw): When drawing in screen raster mode, we should also incorporate a
            //           scale factor from the world transform to get an appropriately
            //           sized border task.
            let world_scale = LayoutToWorldScale::new(1.0);
            let mut scale = world_scale * frame_context.device_pixel_scale;
            let max_scale = BorderRenderTaskInfo::get_max_scale(&border.radius, &widths);
            scale.0 = scale.0.min(max_scale.0);
            let scale_au = Au::from_f32_px(scale.0);
            let needs_update = scale_au != cache_key.scale;
            let mut new_segments = Vec::new();

            if needs_update {
                cache_key.scale = scale_au;

                *task_info = BorderRenderTaskInfo::new(
                    &self.metadata.local_rect,
                    border,
                    widths,
                    scale,
                    &mut new_segments,
                );
            }

            *handle = task_info.as_ref().map(|task_info| {
                frame_state.resource_cache.request_render_task(
                    RenderTaskCacheKey {
                        size: DeviceIntSize::zero(),
                        kind: RenderTaskCacheKeyKind::Border(cache_key.clone()),
                    },
                    frame_state.gpu_cache,
                    frame_state.render_tasks,
                    None,
                    false,          // todo
                    |render_tasks| {
                        let task = RenderTask::new_border(
                            task_info.size,
                            task_info.build_instances(border),
                        );

                        let task_id = render_tasks.add(task);

                        pic_state.tasks.push(task_id);

                        task_id
                    }
                )
            });

            if needs_update {
                brush.segment_desc = Some(BrushSegmentDescriptor {
                    segments: new_segments,
                    clip_mask_kind: BrushClipMaskKind::Unknown,
                });

                // The segments have changed, so force the GPU cache to
                // re-upload the primitive information.
                frame_state.gpu_cache.invalidate(&mut self.metadata.gpu_location);
            }
        }
    }
}
