/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! This module contains the render task graph and it's output (the Frame).
//!
//! Code associated with creating specific render tasks is in the render_task
//! module.

use api::{ColorF, BorderStyle, PipelineId, PremultipliedColorF};
use api::{ImageFormat, LineOrientation};
use api::units::*;
use crate::batch::{AlphaBatchBuilder, AlphaBatchContainer, BatchTextures, ClipBatcher, resolve_image, BatchBuilder};
use crate::clip::ClipStore;
use crate::clip_scroll_tree::{ClipScrollTree, ROOT_SPATIAL_NODE_INDEX};
use crate::device::{Texture};
use crate::frame_builder::FrameGlobalResources;
use crate::gpu_cache::{GpuCache, GpuCacheAddress};
use crate::gpu_types::{BorderInstance, SvgFilterInstance, BlurDirection, BlurInstance, PrimitiveHeaders, ScalingInstance};
use crate::gpu_types::{TransformPalette, ZBufferIdGenerator};
use crate::internal_types::{CacheTextureId, FastHashMap, LayerIndex, SavedTargetIndex, Swizzle, TextureSource};
use crate::picture::SurfaceInfo;
use crate::prim_store::gradient::GRADIENT_FP_STOPS;
use crate::prim_store::{PrimitiveStore, DeferredResolve, PrimitiveScratchBuffer, PrimitiveVisibilityMask};
use crate::render_backend::{DataStores, FrameId};
use crate::render_task::{BlitSource, RenderTargetKind, RenderTask, RenderTaskKind, RenderTaskAddress, RenderTaskData};
use crate::render_task::{ClearMode, RenderTaskLocation, ScalingTask, SvgFilterInfo};
use crate::resource_cache::ResourceCache;
use crate::texture_allocator::{ArrayAllocationTracker, FreeRectSlice};
use std::{cmp, usize, f32, i32, u32, mem};

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

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct RenderTaskGraph {
    pub tasks: Vec<RenderTask>,
    pub task_data: Vec<RenderTaskData>,
    /// Tasks that don't have dependencies, and that may be shared between
    /// picture tasks.
    ///
    /// We render these unconditionally before-rendering the rest of the tree.
    pub cacheable_render_tasks: Vec<RenderTaskId>,
    next_saved: SavedTargetIndex,
    frame_id: FrameId,
}

impl RenderTaskGraph {
    pub fn new(frame_id: FrameId, counters: &RenderTaskGraphCounters) -> Self {
        // Preallocate a little more than what we needed in the previous frame so that small variations
        // in the number of items don't cause us to constantly reallocate.
        let extra_items = 8;
        RenderTaskGraph {
            tasks: Vec::with_capacity(counters.tasks_len + extra_items),
            task_data: Vec::with_capacity(counters.task_data_len + extra_items),
            cacheable_render_tasks: Vec::with_capacity(counters.cacheable_render_tasks_len + extra_items),
            next_saved: SavedTargetIndex(0),
            frame_id,
        }
    }

    pub fn counters(&self) -> RenderTaskGraphCounters {
        RenderTaskGraphCounters {
            tasks_len: self.tasks.len(),
            task_data_len: self.task_data.len(),
            cacheable_render_tasks_len: self.cacheable_render_tasks.len(),
        }
    }

    pub fn add(&mut self, task: RenderTask) -> RenderTaskId {
        let index = self.tasks.len() as _;
        self.tasks.push(task);
        RenderTaskId {
            index,
            #[cfg(debug_assertions)]
            frame_id: self.frame_id,
        }
    }

    /// Express a render task dependency between a parent and child task.
    /// This is used to assign tasks to render passes.
    pub fn add_dependency(
        &mut self,
        parent_id: RenderTaskId,
        child_id: RenderTaskId,
    ) {
        let parent = &mut self[parent_id];
        parent.children.push(child_id);
    }

