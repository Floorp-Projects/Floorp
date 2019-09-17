/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{ColorF, DebugFlags, DocumentLayer, FontRenderMode, PremultipliedColorF};
use api::{PipelineId};
use api::units::*;
use crate::batch::{BatchBuilder, AlphaBatchBuilder, AlphaBatchContainer};
use crate::clip::{ClipStore, ClipChainStack};
use crate::clip_scroll_tree::{ClipScrollTree, ROOT_SPATIAL_NODE_INDEX, SpatialNodeIndex};
use crate::debug_render::DebugItem;
use crate::gpu_cache::{GpuCache, GpuCacheHandle};
use crate::gpu_types::{PrimitiveHeaders, TransformPalette, UvRectKind, ZBufferIdGenerator};
use crate::gpu_types::TransformData;
use crate::internal_types::{FastHashMap, PlaneSplitter, SavedTargetIndex};
use crate::picture::{PictureUpdateState, SurfaceInfo, ROOT_SURFACE_INDEX, SurfaceIndex, RecordedDirtyRegion};
use crate::picture::{RetainedTiles, TileCacheInstance, DirtyRegion, SurfaceRenderTasks, SubpixelMode};
use crate::prim_store::{SpaceMapper, PictureIndex, PrimitiveDebugId, PrimitiveScratchBuffer};
use crate::prim_store::{DeferredResolve, PrimitiveVisibilityMask};
use crate::profiler::{FrameProfileCounters, GpuCacheProfileCounters, TextureCacheProfileCounters};
use crate::render_backend::{DataStores, FrameStamp, FrameId};
use crate::render_target::{RenderTarget, PictureCacheTarget, TextureCacheRenderTarget};
use crate::render_target::{RenderTargetContext, RenderTargetKind};
use crate::render_task_graph::{RenderTaskId, RenderTaskGraph, RenderTaskGraphCounters};
use crate::render_task_graph::{RenderPassKind, RenderPass};
use crate::render_task::{RenderTask, RenderTaskLocation, RenderTaskKind};
use crate::resource_cache::{ResourceCache};
use crate::scene::{BuiltScene, ScenePipeline, SceneProperties};
use crate::segment::SegmentBuilder;
use std::{f32, mem};
use std::sync::Arc;
use crate::util::MaxRect;


#[derive(Clone, Copy, Debug, PartialEq)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub enum ChasePrimitive {
    Nothing,
    Id(PrimitiveDebugId),
    LocalRect(LayoutRect),
}

impl Default for ChasePrimitive {
    fn default() -> Self {
        ChasePrimitive::Nothing
    }
}

#[derive(Clone, Copy, Debug)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct FrameBuilderConfig {
    pub default_font_render_mode: FontRenderMode,
    pub dual_source_blending_is_supported: bool,
    pub dual_source_blending_is_enabled: bool,
    pub chase_primitive: ChasePrimitive,
    pub enable_picture_caching: bool,
    /// True if we're running tests (i.e. via wrench).
    pub testing: bool,
    pub gpu_supports_fast_clears: bool,
    pub gpu_supports_advanced_blend: bool,
    pub advanced_blend_is_coherent: bool,
    pub batch_lookback_count: usize,
    pub background_color: Option<ColorF>,
}

/// A set of common / global resources that are retained between
/// new display lists, such that any GPU cache handles can be
/// persisted even when a new display list arrives.
#[cfg_attr(feature = "capture", derive(Serialize))]
pub struct FrameGlobalResources {
    /// The image shader block for the most common / default
    /// set of image parameters (color white, stretch == rect.size).
    pub default_image_handle: GpuCacheHandle,
}

impl FrameGlobalResources {
    pub fn empty() -> Self {
        FrameGlobalResources {
            default_image_handle: GpuCacheHandle::new(),
        }
    }

