/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{ColorF, DeviceIntPoint, DevicePixelScale, LayoutPixel, PicturePixel, RasterPixel};
use api::{DeviceIntRect, DeviceIntSize, DocumentLayer, FontRenderMode};
use api::{LayoutPoint, LayoutRect, LayoutSize, PipelineId, RasterSpace, WorldPoint, WorldRect, WorldPixel};
use clip::{ClipDataStore, ClipStore};
use clip_scroll_tree::{ClipScrollTree, ROOT_SPATIAL_NODE_INDEX, SpatialNodeIndex};
use display_list_flattener::{DisplayListFlattener};
use gpu_cache::GpuCache;
use gpu_types::{PrimitiveHeaders, TransformPalette, UvRectKind, ZBufferIdGenerator};
use hit_test::{HitTester, HitTestingRun};
use internal_types::{FastHashMap, PlaneSplitter};
use picture::{PictureSurface, PictureUpdateState, SurfaceInfo, ROOT_SURFACE_INDEX, SurfaceIndex};
use picture::{TileCacheUpdateState, RetainedTiles};
use prim_store::{PrimitiveStore, SpaceMapper, PictureIndex, PrimitiveDebugId, PrimitiveScratchBuffer};
#[cfg(feature = "replay")]
use prim_store::{PrimitiveStoreStats};
use profiler::{FrameProfileCounters, GpuCacheProfileCounters, TextureCacheProfileCounters};
use render_backend::{FrameResources, FrameStamp};
use render_task::{RenderTask, RenderTaskId, RenderTaskLocation, RenderTaskTree};
use resource_cache::{ResourceCache};
use scene::{ScenePipeline, SceneProperties};
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
}

/// A builder structure for `tiling::Frame`
pub struct FrameBuilder {
    screen_rect: DeviceIntRect,
    background_color: Option<ColorF>,
    window_size: DeviceIntSize,
    root_pic_index: PictureIndex,
    /// Cache of surface tiles from the previous frame builder
    /// that can optionally be consumed by this frame builder.
    pending_retained_tiles: RetainedTiles,
    pub prim_store: PrimitiveStore,
    pub clip_store: ClipStore,
    pub hit_testing_runs: Vec<HitTestingRun>,
    pub config: FrameBuilderConfig,
}

pub struct FrameBuildingContext<'a> {
    pub device_pixel_scale: DevicePixelScale,
    pub scene_properties: &'a SceneProperties,
    pub pipelines: &'a FastHashMap<PipelineId, Arc<ScenePipeline>>,
    pub screen_world_rect: WorldRect,
    pub clip_scroll_tree: &'a ClipScrollTree,
    pub max_local_clip: LayoutRect,
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
}

/// Immutable context of a picture when processing children.
#[derive(Debug)]
pub struct PictureContext {
    pub pic_index: PictureIndex,
    pub pipeline_id: PipelineId,
    pub apply_local_clip_rect: bool,
    pub allow_subpixel_aa: bool,
    pub is_passthrough: bool,
    pub raster_space: RasterSpace,
    pub surface_spatial_node_index: SpatialNodeIndex,
    pub raster_spatial_node_index: SpatialNodeIndex,
    /// The surface that this picture will render on.
    pub surface_index: SurfaceIndex,
    pub dirty_world_rect: WorldRect,
}

