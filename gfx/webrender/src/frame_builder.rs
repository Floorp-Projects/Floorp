/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{BuiltDisplayList, ColorF, DeviceIntPoint, DeviceIntRect, DevicePixelScale};
use api::{DeviceUintPoint, DeviceUintRect, DeviceUintSize, DocumentLayer, FontRenderMode};
use api::{LayoutRect, LayoutSize, PipelineId, WorldPoint};
use clip::{ClipChain, ClipStore};
use clip_scroll_node::{ClipScrollNode};
use clip_scroll_tree::{ClipScrollNodeIndex, ClipScrollTree};
use display_list_flattener::{DisplayListFlattener};
use gpu_cache::GpuCache;
use gpu_types::{ClipChainRectIndex, ClipScrollNodeData, UvRectKind};
use hit_test::{HitTester, HitTestingRun};
use internal_types::{FastHashMap};
use picture::PictureSurface;
use prim_store::{CachedGradient, PrimitiveIndex, PrimitiveRun, PrimitiveStore};
use profiler::{FrameProfileCounters, GpuCacheProfileCounters, TextureCacheProfileCounters};
use render_backend::FrameId;
use render_task::{RenderTask, RenderTaskId, RenderTaskLocation, RenderTaskTree};
use resource_cache::{ResourceCache};
use scene::{ScenePipeline, SceneProperties};
use std::{mem, f32};
use std::sync::Arc;
use tiling::{Frame, RenderPass, RenderPassKind, RenderTargetContext};
use tiling::{ScrollbarPrimitive, SpecialRenderPasses};
use util::{self, MaxRect, WorldToLayoutFastTransform};

#[derive(Clone, Copy)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct FrameBuilderConfig {
    pub enable_scrollbars: bool,
    pub default_font_render_mode: FontRenderMode,
    pub dual_source_blending_is_supported: bool,
    pub dual_source_blending_is_enabled: bool,
}

/// A builder structure for `tiling::Frame`
pub struct FrameBuilder {
    screen_rect: DeviceUintRect,
    background_color: Option<ColorF>,
    window_size: DeviceUintSize,
    pub prim_store: PrimitiveStore,
    pub clip_store: ClipStore,
    pub hit_testing_runs: Vec<HitTestingRun>,
    pub config: FrameBuilderConfig,
    pub cached_gradients: Vec<CachedGradient>,
    pub scrollbar_prims: Vec<ScrollbarPrimitive>,
}

pub struct FrameBuildingContext<'a> {
    pub device_pixel_scale: DevicePixelScale,
    pub scene_properties: &'a SceneProperties,
    pub pipelines: &'a FastHashMap<PipelineId, Arc<ScenePipeline>>,
    pub screen_rect: DeviceIntRect,
    pub clip_scroll_tree: &'a ClipScrollTree,
    pub node_data: &'a [ClipScrollNodeData],
}

pub struct FrameBuildingState<'a> {
    pub render_tasks: &'a mut RenderTaskTree,
    pub profile_counters: &'a mut FrameProfileCounters,
    pub clip_store: &'a mut ClipStore,
    pub local_clip_rects: &'a mut Vec<LayoutRect>,
    pub resource_cache: &'a mut ResourceCache,
    pub gpu_cache: &'a mut GpuCache,
    pub cached_gradients: &'a mut [CachedGradient],
    pub special_render_passes: &'a mut SpecialRenderPasses,
}

pub struct PictureContext<'a> {
    pub pipeline_id: PipelineId,
    pub prim_runs: Vec<PrimitiveRun>,
    pub original_reference_frame_index: Option<ClipScrollNodeIndex>,
    pub display_list: &'a BuiltDisplayList,
    pub inv_world_transform: Option<WorldToLayoutFastTransform>,
    pub apply_local_clip_rect: bool,
    pub inflation_factor: f32,
    pub allow_subpixel_aa: bool,
}

pub struct PictureState {
    pub tasks: Vec<RenderTaskId>,
    pub has_non_root_coord_system: bool,
}