    /// Assign this frame's render tasks to render passes ordered so that passes appear
    /// earlier than the ones that depend on them.
    pub fn generate_passes(
        &mut self,
        main_render_task: Option<RenderTaskId>,
        screen_size: DeviceIntSize,
        gpu_supports_fast_clears: bool,
    ) -> Vec<RenderPass> {
        let mut passes = Vec::new();

        if !self.cacheable_render_tasks.is_empty() {
            self.generate_passes_impl(
                &self.cacheable_render_tasks[..],
                screen_size,
                gpu_supports_fast_clears,
                false,
                &mut passes,
            );
        }

        if let Some(main_task) = main_render_task {
            self.generate_passes_impl(
                &[main_task],
                screen_size,
                gpu_supports_fast_clears,
                true,
                &mut passes,
            );
        }


        self.resolve_target_conflicts(&mut passes);

        passes
    }

    /// Assign the render tasks from the tree rooted at root_task to render passes and
    /// append them to the `passes` vector so that the passes that we depend on end up
    /// _earlier_ in the pass list.
    fn generate_passes_impl(
        &self,
        root_tasks: &[RenderTaskId],
        screen_size: DeviceIntSize,
        gpu_supports_fast_clears: bool,
        for_main_framebuffer: bool,
        passes: &mut Vec<RenderPass>,
    ) {
        // We recursively visit tasks from the roots (main and cached render tasks), to figure out
        // which ones affect the frame and which passes they should be assigned to.
        //
        // We track the maximum depth of each task (how far it is from the roots) as well as the total
        // maximum depth of the graph to determine each tasks' pass index. In a nutshell, depth 0 is
        // for the last render pass (for example the main framebuffer), while the highest depth
        // corresponds to the first pass.

        fn assign_task_depth(
            tasks: &[RenderTask],
            task_id: RenderTaskId,
            task_depth: i32,
            task_max_depths: &mut [i32],
            max_depth: &mut i32,
        ) {
            *max_depth = std::cmp::max(*max_depth, task_depth);

            let task_max_depth = &mut task_max_depths[task_id.index as usize];
            if task_depth > *task_max_depth {
                *task_max_depth = task_depth;
            } else {
                // If this task has already been processed at a larger depth,
                // there is no need to process it again.
                return;
            }

            let task = &tasks[task_id.index as usize];
            for child in &task.children {
                assign_task_depth(
                    tasks,
                    *child,
                    task_depth + 1,
                    task_max_depths,
                    max_depth,
                );
            }
        }

        // The maximum depth of each task. Values that are still equal to -1 after recursively visiting
        // the nodes correspond to tasks that don't contribute to the frame.
        let mut task_max_depths = vec![-1; self.tasks.len()];
        let mut max_depth = 0;

        for root_task in root_tasks {
            assign_task_depth(
                &self.tasks,
                *root_task,
                0,
                &mut task_max_depths,
                &mut max_depth,
            );
        }

        let offset = passes.len();

        passes.reserve(max_depth as usize + 1);
        for _ in 0..max_depth {
            passes.push(RenderPass::new_off_screen(screen_size, gpu_supports_fast_clears));
        }

        if for_main_framebuffer {
            passes.push(RenderPass::new_main_framebuffer(screen_size, gpu_supports_fast_clears));
        } else {
            passes.push(RenderPass::new_off_screen(screen_size, gpu_supports_fast_clears));
        }

        // Assign tasks to their render passes.
        for task_index in 0..self.tasks.len() {
            if task_max_depths[task_index] < 0 {
                // The task wasn't visited, it means it doesn't contribute to this frame.
                continue;
            }
            let pass_index = offset + (max_depth - task_max_depths[task_index]) as usize;
            let task_id = RenderTaskId {
                index: task_index as u32,
                #[cfg(debug_assertions)]
                frame_id: self.frame_id,
            };
            let task = &self.tasks[task_index];
            passes[pass_index as usize].add_render_task(
                task_id,
                task.get_dynamic_size(),
                task.target_kind(),
                &task.location,
            );
        }
    }

