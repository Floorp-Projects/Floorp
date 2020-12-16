/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{ColorF, DebugFlags, FontRenderMode, PremultipliedColorF};
use api::units::*;
use crate::batch::{BatchBuilder, AlphaBatchBuilder, AlphaBatchContainer};
use crate::clip::{ClipStore, ClipChainStack};
use crate::spatial_tree::{SpatialTree, ROOT_SPATIAL_NODE_INDEX, SpatialNodeIndex};
use crate::composite::{CompositorKind, CompositeState, CompositeStatePreallocator};
use crate::debug_render::DebugItem;
use crate::gpu_cache::{GpuCache, GpuCacheHandle};
use crate::gpu_types::{PrimitiveHeaders, TransformPalette, ZBufferIdGenerator};
use crate::gpu_types::TransformData;
use crate::internal_types::{CacheTextureId, FastHashMap, PlaneSplitter};
use crate::picture::{DirtyRegion, PictureUpdateState, SliceId, TileCacheInstance};
use crate::picture::{SurfaceInfo, SurfaceIndex, ROOT_SURFACE_INDEX};
use crate::picture::{BackdropKind, SubpixelMode, TileCacheLogger, RasterConfig, PictureCompositeMode};
use crate::prepare::prepare_primitives;
use crate::prim_store::{PictureIndex, PrimitiveDebugId};
use crate::prim_store::{DeferredResolve};
use crate::profiler::{self, TransactionProfile};
use crate::render_backend::{DataStores, FrameStamp, FrameId, ScratchBuffer};
use crate::render_target::{RenderTarget, PictureCacheTarget, TextureCacheRenderTarget};
use crate::render_target::{RenderTargetContext, RenderTargetKind};
use crate::render_task_graph::{RenderTaskId, RenderTaskGraph, RenderTaskGraphCounters};
use crate::render_task_graph::RenderPass;
use crate::render_task::{RenderTaskLocation, RenderTaskKind};
use crate::resource_cache::{ResourceCache};
use crate::scene::{BuiltScene, SceneProperties};
use crate::space::SpaceMapper;
use crate::segment::SegmentBuilder;
use std::{f32, mem};
use crate::util::{VecHelper, Recycler, Preallocator};
use crate::visibility::{update_primitive_visibility, FrameVisibilityState, FrameVisibilityContext};
use crate::visibility::{PrimitiveVisibilityMask};


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
    /// True if we're running tests (i.e. via wrench).
    pub testing: bool,
    pub gpu_supports_fast_clears: bool,
    pub gpu_supports_advanced_blend: bool,
    pub advanced_blend_is_coherent: bool,
    pub gpu_supports_render_target_partial_update: bool,
    pub batch_lookback_count: usize,
    pub background_color: Option<ColorF>,
    pub compositor_kind: CompositorKind,
    pub tile_size_override: Option<DeviceIntSize>,
    pub max_depth_ids: i32,
    pub max_target_size: i32,
}

/// A set of common / global resources that are retained between
/// new display lists, such that any GPU cache handles can be
/// persisted even when a new display list arrives.
#[cfg_attr(feature = "capture", derive(Serialize))]
pub struct FrameGlobalResources {
    /// The image shader block for the most common / default
    /// set of image parameters (color white, stretch == rect.size).
    pub default_image_handle: GpuCacheHandle,

    /// A GPU cache config for drawing transparent rectangle primitives.
    /// This is used to 'cut out' overlay tiles where a compositor
    /// surface exists.
    pub default_transparent_rect_handle: GpuCacheHandle,
}