    pub fn update(
        &mut self,
        gpu_cache: &mut GpuCache,
    ) {
        if let Some(mut request) = gpu_cache.request(&mut self.default_image_handle) {
            request.push(PremultipliedColorF::WHITE);
            request.push(PremultipliedColorF::WHITE);
            request.push([
                -1.0,       // -ve means use prim rect for stretch size
                0.0,
                0.0,
                0.0,
            ]);
        }
    }
}

/// Produces the frames that are sent to the renderer.
#[cfg_attr(feature = "capture", derive(Serialize))]
pub struct FrameBuilder {
    /// Cache of surface tiles from the previous frame builder
    /// that can optionally be consumed by this frame builder.
    pending_retained_tiles: RetainedTiles,
    pub globals: FrameGlobalResources,
}

pub struct FrameVisibilityContext<'a> {
    pub clip_scroll_tree: &'a ClipScrollTree,
    pub global_screen_world_rect: WorldRect,
    pub global_device_pixel_scale: DevicePixelScale,
    pub surfaces: &'a [SurfaceInfo],
    pub debug_flags: DebugFlags,
    pub scene_properties: &'a SceneProperties,
    pub config: &'a FrameBuilderConfig,
}

pub struct FrameVisibilityState<'a> {
    pub clip_store: &'a mut ClipStore,
    pub resource_cache: &'a mut ResourceCache,
    pub gpu_cache: &'a mut GpuCache,
    pub scratch: &'a mut PrimitiveScratchBuffer,
    pub tile_cache: Option<Box<TileCacheInstance>>,
    pub retained_tiles: &'a mut RetainedTiles,
    pub data_stores: &'a mut DataStores,
    pub clip_chain_stack: ClipChainStack,
    pub render_tasks: &'a mut RenderTaskGraph,
}

pub struct FrameBuildingContext<'a> {
    pub global_device_pixel_scale: DevicePixelScale,
    pub scene_properties: &'a SceneProperties,
    pub pipelines: &'a FastHashMap<PipelineId, Arc<ScenePipeline>>,
    pub global_screen_world_rect: WorldRect,
    pub clip_scroll_tree: &'a ClipScrollTree,
    pub max_local_clip: LayoutRect,
    pub debug_flags: DebugFlags,
    pub fb_config: &'a FrameBuilderConfig,
}

pub struct FrameBuildingState<'a> {
    pub render_tasks: &'a mut RenderTaskGraph,
    pub profile_counters: &'a mut FrameProfileCounters,
    pub clip_store: &'a mut ClipStore,
    pub resource_cache: &'a mut ResourceCache,
    pub gpu_cache: &'a mut GpuCache,
    pub transforms: &'a mut TransformPalette,
    pub segment_builder: SegmentBuilder,
    pub surfaces: &'a mut Vec<SurfaceInfo>,
    pub dirty_region_stack: Vec<DirtyRegion>,
}

impl<'a> FrameBuildingState<'a> {
    /// Retrieve the current dirty region during primitive traversal.
    pub fn current_dirty_region(&self) -> &DirtyRegion {
        self.dirty_region_stack.last().unwrap()
    }

    /// Push a new dirty region for child primitives to cull / clip against.
    pub fn push_dirty_region(&mut self, region: DirtyRegion) {
        self.dirty_region_stack.push(region);
    }

    /// Pop the top dirty region from the stack.
    pub fn pop_dirty_region(&mut self) {
        self.dirty_region_stack.pop().unwrap();
    }
}

/// Immutable context of a picture when processing children.
#[derive(Debug)]
pub struct PictureContext {
    pub pic_index: PictureIndex,
    pub apply_local_clip_rect: bool,
    pub is_passthrough: bool,
    pub surface_spatial_node_index: SpatialNodeIndex,
    pub raster_spatial_node_index: SpatialNodeIndex,
    /// The surface that this picture will render on.
    pub surface_index: SurfaceIndex,
    pub dirty_region_count: usize,
    pub subpixel_mode: SubpixelMode,
}

