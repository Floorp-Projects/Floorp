/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{ColorF, BorderStyle, FilterPrimitive, MixBlendMode, PipelineId, PremultipliedColorF};
use api::{DocumentLayer, FilterData, ImageFormat, LineOrientation};
use api::units::*;
use crate::batch::{AlphaBatchBuilder, AlphaBatchContainer, BatchTextures, ClipBatcher, resolve_image, BatchBuilder};
use crate::clip::ClipStore;
use crate::clip_scroll_tree::{ClipScrollTree, ROOT_SPATIAL_NODE_INDEX};
use crate::debug_render::DebugItem;
use crate::device::{Texture};
use crate::frame_builder::FrameGlobalResources;
use crate::gpu_cache::{GpuCache, GpuCacheAddress};
use crate::gpu_types::{BorderInstance, SvgFilterInstance, BlurDirection, BlurInstance, PrimitiveHeaders, ScalingInstance};
use crate::gpu_types::{TransformData, TransformPalette, ZBufferIdGenerator};
use crate::internal_types::{CacheTextureId, FastHashMap, LayerIndex, SavedTargetIndex, Swizzle, TextureSource, Filter};
use crate::picture::{RecordedDirtyRegion, SurfaceInfo};
use crate::prim_store::gradient::GRADIENT_FP_STOPS;
use crate::prim_store::{PrimitiveStore, DeferredResolve, PrimitiveScratchBuffer, PrimitiveVisibilityMask};
use crate::profiler::FrameProfileCounters;
use crate::render_backend::{DataStores, FrameId};
use crate::render_task::{BlitSource, RenderTargetKind, RenderTaskAddress, RenderTask, RenderTaskId, RenderTaskKind};
use crate::render_task::{BlurTask, ClearMode, RenderTaskLocation, RenderTaskGraph, ScalingTask, SvgFilterTask, SvgFilterInfo};
use crate::resource_cache::ResourceCache;
use std::{cmp, usize, f32, i32, mem};
use crate::texture_allocator::{ArrayAllocationTracker, FreeRectSlice};


const STYLE_SOLID: i32 = ((BorderStyle::Solid as i32) << 8) | ((BorderStyle::Solid as i32) << 16);
const STYLE_MASK: i32 = 0x00FF_FF00;

/// According to apitrace, textures larger than 2048 break fast clear
/// optimizations on some intel drivers. We sometimes need to go larger, but
/// we try to avoid it. This can go away when proper tiling support lands,
/// since we can then split large primitives across multiple textures.
const IDEAL_MAX_TEXTURE_DIMENSION: i32 = 2048;
/// If we ever need a larger texture than the ideal, we better round it up to a
/// reasonable number in order to have a bit of leeway in placing things inside.
const TEXTURE_DIMENSION_MASK: i32 = 0xFF;

/// Identifies a given `RenderTarget` in a `RenderTargetList`.
#[derive(Debug, Copy, Clone)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct RenderTargetIndex(pub usize);

pub struct RenderTargetContext<'a, 'rc> {
    pub global_device_pixel_scale: DevicePixelScale,
    pub prim_store: &'a PrimitiveStore,
    pub resource_cache: &'rc mut ResourceCache,
    pub use_dual_source_blending: bool,
    pub use_advanced_blending: bool,
    pub break_advanced_blend_batches: bool,
    pub batch_lookback_count: usize,
    pub clip_scroll_tree: &'a ClipScrollTree,
    pub data_stores: &'a DataStores,
    pub surfaces: &'a [SurfaceInfo],
    pub scratch: &'a PrimitiveScratchBuffer,
    pub screen_world_rect: WorldRect,
    pub globals: &'a FrameGlobalResources,
}

/// Represents a number of rendering operations on a surface.
///
/// In graphics parlance, a "render target" usually means "a surface (texture or
/// framebuffer) bound to the output of a shader". This trait has a slightly
/// different meaning, in that it represents the operations on that surface
/// _before_ it's actually bound and rendered. So a `RenderTarget` is built by
/// the `RenderBackend` by inserting tasks, and then shipped over to the
/// `Renderer` where a device surface is resolved and the tasks are transformed
/// into draw commands on that surface.
///
/// We express this as a trait to generalize over color and alpha surfaces.
/// a given `RenderTask` will draw to one or the other, depending on its type
/// and sometimes on its parameters. See `RenderTask::target_kind`.
pub trait RenderTarget {
    /// Creates a new RenderTarget of the given type.
    fn new(
        screen_size: DeviceIntSize,
        gpu_supports_fast_clears: bool,
    ) -> Self;

    /// Optional hook to provide additional processing for the target at the
    /// end of the build phase.
    fn build(
        &mut self,
        _ctx: &mut RenderTargetContext,
        _gpu_cache: &mut GpuCache,
        _render_tasks: &mut RenderTaskGraph,
        _deferred_resolves: &mut Vec<DeferredResolve>,
        _prim_headers: &mut PrimitiveHeaders,
        _transforms: &mut TransformPalette,
        _z_generator: &mut ZBufferIdGenerator,
    ) {
    }

