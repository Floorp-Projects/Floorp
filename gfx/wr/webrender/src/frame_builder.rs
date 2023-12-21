/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{ColorF, DebugFlags, FontRenderMode, PremultipliedColorF, ExternalScrollId, MinimapData};
use api::units::*;
use plane_split::BspSplitter;
use crate::batch::{BatchBuilder, AlphaBatchBuilder, AlphaBatchContainer};
use crate::clip::{ClipStore, ClipTree};
use crate::command_buffer::{PrimitiveCommand, CommandBufferList, CommandBufferIndex};
use crate::debug_colors;
use crate::spatial_node::SpatialNodeType;
use crate::spatial_tree::{SpatialTree, SpatialNodeIndex};
use crate::composite::{CompositorKind, CompositeState, CompositeStatePreallocator};
use crate::debug_item::DebugItem;
use crate::gpu_cache::{GpuCache, GpuCacheHandle};
use crate::gpu_types::{PrimitiveHeaders, TransformPalette, ZBufferIdGenerator};
use crate::gpu_types::{QuadSegment, TransformData};
use crate::internal_types::{FastHashMap, PlaneSplitter, FrameId, FrameStamp};
use crate::picture::{DirtyRegion, SliceId, TileCacheInstance};
use crate::picture::{SurfaceInfo, SurfaceIndex};
use crate::picture::{SubpixelMode, RasterConfig, PictureCompositeMode};
use crate::prepare::{prepare_primitives};
use crate::prim_store::{PictureIndex, PrimitiveScratchBuffer};
use crate::prim_store::{DeferredResolve, PrimitiveInstance};
use crate::profiler::{self, TransactionProfile};
use crate::render_backend::{DataStores, ScratchBuffer};
use crate::renderer::{GpuBuffer, GpuBufferBuilder};
use crate::render_target::{RenderTarget, PictureCacheTarget, TextureCacheRenderTarget, PictureCacheTargetKind};
use crate::render_target::{RenderTargetContext, RenderTargetKind, AlphaRenderTarget, ColorRenderTarget};
use crate::render_task_graph::{RenderTaskGraph, Pass, SubPassSurface};
use crate::render_task_graph::{RenderPass, RenderTaskGraphBuilder};
use crate::render_task::{RenderTaskKind, StaticRenderTaskSurface};
use crate::resource_cache::{ResourceCache};
use crate::scene::{BuiltScene, SceneProperties};
use crate::space::SpaceMapper;
use crate::segment::SegmentBuilder;
use crate::surface::SurfaceBuilder;
use std::{f32, mem};
use crate::util::{VecHelper, Preallocator};
use crate::visibility::{update_prim_visibility, FrameVisibilityState, FrameVisibilityContext};

#[derive(Clone, Copy, Debug)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct FrameBuilderConfig {
    pub default_font_render_mode: FontRenderMode,
    pub dual_source_blending_is_supported: bool,
    /// True if we're running tests (i.e. via wrench).
    pub testing: bool,
    pub gpu_supports_fast_clears: bool,
    pub gpu_supports_advanced_blend: bool,
    pub advanced_blend_is_coherent: bool,
    pub gpu_supports_render_target_partial_update: bool,
    /// Whether ImageBufferKind::TextureExternal images must first be copied
    /// to a regular texture before rendering.
    pub external_images_require_copy: bool,
    pub batch_lookback_count: usize,
    pub background_color: Option<ColorF>,
    pub compositor_kind: CompositorKind,
    pub tile_size_override: Option<DeviceIntSize>,
    pub max_surface_override: Option<usize>,
    pub max_depth_ids: i32,
    pub max_target_size: i32,
    pub force_invalidation: bool,
    pub is_software: bool,
    pub low_quality_pinch_zoom: bool,
    pub max_shared_surface_size: i32,
}

/// A set of common / global resources that are retained between
/// new display lists, such that any GPU cache handles can be
/// persisted even when a new display list arrives.
#[cfg_attr(feature = "capture", derive(Serialize))]
pub struct FrameGlobalResources {
    /// The image shader block for the most common / default
    /// set of image parameters (color white, stretch == rect.size).
    pub default_image_handle: GpuCacheHandle,

    /// A GPU cache config for drawing cut-out rectangle primitives.
    /// This is used to 'cut out' overlay tiles where a compositor
    /// surface exists.
    pub default_black_rect_handle: GpuCacheHandle,
}

