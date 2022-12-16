/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::{spatial_tree::SpatialNodeIndex, render_task_graph::RenderTaskId, surface::SurfaceTileDescriptor, picture::TileKey, renderer::GpuBufferAddress, FastHashMap, prim_store::PrimitiveInstanceIndex, gpu_cache::GpuCacheAddress};

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
    const CMD_DRAW_INSTANCE: u32 = 0x40000000;

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
}

/// The unpacked equivalent to a `Command`.
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
        }
    }

    /// Iterate the command list, calling a provided closure for each primitive draw command.
    pub fn iter_prims<F>(
        &self,
        f: &mut F,
    ) where F: FnMut(&PrimitiveCommand, SpatialNodeIndex) {
        let mut current_spatial_node_index = SpatialNodeIndex::INVALID;
        let mut cmd_iter = self.commands.iter();

        while let Some(cmd) = cmd_iter.next() {
            let command = cmd.0 & Command::CMD_MASK;
            let param = cmd.0 & Command::PARAM_MASK;

            match command {
                Command::CMD_DRAW_SIMPLE_PRIM => {
                    let prim_instance_index = PrimitiveInstanceIndex(param);
                    let cmd = PrimitiveCommand::simple(prim_instance_index);
                    f(&cmd, current_spatial_node_index);
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
                    f(&cmd, current_spatial_node_index);
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
                    f(&cmd, current_spatial_node_index);
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
    ) -> Self {
        CommandBufferBuilder {
            kind: CommandBufferBuilderKind::Simple {
                render_task_id,
                root_task_id,
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
