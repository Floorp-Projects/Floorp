/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{AlphaType, PremultipliedColorF, YuvFormat, YuvRangedColorSpace};
use api::units::*;
use crate::composite::CompositeFeatures;
use crate::segment::EdgeAaSegmentMask;
use crate::spatial_tree::{SpatialTree, SpatialNodeIndex};
use crate::gpu_cache::{GpuCacheAddress, GpuDataRequest};
use crate::internal_types::FastHashMap;
use crate::prim_store::ClipData;
use crate::render_task::RenderTaskAddress;
use crate::render_task_graph::RenderTaskId;
use crate::renderer::{ShaderColorMode, GpuBufferAddress};
use std::i32;
use crate::util::{TransformedRectKind, MatrixHelpers};
use glyph_rasterizer::SubpixelDirection;
use crate::util::{ScaleOffset, pack_as_float};

// Contains type that must exactly match the same structures declared in GLSL.

pub const VECS_PER_TRANSFORM: usize = 8;

#[derive(Copy, Clone, Debug, PartialEq)]
#[repr(C)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct ZBufferId(pub i32);

impl ZBufferId {
    pub fn invalid() -> Self {
        ZBufferId(i32::MAX)
    }
}

#[derive(Debug)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct ZBufferIdGenerator {
    next: i32,
    max_depth_ids: i32,
}

impl ZBufferIdGenerator {
    pub fn new(max_depth_ids: i32) -> Self {
        ZBufferIdGenerator {
            next: 0,
            max_depth_ids,
        }
    }

    pub fn next(&mut self) -> ZBufferId {
        debug_assert!(self.next < self.max_depth_ids);
        let id = ZBufferId(self.next);
        self.next += 1;
        id
    }
}

#[derive(Clone, Debug)]
#[repr(C)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct CopyInstance {
    pub src_rect: DeviceRect,
    pub dst_rect: DeviceRect,
    pub dst_texture_size: DeviceSize,
}

#[derive(Debug, Copy, Clone)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[repr(C)]
pub enum RasterizationSpace {
    Local = 0,
    Screen = 1,
}

#[derive(Debug, Copy, Clone, MallocSizeOf)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[repr(C)]
pub enum BoxShadowStretchMode {
    Stretch = 0,
    Simple = 1,
}

#[repr(i32)]
#[derive(Debug, Copy, Clone)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub enum BlurDirection {
    Horizontal = 0,
    Vertical,
}

#[derive(Clone, Debug)]
#[repr(C)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct BlurInstance {
    pub task_address: RenderTaskAddress,
    pub src_task_address: RenderTaskAddress,
    pub blur_direction: BlurDirection,
}

#[derive(Clone, Debug)]
#[repr(C)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct ScalingInstance {
    pub target_rect: DeviceRect,
    pub source_rect: DeviceRect,
}

#[derive(Clone, Debug)]
#[repr(C)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct SvgFilterInstance {
    pub task_address: RenderTaskAddress,
    pub input_1_task_address: RenderTaskAddress,
    pub input_2_task_address: RenderTaskAddress,
    pub kind: u16,
    pub input_count: u16,
    pub generic_int: u16,
    pub extra_data_address: GpuCacheAddress,
}

#[derive(Copy, Clone, Debug, Hash, MallocSizeOf, PartialEq, Eq)]
#[repr(C)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub enum BorderSegment {
    TopLeft,
    TopRight,
    BottomRight,
    BottomLeft,
    Left,
    Top,
    Right,
    Bottom,
}

#[derive(Debug, Clone)]
#[repr(C)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct BorderInstance {
    pub task_origin: DevicePoint,
    pub local_rect: DeviceRect,
    pub color0: PremultipliedColorF,
    pub color1: PremultipliedColorF,
    pub flags: i32,
    pub widths: DeviceSize,
    pub radius: DeviceSize,
    pub clip_params: [f32; 8],
}