    /// Associates a `RenderTask` with this target. That task must be assigned
    /// to a region returned by invoking `allocate()` on this target.
    ///
    /// TODO(gw): It's a bit odd that we need the deferred resolves and mutable
    /// GPU cache here. They are typically used by the build step above. They
    /// are used for the blit jobs to allow resolve_image to be called. It's a
    /// bit of extra overhead to store the image key here and the resolve them
    /// in the build step separately.  BUT: if/when we add more texture cache
    /// target jobs, we might want to tidy this up.
    fn add_task(
        &mut self,
        task_id: RenderTaskId,
        ctx: &RenderTargetContext,
        gpu_cache: &mut GpuCache,
        render_tasks: &RenderTaskGraph,
        clip_store: &ClipStore,
        transforms: &mut TransformPalette,
        deferred_resolves: &mut Vec<DeferredResolve>,
    );

    fn needs_depth(&self) -> bool;

    fn used_rect(&self) -> DeviceIntRect;
    fn add_used(&mut self, rect: DeviceIntRect);
}

/// A series of `RenderTarget` instances, serving as the high-level container
/// into which `RenderTasks` are assigned.
///
/// During the build phase, we iterate over the tasks in each `RenderPass`. For
/// each task, we invoke `allocate()` on the `RenderTargetList`, which in turn
/// attempts to allocate an output region in the last `RenderTarget` in the
/// list. If allocation fails (or if the list is empty), a new `RenderTarget` is
/// created and appended to the list. The build phase then assign the task into
/// the target associated with the final allocation.
///
/// The result is that each `RenderPass` is associated with one or two
/// `RenderTargetLists`, depending on whether we have all our tasks have the
/// same `RenderTargetKind`. The lists are then shipped to the `Renderer`, which
/// allocates a device texture array, with one slice per render target in the
/// list.
///
/// The upshot of this scheme is that it maximizes batching. In a given pass,
/// we need to do a separate batch for each individual render target. But with
/// the texture array, we can expose the entirety of the previous pass to each
/// task in the current pass in a single batch, which generally allows each
/// task to be drawn in a single batch regardless of how many results from the
/// previous pass it depends on.
///
/// Note that in some cases (like drop-shadows), we can depend on the output of
/// a pass earlier than the immediately-preceding pass. See `SavedTargetIndex`.
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct RenderTargetList<T> {
    screen_size: DeviceIntSize,
    pub format: ImageFormat,
    /// The maximum width and height of any single primitive we've encountered
    /// that will be drawn to a dynamic location.
    ///
    /// We initially create our per-slice allocators with a width and height of
    /// IDEAL_MAX_TEXTURE_DIMENSION. If we encounter a larger primitive, the
    /// allocation will fail, but we'll bump max_dynamic_size, which will cause the
    /// allocator for the next slice to be just large enough to accomodate it.
    pub max_dynamic_size: DeviceIntSize,
    pub targets: Vec<T>,
    pub saved_index: Option<SavedTargetIndex>,
    pub alloc_tracker: ArrayAllocationTracker,
    gpu_supports_fast_clears: bool,
}

impl<T: RenderTarget> RenderTargetList<T> {
    fn new(
        screen_size: DeviceIntSize,
        format: ImageFormat,
        gpu_supports_fast_clears: bool,
    ) -> Self {
        RenderTargetList {
            screen_size,
            format,
            max_dynamic_size: DeviceIntSize::new(0, 0),
            targets: Vec::new(),
            saved_index: None,
            alloc_tracker: ArrayAllocationTracker::new(),
            gpu_supports_fast_clears,
        }
    }

    fn build(
        &mut self,
        ctx: &mut RenderTargetContext,
        gpu_cache: &mut GpuCache,
        render_tasks: &mut RenderTaskGraph,
        deferred_resolves: &mut Vec<DeferredResolve>,
        saved_index: Option<SavedTargetIndex>,
        prim_headers: &mut PrimitiveHeaders,
        transforms: &mut TransformPalette,
        z_generator: &mut ZBufferIdGenerator,
    ) {
        debug_assert_eq!(None, self.saved_index);
        self.saved_index = saved_index;

        for target in &mut self.targets {
            target.build(
                ctx,
                gpu_cache,
                render_tasks,
                deferred_resolves,
                prim_headers,
                transforms,
                z_generator,
            );
        }
    }

    fn allocate(
        &mut self,
        alloc_size: DeviceIntSize,
    ) -> (RenderTargetIndex, DeviceIntPoint) {
        let (free_rect_slice, origin) = match self.alloc_tracker.allocate(&alloc_size) {
            Some(allocation) => allocation,
            None => {
                // Have the allocator restrict slice sizes to our max ideal
                // dimensions, unless we've already gone bigger on a previous
                // slice.
                let rounded_dimensions = DeviceIntSize::new(
                    (self.max_dynamic_size.width + TEXTURE_DIMENSION_MASK) & !TEXTURE_DIMENSION_MASK,
                    (self.max_dynamic_size.height + TEXTURE_DIMENSION_MASK) & !TEXTURE_DIMENSION_MASK,
                );
                let allocator_dimensions = DeviceIntSize::new(
                    cmp::max(IDEAL_MAX_TEXTURE_DIMENSION, rounded_dimensions.width),
                    cmp::max(IDEAL_MAX_TEXTURE_DIMENSION, rounded_dimensions.height),
                );

                assert!(alloc_size.width <= allocator_dimensions.width &&
                    alloc_size.height <= allocator_dimensions.height);
                let slice = FreeRectSlice(self.targets.len() as u32);
                self.targets.push(T::new(self.screen_size, self.gpu_supports_fast_clears));

                self.alloc_tracker.extend(
                    slice,
                    allocator_dimensions,
                    alloc_size,
                );

                (slice, DeviceIntPoint::zero())
            }
        };

        if alloc_size.is_empty_or_negative() && self.targets.is_empty() {
            // push an unused target here, only if we don't have any
            self.targets.push(T::new(self.screen_size, self.gpu_supports_fast_clears));
        }

        self.targets[free_rect_slice.0 as usize]
            .add_used(DeviceIntRect::new(origin, alloc_size));

        (RenderTargetIndex(free_rect_slice.0 as usize), origin)
    }