/// Mutable state of a picture that gets modified when
/// the children are processed.
pub struct PictureState {
    pub is_cacheable: bool,
    pub map_local_to_pic: SpaceMapper<LayoutPixel, PicturePixel>,
    pub map_pic_to_world: SpaceMapper<PicturePixel, WorldPixel>,
    pub map_pic_to_raster: SpaceMapper<PicturePixel, RasterPixel>,
    pub map_raster_to_world: SpaceMapper<RasterPixel, WorldPixel>,
    /// If the plane splitter, the primitives get added to it insted of
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
            hit_testing_runs: Vec::new(),
            prim_store: PrimitiveStore::new(&PrimitiveStoreStats::empty()),
            clip_store: ClipStore::new(),
            screen_rect: DeviceIntRect::zero(),
            window_size: DeviceIntSize::zero(),
            background_color: None,
            root_pic_index: PictureIndex(0),
            pending_retained_tiles: RetainedTiles::new(),
            config: FrameBuilderConfig {
                default_font_render_mode: FontRenderMode::Mono,
                dual_source_blending_is_enabled: true,
                dual_source_blending_is_supported: false,
                chase_primitive: ChasePrimitive::Nothing,
                enable_picture_caching: false,
            },
        }
    }

    /// Provide any cached surface tiles from the previous frame builder
    /// to a new frame builder. These will be consumed or dropped the
    /// first time a new frame builder creates a frame.
    pub fn set_retained_tiles(
        &mut self,
        retained_tiles: RetainedTiles,
    ) {
        debug_assert!(self.pending_retained_tiles.tiles.is_empty());
        self.pending_retained_tiles = retained_tiles;
    }

    pub fn with_display_list_flattener(
        screen_rect: DeviceIntRect,
        background_color: Option<ColorF>,
        window_size: DeviceIntSize,
        flattener: DisplayListFlattener,
    ) -> Self {
        FrameBuilder {
            hit_testing_runs: flattener.hit_testing_runs,
            prim_store: flattener.prim_store,
            clip_store: flattener.clip_store,
            root_pic_index: flattener.root_pic_index,
            screen_rect,
            background_color,
            window_size,
            pending_retained_tiles: RetainedTiles::new(),
            config: flattener.config,
        }
    }

    /// Destroy an existing frame builder. This is called just before
    /// a frame builder is replaced with a newly built scene.
    pub fn destroy(
        self,
        retained_tiles: &mut RetainedTiles,
    ) {
        self.prim_store.destroy(
            retained_tiles,
        );
    }

    /// Compute the contribution (bounding rectangles, and resources) of layers and their
    /// primitives in screen space.
    fn build_layer_screen_rects_and_cull_layers(
        &mut self,
        clip_scroll_tree: &ClipScrollTree,
        pipelines: &FastHashMap<PipelineId, Arc<ScenePipeline>>,
        resource_cache: &mut ResourceCache,
        gpu_cache: &mut GpuCache,
        render_tasks: &mut RenderTaskTree,
        profile_counters: &mut FrameProfileCounters,
        device_pixel_scale: DevicePixelScale,
        scene_properties: &SceneProperties,
        transform_palette: &mut TransformPalette,
        resources: &mut FrameResources,
        surfaces: &mut Vec<SurfaceInfo>,
        scratch: &mut PrimitiveScratchBuffer,
    ) -> Option<RenderTaskId> {
        profile_scope!("cull");

        if self.prim_store.pictures.is_empty() {
            return None
        }

        scratch.begin_frame();

        let root_spatial_node_index = clip_scroll_tree.root_reference_frame_index();

        const MAX_CLIP_COORD: f32 = 1.0e9;

        let screen_world_rect = (self.screen_rect.to_f32() / device_pixel_scale).round_out();

        let frame_context = FrameBuildingContext {
            device_pixel_scale,
            scene_properties,
            pipelines,
            screen_world_rect,
            clip_scroll_tree,
            max_local_clip: LayoutRect::new(
                LayoutPoint::new(-MAX_CLIP_COORD, -MAX_CLIP_COORD),
                LayoutSize::new(2.0 * MAX_CLIP_COORD, 2.0 * MAX_CLIP_COORD),
            ),
        };

        // Construct a dummy root surface, that represents the
        // main framebuffer surface.
        let root_surface = SurfaceInfo::new(
            ROOT_SPATIAL_NODE_INDEX,
            ROOT_SPATIAL_NODE_INDEX,
            0.0,
            screen_world_rect,
            clip_scroll_tree,
        );
        surfaces.push(root_surface);

        let mut pic_update_state = PictureUpdateState::new(surfaces);
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
        self.prim_store.update_picture(
            self.root_pic_index,
            &mut pic_update_state,
            &frame_context,
            gpu_cache,
            resources,
            &self.clip_store,
        );

        // Update the state of any picture tile caches. This is a no-op on most
        // frames (it only does work the first time a new scene is built, or if
        // the tile-relative transform dependencies have changed).
        let mut tile_cache_state = TileCacheUpdateState::new();
        self.prim_store.update_tile_cache(
            self.root_pic_index,
            &mut tile_cache_state,
            &frame_context,
            resource_cache,
            resources,
            &self.clip_store,
            &pic_update_state.surfaces,
            gpu_cache,
            &mut retained_tiles,
        );

        let mut frame_state = FrameBuildingState {
            render_tasks,
            profile_counters,
            clip_store: &mut self.clip_store,
            resource_cache,
            gpu_cache,
            transforms: transform_palette,
            segment_builder: SegmentBuilder::new(),
            surfaces: pic_update_state.surfaces,
        };

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
                screen_world_rect,
            )
            .unwrap();

        self.prim_store.prepare_primitives(
            &mut prim_list,
            &pic_context,
            &mut pic_state,
            &frame_context,
            &mut frame_state,
            resources,
            scratch,
        );

        let pic = &mut self.prim_store.pictures[self.root_pic_index.0];
        pic.restore_context(
            prim_list,
            pic_context,
            pic_state,
            &mut frame_state,
        );

        let child_tasks = frame_state
            .surfaces[ROOT_SURFACE_INDEX.0]
            .take_render_tasks();

        let root_render_task = RenderTask::new_picture(
            RenderTaskLocation::Fixed(self.screen_rect.to_i32()),
            self.screen_rect.size.to_f32(),
            self.root_pic_index,
            DeviceIntPoint::zero(),
            child_tasks,
            UvRectKind::Rect,
            root_spatial_node_index,
            None,
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
        device_pixel_scale: DevicePixelScale,
        layer: DocumentLayer,
        pan: WorldPoint,
        texture_cache_profile: &mut TextureCacheProfileCounters,
        gpu_cache_profile: &mut GpuCacheProfileCounters,
        scene_properties: &SceneProperties,
        resources: &mut FrameResources,
        scratch: &mut PrimitiveScratchBuffer,
    ) -> Frame {
        profile_scope!("build");
        debug_assert!(
            DeviceIntRect::new(DeviceIntPoint::zero(), self.window_size)
                .contains_rect(&self.screen_rect)
        );

        let mut profile_counters = FrameProfileCounters::new();
        profile_counters
            .total_primitives
            .set(self.prim_store.prim_count());

        resource_cache.begin_frame(stamp);
        gpu_cache.begin_frame(stamp.frame_id());

        let mut transform_palette = TransformPalette::new();
        clip_scroll_tree.update_tree(
            pan,
            scene_properties,
            Some(&mut transform_palette),
        );
        self.clip_store.clear_old_instances();

        let mut render_tasks = RenderTaskTree::new(stamp.frame_id());
        let mut surfaces = Vec::new();

        let screen_size = self.screen_rect.size.to_i32();

        let main_render_task_id = self.build_layer_screen_rects_and_cull_layers(
            clip_scroll_tree,
            pipelines,
            resource_cache,
            gpu_cache,
            &mut render_tasks,
            &mut profile_counters,
            device_pixel_scale,
            scene_properties,
            &mut transform_palette,
            resources,
            &mut surfaces,
            scratch,
        );

        resource_cache.block_until_all_resources_added(gpu_cache,
                                                       &mut render_tasks,
                                                       texture_cache_profile);

        let mut passes = vec![];

        // Add passes as required for our cached render tasks.
        if !render_tasks.cacheable_render_tasks.is_empty() {
            passes.push(RenderPass::new_off_screen(screen_size));
            for cacheable_render_task in &render_tasks.cacheable_render_tasks {
                render_tasks.assign_to_passes(
                    *cacheable_render_task,
                    0,
                    screen_size,
                    &mut passes,
                );
            }
            passes.reverse();
        }

        if let Some(main_render_task_id) = main_render_task_id {
            let passes_start = passes.len();
            passes.push(RenderPass::new_main_framebuffer(screen_size));
            render_tasks.assign_to_passes(
                main_render_task_id,
                passes_start,
                screen_size,
                &mut passes,
            );
            passes[passes_start..].reverse();
        }


        let mut deferred_resolves = vec![];
        let mut has_texture_cache_tasks = false;
        let mut prim_headers = PrimitiveHeaders::new();
        // Used to generated a unique z-buffer value per primitive.
        let mut z_generator = ZBufferIdGenerator::new();
        let use_dual_source_blending = self.config.dual_source_blending_is_enabled &&
                                       self.config.dual_source_blending_is_supported;

        for pass in &mut passes {
            let mut ctx = RenderTargetContext {
                device_pixel_scale,
                prim_store: &self.prim_store,
                resource_cache,
                use_dual_source_blending,
                clip_scroll_tree,
                resources,
                surfaces: &surfaces,
                scratch,
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

        let gpu_cache_frame_id = gpu_cache.end_frame(gpu_cache_profile);

        render_tasks.write_task_data(device_pixel_scale);

        resource_cache.end_frame();

        Frame {
            window_size: self.window_size,
            inner_rect: self.screen_rect,
            device_pixel_ratio: device_pixel_scale.0,
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
        }
    }

    pub fn create_hit_tester(
        &mut self,
        clip_scroll_tree: &ClipScrollTree,
        clip_data_store: &ClipDataStore,
    ) -> HitTester {
        HitTester::new(
            &self.hit_testing_runs,
            clip_scroll_tree,
            &self.clip_store,
            clip_data_store,
        )
    }
}

