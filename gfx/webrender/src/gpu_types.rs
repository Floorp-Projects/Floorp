/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::LayerRect;
use gpu_cache::GpuCacheAddress;
use render_task::RenderTaskAddress;
use tiling::PackedLayerIndex;

// Contains type that must exactly match the same structures declared in GLSL.

#[derive(Debug, Copy, Clone)]
pub struct PackedLayerAddress(i32);

impl From<PackedLayerIndex> for PackedLayerAddress {
    fn from(index: PackedLayerIndex) -> PackedLayerAddress {
        PackedLayerAddress(index.0 as i32)
    }
}

#[repr(i32)]
#[derive(Debug, Copy, Clone)]
pub enum BlurDirection {
    Horizontal = 0,
    Vertical,
}

#[derive(Debug)]
#[repr(C)]
pub struct BlurInstance {
    pub task_address: RenderTaskAddress,
    pub src_task_address: RenderTaskAddress,
    pub blur_direction: BlurDirection,
    pub region: LayerRect,
}

/// A clipping primitive drawn into the clipping mask.
/// Could be an image or a rectangle, which defines the
/// way `address` is treated.
#[derive(Debug, Copy, Clone)]
#[repr(C)]
pub struct ClipMaskInstance {
    pub render_task_address: RenderTaskAddress,
    pub layer_address: PackedLayerAddress,
    pub segment: i32,
    pub clip_data_address: GpuCacheAddress,
    pub resource_address: GpuCacheAddress,
}

// 32 bytes per instance should be enough for anyone!
#[derive(Debug, Clone)]
pub struct PrimitiveInstance {
    data: [i32; 8],
}

pub struct SimplePrimitiveInstance {
    pub specific_prim_address: GpuCacheAddress,
    pub task_address: RenderTaskAddress,
    pub clip_task_address: RenderTaskAddress,
    pub layer_address: PackedLayerAddress,
    pub z_sort_index: i32,
}

impl SimplePrimitiveInstance {
    pub fn new(
        specific_prim_address: GpuCacheAddress,
        task_address: RenderTaskAddress,
        clip_task_address: RenderTaskAddress,
        layer_address: PackedLayerAddress,
        z_sort_index: i32,
    ) -> SimplePrimitiveInstance {
        SimplePrimitiveInstance {
            specific_prim_address,
            task_address,
            clip_task_address,
            layer_address,
            z_sort_index,
        }
    }

    pub fn build(&self, data0: i32, data1: i32, data2: i32) -> PrimitiveInstance {
        PrimitiveInstance {
            data: [
                self.specific_prim_address.as_int(),
                self.task_address.0 as i32,
                self.clip_task_address.0 as i32,
                self.layer_address.0,
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
}

impl CompositePrimitiveInstance {
    pub fn new(
        task_address: RenderTaskAddress,
        src_task_address: RenderTaskAddress,
        backdrop_task_address: RenderTaskAddress,
        data0: i32,
        data1: i32,
        z: i32,
    ) -> CompositePrimitiveInstance {
        CompositePrimitiveInstance {
            task_address,
            src_task_address,
            backdrop_task_address,
            data0,
            data1,
            z,
        }
    }
}

impl From<CompositePrimitiveInstance> for PrimitiveInstance {
    fn from(instance: CompositePrimitiveInstance) -> PrimitiveInstance {
        PrimitiveInstance {
            data: [
                instance.task_address.0 as i32,
                instance.src_task_address.0 as i32,
                instance.backdrop_task_address.0 as i32,
                instance.z,
                instance.data0,
                instance.data1,
                0,
                0,
            ],
        }
    }
}

#[repr(C)]
pub struct BrushInstance {
    picture_address: RenderTaskAddress,
    prim_address: GpuCacheAddress,
}

impl BrushInstance {
    pub fn new(
        picture_address: RenderTaskAddress,
        prim_address: GpuCacheAddress
    ) -> BrushInstance {
        BrushInstance {
            picture_address,
            prim_address,
        }
    }
}

impl From<BrushInstance> for PrimitiveInstance {
    fn from(instance: BrushInstance) -> PrimitiveInstance {
        PrimitiveInstance {
            data: [
                instance.picture_address.0 as i32,
                instance.prim_address.as_int(),
                0,
                0,
                0,
                0,
                0,
                0,
            ]
        }
    }
}
