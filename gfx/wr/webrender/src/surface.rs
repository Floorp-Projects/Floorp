/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::units::*;
use crate::batch::{CommandBufferBuilderKind, CommandBufferList, CommandBufferBuilder, CommandBufferIndex};
use crate::internal_types::FastHashMap;
use crate::picture::{SurfaceInfo, SurfaceIndex, TileKey, SubSliceIndex};
use crate::prim_store::{PrimitiveInstanceIndex};
use crate::render_task::RenderTaskKind;
use crate::render_task_graph::{RenderTaskId, RenderTaskGraphBuilder};
use crate::spatial_tree::SpatialNodeIndex;
use crate::visibility::{VisibilityState, PrimitiveVisibility};

/*
 Contains functionality to help building the render task graph from a series of off-screen
 surfaces that are created during the prepare pass. For now, it maintains existing behavior.
 A future patch will add support for surface sub-graphs, while ensuring the render task
 graph itself is built correctly with dependencies regardless of the surface kind (chained,
 tiled, simple).
 */

// Details of how a surface is rendered
pub enum SurfaceDescriptorKind {
    // Picture cache tiles
    Tiled {
        tiles: FastHashMap<TileKey, RenderTaskId>,
    },
    // A single surface (e.g. for an opacity filter)
    Simple {
        render_task_id: RenderTaskId,
    },
    // A surface with 1+ intermediate tasks (e.g. blur)
    Chained {
        render_task_id: RenderTaskId,
        root_task_id: RenderTaskId,
    },
}

// Describes how a surface is rendered
pub struct SurfaceDescriptor {
    kind: SurfaceDescriptorKind,
}

impl SurfaceDescriptor {
    // Create a picture cache tiled surface
    pub fn new_tiled(
        tiles: FastHashMap<TileKey, RenderTaskId>,
    ) -> Self {
        SurfaceDescriptor {
            kind: SurfaceDescriptorKind::Tiled {
                tiles,
            },
        }
    }

    // Create a chained surface (e.g. blur)
    pub fn new_chained(
        render_task_id: RenderTaskId,
        root_task_id: RenderTaskId,
    ) -> Self {
        SurfaceDescriptor {
            kind: SurfaceDescriptorKind::Chained {
                render_task_id,
                root_task_id,
            },
        }
    }

    // Create a simple surface (e.g. opacity)
    pub fn new_simple(
        render_task_id: RenderTaskId,
    ) -> Self {
        SurfaceDescriptor {
            kind: SurfaceDescriptorKind::Simple {
                render_task_id,
            },
        }
    }
}

// Describes a list of command buffers that we are adding primitives to
// for a given surface. These are created from a command buffer builder
// as an optimization - skipping the indirection pic_task -> cmd_buffer_index
enum CommandBufferTargets {
    // Picture cache targets target multiple command buffers
    Tiled {
        tiles: FastHashMap<TileKey, CommandBufferIndex>,
    },
    // Child surfaces target a single command buffer
    Simple {
        cmd_buffer_index: CommandBufferIndex,
    },
}

impl CommandBufferTargets {
    // Initialize command buffer targets from a command buffer builder
    fn init(
        &mut self,
        cb: &CommandBufferBuilder,
        rg_builder: &RenderTaskGraphBuilder,
    ) {
        let new_target = match cb.kind {
            CommandBufferBuilderKind::Tiled { ref tiles, .. } => {
                let mut cb_tiles = FastHashMap::default();

                for (key, task_id) in tiles {
                    let task = rg_builder.get_task(*task_id);
                    match task.kind {
                        RenderTaskKind::Picture(ref info) => {
                            cb_tiles.insert(*key, info.cmd_buffer_index);
                        }
                        _ => unreachable!("bug: not a picture"),
                    }
                }

                CommandBufferTargets::Tiled { tiles: cb_tiles }
            }
            CommandBufferBuilderKind::Simple { render_task_id, .. } => {
                let task = rg_builder.get_task(render_task_id);
                match task.kind {
                    RenderTaskKind::Picture(ref info) => {
                        CommandBufferTargets::Simple { cmd_buffer_index: info.cmd_buffer_index }
                    }
                    _ => unreachable!("bug: not a picture"),
                }
            }
            CommandBufferBuilderKind::Invalid => {
                CommandBufferTargets::Tiled { tiles: FastHashMap::default() }
            }
        };

        *self = new_target;
    }