impl FrameGlobalResources {
    pub fn empty() -> Self {
        FrameGlobalResources {
            default_image_handle: GpuCacheHandle::new(),
            default_black_rect_handle: GpuCacheHandle::new(),
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

        if let Some(mut request) = gpu_cache.request(&mut self.default_black_rect_handle) {
            request.push(PremultipliedColorF::BLACK);
        }
    }
}

pub struct FrameScratchBuffer {
    dirty_region_stack: Vec<DirtyRegion>,
    surface_stack: Vec<(PictureIndex, SurfaceIndex)>,
}

impl Default for FrameScratchBuffer {
    fn default() -> Self {
        FrameScratchBuffer {
            dirty_region_stack: Vec::new(),
            surface_stack: Vec::new(),
        }
    }
}

impl FrameScratchBuffer {
    pub fn begin_frame(&mut self) {
        self.dirty_region_stack.clear();
        self.surface_stack.clear();
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
    #[cfg_attr(feature = "capture", serde(skip))]
    plane_splitters: Vec<PlaneSplitter>,
}

pub struct FrameBuildingContext<'a> {
    pub global_device_pixel_scale: DevicePixelScale,
    pub scene_properties: &'a SceneProperties,
    pub global_screen_world_rect: WorldRect,
    pub spatial_tree: &'a SpatialTree,
    pub max_local_clip: LayoutRect,
    pub debug_flags: DebugFlags,
    pub fb_config: &'a FrameBuilderConfig,
    pub root_spatial_node_index: SpatialNodeIndex,
}

pub struct FrameBuildingState<'a> {
    pub rg_builder: &'a mut RenderTaskGraphBuilder,
    pub clip_store: &'a mut ClipStore,
    pub resource_cache: &'a mut ResourceCache,
    pub gpu_cache: &'a mut GpuCache,
    pub transforms: &'a mut TransformPalette,
    pub segment_builder: SegmentBuilder,
    pub surfaces: &'a mut Vec<SurfaceInfo>,
    pub dirty_region_stack: Vec<DirtyRegion>,
    pub composite_state: &'a mut CompositeState,
    pub num_visible_primitives: u32,
    pub plane_splitters: &'a mut [PlaneSplitter],
    pub surface_builder: SurfaceBuilder,
    pub cmd_buffers: &'a mut CommandBufferList,
    pub clip_tree: &'a ClipTree,
    pub frame_gpu_data: &'a mut GpuBufferBuilder,
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

    /// Push a primitive command to a set of command buffers
    pub fn push_prim(
        &mut self,
        cmd: &PrimitiveCommand,
        spatial_node_index: SpatialNodeIndex,
        targets: &[CommandBufferIndex],
    ) {
        for cmd_buffer_index in targets {
            let cmd_buffer = self.cmd_buffers.get_mut(*cmd_buffer_index);
            cmd_buffer.add_prim(cmd, spatial_node_index);
        }
    }

    /// Push a command to a set of command buffers
    pub fn push_cmd(
        &mut self,
        cmd: &PrimitiveCommand,
        targets: &[CommandBufferIndex],
    ) {
        for cmd_buffer_index in targets {
            let cmd_buffer = self.cmd_buffers.get_mut(*cmd_buffer_index);
            cmd_buffer.add_cmd(cmd);
        }
    }

    /// Set the active list of segments in a set of command buffers
    pub fn set_segments(
        &mut self,
        segments: &[QuadSegment],
        targets: &[CommandBufferIndex],
    ) {
        for cmd_buffer_index in targets {
            let cmd_buffer = self.cmd_buffers.get_mut(*cmd_buffer_index);
            cmd_buffer.set_segments(segments);
        }
    }
}

/// Immutable context of a picture when processing children.
#[derive(Debug)]
pub struct PictureContext {
    pub pic_index: PictureIndex,
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
}