#[derive(Copy, Clone, Debug)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[repr(C)]
pub struct ClipMaskInstanceCommon {
    pub sub_rect: DeviceRect,
    pub task_origin: DevicePoint,
    pub screen_origin: DevicePoint,
    pub device_pixel_scale: f32,
    pub clip_transform_id: TransformPaletteId,
    pub prim_transform_id: TransformPaletteId,
}

#[derive(Clone, Debug)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[repr(C)]
pub struct ClipMaskInstanceImage {
    pub common: ClipMaskInstanceCommon,
    pub tile_rect: LayoutRect,
    pub resource_address: GpuCacheAddress,
    pub local_rect: LayoutRect,
}

#[derive(Clone, Debug)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[repr(C)]
pub struct ClipMaskInstanceRect {
    pub common: ClipMaskInstanceCommon,
    pub local_pos: LayoutPoint,
    pub clip_data: ClipData,
}

#[derive(Clone, Debug)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[repr(C)]
pub struct BoxShadowData {
    pub src_rect_size: LayoutSize,
    pub clip_mode: i32,
    pub stretch_mode_x: i32,
    pub stretch_mode_y: i32,
    pub dest_rect: LayoutRect,
}

#[derive(Clone, Debug)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[repr(C)]
pub struct ClipMaskInstanceBoxShadow {
    pub common: ClipMaskInstanceCommon,
    pub resource_address: GpuCacheAddress,
    pub shadow_data: BoxShadowData,
}

/// A clipping primitive drawn into the clipping mask.
/// Could be an image or a rectangle, which defines the
/// way `address` is treated.
#[derive(Debug, Copy, Clone)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[repr(C)]
pub struct ClipMaskInstance {
    pub clip_transform_id: TransformPaletteId,
    pub prim_transform_id: TransformPaletteId,
    pub clip_data_address: GpuCacheAddress,
    pub resource_address: GpuCacheAddress,
    pub local_pos: LayoutPoint,
    pub tile_rect: LayoutRect,
    pub sub_rect: DeviceRect,
    pub task_origin: DevicePoint,
    pub screen_origin: DevicePoint,
    pub device_pixel_scale: f32,
}

// 16 bytes per instance should be enough for anyone!
#[repr(C)]
#[derive(Debug, Clone)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct PrimitiveInstanceData {
    data: [i32; 4],
}

/// Specifies that an RGB CompositeInstance's UV coordinates are normalized.
const UV_TYPE_NORMALIZED: u32 = 0;
/// Specifies that an RGB CompositeInstance's UV coordinates are not normalized.
const UV_TYPE_UNNORMALIZED: u32 = 1;

/// A GPU-friendly representation of the `ScaleOffset` type
#[derive(Clone, Debug)]
#[repr(C)]
pub struct CompositorTransform {
    pub sx: f32,
    pub sy: f32,
    pub tx: f32,
    pub ty: f32,
}

impl CompositorTransform {
    pub fn identity() -> Self {
        CompositorTransform {
            sx: 1.0,
            sy: 1.0,
            tx: 0.0,
            ty: 0.0,
        }
    }
}

impl From<ScaleOffset> for CompositorTransform {
    fn from(scale_offset: ScaleOffset) -> Self {
        CompositorTransform {
            sx: scale_offset.scale.x,
            sy: scale_offset.scale.y,
            tx: scale_offset.offset.x,
            ty: scale_offset.offset.y,
        }
    }
}

/// Vertex format for picture cache composite shader.
/// When editing the members, update desc::COMPOSITE
/// so its list of instance_attributes matches:
#[derive(Clone, Debug)]
#[repr(C)]
pub struct CompositeInstance {
    // Picture space destination rectangle of surface
    rect: PictureRect,
    // Device space destination clip rect for this surface
    clip_rect: DeviceRect,
    // Color for solid color tiles, white otherwise
    color: PremultipliedColorF,

    // Packed into a single vec4 (aParams)
    _padding: f32,
    color_space_or_uv_type: f32, // YuvColorSpace for YUV;
                                 // UV coordinate space for RGB
    yuv_format: f32,            // YuvFormat
    yuv_channel_bit_depth: f32,