    /// Push a new primitive in to the command buffer builder
    fn push_prim(
        &mut self,
        prim_instance_index: PrimitiveInstanceIndex,
        spatial_node_index: SpatialNodeIndex,
        tile_rect: crate::picture::TileRect,
        sub_slice_index: SubSliceIndex,
        gpu_address: Option<crate::gpu_cache::GpuCacheAddress>,
        cmd_buffers: &mut CommandBufferList,
    ) {
        match self {
            CommandBufferTargets::Tiled { ref mut tiles } => {
                // For tiled builders, add the prim to the command buffer of each
                // tile that this primitive affects.
                for y in tile_rect.min.y .. tile_rect.max.y {
                    for x in tile_rect.min.x .. tile_rect.max.x {
                        let key = TileKey {
                            tile_offset: crate::picture::TileOffset::new(x, y),
                            sub_slice_index,
                        };
                        if let Some(cmd_buffer_index) = tiles.get(&key) {
                            cmd_buffers.get_mut(*cmd_buffer_index).add_prim(
                                prim_instance_index,
                                spatial_node_index,
                                gpu_address,
                            );
                        }
                    }
                }
            }
            CommandBufferTargets::Simple { cmd_buffer_index, .. } => {
                // For simple builders, just add the prim
                cmd_buffers.get_mut(*cmd_buffer_index).add_prim(
                    prim_instance_index,
                    spatial_node_index,
                    gpu_address,
                );
            }
        }
    }
}

// Main helper interface to build a graph of surfaces. In future patches this
// will support building sub-graphs.
pub struct SurfaceBuilder {
    // The currently set cmd buffer targets (updated during push/pop)
    current_cmd_buffers: CommandBufferTargets,
    // Stack of surfaces that are parents to the current targets
    builder_stack: Vec<CommandBufferBuilder>,
    // Dirty rect stack used to reject adding primitives
    dirty_rect_stack: Vec<PictureRect>,
}

impl SurfaceBuilder {
    pub fn new() -> Self {
        SurfaceBuilder {
            current_cmd_buffers: CommandBufferTargets::Tiled { tiles: FastHashMap::default() },
            builder_stack: Vec::new(),
            dirty_rect_stack: Vec::new(),
        }
    }

    // Push a surface on to the stack that we want to add primitives to
    pub fn push_surface(
        &mut self,
        surface_index: SurfaceIndex,
        clipping_rect: PictureRect,
        descriptor: SurfaceDescriptor,
        surfaces: &mut [SurfaceInfo],
        rg_builder: &RenderTaskGraphBuilder,
    ) {
        // Init the surface
        surfaces[surface_index.0].clipping_rect = clipping_rect;

        self.dirty_rect_stack.push(clipping_rect);

        let builder = match descriptor.kind {
            SurfaceDescriptorKind::Tiled { tiles } => {
                CommandBufferBuilder::new_tiled(
                    tiles,
                )
            }
            SurfaceDescriptorKind::Simple { render_task_id } => {
                CommandBufferBuilder::new_simple(
                    render_task_id,
                    None,
                )
            }
            SurfaceDescriptorKind::Chained { render_task_id, root_task_id } => {
                CommandBufferBuilder::new_simple(
                    render_task_id,
                    Some(root_task_id),
                )
            }
        };

        self.current_cmd_buffers.init(&builder, rg_builder);
        self.builder_stack.push(builder);
    }