    pub fn needs_depth(&self) -> bool {
        self.targets.iter().any(|target| target.needs_depth())
    }

    pub fn check_ready(&self, t: &Texture) {
        let dimensions = t.get_dimensions();
        assert!(dimensions.width >= self.max_dynamic_size.width);
        assert!(dimensions.height >= self.max_dynamic_size.height);
        assert_eq!(t.get_format(), self.format);
        assert_eq!(t.get_layer_count() as usize, self.targets.len());
        assert!(t.supports_depth() >= self.needs_depth());
    }
}

/// Frame output information for a given pipeline ID.
/// Storing the task ID allows the renderer to find
/// the target rect within the render target that this
/// pipeline exists at.
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct FrameOutput {
    pub task_id: RenderTaskId,
    pub pipeline_id: PipelineId,
}

// Defines where the source data for a blit job can be found.
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub enum BlitJobSource {
    Texture(TextureSource, i32, DeviceIntRect),
    RenderTask(RenderTaskId),
}

// Information required to do a blit from a source to a target.
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct BlitJob {
    pub source: BlitJobSource,
    pub target_rect: DeviceIntRect,
}

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct LineDecorationJob {
    pub task_rect: DeviceRect,
    pub local_size: LayoutSize,
    pub wavy_line_thickness: f32,
    pub style: i32,
    pub orientation: i32,
}

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[repr(C)]
pub struct GradientJob {
    pub task_rect: DeviceRect,
    pub stops: [f32; GRADIENT_FP_STOPS],
    pub colors: [PremultipliedColorF; GRADIENT_FP_STOPS],
    pub axis_select: f32,
    pub start_stop: [f32; 2],
}

/// Contains the work (in the form of instance arrays) needed to fill a color
/// color output surface (RGBA8).
///
/// See `RenderTarget`.
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct ColorRenderTarget {
    pub alpha_batch_containers: Vec<AlphaBatchContainer>,
    // List of blur operations to apply for this render target.
    pub vertical_blurs: Vec<BlurInstance>,
    pub horizontal_blurs: Vec<BlurInstance>,
    pub readbacks: Vec<DeviceIntRect>,
    pub scalings: FastHashMap<TextureSource, Vec<ScalingInstance>>,
    pub svg_filters: Vec<(BatchTextures, Vec<SvgFilterInstance>)>,
    pub blits: Vec<BlitJob>,
    // List of frame buffer outputs for this render target.
    pub outputs: Vec<FrameOutput>,
    alpha_tasks: Vec<RenderTaskId>,
    screen_size: DeviceIntSize,
    // Track the used rect of the render target, so that
    // we can set a scissor rect and only clear to the
    // used portion of the target as an optimization.
    pub used_rect: DeviceIntRect,
}

impl RenderTarget for ColorRenderTarget {
    fn new(
        screen_size: DeviceIntSize,
        _: bool,
    ) -> Self {
        ColorRenderTarget {
            alpha_batch_containers: Vec::new(),
            vertical_blurs: Vec::new(),
            horizontal_blurs: Vec::new(),
            readbacks: Vec::new(),
            scalings: FastHashMap::default(),
            svg_filters: Vec::new(),
            blits: Vec::new(),
            outputs: Vec::new(),
            alpha_tasks: Vec::new(),
            screen_size,
            used_rect: DeviceIntRect::zero(),
        }
    }

    fn build(
        &mut self,
        ctx: &mut RenderTargetContext,
        gpu_cache: &mut GpuCache,
        render_tasks: &mut RenderTaskGraph,
        deferred_resolves: &mut Vec<DeferredResolve>,
        prim_headers: &mut PrimitiveHeaders,
        transforms: &mut TransformPalette,
        z_generator: &mut ZBufferIdGenerator,
    ) {
        let mut merged_batches = AlphaBatchContainer::new(None);

        for task_id in &self.alpha_tasks {
            let task = &render_tasks[*task_id];

            match task.clear_mode {
                ClearMode::One |
                ClearMode::Zero => {
                    panic!("bug: invalid clear mode for color task");
                }
                ClearMode::DontCare |
                ClearMode::Transparent => {}
            }

            match task.kind {
                RenderTaskKind::Picture(ref pic_task) => {
                    let pic = &ctx.prim_store.pictures[pic_task.pic_index.0];

                    let raster_spatial_node_index = match pic.raster_config {
                        Some(ref raster_config) => {
                            let surface = &ctx.surfaces[raster_config.surface_index.0];
                            surface.raster_spatial_node_index
                        }
                        None => {
                            // This must be the main framebuffer
                            ROOT_SPATIAL_NODE_INDEX
                        }
                    };

                    let (target_rect, _) = task.get_target_rect();

                    let scissor_rect = if pic_task.can_merge {
                        None
                    } else {
                        Some(target_rect)
                    };

                    // TODO(gw): The type names of AlphaBatchBuilder and BatchBuilder
                    //           are still confusing. Once more of the picture caching
                    //           improvement code lands, the AlphaBatchBuilder and
                    //           AlphaBatchList types will be collapsed into one, which
                    //           should simplify coming up with better type names.
                    let alpha_batch_builder = AlphaBatchBuilder::new(
                        self.screen_size,
                        ctx.break_advanced_blend_batches,
                        ctx.batch_lookback_count,
                        *task_id,
                        render_tasks.get_task_address(*task_id),
                        PrimitiveVisibilityMask::all(),
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
                        raster_spatial_node_index,
                        pic_task.surface_spatial_node_index,
                        z_generator,
                    );

                    let alpha_batch_builders = batch_builder.finalize();

                    for batcher in alpha_batch_builders {
                        batcher.build(
                            &mut self.alpha_batch_containers,
                            &mut merged_batches,
                            target_rect,
                            scissor_rect,
                        );
                    }
                }
                _ => {
                    unreachable!();
                }
            }
        }

        if !merged_batches.is_empty() {
            self.alpha_batch_containers.push(merged_batches);
        }
    }