    /// Resolve conflicts between the generated passes and the limitiations of our target
    /// allocation scheme.
    ///
    /// The render task graph operates with a ping-pong target allocation scheme where
    /// a set of targets is written to by even passes and a different set of targets is
    /// written to by odd passes.
    /// Since tasks cannot read and write the same target, we can run into issues if a
    /// task pass in N + 2 reads the result of a task in pass N.
    /// To avoid such cases have to insert blit tasks to copy the content of the task
    /// into pass N + 1 which is readable by pass N + 2.
    ///
    /// In addition, allocated rects of pass N are currently not tracked and can be
    /// overwritten by allocations in later passes on the same target, unless the task
    /// has been marked for saving, which perserves the allocated rect until the end of
    /// the frame. This is a big hammer, hopefully we won't need to mark many passes
    /// for saving. A better solution would be to track allocations through the entire
    /// graph, there is a prototype of that in https://github.com/nical/toy-render-graph/
    fn resolve_target_conflicts(&mut self, passes: &mut [RenderPass]) {
        // Keep track of blit tasks we inserted to avoid adding several blits for the same
        // task.
        let mut task_redirects = vec![None; self.tasks.len()];

        let mut task_passes = vec![-1; self.tasks.len()];
        for pass_index in 0..passes.len() {
            for task in &passes[pass_index].tasks {
                task_passes[task.index as usize] = pass_index as i32;
            }
        }

        for task_index in 0..self.tasks.len() {
            if task_passes[task_index] < 0 {
                // The task doesn't contribute to this frame.
                continue;
            }

            let pass_index = task_passes[task_index];

            // Go through each dependency and check whether they belong
            // to a pass that uses the same targets and/or are more than
            // one pass behind.
            for nth_child in 0..self.tasks[task_index].children.len() {
                let child_task_index = self.tasks[task_index].children[nth_child].index as usize;
                let child_pass_index = task_passes[child_task_index];

                if child_pass_index == pass_index - 1 {
                    // This should be the most common case.
                    continue;
                }

                // TODO: Picture tasks don't support having their dependency tasks redirected.
                // Pictures store their respective render task(s) on their SurfaceInfo.
                // We cannot blit the picture task here because we would need to update the
                // surface's render tasks, but we don't have access to that info here.
                // Also a surface may be expecting a picture task and not a blit task, so
                // even if we could update the surface's render task(s), it might cause other issues.
                // For now we mark the task to be saved rather than trying to redirect to a blit task.
                let task_is_picture = if let RenderTaskKind::Picture(..) = self.tasks[task_index].kind {
                    true
                } else {
                    false
                };

                if child_pass_index % 2 != pass_index % 2 || task_is_picture {
                    // The tasks and its dependency aren't on the same targets,
                    // but the dependency needs to be kept alive.
                    self.tasks[child_task_index].mark_for_saving();
                    continue;
                }

                if let Some(blit_id) = task_redirects[child_task_index] {
                    // We already resolved a similar conflict with a blit task,
                    // reuse the same blit instead of creating a new one.
                    self.tasks[task_index].children[nth_child] = blit_id;

                    // Mark for saving if the blit is more than pass appart from
                    // our task.
                    if child_pass_index < pass_index - 2 {
                        self.tasks[blit_id.index as usize].mark_for_saving();
                    }

                    continue;
                }

                // Our dependency is an even number of passes behind, need
                // to insert a blit to ensure we don't read and write from
                // the same target.

                let child_task_id = RenderTaskId {
                    index: child_task_index as u32,
                    #[cfg(debug_assertions)]
                    frame_id: self.frame_id,
                };

                let mut blit = RenderTask::new_blit(
                    self.tasks[child_task_index].location.size(),
                    BlitSource::RenderTask { task_id: child_task_id },
                );

                // Mark for saving if the blit is more than pass appart from
                // our task.
                if child_pass_index < pass_index - 2 {
                    blit.mark_for_saving();
                }

                let blit_id = RenderTaskId {
                    index: self.tasks.len() as u32,
                    #[cfg(debug_assertions)]
                    frame_id: self.frame_id,
                };

                self.tasks.push(blit);

                passes[child_pass_index as usize + 1].tasks.push(blit_id);

                self.tasks[task_index].children[nth_child] = blit_id;
                task_redirects[child_task_index] = Some(blit_id);
            }
        }
    }