    // UV rectangles (pixel space) for color / yuv texture planes
    uv_rects: [TexelRect; 3],

    // A 2d scale + offset transform for the rect
    transform: CompositorTransform,
}

impl CompositeInstance {
    pub fn new(
        rect: PictureRect,
        clip_rect: DeviceRect,
        color: PremultipliedColorF,
        transform: CompositorTransform,
    ) -> Self {
        let uv = TexelRect::new(0.0, 0.0, 1.0, 1.0);
        CompositeInstance {
            rect,
            clip_rect,
            color,
            _padding: 0.0,
            color_space_or_uv_type: pack_as_float(UV_TYPE_NORMALIZED),
            yuv_format: 0.0,
            yuv_channel_bit_depth: 0.0,
            uv_rects: [uv, uv, uv],
            transform,
        }
    }

    pub fn new_rgb(
        rect: PictureRect,
        clip_rect: DeviceRect,
        color: PremultipliedColorF,
        uv_rect: TexelRect,
        transform: CompositorTransform,
    ) -> Self {
        CompositeInstance {
            rect,
            clip_rect,
            color,
            _padding: 0.0,
            color_space_or_uv_type: pack_as_float(UV_TYPE_UNNORMALIZED),
            yuv_format: 0.0,
            yuv_channel_bit_depth: 0.0,
            uv_rects: [uv_rect, uv_rect, uv_rect],
            transform,
        }
    }

    pub fn new_yuv(
        rect: PictureRect,
        clip_rect: DeviceRect,
        yuv_color_space: YuvRangedColorSpace,
        yuv_format: YuvFormat,
        yuv_channel_bit_depth: u32,
        uv_rects: [TexelRect; 3],
        transform: CompositorTransform,
    ) -> Self {
        CompositeInstance {
            rect,
            clip_rect,
            color: PremultipliedColorF::WHITE,
            _padding: 0.0,
            color_space_or_uv_type: pack_as_float(yuv_color_space as u32),
            yuv_format: pack_as_float(yuv_format as u32),
            yuv_channel_bit_depth: pack_as_float(yuv_channel_bit_depth),
            uv_rects,
            transform,
        }
    }

    // Returns the CompositeFeatures that can be used to composite
    // this RGB instance.
    pub fn get_rgb_features(&self) -> CompositeFeatures {
        let mut features = CompositeFeatures::empty();

        // If the UV rect covers the entire texture then we can avoid UV clamping.
        // We should try harder to determine this for unnormalized UVs too.
        if self.color_space_or_uv_type == pack_as_float(UV_TYPE_NORMALIZED)
            && self.uv_rects[0] == TexelRect::new(0.0, 0.0, 1.0, 1.0)
        {
            features |= CompositeFeatures::NO_UV_CLAMP;
        }

        if self.color == PremultipliedColorF::WHITE {
            features |= CompositeFeatures::NO_COLOR_MODULATION
        }

        features
    }
}

/// Vertex format for issuing colored quads.
#[derive(Debug, Clone)]
#[repr(C)]
pub struct ClearInstance {
    pub rect: [f32; 4],
    pub color: [f32; 4],
}

#[derive(Debug, Copy, Clone)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct PrimitiveHeaderIndex(pub i32);

#[derive(Debug)]
#[repr(C)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct PrimitiveHeaders {
    // The integer-type headers for a primitive.
    pub headers_int: Vec<PrimitiveHeaderI>,
    // The float-type headers for a primitive.
    pub headers_float: Vec<PrimitiveHeaderF>,
}

impl PrimitiveHeaders {
    pub fn new() -> PrimitiveHeaders {
        PrimitiveHeaders {
            headers_int: Vec::new(),
            headers_float: Vec::new(),
        }
    }