    fn add_task(
        &mut self,
        task_id: RenderTaskId,
        ctx: &RenderTargetContext,
        gpu_cache: &mut GpuCache,
        render_tasks: &RenderTaskGraph,
        _: &ClipStore,
        _: &mut TransformPalette,
        deferred_resolves: &mut Vec<DeferredResolve>,
    ) {
        let task = &render_tasks[task_id];

        match task.kind {
            RenderTaskKind::VerticalBlur(ref info) => {
                info.add_instances(
                    &mut self.vertical_blurs,
                    BlurDirection::Vertical,
                    render_tasks.get_task_address(task_id),
                    render_tasks.get_task_address(task.children[0]),
                );
            }
            RenderTaskKind::HorizontalBlur(ref info) => {
                info.add_instances(
                    &mut self.horizontal_blurs,
                    BlurDirection::Horizontal,
                    render_tasks.get_task_address(task_id),
                    render_tasks.get_task_address(task.children[0]),
                );
            }
            RenderTaskKind::Picture(ref task_info) => {
                let pic = &ctx.prim_store.pictures[task_info.pic_index.0];
                self.alpha_tasks.push(task_id);

                // If this pipeline is registered as a frame output
                // store the information necessary to do the copy.
                if let Some(pipeline_id) = pic.frame_output_pipeline_id {
                    self.outputs.push(FrameOutput {
                        pipeline_id,
                        task_id,
                    });
                }
            }
            RenderTaskKind::SvgFilter(ref task_info) => {
                task_info.add_instances(
                    &mut self.svg_filters,
                    render_tasks,
                    &task_info.info,
                    task_id,
                    task.children.get(0).cloned(),
                    task.children.get(1).cloned(),
                    task_info.extra_gpu_cache_handle.map(|handle| gpu_cache.get_address(&handle)),
                )
            }
            RenderTaskKind::ClipRegion(..) |
            RenderTaskKind::Border(..) |
            RenderTaskKind::CacheMask(..) |
            RenderTaskKind::Gradient(..) |
            RenderTaskKind::LineDecoration(..) => {
                panic!("Should not be added to color target!");
            }
            RenderTaskKind::Readback(device_rect) => {
                self.readbacks.push(device_rect);
            }
            RenderTaskKind::Scaling(ref info) => {
                info.add_instances(
                    &mut self.scalings,
                    task,
                    task.children.first().map(|&child| &render_tasks[child]),
                    ctx.resource_cache,
                    gpu_cache,
                    deferred_resolves,
                );
            }
            RenderTaskKind::Blit(ref task_info) => {
                let source = match task_info.source {
                    BlitSource::Image { key } => {
                        // Get the cache item for the source texture.
                        let cache_item = resolve_image(
                            key.request,
                            ctx.resource_cache,
                            gpu_cache,
                            deferred_resolves,
                        );

                        // Work out a source rect to copy from the texture, depending on whether
                        // a sub-rect is present or not.
                        let source_rect = key.texel_rect.map_or(cache_item.uv_rect.to_i32(), |sub_rect| {
                            DeviceIntRect::new(
                                DeviceIntPoint::new(
                                    cache_item.uv_rect.origin.x as i32 + sub_rect.origin.x,
                                    cache_item.uv_rect.origin.y as i32 + sub_rect.origin.y,
                                ),
                                sub_rect.size,
                            )
                        });

                        // Store the blit job for the renderer to execute, including
                        // the allocated destination rect within this target.
                        BlitJobSource::Texture(
                            cache_item.texture_id,
                            cache_item.texture_layer,
                            source_rect,
                        )
                    }
                    BlitSource::RenderTask { task_id } => {
                        BlitJobSource::RenderTask(task_id)
                    }
                };

                let target_rect = task
                    .get_target_rect()
                    .0
                    .inner_rect(task_info.padding);
                self.blits.push(BlitJob {
                    source,
                    target_rect,
                });
            }
            #[cfg(test)]
            RenderTaskKind::Test(..) => {}
        }
    }

    fn needs_depth(&self) -> bool {
        self.alpha_batch_containers.iter().any(|ab| {
            !ab.opaque_batches.is_empty()
        })
    }

