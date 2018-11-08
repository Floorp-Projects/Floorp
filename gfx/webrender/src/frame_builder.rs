/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{ColorF, DeviceIntPoint, DevicePixelScale, LayoutPixel, PicturePixel, RasterPixel};
use api::{DeviceUintPoint, DeviceUintRect, DeviceUintSize, DocumentLayer, FontRenderMode};
use api::{LayoutPoint, LayoutRect, LayoutSize, PipelineId, RasterSpace, WorldPoint, WorldRect, WorldPixel};
use clip::{ClipDataStore, ClipStore};
use clip_scroll_tree::{ClipScrollTree, ROOT_SPATIAL_NODE_INDEX, SpatialNodeIndex};
use display_list_flattener::{DisplayListFlattener};
use gpu_cache::GpuCache;
use gpu_types::{PrimitiveHeaders, TransformPalette, UvRectKind, ZBufferIdGenerator};
use hit_test::{HitTester, HitTestingRun};
use internal_types::{FastHashMap, PlaneSplitter};
use picture::{PictureSurface, PictureUpdateContext, SurfaceInfo, ROOT_SURFACE_INDEX, SurfaceIndex};
use prim_store::{PrimitiveStore, SpaceMapper, PictureIndex, PrimitiveDebugId};
use profiler::{FrameProfileCounters, GpuCacheProfileCounters, TextureCacheProfileCounters};
use render_backend::{FrameResources, FrameId};
use render_task::{RenderTask, RenderTaskId, RenderTaskLocation, RenderTaskTree};
use resource_cache::{ResourceCache};
use scene::{ScenePipeline, SceneProperties};
use segment::SegmentBuilder;
use spatial_node::SpatialNode;
use std::f32;
use std::sync::Arc;
use tiling::{Frame, RenderPass, RenderPassKind, RenderTargetContext};
use tiling::{SpecialRenderPasses};


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
}

/// A builder structure for `tiling::Frame`
pub struct FrameBuilder {
    screen_rect: DeviceUintRect,
    background_color: Option<ColorF>,
    window_size: DeviceUintSize,
    root_pic_index: PictureIndex,
    pub prim_store: PrimitiveStore,
    pub clip_store: ClipStore,
    pub hit_testing_runs: Vec<HitTestingRun>,
    pub config: FrameBuilderConfig,
}

pub struct FrameBuildingContext<'a> {
    pub device_pixel_scale: DevicePixelScale,
    pub scene_properties: &'a SceneProperties,
    pub pipelines: &'a FastHashMap<PipelineId, Arc<ScenePipeline>>,
    pub world_rect: WorldRect,
    pub clip_scroll_tree: &'a ClipScrollTree,
    pub max_local_clip: LayoutRect,
}

pub struct FrameBuildingState<'a> {
    pub render_tasks: &'a mut RenderTaskTree,
    pub profile_counters: &'a mut FrameProfileCounters,
    pub clip_store: &'a mut ClipStore,
    pub resource_cache: &'a mut ResourceCache,
    pub gpu_cache: &'a mut GpuCache,
    pub special_render_passes: &'a mut SpecialRenderPasses,
    pub transforms: &'a mut TransformPalette,
    pub resources: &'a mut FrameResources,
    pub segment_builder: SegmentBuilder,
    pub surfaces: &'a mut Vec<SurfaceInfo>,
}

/// Immutable context of a picture when processing children.
#[derive(Debug)]
pub struct PictureContext {
    pub pic_index: PictureIndex,
    pub pipeline_id: PipelineId,
    pub apply_local_clip_rect: bool,
    pub inflation_factor: f32,
    pub allow_subpixel_aa: bool,
    pub is_passthrough: bool,
    pub raster_space: RasterSpace,
    pub surface_spatial_node_index: SpatialNodeIndex,
    pub raster_spatial_node_index: SpatialNodeIndex,
    /// The surface that this picture will render on.
    pub surface_index: SurfaceIndex,
}