    // Add a new primitive header.
    pub fn push(
        &mut self,
        prim_header: &PrimitiveHeader,
        z: ZBufferId,
        user_data: [i32; 4],
    ) -> PrimitiveHeaderIndex {
        debug_assert_eq!(self.headers_int.len(), self.headers_float.len());
        let id = self.headers_float.len();

        self.headers_float.push(PrimitiveHeaderF {
            local_rect: prim_header.local_rect,
            local_clip_rect: prim_header.local_clip_rect,
        });

        self.headers_int.push(PrimitiveHeaderI {
            z,
            unused: 0,
            specific_prim_address: prim_header.specific_prim_address.as_int(),
            transform_id: prim_header.transform_id,
            user_data,
        });

        PrimitiveHeaderIndex(id as i32)
    }
}

// This is a convenience type used to make it easier to pass
// the common parts around during batching.
#[derive(Debug)]
pub struct PrimitiveHeader {
    pub local_rect: LayoutRect,
    pub local_clip_rect: LayoutRect,
    pub specific_prim_address: GpuCacheAddress,
    pub transform_id: TransformPaletteId,
}

// f32 parts of a primitive header
#[derive(Debug)]
#[repr(C)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct PrimitiveHeaderF {
    pub local_rect: LayoutRect,
    pub local_clip_rect: LayoutRect,
}

// i32 parts of a primitive header
// TODO(gw): Compress parts of these down to u16
#[derive(Debug)]
#[repr(C)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct PrimitiveHeaderI {
    pub z: ZBufferId,
    pub specific_prim_address: i32,
    pub transform_id: TransformPaletteId,
    pub unused: i32,                    // To ensure required 16 byte alignment of vertex textures
    pub user_data: [i32; 4],
}

pub struct GlyphInstance {
    pub prim_header_index: PrimitiveHeaderIndex,
}

impl GlyphInstance {
    pub fn new(
        prim_header_index: PrimitiveHeaderIndex,
    ) -> Self {
        GlyphInstance {
            prim_header_index,
        }
    }

    // TODO(gw): Some of these fields can be moved to the primitive
    //           header since they are constant, and some can be
    //           compressed to a smaller size.
    pub fn build(&self,
        render_task: RenderTaskAddress,
        clip_task: RenderTaskAddress,
        subpx_dir: SubpixelDirection,
        glyph_index_in_text_run: i32,
        glyph_uv_rect: GpuCacheAddress,
        color_mode: ShaderColorMode,
    ) -> PrimitiveInstanceData {
        PrimitiveInstanceData {
            data: [
                self.prim_header_index.0 as i32,
                ((render_task.0 as i32) << 16)
                | clip_task.0 as i32,
                (subpx_dir as u32 as i32) << 24
                | (color_mode as u32 as i32) << 16
                | glyph_index_in_text_run,
                glyph_uv_rect.as_int(),
            ],
        }
    }
}

pub struct SplitCompositeInstance {
    pub prim_header_index: PrimitiveHeaderIndex,
    pub polygons_address: i32,
    pub z: ZBufferId,
    pub render_task_address: RenderTaskAddress,
}

impl From<SplitCompositeInstance> for PrimitiveInstanceData {
    fn from(instance: SplitCompositeInstance) -> Self {
        PrimitiveInstanceData {
            data: [
                instance.prim_header_index.0,
                instance.polygons_address,
                instance.z.0,
                instance.render_task_address.0 as i32,
            ],
        }
    }
}

#[derive(Copy, Clone)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct QuadInstance {
    pub render_task_address: RenderTaskAddress,
    pub prim_address: GpuBufferAddress,
    pub z_id: ZBufferId,
    pub transform_id: TransformPaletteId,
    pub quad_flags: u8,
    pub edge_flags: u8,
    pub part_index: u8,
    pub segment_index: u8,
}