/// Mutable state of a picture that gets modified when
/// the children are processed.
pub struct PictureState {
    pub map_local_to_pic: SpaceMapper<LayoutPixel, PicturePixel>,
    pub map_pic_to_world: SpaceMapper<PicturePixel, WorldPixel>,
    pub map_pic_to_raster: SpaceMapper<PicturePixel, RasterPixel>,
    pub map_raster_to_world: SpaceMapper<RasterPixel, WorldPixel>,
    /// If the plane splitter, the primitives get added to it instead of
    /// batching into their parent pictures.
    pub plane_splitter: Option<PlaneSplitter>,
}

impl FrameBuilder {
    pub fn new() -> Self {
        FrameBuilder {
            pending_retained_tiles: RetainedTiles::new(),
            globals: FrameGlobalResources::empty(),
        }
    }

    /// Provide any cached surface tiles from the previous frame builder
    /// to a new frame builder. These will be consumed or dropped the
    /// first time a new frame builder creates a frame.
    pub fn set_retained_resources(&mut self, retained_tiles: RetainedTiles) {
        // In general, the pending retained tiles are consumed by the frame
        // builder the first time a frame is built after a new scene has
        // arrived. However, if two scenes arrive in quick succession, the
        // frame builder may not have had a chance to build a frame and
        // consume the pending tiles. In this case, the pending tiles will
        // be lost, causing a full invalidation of the entire screen. To
        // avoid this, if there are still pending tiles, include them in
        // the retained tiles passed to the next frame builder.
        self.pending_retained_tiles.merge(retained_tiles);
    }

