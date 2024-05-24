/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! # Visibility pass
//!
//! TODO: document what this pass does!
//!

use api::{DebugFlags};
use api::units::*;
use std::{usize};
use crate::clip::ClipStore;
use crate::composite::CompositeState;
use crate::spatial_tree::{SpatialTree, SpatialNodeIndex};
use crate::clip::{ClipChainInstance, ClipTree};
use crate::frame_builder::FrameBuilderConfig;
use crate::gpu_cache::GpuCache;
use crate::picture::{PictureCompositeMode, ClusterFlags, SurfaceInfo, TileCacheInstance};
use crate::picture::{SurfaceIndex, RasterConfig, SubSliceIndex};
use crate::prim_store::{ClipTaskIndex, PictureIndex, PrimitiveInstanceKind};
use crate::prim_store::{PrimitiveStore, PrimitiveInstance};
use crate::render_backend::{DataStores, ScratchBuffer};
use crate::render_task_graph::RenderTaskGraphBuilder;
use crate::resource_cache::ResourceCache;
use crate::scene::SceneProperties;
use crate::space::SpaceMapper;
use crate::util::{MaxRect};

pub struct FrameVisibilityContext<'a> {
    pub spatial_tree: &'a SpatialTree,
    pub global_screen_world_rect: WorldRect,
    pub global_device_pixel_scale: DevicePixelScale,
    pub debug_flags: DebugFlags,
    pub scene_properties: &'a SceneProperties,
    pub config: FrameBuilderConfig,
    pub root_spatial_node_index: SpatialNodeIndex,
}

pub struct FrameVisibilityState<'a> {
    pub clip_store: &'a mut ClipStore,
    pub resource_cache: &'a mut ResourceCache,
    pub gpu_cache: &'a mut GpuCache,
    pub scratch: &'a mut ScratchBuffer,
    pub data_stores: &'a mut DataStores,
    pub clip_tree: &'a mut ClipTree,
    pub composite_state: &'a mut CompositeState,
    pub rg_builder: &'a mut RenderTaskGraphBuilder,
    /// A stack of currently active off-screen surfaces during the
    /// visibility frame traversal.
    pub surface_stack: Vec<(PictureIndex, SurfaceIndex)>,
}

impl<'a> FrameVisibilityState<'a> {
    pub fn push_surface(
        &mut self,
        pic_index: PictureIndex,
        surface_index: SurfaceIndex,
    ) {
        self.surface_stack.push((pic_index, surface_index));
    }

    pub fn pop_surface(&mut self) {
        self.surface_stack.pop().unwrap();
    }
}

bitflags! {
    /// A set of bitflags that can be set in the visibility information
    /// for a primitive instance. This can be used to control how primitives
    /// are treated during batching.
    // TODO(gw): We should also move `is_compositor_surface` to be part of
    //           this flags struct.
    #[cfg_attr(feature = "capture", derive(Serialize))]
    #[derive(Debug, Copy, PartialEq, Eq, Clone, PartialOrd, Ord, Hash)]
    pub struct PrimitiveVisibilityFlags: u8 {
        /// Implies that this primitive covers the entire picture cache slice,
        /// and can thus be dropped during batching and drawn with clear color.
        const IS_BACKDROP = 1;
    }
}

/// Contains the current state of the primitive's visibility.
#[derive(Debug)]
#[cfg_attr(feature = "capture", derive(Serialize))]
pub enum VisibilityState {
    /// Uninitialized - this should never be encountered after prim reset
    Unset,
    /// Culled for being off-screen, or not possible to render (e.g. missing image resource)
    Culled,
    /// A picture that doesn't have a surface - primitives are composed into the
    /// parent picture with a surface.
    PassThrough,
    /// A primitive that has been found to be visible
    Visible {
        /// A set of flags that define how this primitive should be handled
        /// during batching of visible primitives.
        vis_flags: PrimitiveVisibilityFlags,

        /// Sub-slice within the picture cache that this prim exists on
        sub_slice_index: SubSliceIndex,
    },
}

/// Information stored for a visible primitive about the visible
/// rect and associated clip information.
#[derive(Debug)]
#[cfg_attr(feature = "capture", derive(Serialize))]
pub struct PrimitiveVisibility {
    /// The clip chain instance that was built for this primitive.
    pub clip_chain: ClipChainInstance,

    /// Current visibility state of the primitive.
    // TODO(gw): Move more of the fields from this struct into
    //           the state enum.
    pub state: VisibilityState,