impl FrameBuilder {
    pub fn new() -> Self {
        FrameBuilder {
            globals: FrameGlobalResources::empty(),
            prim_headers_prealloc: Preallocator::new(0),
            composite_state_prealloc: CompositeStatePreallocator::default(),
            plane_splitters: Vec::new(),
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
        rg_builder: &mut RenderTaskGraphBuilder,
        global_device_pixel_scale: DevicePixelScale,
        scene_properties: &SceneProperties,
        transform_palette: &mut TransformPalette,
        data_stores: &mut DataStores,
        scratch: &mut ScratchBuffer,
        debug_flags: DebugFlags,
        composite_state: &mut CompositeState,
        tile_caches: &mut FastHashMap<SliceId, Box<TileCacheInstance>>,
        spatial_tree: &SpatialTree,
        cmd_buffers: &mut CommandBufferList,
        frame_gpu_data: &mut GpuBufferBuilder,
        profile: &mut TransactionProfile,
    ) {
        profile_scope!("build_layer_screen_rects_and_cull_layers");

        let root_spatial_node_index = spatial_tree.root_reference_frame_index();

        const MAX_CLIP_COORD: f32 = 1.0e9;

        // Reset all plane splitters. These are retained from frame to frame to reduce
        // per-frame allocations
        self.plane_splitters.resize_with(scene.num_plane_splitters, BspSplitter::new);
        for splitter in &mut self.plane_splitters {
            splitter.reset();
        }

        let frame_context = FrameBuildingContext {
            global_device_pixel_scale,
            scene_properties,
            global_screen_world_rect,
            spatial_tree,
            max_local_clip: LayoutRect {
                min: LayoutPoint::new(-MAX_CLIP_COORD, -MAX_CLIP_COORD),
                max: LayoutPoint::new(MAX_CLIP_COORD, MAX_CLIP_COORD),
            },
            debug_flags,
            fb_config: &scene.config,
            root_spatial_node_index,
        };

        scene.picture_graph.build_update_passes(
            &mut scene.prim_store.pictures,
            &frame_context,
        );

        scene.picture_graph.assign_surfaces(
            &mut scene.prim_store.pictures,
            &mut scene.surfaces,
            tile_caches,
            &frame_context,
        );

        scene.picture_graph.propagate_bounding_rects(
            &mut scene.prim_store.pictures,
            &mut scene.surfaces,
            &frame_context,
        );

        {
            profile_scope!("UpdateVisibility");
            profile_marker!("UpdateVisibility");
            profile.start_time(profiler::FRAME_VISIBILITY_TIME);

            let visibility_context = FrameVisibilityContext {
                global_device_pixel_scale,
                spatial_tree,
                global_screen_world_rect,
                debug_flags,
                scene_properties,
                config: scene.config,
                root_spatial_node_index,
            };

            for pic_index in scene.tile_cache_pictures.iter().rev() {
                let pic = &mut scene.prim_store.pictures[pic_index.0];

                match pic.raster_config {
                    Some(RasterConfig { surface_index, composite_mode: PictureCompositeMode::TileCache { slice_id }, .. }) => {
                        let tile_cache = tile_caches
                            .get_mut(&slice_id)
                            .expect("bug: non-existent tile cache");

                        let mut visibility_state = FrameVisibilityState {
                            surface_stack: scratch.frame.surface_stack.take(),
                            resource_cache,
                            gpu_cache,
                            clip_store: &mut scene.clip_store,
                            scratch,
                            data_stores,
                            composite_state,
                            clip_tree: &mut scene.clip_tree,
                            rg_builder,
                        };

                        // If we have a tile cache for this picture, see if any of the
                        // relative transforms have changed, which means we need to
                        // re-map the dependencies of any child primitives.
                        let surface = &scene.surfaces[surface_index.0];
                        let world_culling_rect = tile_cache.pre_update(
                            surface.unclipped_local_rect,
                            surface_index,
                            &visibility_context,
                            &mut visibility_state,
                        );

                        // Push a new surface, supplying the list of clips that should be
                        // ignored, since they are handled by clipping when drawing this surface.
                        visibility_state.push_surface(
                            *pic_index,
                            surface_index,
                        );
                        visibility_state.clip_tree.push_clip_root_node(tile_cache.shared_clip_node_id);

                        update_prim_visibility(
                            *pic_index,
                            None,
                            &world_culling_rect,
                            &mut scene.prim_store,
                            &mut scene.prim_instances,
                            &mut scene.surfaces,
                            true,
                            &visibility_context,
                            &mut visibility_state,
                            tile_cache,
                        );

                        // Build the dirty region(s) for this tile cache.
                        tile_cache.post_update(
                            &visibility_context,
                            &mut visibility_state,
                        );

                        visibility_state.clip_tree.pop_clip_root();
                        visibility_state.pop_surface();
                        visibility_state.scratch.frame.surface_stack = visibility_state.surface_stack.take();
                    }
                    _ => {
                        panic!("bug: not a tile cache");
                    }
                }
            }

            profile.end_time(profiler::FRAME_VISIBILITY_TIME);
        }

        profile.start_time(profiler::FRAME_PREPARE_TIME);

        let mut frame_state = FrameBuildingState {
            rg_builder,
            clip_store: &mut scene.clip_store,
            resource_cache,
            gpu_cache,
            transforms: transform_palette,
            segment_builder: SegmentBuilder::new(),
            surfaces: &mut scene.surfaces,
            dirty_region_stack: scratch.frame.dirty_region_stack.take(),
            composite_state,
            num_visible_primitives: 0,
            plane_splitters: &mut self.plane_splitters,
            surface_builder: SurfaceBuilder::new(),
            cmd_buffers,
            clip_tree: &mut scene.clip_tree,
            frame_gpu_data,
        };

        // Push a default dirty region which culls primitives
        // against the screen world rect, in absence of any
        // other dirty regions.
        let mut default_dirty_region = DirtyRegion::new(
            root_spatial_node_index,
        );
        default_dirty_region.add_dirty_region(
            frame_context.global_screen_world_rect.cast_unit(),
            frame_context.spatial_tree,
        );
        frame_state.push_dirty_region(default_dirty_region);

        for pic_index in &scene.tile_cache_pictures {
            if let Some((pic_context, mut pic_state, mut prim_list)) = scene
                .prim_store
                .pictures[pic_index.0]
                .take_context(
                    *pic_index,
                    None,
                    SubpixelMode::Allow,
                    &mut frame_state,
                    &frame_context,
                    &mut scratch.primitive,
                    tile_caches,
                )
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
                    tile_caches,
                    &mut scene.prim_instances,
                );

                let pic = &mut scene.prim_store.pictures[pic_index.0];
                pic.restore_context(
                    *pic_index,
                    prim_list,
                    pic_context,
                    &scene.prim_instances,
                    &frame_context,
                    &mut frame_state,
                );
            }
        }