/// Mutable state of a picture that gets modified when
/// the children are processed.
pub struct PictureState {
    pub is_cacheable: bool,
    pub local_rect_changed: bool,
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
            prim_store: PrimitiveStore::new(),
            clip_store: ClipStore::new(),
            screen_rect: DeviceUintRect::zero(),
            window_size: DeviceUintSize::zero(),
            background_color: None,
            root_pic_index: PictureIndex(0),
            config: FrameBuilderConfig {
                default_font_render_mode: FontRenderMode::Mono,
                dual_source_blending_is_enabled: true,
                dual_source_blending_is_supported: false,
                chase_primitive: ChasePrimitive::Nothing,
            },
        }
    }

    pub fn with_display_list_flattener(
        screen_rect: DeviceUintRect,
        background_color: Option<ColorF>,
        window_size: DeviceUintSize,
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
            config: flattener.config,
        }
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
        special_render_passes: &mut SpecialRenderPasses,
        profile_counters: &mut FrameProfileCounters,
        device_pixel_scale: DevicePixelScale,
        scene_properties: &SceneProperties,
        transform_palette: &mut TransformPalette,
        resources: &mut FrameResources,
        surfaces: &mut Vec<SurfaceInfo>,
    ) -> Option<RenderTaskId> {
        profile_scope!("cull");

        if self.prim_store.pictures.is_empty() {
            return None
        }

        let root_spatial_node_index = clip_scroll_tree.root_reference_frame_index();

        const MAX_CLIP_COORD: f32 = 1.0e9;

        let world_rect = (self.screen_rect.to_f32() / device_pixel_scale).round_out();

        let frame_context = FrameBuildingContext {
            device_pixel_scale,
            scene_properties,
            pipelines,
            world_rect,
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
            world_rect,
            clip_scroll_tree,
        );
        surfaces.push(root_surface);

        let pic_update_context = PictureUpdateContext::new(
            ROOT_SURFACE_INDEX,
            ROOT_SPATIAL_NODE_INDEX,
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
            &pic_update_context,
            &frame_context,
            surfaces,
        );

        let mut frame_state = FrameBuildingState {
            render_tasks,
            profile_counters,
            clip_store: &mut self.clip_store,
            resource_cache,
            gpu_cache,
            special_render_passes,
            transforms: transform_palette,
            resources,
            segment_builder: SegmentBuilder::new(),
            surfaces,
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
            )
            .unwrap();

        self.prim_store.prepare_primitives(
            &mut prim_list,
            &pic_context,
            &mut pic_state,
            &frame_context,
            &mut frame_state,
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
        frame_id: FrameId,
        clip_scroll_tree: &mut ClipScrollTree,
        pipelines: &FastHashMap<PipelineId, Arc<ScenePipeline>>,
        device_pixel_scale: DevicePixelScale,
        layer: DocumentLayer,
        pan: WorldPoint,
        texture_cache_profile: &mut TextureCacheProfileCounters,
        gpu_cache_profile: &mut GpuCacheProfileCounters,
        scene_properties: &SceneProperties,
        resources: &mut FrameResources,
    ) -> Frame {
        profile_scope!("build");
        debug_assert!(
            DeviceUintRect::new(DeviceUintPoint::zero(), self.window_size)
                .contains_rect(&self.screen_rect)
        );

        let mut profile_counters = FrameProfileCounters::new();
        profile_counters
            .total_primitives
            .set(self.prim_store.prim_count());

        resource_cache.begin_frame(frame_id);
        gpu_cache.begin_frame(frame_id);

        let mut transform_palette = TransformPalette::new();
        clip_scroll_tree.update_tree(
            pan,
            scene_properties,
            Some(&mut transform_palette),
        );

        let mut render_tasks = RenderTaskTree::new(frame_id);
        let mut surfaces = Vec::new();

        let screen_size = self.screen_rect.size.to_i32();
        let mut special_render_passes = SpecialRenderPasses::new(&screen_size);

        let main_render_task_id = self.build_layer_screen_rects_and_cull_layers(
            clip_scroll_tree,
            pipelines,
            resource_cache,
            gpu_cache,
            &mut render_tasks,
            &mut special_render_passes,
            &mut profile_counters,
            device_pixel_scale,
            scene_properties,
            &mut transform_palette,
            resources,
            &mut surfaces,
        );

        resource_cache.block_until_all_resources_added(gpu_cache,
                                                       &mut render_tasks,
                                                       texture_cache_profile);

        let mut passes = vec![
            special_render_passes.alpha_glyph_pass,
            special_render_passes.color_glyph_pass,
        ];

        if let Some(main_render_task_id) = main_render_task_id {
            let mut required_pass_count = 0;
            render_tasks.max_depth(main_render_task_id, 0, &mut required_pass_count);
            assert_ne!(required_pass_count, 0);

            // Do the allocations now, assigning each tile's tasks to a render
            // pass and target as required.
            for _ in 0 .. required_pass_count - 1 {
                passes.push(RenderPass::new_off_screen(screen_size));
            }
            passes.push(RenderPass::new_main_framebuffer(screen_size));

            render_tasks.assign_to_passes(
                main_render_task_id,
                required_pass_count - 1,
                &mut passes[2..],
            );
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

            if let RenderPassKind::OffScreen { ref texture_cache, .. } = pass.kind {
                has_texture_cache_tasks |= !texture_cache.is_empty();
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