impl PictureState {
    pub fn new() -> PictureState {
        PictureState {
            tasks: Vec::new(),
            has_non_root_coord_system: false,
        }
    }
}

pub struct PrimitiveRunContext<'a> {
    pub clip_chain: &'a ClipChain,
    pub scroll_node: &'a ClipScrollNode,
    pub clip_chain_rect_index: ClipChainRectIndex,
}

impl<'a> PrimitiveRunContext<'a> {
    pub fn new(
        clip_chain: &'a ClipChain,
        scroll_node: &'a ClipScrollNode,
        clip_chain_rect_index: ClipChainRectIndex,
    ) -> Self {
        PrimitiveRunContext {
            clip_chain,
            scroll_node,
            clip_chain_rect_index,
        }
    }
}

impl FrameBuilder {
    pub fn empty() -> Self {
        FrameBuilder {
            hit_testing_runs: Vec::new(),
            cached_gradients: Vec::new(),
            scrollbar_prims: Vec::new(),
            prim_store: PrimitiveStore::new(),
            clip_store: ClipStore::new(),
            screen_rect: DeviceUintRect::zero(),
            window_size: DeviceUintSize::zero(),
            background_color: None,
            config: FrameBuilderConfig {
                enable_scrollbars: false,
                default_font_render_mode: FontRenderMode::Mono,
                dual_source_blending_is_enabled: true,
                dual_source_blending_is_supported: false,
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
            cached_gradients: flattener.cached_gradients,
            scrollbar_prims: flattener.scrollbar_prims,
            prim_store: flattener.prim_store,
            clip_store: flattener.clip_store,
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
        local_clip_rects: &mut Vec<LayoutRect>,
        node_data: &[ClipScrollNodeData],
    ) -> Option<RenderTaskId> {
        profile_scope!("cull");

        if self.prim_store.pictures.is_empty() {
            return None
        }

        // The root picture is always the first one added.
        let root_clip_scroll_node =
            &clip_scroll_tree.nodes[clip_scroll_tree.root_reference_frame_index().0];

        let display_list = &pipelines
            .get(&root_clip_scroll_node.pipeline_id)
            .expect("No display list?")
            .display_list;

        let frame_context = FrameBuildingContext {
            device_pixel_scale,
            scene_properties,
            pipelines,
            screen_rect: self.screen_rect.to_i32(),
            clip_scroll_tree,
            node_data,
        };

        let mut frame_state = FrameBuildingState {
            render_tasks,
            profile_counters,
            clip_store: &mut self.clip_store,
            local_clip_rects,
            resource_cache,
            gpu_cache,
            special_render_passes,
            cached_gradients: &mut self.cached_gradients,
        };

        let pic_context = PictureContext {
            pipeline_id: root_clip_scroll_node.pipeline_id,
            prim_runs: mem::replace(&mut self.prim_store.pictures[0].runs, Vec::new()),
            original_reference_frame_index: None,
            display_list,
            inv_world_transform: None,
            apply_local_clip_rect: true,
            inflation_factor: 0.0,
            allow_subpixel_aa: true,
        };

        let mut pic_state = PictureState::new();

        self.prim_store.reset_prim_visibility();
        self.prim_store.prepare_prim_runs(
            &pic_context,
            &mut pic_state,
            &frame_context,
            &mut frame_state,
        );

        let pic = &mut self.prim_store.pictures[0];
        pic.runs = pic_context.prim_runs;

        let root_render_task = RenderTask::new_picture(
            RenderTaskLocation::Fixed(frame_context.screen_rect),
            PrimitiveIndex(0),
            DeviceIntPoint::zero(),
            pic_state.tasks,
            UvRectKind::Rect,
        );

        let render_task_id = frame_state.render_tasks.add(root_render_task);
        pic.surface = Some(PictureSurface::RenderTask(render_task_id));
        Some(render_task_id)
    }

    fn update_scroll_bars(&mut self, clip_scroll_tree: &ClipScrollTree, gpu_cache: &mut GpuCache) {
        static SCROLLBAR_PADDING: f32 = 8.0;

        for scrollbar_prim in &self.scrollbar_prims {
            let metadata = &mut self.prim_store.cpu_metadata[scrollbar_prim.prim_index.0];
            let scroll_frame = &clip_scroll_tree.nodes[scrollbar_prim.scroll_frame_index.0];

            // Invalidate what's in the cache so it will get rebuilt.
            gpu_cache.invalidate(&metadata.gpu_location);

            let scrollable_distance = scroll_frame.scrollable_size().height;
            if scrollable_distance <= 0.0 {
                metadata.local_clip_rect.size = LayoutSize::zero();
                continue;
            }
            let amount_scrolled = -scroll_frame.scroll_offset().y / scrollable_distance;

            let frame_rect = scrollbar_prim.frame_rect;
            let min_y = frame_rect.origin.y + SCROLLBAR_PADDING;
            let max_y = frame_rect.origin.y + frame_rect.size.height -
                (SCROLLBAR_PADDING + metadata.local_rect.size.height);

            metadata.local_rect.origin.x = frame_rect.origin.x + frame_rect.size.width -
                (metadata.local_rect.size.width + SCROLLBAR_PADDING);
            metadata.local_rect.origin.y = util::lerp(min_y, max_y, amount_scrolled);
            metadata.local_clip_rect = metadata.local_rect;
        }
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
        gpu_cache.begin_frame();

        let mut node_data = Vec::with_capacity(clip_scroll_tree.nodes.len());
        let total_prim_runs =
            self.prim_store.pictures.iter().fold(1, |count, pic| count + pic.runs.len());
        let mut clip_chain_local_clip_rects = Vec::with_capacity(total_prim_runs);
        clip_chain_local_clip_rects.push(LayoutRect::max_rect());

        clip_scroll_tree.update_tree(
            &self.screen_rect.to_i32(),
            device_pixel_scale,
            &mut self.clip_store,
            resource_cache,
            gpu_cache,
            pan,
            &mut node_data,
            scene_properties,
        );

        self.update_scroll_bars(clip_scroll_tree, gpu_cache);

        let mut render_tasks = RenderTaskTree::new(frame_id);

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
            &mut clip_chain_local_clip_rects,
            &node_data,
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
        let use_dual_source_blending = self.config.dual_source_blending_is_enabled &&
                                       self.config.dual_source_blending_is_supported;

        for pass in &mut passes {
            let mut ctx = RenderTargetContext {
                device_pixel_scale,
                prim_store: &self.prim_store,
                resource_cache,
                clip_scroll_tree,
                use_dual_source_blending,
                node_data: &node_data,
                cached_gradients: &self.cached_gradients,
            };

            pass.build(
                &mut ctx,
                gpu_cache,
                &mut render_tasks,
                &mut deferred_resolves,
                &self.clip_store,
            );

            if let RenderPassKind::OffScreen { ref texture_cache, .. } = pass.kind {
                has_texture_cache_tasks |= !texture_cache.is_empty();
            }
        }

        let gpu_cache_frame_id = gpu_cache.end_frame(gpu_cache_profile);

        render_tasks.write_task_data();

        resource_cache.end_frame();

        Frame {
            window_size: self.window_size,
            inner_rect: self.screen_rect,
            device_pixel_ratio: device_pixel_scale.0,
            background_color: self.background_color,
            layer,
            profile_counters,
            passes,
            node_data,
            clip_chain_local_clip_rects,
            render_tasks,
            deferred_resolves,
            gpu_cache_frame_id,
            has_been_rendered: false,
            has_texture_cache_tasks,
        }
    }

    pub fn create_hit_tester(&mut self, clip_scroll_tree: &ClipScrollTree) -> HitTester {
        HitTester::new(
            &self.hit_testing_runs,
            clip_scroll_tree,
            &self.clip_store
        )
    }
}

