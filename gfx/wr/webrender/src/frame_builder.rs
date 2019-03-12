/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{ColorF, DebugFlags, DocumentLayer, FontRenderMode, PremultipliedColorF};
use api::{PipelineId, RasterSpace};
use api::units::*;
use clip::{ClipDataStore, ClipStore, ClipChainStack};
use clip_scroll_tree::{ClipScrollTree, ROOT_SPATIAL_NODE_INDEX, SpatialNodeIndex};
use display_list_flattener::{DisplayListFlattener};
use gpu_cache::{GpuCache, GpuCacheHandle};
use gpu_types::{PrimitiveHeaders, TransformPalette, UvRectKind, ZBufferIdGenerator};
use hit_test::{HitTester, HitTestingScene};
#[cfg(feature = "replay")]
use hit_test::HitTestingSceneStats;
use internal_types::{FastHashMap, PlaneSplitter};
use picture::{PictureSurface, PictureUpdateState, SurfaceInfo, ROOT_SURFACE_INDEX, SurfaceIndex};
use picture::{RetainedTiles, TileCache, DirtyRegion};
use prim_store::{PrimitiveStore, SpaceMapper, PictureIndex, PrimitiveDebugId, PrimitiveScratchBuffer};
#[cfg(feature = "replay")]
use prim_store::{PrimitiveStoreStats};
use profiler::{FrameProfileCounters, GpuCacheProfileCounters, TextureCacheProfileCounters};
use render_backend::{DataStores, FrameStamp};
use render_task::{RenderTask, RenderTaskId, RenderTaskLocation, RenderTaskTree, RenderTaskTreeCounters};
use resource_cache::{ResourceCache};
use scene::{ScenePipeline, SceneProperties};
use scene_builder::DocumentStats;
use segment::SegmentBuilder;
use spatial_node::SpatialNode;
use std::{f32, mem};
use std::sync::Arc;
use tiling::{Frame, RenderPass, RenderPassKind, RenderTargetContext, RenderTarget};


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

#[derive(Clone, Copy)]
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

/// A builder structure for `tiling::Frame`
#[cfg_attr(feature = "capture", derive(Serialize))]
pub struct FrameBuilder {
    output_rect: DeviceIntRect,
    background_color: Option<ColorF>,
    root_pic_index: PictureIndex,
    /// Cache of surface tiles from the previous frame builder
    /// that can optionally be consumed by this frame builder.
    pending_retained_tiles: RetainedTiles,
    pub prim_store: PrimitiveStore,
    pub clip_store: ClipStore,
    #[cfg_attr(feature = "capture", serde(skip))] //TODO
    pub hit_testing_scene: Arc<HitTestingScene>,
    pub config: FrameBuilderConfig,
    pub globals: FrameGlobalResources,
}

pub struct FrameVisibilityContext<'a> {
    pub clip_scroll_tree: &'a ClipScrollTree,
    pub screen_world_rect: WorldRect,
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
    pub tile_cache: Option<TileCache>,
    pub retained_tiles: &'a mut RetainedTiles,
    pub data_stores: &'a mut DataStores,
    pub clip_chain_stack: ClipChainStack,
}

pub struct FrameBuildingContext<'a> {
    pub global_device_pixel_scale: DevicePixelScale,
    pub scene_properties: &'a SceneProperties,
    pub pipelines: &'a FastHashMap<PipelineId, Arc<ScenePipeline>>,
    pub screen_world_rect: WorldRect,
    pub clip_scroll_tree: &'a ClipScrollTree,
    pub max_local_clip: LayoutRect,
    pub debug_flags: DebugFlags,
    pub fb_config: &'a FrameBuilderConfig,
}

pub struct FrameBuildingState<'a> {
    pub render_tasks: &'a mut RenderTaskTree,
    pub profile_counters: &'a mut FrameProfileCounters,
    pub clip_store: &'a mut ClipStore,
    pub resource_cache: &'a mut ResourceCache,
    pub gpu_cache: &'a mut GpuCache,
    pub transforms: &'a mut TransformPalette,
    pub segment_builder: SegmentBuilder,
    pub surfaces: &'a mut Vec<SurfaceInfo>,
    pub dirty_region_stack: Vec<DirtyRegion>,
    pub clip_chain_stack: ClipChainStack,
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
    pub allow_subpixel_aa: bool,
    pub is_passthrough: bool,
    pub raster_space: RasterSpace,
    pub surface_spatial_node_index: SpatialNodeIndex,
    pub raster_spatial_node_index: SpatialNodeIndex,
    /// The surface that this picture will render on.
    pub surface_index: SurfaceIndex,
    pub dirty_region_count: usize,
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

pub struct PrimitiveContext<'a> {
    pub spatial_node: &'a SpatialNode,
    pub spatial_node_index: SpatialNodeIndex,
}

