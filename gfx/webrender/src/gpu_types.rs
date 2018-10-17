/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{
    DevicePoint, DeviceSize, DeviceRect, LayoutRect, LayoutToWorldTransform, LayoutTransform,
    PremultipliedColorF, LayoutToPictureTransform, PictureToLayoutTransform, PicturePixel,
    WorldPixel, WorldToLayoutTransform,
};
use clip_scroll_tree::{ClipScrollTree, ROOT_SPATIAL_NODE_INDEX, SpatialNodeIndex};
use gpu_cache::{GpuCacheAddress, GpuDataRequest};
use internal_types::FastHashMap;
use prim_store::EdgeAaSegmentMask;
use render_task::RenderTaskAddress;
use util::{TransformedRectKind, MatrixHelpers};

// Contains type that must exactly match the same structures declared in GLSL.

#[derive(Copy, Clone, Debug)]
#[repr(C)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct ZBufferId(i32);

#[derive(Debug)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct ZBufferIdGenerator {
    next: i32,
}

impl ZBufferIdGenerator {
    pub fn new() -> Self {
        ZBufferIdGenerator {
            next: 0
        }
    }

    pub fn next(&mut self) -> ZBufferId {
        let id = ZBufferId(self.next);
        self.next += 1;
        id
    }
}

#[derive(Debug, Copy, Clone)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[repr(C)]
pub enum RasterizationSpace {
    Local = 0,
    Screen = 1,
}

#[derive(Debug, Copy, Clone)]
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

#[derive(Debug)]
#[repr(C)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct BlurInstance {
    pub task_address: RenderTaskAddress,
    pub src_task_address: RenderTaskAddress,
    pub blur_direction: BlurDirection,
}

#[derive(Debug)]
#[repr(C)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct ScalingInstance {
    pub task_address: RenderTaskAddress,
    pub src_task_address: RenderTaskAddress,
}

#[derive(Debug, Copy, Clone, PartialEq)]
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

/// A clipping primitive drawn into the clipping mask.
/// Could be an image or a rectangle, which defines the
/// way `address` is treated.
#[derive(Debug, Copy, Clone)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[repr(C)]
pub struct ClipMaskInstance {
    pub render_task_address: RenderTaskAddress,
    pub clip_transform_id: TransformPaletteId,
    pub prim_transform_id: TransformPaletteId,
    pub segment: i32,
    pub clip_data_address: GpuCacheAddress,
    pub resource_address: GpuCacheAddress,
}

/// A border corner dot or dash drawn into the clipping mask.
#[derive(Debug, Copy, Clone)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[repr(C)]
pub struct ClipMaskBorderCornerDotDash {
    pub clip_mask_instance: ClipMaskInstance,
    pub dot_dash_data: [f32; 8],
}

// 16 bytes per instance should be enough for anyone!
#[derive(Debug, Clone)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct PrimitiveInstanceData {
    data: [i32; 4],
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
    // Used to generated a unique z-buffer value per primitive.
    pub z_generator: ZBufferIdGenerator,
}

impl PrimitiveHeaders {
    pub fn new() -> PrimitiveHeaders {
        PrimitiveHeaders {
            headers_int: Vec::new(),
            headers_float: Vec::new(),
            z_generator: ZBufferIdGenerator::new(),
        }
    }