        frame_state.pop_dirty_region();
        frame_state.surface_builder.finalize();
        profile.end_time(profiler::FRAME_PREPARE_TIME);
        profile.set(profiler::VISIBLE_PRIMITIVES, frame_state.num_visible_primitives);

        scratch.frame.dirty_region_stack = frame_state.dirty_region_stack.take();

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
        rg_builder: &mut RenderTaskGraphBuilder,
        stamp: FrameStamp,
        device_origin: DeviceIntPoint,
        scene_properties: &SceneProperties,
        data_stores: &mut DataStores,
        scratch: &mut ScratchBuffer,
        debug_flags: DebugFlags,
        tile_caches: &mut FastHashMap<SliceId, Box<TileCacheInstance>>,
        spatial_tree: &mut SpatialTree,
        dirty_rects_are_valid: bool,
        profile: &mut TransactionProfile,
        minimap_data: FastHashMap<ExternalScrollId, MinimapData>
    ) -> Frame {
        profile_scope!("build");
        profile_marker!("BuildFrame");

        profile.set(profiler::PRIMITIVES, scene.prim_instances.len());
        profile.set(profiler::PICTURE_CACHE_SLICES, scene.tile_cache_config.picture_cache_slice_count);
        scratch.begin_frame();
        gpu_cache.begin_frame(stamp);
        resource_cache.begin_frame(stamp, gpu_cache, profile);

        // TODO(gw): Follow up patches won't clear this, as they'll be assigned
        //           statically during scene building.
        scene.surfaces.clear();

        self.globals.update(gpu_cache);

        spatial_tree.update_tree(scene_properties);
        let mut transform_palette = spatial_tree.build_transform_palette();
        scene.clip_store.begin_frame(&mut scratch.clip_store);

        rg_builder.begin_frame(stamp.frame_id());

        // TODO(dp): Remove me completely!!
        let global_device_pixel_scale = DevicePixelScale::new(1.0);

        let output_size = scene.output_rect.size();
        let screen_world_rect = (scene.output_rect.to_f32() / global_device_pixel_scale).round_out();

        let mut composite_state = CompositeState::new(
            scene.config.compositor_kind,
            scene.config.max_depth_ids,
            dirty_rects_are_valid,
            scene.config.low_quality_pinch_zoom,
        );

        self.composite_state_prealloc.preallocate(&mut composite_state);

        let mut cmd_buffers = CommandBufferList::new();

        // TODO(gw): Recycle backing vec buffers for gpu buffer builder between frames
        let mut gpu_buffer_builder = GpuBufferBuilder::new();

        self.build_layer_screen_rects_and_cull_layers(
            scene,
            screen_world_rect,
            resource_cache,
            gpu_cache,
            rg_builder,
            global_device_pixel_scale,
            scene_properties,
            &mut transform_palette,
            data_stores,
            scratch,
            debug_flags,
            &mut composite_state,
            tile_caches,
            spatial_tree,
            &mut cmd_buffers,
            &mut gpu_buffer_builder,
            profile,
        );

        self.render_minimap(&mut scratch.primitive, &spatial_tree, minimap_data);

        profile.start_time(profiler::FRAME_BATCHING_TIME);

        let mut deferred_resolves = vec![];

        // Finish creating the frame graph and build it.
        let render_tasks = rg_builder.end_frame(
            resource_cache,
            gpu_cache,
            &mut deferred_resolves,
            scene.config.max_shared_surface_size,
        );

        let mut passes = Vec::new();
        let mut has_texture_cache_tasks = false;
        let mut prim_headers = PrimitiveHeaders::new();
        self.prim_headers_prealloc.preallocate_vec(&mut prim_headers.headers_int);
        self.prim_headers_prealloc.preallocate_vec(&mut prim_headers.headers_float);

        {
            profile_marker!("Batching");

            // Used to generated a unique z-buffer value per primitive.
            let mut z_generator = ZBufferIdGenerator::new(scene.config.max_depth_ids);
            let use_dual_source_blending = scene.config.dual_source_blending_is_supported;

            for pass in render_tasks.passes.iter().rev() {
                let mut ctx = RenderTargetContext {
                    global_device_pixel_scale,
                    prim_store: &scene.prim_store,
                    clip_store: &scene.clip_store,
                    resource_cache,
                    use_dual_source_blending,
                    use_advanced_blending: scene.config.gpu_supports_advanced_blend,
                    break_advanced_blend_batches: !scene.config.advanced_blend_is_coherent,
                    batch_lookback_count: scene.config.batch_lookback_count,
                    spatial_tree,
                    data_stores,
                    surfaces: &scene.surfaces,
                    scratch: &mut scratch.primitive,
                    screen_world_rect,
                    globals: &self.globals,
                    tile_caches,
                    root_spatial_node_index: spatial_tree.root_reference_frame_index(),
                };

                let pass = build_render_pass(
                    pass,
                    output_size,
                    &mut ctx,
                    gpu_cache,
                    &mut gpu_buffer_builder,
                    &render_tasks,
                    &scene.clip_store,
                    &mut transform_palette,
                    &mut prim_headers,
                    &mut z_generator,
                    scene.config.gpu_supports_fast_clears,
                    &scene.prim_instances,
                    &cmd_buffers,
                );

                has_texture_cache_tasks |= !pass.texture_cache.is_empty();
                has_texture_cache_tasks |= !pass.picture_cache.is_empty();

                passes.push(pass);
            }

            let mut ctx = RenderTargetContext {
                global_device_pixel_scale,
                clip_store: &scene.clip_store,
                prim_store: &scene.prim_store,
                resource_cache,
                use_dual_source_blending,
                use_advanced_blending: scene.config.gpu_supports_advanced_blend,
                break_advanced_blend_batches: !scene.config.advanced_blend_is_coherent,
                batch_lookback_count: scene.config.batch_lookback_count,
                spatial_tree,
                data_stores,
                surfaces: &scene.surfaces,
                scratch: &mut scratch.primitive,
                screen_world_rect,
                globals: &self.globals,
                tile_caches,
                root_spatial_node_index: spatial_tree.root_reference_frame_index(),
            };

            self.build_composite_pass(
                scene,
                &mut ctx,
                gpu_cache,
                &mut deferred_resolves,
                &mut composite_state,
            );
        }

        profile.end_time(profiler::FRAME_BATCHING_TIME);

        let gpu_cache_frame_id = gpu_cache.end_frame(profile).frame_id();

        resource_cache.end_frame(profile);

        self.prim_headers_prealloc.record_vec(&mut prim_headers.headers_int);
        self.composite_state_prealloc.record(&composite_state);

        composite_state.end_frame();
        scene.clip_store.end_frame(&mut scratch.clip_store);
        scratch.end_frame();

        let gpu_buffer = gpu_buffer_builder.finalize(&render_tasks);

        Frame {
            device_rect: DeviceIntRect::from_origin_and_size(
                device_origin,
                scene.output_rect.size(),
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
            gpu_buffer,
        }
    }

    fn render_minimap(
        &self,
        scratch: &mut PrimitiveScratchBuffer,
        spatial_tree: &SpatialTree,
        minimap_data_store: FastHashMap<ExternalScrollId, MinimapData>) {
      // TODO: Replace minimap_data_store with Option<FastHastMap>?
      if minimap_data_store.is_empty() {
        return
      }

      // In our main walk over the spatial tree (below), for nodes inside a 
      // subtree rooted at a root-content node, we need some information from
      // that enclosing root-content node. To collect this information, do an
      // preliminary walk over the spatial tree now and collect the root-content
      // info in a HashMap.
      struct RootContentInfo {
        transform: LayoutToWorldTransform,
        clip: LayoutRect
      }
      let mut root_content_info = FastHashMap::<ExternalScrollId, RootContentInfo>::default();
      spatial_tree.visit_nodes(|index, node| {
        if let SpatialNodeType::ScrollFrame(ref scroll_frame_info) = node.node_type {
          if let Some(minimap_data) = minimap_data_store.get(&scroll_frame_info.external_id) {
            if minimap_data.is_root_content {
              let transform = spatial_tree.get_world_viewport_transform(index).into_transform();
              root_content_info.insert(scroll_frame_info.external_id, RootContentInfo{
                transform,
                clip: scroll_frame_info.viewport_rect
              });
            }
          }
        }
      });

      // This is the main walk over the spatial tree. For every scroll frame node which
      // has minimap data, compute the rects we want to render for that minimap in world
      // coordinates and add them to `scratch.debug_items`.
      spatial_tree.visit_nodes(|index, node| {
        if let SpatialNodeType::ScrollFrame(ref scroll_frame_info) = node.node_type {
          if let Some(minimap_data) = minimap_data_store.get(&scroll_frame_info.external_id) {
            const HORIZONTAL_PADDING: f32 = 5.0;
            const VERTICAL_PADDING: f32 = 10.0;
            const PAGE_BORDER_COLOR: ColorF = debug_colors::BLACK;
            const BACKGROUND_COLOR: ColorF = ColorF { r: 0.3, g: 0.3, b: 0.3, a: 0.3};
            const DISPLAYPORT_BACKGROUND_COLOR: ColorF = ColorF { r: 1.0, g: 1.0, b: 1.0, a: 0.4};
            const LAYOUT_PORT_COLOR: ColorF = debug_colors::RED;
            const VISUAL_PORT_COLOR: ColorF = debug_colors::BLUE;
            const DISPLAYPORT_COLOR: ColorF = debug_colors::LIME;
            const NOTHING: ColorF = ColorF { r: 0.0, g: 0.0, b: 0.0, a: 0.0 };

            let viewport = scroll_frame_info.viewport_rect;

            // Scale the minimap to make it 100px wide (if there's space), and the full height
            // of the scroll frame's viewport, minus some padding. Position it at the left edge
            // of the scroll frame's viewport.
            let scale_factor_x = 100f32.min(viewport.width() - (2.0 * HORIZONTAL_PADDING))
                                   / minimap_data.scrollable_rect.width();
            let scale_factor_y = (viewport.height() - (2.0 * VERTICAL_PADDING))
                                / minimap_data.scrollable_rect.height();
            if scale_factor_x <= 0.0 || scale_factor_y <= 0.0 {
              return;
            }
            let transform = LayoutTransform::scale(scale_factor_x, scale_factor_y, 1.0)
                .then_translate(LayoutVector3D::new(HORIZONTAL_PADDING, VERTICAL_PADDING, 0.0))
                .then_translate(LayoutVector3D::new(viewport.min.x, viewport.min.y, 0.0));

            // Transforms for transforming rects in this scroll frame's local coordintes, to world coordinates.
            // For scroll frames inside a root-content subtree, we apply this transform in two parts
            // (local to root-content, and root-content to world), so that we can make additional
            // adjustments in root-content space. For scroll frames outside of a root-content subtree,
            // the entire world transform will be in `local_to_root_content`.
            let world_transform = spatial_tree
                .get_world_viewport_transform(index)
                .into_transform();
            let mut local_to_root_content = 
                world_transform.with_destination::<LayoutPixel>();
            let mut root_content_to_world = LayoutToWorldTransform::default();
            let mut root_content_clip = None;
            if minimap_data.root_content_scroll_id != 0 {
              if let Some(RootContentInfo{transform: root_content_transform, clip}) = root_content_info.get(&ExternalScrollId(minimap_data.root_content_scroll_id, minimap_data.root_content_pipeline_id)) {
                // Exclude the root-content node's zoom transform from `local_to_root_content`.
                // This ensures that the minimap remains unaffected by pinch-zooming
                // (in essence, remaining attached to the *visual* viewport, rather than to
                // the *layout* viewport which is what happens by default).
                let zoom_transform = minimap_data.zoom_transform;
                local_to_root_content = world_transform
                  .then(&root_content_transform.inverse().unwrap())
                  .then(&zoom_transform.inverse().unwrap());
                root_content_to_world = root_content_transform.clone();
                root_content_clip = Some(clip);
              }
            }

            let mut add_rect = |rect, border, fill| -> Option<()> {
              // Place rect in scroll frame's local coordinate space
              let transformed_rect = transform.outer_transformed_box2d(&rect)?;
              
              // Transform to world coordinates, using root-content coords as an intermediate step.
              let mut root_content_rect = local_to_root_content.outer_transformed_box2d(&transformed_rect)?;
              // In root-content coords, apply the root content node's viewport clip.
              // This prevents subframe minimaps from leaking into the chrome area when the root
              // scroll frame is scrolled.
              // TODO: The minimaps of nested subframes can still leak outside of the viewports of
              // their containing subframes. Should have a more proper fix for this.
              if let Some(clip) = root_content_clip {
                root_content_rect = root_content_rect.intersection(clip)?;
              }
              let world_rect = root_content_to_world.outer_transformed_box2d(&root_content_rect)?;

              // Add world coordinate rects to scratch.debug_items
              // TODO: Add a parameter to control the border thickness of the rects, and make them a bit thicker. 
              scratch.push_debug_rect(world_rect * DevicePixelScale::new(1.0), border, fill);

              Some(())
            };

            add_rect(minimap_data.scrollable_rect, PAGE_BORDER_COLOR, BACKGROUND_COLOR);
            add_rect(minimap_data.visual_viewport, VISUAL_PORT_COLOR, NOTHING);
            add_rect(minimap_data.displayport, DISPLAYPORT_COLOR, DISPLAYPORT_BACKGROUND_COLOR);
            // Only render a distinct layout viewport for the root content.
            // For other scroll frames, the visual and layout viewports coincide.
            if minimap_data.is_root_content {
              add_rect(minimap_data.layout_viewport, LAYOUT_PORT_COLOR, NOTHING);
            }
          }
        }
      });
    }

    fn build_composite_pass(
        &self,
        scene: &BuiltScene,
        ctx: &RenderTargetContext,
        gpu_cache: &mut GpuCache,
        deferred_resolves: &mut Vec<DeferredResolve>,
        composite_state: &mut CompositeState,
    ) {
        for pic_index in &scene.tile_cache_pictures {
            let pic = &ctx.prim_store.pictures[pic_index.0];

            match pic.raster_config {
                Some(RasterConfig { composite_mode: PictureCompositeMode::TileCache { slice_id }, .. }) => {
                    // Tile cache instances are added to the composite config, rather than
                    // directly added to batches. This allows them to be drawn with various
                    // present modes during render, such as partial present etc.
                    let tile_cache = &ctx.tile_caches[&slice_id];
                    let map_local_to_world = SpaceMapper::new_with_target(
                        ctx.root_spatial_node_index,
                        tile_cache.spatial_node_index,
                        ctx.screen_world_rect,
                        ctx.spatial_tree,
                    );
                    let world_clip_rect = map_local_to_world
                        .map(&tile_cache.local_clip_rect)
                        .expect("bug: unable to map clip rect");
                    let device_clip_rect = (world_clip_rect * ctx.global_device_pixel_scale).round();

                    composite_state.push_surface(
                        tile_cache,
                        device_clip_rect,
                        ctx.resource_cache,
                        gpu_cache,
                        deferred_resolves,
                    );
                }
                _ => {
                    panic!("bug: found a top-level prim that isn't a tile cache");
                }
            }
        }
    }
}