impl FrameGlobalResources {
    pub fn empty() -> Self {
        FrameGlobalResources {
            default_image_handle: GpuCacheHandle::new(),
            default_transparent_rect_handle: GpuCacheHandle::new(),
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

        if let Some(mut request) = gpu_cache.request(&mut self.default_transparent_rect_handle) {
            request.push(PremultipliedColorF::TRANSPARENT);
        }
    }
}

pub struct FrameScratchBuffer {
    surfaces: Vec<SurfaceInfo>,
    dirty_region_stack: Vec<DirtyRegion>,
    surface_stack: Vec<SurfaceIndex>,
    clip_chain_stack: ClipChainStack,
}

impl Default for FrameScratchBuffer {
    fn default() -> Self {
        FrameScratchBuffer {
            surfaces: Vec::new(),
            dirty_region_stack: Vec::new(),
            surface_stack: Vec::new(),
            clip_chain_stack: ClipChainStack::new(),
        }
    }
}

impl FrameScratchBuffer {
    pub fn begin_frame(&mut self) {
        self.surfaces.clear();
        self.dirty_region_stack.clear();
        self.surface_stack.clear();
        self.clip_chain_stack.clear();
    }

    pub fn recycle(&mut self, recycler: &mut Recycler) {
        recycler.recycle_vec(&mut self.surfaces);
        // Don't call recycle on the stacks because the reycler's
        // role is to get rid of allocations when the capacity
        // is much larger than the lengths. with stacks the
        // length varies through the frame but is supposedly
        // back to zero by the end so we would always throw the
        // allocation away.
    }
}

/// Produces the frames that are sent to the renderer.
#[cfg_attr(feature = "capture", derive(Serialize))]
pub struct FrameBuilder {
    pub globals: FrameGlobalResources,
    #[cfg_attr(feature = "capture", serde(skip))]
    prim_headers_prealloc: Preallocator,
    #[cfg_attr(feature = "capture", serde(skip))]
    composite_state_prealloc: CompositeStatePreallocator,
}

pub struct FrameBuildingContext<'a> {
    pub global_device_pixel_scale: DevicePixelScale,
    pub scene_properties: &'a SceneProperties,
    pub global_screen_world_rect: WorldRect,
    pub spatial_tree: &'a SpatialTree,
    pub max_local_clip: LayoutRect,
    pub debug_flags: DebugFlags,
    pub fb_config: &'a FrameBuilderConfig,
}

