/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::units::PictureRect;
use crate::{spatial_tree::SpatialNodeIndex, render_task_graph::RenderTaskId, surface::SurfaceTileDescriptor, picture::TileKey, renderer::GpuBufferAddress, FastHashMap, prim_store::PrimitiveInstanceIndex, gpu_cache::GpuCacheAddress};
use crate::gpu_types::{QuadSegment, TransformPaletteId};
use crate::segment::EdgeAaSegmentMask;

/// A tightly packed command stored in a command buffer
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(Debug, Copy, Clone)]
pub struct Command(u32);

impl Command {
    /// Draw a simple primitive that needs prim instance index only.
    const CMD_DRAW_SIMPLE_PRIM: u32 = 0x00000000;
    /// Change the current spatial node.
    const CMD_SET_SPATIAL_NODE: u32 = 0x10000000;
    /// Draw a complex (3d-split) primitive, that has multiple GPU cache addresses.
    const CMD_DRAW_COMPLEX_PRIM: u32 = 0x20000000;
    /// Draw a primitive, that has a single GPU buffer addresses.
    const CMD_DRAW_INSTANCE: u32 = 0x30000000;
    /// Draw a generic quad primitive
    const CMD_DRAW_QUAD: u32 = 0x40000000;
    /// Set a list of variable-length segments
    const CMD_SET_SEGMENTS: u32 = 0x50000000;

    /// Bitmask for command bits of the command.
    const CMD_MASK: u32 = 0xf0000000;
    /// Bitmask for param bits of the command.
    const PARAM_MASK: u32 = 0x0fffffff;

    /// Encode drawing a simple primitive.
    fn draw_simple_prim(prim_instance_index: PrimitiveInstanceIndex) -> Self {
        Command(Command::CMD_DRAW_SIMPLE_PRIM | prim_instance_index.0)
    }

    /// Encode changing spatial node.
    fn set_spatial_node(spatial_node_index: SpatialNodeIndex) -> Self {
        Command(Command::CMD_SET_SPATIAL_NODE | spatial_node_index.0)
    }

    /// Encode a list of segments that follow
    fn set_segments(count: usize) -> Self {
        Command(Command::CMD_SET_SEGMENTS | count as u32)
    }

    /// Encode drawing a complex prim.
    fn draw_complex_prim(prim_instance_index: PrimitiveInstanceIndex) -> Self {
        Command(Command::CMD_DRAW_COMPLEX_PRIM | prim_instance_index.0)
    }

    fn draw_instance(prim_instance_index: PrimitiveInstanceIndex) -> Self {
        Command(Command::CMD_DRAW_INSTANCE | prim_instance_index.0)
    }

    /// Encode arbitrary data word.
    fn data(data: u32) -> Self {
        Command(data)
    }

    fn draw_quad(prim_instance_index: PrimitiveInstanceIndex) -> Self {
        Command(Command::CMD_DRAW_QUAD | prim_instance_index.0)
    }
}

bitflags! {
    /// Flags related to quad primitives
    #[repr(transparent)]
    #[cfg_attr(feature = "capture", derive(Serialize))]
    #[cfg_attr(feature = "replay", derive(Deserialize))]
    #[derive(Debug, Copy, PartialEq, Eq, Clone, PartialOrd, Ord, Hash)]
    pub struct QuadFlags : u8 {
        const IS_OPAQUE = 1 << 0;

        /// If true, the prim is 2d and we can apply a clip to the task rect in vertex shader
        const APPLY_DEVICE_CLIP = 1 << 1;

        /// If true, the device-pixel scale is already applied, so ignore in vertex shaders
        const IGNORE_DEVICE_PIXEL_SCALE = 1 << 2;

        /// If true, use segments for drawing the AA edges, to allow inner section to be opaque
        const USE_AA_SEGMENTS = 1 << 3;

        /// If true, apply texture sample as mask
        const SAMPLE_AS_MASK = 1 << 4;
    }
}