    /// Compute the contribution (bounding rectangles, and resources) of layers and their
    /// primitives in screen space.
    fn build_layer_screen_rects_and_cull_layers(
        &mut self,
        scene: &mut BuiltScene,
        global_screen_world_rect: WorldRect,
        resource_cache: &mut ResourceCache,
        gpu_cache: &mut GpuCache,
        render_tasks: &mut RenderTaskGraph,
        profile_counters: &mut FrameProfileCounters,
        global_device_pixel_scale: DevicePixelScale,
        scene_properties: &SceneProperties,
        transform_palette: &mut TransformPalette,
        data_stores: &mut DataStores,
        surfaces: &mut Vec<SurfaceInfo>,
        scratch: &mut PrimitiveScratchBuffer,
        debug_flags: DebugFlags,
        texture_cache_profile: &mut TextureCacheProfileCounters,
    ) -> Option<RenderTaskId> {
        profile_scope!("cull");

        if scene.prim_store.pictures.is_empty() {
            return None
        }

        scratch.begin_frame();

        let root_spatial_node_index = scene.clip_scroll_tree.root_reference_frame_index();

        const MAX_CLIP_COORD: f32 = 1.0e9;

        let frame_context = FrameBuildingContext {
            global_device_pixel_scale,
            scene_properties,
            pipelines: &scene.src.pipelines,
            global_screen_world_rect,
            clip_scroll_tree: &scene.clip_scroll_tree,
            max_local_clip: LayoutRect::new(
                LayoutPoint::new(-MAX_CLIP_COORD, -MAX_CLIP_COORD),
                LayoutSize::new(2.0 * MAX_CLIP_COORD, 2.0 * MAX_CLIP_COORD),
            ),
            debug_flags,
            fb_config: &scene.config,
        };

        let root_render_task = RenderTask::new_picture(
            RenderTaskLocation::Fixed(scene.output_rect),
            scene.output_rect.size.to_f32(),
            scene.root_pic_index,
            DeviceIntPoint::zero(),
            UvRectKind::Rect,
            ROOT_SPATIAL_NODE_INDEX,
            global_device_pixel_scale,
            PrimitiveVisibilityMask::all(),
            None,
        );

        let root_render_task_id = render_tasks.add(root_render_task);

        // Construct a dummy root surface, that represents the
        // main framebuffer surface.
        let root_surface = SurfaceInfo::new(
            ROOT_SPATIAL_NODE_INDEX,
            ROOT_SPATIAL_NODE_INDEX,
            0.0,
            global_screen_world_rect,
            &scene.clip_scroll_tree,
            global_device_pixel_scale,
        );
        surfaces.push(root_surface);

        let mut retained_tiles = mem::replace(
            &mut self.pending_retained_tiles,
            RetainedTiles::new(),
        );

        // The first major pass of building a frame is to walk the picture
        // tree. This pass must be quick (it should never touch individual
        // primitives). For now, all we do here is determine which pictures
        // will create surfaces. In the future, this will be expanded to
        // set up render tasks, determine scaling of surfaces, and detect
        // which surfaces have valid cached surfaces that don't need to
        // be rendered this frame.
        PictureUpdateState::update_all(
            surfaces,
            scene.root_pic_index,
            &mut scene.prim_store.pictures,
            &frame_context,
            gpu_cache,
            &scene.clip_store,
            data_stores,
        );

        {
            profile_marker!("UpdateVisibility");

            let visibility_context = FrameVisibilityContext {
                global_device_pixel_scale,
                clip_scroll_tree: &scene.clip_scroll_tree,
                global_screen_world_rect,
                surfaces,
                debug_flags,
                scene_properties,
                config: &scene.config,
            };

            let mut visibility_state = FrameVisibilityState {
                resource_cache,
                gpu_cache,
                clip_store: &mut scene.clip_store,
                scratch,
                tile_cache: None,
                retained_tiles: &mut retained_tiles,
                data_stores,
                clip_chain_stack: ClipChainStack::new(),
                render_tasks,
            };

            scene.prim_store.update_visibility(
                scene.root_pic_index,
                ROOT_SURFACE_INDEX,
                &global_screen_world_rect,
                &visibility_context,
                &mut visibility_state,
            );
        }

        let mut frame_state = FrameBuildingState {
            render_tasks,
            profile_counters,
            clip_store: &mut scene.clip_store,
            resource_cache,
            gpu_cache,
            transforms: transform_palette,
            segment_builder: SegmentBuilder::new(),
            surfaces,
            dirty_region_stack: Vec::new(),
        };

        frame_state
            .surfaces
            .first_mut()
            .unwrap()
            .render_tasks = Some(SurfaceRenderTasks {
                root: root_render_task_id,
                port: root_render_task_id,
            });

        // Push a default dirty region which culls primitives
        // against the screen world rect, in absence of any
        // other dirty regions.
        let mut default_dirty_region = DirtyRegion::new();
        default_dirty_region.push(
            frame_context.global_screen_world_rect,
            PrimitiveVisibilityMask::all(),
        );
        frame_state.push_dirty_region(default_dirty_region);

        let (pic_context, mut pic_state, mut prim_list) = scene
            .prim_store
            .pictures[scene.root_pic_index.0]
            .take_context(
                scene.root_pic_index,
                WorldRect::max_rect(),
                root_spatial_node_index,
                root_spatial_node_index,
                ROOT_SURFACE_INDEX,
                SubpixelMode::Allow,
                &mut frame_state,
                &frame_context,
            )
            .unwrap();

        {
            profile_marker!("PreparePrims");

            scene.prim_store.prepare_primitives(
                &mut prim_list,
                &pic_context,
                &mut pic_state,
                &frame_context,
                &mut frame_state,
                data_stores,
                scratch,
            );
        }

        let pic = &mut scene.prim_store.pictures[scene.root_pic_index.0];
        pic.restore_context(
            prim_list,
            pic_context,
            pic_state,
            &mut frame_state,
        );

        frame_state.pop_dirty_region();

        {
            profile_marker!("BlockOnResources");

            resource_cache.block_until_all_resources_added(gpu_cache,
                                                           render_tasks,
                                                           texture_cache_profile);
        }

        Some(root_render_task_id)
    }

