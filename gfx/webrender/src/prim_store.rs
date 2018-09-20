/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{AlphaType, BorderRadius, BuiltDisplayList, ClipMode, ColorF, PictureRect};
use api::{DeviceIntRect, DeviceIntSize, DevicePixelScale, ExtendMode, DeviceRect, PictureToRasterTransform};
use api::{FilterOp, GlyphInstance, GradientStop, ImageKey, ImageRendering, ItemRange, ItemTag, TileOffset};
use api::{GlyphRasterSpace, LayoutPoint, LayoutRect, LayoutSideOffsets, LayoutSize, LayoutToWorldTransform};
use api::{LayoutVector2D, PremultipliedColorF, PropertyBinding, Shadow, YuvColorSpace, YuvFormat};
use api::{DeviceIntSideOffsets, WorldPixel, BoxShadowClipMode, LayoutToWorldScale, NormalBorder, WorldRect};
use api::{PicturePixel, RasterPixel};
use app_units::Au;
use border::{BorderCacheKey, BorderRenderTaskInfo};
use clip_scroll_tree::{ClipScrollTree, CoordinateSystemId, SpatialNodeIndex};
use clip::{ClipNodeFlags, ClipChainId, ClipChainInstance, ClipItem, ClipNodeCollector};
use euclid::{TypedTransform3D, TypedRect};
use frame_builder::{FrameBuildingContext, FrameBuildingState, PictureContext, PictureState};
use frame_builder::PrimitiveContext;
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
use std::{cmp, fmt, mem, usize};
use util::{ScaleOffset, MatrixHelpers, pack_as_float, project_rect, raster_rect_to_device_pixels};


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

#[derive(Debug)]
pub struct PrimitiveRun {
    pub base_prim_index: PrimitiveIndex,
    pub count: usize,
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
pub enum CoordinateSpaceMapping<F, T> {
    Local,
    ScaleOffset(ScaleOffset),
    Transform(TypedTransform3D<f32, F, T>),
}

#[derive(Debug)]
pub struct SpaceMapper<F, T> {
    kind: CoordinateSpaceMapping<F, T>,
    pub ref_spatial_node_index: SpatialNodeIndex,
    pub current_target_spatial_node_index: SpatialNodeIndex,
    pub bounds: TypedRect<f32, T>,
}

impl<F, T> SpaceMapper<F, T> where F: fmt::Debug {
    pub fn new(
        ref_spatial_node_index: SpatialNodeIndex,
        bounds: TypedRect<f32, T>,
    ) -> Self {
        SpaceMapper {
            kind: CoordinateSpaceMapping::Local,
            ref_spatial_node_index,
            current_target_spatial_node_index: ref_spatial_node_index,
            bounds,
        }
    }

    pub fn new_with_target(
        ref_spatial_node_index: SpatialNodeIndex,
        target_node_index: SpatialNodeIndex,
        bounds: TypedRect<f32, T>,
        clip_scroll_tree: &ClipScrollTree,
    ) -> Self {
        let mut mapper = SpaceMapper::new(ref_spatial_node_index, bounds);
        mapper.set_target_spatial_node(target_node_index, clip_scroll_tree);
        mapper
    }

    pub fn set_target_spatial_node(
        &mut self,
        target_node_index: SpatialNodeIndex,
        clip_scroll_tree: &ClipScrollTree,
    ) {
        if target_node_index != self.current_target_spatial_node_index {
            let spatial_nodes = &clip_scroll_tree.spatial_nodes;
            let ref_spatial_node = &spatial_nodes[self.ref_spatial_node_index.0];
            let target_spatial_node = &spatial_nodes[target_node_index.0];
            self.current_target_spatial_node_index = target_node_index;

            self.kind = if self.ref_spatial_node_index == target_node_index {
                CoordinateSpaceMapping::Local
            } else if ref_spatial_node.coordinate_system_id == target_spatial_node.coordinate_system_id {
                CoordinateSpaceMapping::ScaleOffset(
                    ref_spatial_node.coordinate_system_relative_scale_offset
                        .difference(
                            &target_spatial_node.coordinate_system_relative_scale_offset
                        )
                )
            } else {
                let transform = clip_scroll_tree.get_relative_transform(
                    target_node_index,
                    self.ref_spatial_node_index,
                ).expect("bug: should have already been culled");

                CoordinateSpaceMapping::Transform(
                    transform.with_source::<F>().with_destination::<T>()
                )
            };
        }
    }