impl From<QuadInstance> for PrimitiveInstanceData {
    fn from(instance: QuadInstance) -> Self {
        /*
            [32 bits prim address]
            [8 bits quad flags] [8 bits edge flags] [16 bits render task address]
            [8 bits segment flags] [24 bits z_id]
            [8 bits segment index] [24 bits xf_id]
         */
        PrimitiveInstanceData {
            data: [
                instance.prim_address.as_int(),
                ((instance.quad_flags as i32) << 24) |
                ((instance.edge_flags as i32) << 16) |
                instance.render_task_address.0 as i32,
                ((instance.part_index as i32) << 24) | instance.z_id.0,
                ((instance.segment_index as i32) << 24) | instance.transform_id.0 as i32,
            ],
        }
    }
}

#[cfg_attr(feature = "capture", derive(Serialize))]
pub struct QuadSegment {
    pub rect: LayoutRect,
    pub task_id: RenderTaskId,
}

#[derive(Copy, Debug, Clone, PartialEq)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[repr(u32)]
pub enum ClipSpace {
    Raster = 0,
    Primitive = 1,
}

#[repr(C)]
#[derive(Clone)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct MaskInstance {
    pub prim: PrimitiveInstanceData,
    pub clip_transform_id: TransformPaletteId,
    pub clip_address: i32,
    pub clip_space: ClipSpace,
    pub unused: i32,
}


// Note: This can use up to 12 bits due to how it will
// be packed in the instance data.

/// Flags that define how the common brush shader
/// code should process this instance.
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(Debug, Copy, PartialEq, Eq, Clone, PartialOrd, Ord, Hash, MallocSizeOf)]
pub struct BrushFlags(u16);

bitflags! {
    impl BrushFlags: u16 {
        /// Apply perspective interpolation to UVs
        const PERSPECTIVE_INTERPOLATION = 1;
        /// Do interpolation relative to segment rect,
        /// rather than primitive rect.
        const SEGMENT_RELATIVE = 2;
        /// Repeat UVs horizontally.
        const SEGMENT_REPEAT_X = 4;
        /// Repeat UVs vertically.
        const SEGMENT_REPEAT_Y = 8;
        /// Horizontally follow border-image-repeat: round.
        const SEGMENT_REPEAT_X_ROUND = 16;
        /// Vertically follow border-image-repeat: round.
        const SEGMENT_REPEAT_Y_ROUND = 32;
        /// Whether to position the repetitions so that the middle tile
        /// is horizontally centered.
        const SEGMENT_REPEAT_X_CENTERED = 64;
        /// Whether to position the repetitions so that the middle tile
        /// is vertically centered.
        const SEGMENT_REPEAT_Y_CENTERED = 128;
        /// Middle (fill) area of a border-image-repeat.
        const SEGMENT_NINEPATCH_MIDDLE = 256;
        /// The extra segment data is a texel rect.
        const SEGMENT_TEXEL_RECT = 512;
        /// Whether to force the anti-aliasing when the primitive
        /// is axis-aligned.
        const FORCE_AA = 1024;
    }
}

/// Convenience structure to encode into PrimitiveInstanceData.
pub struct BrushInstance {
    pub prim_header_index: PrimitiveHeaderIndex,
    pub render_task_address: RenderTaskAddress,
    pub clip_task_address: RenderTaskAddress,
    pub segment_index: i32,
    pub edge_flags: EdgeAaSegmentMask,
    pub brush_flags: BrushFlags,
    pub resource_address: i32,
}

impl From<BrushInstance> for PrimitiveInstanceData {
    fn from(instance: BrushInstance) -> Self {
        PrimitiveInstanceData {
            data: [
                instance.prim_header_index.0,
                ((instance.render_task_address.0 as i32) << 16)
                | instance.clip_task_address.0 as i32,
                instance.segment_index
                | ((instance.brush_flags.bits() as i32) << 16)
                | ((instance.edge_flags.bits() as i32) << 28),
                instance.resource_address,
            ]
        }
    }
}

/// Convenience structure to encode into the image brush's user data.
#[derive(Copy, Clone, Debug)]
pub struct ImageBrushData {
    pub color_mode: ShaderColorMode,
    pub alpha_type: AlphaType,
    pub raster_space: RasterizationSpace,
    pub opacity: f32,
}