    // Add a new primitive header.
    pub fn push(
        &mut self,
        prim_header: &PrimitiveHeader,
        user_data: [i32; 3],
    ) -> PrimitiveHeaderIndex {
        debug_assert_eq!(self.headers_int.len(), self.headers_float.len());
        let id = self.headers_float.len();

        self.headers_float.push(PrimitiveHeaderF {
            local_rect: prim_header.local_rect,
            local_clip_rect: prim_header.local_clip_rect,
        });

        self.headers_int.push(PrimitiveHeaderI {
            z: self.z_generator.next(),
            task_address: prim_header.task_address,
            specific_prim_address: prim_header.specific_prim_address.as_int(),
            clip_task_address: prim_header.clip_task_address,
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
    pub task_address: RenderTaskAddress,
    pub specific_prim_address: GpuCacheAddress,
    pub clip_task_address: RenderTaskAddress,
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
    pub task_address: RenderTaskAddress,
    pub specific_prim_address: i32,
    pub clip_task_address: RenderTaskAddress,
    pub transform_id: TransformPaletteId,
    pub user_data: [i32; 3],
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
    pub fn build(&self, data0: i32, data1: i32, data2: i32) -> PrimitiveInstanceData {
        PrimitiveInstanceData {
            data: [
                self.prim_header_index.0 as i32,
                data0,
                data1,
                data2,
            ],
        }
    }
}

pub struct SplitCompositeInstance {
    pub prim_header_index: PrimitiveHeaderIndex,
    pub polygons_address: GpuCacheAddress,
    pub z: ZBufferId,
}

impl SplitCompositeInstance {
    pub fn new(
        prim_header_index: PrimitiveHeaderIndex,
        polygons_address: GpuCacheAddress,
        z: ZBufferId,
    ) -> Self {
        SplitCompositeInstance {
            prim_header_index,
            polygons_address,
            z,
        }
    }
}

impl From<SplitCompositeInstance> for PrimitiveInstanceData {
    fn from(instance: SplitCompositeInstance) -> Self {
        PrimitiveInstanceData {
            data: [
                instance.prim_header_index.0,
                instance.polygons_address.as_int(),
                instance.z.0,
                0,
            ],
        }
    }
}

bitflags! {
    /// Flags that define how the common brush shader
    /// code should process this instance.
    pub struct BrushFlags: u8 {
        /// Apply perspective interpolation to UVs
        const PERSPECTIVE_INTERPOLATION = 0x1;
        /// Do interpolation relative to segment rect,
        /// rather than primitive rect.
        const SEGMENT_RELATIVE = 0x2;
        /// Repeat UVs horizontally.
        const SEGMENT_REPEAT_X = 0x4;
        /// Repeat UVs vertically.
        const SEGMENT_REPEAT_Y = 0x8;
    }
}

// TODO(gw): Some of these fields can be moved to the primitive
//           header since they are constant, and some can be
//           compressed to a smaller size.
#[repr(C)]
pub struct BrushInstance {
    pub prim_header_index: PrimitiveHeaderIndex,
    pub clip_task_address: RenderTaskAddress,
    pub segment_index: i32,
    pub edge_flags: EdgeAaSegmentMask,
    pub brush_flags: BrushFlags,
    pub user_data: i32,
}

impl From<BrushInstance> for PrimitiveInstanceData {
    fn from(instance: BrushInstance) -> Self {
        PrimitiveInstanceData {
            data: [
                instance.prim_header_index.0,
                instance.clip_task_address.0 as i32,
                instance.segment_index |
                ((instance.edge_flags.bits() as i32) << 16) |
                ((instance.brush_flags.bits() as i32) << 24),
                instance.user_data,
            ]
        }
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
        if (self.0 >> 24) == 0 {
            TransformedRectKind::AxisAligned
        } else {
            TransformedRectKind::Complex
        }
    }
}

// The GPU data payload for a transform palette entry.
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
    pub transforms: Vec<TransformData>,
    metadata: Vec<TransformMetadata>,
    map: FastHashMap<RelativeTransformKey, usize>,
}

impl TransformPalette {
    pub fn new() -> Self {
        TransformPalette {
            transforms: Vec::new(),
            metadata: Vec::new(),
            map: FastHashMap::default(),
        }
    }

    pub fn allocate(&mut self, count: usize) {
        self.transforms = vec![TransformData::invalid(); count];
        self.metadata = vec![TransformMetadata::invalid(); count];
    }

    pub fn set_world_transform(
        &mut self,
        index: SpatialNodeIndex,
        transform: LayoutToWorldTransform,
    ) {
        register_transform(
            &mut self.metadata,
            &mut self.transforms,
            index,
            ROOT_SPATIAL_NODE_INDEX,
            // We know the root picture space == world space
            transform.with_destination::<PicturePixel>(),
        );
    }

    fn get_index(
        &mut self,
        from_index: SpatialNodeIndex,
        to_index: SpatialNodeIndex,
        clip_scroll_tree: &ClipScrollTree,
    ) -> usize {
        if to_index == ROOT_SPATIAL_NODE_INDEX {
            from_index.0
        } else if from_index == to_index {
            0
        } else {
            let key = RelativeTransformKey {
                from_index,
                to_index,
            };

            let metadata = &mut self.metadata;
            let transforms = &mut self.transforms;

            *self.map
                .entry(key)
                .or_insert_with(|| {
                    let transform = clip_scroll_tree.get_relative_transform(
                        from_index,
                        to_index,
                    )
                    .unwrap_or(LayoutTransform::identity())
                    .with_destination::<PicturePixel>();

                    register_transform(
                        metadata,
                        transforms,
                        from_index,
                        to_index,
                        transform,
                    )
                })
        }
    }

    pub fn get_world_transform(
        &self,
        index: SpatialNodeIndex,
    ) -> LayoutToWorldTransform {
        self.transforms[index.0]
            .transform
            .with_destination::<WorldPixel>()
    }

    pub fn get_world_inv_transform(
        &self,
        index: SpatialNodeIndex,
    ) -> WorldToLayoutTransform {
        self.transforms[index.0]
            .inv_transform
            .with_source::<WorldPixel>()
    }

    // Get a transform palette id for the given spatial node.
    // TODO(gw): In the future, it will be possible to specify
    //           a coordinate system id here, to allow retrieving
    //           transforms in the local space of a given spatial node.
    pub fn get_id(
        &mut self,
        from_index: SpatialNodeIndex,
        to_index: SpatialNodeIndex,
        clip_scroll_tree: &ClipScrollTree,
    ) -> TransformPaletteId {
        let index = self.get_index(
            from_index,
            to_index,
            clip_scroll_tree,
        );
        let transform_kind = self.metadata[index].transform_kind as u32;
        TransformPaletteId(
            (index as u32) |
            (transform_kind << 24)
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
        top_left: DevicePoint,
        top_right: DevicePoint,
        bottom_left: DevicePoint,
        bottom_right: DevicePoint,
    },
}

#[derive(Debug, Copy, Clone)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct ImageSource {
    pub p0: DevicePoint,
    pub p1: DevicePoint,
    pub texture_layer: f32,
    pub user_data: [f32; 3],
    pub uv_rect_kind: UvRectKind,
}

impl ImageSource {
    pub fn write_gpu_blocks(&self, request: &mut GpuDataRequest) {
        request.push([
            self.p0.x,
            self.p0.y,
            self.p1.x,
            self.p1.y,
        ]);
        request.push([
            self.texture_layer,
            self.user_data[0],
            self.user_data[1],
            self.user_data[2],
        ]);

        // If this is a polygon uv kind, then upload the four vertices.
        if let UvRectKind::Quad { top_left, top_right, bottom_left, bottom_right } = self.uv_rect_kind {
            request.push([
                top_left.x,
                top_left.y,
                top_right.x,
                top_right.y,
            ]);

            request.push([
                bottom_left.x,
                bottom_left.y,
                bottom_right.x,
                bottom_right.y,
            ]);
        }
    }
}

// Set the local -> world transform for a given spatial
// node in the transform palette.
fn register_transform(
    metadatas: &mut Vec<TransformMetadata>,
    transforms: &mut Vec<TransformData>,
    from_index: SpatialNodeIndex,
    to_index: SpatialNodeIndex,
    transform: LayoutToPictureTransform,
) -> usize {
    // TODO(gw): This shouldn't ever happen - should be eliminated before
    //           we get an uninvertible transform here. But maybe do
    //           some investigation on if this ever happens?
    let inv_transform = match transform.inverse() {
        Some(inv_transform) => inv_transform,
        None => {
            error!("Unable to get inverse transform");
            PictureToLayoutTransform::identity()
        }
    };

    let metadata = TransformMetadata {
        transform_kind: transform.transform_kind()
    };
    let data = TransformData {
        transform,
        inv_transform,
    };

    if to_index == ROOT_SPATIAL_NODE_INDEX {
        let index = from_index.0 as usize;
        metadatas[index] = metadata;
        transforms[index] = data;
        index
    } else {
        let index = transforms.len();
        metadatas.push(metadata);
        transforms.push(data);
        index
    }
}