    /// An index into the clip task instances array in the primitive
    /// store. If this is ClipTaskIndex::INVALID, then the primitive
    /// has no clip mask. Otherwise, it may store the offset of the
    /// global clip mask task for this primitive, or the first of
    /// a list of clip task ids (one per segment).
    pub clip_task_index: ClipTaskIndex,
}

impl PrimitiveVisibility {
    pub fn new() -> Self {
        PrimitiveVisibility {
            state: VisibilityState::Unset,
            clip_chain: ClipChainInstance::empty(),
            clip_task_index: ClipTaskIndex::INVALID,
        }
    }

    pub fn reset(&mut self) {
        self.state = VisibilityState::Culled;
        self.clip_task_index = ClipTaskIndex::INVALID;
    }
}

pub fn update_prim_visibility(
    pic_index: PictureIndex,
    parent_surface_index: Option<SurfaceIndex>,
    world_culling_rect: &WorldRect,
    store: &PrimitiveStore,
    prim_instances: &mut [PrimitiveInstance],
    surfaces: &mut [SurfaceInfo],
    is_root_tile_cache: bool,
    frame_context: &FrameVisibilityContext,
    frame_state: &mut FrameVisibilityState,
    tile_cache: &mut TileCacheInstance,
 ) {
    let pic = &store.pictures[pic_index.0];

    let (surface_index, pop_surface) = match pic.raster_config {
        Some(RasterConfig { surface_index, composite_mode: PictureCompositeMode::TileCache { .. }, .. }) => {
            (surface_index, false)
        }
        Some(ref raster_config) => {
            frame_state.push_surface(
                pic_index,
                raster_config.surface_index,
            );

            let surface_local_rect = surfaces[raster_config.surface_index.0]
                .unclipped_local_rect
                .cast_unit();

            // Let the picture cache know that we are pushing an off-screen
            // surface, so it can treat dependencies of surface atomically.
            tile_cache.push_surface(
                surface_local_rect,
                pic.spatial_node_index,
                frame_context.spatial_tree,
            );

            (raster_config.surface_index, true)
        }
        None => {
            (parent_surface_index.expect("bug: pass-through with no parent"), false)
        }
    };

    let surface = &surfaces[surface_index.0 as usize];
    let device_pixel_scale = surface.device_pixel_scale;
    let mut map_local_to_surface = surface.map_local_to_surface.clone();
    let map_surface_to_world = SpaceMapper::new_with_target(
        frame_context.root_spatial_node_index,
        surface.surface_spatial_node_index,
        frame_context.global_screen_world_rect,
        frame_context.spatial_tree,
    );

    for cluster in &pic.prim_list.clusters {
        profile_scope!("cluster");

        // Each prim instance must have reset called each frame, to clear
        // indices into various scratch buffers. If this doesn't occur,
        // the primitive may incorrectly be considered visible, which can
        // cause unexpected conditions to occur later during the frame.
        // Primitive instances are normally reset in the main loop below,
        // but we must also reset them in the rare case that the cluster
        // visibility has changed (due to an invalid transform and/or
        // backface visibility changing for this cluster).
        // TODO(gw): This is difficult to test for in CI - as a follow up,
        //           we should add a debug flag that validates the prim
        //           instance is always reset every frame to catch similar
        //           issues in future.
        for prim_instance in &mut prim_instances[cluster.prim_range()] {
            prim_instance.reset();
        }

        // Get the cluster and see if is visible
        if !cluster.flags.contains(ClusterFlags::IS_VISIBLE) {
            continue;
        }

        map_local_to_surface.set_target_spatial_node(
            cluster.spatial_node_index,
            frame_context.spatial_tree,
        );

        for prim_instance_index in cluster.prim_range() {
            if let PrimitiveInstanceKind::Picture { pic_index, .. } = prim_instances[prim_instance_index].kind {
                if !store.pictures[pic_index.0].is_visible(frame_context.spatial_tree) {
                    continue;
                }

                let is_passthrough = match store.pictures[pic_index.0].raster_config {
                    Some(..) => false,
                    None => true,
                };

                if !is_passthrough {
                    let clip_root = store
                        .pictures[pic_index.0]
                        .clip_root
                        .unwrap_or_else(|| {
                            // If we couldn't find a common ancestor then just use the
                            // clip node of the picture primitive itself
                            let leaf_id = prim_instances[prim_instance_index].clip_leaf_id;
                            frame_state.clip_tree.get_leaf(leaf_id).node_id
                        }
                    );

                    frame_state.clip_tree.push_clip_root_node(clip_root);
                }

                update_prim_visibility(
                    pic_index,
                    Some(surface_index),
                    world_culling_rect,
                    store,
                    prim_instances,
                    surfaces,
                    false,
                    frame_context,
                    frame_state,
                    tile_cache,
                );

                if is_passthrough {
                    // Pass through pictures are always considered visible in all dirty tiles.
                    prim_instances[prim_instance_index].vis.state = VisibilityState::PassThrough;

                    continue;
                } else {
                    frame_state.clip_tree.pop_clip_root();
                }
            }

            let prim_instance = &mut prim_instances[prim_instance_index];

            let local_coverage_rect = frame_state.data_stores.get_local_prim_coverage_rect(
                prim_instance,
                &store.pictures,
                surfaces,
            );

            frame_state.clip_store.set_active_clips(
                cluster.spatial_node_index,
                map_local_to_surface.ref_spatial_node_index,
                prim_instance.clip_leaf_id,
                &frame_context.spatial_tree,
                &frame_state.data_stores.clip,
                frame_state.clip_tree,
            );

            let clip_chain = frame_state
                .clip_store
                .build_clip_chain_instance(
                    local_coverage_rect,
                    &map_local_to_surface,
                    &map_surface_to_world,
                    &frame_context.spatial_tree,
                    frame_state.gpu_cache,
                    frame_state.resource_cache,
                    device_pixel_scale,
                    &world_culling_rect,
                    &mut frame_state.data_stores.clip,
                    frame_state.rg_builder,
                    true,
                );

            prim_instance.vis.clip_chain = match clip_chain {
                Some(clip_chain) => clip_chain,
                None => {
                    continue;
                }
            };

            tile_cache.update_prim_dependencies(
                prim_instance,
                cluster.spatial_node_index,
                // It's OK to pass the local_coverage_rect here as it's only used by primitives
                // (for compositor surfaces) that don't have inflation anyway.
                local_coverage_rect,
                frame_context,
                frame_state.data_stores,
                frame_state.clip_store,
                &store.pictures,
                frame_state.resource_cache,
                &store.color_bindings,
                &frame_state.surface_stack,
                &mut frame_state.composite_state,
                &mut frame_state.gpu_cache,
                &mut frame_state.scratch.primitive,
                is_root_tile_cache,
                surfaces,
            );
        }
    }

    if pop_surface {
        frame_state.pop_surface();
    }

    if let Some(ref rc) = pic.raster_config {
        match rc.composite_mode {
            PictureCompositeMode::TileCache { .. } => {}
            _ => {
                // Pop the off-screen surface from the picture cache stack
                tile_cache.pop_surface();
            }
        }
    }
}