    pub fn get_transform(&self) -> TypedTransform3D<f32, F, T> {
        match self.kind {
            CoordinateSpaceMapping::Local => {
                TypedTransform3D::identity()
            }
            CoordinateSpaceMapping::ScaleOffset(ref scale_offset) => {
                scale_offset.to_transform()
            }
            CoordinateSpaceMapping::Transform(transform) => {
                transform
            }
        }
    }

    pub fn unmap(&self, rect: &TypedRect<f32, T>) -> Option<TypedRect<f32, F>> {
        match self.kind {
            CoordinateSpaceMapping::Local => {
                Some(TypedRect::from_untyped(&rect.to_untyped()))
            }
            CoordinateSpaceMapping::ScaleOffset(ref scale_offset) => {
                Some(scale_offset.unmap_rect(rect))
            }
            CoordinateSpaceMapping::Transform(ref transform) => {
                transform.inverse_rect_footprint(rect)
            }
        }
    }

    pub fn map(&self, rect: &TypedRect<f32, F>) -> Option<TypedRect<f32, T>> {
        match self.kind {
            CoordinateSpaceMapping::Local => {
                Some(TypedRect::from_untyped(&rect.to_untyped()))
            }
            CoordinateSpaceMapping::ScaleOffset(ref scale_offset) => {
                Some(scale_offset.map_rect(rect))
            }
            CoordinateSpaceMapping::Transform(ref transform) => {
                match project_rect(transform, rect, &self.bounds) {
                    Some(bounds) => {
                        Some(bounds)
                    }
                    None => {
                        warn!("parent relative transform can't transform the primitive rect for {:?}", rect);
                        None
                    }
                }
            }
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
/// texture data and update the UV rect. Any filtering
/// is handled externally for NativeTexture external
/// images.
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct DeferredResolve {
    pub address: GpuCacheAddress,
    pub image_properties: ImageProperties,
    pub rendering: ImageRendering,
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

// TODO(gw): Pack the fields here better!
#[derive(Debug)]
pub struct PrimitiveMetadata {
    pub opacity: PrimitiveOpacity,
    pub clip_chain_id: ClipChainId,
    pub spatial_node_index: SpatialNodeIndex,
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
    pub clipped_world_rect: Option<WorldRect>,

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
        widths: LayoutSideOffsets,
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
        color: ColorF,
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

    // Construct a brush that is a border with `border` style and `widths`
    // dimensions.
    pub fn new_border(mut border: NormalBorder, widths: LayoutSideOffsets) -> BrushKind {
        // FIXME(emilio): Is this the best place to do this?
        border.normalize(&widths);

        let cache_key = BorderCacheKey::new(&border, &widths);
        BrushKind::Border {
            source: BorderSource::Border {
                border,
                widths,
                cache_key,
                task_info: None,
                handle: None,
            }
        }
    }

    // Construct a brush that is an image wisth `stretch_size` dimensions and
    // `color`.
    pub fn new_image(
        request: ImageRequest,
        stretch_size: LayoutSize,
        color: ColorF
    ) -> BrushKind {
        BrushKind::Image {
            request,
            alpha_type: AlphaType::PremultipliedAlpha,
            stretch_size,
            tile_spacing: LayoutSize::new(0., 0.),
            color,
            source: ImageSource::Default,
            sub_rect: None,
            opacity_binding: OpacityBinding::new(),
            visible_tiles: Vec::new(),
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
        local_rect: LayoutRect,
        may_need_clip_mask: bool,
        edge_flags: EdgeAaSegmentMask,
        extra_data: [f32; 4],
        brush_flags: BrushFlags,
    ) -> Self {
        Self {
            local_rect,
            clip_task_id: BrushSegmentTaskId::Opaque,
            may_need_clip_mask,
            edge_flags,
            extra_data,
            brush_flags,
        }
    }
}

#[derive(Debug)]
pub struct BrushSegmentDescriptor {
    pub segments: Vec<BrushSegment>,
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

    pub fn may_need_clip_mask(&self) -> bool {
        match self.kind {
            BrushKind::Picture(ref pic) => {
                pic.raster_config.is_some()
            }
            _ => {
                true
            }
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
            BrushKind::Image { stretch_size, tile_spacing, color, ref opacity_binding, .. } => {
                request.push(color.scale_alpha(opacity_binding.current).premultiplied());
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
                    BrushKind::Border { ref source } => {
                        let source = match *source {
                            BorderSource::Image(request) => {
                                BrushKind::Border {
                                    source: BorderSource::Image(request)
                                }
                            },
                            BorderSource::Border { border, widths, .. } => {
                                let border = border.with_color(shadow.color);
                                BrushKind::new_border(border, widths)

                            }
                        };
                        PrimitiveContainer::Brush(BrushPrimitive::new(
                            source,
                            None,
                        ))
                    }
                    BrushKind::Image { request, stretch_size, .. } => {
                        PrimitiveContainer::Brush(BrushPrimitive::new(
                            BrushKind::new_image(request.clone(),
                                                 stretch_size.clone(),
                                                 shadow.color),
                            None,
                        ))
                    }
                    BrushKind::Clear |
                    BrushKind::Picture { .. } |
                    BrushKind::YuvImage { .. } |
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
        spatial_node_index: SpatialNodeIndex,
        tag: Option<ItemTag>,
        container: PrimitiveContainer,
    ) -> PrimitiveIndex {
        let prim_index = self.primitives.len();

        let base_metadata = PrimitiveMetadata {
            clip_chain_id,
            gpu_location: GpuCacheHandle::new(),
            clip_task_id: None,
            spatial_node_index,
            local_rect: *local_rect,
            local_clip_rect: *local_clip_rect,
            combined_local_clip_rect: *local_clip_rect,
            is_backface_visible,
            clipped_world_rect: None,
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
                        if pic.requested_composite_mode.is_none() {
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
        let binding = match self.get_pic(pic_prim_index).requested_composite_mode {
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
        self.get_pic_mut(pic_prim_index).requested_composite_mode = None;
    }

    pub fn prim_count(&self) -> usize {
        self.primitives.len()
    }

    pub fn prepare_prim_for_render(
        &mut self,
        prim_index: PrimitiveIndex,
        prim_context: &PrimitiveContext,
        pic_context: &PictureContext,
        pic_state: &mut PictureState,
        frame_context: &FrameBuildingContext,
        frame_state: &mut FrameBuildingState,
        display_list: &BuiltDisplayList,
        is_chased: bool,
        current_pic_rect: &mut PictureRect,
    ) -> bool {
        // If we have dependencies, we need to prepare them first, in order
        // to know the actual rect of this primitive.
        // For example, scrolling may affect the location of an item in
        // local space, which may force us to render this item on a larger
        // picture target, if being composited.
        let pic_info = {
            match self.primitives[prim_index.0].details {
                PrimitiveDetails::Brush(BrushPrimitive { kind: BrushKind::Picture(ref mut pic), .. }) => {
                    match pic.take_context(
                        prim_context,
                        pic_state.surface_spatial_node_index,
                        pic_state.raster_spatial_node_index,
                        pic_context.allow_subpixel_aa,
                        frame_state,
                        frame_context,
                        is_chased,
                    ) {
                        Some(info) => Some(info),
                        None => return false,
                    }
                }
                PrimitiveDetails::Brush(_) |
                PrimitiveDetails::TextRun(..) => {
                    None
                }
            }
        };

        let (is_passthrough, clip_node_collector) = match pic_info {
            Some((pic_context_for_children, mut pic_state_for_children)) => {
                // Mark whether this picture has a complex coordinate system.
                let is_passthrough = pic_context_for_children.is_passthrough;
                pic_state_for_children.has_non_root_coord_system |=
                    prim_context.spatial_node.coordinate_system_id != CoordinateSystemId::root();

                let mut pic_rect = PictureRect::zero();
                self.prepare_prim_runs(
                    &pic_context_for_children,
                    &mut pic_state_for_children,
                    frame_context,
                    frame_state,
                    &mut pic_rect,
                );

                let pic_rect = if is_passthrough {
                    *current_pic_rect = current_pic_rect.union(&pic_rect);
                    None
                } else {
                    Some(pic_rect)
                };

                if !pic_state_for_children.is_cacheable {
                  pic_state.is_cacheable = false;
                }

                // Restore the dependencies (borrow check dance)
                let prim = &mut self.primitives[prim_index.0];
                let (new_local_rect, clip_node_collector) = prim
                    .as_pic_mut()
                    .restore_context(
                        pic_context_for_children,
                        pic_state_for_children,
                        pic_rect,
                        frame_state,
                    );

                if new_local_rect != prim.metadata.local_rect {
                    prim.metadata.local_rect = new_local_rect;
                    frame_state.gpu_cache.invalidate(&mut prim.metadata.gpu_location);
                    pic_state.local_rect_changed = true;
                }

                (is_passthrough, clip_node_collector)
            }
            None => {
                (false, None)
            }
        };

        let prim = &mut self.primitives[prim_index.0];

        if !prim.is_cacheable(frame_state.resource_cache) {
            pic_state.is_cacheable = false;
        }

        if is_passthrough {
            prim.metadata.clipped_world_rect = Some(pic_state.map_pic_to_world.bounds);
        } else {
            if prim.metadata.local_rect.size.width <= 0.0 ||
               prim.metadata.local_rect.size.height <= 0.0 {
                if cfg!(debug_assertions) && is_chased {
                    println!("\tculled for zero local rectangle");
                }
                return false;
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
                    return false;
                }
            };

            let clip_chain = frame_state
                .clip_store
                .build_clip_chain_instance(
                    prim.metadata.clip_chain_id,
                    local_rect,
                    prim.metadata.local_clip_rect,
                    prim_context.spatial_node_index,
                    &pic_state.map_local_to_pic,
                    &pic_state.map_pic_to_world,
                    &frame_context.clip_scroll_tree,
                    frame_state.gpu_cache,
                    frame_state.resource_cache,
                    frame_context.device_pixel_scale,
                    &frame_context.world_rect,
                    &clip_node_collector,
                    frame_state.clip_data_store,
                );

            let clip_chain = match clip_chain {
                Some(clip_chain) => clip_chain,
                None => {
                    prim.metadata.clipped_world_rect = None;
                    return false;
                }
            };

            if cfg!(debug_assertions) && is_chased {
                println!("\teffective clip chain from {:?} {}",
                    clip_chain.clips_range,
                    if pic_context.apply_local_clip_rect { "(applied)" } else { "" },
                );
            }

            pic_state.has_non_root_coord_system |= clip_chain.has_non_root_coord_system;

            prim.metadata.combined_local_clip_rect = if pic_context.apply_local_clip_rect {
                clip_chain.local_clip_rect
            } else {
                prim.metadata.local_clip_rect
            };

            let pic_rect = match pic_state.map_local_to_pic
                                          .map(&prim.metadata.local_rect) {
                Some(pic_rect) => pic_rect,
                None => return false,
            };

            // Check if the clip bounding rect (in pic space) is visible on screen
            // This includes both the prim bounding rect + local prim clip rect!
            let world_rect = match pic_state.map_pic_to_world
                                            .map(&clip_chain.pic_clip_rect) {
                Some(world_rect) => world_rect,
                None => {
                    return false;
                }
            };

            let clipped_world_rect = match world_rect.intersection(&frame_context.world_rect) {
                Some(rect) => rect,
                None => {
                    return false;
                }
            };

            prim.metadata.clipped_world_rect = Some(clipped_world_rect);

            prim.build_prim_segments_if_needed(
                pic_state,
                frame_state,
                frame_context,
            );

            prim.update_clip_task(
                prim_context,
                clipped_world_rect,
                pic_state.raster_spatial_node_index,
                &clip_chain,
                pic_state,
                frame_context,
                frame_state,
                is_chased,
                &clip_node_collector,
            );

            if cfg!(debug_assertions) && is_chased {
                println!("\tconsidered visible and ready with local rect {:?}", local_rect);
            }

            *current_pic_rect = current_pic_rect.union(&pic_rect);
        }

        prim.prepare_prim_for_render_inner(
            prim_index,
            prim_context,
            pic_context,
            pic_state,
            frame_context,
            frame_state,
            display_list,
            is_chased,
        );

        true
    }

    // TODO(gw): Make this simpler / more efficient by tidying
    //           up the logic that early outs from prepare_prim_for_render.
    pub fn reset_prim_visibility(&mut self) {
        for prim in &mut self.primitives {
            prim.metadata.clipped_world_rect = None;
        }
    }

    pub fn prepare_prim_runs(
        &mut self,
        pic_context: &PictureContext,
        pic_state: &mut PictureState,
        frame_context: &FrameBuildingContext,
        frame_state: &mut FrameBuildingState,
        current_pic_rect: &mut PictureRect,
    ) {
        let display_list = &frame_context
            .pipelines
            .get(&pic_context.pipeline_id)
            .expect("No display list?")
            .display_list;

        for run in &pic_context.prim_runs {
            if run.is_chasing(self.chase_id) {
                println!("\tpreparing a run of length {} in pipeline {:?}",
                    run.count, pic_context.pipeline_id);
            }

            for i in 0 .. run.count {
                let prim_index = PrimitiveIndex(run.base_prim_index.0 + i);
                let is_chased = Some(prim_index) == self.chase_id;

                // TODO(gw): These workarounds for borrowck are unfortunate. We
                //           should see if we can re-structure these to avoid so
                //           many special borrow blocks.
                let (spatial_node_index, is_backface_visible) = {
                    let prim = &self.primitives[prim_index.0];
                    (prim.metadata.spatial_node_index, prim.metadata.is_backface_visible)
                };

                let spatial_node = &frame_context
                    .clip_scroll_tree
                    .spatial_nodes[spatial_node_index.0];

                // TODO(gw): Although constructing these is cheap, they are often
                //           the same for many consecutive primitives, so it may
                //           be worth caching the most recent context.
                let prim_context = PrimitiveContext::new(
                    spatial_node,
                    spatial_node_index,
                );

                // Do some basic checks first, that can early out
                // without even knowing the local rect.
                if !is_backface_visible && spatial_node.world_content_transform.is_backface_visible() {
                    if cfg!(debug_assertions) && is_chased {
                        println!("\tculled for not having visible back faces");
                    }
                    continue;
                }

                if !spatial_node.invertible {
                    if cfg!(debug_assertions) && is_chased {
                        println!("\tculled for the scroll node transform being invertible");
                    }
                    continue;
                }

                // Mark whether this picture contains any complex coordinate
                // systems, due to either the scroll node or the clip-chain.
                pic_state.has_non_root_coord_system |=
                    spatial_node.coordinate_system_id != CoordinateSystemId::root();

                pic_state.map_local_to_pic.set_target_spatial_node(
                    spatial_node_index,
                    frame_context.clip_scroll_tree,
                );

                if self.prepare_prim_for_render(
                    prim_index,
                    &prim_context,
                    pic_context,
                    pic_state,
                    frame_context,
                    frame_state,
                    display_list,
                    is_chased,
                    current_pic_rect,
                ) {
                    frame_state.profile_counters.visible_primitives.inc();
                }
            }
        }
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
    prim_context: &PrimitiveContext,
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

    let clipped_world_rect = &metadata
        .clipped_world_rect
        .unwrap();

    let visible_rect = compute_conservative_visible_rect(
        prim_context,
        clipped_world_rect,
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
        metadata.clipped_world_rect = None;
    }
}

fn compute_conservative_visible_rect(
    prim_context: &PrimitiveContext,
    clipped_world_rect: &WorldRect,
    local_clip_rect: &LayoutRect,
) -> LayoutRect {
    if let Some(layer_screen_rect) = prim_context
        .spatial_node
        .world_content_transform
        .unapply(clipped_world_rect) {

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
        Some(..) => {
            // If we already have a segment descriptor, skip segment build.
            return;
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

    // Segment the primitive on all the local-space clip sources that we can.
    let mut local_clip_count = 0;
    for i in 0 .. clip_chain.clips_range.count {
        let clip_instance = frame_state
            .clip_store
            .get_instance_from_range(&clip_chain.clips_range, i);
        let clip_node = &frame_state.clip_data_store[clip_instance.handle];

        // If this clip item is positioned by another positioning node, its relative position
        // could change during scrolling. This means that we would need to resegment. Instead
        // of doing that, only segment with clips that have the same positioning node.
        // TODO(mrobinson, #2858): It may make sense to include these nodes, resegmenting only
        // when necessary while scrolling.
        if !clip_instance.flags.contains(ClipNodeFlags::SAME_SPATIAL_NODE) {
            continue;
        }

        local_clip_count += 1;

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
        // If there were no local clips, then we will subdivide the primitive into
        // a uniform grid (up to 8x8 segments). This will typically result in
        // a significant number of those segments either being completely clipped,
        // or determined to not need a clip mask for that segment.
        if local_clip_count == 0 && clip_chain.clips_range.count > 0 {
            let x_clip_count = cmp::min(8, (metadata.local_rect.size.width / 128.0).ceil() as i32);
            let y_clip_count = cmp::min(8, (metadata.local_rect.size.height / 128.0).ceil() as i32);

            for y in 0 .. y_clip_count {
                let y0 = metadata.local_rect.size.height * y as f32 / y_clip_count as f32;
                let y1 = metadata.local_rect.size.height * (y+1) as f32 / y_clip_count as f32;

                for x in 0 .. x_clip_count {
                    let x0 = metadata.local_rect.size.width * x as f32 / x_clip_count as f32;
                    let x1 = metadata.local_rect.size.width * (x+1) as f32 / x_clip_count as f32;

                    let rect = LayoutRect::new(
                        LayoutPoint::new(
                            x0 + metadata.local_rect.origin.x,
                            y0 + metadata.local_rect.origin.y,
                        ),
                        LayoutSize::new(
                            x1 - x0,
                            y1 - y0,
                        ),
                    );

                    segment_builder.push_mask_region(rect, LayoutRect::zero(), None);
                }
            }
        }

        match brush.segment_desc {
            Some(..) => panic!("bug: should not already have descriptor"),
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
                });
            }
        }
    }
}

impl Primitive {
    fn update_clip_task_for_brush(
        &mut self,
        root_spatial_node_index: SpatialNodeIndex,
        prim_bounding_rect: WorldRect,
        prim_context: &PrimitiveContext,
        prim_clip_chain: &ClipChainInstance,
        pic_state: &mut PictureState,
        frame_context: &FrameBuildingContext,
        frame_state: &mut FrameBuildingState,
        clip_node_collector: &Option<ClipNodeCollector>,
    ) -> bool {
        let brush = match self.details {
            PrimitiveDetails::Brush(ref mut brush) => brush,
            PrimitiveDetails::TextRun(..) => return false,
        };

        write_brush_segment_description(
            brush,
            &self.metadata,
            prim_clip_chain,
            frame_state,
        );

        let segment_desc = match brush.segment_desc {
            Some(ref mut description) => description,
            None => return false,
        };

        for segment in &mut segment_desc.segments {
            // Build a clip chain for the smaller segment rect. This will
            // often manage to eliminate most/all clips, and sometimes
            // clip the segment completely.
            let segment_clip_chain = frame_state
                .clip_store
                .build_clip_chain_instance(
                    self.metadata.clip_chain_id,
                    segment.local_rect,
                    self.metadata.local_clip_rect,
                    prim_context.spatial_node_index,
                    &pic_state.map_local_to_pic,
                    &pic_state.map_pic_to_world,
                    &frame_context.clip_scroll_tree,
                    frame_state.gpu_cache,
                    frame_state.resource_cache,
                    frame_context.device_pixel_scale,
                    &frame_context.world_rect,
                    clip_node_collector,
                    frame_state.clip_data_store,
                );

            match segment_clip_chain {
                Some(segment_clip_chain) => {
                    if !segment_clip_chain.needs_mask ||
                       (!segment.may_need_clip_mask && !segment_clip_chain.has_non_local_clips) {
                        segment.clip_task_id = BrushSegmentTaskId::Opaque;
                        continue;
                    }

                    let (device_rect, _, _) = match get_raster_rects(
                        segment_clip_chain.pic_clip_rect,
                        &pic_state.map_pic_to_raster,
                        &pic_state.map_raster_to_world,
                        prim_bounding_rect,
                        frame_context.device_pixel_scale,
                    ) {
                        Some(info) => info,
                        None => {
                            segment.clip_task_id = BrushSegmentTaskId::Empty;
                            continue;
                        }
                    };

                    let clip_task = RenderTask::new_mask(
                        device_rect.to_i32(),
                        segment_clip_chain.clips_range,
                        root_spatial_node_index,
                        frame_state.clip_store,
                        frame_state.gpu_cache,
                        frame_state.resource_cache,
                        frame_state.render_tasks,
                        frame_state.clip_data_store,
                    );

                    let clip_task_id = frame_state.render_tasks.add(clip_task);
                    pic_state.tasks.push(clip_task_id);
                    segment.clip_task_id = BrushSegmentTaskId::RenderTaskId(clip_task_id);
                }
                None => {
                    segment.clip_task_id = BrushSegmentTaskId::Empty;
                }
            }
        }

        true
    }

    // Returns true if the primitive *might* need a clip mask. If
    // false, there is no need to even check for clip masks for
    // this primitive.
    fn reset_clip_task(&mut self) -> bool {
        self.metadata.clip_task_id = None;
        match self.details {
            PrimitiveDetails::Brush(ref mut brush) => {
                if let Some(ref mut desc) = brush.segment_desc {
                    for segment in &mut desc.segments {
                        segment.clip_task_id = BrushSegmentTaskId::Opaque;
                    }
                }
                brush.may_need_clip_mask()
            }
            PrimitiveDetails::TextRun(..) => {
                true
            }
        }
    }

    fn prepare_prim_for_render_inner(
        &mut self,
        prim_index: PrimitiveIndex,
        prim_context: &PrimitiveContext,
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
                let transform = prim_context.spatial_node.world_content_transform.to_transform();
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
                        color,
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
                                opacity_binding.current == 1.0 &&
                                color.a == 1.0;

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
                                    prim_context,
                                    &metadata.clipped_world_rect.unwrap(),
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
                                    metadata.clipped_world_rect = None;
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
                                prim_context,
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
                                prim_context,
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
                        if !pic.prepare_for_render(
                            prim_index,
                            metadata,
                            pic_state,
                            frame_context,
                            frame_state,
                        ) {
                            metadata.clipped_world_rect = None;
                        }
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
        prim_context: &PrimitiveContext,
        prim_bounding_rect: WorldRect,
        root_spatial_node_index: SpatialNodeIndex,
        clip_chain: &ClipChainInstance,
        pic_state: &mut PictureState,
        frame_context: &FrameBuildingContext,
        frame_state: &mut FrameBuildingState,
        is_chased: bool,
        clip_node_collector: &Option<ClipNodeCollector>,
    ) {
        if cfg!(debug_assertions) && is_chased {
            println!("\tupdating clip task with pic rect {:?}", clip_chain.pic_clip_rect);
        }
        // Reset clips from previous frames since we may clip differently each frame.
        // If this primitive never needs clip masks, just return straight away.
        if !self.reset_clip_task() {
            return;
        }

        // First try to  render this primitive's mask using optimized brush rendering.
        if self.update_clip_task_for_brush(
            root_spatial_node_index,
            prim_bounding_rect,
            prim_context,
            &clip_chain,
            pic_state,
            frame_context,
            frame_state,
            clip_node_collector,
        ) {
            if cfg!(debug_assertions) && is_chased {
                println!("\tsegment tasks have been created for clipping");
            }
            return;
        }

        if clip_chain.needs_mask {
            if let Some((device_rect, _, _)) = get_raster_rects(
                clip_chain.pic_clip_rect,
                &pic_state.map_pic_to_raster,
                &pic_state.map_raster_to_world,
                prim_bounding_rect,
                frame_context.device_pixel_scale,
            ) {
                let clip_task = RenderTask::new_mask(
                    device_rect,
                    clip_chain.clips_range,
                    root_spatial_node_index,
                    frame_state.clip_store,
                    frame_state.gpu_cache,
                    frame_state.resource_cache,
                    frame_state.render_tasks,
                    frame_state.clip_data_store,
                );

                let clip_task_id = frame_state.render_tasks.add(clip_task);
                if cfg!(debug_assertions) && is_chased {
                    println!("\tcreated task {:?} with world rect {:?}",
                        clip_task_id, self.metadata.clipped_world_rect);
                }
                self.metadata.clip_task_id = Some(clip_task_id);
                pic_state.tasks.push(clip_task_id);
            }
        }
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

            // NOTE(emilio): This `needs_update` relies on the local rect for a
            // given primitive being immutable. If that changes, this code
            // should probably handle changes to it as well, retaining the old
            // size in cache_key.
            let needs_update = scale_au != cache_key.scale;

            let mut new_segments = Vec::new();

            let local_rect = &self.metadata.local_rect;
            if needs_update {
                cache_key.scale = scale_au;

                *task_info = BorderRenderTaskInfo::new(
                    local_rect,
                    border,
                    widths,
                    scale,
                    &mut new_segments,
                );
            }

            *handle = task_info.as_ref().map(|task_info| {
                frame_state.resource_cache.request_render_task(
                    RenderTaskCacheKey {
                        size: task_info.cache_key_size(&local_rect.size, scale),
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
                });

                // The segments have changed, so force the GPU cache to
                // re-upload the primitive information.
                frame_state.gpu_cache.invalidate(&mut self.metadata.gpu_location);
            }
        }
    }
}

pub fn get_raster_rects(
    pic_rect: PictureRect,
    map_to_raster: &SpaceMapper<PicturePixel, RasterPixel>,
    map_to_world: &SpaceMapper<RasterPixel, WorldPixel>,
    prim_bounding_rect: WorldRect,
    device_pixel_scale: DevicePixelScale,
) -> Option<(DeviceIntRect, DeviceRect, PictureToRasterTransform)> {
    let unclipped_raster_rect = map_to_raster.map(&pic_rect)?;

    let unclipped = raster_rect_to_device_pixels(
        unclipped_raster_rect,
        device_pixel_scale,
    );

    let unclipped_world_rect = map_to_world.map(&unclipped_raster_rect)?;

    let clipped_world_rect = unclipped_world_rect.intersection(&prim_bounding_rect)?;

    let clipped_raster_rect = map_to_world.unmap(&clipped_world_rect)?;

    let clipped_raster_rect = clipped_raster_rect.intersection(&unclipped_raster_rect)?;

    let clipped = raster_rect_to_device_pixels(
        clipped_raster_rect,
        device_pixel_scale,
    );

    let transform = map_to_raster.get_transform();

    Some((clipped.to_i32(), unclipped, transform))
}