bitflags! {
    /// Defines the space that a quad primitive is drawn in
    #[repr(transparent)]
    #[cfg_attr(feature = "capture", derive(Serialize))]
    #[cfg_attr(feature = "replay", derive(Deserialize))]
    #[derive(Debug, Copy, PartialEq, Eq, Clone, PartialOrd, Ord, Hash)]
    pub struct MaskFlags : i32 {
        const PRIM_SPACE = 1 << 0;
    }
}

/// The unpacked equivalent to a `Command`.
#[cfg_attr(feature = "capture", derive(Serialize))]
pub enum PrimitiveCommand {
    Simple {
        prim_instance_index: PrimitiveInstanceIndex,
    },
    Complex {
        prim_instance_index: PrimitiveInstanceIndex,
        gpu_address: GpuCacheAddress,
    },
    Instance {
        prim_instance_index: PrimitiveInstanceIndex,
        gpu_buffer_address: GpuBufferAddress,
    },
    Quad {
        // TODO(gw): Used for bounding rect only, could possibly remove
        prim_instance_index: PrimitiveInstanceIndex,
        gpu_buffer_address: GpuBufferAddress,
        transform_id: TransformPaletteId,
        quad_flags: QuadFlags,
        edge_flags: EdgeAaSegmentMask,
    },
}

impl PrimitiveCommand {
    pub fn simple(
        prim_instance_index: PrimitiveInstanceIndex,
    ) -> Self {
        PrimitiveCommand::Simple {
            prim_instance_index,
        }
    }

    pub fn complex(
        prim_instance_index: PrimitiveInstanceIndex,
        gpu_address: GpuCacheAddress,
    ) -> Self {
        PrimitiveCommand::Complex {
            prim_instance_index,
            gpu_address,
        }
    }

    pub fn quad(
        prim_instance_index: PrimitiveInstanceIndex,
        gpu_buffer_address: GpuBufferAddress,
        transform_id: TransformPaletteId,
        quad_flags: QuadFlags,
        edge_flags: EdgeAaSegmentMask,
    ) -> Self {
        PrimitiveCommand::Quad {
            prim_instance_index,
            gpu_buffer_address,
            transform_id,
            quad_flags,
            edge_flags,
        }
    }

    pub fn instance(
        prim_instance_index: PrimitiveInstanceIndex,
        gpu_buffer_address: GpuBufferAddress,
    ) -> Self {
        PrimitiveCommand::Instance {
            prim_instance_index,
            gpu_buffer_address,
        }
    }
}


/// A list of commands describing how to draw a primitive list.
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct CommandBuffer {
    /// The encoded drawing commands.
    commands: Vec<Command>,
    /// Cached current spatial node.
    current_spatial_node_index: SpatialNodeIndex,
}

impl CommandBuffer {
    /// Construct a new cmd buffer.
    pub fn new() -> Self {
        CommandBuffer {
            commands: Vec::new(),
            current_spatial_node_index: SpatialNodeIndex::INVALID,
        }
    }

    /// Push a list of segments in to the cmd buffer
    pub fn set_segments(
        &mut self,
        segments: &[QuadSegment],
    ) {
        self.commands.push(Command::set_segments(segments.len()));
        for segment in segments {
            self.commands.push(Command::data(segment.task_id.index));
        }
    }

    /// Add a primitive to the command buffer.
    pub fn add_prim(
        &mut self,
        prim_cmd: &PrimitiveCommand,
        spatial_node_index: SpatialNodeIndex,
    ) {
        if self.current_spatial_node_index != spatial_node_index {
            self.commands.push(Command::set_spatial_node(spatial_node_index));
            self.current_spatial_node_index = spatial_node_index;
        }

        self.add_cmd(prim_cmd);
    }

