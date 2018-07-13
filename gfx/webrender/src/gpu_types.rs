/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{DevicePoint, DeviceSize, DeviceRect, LayoutRect, LayoutToWorldTransform};
use api::{PremultipliedColorF, WorldToLayoutTransform};
use gpu_cache::{GpuCacheAddress, GpuDataRequest};
use prim_store::{EdgeAaSegmentMask};
use render_task::RenderTaskAddress;
use util::{MatrixHelpers, TransformedRectKind};

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
    pub transform_id: TransformPaletteId,
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
pub struct PrimitiveInstance {
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
    pub fn build(&self, data0: i32, data1: i32, data2: i32) -> PrimitiveInstance {
        PrimitiveInstance {
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
    pub task_address: RenderTaskAddress,
    pub src_task_address: RenderTaskAddress,
    pub polygons_address: GpuCacheAddress,
    pub z: ZBufferId,
}

impl SplitCompositeInstance {
    pub fn new(
        task_address: RenderTaskAddress,
        src_task_address: RenderTaskAddress,
        polygons_address: GpuCacheAddress,
        z: ZBufferId,
    ) -> Self {
        SplitCompositeInstance {
            task_address,
            src_task_address,
            polygons_address,
            z,
        }
    }
}

impl From<SplitCompositeInstance> for PrimitiveInstance {
    fn from(instance: SplitCompositeInstance) -> Self {
        PrimitiveInstance {
            data: [
                instance.task_address.0 as i32,
                instance.src_task_address.0 as i32,
                instance.polygons_address.as_int(),
                instance.z.0,
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
}

impl From<BrushInstance> for PrimitiveInstance {
    fn from(instance: BrushInstance) -> Self {
        PrimitiveInstance {
            data: [
                instance.prim_header_index.0,
                instance.clip_task_address.0 as i32,
                instance.segment_index |
                ((instance.edge_flags.bits() as i32) << 16) |
                ((instance.brush_flags.bits() as i32) << 24),
                0,
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
    // Get the palette ID for an identity transform.
    pub fn identity() -> TransformPaletteId {
        TransformPaletteId(0)
    }

    // Extract the transform kind from the id.
    pub fn transform_kind(&self) -> TransformedRectKind {
        if (self.0 >> 24) == 0 {
            TransformedRectKind::AxisAligned
        } else {
            TransformedRectKind::Complex
        }
    }
}

// The GPU data payload for a transform palette entry.
#[derive(Debug)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[repr(C)]
pub struct TransformData {
    pub transform: LayoutToWorldTransform,
    pub inv_transform: WorldToLayoutTransform,
}

impl TransformData {
    pub fn invalid() -> Self {
        TransformData {
            transform: LayoutToWorldTransform::identity(),
            inv_transform: WorldToLayoutTransform::identity(),
        }
    }
}

// Extra data stored about each transform palette entry.
pub struct TransformMetadata {
    pub transform_kind: TransformedRectKind,
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
}

#[derive(Copy, Debug, Clone, PartialEq)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct TransformIndex(pub u32);

impl TransformPalette {
    pub fn new(spatial_node_count: usize) -> TransformPalette {
        TransformPalette {
            transforms: Vec::with_capacity(spatial_node_count),
            metadata: Vec::with_capacity(spatial_node_count),
        }
    }

    // Set the local -> world transform for a given spatial
    // node in the transform palette.
    pub fn set(
        &mut self,
        index: TransformIndex,
        data: TransformData,
    ) {
        let index = index.0 as usize;

        // Pad the vectors out if they are not long enough to
        // account for this index. This can occur, for instance,
        // when we stop recursing down the CST due to encountering
        // a node with an invalid transform.
        while index >= self.transforms.len() {
            self.transforms.push(TransformData::invalid());
            self.metadata.push(TransformMetadata {
                transform_kind: TransformedRectKind::AxisAligned,
            });
        }

        // Store the transform itself, along with metadata about it.
        self.metadata[index] = TransformMetadata {
            transform_kind: data.transform.transform_kind(),
        };
        self.transforms[index] = data;
    }

    // Get a transform palette id for the given spatial node.
    // TODO(gw): In the future, it will be possible to specify
    //           a coordinate system id here, to allow retrieving
    //           transforms in the local space of a given spatial node.
    pub fn get_id(&self, index: TransformIndex) -> TransformPaletteId {
        let transform_kind = self.metadata[index.0 as usize].transform_kind as u32;
        TransformPaletteId(index.0 | (transform_kind << 24))
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