    // Add a child render task (e.g. a render task cache item, or a clip mask) as a
    // dependency of the current surface
    pub fn add_child_render_task(
        &mut self,
        child_task_id: RenderTaskId,
        rg_builder: &mut RenderTaskGraphBuilder,
    ) {
        match self.builder_stack.last().unwrap().kind {
            CommandBufferBuilderKind::Tiled { ref tiles } => {
                // For a tiled render task, add as a dependency to every tile.
                for (_, parent_task_id) in tiles {
                    rg_builder.add_dependency(*parent_task_id, child_task_id);
                }
            }
            CommandBufferBuilderKind::Simple { render_task_id, .. } => {
                rg_builder.add_dependency(render_task_id, child_task_id);
            }
            CommandBufferBuilderKind::Invalid => {
                unreachable!();
            }
        }
    }

    // Returns true if the given primitive is visible and also intersects the dirty
    // region of the current surface
    pub fn is_prim_visible_and_in_dirty_region(
        &self,
        vis: &PrimitiveVisibility,
    ) -> bool {
        match vis.state {
            VisibilityState::Unset => {
                panic!("bug: invalid vis state");
            }
            VisibilityState::Culled => {
                false
            }
            VisibilityState::Visible { .. } => {
                self.dirty_rect_stack
                    .last()
                    .unwrap()
                    .intersects(&vis.clip_chain.pic_coverage_rect)
            }
            VisibilityState::PassThrough => {
                true
            }
        }
    }

    // Push a primitive to the current cmd buffer target(s)
    pub fn push_prim(
        &mut self,
        prim_instance_index: PrimitiveInstanceIndex,
        spatial_node_index: SpatialNodeIndex,
        vis: &PrimitiveVisibility,
        gpu_address: Option<crate::gpu_cache::GpuCacheAddress>,
        cmd_buffers: &mut CommandBufferList,
    ) {
        match vis.state {
            VisibilityState::Unset => {
                panic!("bug: invalid vis state");
            }
            VisibilityState::Visible { tile_rect, sub_slice_index, .. } => {
                self.current_cmd_buffers.push_prim(
                    prim_instance_index,
                    spatial_node_index,
                    tile_rect,
                    sub_slice_index,
                    gpu_address,
                    cmd_buffers,
                )
            }
            VisibilityState::PassThrough | VisibilityState::Culled => {}
        }
    }

    // Finish adding primitives and child tasks to a surface and pop it off the stack
    pub fn pop_surface(
        &mut self,
        rg_builder: &mut RenderTaskGraphBuilder,
    ) {
        self.dirty_rect_stack.pop().unwrap();

        // For now, this is fairly straightforward as we're building a single DAG. Follow up patches
        // will handle popping sub-graphs here, and fixing up the dependencies for these.

        let builder = self.builder_stack.pop().unwrap();

        match builder.kind {
            CommandBufferBuilderKind::Tiled { .. } => {
                // nothing do do as must be root
            }
            CommandBufferBuilderKind::Simple { render_task_id: child_task_id, root_task_id: child_root_task_id } => {
                match self.builder_stack.last().unwrap().kind {
                    CommandBufferBuilderKind::Tiled { ref tiles } => {
                        // For a tiled render task, add as a dependency to every tile.
                        for (_, parent_task_id) in tiles {
                            rg_builder.add_dependency(
                                *parent_task_id,
                                child_root_task_id.unwrap_or(child_task_id),
                            );
                        }
                    }
                    CommandBufferBuilderKind::Simple { render_task_id: parent_task_id, .. } => {
                        rg_builder.add_dependency(
                            parent_task_id,
                            child_root_task_id.unwrap_or(child_task_id),
                        );
                    }
                    CommandBufferBuilderKind::Invalid => {
                        unreachable!();
                    }
                }
            }
            CommandBufferBuilderKind::Invalid => {
                unreachable!();
            }
        }

        // Set up the cmd-buffer targets to write prims into the popped surface
        self.current_cmd_buffers.init(
            self.builder_stack.last().unwrap_or(&CommandBufferBuilder::empty()), rg_builder
        );
    }

    pub fn finalize(self) {
        assert!(self.builder_stack.is_empty());
    }
}