    pub fn get_task_address(&self, id: RenderTaskId) -> RenderTaskAddress {
        #[cfg(all(debug_assertions, not(feature = "replay")))]
        debug_assert_eq!(self.frame_id, id.frame_id);
        RenderTaskAddress(id.index as u16)
    }

    pub fn write_task_data(&mut self) {
        for task in &self.tasks {
            self.task_data.push(task.write_task_data());
        }
    }

    pub fn save_target(&mut self) -> SavedTargetIndex {
        let id = self.next_saved;
        self.next_saved.0 += 1;
        id
    }

    #[cfg(debug_assertions)]
    pub fn frame_id(&self) -> FrameId {
        self.frame_id
    }
}

impl std::ops::Index<RenderTaskId> for RenderTaskGraph {
    type Output = RenderTask;
    fn index(&self, id: RenderTaskId) -> &RenderTask {
        #[cfg(all(debug_assertions, not(feature = "replay")))]
        debug_assert_eq!(self.frame_id, id.frame_id);
        &self.tasks[id.index as usize]
    }
}

impl std::ops::IndexMut<RenderTaskId> for RenderTaskGraph {
    fn index_mut(&mut self, id: RenderTaskId) -> &mut RenderTask {
        #[cfg(all(debug_assertions, not(feature = "replay")))]
        debug_assert_eq!(self.frame_id, id.frame_id);
        &mut self.tasks[id.index as usize]
    }
}

#[derive(Copy, Clone, PartialEq, Eq, Hash, Debug)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct RenderTaskId {
    pub index: u32,

    #[cfg(debug_assertions)]
    #[cfg_attr(feature = "replay", serde(default = "FrameId::first"))]
    frame_id: FrameId,
}

#[derive(Debug)]
pub struct RenderTaskGraphCounters {
    tasks_len: usize,
    task_data_len: usize,
    cacheable_render_tasks_len: usize,
}

impl RenderTaskGraphCounters {
    pub fn new() -> Self {
        RenderTaskGraphCounters {
            tasks_len: 0,
            task_data_len: 0,
            cacheable_render_tasks_len: 0,
        }
    }
}