impl ImageBrushData {
    #[inline]
    pub fn encode(&self) -> [i32; 4] {
        [
            self.color_mode as i32 | ((self.alpha_type as i32) << 16),
            self.raster_space as i32,
            get_shader_opacity(self.opacity),
            0,
        ]
    }
}

// Represents the information about a transform palette
// entry that is passed to shaders. It includes an index
// into the transform palette, and a set of flags. The
// only flag currently used determines whether the
// transform is axis-aligned (and this should have
// pixel snapping applied).
#[derive(Copy, Debug, Clone, PartialEq)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[repr(C)]
pub struct TransformPaletteId(pub u32);

impl TransformPaletteId {
    /// Identity transform ID.
    pub const IDENTITY: Self = TransformPaletteId(0);

    /// Extract the transform kind from the id.
    pub fn transform_kind(&self) -> TransformedRectKind {
        if (self.0 >> 23) == 0 {
            TransformedRectKind::AxisAligned
        } else {
            TransformedRectKind::Complex
        }
    }

    /// Override the kind of transform stored in this id. This can be useful in
    /// cases where we don't want shaders to consider certain transforms axis-
    /// aligned (i.e. perspective warp) even though we may still want to for the
    /// general case.
    pub fn override_transform_kind(&self, kind: TransformedRectKind) -> Self {
        TransformPaletteId((self.0 & 0x7FFFFFu32) | ((kind as u32) << 23))
    }
}

/// The GPU data payload for a transform palette entry.
#[derive(Debug, Clone)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[repr(C)]
pub struct TransformData {
    transform: LayoutToPictureTransform,
    inv_transform: PictureToLayoutTransform,
}

impl TransformData {
    fn invalid() -> Self {
        TransformData {
            transform: LayoutToPictureTransform::identity(),
            inv_transform: PictureToLayoutTransform::identity(),
        }
    }
}

// Extra data stored about each transform palette entry.
#[derive(Clone)]
pub struct TransformMetadata {
    transform_kind: TransformedRectKind,
}

impl TransformMetadata {
    pub fn invalid() -> Self {
        TransformMetadata {
            transform_kind: TransformedRectKind::AxisAligned,
        }
    }
}

#[derive(Debug, Hash, Eq, PartialEq)]
struct RelativeTransformKey {
    from_index: SpatialNodeIndex,
    to_index: SpatialNodeIndex,
}

// Stores a contiguous list of TransformData structs, that
// are ready for upload to the GPU.
// TODO(gw): For now, this only stores the complete local
//           to world transform for each spatial node. In
//           the future, the transform palette will support
//           specifying a coordinate system that the transform
//           should be relative to.
pub struct TransformPalette {
    transforms: Vec<TransformData>,
    metadata: Vec<TransformMetadata>,
    map: FastHashMap<RelativeTransformKey, usize>,
}

impl TransformPalette {
    pub fn new(
        count: usize,
    ) -> Self {
        let _ = VECS_PER_TRANSFORM;

        let mut transforms = Vec::with_capacity(count);
        let mut metadata = Vec::with_capacity(count);

        transforms.push(TransformData::invalid());
        metadata.push(TransformMetadata::invalid());

        TransformPalette {
            transforms,
            metadata,
            map: FastHashMap::default(),
        }
    }

    pub fn finish(self) -> Vec<TransformData> {
        self.transforms
    }

    fn get_index(
        &mut self,
        child_index: SpatialNodeIndex,
        parent_index: SpatialNodeIndex,
        spatial_tree: &SpatialTree,
    ) -> usize {
        if child_index == parent_index {
            0
        } else {
            let key = RelativeTransformKey {
                from_index: child_index,
                to_index: parent_index,
            };

            let metadata = &mut self.metadata;
            let transforms = &mut self.transforms;

            *self.map
                .entry(key)
                .or_insert_with(|| {
                    let transform = spatial_tree.get_relative_transform(
                        child_index,
                        parent_index,
                    )
                    .into_transform()
                    .with_destination::<PicturePixel>();

                    register_transform(
                        metadata,
                        transforms,
                        transform,
                    )
                })
        }
    }