    pub fn build(
        &mut self,
        scene: &mut BuiltScene,
        resource_cache: &mut ResourceCache,
        gpu_cache: &mut GpuCache,
        stamp: FrameStamp,
        global_device_pixel_scale: DevicePixelScale,
        layer: DocumentLayer,
        device_origin: DeviceIntPoint,
        pan: WorldPoint,
        texture_cache_profile: &mut TextureCacheProfileCounters,
        gpu_cache_profile: &mut GpuCacheProfileCounters,
        scene_properties: &SceneProperties,
        data_stores: &mut DataStores,
        scratch: &mut PrimitiveScratchBuffer,
        render_task_counters: &mut RenderTaskGraphCounters,
        debug_flags: DebugFlags,
    ) -> Frame {
        profile_scope!("build");
        profile_marker!("BuildFrame");

        let mut profile_counters = FrameProfileCounters::new();
        profile_counters
            .total_primitives
            .set(scene.prim_store.prim_count());

        resource_cache.begin_frame(stamp);
        gpu_cache.begin_frame(stamp);

        self.globals.update(gpu_cache);

        scene.clip_scroll_tree.update_tree(
            pan,
            global_device_pixel_scale,
            scene_properties,
        );
        let mut transform_palette = scene.clip_scroll_tree.build_transform_palette();
        scene.clip_store.clear_old_instances();

        let mut render_tasks = RenderTaskGraph::new(
            stamp.frame_id(),
            render_task_counters,
        );
        let mut surfaces = Vec::new();

        let output_size = scene.output_rect.size.to_i32();
        let screen_world_rect = (scene.output_rect.to_f32() / global_device_pixel_scale).round_out();

        let main_render_task_id = self.build_layer_screen_rects_and_cull_layers(
            scene,
            screen_world_rect,
            resource_cache,
            gpu_cache,
            &mut render_tasks,
            &mut profile_counters,
            global_device_pixel_scale,
            scene_properties,
            &mut transform_palette,
            data_stores,
            &mut surfaces,
            scratch,
            debug_flags,
            texture_cache_profile,
        );

        let mut passes;
        let mut deferred_resolves = vec![];
        let mut has_texture_cache_tasks = false;
        let mut prim_headers = PrimitiveHeaders::new();

        {
            profile_marker!("Batching");

            passes = render_tasks.generate_passes(
                main_render_task_id,
                output_size,
                scene.config.gpu_supports_fast_clears,
            );

            // Used to generated a unique z-buffer value per primitive.
            let mut z_generator = ZBufferIdGenerator::new(layer);
            let use_dual_source_blending = scene.config.dual_source_blending_is_enabled &&
                                           scene.config.dual_source_blending_is_supported;

            for pass in &mut passes {
                let mut ctx = RenderTargetContext {
                    global_device_pixel_scale,
                    prim_store: &scene.prim_store,
                    resource_cache,
                    use_dual_source_blending,
                    use_advanced_blending: scene.config.gpu_supports_advanced_blend,
                    break_advanced_blend_batches: !scene.config.advanced_blend_is_coherent,
                    batch_lookback_count: scene.config.batch_lookback_count,
                    clip_scroll_tree: &scene.clip_scroll_tree,
                    data_stores,
                    surfaces: &surfaces,
                    scratch,
                    screen_world_rect,
                    globals: &self.globals,
                };

                build_render_pass(
                    pass,
                    &mut ctx,
                    gpu_cache,
                    &mut render_tasks,
                    &mut deferred_resolves,
                    &scene.clip_store,
                    &mut transform_palette,
                    &mut prim_headers,
                    &mut z_generator,
                );

                match pass.kind {
                    RenderPassKind::MainFramebuffer { .. } => {}
                    RenderPassKind::OffScreen {
                        ref texture_cache,
                        ref picture_cache,
                        ..
                    } => {
                        has_texture_cache_tasks |= !texture_cache.is_empty();
                        has_texture_cache_tasks |= !picture_cache.is_empty();
                    }
                }
            }
        }

        let gpu_cache_frame_id = gpu_cache.end_frame(gpu_cache_profile).frame_id();

        render_tasks.write_task_data();
        *render_task_counters = render_tasks.counters();
        resource_cache.end_frame(texture_cache_profile);

        Frame {
            content_origin: scene.output_rect.origin,
            device_rect: DeviceIntRect::new(
                device_origin,
                scene.output_rect.size,
            ),
            background_color: scene.background_color,
            layer,
            profile_counters,
            passes,
            transform_palette: transform_palette.finish(),
            render_tasks,
            deferred_resolves,
            gpu_cache_frame_id,
            has_been_rendered: false,
            has_texture_cache_tasks,
            prim_headers,
            recorded_dirty_regions: mem::replace(&mut scratch.recorded_dirty_regions, Vec::new()),
            debug_items: mem::replace(&mut scratch.debug_items, Vec::new()),
        }
    }
}