    fn used_rect(&self) -> DeviceIntRect {
        self.used_rect
    }

    fn add_used(&mut self, rect: DeviceIntRect) {
        self.used_rect = self.used_rect.union(&rect);
    }
}

/// Contains the work (in the form of instance arrays) needed to fill an alpha
/// output surface (R8).
///
/// See `RenderTarget`.
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct AlphaRenderTarget {
    pub clip_batcher: ClipBatcher,
    // List of blur operations to apply for this render target.
    pub vertical_blurs: Vec<BlurInstance>,
    pub horizontal_blurs: Vec<BlurInstance>,
    pub scalings: FastHashMap<TextureSource, Vec<ScalingInstance>>,
    pub zero_clears: Vec<RenderTaskId>,
    pub one_clears: Vec<RenderTaskId>,
    // Track the used rect of the render target, so that
    // we can set a scissor rect and only clear to the
    // used portion of the target as an optimization.
    pub used_rect: DeviceIntRect,
}

impl RenderTarget for AlphaRenderTarget {
    fn new(
        _: DeviceIntSize,
        gpu_supports_fast_clears: bool,
    ) -> Self {
        AlphaRenderTarget {
            clip_batcher: ClipBatcher::new(gpu_supports_fast_clears),
            vertical_blurs: Vec::new(),
            horizontal_blurs: Vec::new(),
            scalings: FastHashMap::default(),
            zero_clears: Vec::new(),
            one_clears: Vec::new(),
            used_rect: DeviceIntRect::zero(),
        }
    }

    fn add_task(
        &mut self,
        task_id: RenderTaskId,
        ctx: &RenderTargetContext,
        gpu_cache: &mut GpuCache,
        render_tasks: &RenderTaskGraph,
        clip_store: &ClipStore,
        transforms: &mut TransformPalette,
        deferred_resolves: &mut Vec<DeferredResolve>,
    ) {
        let task = &render_tasks[task_id];
        let (target_rect, _) = task.get_target_rect();

        match task.clear_mode {
            ClearMode::Zero => {
                self.zero_clears.push(task_id);
            }
            ClearMode::One => {
                self.one_clears.push(task_id);
            }
            ClearMode::DontCare => {}
            ClearMode::Transparent => {
                panic!("bug: invalid clear mode for alpha task");
            }
        }

        match task.kind {
            RenderTaskKind::Readback(..) |
            RenderTaskKind::Picture(..) |
            RenderTaskKind::Blit(..) |
            RenderTaskKind::Border(..) |
            RenderTaskKind::LineDecoration(..) |
            RenderTaskKind::Gradient(..) |
            RenderTaskKind::SvgFilter(..) => {
                panic!("BUG: should not be added to alpha target!");
            }
            RenderTaskKind::VerticalBlur(ref info) => {
                info.add_instances(
                    &mut self.vertical_blurs,
                    BlurDirection::Vertical,
                    render_tasks.get_task_address(task_id),
                    render_tasks.get_task_address(task.children[0]),
                );
            }
            RenderTaskKind::HorizontalBlur(ref info) => {
                info.add_instances(
                    &mut self.horizontal_blurs,
                    BlurDirection::Horizontal,
                    render_tasks.get_task_address(task_id),
                    render_tasks.get_task_address(task.children[0]),
                );
            }
            RenderTaskKind::CacheMask(ref task_info) => {
                self.clip_batcher.add(
                    task_info.clip_node_range,
                    task_info.root_spatial_node_index,
                    ctx.resource_cache,
                    gpu_cache,
                    clip_store,
                    ctx.clip_scroll_tree,
                    transforms,
                    &ctx.data_stores.clip,
                    task_info.actual_rect,
                    &ctx.screen_world_rect,
                    task_info.device_pixel_scale,
                    task_info.snap_offsets,
                    target_rect.origin.to_f32(),
                    task_info.actual_rect.origin.to_f32(),
                );
            }
            RenderTaskKind::ClipRegion(ref region_task) => {
                let device_rect = DeviceRect::new(
                    DevicePoint::zero(),
                    target_rect.size.to_f32(),
                );
                self.clip_batcher.add_clip_region(
                    region_task.clip_data_address,
                    region_task.local_pos,
                    device_rect,
                    target_rect.origin.to_f32(),
                    DevicePoint::zero(),
                    region_task.device_pixel_scale.0,
                );
            }
            RenderTaskKind::Scaling(ref info) => {
                info.add_instances(
                    &mut self.scalings,
                    task,
                    task.children.first().map(|&child| &render_tasks[child]),
                    ctx.resource_cache,
                    gpu_cache,
                    deferred_resolves,
                );
            }
            #[cfg(test)]
            RenderTaskKind::Test(..) => {}
        }
    }

    fn needs_depth(&self) -> bool {
        false
    }

    fn used_rect(&self) -> DeviceIntRect {
        self.used_rect
    }

    fn add_used(&mut self, rect: DeviceIntRect) {
        self.used_rect = self.used_rect.union(&rect);
    }
}

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct PictureCacheTarget {
    pub texture: TextureSource,
    pub layer: usize,
    pub alpha_batch_container: AlphaBatchContainer,
    pub clear_color: Option<ColorF>,
}

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct TextureCacheRenderTarget {
    pub target_kind: RenderTargetKind,
    pub horizontal_blurs: Vec<BlurInstance>,
    pub blits: Vec<BlitJob>,
    pub border_segments_complex: Vec<BorderInstance>,
    pub border_segments_solid: Vec<BorderInstance>,
    pub clears: Vec<DeviceIntRect>,
    pub line_decorations: Vec<LineDecorationJob>,
    pub gradients: Vec<GradientJob>,
}