    /// Add a cmd to the command buffer.
    pub fn add_cmd(
        &mut self,
        prim_cmd: &PrimitiveCommand,
    ) {
        match *prim_cmd {
            PrimitiveCommand::Simple { prim_instance_index } => {
                self.commands.push(Command::draw_simple_prim(prim_instance_index));
            }
            PrimitiveCommand::Complex { prim_instance_index, gpu_address } => {
                self.commands.push(Command::draw_complex_prim(prim_instance_index));
                self.commands.push(Command::data((gpu_address.u as u32) << 16 | gpu_address.v as u32));
            }
            PrimitiveCommand::Instance { prim_instance_index, gpu_buffer_address } => {
                self.commands.push(Command::draw_instance(prim_instance_index));
                self.commands.push(Command::data((gpu_buffer_address.u as u32) << 16 | gpu_buffer_address.v as u32));
            }
            PrimitiveCommand::Quad { prim_instance_index, gpu_buffer_address, transform_id, quad_flags, edge_flags } => {
                self.commands.push(Command::draw_quad(prim_instance_index));
                self.commands.push(Command::data((gpu_buffer_address.u as u32) << 16 | gpu_buffer_address.v as u32));
                self.commands.push(Command::data(transform_id.0));
                self.commands.push(Command::data((quad_flags.bits() as u32) << 16 | edge_flags.bits() as u32));
            }
        }
    }

    /// Iterate the command list, calling a provided closure for each primitive draw command.
    pub fn iter_prims<F>(
        &self,
        f: &mut F,
    ) where F: FnMut(&PrimitiveCommand, SpatialNodeIndex, &[RenderTaskId]) {
        let mut current_spatial_node_index = SpatialNodeIndex::INVALID;
        let mut cmd_iter = self.commands.iter();
        // TODO(gw): Consider pre-allocating this / Smallvec if it shows up in profiles.
        let mut segments = Vec::new();

        while let Some(cmd) = cmd_iter.next() {
            let command = cmd.0 & Command::CMD_MASK;
            let param = cmd.0 & Command::PARAM_MASK;

            match command {
                Command::CMD_DRAW_SIMPLE_PRIM => {
                    let prim_instance_index = PrimitiveInstanceIndex(param);
                    let cmd = PrimitiveCommand::simple(prim_instance_index);
                    f(&cmd, current_spatial_node_index, &[]);
                }
                Command::CMD_SET_SPATIAL_NODE => {
                    current_spatial_node_index = SpatialNodeIndex(param);
                }
                Command::CMD_DRAW_COMPLEX_PRIM => {
                    let prim_instance_index = PrimitiveInstanceIndex(param);
                    let data = cmd_iter.next().unwrap();
                    let gpu_address = GpuCacheAddress {
                        u: (data.0 >> 16) as u16,
                        v: (data.0 & 0xffff) as u16,
                    };
                    let cmd = PrimitiveCommand::complex(
                        prim_instance_index,
                        gpu_address,
                    );
                    f(&cmd, current_spatial_node_index, &[]);
                }
                Command::CMD_DRAW_QUAD => {
                    let prim_instance_index = PrimitiveInstanceIndex(param);
                    let data = cmd_iter.next().unwrap();
                    let transform_id = TransformPaletteId(cmd_iter.next().unwrap().0);
                    let bits = cmd_iter.next().unwrap().0;
                    let quad_flags = QuadFlags::from_bits((bits >> 16) as u8).unwrap();
                    let edge_flags = EdgeAaSegmentMask::from_bits((bits & 0xff) as u8).unwrap();
                    let gpu_buffer_address = GpuBufferAddress {
                        u: (data.0 >> 16) as u16,
                        v: (data.0 & 0xffff) as u16,
                    };
                    let cmd = PrimitiveCommand::quad(
                        prim_instance_index,
                        gpu_buffer_address,
                        transform_id,
                        quad_flags,
                        edge_flags,
                    );
                    f(&cmd, current_spatial_node_index, &segments);
                    segments.clear()
                }
                Command::CMD_DRAW_INSTANCE => {
                    let prim_instance_index = PrimitiveInstanceIndex(param);
                    let data = cmd_iter.next().unwrap();
                    let gpu_buffer_address = GpuBufferAddress {
                        u: (data.0 >> 16) as u16,
                        v: (data.0 & 0xffff) as u16,
                    };
                    let cmd = PrimitiveCommand::instance(
                        prim_instance_index,
                        gpu_buffer_address,
                    );
                    f(&cmd, current_spatial_node_index, &[]);
                }
                Command::CMD_SET_SEGMENTS => {
                    let count = param;
                    for _ in 0 .. count {
                        segments.push(RenderTaskId { index: cmd_iter.next().unwrap().0 });
                    }
                }
                _ => {
                    unreachable!();
                }
            }
        }
    }
}