impl<'a> PrimitiveContext<'a> {
    pub fn new(
        spatial_node: &'a SpatialNode,
        spatial_node_index: SpatialNodeIndex,
    ) -> Self {
        PrimitiveContext {
            spatial_node,
            spatial_node_index,
        }
    }
}

impl FrameBuilder {
    #[cfg(feature = "replay")]
    pub fn empty() -> Self {
        FrameBuilder {
            hit_testing_scene: Arc::new(HitTestingScene::new(&HitTestingSceneStats::empty())),
            prim_store: PrimitiveStore::new(&PrimitiveStoreStats::empty()),
            clip_store: ClipStore::new(),
            output_rect: DeviceIntRect::zero(),
            background_color: None,
            root_pic_index: PictureIndex(0),
            pending_retained_tiles: RetainedTiles::new(),
            globals: FrameGlobalResources::empty(),
            config: FrameBuilderConfig {
                default_font_render_mode: FontRenderMode::Mono,
                dual_source_blending_is_enabled: true,
                dual_source_blending_is_supported: false,
                chase_primitive: ChasePrimitive::Nothing,
                enable_picture_caching: false,
                testing: false,
                gpu_supports_fast_clears: false,
            },
        }
    }

    /// Provide any cached surface tiles from the previous frame builder
    /// to a new frame builder. These will be consumed or dropped the
    /// first time a new frame builder creates a frame.
    pub fn set_retained_resources(
        &mut self,
        retained_tiles: RetainedTiles,
        globals: FrameGlobalResources,
    ) {
        debug_assert!(self.pending_retained_tiles.tiles.is_empty());
        self.pending_retained_tiles = retained_tiles;
        self.globals = globals;
    }

    pub fn with_display_list_flattener(
        output_rect: DeviceIntRect,
        background_color: Option<ColorF>,
        flattener: DisplayListFlattener,
    ) -> Self {
        FrameBuilder {
            hit_testing_scene: Arc::new(flattener.hit_testing_scene),
            prim_store: flattener.prim_store,
            clip_store: flattener.clip_store,
            root_pic_index: flattener.root_pic_index,
            output_rect,
            background_color,
            pending_retained_tiles: RetainedTiles::new(),
            config: flattener.config,
            globals: FrameGlobalResources::empty(),
        }
    }

    /// Get the memory usage statistics to pre-allocate for the next scene.
    pub fn get_stats(&self) -> DocumentStats {
        DocumentStats {
            prim_store_stats: self.prim_store.get_stats(),
            hit_test_stats: self.hit_testing_scene.get_stats(),
        }
    }

    /// Destroy an existing frame builder. This is called just before
    /// a frame builder is replaced with a newly built scene.
    pub fn destroy(
        self,
        retained_tiles: &mut RetainedTiles,
        clip_scroll_tree: &ClipScrollTree,
    ) -> FrameGlobalResources {
        self.prim_store.destroy(
            retained_tiles,
            clip_scroll_tree,
        );

        // In general, the pending retained tiles are consumed by the frame
        // builder the first time a frame is built after a new scene has
        // arrived. However, if two scenes arrive in quick succession, the
        // frame builder may not have had a chance to build a frame and
        // consume the pending tiles. In this case, the pending tiles will
        // be lost, causing a full invalidation of the entire screen. To
        // avoid this, if there are still pending tiles, include them in
        // the retained tiles passed to the next frame builder.
        retained_tiles.merge(self.pending_retained_tiles);

        self.globals
    }