/// Processes this pass to prepare it for rendering.
///
/// Among other things, this allocates output regions for each of our tasks
/// (added via `add_render_task`) in a RenderTarget and assigns it into that
/// target.
pub fn build_render_pass(
    src_pass: &Pass,
    screen_size: DeviceIntSize,
    ctx: &mut RenderTargetContext,
    gpu_cache: &mut GpuCache,
    gpu_buffer_builder: &mut GpuBufferBuilder,
    render_tasks: &RenderTaskGraph,
    clip_store: &ClipStore,
    transforms: &mut TransformPalette,
    prim_headers: &mut PrimitiveHeaders,
    z_generator: &mut ZBufferIdGenerator,
    gpu_supports_fast_clears: bool,
    prim_instances: &[PrimitiveInstance],
    cmd_buffers: &CommandBufferList,
) -> RenderPass {
    profile_scope!("build_render_pass");

    // TODO(gw): In this initial frame graph work, we try to maintain the existing
    //           build_render_pass code as closely as possible, to make the review
    //           simpler and reduce chance of regressions. However, future work should
    //           include refactoring this to more closely match the built frame graph.
    let mut pass = RenderPass::new(src_pass);

    for sub_pass in &src_pass.sub_passes {
        match sub_pass.surface {
            SubPassSurface::Dynamic { target_kind, texture_id, used_rect } => {
                match target_kind {
                    RenderTargetKind::Color => {
                        let mut target = ColorRenderTarget::new(
                            texture_id,
                            screen_size,
                            gpu_supports_fast_clears,
                            used_rect,
                        );

                        for task_id in &sub_pass.task_ids {
                            target.add_task(
                                *task_id,
                                ctx,
                                gpu_cache,
                                gpu_buffer_builder,
                                render_tasks,
                                clip_store,
                                transforms,
                            );
                        }

                        pass.color.targets.push(target);
                    }
                    RenderTargetKind::Alpha => {
                        let mut target = AlphaRenderTarget::new(
                            texture_id,
                            screen_size,
                            gpu_supports_fast_clears,
                            used_rect,
                        );

                        for task_id in &sub_pass.task_ids {
                            target.add_task(
                                *task_id,
                                ctx,
                                gpu_cache,
                                gpu_buffer_builder,
                                render_tasks,
                                clip_store,
                                transforms,
                            );
                        }

                        pass.alpha.targets.push(target);
                    }
                }
            }
            SubPassSurface::Persistent { surface: StaticRenderTaskSurface::PictureCache { ref surface, .. }, .. } => {
                assert_eq!(sub_pass.task_ids.len(), 1);
                let task_id = sub_pass.task_ids[0];
                let task = &render_tasks[task_id];
                let target_rect = task.get_target_rect();
                let mut gpu_buffer_builder = GpuBufferBuilder::new();

                match task.kind {
                    RenderTaskKind::Picture(ref pic_task) => {
                        let cmd_buffer = cmd_buffers.get(pic_task.cmd_buffer_index);
                        let scissor_rect = pic_task.scissor_rect.expect("bug: must be set for cache tasks");
                        let valid_rect = pic_task.valid_rect.expect("bug: must be set for cache tasks");

                        let batcher = AlphaBatchBuilder::new(
                            screen_size,
                            ctx.break_advanced_blend_batches,
                            ctx.batch_lookback_count,
                            task_id,
                            task_id.into(),
                        );

                        let mut batch_builder = BatchBuilder::new(batcher);

                        cmd_buffer.iter_prims(&mut |cmd, spatial_node_index, segments| {
                            batch_builder.add_prim_to_batch(
                                cmd,
                                spatial_node_index,
                                ctx,
                                gpu_cache,
                                render_tasks,
                                prim_headers,
                                transforms,
                                pic_task.raster_spatial_node_index,
                                pic_task.surface_spatial_node_index,
                                z_generator,
                                prim_instances,
                                &mut gpu_buffer_builder,
                                segments,
                            );
                        });

                        let batcher = batch_builder.finalize();

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
                            clear_color: pic_task.clear_color,
                            kind: PictureCacheTargetKind::Draw {
                                alpha_batch_container,
                            },
                            dirty_rect: scissor_rect,
                            valid_rect,
                        };

                        pass.picture_cache.push(target);
                    }
                    RenderTaskKind::TileComposite(ref tile_task) => {
                        let target = PictureCacheTarget {
                            surface: surface.clone(),
                            clear_color: Some(tile_task.clear_color),
                            kind: PictureCacheTargetKind::Blit {
                                task_id: tile_task.task_id.expect("bug: no source task_id set"),
                                sub_rect_offset: tile_task.sub_rect_offset,
                            },
                            dirty_rect: tile_task.scissor_rect,
                            valid_rect: tile_task.valid_rect,
                        };

                        pass.picture_cache.push(target);
                    }
                    _ => {
                        unreachable!();
                    }
                };
            }
            SubPassSurface::Persistent { surface: StaticRenderTaskSurface::TextureCache { target_kind, texture, .. } } => {
                let texture = pass.texture_cache
                    .entry(texture)
                    .or_insert_with(||
                        TextureCacheRenderTarget::new(target_kind)
                    );
                for task_id in &sub_pass.task_ids {
                    texture.add_task(*task_id, render_tasks);
                }
            }
            SubPassSurface::Persistent { surface: StaticRenderTaskSurface::ReadOnly { .. } } => {
                panic!("Should not create a render pass for read-only task locations.");
            }
        }
    }

    pass.color.build(
        ctx,
        gpu_cache,
        render_tasks,
        prim_headers,
        transforms,
        z_generator,
        prim_instances,
        cmd_buffers,
    );
    pass.alpha.build(
        ctx,
        gpu_cache,
        render_tasks,
        prim_headers,
        transforms,
        z_generator,
        prim_instances,
        cmd_buffers,
    );

    pass
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

    /// Main GPU data buffer constructed (primarily) during the prepare
    /// pass for primitives that were visible and dirty.
    pub gpu_buffer: GpuBuffer,
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
