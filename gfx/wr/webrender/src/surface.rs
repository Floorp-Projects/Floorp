/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::units::*;
use crate::batch::{CommandBufferBuilderKind, CommandBufferList, CommandBufferBuilder, CommandBufferIndex};
use crate::internal_types::FastHashMap;
use crate::picture::{SurfaceInfo, SurfaceIndex, TileKey, SubSliceIndex};
use crate::prim_store::{PrimitiveInstanceIndex};
use crate::render_task_graph::{RenderTaskId, RenderTaskGraphBuilder};
use crate::spatial_tree::SpatialNodeIndex;
use crate::render_target::ResolveOp;
use crate::render_task::{RenderTask, RenderTaskKind, RenderTaskLocation};
use crate::visibility::{VisibilityState, PrimitiveVisibility};

/*
 Contains functionality to help building the render task graph from a series of off-screen
 surfaces that are created during the prepare pass. For now, it maintains existing behavior.
 A future patch will add support for surface sub-graphs, while ensuring the render task
 graph itself is built correctly with dependencies regardless of the surface kind (chained,
 tiled, simple).
 */

// Information about the render task(s) for a given tile
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct SurfaceTileDescriptor {
    /// Target render task for commands added to this tile. This is changed
    /// each time a sub-graph is encountered on this tile
    pub current_task_id: RenderTaskId,
    /// The compositing task for this tile, if required. This is only needed
    /// when a tile contains one or more sub-graphs.
    pub composite_task_id: Option<RenderTaskId>,
}

// Details of how a surface is rendered
pub enum SurfaceDescriptorKind {
    // Picture cache tiles
    Tiled {
        tiles: FastHashMap<TileKey, SurfaceTileDescriptor>,
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
    dirty_rects: Vec<PictureRect>,
}

impl SurfaceDescriptor {
    // Create a picture cache tiled surface
    pub fn new_tiled(
        tiles: FastHashMap<TileKey, SurfaceTileDescriptor>,
        dirty_rects: Vec<PictureRect>,
    ) -> Self {
        SurfaceDescriptor {
            kind: SurfaceDescriptorKind::Tiled {
                tiles,
            },
            dirty_rects,
        }
    }

    // Create a chained surface (e.g. blur)
    pub fn new_chained(
        render_task_id: RenderTaskId,
        root_task_id: RenderTaskId,
        dirty_rect: PictureRect,
    ) -> Self {
        SurfaceDescriptor {
            kind: SurfaceDescriptorKind::Chained {
                render_task_id,
                root_task_id,
            },
            dirty_rects: vec![dirty_rect],
        }
    }