    /// Compute the contribution (bounding rectangles, and resources) of layers and their
    /// primitives in screen space.
    fn build_layer_screen_rects_and_cull_layers(
        &mut self,
        screen_world_rect: WorldRect,
        clip_scroll_tree: &ClipScrollTree,
        pipelines: &FastHashMap<PipelineId, Arc<ScenePipeline>>,
        resource_cache: &mut ResourceCache,
        gpu_cache: &mut GpuCache,
        render_tasks: &mut RenderTaskTree,
        profile_counters: &mut FrameProfileCounters,
        global_device_pixel_scale: DevicePixelScale,
        scene_properties: &SceneProperties,
        transform_palette: &mut TransformPalette,
        data_stores: &mut DataStores,
        surfaces: &mut Vec<SurfaceInfo>,
        scratch: &mut PrimitiveScratchBuffer,
        debug_flags: DebugFlags,
    ) -> Option<RenderTaskId> {
        profile_scope!("cull");

        if self.prim_store.pictures.is_empty() {
            return None
        }

        scratch.begin_frame();

        let root_spatial_node_index = clip_scroll_tree.root_reference_frame_index();

        const MAX_CLIP_COORD: f32 = 1.0e9;

        let frame_context = FrameBuildingContext {
            global_device_pixel_scale,
            scene_properties,
            pipelines,
            screen_world_rect,
            clip_scroll_tree,
            max_local_clip: LayoutRect::new(
                LayoutPoint::new(-MAX_CLIP_COORD, -MAX_CLIP_COORD),
                LayoutSize::new(2.0 * MAX_CLIP_COORD, 2.0 * MAX_CLIP_COORD),
            ),
            debug_flags,
            fb_config: &self.config,
        };

        // Construct a dummy root surface, that represents the
        // main framebuffer surface.
        let root_surface = SurfaceInfo::new(
            ROOT_SPATIAL_NODE_INDEX,
            ROOT_SPATIAL_NODE_INDEX,
            0.0,
            screen_world_rect,
            clip_scroll_tree,
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
            self.root_pic_index,
            &mut self.prim_store.pictures,
            &frame_context,
            gpu_cache,
            &self.clip_store,
            &data_stores.clip,
        );

        {
            profile_marker!("UpdateVisibility");

            let visibility_context = FrameVisibilityContext {
                global_device_pixel_scale,
                clip_scroll_tree,
                screen_world_rect,
                surfaces,
                debug_flags,
                scene_properties,
                config: &self.config,
            };

            let mut visibility_state = FrameVisibilityState {
                resource_cache,
                gpu_cache,
                clip_store: &mut self.clip_store,
                scratch,
                tile_cache: None,
                retained_tiles: &mut retained_tiles,
                data_stores,
                clip_chain_stack: ClipChainStack::new(),
            };

            self.prim_store.update_visibility(
                self.root_pic_index,
                ROOT_SURFACE_INDEX,
                &visibility_context,
                &mut visibility_state,
            );
        }

        let mut frame_state = FrameBuildingState {
            render_tasks,
            profile_counters,
            clip_store: &mut self.clip_store,
            resource_cache,
            gpu_cache,
            transforms: transform_palette,
            segment_builder: SegmentBuilder::new(),
            surfaces,
            dirty_region_stack: Vec::new(),
            clip_chain_stack: ClipChainStack::new(),
        };

        // Push a default dirty region which culls primitives
        // against the screen world rect, in absence of any
        // other dirty regions.
        let mut default_dirty_region = DirtyRegion::new();
        default_dirty_region.push(
            frame_context.screen_world_rect,
        );
        frame_state.push_dirty_region(default_dirty_region);

        let (pic_context, mut pic_state, mut prim_list) = self
            .prim_store
            .pictures[self.root_pic_index.0]
            .take_context(
                self.root_pic_index,
                root_spatial_node_index,
                root_spatial_node_index,
                ROOT_SURFACE_INDEX,
                true,
                &mut frame_state,
                &frame_context,
            )
            .unwrap();

        {
            profile_marker!("PreparePrims");

            self.prim_store.prepare_primitives(
                &mut prim_list,
                &pic_context,
                &mut pic_state,
                &frame_context,
                &mut frame_state,
                data_stores,
                scratch,
            );
        }

        let pic = &mut self.prim_store.pictures[self.root_pic_index.0];
        pic.restore_context(
            prim_list,
            pic_context,
            pic_state,
            &mut frame_state,
        );

        frame_state.pop_dirty_region();

        let child_tasks = frame_state
            .surfaces[ROOT_SURFACE_INDEX.0]
            .take_render_tasks();

        let root_render_task = RenderTask::new_picture(
            RenderTaskLocation::Fixed(self.output_rect),
            self.output_rect.size.to_f32(),
            self.root_pic_index,
            DeviceIntPoint::zero(),
            child_tasks,
            UvRectKind::Rect,
            root_spatial_node_index,
            global_device_pixel_scale,
        );

        let render_task_id = frame_state.render_tasks.add(root_render_task);
        frame_state
            .surfaces
            .first_mut()
            .unwrap()
            .surface = Some(PictureSurface::RenderTask(render_task_id));
        Some(render_task_id)
    }