impl TextureCacheRenderTarget {
    fn new(target_kind: RenderTargetKind) -> Self {
        TextureCacheRenderTarget {
            target_kind,
            horizontal_blurs: vec![],
            blits: vec![],
            border_segments_complex: vec![],
            border_segments_solid: vec![],
            clears: vec![],
            line_decorations: vec![],
            gradients: vec![],
        }
    }

    fn add_task(
        &mut self,
        task_id: RenderTaskId,
        render_tasks: &mut RenderTaskGraph,
    ) {
        let task_address = render_tasks.get_task_address(task_id);
        let src_task_address = render_tasks[task_id].children.get(0).map(|src_task_id| {
            render_tasks.get_task_address(*src_task_id)
        });

        let task = &mut render_tasks[task_id];
        let target_rect = task.get_target_rect();

        match task.kind {
            RenderTaskKind::LineDecoration(ref info) => {
                self.clears.push(target_rect.0);

                self.line_decorations.push(LineDecorationJob {
                    task_rect: target_rect.0.to_f32(),
                    local_size: info.local_size,
                    style: info.style as i32,
                    orientation: info.orientation as i32,
                    wavy_line_thickness: info.wavy_line_thickness,
                });
            }
            RenderTaskKind::HorizontalBlur(ref info) => {
                info.add_instances(
                    &mut self.horizontal_blurs,
                    BlurDirection::Horizontal,
                    task_address,
                    src_task_address.unwrap(),
                );
            }
            RenderTaskKind::Blit(ref task_info) => {
                match task_info.source {
                    BlitSource::Image { .. } => {
                        // reading/writing from the texture cache at the same time
                        // is undefined behavior.
                        panic!("bug: a single blit cannot be to/from texture cache");
                    }
                    BlitSource::RenderTask { task_id } => {
                        // Add a blit job to copy from an existing render
                        // task to this target.
                        self.blits.push(BlitJob {
                            source: BlitJobSource::RenderTask(task_id),
                            target_rect: target_rect.0.inner_rect(task_info.padding),
                        });
                    }
                }
            }
            RenderTaskKind::Border(ref mut task_info) => {
                self.clears.push(target_rect.0);

                let task_origin = target_rect.0.origin.to_f32();
                let instances = mem::replace(&mut task_info.instances, Vec::new());
                for mut instance in instances {
                    // TODO(gw): It may be better to store the task origin in
                    //           the render task data instead of per instance.
                    instance.task_origin = task_origin;
                    if instance.flags & STYLE_MASK == STYLE_SOLID {
                        self.border_segments_solid.push(instance);
                    } else {
                        self.border_segments_complex.push(instance);
                    }
                }
            }
            RenderTaskKind::Gradient(ref task_info) => {
                let mut stops = [0.0; 4];
                let mut colors = [PremultipliedColorF::BLACK; 4];

                let axis_select = match task_info.orientation {
                    LineOrientation::Horizontal => 0.0,
                    LineOrientation::Vertical => 1.0,
                };

                for (stop, (offset, color)) in task_info.stops.iter().zip(stops.iter_mut().zip(colors.iter_mut())) {
                    *offset = stop.offset;
                    *color = ColorF::from(stop.color).premultiplied();
                }

                self.gradients.push(GradientJob {
                    task_rect: target_rect.0.to_f32(),
                    axis_select,
                    stops,
                    colors,
                    start_stop: [task_info.start_point, task_info.end_point],
                });
            }
            RenderTaskKind::VerticalBlur(..) |
            RenderTaskKind::Picture(..) |
            RenderTaskKind::ClipRegion(..) |
            RenderTaskKind::CacheMask(..) |
            RenderTaskKind::Readback(..) |
            RenderTaskKind::Scaling(..) |
            RenderTaskKind::SvgFilter(..) => {
                panic!("BUG: unexpected task kind for texture cache target");
            }
            #[cfg(test)]
            RenderTaskKind::Test(..) => {}
        }
    }
}

/// Contains the set of `RenderTarget`s specific to the kind of pass.
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub enum RenderPassKind {
    /// The final pass to the main frame buffer, where we have a single color
    /// target for display to the user.
    MainFramebuffer {
        main_target: ColorRenderTarget,
    },
    /// An intermediate pass, where we may have multiple targets.
    OffScreen {
        alpha: RenderTargetList<AlphaRenderTarget>,
        color: RenderTargetList<ColorRenderTarget>,
        texture_cache: FastHashMap<(CacheTextureId, usize), TextureCacheRenderTarget>,
        picture_cache: Vec<PictureCacheTarget>,
    },
}

/// A render pass represents a set of rendering operations that don't depend on one
/// another.
///
/// A render pass can have several render targets if there wasn't enough space in one
/// target to do all of the rendering for that pass. See `RenderTargetList`.
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct RenderPass {
    /// The kind of pass, as well as the set of targets associated with that
    /// kind of pass.
    pub kind: RenderPassKind,
    /// The set of tasks to be performed in this pass, as indices into the
    /// `RenderTaskGraph`.
    pub tasks: Vec<RenderTaskId>,
    /// Screen size in device pixels - used for opaque alpha batch break threshold.
    screen_size: DeviceIntSize,
}

