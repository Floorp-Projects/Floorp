/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{DevicePoint, DeviceSize, DeviceRect, LayoutToWorldTransform};
use api::{PremultipliedColorF, WorldToLayoutTransform};
use gpu_cache::{GpuCacheAddress, GpuDataRequest};
use prim_store::{VECS_PER_SEGMENT, EdgeAaSegmentMask};
use render_task::RenderTaskAddress;
use renderer::MAX_VERTEX_TEXTURE_WIDTH;

// Contains type that must exactly match the same structures declared in GLSL.

const INT_BITS: usize = 31; //TODO: convert to unsigned
const CLIP_CHAIN_RECT_BITS: usize = 22;
const SEGMENT_BITS: usize = INT_BITS - CLIP_CHAIN_RECT_BITS;
// The guard ensures (at compile time) that the designated number of bits cover
// the maximum supported segment count for the texture width.
const _SEGMENT_GUARD: usize = (1 << SEGMENT_BITS) * VECS_PER_SEGMENT - MAX_VERTEX_TEXTURE_WIDTH;
const EDGE_FLAG_BITS: usize = 4;
const BRUSH_FLAG_BITS: usize = 4;
const CLIP_SCROLL_INDEX_BITS: usize = INT_BITS - EDGE_FLAG_BITS - BRUSH_FLAG_BITS;

#[derive(Copy, Clone, Debug)]
#[repr(C)]
pub struct ZBufferId(i32);

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
    pub scroll_node_data_index: ClipScrollNodeIndex,
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

// 32 bytes per instance should be enough for anyone!
#[derive(Debug, Clone)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct PrimitiveInstance {
    data: [i32; 8],
}

pub struct SimplePrimitiveInstance {
    pub specific_prim_address: GpuCacheAddress,
    pub task_address: RenderTaskAddress,
    pub clip_task_address: RenderTaskAddress,
    pub clip_chain_rect_index: ClipChainRectIndex,
    pub scroll_id: ClipScrollNodeIndex,
    pub z: ZBufferId,
}

impl SimplePrimitiveInstance {
    pub fn new(
        specific_prim_address: GpuCacheAddress,
        task_address: RenderTaskAddress,
        clip_task_address: RenderTaskAddress,
        clip_chain_rect_index: ClipChainRectIndex,
        scroll_id: ClipScrollNodeIndex,
        z: ZBufferId,
    ) -> Self {
        SimplePrimitiveInstance {
            specific_prim_address,
            task_address,
            clip_task_address,
            clip_chain_rect_index,
            scroll_id,
            z,
        }
    }

    pub fn build(&self, data0: i32, data1: i32, data2: i32) -> PrimitiveInstance {
        PrimitiveInstance {
            data: [
                self.specific_prim_address.as_int(),
                self.task_address.0 as i32 | (self.clip_task_address.0 as i32) << 16,
                self.clip_chain_rect_index.0 as i32,
                self.scroll_id.0 as i32,
                self.z.0,
                data0,
                data1,
                data2,
            ],
        }
    }
}

pub struct CompositePrimitiveInstance {
    pub task_address: RenderTaskAddress,
    pub src_task_address: RenderTaskAddress,
    pub backdrop_task_address: RenderTaskAddress,
    pub data0: i32,
    pub data1: i32,
    pub z: ZBufferId,
    pub data2: i32,
    pub data3: i32,
}

impl CompositePrimitiveInstance {
    pub fn new(
        task_address: RenderTaskAddress,
        src_task_address: RenderTaskAddress,
        backdrop_task_address: RenderTaskAddress,
        data0: i32,
        data1: i32,
        z: ZBufferId,
        data2: i32,
        data3: i32,
    ) -> Self {
        CompositePrimitiveInstance {
            task_address,
            src_task_address,
            backdrop_task_address,
            data0,
            data1,
            z,
            data2,
            data3,
        }
    }
}

impl From<CompositePrimitiveInstance> for PrimitiveInstance {
    fn from(instance: CompositePrimitiveInstance) -> Self {
        PrimitiveInstance {
            data: [
                instance.task_address.0 as i32,
                instance.src_task_address.0 as i32,
                instance.backdrop_task_address.0 as i32,
                instance.z.0,
                instance.data0,
                instance.data1,
                instance.data2,
                instance.data3,
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

// TODO(gw): While we are converting things over, we
//           need to have the instance be the same
//           size as an old PrimitiveInstance. In the
//           future, we can compress this vertex
//           format a lot - e.g. z, render task
//           addresses etc can reasonably become
//           a u16 type.
#[repr(C)]
pub struct BrushInstance {
    pub picture_address: RenderTaskAddress,
    pub prim_address: GpuCacheAddress,
    pub clip_chain_rect_index: ClipChainRectIndex,
    pub scroll_id: ClipScrollNodeIndex,
    pub clip_task_address: RenderTaskAddress,
    pub z: ZBufferId,
    pub segment_index: i32,
    pub edge_flags: EdgeAaSegmentMask,
    pub brush_flags: BrushFlags,
    pub user_data: [i32; 3],
}

impl From<BrushInstance> for PrimitiveInstance {
    fn from(instance: BrushInstance) -> Self {
        debug_assert_eq!(0, instance.clip_chain_rect_index.0 >> CLIP_CHAIN_RECT_BITS);
        debug_assert_eq!(0, instance.scroll_id.0 >> CLIP_SCROLL_INDEX_BITS);
        debug_assert_eq!(0, instance.segment_index >> SEGMENT_BITS);
        PrimitiveInstance {
            data: [
                instance.picture_address.0 as i32 | (instance.clip_task_address.0 as i32) << 16,
                instance.prim_address.as_int(),
                instance.clip_chain_rect_index.0 as i32 | (instance.segment_index << CLIP_CHAIN_RECT_BITS),
                instance.z.0,
                instance.scroll_id.0 as i32 |
                    ((instance.edge_flags.bits() as i32) << CLIP_SCROLL_INDEX_BITS) |
                    ((instance.brush_flags.bits() as i32) << (CLIP_SCROLL_INDEX_BITS + EDGE_FLAG_BITS)),
                instance.user_data[0],
                instance.user_data[1],
                instance.user_data[2],
            ]
        }
    }
}

#[derive(Copy, Debug, Clone, PartialEq)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[repr(C)]
pub struct ClipScrollNodeIndex(pub u32);

#[derive(Debug)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[repr(C)]
pub struct ClipScrollNodeData {
    pub transform: LayoutToWorldTransform,
    pub inv_transform: WorldToLayoutTransform,
    pub transform_kind: f32,
    pub padding: [f32; 3],
}

impl ClipScrollNodeData {
    pub fn invalid() -> Self {
        ClipScrollNodeData {
            transform: LayoutToWorldTransform::identity(),
            inv_transform: WorldToLayoutTransform::identity(),
            transform_kind: 0.0,
            padding: [0.0; 3],
        }
    }
}

#[derive(Copy, Debug, Clone, PartialEq)]
#[repr(C)]
pub struct ClipChainRectIndex(pub usize);

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