/// Abstracts whether a command buffer is being built for a tiled (picture cache)
/// or simple (child surface).
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub enum CommandBufferBuilderKind {
    Tiled {
        // TODO(gw): It might be worth storing this as a 2d-array instead
        //           of a hash map if it ever shows up in profiles. This is
        //           slightly complicated by the sub_slice_index in the
        //           TileKey structure - could have a 2 level array?
        tiles: FastHashMap<TileKey, SurfaceTileDescriptor>,
    },
    Simple {
        render_task_id: RenderTaskId,
        root_task_id: Option<RenderTaskId>,
        dirty_rect: PictureRect,
    },
    Invalid,
}

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct CommandBufferBuilder {
    pub kind: CommandBufferBuilderKind,

    /// If a command buffer establishes a sub-graph, then at the end of constructing
    /// the surface, the parent surface is supplied as an input dependency, and the
    /// parent surface gets a duplicated (existing) task with the same location, and
    /// with the sub-graph output as an input dependency.
    pub establishes_sub_graph: bool,

    /// If this surface builds a sub-graph, it will mark a task in the filter sub-graph
    /// as a resolve source for the input from the parent surface.
    pub resolve_source: Option<RenderTaskId>,

    /// List of render tasks that depend on the task that will be created for this builder.
    pub extra_dependencies: Vec<RenderTaskId>,
}

impl CommandBufferBuilder {
    pub fn empty() -> Self {
        CommandBufferBuilder {
            kind: CommandBufferBuilderKind::Invalid,
            establishes_sub_graph: false,
            resolve_source: None,
            extra_dependencies: Vec::new(),
        }
    }

    /// Construct a tiled command buffer builder.
    pub fn new_tiled(
        tiles: FastHashMap<TileKey, SurfaceTileDescriptor>,
    ) -> Self {
        CommandBufferBuilder {
            kind: CommandBufferBuilderKind::Tiled {
                tiles,
            },
            establishes_sub_graph: false,
            resolve_source: None,
            extra_dependencies: Vec::new(),
        }
    }

    /// Construct a simple command buffer builder.
    pub fn new_simple(
        render_task_id: RenderTaskId,
        establishes_sub_graph: bool,
        root_task_id: Option<RenderTaskId>,
        dirty_rect: PictureRect,
    ) -> Self {
        CommandBufferBuilder {
            kind: CommandBufferBuilderKind::Simple {
                render_task_id,
                root_task_id,
                dirty_rect,
            },
            establishes_sub_graph,
            resolve_source: None,
            extra_dependencies: Vec::new(),
        }
    }
}

// Index into a command buffer stored in a `CommandBufferList`.
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(Debug, Copy, Clone)]
pub struct CommandBufferIndex(pub u32);

// Container for a list of command buffers that are built for a frame.
pub struct CommandBufferList {
    cmd_buffers: Vec<CommandBuffer>,
}

impl CommandBufferList {
    pub fn new() -> Self {
        CommandBufferList {
            cmd_buffers: Vec::new(),
        }
    }

    pub fn create_cmd_buffer(
        &mut self,
    ) -> CommandBufferIndex {
        let index = CommandBufferIndex(self.cmd_buffers.len() as u32);
        self.cmd_buffers.push(CommandBuffer::new());
        index
    }

    pub fn get(&self, index: CommandBufferIndex) -> &CommandBuffer {
        &self.cmd_buffers[index.0 as usize]
    }

    pub fn get_mut(&mut self, index: CommandBufferIndex) -> &mut CommandBuffer {
        &mut self.cmd_buffers[index.0 as usize]
    }
}