pub struct FrameBuildingState<'a> {
    pub render_tasks: &'a mut RenderTaskGraph,
    pub clip_store: &'a mut ClipStore,
    pub resource_cache: &'a mut ResourceCache,
    pub gpu_cache: &'a mut GpuCache,
    pub transforms: &'a mut TransformPalette,
    pub segment_builder: SegmentBuilder,
    pub surfaces: &'a mut Vec<SurfaceInfo>,
    pub dirty_region_stack: Vec<DirtyRegion>,
    pub composite_state: &'a mut CompositeState,
    pub num_visible_primitives: u32,
    pub render_task_roots: &'a mut Vec<RenderTaskId>,
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
            globals: FrameGlobalResources::empty(),
            prim_headers_prealloc: Preallocator::new(0),
            composite_state_prealloc: CompositeStatePreallocator::default(),
        }
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
        global_device_pixel_scale: DevicePixelScale,
        scene_properties: &SceneProperties,
        transform_palette: &mut TransformPalette,
        data_stores: &mut DataStores,
        scratch: &mut ScratchBuffer,
        debug_flags: DebugFlags,
        composite_state: &mut CompositeState,
        tile_cache_logger: &mut TileCacheLogger,
        tile_caches: &mut FastHashMap<SliceId, Box<TileCacheInstance>>,
        profile: &mut TransactionProfile,
        render_task_roots: &mut Vec<RenderTaskId>,
    ) {
        profile_scope!("build_layer_screen_rects_and_cull_layers");

        if scene.prim_store.pictures.is_empty() {
            return
        }

        scratch.begin_frame();

        let root_spatial_node_index = scene.spatial_tree.root_reference_frame_index();

        const MAX_CLIP_COORD: f32 = 1.0e9;

        let frame_context = FrameBuildingContext {
            global_device_pixel_scale,
            scene_properties,
            global_screen_world_rect,
            spatial_tree: &scene.spatial_tree,
            max_local_clip: LayoutRect::new(
                LayoutPoint::new(-MAX_CLIP_COORD, -MAX_CLIP_COORD),
                LayoutSize::new(2.0 * MAX_CLIP_COORD, 2.0 * MAX_CLIP_COORD),
            ),
            debug_flags,
            fb_config: &scene.config,
        };

        // Construct a dummy root surface, that represents the
        // main framebuffer surface.
        let root_surface = SurfaceInfo::new(
            ROOT_SPATIAL_NODE_INDEX,
            ROOT_SPATIAL_NODE_INDEX,
            0.0,
            global_screen_world_rect,
            &scene.spatial_tree,
            global_device_pixel_scale,
            (1.0, 1.0),
        );
        let mut surfaces = scratch.frame.surfaces.take();
        surfaces.push(root_surface);

        // The first major pass of building a frame is to walk the picture
        // tree. This pass must be quick (it should never touch individual
        // primitives). For now, all we do here is determine which pictures
        // will create surfaces. In the future, this will be expanded to
        // set up render tasks, determine scaling of surfaces, and detect
        // which surfaces have valid cached surfaces that don't need to
        // be rendered this frame.
        PictureUpdateState::update_all(
            &mut scratch.picture,
            &mut surfaces,
            scene.root_pic_index,
            &mut scene.prim_store.pictures,
            &frame_context,
            gpu_cache,
            &scene.clip_store,
            data_stores,
        );

        {
            profile_scope!("UpdateVisibility");
            profile_marker!("UpdateVisibility");
            profile.start_time(profiler::FRAME_VISIBILITY_TIME);

            let visibility_context = FrameVisibilityContext {
                global_device_pixel_scale,
                spatial_tree: &scene.spatial_tree,
                global_screen_world_rect,
                surfaces: &mut surfaces,
                debug_flags,
                scene_properties,
                config: scene.config,
            };

            let mut visibility_state = FrameVisibilityState {
                clip_chain_stack: scratch.frame.clip_chain_stack.take(),
                surface_stack: scratch.frame.surface_stack.take(),
                resource_cache,
                gpu_cache,
                clip_store: &mut scene.clip_store,
                scratch,
                tile_cache: None,
                data_stores,
                render_tasks,
                composite_state,
            };

            update_primitive_visibility(
                &mut scene.prim_store,
                scene.root_pic_index,
                ROOT_SURFACE_INDEX,
                &global_screen_world_rect,
                &visibility_context,
                &mut visibility_state,
                tile_caches,
            );

            visibility_state.scratch.frame.clip_chain_stack = visibility_state.clip_chain_stack.take();
            visibility_state.scratch.frame.surface_stack = visibility_state.surface_stack.take();

            profile.end_time(profiler::FRAME_VISIBILITY_TIME);
        }

        profile.start_time(profiler::FRAME_PREPARE_TIME);

        let mut frame_state = FrameBuildingState {
            render_tasks,
            clip_store: &mut scene.clip_store,
            resource_cache,
            gpu_cache,
            transforms: transform_palette,
            segment_builder: SegmentBuilder::new(),
            surfaces: &mut surfaces,
            dirty_region_stack: scratch.frame.dirty_region_stack.take(),
            composite_state,
            num_visible_primitives: 0,
            render_task_roots,
        };

        // Push a default dirty region which culls primitives
        // against the screen world rect, in absence of any
        // other dirty regions.
        let mut default_dirty_region = DirtyRegion::new(
            ROOT_SPATIAL_NODE_INDEX,
        );
        default_dirty_region.add_dirty_region(
            frame_context.global_screen_world_rect.cast_unit(),
            frame_context.spatial_tree,
        );
        frame_state.push_dirty_region(default_dirty_region);

        let (pic_context, mut pic_state, mut prim_list) = scene
            .prim_store
            .pictures[scene.root_pic_index.0]
            .take_context(
                scene.root_pic_index,
                root_spatial_node_index,
                root_spatial_node_index,
                ROOT_SURFACE_INDEX,
                &SubpixelMode::Allow,
                &mut frame_state,
                &frame_context,
                &mut scratch.primitive,
                tile_cache_logger,
                tile_caches,
            )
            .unwrap();

        tile_cache_logger.advance();

        {
            profile_marker!("PreparePrims");

            prepare_primitives(
                &mut scene.prim_store,
                &mut prim_list,
                &pic_context,
                &mut pic_state,
                &frame_context,
                &mut frame_state,
                data_stores,
                &mut scratch.primitive,
                tile_cache_logger,
                tile_caches,
            );
        }

        let pic = &mut scene.prim_store.pictures[scene.root_pic_index.0];
        pic.restore_context(
            ROOT_SURFACE_INDEX,
            prim_list,
            pic_context,
            pic_state,
            &mut frame_state,
        );

        frame_state.pop_dirty_region();
        profile.end_time(profiler::FRAME_PREPARE_TIME);
        profile.set(profiler::VISIBLE_PRIMITIVES, frame_state.num_visible_primitives);

        scratch.frame.dirty_region_stack = frame_state.dirty_region_stack.take();
        scratch.frame.surfaces = surfaces.take();

        {
            profile_marker!("BlockOnResources");

            resource_cache.block_until_all_resources_added(
                gpu_cache,
                profile,
            );
        }
    }

    pub fn build(
        &mut self,
        scene: &mut BuiltScene,
        resource_cache: &mut ResourceCache,
        gpu_cache: &mut GpuCache,
        stamp: FrameStamp,
        global_device_pixel_scale: DevicePixelScale,
        device_origin: DeviceIntPoint,
        pan: WorldPoint,
        scene_properties: &SceneProperties,
        data_stores: &mut DataStores,
        scratch: &mut ScratchBuffer,
        render_task_counters: &mut RenderTaskGraphCounters,
        debug_flags: DebugFlags,
        tile_cache_logger: &mut TileCacheLogger,
        tile_caches: &mut FastHashMap<SliceId, Box<TileCacheInstance>>,
        dirty_rects_are_valid: bool,
        profile: &mut TransactionProfile,
    ) -> Frame {
        profile_scope!("build");
        profile_marker!("BuildFrame");

        profile.set(profiler::PRIMITIVES, scene.prim_store.prim_count());
        profile.set(profiler::PICTURE_CACHE_SLICES, scene.tile_cache_config.picture_cache_slice_count);
        resource_cache.begin_frame(stamp);
        gpu_cache.begin_frame(stamp);

        self.globals.update(gpu_cache);

        scene.spatial_tree.update_tree(
            pan,
            global_device_pixel_scale,
            scene_properties,
        );
        let mut transform_palette = scene.spatial_tree.build_transform_palette();
        scene.clip_store.clear_old_instances();

        let mut render_tasks = RenderTaskGraph::new(
            stamp.frame_id(),
            render_task_counters,
        );

        let output_size = scene.output_rect.size.to_i32();
        let screen_world_rect = (scene.output_rect.to_f32() / global_device_pixel_scale).round_out();

        let mut composite_state = CompositeState::new(
            scene.config.compositor_kind,
            global_device_pixel_scale,
            scene.config.max_depth_ids,
            dirty_rects_are_valid,
        );

        self.composite_state_prealloc.preallocate(&mut composite_state);

        let mut render_task_roots = Vec::new();
        self.build_layer_screen_rects_and_cull_layers(
            scene,
            screen_world_rect,
            resource_cache,
            gpu_cache,
            &mut render_tasks,
            global_device_pixel_scale,
            scene_properties,
            &mut transform_palette,
            data_stores,
            scratch,
            debug_flags,
            &mut composite_state,
            tile_cache_logger,
            tile_caches,
            profile,
            &mut render_task_roots,
        );

        profile.start_time(profiler::FRAME_BATCHING_TIME);

        let mut passes;
        let mut deferred_resolves = vec![];
        let mut has_texture_cache_tasks = false;
        let mut prim_headers = PrimitiveHeaders::new();
        self.prim_headers_prealloc.preallocate_vec(&mut prim_headers.headers_int);
        self.prim_headers_prealloc.preallocate_vec(&mut prim_headers.headers_float);

        {
            profile_marker!("Batching");

            passes = render_tasks.generate_passes(
                &render_task_roots,
                output_size,
                scene.config.gpu_supports_fast_clears,
            );

            // Used to generated a unique z-buffer value per primitive.
            let mut z_generator = ZBufferIdGenerator::new(scene.config.max_depth_ids);
            let use_dual_source_blending = scene.config.dual_source_blending_is_enabled &&
                                           scene.config.dual_source_blending_is_supported;

            // As an incremental approach to moving to a proper DAG for render tasks, we
            // implement the existing saved texture functionality here.

            // Textures that are marked for saving until the end of the frame
            let mut saved_texture_ids = Vec::new();
            // Textures that are written to on this pass and should be returned / invalidated
            // on the following pass.
            let mut active_texture_ids = Vec::new();

            for pass in &mut passes {
                let mut ctx = RenderTargetContext {
                    global_device_pixel_scale,
                    prim_store: &scene.prim_store,
                    resource_cache,
                    use_dual_source_blending,
                    use_advanced_blending: scene.config.gpu_supports_advanced_blend,
                    break_advanced_blend_batches: !scene.config.advanced_blend_is_coherent,
                    batch_lookback_count: scene.config.batch_lookback_count,
                    spatial_tree: &scene.spatial_tree,
                    data_stores,
                    surfaces: &scratch.frame.surfaces,
                    scratch: &mut scratch.primitive,
                    screen_world_rect,
                    globals: &self.globals,
                    tile_caches,
                };

                build_render_pass(
                    pass,
                    output_size,
                    &mut ctx,
                    gpu_cache,
                    &mut render_tasks,
                    &mut deferred_resolves,
                    &scene.clip_store,
                    &mut transform_palette,
                    &mut prim_headers,
                    &mut z_generator,
                    &mut composite_state,
                );

                has_texture_cache_tasks |= !pass.texture_cache.is_empty();
                has_texture_cache_tasks |= !pass.picture_cache.is_empty();

                // Check which textures in this pass need to be saved until the end of
                // the frame. This will all disappear once render task graph is a full DAG.
                let mut save_color = false;
                let mut save_alpha = false;

                for task_id in &pass.tasks {
                    let task = &render_tasks[*task_id];
                    match task.target_kind() {
                        RenderTargetKind::Color => {
                            save_color |= task.save_target;
                        }
                        RenderTargetKind::Alpha => {
                            save_alpha |= task.save_target;
                        }
                    }
                }

                // Return previous frame's textures to the target pool, so they are available
                // for use on subsequent passes. Also include these in the list for the renderer
                // to immediately invalidate once they are no longer used.
                for texture_id in active_texture_ids.drain(..) {
                    resource_cache.return_render_target_to_pool(texture_id);
                    pass.textures_to_invalidate.push(texture_id);
                }

                if let Some(texture_id) = pass.color.texture_id {
                    if save_color {
                        saved_texture_ids.push(texture_id);
                    } else {
                        active_texture_ids.push(texture_id);
                    }
                }

                if let Some(texture_id) = pass.alpha.texture_id {
                    if save_alpha {
                        saved_texture_ids.push(texture_id);
                    } else {
                        active_texture_ids.push(texture_id);
                    }
                }
            }

            assert!(active_texture_ids.is_empty());

            // At the end of the pass loop, return any saved textures to the pool. Also
            // add them to the final pass to invalidate those textures (which will be the
            // pass that writes to picture cache tiles or texture cache targets). This is
            // mostly implicit in the current scheme, but will become a lot clearer and
            // more explicit once we implement the full render task DAG.

            for texture_id in saved_texture_ids.drain(..) {
                resource_cache.return_render_target_to_pool(texture_id);
                passes.last_mut().unwrap().textures_to_invalidate.push(texture_id);
            }

            let mut ctx = RenderTargetContext {
                global_device_pixel_scale,
                prim_store: &scene.prim_store,
                resource_cache,
                use_dual_source_blending,
                use_advanced_blending: scene.config.gpu_supports_advanced_blend,
                break_advanced_blend_batches: !scene.config.advanced_blend_is_coherent,
                batch_lookback_count: scene.config.batch_lookback_count,
                spatial_tree: &scene.spatial_tree,
                data_stores,
                surfaces: &scratch.frame.surfaces,
                scratch: &mut scratch.primitive,
                screen_world_rect,
                globals: &self.globals,
                tile_caches,
            };

            self.build_composite_pass(
                scene,
                &mut ctx,
                gpu_cache,
                &render_tasks,
                &mut deferred_resolves,
                &mut transform_palette,
                &mut prim_headers,
                &mut z_generator,
                &mut composite_state,
            );
        }

        profile.end_time(profiler::FRAME_BATCHING_TIME);

        let gpu_cache_frame_id = gpu_cache.end_frame(profile).frame_id();

        render_tasks.write_task_data();
        *render_task_counters = render_tasks.counters();
        resource_cache.end_frame(profile);

        self.prim_headers_prealloc.record_vec(&mut prim_headers.headers_int);
        self.composite_state_prealloc.record(&composite_state);

        Frame {
            device_rect: DeviceIntRect::new(
                device_origin,
                scene.output_rect.size,
            ),
            passes,
            transform_palette: transform_palette.finish(),
            render_tasks,
            deferred_resolves,
            gpu_cache_frame_id,
            has_been_rendered: false,
            has_texture_cache_tasks,
            prim_headers,
            debug_items: mem::replace(&mut scratch.primitive.debug_items, Vec::new()),
            composite_state,
        }
    }

    /// This is a temporary method that does the minimal amount of work to
    /// invoke the existing batch builder code to produce the composite
    /// state for the main pass.
    // TODO(gw): Remove the code to create the tile cache composite state
    //           from batch building next, and this can then be simplified too.
    fn build_composite_pass(
        &self,
        scene: &BuiltScene,
        ctx: &RenderTargetContext,
        gpu_cache: &mut GpuCache,
        render_tasks: &RenderTaskGraph,
        deferred_resolves: &mut Vec<DeferredResolve>,
        transforms: &mut TransformPalette,
        prim_headers: &mut PrimitiveHeaders,
        z_generator: &mut ZBufferIdGenerator,
        composite_state: &mut CompositeState,
    ) {
        let pic = &ctx.prim_store.pictures[scene.root_pic_index.0];

        let alpha_batch_builder = AlphaBatchBuilder::new(
            scene.output_rect.size,
            false,
            0,
            RenderTaskId::INVALID,
            RenderTaskId::INVALID.into(),
            PrimitiveVisibilityMask::all(),
            0,
        );

        let mut batch_builder = BatchBuilder::new(
            vec![alpha_batch_builder],
        );

        batch_builder.add_pic_to_batch(
            pic,
            ctx,
            gpu_cache,
            render_tasks,
            deferred_resolves,
            prim_headers,
            transforms,
            ROOT_SPATIAL_NODE_INDEX,
            ROOT_SPATIAL_NODE_INDEX,
            z_generator,
            composite_state,
        );

        let alpha_batch_builders = batch_builder.finalize();
        let mut merged_batches = Vec::new();
        let mut alpha_batch_container = AlphaBatchContainer::new(None);

        for batcher in alpha_batch_builders {
            batcher.build(
                &mut merged_batches,
                &mut alpha_batch_container,
                scene.output_rect,
                None,
            );
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
    screen_size: DeviceIntSize,
    ctx: &mut RenderTargetContext,
    gpu_cache: &mut GpuCache,
    render_tasks: &mut RenderTaskGraph,
    deferred_resolves: &mut Vec<DeferredResolve>,
    clip_store: &ClipStore,
    transforms: &mut TransformPalette,
    prim_headers: &mut PrimitiveHeaders,
    z_generator: &mut ZBufferIdGenerator,
    composite_state: &mut CompositeState,
) {
    profile_scope!("build_render_pass");

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
                RenderTaskLocation::Dynamic(ref mut origin, size) => {
                    let (target_index, alloc_origin) =  match target_kind {
                        RenderTargetKind::Color => pass.color.allocate(size),
                        RenderTargetKind::Alpha => pass.alpha.allocate(size),
                    };
                    *origin = Some((alloc_origin, CacheTextureId::INVALID, target_index));
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

            // Give the render task an opportunity to add any
            // information to the GPU cache, if appropriate.
            let (target_rect, target_index) = task.get_target_rect();
            task.kind.write_gpu_blocks(
                target_rect,
                target_index,
                gpu_cache,
            );

            (target_kind, texture_target, layer)
        };

        match texture_target {
            Some(texture_target) => {
                let texture = pass.texture_cache
                    .entry((texture_target, layer))
                    .or_insert_with(||
                        TextureCacheRenderTarget::new(target_kind)
                    );
                texture.add_task(task_id, render_tasks);
            }
            None => {
                match target_kind {
                    RenderTargetKind::Color => {
                        pass.color.targets[layer].add_task(
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
                        pass.alpha.targets[layer].add_task(
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
        profile_scope!("picture_cache_task");
        let pic = &ctx.prim_store.pictures[pic_index.0];

        // Extract raster/surface spatial nodes for this surface.
        let (root_spatial_node_index, surface_spatial_node_index, tile_cache) = match pic.raster_config {
            Some(RasterConfig { surface_index, composite_mode: PictureCompositeMode::TileCache { slice_id }, .. }) => {
                let surface = &ctx.surfaces[surface_index.0];
                (
                    surface.raster_spatial_node_index,
                    surface.surface_spatial_node_index,
                    &ctx.tile_caches[&slice_id],
                )
            }
            _ => {
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
        let mut clear_color = if forced_opaque {
            Some(ColorF::WHITE)
        } else {
            Some(ColorF::TRANSPARENT)
        };

        // If this picture cache has a valid color backdrop, we will use
        // that as the clear color, skipping the draw of the backdrop
        // primitive (and anything prior to it) during batching.
        if let Some(BackdropKind::Color { color }) = tile_cache.backdrop.kind {
            clear_color = Some(color);
        }

        // Create an alpha batcher for each of the tasks of this picture.
        let mut batchers = Vec::new();
        for task_id in &task_ids {
            let task_id = *task_id;
            let vis_mask = match render_tasks[task_id].kind {
                RenderTaskKind::Picture(ref info) => info.vis_mask,
                _ => unreachable!(),
            };
            batchers.push(AlphaBatchBuilder::new(
                screen_size,
                ctx.break_advanced_blend_batches,
                ctx.batch_lookback_count,
                task_id,
                task_id.into(),
                vis_mask,
                0,
            ));
        }

        // Run the batch creation code for this picture, adding items to
        // all relevant per-task batchers.
        let mut batch_builder = BatchBuilder::new(batchers);
        {
        profile_scope!("add_pic_to_batch");
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
            composite_state,
        );
        }

        // Create picture cache targets, one per render task, and assign
        // the correct batcher to them.
        let batchers = batch_builder.finalize();
        for (task_id, batcher) in task_ids.into_iter().zip(batchers.into_iter()) {
            profile_scope!("task");
            let task = &render_tasks[task_id];
            let (target_rect, _) = task.get_target_rect();

            match task.location {
                RenderTaskLocation::PictureCache { ref surface, .. } => {
                    // TODO(gw): The interface here is a bit untidy since it's
                    //           designed to support batch merging, which isn't
                    //           relevant for picture cache targets. We
                    //           can restructure / tidy this up a bit.
                    let (scissor_rect, valid_rect)  = match render_tasks[task_id].kind {
                        RenderTaskKind::Picture(ref info) => {
                            (
                                info.scissor_rect.expect("bug: must be set for cache tasks"),
                                info.valid_rect.expect("bug: must be set for cache tasks"),
                            )
                        }
                        _ => unreachable!(),
                    };
                    let mut batch_containers = Vec::new();
                    let mut alpha_batch_container = AlphaBatchContainer::new(Some(scissor_rect));
                    batcher.build(
                        &mut batch_containers,
                        &mut alpha_batch_container,
                        target_rect,
                        None,
                    );
                    debug_assert!(batch_containers.is_empty());

                    let target = PictureCacheTarget {
                        surface: surface.clone(),
                        clear_color,
                        alpha_batch_container,
                        dirty_rect: scissor_rect,
                        valid_rect,
                    };

                    pass.picture_cache.push(target);
                }
                _ => {
                    unreachable!()
                }
            }
        }
    }

    pass.color.build(
        ctx,
        gpu_cache,
        render_tasks,
        deferred_resolves,
        prim_headers,
        transforms,
        z_generator,
        composite_state,
    );
    pass.alpha.build(
        ctx,
        gpu_cache,
        render_tasks,
        deferred_resolves,
        prim_headers,
        transforms,
        z_generator,
        composite_state,
    );

    // Now that the passes have been built, we know what the texture_id is for this surface
    // (since we know the layer count and texture size). Step through the tasks on this
    // texture and store that in the task location. This is used so that tasks that get added
    // on following passes can directly access and reference the texture_id, rather than
    // referring to PrevPassAlpha/Color. Again, this will become a lot simpler once we
    // have the full DAG in place (and once sub-passes are individual textures rather than
    // a single texture array).

    for &task_id in &pass.tasks {
        let task = &mut render_tasks[task_id];
        let target_kind = task.target_kind();

        match task.location {
            RenderTaskLocation::TextureCache { .. } |
            RenderTaskLocation::PictureCache { .. } => {}

            RenderTaskLocation::Dynamic(None, _) => {
                unreachable!();
            }

            RenderTaskLocation::Dynamic(Some((_, ref mut texture_id, _)), _) => {
                assert_eq!(*texture_id, CacheTextureId::INVALID);

                match target_kind {
                    RenderTargetKind::Color => {
                        *texture_id = pass
                            .color
                            .texture_id
                            .expect("bug: color texture must be allocated by now");
                    }
                    RenderTargetKind::Alpha => {
                        *texture_id = pass
                            .alpha
                            .texture_id
                            .expect("bug: alpha texture must be allocated by now");
                    }
                }
            }
        }
    }
}

/// A rendering-oriented representation of the frame built by the render backend
/// and presented to the renderer.
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct Frame {
    /// The rectangle to show the frame in, on screen.
    pub device_rect: DeviceIntRect,
    pub passes: Vec<RenderPass>,

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

    /// Debugging information to overlay for this frame.
    pub debug_items: Vec<DebugItem>,

    /// Contains picture cache tiles, and associated information.
    /// Used by the renderer to composite tiles into the framebuffer,
    /// or hand them off to an OS compositor.
    pub composite_state: CompositeState,
}

impl Frame {
    // This frame must be flushed if it writes to the
    // texture cache, and hasn't been drawn yet.
    pub fn must_be_drawn(&self) -> bool {
        self.has_texture_cache_tasks && !self.has_been_rendered
    }

    // Returns true if this frame doesn't alter what is on screen currently.
    pub fn is_nop(&self) -> bool {
        // If there are no off-screen passes, that implies that there are no
        // picture cache tiles, and no texture cache tasks being updates. If this
        // is the case, we can consider the frame a nop (higher level checks
        // test if a composite is needed due to picture cache surfaces moving
        // or external surfaces being updated).
        self.passes.is_empty()
    }
}
