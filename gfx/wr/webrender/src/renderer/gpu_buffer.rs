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
use api::units::{DeviceIntRect, DeviceIntSize, LayoutRect, PictureRect, DeviceRect};
use api::{PremultipliedColorF};
use crate::device::Texel;
use crate::render_task_graph::{RenderTaskGraph, RenderTaskId};


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

    pub const INVALID: GpuBufferAddress = GpuBufferAddress { u: !0, v: !0 };
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

impl Into<GpuBufferBlock> for PictureRect {
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

impl Into<GpuBufferBlock> for DeviceRect {
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

impl Into<GpuBufferBlock> for DeviceIntRect {
    fn into(self) -> GpuBufferBlock {
        GpuBufferBlock {
            data: [
                self.min.x as f32,
                self.min.y as f32,
                self.max.x as f32,
                self.max.y as f32,
            ],
        }
    }
}

impl Into<GpuBufferBlock> for [f32; 4] {
    fn into(self) -> GpuBufferBlock {
        GpuBufferBlock {
            data: self,
        }
    }
}

/// Record a patch to the GPU buffer for a render task
struct DeferredBlock {
    task_id: RenderTaskId,
    index: usize,
}

/// Interface to allow writing multiple GPU blocks, possibly of different types
pub struct GpuBufferWriter<'a> {
    buffer: &'a mut Vec<GpuBufferBlock>,
    deferred: &'a mut Vec<DeferredBlock>,
    index: usize,
    block_count: usize,
}

impl<'a> GpuBufferWriter<'a> {
    fn new(
        buffer: &'a mut Vec<GpuBufferBlock>,
        deferred: &'a mut Vec<DeferredBlock>,
        index: usize,
        block_count: usize,
    ) -> Self {
        GpuBufferWriter {
            buffer,
            deferred,
            index,
            block_count,
        }
    }

    /// Push one (16 byte) block of data in to the writer
    pub fn push_one<B>(&mut self, block: B) where B: Into<GpuBufferBlock> {
        self.buffer.push(block.into());
    }

    /// Push a reference to a render task in to the writer. Once the render
    /// task graph is resolved, this will be patched with the UV rect of the task
    pub fn push_render_task(&mut self, task_id: RenderTaskId) {
        self.deferred.push(DeferredBlock {
            task_id,
            index: self.buffer.len(),
        });
        self.buffer.push(GpuBufferBlock::EMPTY);
    }

    /// Close this writer, returning the GPU address of this set of block(s).
    pub fn finish(self) -> GpuBufferAddress {
        assert_eq!(self.buffer.len(), self.index + self.block_count);

        GpuBufferAddress {
            u: (self.index % MAX_VERTEX_TEXTURE_WIDTH) as u16,
            v: (self.index / MAX_VERTEX_TEXTURE_WIDTH) as u16,
        }
    }
}

impl<'a> Drop for GpuBufferWriter<'a> {
    fn drop(&mut self) {
        assert_eq!(self.buffer.len(), self.index + self.block_count, "Claimed block_count was not written");
    }
}

pub struct GpuBufferBuilder {
    data: Vec<GpuBufferBlock>,
    deferred: Vec<DeferredBlock>,
}

impl GpuBufferBuilder {
    pub fn new() -> Self {
        GpuBufferBuilder {
            data: Vec::new(),
            deferred: Vec::new(),
        }
    }

    #[allow(dead_code)]
    pub fn push(
        &mut self,
        blocks: &[GpuBufferBlock],
    ) -> GpuBufferAddress {
        assert!(blocks.len() <= MAX_VERTEX_TEXTURE_WIDTH);

        if (self.data.len() % MAX_VERTEX_TEXTURE_WIDTH) + blocks.len() > MAX_VERTEX_TEXTURE_WIDTH {
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

    /// Begin writing a specific number of blocks
    pub fn write_blocks(
        &mut self,
        block_count: usize,
    ) -> GpuBufferWriter {
        assert!(block_count <= MAX_VERTEX_TEXTURE_WIDTH);

        if (self.data.len() % MAX_VERTEX_TEXTURE_WIDTH) + block_count > MAX_VERTEX_TEXTURE_WIDTH {
            while self.data.len() % MAX_VERTEX_TEXTURE_WIDTH != 0 {
                self.data.push(GpuBufferBlock::EMPTY);
            }
        }

        let index = self.data.len();

        GpuBufferWriter::new(
            &mut self.data,
            &mut self.deferred,
            index,
            block_count,
        )
    }

    pub fn finalize(
        mut self,
        render_tasks: &RenderTaskGraph,
    ) -> GpuBuffer {
        let required_len = (self.data.len() + MAX_VERTEX_TEXTURE_WIDTH-1) & !(MAX_VERTEX_TEXTURE_WIDTH-1);

        for _ in 0 .. required_len - self.data.len() {
            self.data.push(GpuBufferBlock::EMPTY);
        }

        let len = self.data.len();
        assert!(len % MAX_VERTEX_TEXTURE_WIDTH == 0);

        // At this point, we know that the render task graph has been built, and we can
        // query the location of any dynamic (render target) or static (texture cache)
        // task. This allows us to patch the UV rects in to the GPU buffer before upload
        // to the GPU.
        for block in self.deferred.drain(..) {
            let render_task = &render_tasks[block.task_id];
            let target_rect = render_task.get_target_rect();
            self.data[block.index] = target_rect.into();
        }

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


#[test]
fn test_gpu_buffer_sizing_push() {
    let render_task_graph = RenderTaskGraph::new_for_testing();
    let mut builder = GpuBufferBuilder::new();

    let row = vec![GpuBufferBlock::EMPTY; MAX_VERTEX_TEXTURE_WIDTH];
    builder.push(&row);

    builder.push(&[GpuBufferBlock::EMPTY]);
    builder.push(&[GpuBufferBlock::EMPTY]);

    let buffer = builder.finalize(&render_task_graph);
    assert_eq!(buffer.data.len(), MAX_VERTEX_TEXTURE_WIDTH * 2);
}

#[test]
fn test_gpu_buffer_sizing_writer() {
    let render_task_graph = RenderTaskGraph::new_for_testing();
    let mut builder = GpuBufferBuilder::new();

    let mut writer = builder.write_blocks(MAX_VERTEX_TEXTURE_WIDTH);
    for _ in 0 .. MAX_VERTEX_TEXTURE_WIDTH {
        writer.push_one(GpuBufferBlock::EMPTY);
    }
    writer.finish();

    let mut writer = builder.write_blocks(1);
    writer.push_one(GpuBufferBlock::EMPTY);
    writer.finish();

    let mut writer = builder.write_blocks(1);
    writer.push_one(GpuBufferBlock::EMPTY);
    writer.finish();

    let buffer = builder.finalize(&render_task_graph);
    assert_eq!(buffer.data.len(), MAX_VERTEX_TEXTURE_WIDTH * 2);
}