impl RenderTaskId {
    pub const INVALID: RenderTaskId = RenderTaskId {
        index: u32::MAX,
        #[cfg(debug_assertions)]
        frame_id: FrameId::INVALID,
    };
}

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
            RenderTaskKind::VerticalBlur(..) => {
                add_blur_instances(
                    &mut self.vertical_blurs,
                    BlurDirection::Vertical,
                    render_tasks.get_task_address(task_id),
                    render_tasks.get_task_address(task.children[0]),
                );
            }
            RenderTaskKind::HorizontalBlur(..) => {
                add_blur_instances(
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
                add_svg_filter_instances(
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
                add_scaling_instances(
                    info,
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
            RenderTaskKind::VerticalBlur(..) => {
                add_blur_instances(
                    &mut self.vertical_blurs,
                    BlurDirection::Vertical,
                    render_tasks.get_task_address(task_id),
                    render_tasks.get_task_address(task.children[0]),
                );
            }
            RenderTaskKind::HorizontalBlur(..) => {
                add_blur_instances(
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
                add_scaling_instances(
                    info,
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
            RenderTaskKind::HorizontalBlur(..) => {
                add_blur_instances(
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

fn add_blur_instances(
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

fn add_scaling_instances(
    task: &ScalingTask,
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
        .inner_rect(task.padding)
        .to_f32();

    let (source, (source_rect, source_layer)) = match task.image {
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
                match task.target_kind {
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

fn add_svg_filter_instances(
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

// Dump an SVG visualization of the render graph for debugging purposes
#[allow(dead_code)]
pub fn dump_render_tasks_as_svg(
    render_tasks: &RenderTaskGraph,
    passes: &[RenderPass],
    output: &mut dyn std::io::Write,
) -> std::io::Result<()> {
    use svg_fmt::*;

    let node_width = 80.0;
    let node_height = 30.0;
    let vertical_spacing = 8.0;
    let horizontal_spacing = 20.0;
    let margin = 10.0;
    let text_size = 10.0;

    let mut pass_rects = Vec::new();
    let mut nodes = vec![None; render_tasks.tasks.len()];

    let mut x = margin;
    let mut max_y: f32 = 0.0;

    #[derive(Clone)]
    struct Node {
        rect: Rectangle,
        label: Text,
        size: Text,
    }

    for pass in passes {
        let mut layout = VerticalLayout::new(x, margin, node_width);

        for task_id in &pass.tasks {
            let task_index = task_id.index as usize;
            let task = &render_tasks.tasks[task_index];

            let rect = layout.push_rectangle(node_height);

            let tx = rect.x + rect.w / 2.0;
            let ty = rect.y + 10.0;

            let saved = if task.saved_index.is_some() { " (Saved)" } else { "" };
            let label = text(tx, ty, format!("{}{}", task.kind.as_str(), saved));
            let size = text(tx, ty + 12.0, format!("{}", task.location.size()));

            nodes[task_index] = Some(Node { rect, label, size });

            layout.advance(vertical_spacing);
        }

        pass_rects.push(layout.total_rectangle());

        x += node_width + horizontal_spacing;
        max_y = max_y.max(layout.y + margin);
    }

    let mut links = Vec::new();
    for node_index in 0..nodes.len() {
        if nodes[node_index].is_none() {
            continue;
        }

        let task = &render_tasks.tasks[node_index];
        for dep in &task.children {
            let dep_index = dep.index as usize;

            if let (&Some(ref node), &Some(ref dep_node)) = (&nodes[node_index], &nodes[dep_index]) {
                links.push((
                    dep_node.rect.x + dep_node.rect.w,
                    dep_node.rect.y + dep_node.rect.h / 2.0,
                    node.rect.x,
                    node.rect.y + node.rect.h / 2.0,
                ));
            }
        }
    }

    let svg_w = x + margin;
    let svg_h = max_y + margin;
    writeln!(output, "{}", BeginSvg { w: svg_w, h: svg_h })?;

    // Background.
    writeln!(output,
        "    {}",
        rectangle(0.0, 0.0, svg_w, svg_h)
            .inflate(1.0, 1.0)
            .fill(rgb(50, 50, 50))
    )?;

    // Passes.
    for rect in pass_rects {
        writeln!(output,
            "    {}",
            rect.inflate(3.0, 3.0)
                .border_radius(4.0)
                .opacity(0.4)
                .fill(black())
        )?;
    }

    // Links.
    for (x1, y1, x2, y2) in links {
        dump_task_dependency_link(output, x1, y1, x2, y2);
    }

    // Tasks.
    for node in &nodes {
        if let Some(node) = node {
            writeln!(output,
                "    {}",
                node.rect
                    .clone()
                    .fill(black())
                    .border_radius(3.0)
                    .opacity(0.5)
                    .offset(0.0, 2.0)
            )?;
            writeln!(output,
                "    {}",
                node.rect
                    .clone()
                    .fill(rgb(200, 200, 200))
                    .border_radius(3.0)
                    .opacity(0.8)
            )?;

            writeln!(output,
                "    {}",
                node.label
                    .clone()
                    .size(text_size)
                    .align(Align::Center)
                    .color(rgb(50, 50, 50))
            )?;
            writeln!(output,
                "    {}",
                node.size
                    .clone()
                    .size(text_size * 0.7)
                    .align(Align::Center)
                    .color(rgb(50, 50, 50))
            )?;
        }
    }

    writeln!(output, "{}", EndSvg)
}

#[allow(dead_code)]
fn dump_task_dependency_link(
    output: &mut dyn std::io::Write,
    x1: f32, y1: f32,
    x2: f32, y2: f32,
) {
    use svg_fmt::*;

    // If the link is a straight horizontal line and spans over multiple passes, it
    // is likely to go straight though unrelated nodes in a way that makes it look like
    // they are connected, so we bend the line upward a bit to avoid that.
    let simple_path = (y1 - y2).abs() > 1.0 || (x2 - x1) < 45.0;

    let mid_x = (x1 + x2) / 2.0;
    if simple_path {
        write!(output, "    {}",
            path().move_to(x1, y1)
                .cubic_bezier_to(mid_x, y1, mid_x, y2, x2, y2)
                .fill(Fill::None)
                .stroke(Stroke::Color(rgb(100, 100, 100), 3.0))
        ).unwrap();
    } else {
        let ctrl1_x = (mid_x + x1) / 2.0;
        let ctrl2_x = (mid_x + x2) / 2.0;
        let ctrl_y = y1 - 25.0;
        write!(output, "    {}",
            path().move_to(x1, y1)
                .cubic_bezier_to(ctrl1_x, y1, ctrl1_x, ctrl_y, mid_x, ctrl_y)
                .cubic_bezier_to(ctrl2_x, ctrl_y, ctrl2_x, y2, x2, y2)
                .fill(Fill::None)
                .stroke(Stroke::Color(rgb(100, 100, 100), 3.0))
        ).unwrap();
    }
}

#[cfg(test)]
use euclid::{size2, rect};

#[cfg(test)]
fn dyn_location(w: i32, h: i32) -> RenderTaskLocation {
    RenderTaskLocation::Dynamic(None, size2(w, h))
}

#[test]
fn diamond_task_graph() {
    // A simple diamon shaped task graph.
    //
    //     [b1]
    //    /    \
    // [a]      [main_pic]
    //    \    /
    //     [b2]

    let color = RenderTargetKind::Color;

    let counters = RenderTaskGraphCounters::new();
    let mut tasks = RenderTaskGraph::new(FrameId::first(), &counters);

    let a = tasks.add(RenderTask::new_test(color, dyn_location(640, 640), Vec::new()));
    let b1 = tasks.add(RenderTask::new_test(color, dyn_location(320, 320), vec![a]));
    let b2 = tasks.add(RenderTask::new_test(color, dyn_location(320, 320), vec![a]));

    let main_pic = tasks.add(RenderTask::new_test(
        color,
        RenderTaskLocation::Fixed(rect(0, 0, 3200, 1800)),
        vec![b1, b2],
    ));

    let initial_number_of_tasks = tasks.tasks.len();

    let passes = tasks.generate_passes(Some(main_pic), size2(3200, 1800), true);

    // We should not have added any blits.
    assert_eq!(tasks.tasks.len(), initial_number_of_tasks);

    assert_eq!(passes.len(), 3);
    assert_eq!(passes[0].tasks, vec![a]);

    assert_eq!(passes[1].tasks.len(), 2);
    assert!(passes[1].tasks.contains(&b1));
    assert!(passes[1].tasks.contains(&b2));

    assert_eq!(passes[2].tasks, vec![main_pic]);
}

#[test]
fn blur_task_graph() {
    // This test simulates a complicated shadow stack effect with target allocation
    // conflicts to resolve.

    let color = RenderTargetKind::Color;

    let counters = RenderTaskGraphCounters::new();
    let mut tasks = RenderTaskGraph::new(FrameId::first(), &counters);

    let pic = tasks.add(RenderTask::new_test(color, dyn_location(640, 640), Vec::new()));
    let scale1 = tasks.add(RenderTask::new_test(color, dyn_location(320, 320), vec![pic]));
    let scale2 = tasks.add(RenderTask::new_test(color, dyn_location(160, 160), vec![scale1]));
    let scale3 = tasks.add(RenderTask::new_test(color, dyn_location(80, 80), vec![scale2]));
    let scale4 = tasks.add(RenderTask::new_test(color, dyn_location(40, 40), vec![scale3]));

    let vblur1 = tasks.add(RenderTask::new_test(color, dyn_location(40, 40), vec![scale4]));
    let hblur1 = tasks.add(RenderTask::new_test(color, dyn_location(40, 40), vec![vblur1]));

    let vblur2 = tasks.add(RenderTask::new_test(color, dyn_location(40, 40), vec![scale4]));
    let hblur2 = tasks.add(RenderTask::new_test(color, dyn_location(40, 40), vec![vblur2]));

    // Insert a task that is an even number of passes away from its dependency.
    // This means the source and destination are on the same target and we have to resolve
    // this conflict by automatically inserting a blit task.
    let vblur3 = tasks.add(RenderTask::new_test(color, dyn_location(80, 80), vec![scale3]));
    let hblur3 = tasks.add(RenderTask::new_test(color, dyn_location(80, 80), vec![vblur3]));

    // Insert a task that is an odd number > 1 of passes away from its dependency.
    // This should force us to mark the dependency "for saving" to keep its content valid
    // until the task can access it.
    let vblur4 = tasks.add(RenderTask::new_test(color, dyn_location(160, 160), vec![scale2]));
    let hblur4 = tasks.add(RenderTask::new_test(color, dyn_location(160, 160), vec![vblur4]));

    let main_pic = tasks.add(RenderTask::new_test(
        color,
        RenderTaskLocation::Fixed(rect(0, 0, 3200, 1800)),
        vec![hblur1, hblur2, hblur3, hblur4],
    ));

    let initial_number_of_tasks = tasks.tasks.len();

    let passes = tasks.generate_passes(Some(main_pic), size2(3200, 1800), true);

    // We should have added a single blit task.
    assert_eq!(tasks.tasks.len(), initial_number_of_tasks + 1);

    // vblur3's dependency to scale3 should be replaced by a blit.
    let blit = tasks[vblur3].children[0];
    assert!(blit != scale3);

    match tasks[blit].kind {
        RenderTaskKind::Blit(..) => {}
        _ => { panic!("This should be a blit task."); }
    }

    assert_eq!(passes.len(), 8);

    assert_eq!(passes[0].tasks, vec![pic]);
    assert_eq!(passes[1].tasks, vec![scale1]);
    assert_eq!(passes[2].tasks, vec![scale2]);
    assert_eq!(passes[3].tasks, vec![scale3]);

    assert_eq!(passes[4].tasks.len(), 2);
    assert!(passes[4].tasks.contains(&scale4));
    assert!(passes[4].tasks.contains(&blit));

    assert_eq!(passes[5].tasks.len(), 4);
    assert!(passes[5].tasks.contains(&vblur1));
    assert!(passes[5].tasks.contains(&vblur2));
    assert!(passes[5].tasks.contains(&vblur3));
    assert!(passes[5].tasks.contains(&vblur4));

    assert_eq!(passes[6].tasks.len(), 4);
    assert!(passes[6].tasks.contains(&hblur1));
    assert!(passes[6].tasks.contains(&hblur2));
    assert!(passes[6].tasks.contains(&hblur3));
    assert!(passes[6].tasks.contains(&hblur4));

    assert_eq!(passes[7].tasks, vec![main_pic]);

    // See vblur4's comment above.
    assert!(tasks[scale2].saved_index.is_some());
}

#[test]
fn culled_tasks() {
    // This test checks that tasks that do not contribute to the frame don't appear in the
    // generated passes.

    let color = RenderTargetKind::Color;

    let counters = RenderTaskGraphCounters::new();
    let mut tasks = RenderTaskGraph::new(FrameId::first(), &counters);

    let a1 = tasks.add(RenderTask::new_test(color, dyn_location(640, 640), Vec::new()));
    let _a2 = tasks.add(RenderTask::new_test(color, dyn_location(320, 320), vec![a1]));

    let b1 = tasks.add(RenderTask::new_test(color, dyn_location(640, 640), Vec::new()));
    let b2 = tasks.add(RenderTask::new_test(color, dyn_location(320, 320), vec![b1]));
    let _b3 = tasks.add(RenderTask::new_test(color, dyn_location(320, 320), vec![b2]));

    let main_pic = tasks.add(RenderTask::new_test(
        color,
        RenderTaskLocation::Fixed(rect(0, 0, 3200, 1800)),
        vec![b2],
    ));

    let initial_number_of_tasks = tasks.tasks.len();

    let passes = tasks.generate_passes(Some(main_pic), size2(3200, 1800), true);

    // We should not have added any blits.
    assert_eq!(tasks.tasks.len(), initial_number_of_tasks);

    assert_eq!(passes.len(), 3);
    assert_eq!(passes[0].tasks, vec![b1]);
    assert_eq!(passes[1].tasks, vec![b2]);
    assert_eq!(passes[2].tasks, vec![main_pic]);
}