/// Processes this pass to prepare it for rendering.
///
/// Among other things, this allocates output regions for each of our tasks
/// (added via `add_render_task`) in a RenderTarget and assigns it into that
/// target.
pub fn build_render_pass(
    pass: &mut RenderPass,
    ctx: &mut RenderTargetContext,
    gpu_cache: &mut GpuCache,
    render_tasks: &mut RenderTaskGraph,
    deferred_resolves: &mut Vec<DeferredResolve>,
    clip_store: &ClipStore,
    transforms: &mut TransformPalette,
    prim_headers: &mut PrimitiveHeaders,
    z_generator: &mut ZBufferIdGenerator,
) {
    profile_scope!("RenderPass::build");

    match pass.kind {
        RenderPassKind::MainFramebuffer { ref mut main_target, .. } => {
            for &task_id in &pass.tasks {
                assert_eq!(render_tasks[task_id].target_kind(), RenderTargetKind::Color);
                main_target.add_task(
                    task_id,
                    ctx,
                    gpu_cache,
                    render_tasks,
                    clip_store,
                    transforms,
                    deferred_resolves,
                );
            }
            main_target.build(
                ctx,
                gpu_cache,
                render_tasks,
                deferred_resolves,
                prim_headers,
                transforms,
                z_generator,
            );
        }
        RenderPassKind::OffScreen {
            ref mut color,
            ref mut alpha,
            ref mut texture_cache,
            ref mut picture_cache,
        } => {
            let saved_color = if pass.tasks.iter().any(|&task_id| {
                let t = &render_tasks[task_id];
                t.target_kind() == RenderTargetKind::Color && t.saved_index.is_some()
            }) {
                Some(render_tasks.save_target())
            } else {
                None
            };
            let saved_alpha = if pass.tasks.iter().any(|&task_id| {
                let t = &render_tasks[task_id];
                t.target_kind() == RenderTargetKind::Alpha && t.saved_index.is_some()
            }) {
                Some(render_tasks.save_target())
            } else {
                None
            };

            // Collect a list of picture cache tasks, keyed by picture index.
            // This allows us to only walk that picture root once, adding the
            // primitives to all relevant batches at the same time.
            let mut picture_cache_tasks = FastHashMap::default();

            // Step through each task, adding to batches as appropriate.
            for &task_id in &pass.tasks {
                let (target_kind, texture_target, layer) = {
                    let task = &mut render_tasks[task_id];
                    let target_kind = task.target_kind();

                    // Find a target to assign this task to, or create a new
                    // one if required.
                    let (texture_target, layer) = match task.location {
                        RenderTaskLocation::TextureCache { texture, layer, .. } => {
                            (Some(texture), layer)
                        }
                        RenderTaskLocation::Fixed(..) => {
                            (None, 0)
                        }
                        RenderTaskLocation::Dynamic(ref mut origin, size) => {
                            let (target_index, alloc_origin) =  match target_kind {
                                RenderTargetKind::Color => color.allocate(size),
                                RenderTargetKind::Alpha => alpha.allocate(size),
                            };
                            *origin = Some((alloc_origin, target_index));
                            (None, target_index.0)
                        }
                        RenderTaskLocation::PictureCache { .. } => {
                            // For picture cache tiles, just store them in the map
                            // of picture cache tasks, to be handled below.
                            let pic_index = match task.kind {
                                RenderTaskKind::Picture(ref info) => {
                                    info.pic_index
                                }
                                _ => {
                                    unreachable!();
                                }
                            };

                            picture_cache_tasks
                                .entry(pic_index)
                                .or_insert_with(Vec::new)
                                .push(task_id);

                            continue;
                        }
                    };

                    // Replace the pending saved index with a real one
                    if let Some(index) = task.saved_index {
                        assert_eq!(index, SavedTargetIndex::PENDING);
                        task.saved_index = match target_kind {
                            RenderTargetKind::Color => saved_color,
                            RenderTargetKind::Alpha => saved_alpha,
                        };
                    }

                    // Give the render task an opportunity to add any
                    // information to the GPU cache, if appropriate.
                    task.write_gpu_blocks(gpu_cache);

                    (target_kind, texture_target, layer)
                };

                match texture_target {
                    Some(texture_target) => {
                        let texture = texture_cache
                            .entry((texture_target, layer))
                            .or_insert_with(||
                                TextureCacheRenderTarget::new(target_kind)
                            );
                        texture.add_task(task_id, render_tasks);
                    }
                    None => {
                        match target_kind {
                            RenderTargetKind::Color => {
                                color.targets[layer].add_task(
                                    task_id,
                                    ctx,
                                    gpu_cache,
                                    render_tasks,
                                    clip_store,
                                    transforms,
                                    deferred_resolves,
                                )
                            }
                            RenderTargetKind::Alpha => {
                                alpha.targets[layer].add_task(
                                    task_id,
                                    ctx,
                                    gpu_cache,
                                    render_tasks,
                                    clip_store,
                                    transforms,
                                    deferred_resolves,
                                )
                            }
                        }
                    }
                }
            }

            // For each picture in this pass that has picture cache tiles, create
            // a batcher per task, and then build batches for each of the tasks
            // at the same time.
            for (pic_index, task_ids) in picture_cache_tasks {
                let pic = &ctx.prim_store.pictures[pic_index.0];
                let tile_cache = pic.tile_cache.as_ref().expect("bug");

                // Extract raster/surface spatial nodes for this surface.
                let (root_spatial_node_index, surface_spatial_node_index) = match pic.raster_config {
                    Some(ref rc) => {
                        let surface = &ctx.surfaces[rc.surface_index.0];
                        (surface.raster_spatial_node_index, surface.surface_spatial_node_index)
                    }
                    None => {
                        unreachable!();
                    }
                };

                // Determine the clear color for this picture cache.
                // If the entire tile cache is opaque, we can skip clear completely.
                // If it's the first layer, clear it to white to allow subpixel AA on that
                // first layer even if it's technically transparent.
                // Otherwise, clear to transparent and composite with alpha.
                // TODO(gw): We can detect per-tile opacity for the clear color here
                //           which might be a significant win on some pages?
                let forced_opaque = match tile_cache.background_color {
                    Some(color) => color.a >= 1.0,
                    None => false,
                };
                // TODO(gw): Once we have multiple slices enabled, take advantage of
                //           option to skip clears if the slice is opaque.
                let clear_color = if forced_opaque {
                    Some(ColorF::WHITE)
                } else {
                    Some(ColorF::TRANSPARENT)
                };

                // Create an alpha batcher for each of the tasks of this picture.
                let mut batchers = Vec::new();
                for task_id in &task_ids {
                    let task_id = *task_id;
                    let vis_mask = match render_tasks[task_id].kind {
                        RenderTaskKind::Picture(ref info) => info.vis_mask,
                        _ => unreachable!(),
                    };
                    batchers.push(AlphaBatchBuilder::new(
                        pass.screen_size,
                        ctx.break_advanced_blend_batches,
                        ctx.batch_lookback_count,
                        task_id,
                        render_tasks.get_task_address(task_id),
                        vis_mask,
                    ));
                }

                // Run the batch creation code for this picture, adding items to
                // all relevant per-task batchers.
                let mut batch_builder = BatchBuilder::new(batchers);
                batch_builder.add_pic_to_batch(
                    pic,
                    ctx,
                    gpu_cache,
                    render_tasks,
                    deferred_resolves,
                    prim_headers,
                    transforms,
                    root_spatial_node_index,
                    surface_spatial_node_index,
                    z_generator,
                );

                // Create picture cache targets, one per render task, and assign
                // the correct batcher to them.
                let batchers = batch_builder.finalize();
                for (task_id, batcher) in task_ids.into_iter().zip(batchers.into_iter()) {
                    let task = &render_tasks[task_id];
                    let (target_rect, _) = task.get_target_rect();

                    match task.location {
                        RenderTaskLocation::PictureCache { texture, layer, .. } => {
                            // TODO(gw): The interface here is a bit untidy since it's
                            //           designed to support batch merging, which isn't
                            //           relevant for picture cache targets. We
                            //           can restructure / tidy this up a bit.
                            let scissor_rect  = match render_tasks[task_id].kind {
                                RenderTaskKind::Picture(ref info) => info.scissor_rect,
                                _ => unreachable!(),
                            };
                            let mut batch_containers = Vec::new();
                            let mut alpha_batch_container = AlphaBatchContainer::new(scissor_rect);
                            batcher.build(
                                &mut batch_containers,
                                &mut alpha_batch_container,
                                target_rect,
                                None,
                            );
                            debug_assert!(batch_containers.is_empty());

                            let target = PictureCacheTarget {
                                texture,
                                layer: layer as usize,
                                clear_color,
                                alpha_batch_container,
                            };

                            picture_cache.push(target);
                        }
                        _ => {
                            unreachable!()
                        }
                    }
                }
            }

            color.build(
                ctx,
                gpu_cache,
                render_tasks,
                deferred_resolves,
                saved_color,
                prim_headers,
                transforms,
                z_generator,
            );
            alpha.build(
                ctx,
                gpu_cache,
                render_tasks,
                deferred_resolves,
                saved_alpha,
                prim_headers,
                transforms,
                z_generator,
            );
        }
    }
}