pub fn compute_conservative_visible_rect(
    clip_chain: &ClipChainInstance,
    world_culling_rect: WorldRect,
    prim_spatial_node_index: SpatialNodeIndex,
    spatial_tree: &SpatialTree,
) -> LayoutRect {
    let root_spatial_node_index = spatial_tree.root_reference_frame_index();

    // Mapping from picture space -> world space
    let map_pic_to_world: SpaceMapper<PicturePixel, WorldPixel> = SpaceMapper::new_with_target(
        root_spatial_node_index,
        clip_chain.pic_spatial_node_index,
        world_culling_rect,
        spatial_tree,
    );

    // Mapping from local space -> picture space
    let map_local_to_pic: SpaceMapper<LayoutPixel, PicturePixel> = SpaceMapper::new_with_target(
        clip_chain.pic_spatial_node_index,
        prim_spatial_node_index,
        PictureRect::max_rect(),
        spatial_tree,
    );

    // Unmap the world culling rect from world -> picture space. If this mapping fails due
    // to matrix weirdness, best we can do is use the clip chain's local clip rect.
    let pic_culling_rect = match map_pic_to_world.unmap(&world_culling_rect) {
        Some(rect) => rect,
        None => return clip_chain.local_clip_rect,
    };

    // Intersect the unmapped world culling rect with the primitive's clip chain rect that
    // is in picture space (the clip-chain already takes into account the bounds of the
    // primitive local_rect and local_clip_rect). If there is no intersection here, the
    // primitive is not visible at all.
    let pic_culling_rect = match pic_culling_rect.intersection(&clip_chain.pic_coverage_rect) {
        Some(rect) => rect,
        None => return LayoutRect::zero(),
    };

    // Unmap the picture culling rect from picture -> local space. If this mapping fails due
    // to matrix weirdness, best we can do is use the clip chain's local clip rect.
    match map_local_to_pic.unmap(&pic_culling_rect) {
        Some(rect) => rect,
        None => clip_chain.local_clip_rect,
    }
}
