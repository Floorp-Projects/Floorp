/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*

    TODO:
        Recycle GpuBuffers in a pool (support return from render thread)
        Efficiently allow writing to buffer (better push interface)
        Support other texel types (e.g. i32)

 */

use crate::renderer::MAX_VERTEX_TEXTURE_WIDTH;
use api::units::{DeviceIntSize, LayoutRect};
use api::{ColorF, PremultipliedColorF};
use crate::device::Texel;

unsafe impl Texel for GpuBufferBlock {}

/// A single texel in RGBAF32 texture - 16 bytes.
#[derive(Copy, Clone, Debug, MallocSizeOf)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct GpuBufferBlock {
    data: [f32; 4],
}

#[derive(Copy, Debug, Clone, MallocSizeOf, Eq, PartialEq)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct GpuBufferAddress {
    pub u: u16,
    pub v: u16,
}

impl GpuBufferAddress {
    #[allow(dead_code)]
    pub fn as_int(self) -> i32 {
        // TODO(gw): Temporarily encode GPU Cache addresses as a single int.
        //           In the future, we can change the PrimitiveInstanceData struct
        //           to use 2x u16 for the vertex attribute instead of an i32.
        self.v as i32 * MAX_VERTEX_TEXTURE_WIDTH as i32 + self.u as i32
    }
}

impl GpuBufferBlock {
    pub const EMPTY: Self = GpuBufferBlock { data: [0.0; 4] };
}

impl Into<GpuBufferBlock> for LayoutRect {
    fn into(self) -> GpuBufferBlock {
        GpuBufferBlock {
            data: [
                self.min.x,
                self.min.y,
                self.max.x,
                self.max.y,
            ],
        }
    }
}

impl Into<GpuBufferBlock> for ColorF {
    fn into(self) -> GpuBufferBlock {
        GpuBufferBlock {
            data: [
                self.r,
                self.g,
                self.b,
                self.a,
            ],
        }
    }
}

impl Into<GpuBufferBlock> for PremultipliedColorF {
    fn into(self) -> GpuBufferBlock {
        GpuBufferBlock {
            data: [
                self.r,
                self.g,
                self.b,
                self.a,
            ],
        }
    }
}

pub struct GpuBufferBuilder {
    data: Vec<GpuBufferBlock>,
}

impl GpuBufferBuilder {
    pub fn new() -> Self {
        GpuBufferBuilder {
            data: Vec::new(),
        }
    }

    #[allow(dead_code)]
    pub fn push(
        &mut self,
        blocks: &[GpuBufferBlock],
    ) -> GpuBufferAddress {
        assert!(blocks.len() < MAX_VERTEX_TEXTURE_WIDTH);

        if self.data.len() + blocks.len() >= MAX_VERTEX_TEXTURE_WIDTH {
            while self.data.len() % MAX_VERTEX_TEXTURE_WIDTH != 0 {
                self.data.push(GpuBufferBlock::EMPTY);
            }
        }

        let index = self.data.len();

        self.data.extend_from_slice(blocks);

        GpuBufferAddress {
            u: (index % MAX_VERTEX_TEXTURE_WIDTH) as u16,
            v: (index / MAX_VERTEX_TEXTURE_WIDTH) as u16,
        }
    }

    pub fn finalize(mut self) -> GpuBuffer {
        let required_len = (self.data.len() + MAX_VERTEX_TEXTURE_WIDTH-1) & !(MAX_VERTEX_TEXTURE_WIDTH-1);

        for _ in 0 .. required_len - self.data.len() {
            self.data.push(GpuBufferBlock::EMPTY);
        }

        let len = self.data.len();
        assert!(len % MAX_VERTEX_TEXTURE_WIDTH == 0);

        GpuBuffer {
            data: self.data,
            size: DeviceIntSize::new(MAX_VERTEX_TEXTURE_WIDTH as i32, (len / MAX_VERTEX_TEXTURE_WIDTH) as i32),
        }
    }
}

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct GpuBuffer {
    pub data: Vec<GpuBufferBlock>,
    pub size: DeviceIntSize,
}

impl GpuBuffer {
    pub fn is_empty(&self) -> bool {
        self.data.is_empty()
    }
}