/// A rendering-oriented representation of the frame built by the render backend
/// and presented to the renderer.
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct Frame {
    /// The origin on content produced by the render tasks.
    pub content_origin: DeviceIntPoint,
    /// The rectangle to show the frame in, on screen.
    pub device_rect: DeviceIntRect,
    pub background_color: Option<ColorF>,
    pub layer: DocumentLayer,
    pub passes: Vec<RenderPass>,
    #[cfg_attr(any(feature = "capture", feature = "replay"), serde(default = "FrameProfileCounters::new", skip))]
    pub profile_counters: FrameProfileCounters,

    pub transform_palette: Vec<TransformData>,
    pub render_tasks: RenderTaskGraph,
    pub prim_headers: PrimitiveHeaders,

    /// The GPU cache frame that the contents of Self depend on
    pub gpu_cache_frame_id: FrameId,

    /// List of textures that we don't know about yet
    /// from the backend thread. The render thread
    /// will use a callback to resolve these and
    /// patch the data structures.
    pub deferred_resolves: Vec<DeferredResolve>,

    /// True if this frame contains any render tasks
    /// that write to the texture cache.
    pub has_texture_cache_tasks: bool,

    /// True if this frame has been drawn by the
    /// renderer.
    pub has_been_rendered: bool,

    /// Dirty regions recorded when generating this frame. Empty when not in
    /// testing.
    #[cfg_attr(feature = "serde", serde(skip))]
    pub recorded_dirty_regions: Vec<RecordedDirtyRegion>,

    /// Debugging information to overlay for this frame.
    pub debug_items: Vec<DebugItem>,
}

impl Frame {
    // This frame must be flushed if it writes to the
    // texture cache, and hasn't been drawn yet.
    pub fn must_be_drawn(&self) -> bool {
        self.has_texture_cache_tasks && !self.has_been_rendered
    }
}