impl RenderPass {
    /// Creates a pass for the main framebuffer. There is only one of these, and
    /// it is always the last pass.
    pub fn new_main_framebuffer(
        screen_size: DeviceIntSize,
        gpu_supports_fast_clears: bool,
    ) -> Self {
        let main_target = ColorRenderTarget::new(screen_size, gpu_supports_fast_clears);
        RenderPass {
            kind: RenderPassKind::MainFramebuffer {
                main_target,
            },
            tasks: vec![],
            screen_size,
        }
    }

    /// Creates an intermediate off-screen pass.
    pub fn new_off_screen(
        screen_size: DeviceIntSize,
        gpu_supports_fast_clears: bool,
    ) -> Self {
        RenderPass {
            kind: RenderPassKind::OffScreen {
                color: RenderTargetList::new(
                    screen_size,
                    ImageFormat::RGBA8,
                    gpu_supports_fast_clears,
                ),
                alpha: RenderTargetList::new(
                    screen_size,
                    ImageFormat::R8,
                    gpu_supports_fast_clears,
                ),
                texture_cache: FastHashMap::default(),
                picture_cache: Vec::new(),
            },
            tasks: vec![],
            screen_size,
        }
    }

    /// Adds a task to this pass.
    pub fn add_render_task(
        &mut self,
        task_id: RenderTaskId,
        size: DeviceIntSize,
        target_kind: RenderTargetKind,
        location: &RenderTaskLocation,
    ) {
        if let RenderPassKind::OffScreen { ref mut color, ref mut alpha, .. } = self.kind {
            // If this will be rendered to a dynamically-allocated region on an
            // off-screen render target, update the max-encountered size. We don't
            // need to do this for things drawn to the texture cache, since those
            // don't affect our render target allocation.
            if location.is_dynamic() {
                let max_size = match target_kind {
                    RenderTargetKind::Color => &mut color.max_dynamic_size,
                    RenderTargetKind::Alpha => &mut alpha.max_dynamic_size,
                };
                max_size.width = cmp::max(max_size.width, size.width);
                max_size.height = cmp::max(max_size.height, size.height);
            }
        }

        self.tasks.push(task_id);
    }