    // Create a simple surface (e.g. opacity)
    pub fn new_simple(
        render_task_id: RenderTaskId,
        dirty_rect: PictureRect,
    ) -> Self {
        SurfaceDescriptor {
            kind: SurfaceDescriptorKind::Simple {
                render_task_id,
            },
            dirty_rects: vec![dirty_rect],
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

                for (key, desc) in tiles {
                    let task = rg_builder.get_task(desc.current_task_id);
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
    dirty_rect_stack: Vec<Vec<PictureRect>>,
    // A stack of the output render tasks from any sub-graphs that haven't
    // been consumed by BackdropRender prims yet
    pub sub_graph_output_stack: Vec<RenderTaskId>,
}

impl SurfaceBuilder {
    pub fn new() -> Self {
        SurfaceBuilder {
            current_cmd_buffers: CommandBufferTargets::Tiled { tiles: FastHashMap::default() },
            builder_stack: Vec::new(),
            dirty_rect_stack: Vec::new(),
            sub_graph_output_stack: Vec::new(),
        }
    }

    /// Register the current surface as the source of a resolve for the task sub-graph that
    /// is currently on the surface builder stack.
    pub fn register_resolve_source(
        &mut self,
    ) {
        let surface_task_id = match self.builder_stack.last().unwrap().kind {
            CommandBufferBuilderKind::Tiled { .. } | CommandBufferBuilderKind::Invalid => {
                panic!("bug: only supported for non-tiled surfaces");
            }
            CommandBufferBuilderKind::Simple { render_task_id, .. } => render_task_id,
        };

        for builder in self.builder_stack.iter_mut().rev() {
            if builder.establishes_sub_graph {
                assert_eq!(builder.resolve_source, None);
                builder.resolve_source = Some(surface_task_id);
                return;
            }
        }

        unreachable!("bug: resolve source with no sub-graph");
    }

    pub fn push_surface(
        &mut self,
        surface_index: SurfaceIndex,
        is_sub_graph: bool,
        clipping_rect: PictureRect,
        descriptor: SurfaceDescriptor,
        surfaces: &mut [SurfaceInfo],
        rg_builder: &RenderTaskGraphBuilder,
    ) {
        // Init the surface
        surfaces[surface_index.0].clipping_rect = clipping_rect;

        self.dirty_rect_stack.push(descriptor.dirty_rects);

        let builder = match descriptor.kind {
            SurfaceDescriptorKind::Tiled { tiles } => {
                CommandBufferBuilder::new_tiled(
                    tiles,
                )
            }
            SurfaceDescriptorKind::Simple { render_task_id } => {
                CommandBufferBuilder::new_simple(
                    render_task_id,
                    is_sub_graph,
                    None,
                )
            }
            SurfaceDescriptorKind::Chained { render_task_id, root_task_id } => {
                CommandBufferBuilder::new_simple(
                    render_task_id,
                    is_sub_graph,
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
        let builder = self.builder_stack.last().unwrap();

        match builder.kind {
            CommandBufferBuilderKind::Tiled { ref tiles } => {
                for (_, descriptor) in tiles {
                    rg_builder.add_dependency(
                        descriptor.current_task_id,
                        child_task_id,
                    );
                }
            }
            CommandBufferBuilderKind::Simple { render_task_id, .. } => {
                rg_builder.add_dependency(
                    render_task_id,
                    child_task_id,
                );
            }
            CommandBufferBuilderKind::Invalid { .. } => {}
        }
    }

    // Add a picture render task as a dependency of the parent surface. This is a
    // special case with extra complexity as the root of the surface may change
    // when inside a sub-graph. It's currently only needed for drop-shadow effects.
    pub fn add_picture_render_task(
        &mut self,
        child_task_id: RenderTaskId,
    ) {
        self.builder_stack
            .last_mut()
            .unwrap()
            .extra_dependencies
            .push(child_task_id);
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
                    .iter()
                    .any(|dirty_rect| {
                        dirty_rect.intersects(&vis.clip_chain.pic_coverage_rect)
                    })
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
        cmd_buffers: &mut CommandBufferList,
    ) {
        self.dirty_rect_stack.pop().unwrap();

        let builder = self.builder_stack.pop().unwrap();

        if builder.establishes_sub_graph {
            // If we are popping a sub-graph off the stack the dependency setup is rather more complex...
            match builder.kind {
                CommandBufferBuilderKind::Tiled { .. } | CommandBufferBuilderKind::Invalid => {
                    unreachable!("bug: sub-graphs can only be simple surfaces");
                }
                CommandBufferBuilderKind::Simple { render_task_id: child_render_task_id, root_task_id: child_root_task_id } => {
                    // Get info about the resolve operation to copy from parent surface or tiles to the picture cache task
                    let resolve_task_id = builder.resolve_source.expect("bug: no resolve set");
                    let mut src_task_ids = Vec::new();

                    // Make the output of the sub-graph a dependency of the new replacement tile task
                    self.sub_graph_output_stack.push(child_root_task_id.unwrap_or(child_render_task_id));

                    // Set up dependencies for the sub-graph. The basic concepts below are the same, but for
                    // tiled surfaces are a little more complex as there are multiple tasks to set up.
                    //  (a) Set up new task(s) on parent surface that write to the same location
                    //  (b) Set up a resolve target to copy from parent surface tasks(s) to the resolve target
                    //  (c) Make the old parent surface tasks input dependencies of the resolve target
                    //  (d) Make the sub-graph output an input dependency of the new task(s).

                    match self.builder_stack.last_mut().unwrap().kind {
                        CommandBufferBuilderKind::Tiled { ref mut tiles } => {
                            let keys: Vec<TileKey> = tiles.keys().cloned().collect();

                            // For each tile in parent surface
                            for key in keys {
                                let descriptor = tiles.remove(&key).unwrap();
                                let parent_task_id = descriptor.current_task_id;
                                let parent_task = rg_builder.get_task_mut(parent_task_id);

                                match parent_task.location {
                                    RenderTaskLocation::Unallocated { .. } | RenderTaskLocation::Existing { .. } => {
                                        // Get info about the parent tile task location and params
                                        let location = RenderTaskLocation::Existing {
                                            parent_task_id,
                                            size: parent_task.location.size(),
                                        };

                                        let pic_task = match parent_task.kind {
                                            RenderTaskKind::Picture(ref mut pic_task) => {
                                                let cmd_buffer_index = cmd_buffers.create_cmd_buffer();
                                                let new_pic_task = pic_task.duplicate(cmd_buffer_index);

                                                // Add the resolve src to copy from tile -> picture input task
                                                src_task_ids.push(parent_task_id);

                                                new_pic_task
                                            }
                                            _ => panic!("bug: not a picture"),
                                        };

                                        // Make the existing tile an input dependency of the resolve target
                                        rg_builder.add_dependency(
                                            resolve_task_id,
                                            parent_task_id,
                                        );

                                        // Create the new task to replace the tile task
                                        let new_task_id = rg_builder.add().init(
                                            RenderTask::new(
                                                location,          // draw to same place
                                                RenderTaskKind::Picture(pic_task),
                                            ),
                                        );

                                        // Update the surface builder with the now current target for future primitives
                                        tiles.insert(
                                            key,
                                            SurfaceTileDescriptor {
                                                current_task_id: new_task_id,
                                                ..descriptor
                                            },
                                        );
                                    }
                                    RenderTaskLocation::Static { .. } => {
                                        // Update the surface builder with the now current target for future primitives
                                        tiles.insert(
                                            key,
                                            descriptor,
                                        );
                                    }
                                    _ => {
                                        panic!("bug: unexpected task location");
                                    }
                                }
                            }
                        }
                        CommandBufferBuilderKind::Simple { render_task_id: ref mut parent_task_id, .. } => {
                            let parent_task = rg_builder.get_task_mut(*parent_task_id);

                            // Get info about the parent tile task location and params
                            let location = RenderTaskLocation::Existing {
                                parent_task_id: *parent_task_id,
                                size: parent_task.location.size(),
                            };
                            let pic_task = match parent_task.kind {
                                RenderTaskKind::Picture(ref mut pic_task) => {
                                    let cmd_buffer_index = cmd_buffers.create_cmd_buffer();

                                    let new_pic_task = pic_task.duplicate(cmd_buffer_index);

                                    // Add the resolve src to copy from tile -> picture input task
                                    src_task_ids.push(*parent_task_id);

                                    new_pic_task
                                }
                                _ => panic!("bug: not a picture"),
                            };

                            // Make the existing surface an input dependency of the resolve target
                            rg_builder.add_dependency(
                                resolve_task_id,
                                *parent_task_id,
                            );

                            // Create the new task to replace the parent surface task
                            let new_task_id = rg_builder.add().init(
                                RenderTask::new(
                                    location,          // draw to same place
                                    RenderTaskKind::Picture(pic_task),
                                ),
                            );

                            // Update the surface builder with the now current target for future primitives
                            *parent_task_id = new_task_id;
                        }
                        CommandBufferBuilderKind::Invalid => {
                            unreachable!();
                        }
                    }

                    let dest_task = rg_builder.get_task_mut(resolve_task_id);

                    match dest_task.kind {
                        RenderTaskKind::Picture(ref mut dest_task_info) => {
                            assert!(dest_task_info.resolve_op.is_none());
                            dest_task_info.resolve_op = Some(ResolveOp {
                                src_task_ids,
                                dest_task_id: resolve_task_id,
                            })
                        }
                        _ => {
                            unreachable!("bug: not a picture");
                        }
                    }
                }
            }
        } else {
            match builder.kind {
                CommandBufferBuilderKind::Tiled { ref tiles } => {
                    for (_, descriptor) in tiles {
                        if let Some(composite_task_id) = descriptor.composite_task_id {
                            rg_builder.add_dependency(
                                composite_task_id,
                                descriptor.current_task_id,
                            );

                            let composite_task = rg_builder.get_task_mut(composite_task_id);
                            match composite_task.kind {
                                RenderTaskKind::TileComposite(ref mut info) => {
                                    info.task_id = Some(descriptor.current_task_id);
                                }
                                _ => unreachable!("bug: not a tile composite"),
                            }
                        }
                    }
                }
                CommandBufferBuilderKind::Simple { render_task_id: child_task_id, root_task_id: child_root_task_id } => {
                    match self.builder_stack.last().unwrap().kind {
                        CommandBufferBuilderKind::Tiled { ref tiles } => {
                            // For a tiled render task, add as a dependency to every tile.
                            for (_, descriptor) in tiles {
                                rg_builder.add_dependency(
                                    descriptor.current_task_id,
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
        }

        // Step through the dependencies for this builder and add them to the finalized
        // render task root(s) for this surface
        match builder.kind {
            CommandBufferBuilderKind::Tiled { ref tiles } => {
                for (_, descriptor) in tiles {
                    for task_id in &builder.extra_dependencies {
                        rg_builder.add_dependency(
                            descriptor.current_task_id,
                            *task_id,
                        );
                    }
                }
            }
            CommandBufferBuilderKind::Simple { render_task_id, .. } => {
                for task_id in &builder.extra_dependencies {
                    rg_builder.add_dependency(
                        render_task_id,
                        *task_id,
                    );
                }
            }
            CommandBufferBuilderKind::Invalid { .. } => {}
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