    // Get a transform palette id for the given spatial node.
    // TODO(gw): In the future, it will be possible to specify
    //           a coordinate system id here, to allow retrieving
    //           transforms in the local space of a given spatial node.
    pub fn get_id(
        &mut self,
        from_index: SpatialNodeIndex,
        to_index: SpatialNodeIndex,
        spatial_tree: &SpatialTree,
    ) -> TransformPaletteId {
        let index = self.get_index(
            from_index,
            to_index,
            spatial_tree,
        );
        let transform_kind = self.metadata[index].transform_kind as u32;
        TransformPaletteId(
            (index as u32) |
            (transform_kind << 23)
        )
    }

    pub fn get_custom(
        &mut self,
        transform: LayoutToPictureTransform,
    ) -> TransformPaletteId {
        let index = register_transform(
            &mut self.metadata,
            &mut self.transforms,
            transform,
        );

        let transform_kind = self.metadata[index].transform_kind as u32;
        TransformPaletteId(
            (index as u32) |
            (transform_kind << 23)
        )
    }
}

// Texture cache resources can be either a simple rect, or define
// a polygon within a rect by specifying a UV coordinate for each
// corner. This is useful for rendering screen-space rasterized
// off-screen surfaces.
#[derive(Debug, Copy, Clone)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub enum UvRectKind {
    // The 2d bounds of the texture cache entry define the
    // valid UV space for this texture cache entry.
    Rect,
    // The four vertices below define a quad within
    // the texture cache entry rect. The shader can
    // use a bilerp() to correctly interpolate a
    // UV coord in the vertex shader.
    Quad {
        top_left: DeviceHomogeneousVector,
        top_right: DeviceHomogeneousVector,
        bottom_left: DeviceHomogeneousVector,
        bottom_right: DeviceHomogeneousVector,
    },
}

#[derive(Debug, Copy, Clone)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct ImageSource {
    pub p0: DevicePoint,
    pub p1: DevicePoint,
    // TODO: It appears that only glyphs make use of user_data (to store glyph offset
    // and scale).
    // Perhaps we should separate the two so we don't have to push an empty unused vec4
    // for all image sources.
    pub user_data: [f32; 4],
    pub uv_rect_kind: UvRectKind,
}

impl ImageSource {
    pub fn write_gpu_blocks(&self, request: &mut GpuDataRequest) {
        // see fetch_image_resource in GLSL
        // has to be VECS_PER_IMAGE_RESOURCE vectors
        request.push([
            self.p0.x,
            self.p0.y,
            self.p1.x,
            self.p1.y,
        ]);
        request.push(self.user_data);

        // If this is a polygon uv kind, then upload the four vertices.
        if let UvRectKind::Quad { top_left, top_right, bottom_left, bottom_right } = self.uv_rect_kind {
            // see fetch_image_resource_extra in GLSL
            //Note: we really need only 3 components per point here: X, Y, and W
            request.push(top_left);
            request.push(top_right);
            request.push(bottom_left);
            request.push(bottom_right);
        }
    }
}

// Set the local -> world transform for a given spatial
// node in the transform palette.
fn register_transform(
    metadatas: &mut Vec<TransformMetadata>,
    transforms: &mut Vec<TransformData>,
    transform: LayoutToPictureTransform,
) -> usize {
    // TODO: refactor the calling code to not even try
    // registering a non-invertible transform.
    let inv_transform = transform
        .inverse()
        .unwrap_or_else(PictureToLayoutTransform::identity);

    let metadata = TransformMetadata {
        transform_kind: transform.transform_kind()
    };
    let data = TransformData {
        transform,
        inv_transform,
    };

    let index = transforms.len();
    metadatas.push(metadata);
    transforms.push(data);

    index
}

pub fn get_shader_opacity(opacity: f32) -> i32 {
    (opacity * 65535.0).round() as i32
}
