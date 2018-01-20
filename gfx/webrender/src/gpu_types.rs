/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{LayerToWorldTransform};
use gpu_cache::GpuCacheAddress;
use prim_store::EdgeAaSegmentMask;
use render_task::RenderTaskAddress;

// Contains type that must exactly match the same structures declared in GLSL.

#[repr(i32)]
#[derive(Debug, Copy, Clone)]
#[cfg_attr(feature = "capture", derive(Deserialize, Serialize))]
pub enum BlurDirection {
    Horizontal = 0,
    Vertical,
}

#[derive(Debug)]
#[repr(C)]
#[cfg_attr(feature = "capture", derive(Deserialize, Serialize))]
pub struct BlurInstance {
    pub task_address: RenderTaskAddress,
    pub src_task_address: RenderTaskAddress,
    pub blur_direction: BlurDirection,
}

/// A clipping primitive drawn into the clipping mask.
/// Could be an image or a rectangle, which defines the
/// way `address` is treated.
#[derive(Debug, Copy, Clone)]
#[cfg_attr(feature = "capture", derive(Deserialize, Serialize))]
#[repr(C)]
pub struct ClipMaskInstance {
    pub render_task_address: RenderTaskAddress,
    pub scroll_node_data_index: ClipScrollNodeIndex,
    pub segment: i32,
    pub clip_data_address: GpuCacheAddress,
    pub resource_address: GpuCacheAddress,
}

// 32 bytes per instance should be enough for anyone!
#[derive(Debug, Clone)]
#[cfg_attr(feature = "capture", derive(Deserialize, Serialize))]
pub struct PrimitiveInstance {
    data: [i32; 8],
}

pub struct SimplePrimitiveInstance {
    pub specific_prim_address: GpuCacheAddress,
    pub task_address: RenderTaskAddress,
    pub clip_task_address: RenderTaskAddress,
    pub clip_chain_rect_index: ClipChainRectIndex,
    pub scroll_id: ClipScrollNodeIndex,
    pub z_sort_index: i32,
}

impl SimplePrimitiveInstance {
    pub fn new(
        specific_prim_address: GpuCacheAddress,
        task_address: RenderTaskAddress,
        clip_task_address: RenderTaskAddress,
        clip_chain_rect_index: ClipChainRectIndex,
        scroll_id: ClipScrollNodeIndex,
        z_sort_index: i32,
    ) -> Self {
        SimplePrimitiveInstance {
            specific_prim_address,
            task_address,
            clip_task_address,
            clip_chain_rect_index,
            scroll_id,
            z_sort_index,
        }
    }

    pub fn build(&self, data0: i32, data1: i32, data2: i32) -> PrimitiveInstance {
        PrimitiveInstance {
            data: [
                self.specific_prim_address.as_int(),
                self.task_address.0 as i32,
                self.clip_task_address.0 as i32,
                ((self.clip_chain_rect_index.0 as i32) << 16) | self.scroll_id.0 as i32,
                self.z_sort_index,
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
    pub z: i32,
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
        z: i32,
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
                instance.z,
                instance.data0,
                instance.data1,
                instance.data2,
                instance.data3,
            ],
        }
    }
}

// TODO(gw): While we are comverting things over, we
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
    pub z: i32,
    pub segment_index: i32,
    pub edge_flags: EdgeAaSegmentMask,
    pub user_data0: i32,
    pub user_data1: i32,
}

impl From<BrushInstance> for PrimitiveInstance {
    fn from(instance: BrushInstance) -> Self {
        PrimitiveInstance {
            data: [
                instance.picture_address.0 as i32,
                instance.prim_address.as_int(),
                ((instance.clip_chain_rect_index.0 as i32) << 16) | instance.scroll_id.0 as i32,
                instance.clip_task_address.0 as i32,
                instance.z,
                instance.segment_index | ((instance.edge_flags.bits() as i32) << 16),
                instance.user_data0,
                instance.user_data1,
            ]
        }
    }
}

// Defines how a brush image is stretched onto the primitive.
// In the future, we may draw with segments for each portion
// of the primitive, in which case this will be redundant.
#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub enum BrushImageKind {
    Simple = 0,     // A normal rect
    NinePatch = 1,  // A nine-patch image (stretch inside segments)
    Mirror = 2,     // A top left corner only (mirror across x/y axes)
}

#[derive(Copy, Debug, Clone, PartialEq)]
#[cfg_attr(feature = "capture", derive(Deserialize, Serialize))]
#[repr(C)]
pub struct ClipScrollNodeIndex(pub u32);

#[derive(Debug)]
#[cfg_attr(feature = "capture", derive(Deserialize, Serialize))]
#[repr(C)]
pub struct ClipScrollNodeData {
    pub transform: LayerToWorldTransform,
    pub transform_kind: f32,
    pub padding: [f32; 3],
}

impl ClipScrollNodeData {
    pub fn invalid() -> Self {
        ClipScrollNodeData {
            transform: LayerToWorldTransform::identity(),
            transform_kind: 0.0,
            padding: [0.0; 3],
        }
    }
}

#[derive(Copy, Debug, Clone, PartialEq)]
#[repr(C)]
pub struct ClipChainRectIndex(pub usize);

#[derive(Copy, Debug, Clone, PartialEq)]
#[cfg_attr(feature = "capture", derive(Deserialize, Serialize))]
#[repr(C)]
pub enum PictureType {
    Image = 1,
    TextShadow = 2,
    BoxShadow = 3,
}