    /// Processes this pass to prepare it for rendering.
    ///
    /// Among other things, this allocates output regions for each of our tasks
    /// (added via `add_render_task`) in a RenderTarget and assigns it into that
    /// target.
    pub fn build(
        &mut self,
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

        match self.kind {
            RenderPassKind::MainFramebuffer { ref mut main_target, .. } => {
                for &task_id in &self.tasks {
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
                let saved_color = if self.tasks.iter().any(|&task_id| {
                    let t = &render_tasks[task_id];
                    t.target_kind() == RenderTargetKind::Color && t.saved_index.is_some()
                }) {
                    Some(render_tasks.save_target())
                } else {
                    None
                };
                let saved_alpha = if self.tasks.iter().any(|&task_id| {
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
                for &task_id in &self.tasks {
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
                            self.screen_size,
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
                                let mut batch_containers = Vec::new();
                                let mut alpha_batch_container = AlphaBatchContainer::new(None);
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
}

#[derive(Debug, Clone, Default)]
pub struct CompositeOps {
    // Requires only a single texture as input (e.g. most filters)
    pub filters: Vec<Filter>,
    pub filter_datas: Vec<FilterData>,
    pub filter_primitives: Vec<FilterPrimitive>,

    // Requires two source textures (e.g. mix-blend-mode)
    pub mix_blend_mode: Option<MixBlendMode>,
}

impl CompositeOps {
    pub fn new(
        filters: Vec<Filter>,
        filter_datas: Vec<FilterData>,
        filter_primitives: Vec<FilterPrimitive>,
        mix_blend_mode: Option<MixBlendMode>
    ) -> Self {
        CompositeOps {
            filters,
            filter_datas,
            filter_primitives,
            mix_blend_mode,
        }
    }

    pub fn is_empty(&self) -> bool {
        self.filters.is_empty() &&
            self.filter_primitives.is_empty() &&
            self.mix_blend_mode.is_none()
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

impl BlurTask {
    fn add_instances(
        &self,
        instances: &mut Vec<BlurInstance>,
        blur_direction: BlurDirection,
        task_address: RenderTaskAddress,
        src_task_address: RenderTaskAddress,
    ) {
        let instance = BlurInstance {
            task_address,
            src_task_address,
            blur_direction,
        };

        instances.push(instance);
    }
}

impl ScalingTask {
    fn add_instances(
        &self,
        instances: &mut FastHashMap<TextureSource, Vec<ScalingInstance>>,
        target_task: &RenderTask,
        source_task: Option<&RenderTask>,
        resource_cache: &ResourceCache,
        gpu_cache: &mut GpuCache,
        deferred_resolves: &mut Vec<DeferredResolve>,
    ) {
        let target_rect = target_task
            .get_target_rect()
            .0
            .inner_rect(self.padding)
            .to_f32();

        let (source, (source_rect, source_layer)) = match self.image {
            Some(key) => {
                assert!(source_task.is_none());

                // Get the cache item for the source texture.
                let cache_item = resolve_image(
                    key.request,
                    resource_cache,
                    gpu_cache,
                    deferred_resolves,
                );

                // Work out a source rect to copy from the texture, depending on whether
                // a sub-rect is present or not.
                let source_rect = key.texel_rect.map_or(cache_item.uv_rect, |sub_rect| {
                    DeviceIntRect::new(
                        DeviceIntPoint::new(
                            cache_item.uv_rect.origin.x + sub_rect.origin.x,
                            cache_item.uv_rect.origin.y + sub_rect.origin.y,
                        ),
                        sub_rect.size,
                    )
                });

                (
                    cache_item.texture_id,
                    (source_rect, cache_item.texture_layer as LayerIndex),
                )
            }
            None => {
                (
                    match self.target_kind {
                        RenderTargetKind::Color => TextureSource::PrevPassColor,
                        RenderTargetKind::Alpha => TextureSource::PrevPassAlpha,
                    },
                    source_task.unwrap().location.to_source_rect(),
                )
            }
        };

        instances
            .entry(source)
            .or_insert(Vec::new())
            .push(ScalingInstance {
                target_rect,
                source_rect,
                source_layer: source_layer as i32,
            });
    }
}

impl SvgFilterTask {
    fn add_instances(
        &self,
        instances: &mut Vec<(BatchTextures, Vec<SvgFilterInstance>)>,
        render_tasks: &RenderTaskGraph,
        filter: &SvgFilterInfo,
        task_id: RenderTaskId,
        input_1_task: Option<RenderTaskId>,
        input_2_task: Option<RenderTaskId>,
        extra_data_address: Option<GpuCacheAddress>,
    ) {
        let mut textures = BatchTextures::no_texture();

        if let Some(saved_index) = input_1_task.map(|id| &render_tasks[id].saved_index) {
            textures.colors[0] = match saved_index {
                Some(saved_index) => TextureSource::RenderTaskCache(*saved_index, Swizzle::default()),
                None => TextureSource::PrevPassColor,
            };
        }

        if let Some(saved_index) = input_2_task.map(|id| &render_tasks[id].saved_index) {
            textures.colors[1] = match saved_index {
                Some(saved_index) => TextureSource::RenderTaskCache(*saved_index, Swizzle::default()),
                None => TextureSource::PrevPassColor,
            };
        }

        let kind = match filter {
            SvgFilterInfo::Blend(..) => 0,
            SvgFilterInfo::Flood(..) => 1,
            SvgFilterInfo::LinearToSrgb => 2,
            SvgFilterInfo::SrgbToLinear => 3,
            SvgFilterInfo::Opacity(..) => 4,
            SvgFilterInfo::ColorMatrix(..) => 5,
            SvgFilterInfo::DropShadow(..) => 6,
            SvgFilterInfo::Offset(..) => 7,
            SvgFilterInfo::ComponentTransfer(..) => 8,
            SvgFilterInfo::Identity => 9,
            SvgFilterInfo::Composite(..) => 10,
        };

        let input_count = match filter {
            SvgFilterInfo::Flood(..) => 0,

            SvgFilterInfo::LinearToSrgb |
            SvgFilterInfo::SrgbToLinear |
            SvgFilterInfo::Opacity(..) |
            SvgFilterInfo::ColorMatrix(..) |
            SvgFilterInfo::Offset(..) |
            SvgFilterInfo::ComponentTransfer(..) |
            SvgFilterInfo::Identity => 1,

            // Not techincally a 2 input filter, but we have 2 inputs here: original content & blurred content.
            SvgFilterInfo::DropShadow(..) |
            SvgFilterInfo::Blend(..) |
            SvgFilterInfo::Composite(..) => 2,
        };

        let generic_int = match filter {
            SvgFilterInfo::Blend(mode) => *mode as u16,
            SvgFilterInfo::ComponentTransfer(data) =>
                ((data.r_func.to_int() << 12 |
                  data.g_func.to_int() << 8 |
                  data.b_func.to_int() << 4 |
                  data.a_func.to_int()) as u16),
            SvgFilterInfo::Composite(operator) =>
                operator.as_int() as u16,
            SvgFilterInfo::LinearToSrgb |
            SvgFilterInfo::SrgbToLinear |
            SvgFilterInfo::Flood(..) |
            SvgFilterInfo::Opacity(..) |
            SvgFilterInfo::ColorMatrix(..) |
            SvgFilterInfo::DropShadow(..) |
            SvgFilterInfo::Offset(..) |
            SvgFilterInfo::Identity => 0,
        };

        let instance = SvgFilterInstance {
            task_address: render_tasks.get_task_address(task_id),
            input_1_task_address: input_1_task.map(|id| render_tasks.get_task_address(id)).unwrap_or(RenderTaskAddress(0)),
            input_2_task_address: input_2_task.map(|id| render_tasks.get_task_address(id)).unwrap_or(RenderTaskAddress(0)),
            kind,
            input_count,
            generic_int,
            extra_data_address: extra_data_address.unwrap_or(GpuCacheAddress::INVALID),
        };

        for (ref mut batch_textures, ref mut batch) in instances.iter_mut() {
            if let Some(combined_textures) = batch_textures.combine_textures(textures) {
                batch.push(instance);
                // Update the batch textures to the newly combined batch textures
                *batch_textures = combined_textures;
                return;
            }
        }

        instances.push((textures, vec![instance]));
    }
}