    pub fn build(
        &mut self,
        resource_cache: &mut ResourceCache,
        gpu_cache: &mut GpuCache,
        stamp: FrameStamp,
        clip_scroll_tree: &mut ClipScrollTree,
        pipelines: &FastHashMap<PipelineId, Arc<ScenePipeline>>,
        global_device_pixel_scale: DevicePixelScale,
        layer: DocumentLayer,
        framebuffer_origin: FramebufferIntPoint,
        pan: WorldPoint,
        texture_cache_profile: &mut TextureCacheProfileCounters,
        gpu_cache_profile: &mut GpuCacheProfileCounters,
        scene_properties: &SceneProperties,
        data_stores: &mut DataStores,
        scratch: &mut PrimitiveScratchBuffer,
        render_task_counters: &mut RenderTaskTreeCounters,
        debug_flags: DebugFlags,
    ) -> Frame {
        profile_scope!("build");
        profile_marker!("BuildFrame");

        let mut profile_counters = FrameProfileCounters::new();
        profile_counters
            .total_primitives
            .set(self.prim_store.prim_count());

        resource_cache.begin_frame(stamp);
        gpu_cache.begin_frame(stamp);

        self.globals.update(gpu_cache);

        let mut transform_palette = TransformPalette::new();
        clip_scroll_tree.update_tree(
            pan,
            scene_properties,
            Some(&mut transform_palette),
        );
        self.clip_store.clear_old_instances();

        let mut render_tasks = RenderTaskTree::new(
            stamp.frame_id(),
            render_task_counters,
        );
        let mut surfaces = Vec::new();

        let output_size = self.output_rect.size.to_i32();
        let screen_world_rect = (self.output_rect.to_f32() / global_device_pixel_scale).round_out();

        let main_render_task_id = self.build_layer_screen_rects_and_cull_layers(
            screen_world_rect,
            clip_scroll_tree,
            pipelines,
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
        );

        {
            profile_marker!("BlockOnResources");

            resource_cache.block_until_all_resources_added(gpu_cache,
                                                           &mut render_tasks,
                                                           texture_cache_profile);
        }

        let mut passes = vec![];
        let mut deferred_resolves = vec![];
        let mut has_texture_cache_tasks = false;
        let mut prim_headers = PrimitiveHeaders::new();

        {
            profile_marker!("Batching");

            // Add passes as required for our cached render tasks.
            if !render_tasks.cacheable_render_tasks.is_empty() {
                passes.push(RenderPass::new_off_screen(output_size, self.config.gpu_supports_fast_clears));
                for cacheable_render_task in &render_tasks.cacheable_render_tasks {
                    render_tasks.assign_to_passes(
                        *cacheable_render_task,
                        0,
                        output_size,
                        &mut passes,
                        self.config.gpu_supports_fast_clears,
                    );
                }
                passes.reverse();
            }

            if let Some(main_render_task_id) = main_render_task_id {
                let passes_start = passes.len();
                passes.push(RenderPass::new_main_framebuffer(output_size, self.config.gpu_supports_fast_clears));
                render_tasks.assign_to_passes(
                    main_render_task_id,
                    passes_start,
                    output_size,
                    &mut passes,
                    self.config.gpu_supports_fast_clears,
                );
                passes[passes_start..].reverse();
            }

            // Used to generated a unique z-buffer value per primitive.
            let mut z_generator = ZBufferIdGenerator::new();
            let use_dual_source_blending = self.config.dual_source_blending_is_enabled &&
                                           self.config.dual_source_blending_is_supported;

            for pass in &mut passes {
                let mut ctx = RenderTargetContext {
                    global_device_pixel_scale,
                    prim_store: &self.prim_store,
                    resource_cache,
                    use_dual_source_blending,
                    clip_scroll_tree,
                    data_stores,
                    surfaces: &surfaces,
                    scratch,
                    screen_world_rect,
                    globals: &self.globals,
                };

                pass.build(
                    &mut ctx,
                    gpu_cache,
                    &mut render_tasks,
                    &mut deferred_resolves,
                    &self.clip_store,
                    &mut transform_palette,
                    &mut prim_headers,
                    &mut z_generator,
                );

                match pass.kind {
                    RenderPassKind::MainFramebuffer(ref color) => {
                        has_texture_cache_tasks |= color.must_be_drawn();
                    }
                    RenderPassKind::OffScreen { ref texture_cache, ref color, .. } => {
                        has_texture_cache_tasks |= !texture_cache.is_empty();
                        has_texture_cache_tasks |= color.must_be_drawn();
                    }
                }
            }
        }

        let gpu_cache_frame_id = gpu_cache.end_frame(gpu_cache_profile).frame_id();

        render_tasks.write_task_data();
        *render_task_counters = render_tasks.counters();
        resource_cache.end_frame(texture_cache_profile);

        Frame {
            content_origin: self.output_rect.origin,
            framebuffer_rect: FramebufferIntRect::new(
                framebuffer_origin,
                FramebufferIntSize::from_untyped(&self.output_rect.size.to_untyped()),
            ),
            background_color: self.background_color,
            layer,
            profile_counters,
            passes,
            transform_palette: transform_palette.transforms,
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

    pub fn create_hit_tester(
        &mut self,
        clip_scroll_tree: &ClipScrollTree,
        clip_data_store: &ClipDataStore,
    ) -> HitTester {
        HitTester::new(
            Arc::clone(&self.hit_testing_scene),
            clip_scroll_tree,
            &self.clip_store,
            clip_data_store,
        )
    }
}
