/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{ImageDescriptor, FilterPrimitive, FilterPrimitiveInput, FilterPrimitiveKind};
use api::{LineStyle, LineOrientation, ClipMode, DirtyRect, MixBlendMode, ColorF, ColorSpace};
use api::units::*;
use crate::border::BorderSegmentCacheKey;
use crate::box_shadow::{BoxShadowCacheKey};
use crate::clip::{ClipDataStore, ClipItem, ClipStore, ClipNodeRange, ClipNodeFlags};
use crate::clip_scroll_tree::SpatialNodeIndex;
use crate::device::TextureFilter;
use crate::filterdata::SFilterData;
use crate::frame_builder::FrameBuilderConfig;
use crate::freelist::{FreeList, FreeListHandle, WeakFreeListHandle};
use crate::gpu_cache::{GpuCache, GpuCacheAddress, GpuCacheHandle};
use crate::gpu_types::{BorderInstance, ImageSource, UvRectKind, SnapOffsets};
use crate::internal_types::{CacheTextureId, FastHashMap, LayerIndex, SavedTargetIndex, TextureSource};
use crate::prim_store::{PictureIndex, PrimitiveVisibilityMask};
use crate::prim_store::image::ImageCacheKey;
use crate::prim_store::gradient::{GRADIENT_FP_STOPS, GradientCacheKey, GradientStopKey};
use crate::prim_store::line_dec::LineDecorationCacheKey;
#[cfg(feature = "debugger")]
use crate::print_tree::{PrintTreePrinter};
use crate::render_backend::FrameId;
use crate::resource_cache::{CacheItem, ResourceCache};
use std::{ops, mem, usize, f32, i32, u32};
use crate::texture_cache::{TextureCache, TextureCacheHandle, Eviction};
use crate::tiling::{RenderPass, RenderTargetIndex};
use std::io;


const RENDER_TASK_SIZE_SANITY_CHECK: i32 = 16000;
const FLOATS_PER_RENDER_TASK_INFO: usize = 8;
pub const MAX_BLUR_STD_DEVIATION: f32 = 4.0;
pub const MIN_DOWNSCALING_RT_SIZE: i32 = 8;

fn render_task_sanity_check(size: &DeviceIntSize) {
    if size.width > RENDER_TASK_SIZE_SANITY_CHECK ||
        size.height > RENDER_TASK_SIZE_SANITY_CHECK {
        error!("Attempting to create a render task of size {}x{}", size.width, size.height);
        panic!();
    }
}

/// A tag used to identify the output format of a `RenderTarget`.
#[derive(Debug, Copy, Clone, Eq, PartialEq, Hash)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub enum RenderTargetKind {
    Color, // RGBA8
    Alpha, // R8
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

impl RenderTaskId {
    pub const INVALID: RenderTaskId = RenderTaskId {
        index: u32::MAX,
        #[cfg(debug_assertions)]
        frame_id: FrameId::INVALID,
    };
}

#[derive(Debug, Copy, Clone, PartialEq)]
#[repr(C)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct RenderTaskAddress(pub u16);

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

impl ops::Index<RenderTaskId> for RenderTaskGraph {
    type Output = RenderTask;
    fn index(&self, id: RenderTaskId) -> &RenderTask {
        #[cfg(all(debug_assertions, not(feature = "replay")))]
        debug_assert_eq!(self.frame_id, id.frame_id);
        &self.tasks[id.index as usize]
    }
}

impl ops::IndexMut<RenderTaskId> for RenderTaskGraph {
    fn index_mut(&mut self, id: RenderTaskId) -> &mut RenderTask {
        #[cfg(all(debug_assertions, not(feature = "replay")))]
        debug_assert_eq!(self.frame_id, id.frame_id);
        &mut self.tasks[id.index as usize]
    }
}

/// Identifies the output buffer location for a given `RenderTask`.
#[derive(Clone, Debug)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub enum RenderTaskLocation {
    /// The `RenderTask` should be drawn to a fixed region in a specific render
    /// target. This is used for the root `RenderTask`, where the main
    /// framebuffer is used as the render target.
    Fixed(DeviceIntRect),
    /// The `RenderTask` should be drawn to a target provided by the atlas
    /// allocator. This is the most common case.
    ///
    /// The second member specifies the width and height of the task
    /// output, and the first member is initially left as `None`. During the
    /// build phase, we invoke `RenderTargetList::alloc()` and store the
    /// resulting location in the first member. That location identifies the
    /// render target and the offset of the allocated region within that target.
    Dynamic(Option<(DeviceIntPoint, RenderTargetIndex)>, DeviceIntSize),
    /// The output of the `RenderTask` will be persisted beyond this frame, and
    /// thus should be drawn into the `TextureCache`.
    TextureCache {
        /// Which texture in the texture cache should be drawn into.
        texture: CacheTextureId,
        /// The target layer in the above texture.
        layer: LayerIndex,
        /// The target region within the above layer.
        rect: DeviceIntRect,

    },
    /// This render task will be drawn to a picture cache texture that is
    /// persisted between both frames and scenes, if the content remains valid.
    PictureCache {
        /// The texture ID to draw to.
        texture: TextureSource,
        /// Slice index in the texture array to draw to.
        layer: i32,
        /// Size in device pixels of this picture cache tile.
        size: DeviceIntSize,
    },
}

impl RenderTaskLocation {
    /// Returns true if this is a dynamic location.
    pub fn is_dynamic(&self) -> bool {
        match *self {
            RenderTaskLocation::Dynamic(..) => true,
            _ => false,
        }
    }

    pub fn size(&self) -> DeviceIntSize {
        match self {
            RenderTaskLocation::Fixed(rect) => rect.size,
            RenderTaskLocation::Dynamic(_, size) => *size,
            RenderTaskLocation::TextureCache { rect, .. } => rect.size,
            RenderTaskLocation::PictureCache { size, .. } => *size,
        }
    }

    pub fn to_source_rect(&self) -> (DeviceIntRect, LayerIndex) {
        match *self {
            RenderTaskLocation::Fixed(rect) => (rect, 0),
            RenderTaskLocation::Dynamic(None, _) => panic!("Expected position to be set for the task!"),
            RenderTaskLocation::Dynamic(Some((origin, layer)), size) => (DeviceIntRect::new(origin, size), layer.0 as LayerIndex),
            RenderTaskLocation::TextureCache { rect, layer, .. } => (rect, layer),
            RenderTaskLocation::PictureCache { layer, size, .. } => (size.into(), layer as LayerIndex),
        }
    }
}

#[derive(Debug)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct CacheMaskTask {
    pub actual_rect: DeviceIntRect,
    pub root_spatial_node_index: SpatialNodeIndex,
    pub clip_node_range: ClipNodeRange,
    pub snap_offsets: SnapOffsets,
    pub device_pixel_scale: DevicePixelScale,
}

#[derive(Debug)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct ClipRegionTask {
    pub clip_data_address: GpuCacheAddress,
    pub local_pos: LayoutPoint,
    pub device_pixel_scale: DevicePixelScale,
}

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct PictureTask {
    pub pic_index: PictureIndex,
    pub can_merge: bool,
    pub content_origin: DeviceIntPoint,
    pub uv_rect_handle: GpuCacheHandle,
    pub surface_spatial_node_index: SpatialNodeIndex,
    uv_rect_kind: UvRectKind,
    device_pixel_scale: DevicePixelScale,
    /// A bitfield that describes which dirty regions should be included
    /// in batches built for this picture task.
    pub vis_mask: PrimitiveVisibilityMask,
}

#[derive(Debug)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct BlurTask {
    pub blur_std_deviation: f32,
    pub target_kind: RenderTargetKind,
    pub uv_rect_handle: GpuCacheHandle,
    uv_rect_kind: UvRectKind,
}

impl BlurTask {
    #[cfg(feature = "debugger")]
    fn print_with<T: PrintTreePrinter>(&self, pt: &mut T) {
        pt.add_item(format!("std deviation: {}", self.blur_std_deviation));
        pt.add_item(format!("target: {:?}", self.target_kind));
    }
}

#[derive(Debug)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct ScalingTask {
    pub target_kind: RenderTargetKind,
    pub image: Option<ImageCacheKey>,
    uv_rect_kind: UvRectKind,
    pub padding: DeviceIntSideOffsets,
}

// Where the source data for a blit task can be found.
#[derive(Debug)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub enum BlitSource {
    Image {
        key: ImageCacheKey,
    },
    RenderTask {
        task_id: RenderTaskId,
    },
}

#[derive(Debug)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct BorderTask {
    pub instances: Vec<BorderInstance>,
}

#[derive(Debug)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct BlitTask {
    pub source: BlitSource,
    pub padding: DeviceIntSideOffsets,
}

#[derive(Debug)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct GradientTask {
    pub stops: [GradientStopKey; GRADIENT_FP_STOPS],
    pub orientation: LineOrientation,
    pub start_point: f32,
    pub end_point: f32,
}

#[derive(Debug)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct LineDecorationTask {
    pub wavy_line_thickness: f32,
    pub style: LineStyle,
    pub orientation: LineOrientation,
    pub local_size: LayoutSize,
}

#[derive(Debug)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub enum SvgFilterInfo {
    Blend(MixBlendMode),
    Flood(ColorF),
    LinearToSrgb,
    SrgbToLinear,
    Opacity(f32),
    ColorMatrix(Box<[f32; 20]>),
    DropShadow(ColorF),
    Offset(DeviceVector2D),
    ComponentTransfer(SFilterData),
    // TODO: This is used as a hack to ensure that a blur task's input is always in the blur's previous pass.
    Identity,
}

#[derive(Debug)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct SvgFilterTask {
    pub info: SvgFilterInfo,
    pub extra_gpu_cache_handle: Option<GpuCacheHandle>,
    pub uv_rect_handle: GpuCacheHandle,
    uv_rect_kind: UvRectKind,
}

#[derive(Debug)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct RenderTaskData {
    pub data: [f32; FLOATS_PER_RENDER_TASK_INFO],
}

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub enum RenderTaskKind {
    Picture(PictureTask),
    CacheMask(CacheMaskTask),
    ClipRegion(ClipRegionTask),
    VerticalBlur(BlurTask),
    HorizontalBlur(BlurTask),
    Readback(DeviceIntRect),
    Scaling(ScalingTask),
    Blit(BlitTask),
    Border(BorderTask),
    LineDecoration(LineDecorationTask),
    Gradient(GradientTask),
    SvgFilter(SvgFilterTask),
    #[cfg(test)]
    Test(RenderTargetKind),
}

impl RenderTaskKind {
    pub fn as_str(&self) -> &'static str {
        match *self {
            RenderTaskKind::Picture(..) => "Picture",
            RenderTaskKind::CacheMask(..) => "CacheMask",
            RenderTaskKind::ClipRegion(..) => "ClipRegion",
            RenderTaskKind::VerticalBlur(..) => "VerticalBlur",
            RenderTaskKind::HorizontalBlur(..) => "HorizontalBlur",
            RenderTaskKind::Readback(..) => "Readback",
            RenderTaskKind::Scaling(..) => "Scaling",
            RenderTaskKind::Blit(..) => "Blit",
            RenderTaskKind::Border(..) => "Border",
            RenderTaskKind::LineDecoration(..) => "LineDecoration",
            RenderTaskKind::Gradient(..) => "Gradient",
            RenderTaskKind::SvgFilter(..) => "SvgFilter",
            #[cfg(test)]
            RenderTaskKind::Test(..) => "Test",
        }
    }
}

#[derive(Debug, Copy, Clone, PartialEq)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub enum ClearMode {
    // Applicable to color and alpha targets.
    Zero,
    One,
    /// This task doesn't care what it is cleared to - it will completely overwrite it.
    DontCare,

    // Applicable to color targets only.
    Transparent,
}

/// In order to avoid duplicating the down-scaling and blur passes when a picture has several blurs,
/// we use a local (primitive-level) cache of the render tasks generated for a single shadowed primitive
/// in a single frame.
pub type BlurTaskCache = FastHashMap<BlurTaskKey, RenderTaskId>;

/// Since we only use it within a single primitive, the key only needs to contain the down-scaling level
/// and the blur std deviation.
#[derive(Copy, Clone, Debug, PartialEq, Eq, Hash)]
pub enum BlurTaskKey {
    DownScale(u32),
    Blur { downscale_level: u32, stddev_x: u32, stddev_y: u32 },
}

impl BlurTaskKey {
    fn downscale_and_blur(downscale_level: u32, blur_stddev: DeviceSize) -> Self {
        // Quantise the std deviations and store it as integers to work around
        // Eq and Hash's f32 allergy.
        // The blur radius is rounded before RenderTask::new_blur so we don't need
        // a lot of precision.
        const QUANTIZATION_FACTOR: f32 = 1024.0;
        let stddev_x = (blur_stddev.width * QUANTIZATION_FACTOR) as u32;
        let stddev_y = (blur_stddev.height * QUANTIZATION_FACTOR) as u32;
        BlurTaskKey::Blur { downscale_level, stddev_x, stddev_y }
    }
}

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct RenderTask {
    pub location: RenderTaskLocation,
    pub children: Vec<RenderTaskId>,
    pub kind: RenderTaskKind,
    pub clear_mode: ClearMode,
    pub saved_index: Option<SavedTargetIndex>,
}

impl RenderTask {
    #[inline]
    pub fn with_dynamic_location(
        size: DeviceIntSize,
        children: Vec<RenderTaskId>,
        kind: RenderTaskKind,
        clear_mode: ClearMode,
    ) -> Self {
        render_task_sanity_check(&size);

        RenderTask {
            location: RenderTaskLocation::Dynamic(None, size),
            children,
            kind,
            clear_mode,
            saved_index: None,
        }
    }

    #[cfg(test)]
    pub fn new_test(
        target: RenderTargetKind,
        location: RenderTaskLocation,
        children: Vec<RenderTaskId>,
    ) -> Self {
        RenderTask {
            location,
            children,
            kind: RenderTaskKind::Test(target),
            clear_mode: ClearMode::Transparent,
            saved_index: None,
        }
    }

    pub fn new_picture(
        location: RenderTaskLocation,
        unclipped_size: DeviceSize,
        pic_index: PictureIndex,
        content_origin: DeviceIntPoint,
        uv_rect_kind: UvRectKind,
        surface_spatial_node_index: SpatialNodeIndex,
        device_pixel_scale: DevicePixelScale,
        vis_mask: PrimitiveVisibilityMask,
    ) -> Self {
        let size = match location {
            RenderTaskLocation::Dynamic(_, size) => size,
            RenderTaskLocation::Fixed(rect) => rect.size,
            RenderTaskLocation::TextureCache { rect, .. } => rect.size,
            RenderTaskLocation::PictureCache { size, .. } => size,
        };

        render_task_sanity_check(&size);

        let can_merge = size.width as f32 >= unclipped_size.width &&
                        size.height as f32 >= unclipped_size.height;

        RenderTask {
            location,
            children: Vec::new(),
            kind: RenderTaskKind::Picture(PictureTask {
                pic_index,
                content_origin,
                can_merge,
                uv_rect_handle: GpuCacheHandle::new(),
                uv_rect_kind,
                surface_spatial_node_index,
                device_pixel_scale,
                vis_mask,
            }),
            clear_mode: ClearMode::Transparent,
            saved_index: None,
        }
    }

    pub fn new_gradient(
        size: DeviceIntSize,
        stops: [GradientStopKey; GRADIENT_FP_STOPS],
        orientation: LineOrientation,
        start_point: f32,
        end_point: f32,
    ) -> Self {
        RenderTask::with_dynamic_location(
            size,
            Vec::new(),
            RenderTaskKind::Gradient(GradientTask {
                stops,
                orientation,
                start_point,
                end_point,
            }),
            ClearMode::DontCare,
        )
    }

    pub fn new_readback(screen_rect: DeviceIntRect) -> Self {
        RenderTask::with_dynamic_location(
            screen_rect.size,
            Vec::new(),
            RenderTaskKind::Readback(screen_rect),
            ClearMode::Transparent,
        )
    }

    pub fn new_blit(
        size: DeviceIntSize,
        source: BlitSource,
    ) -> Self {
        RenderTask::new_blit_with_padding(size, DeviceIntSideOffsets::zero(), source)
    }

    pub fn new_blit_with_padding(
        padded_size: DeviceIntSize,
        padding: DeviceIntSideOffsets,
        source: BlitSource,
    ) -> Self {
        // If this blit uses a render task as a source,
        // ensure it's added as a child task. This will
        // ensure it gets allocated in the correct pass
        // and made available as an input when this task
        // executes.
        let children = match source {
            BlitSource::RenderTask { task_id } => vec![task_id],
            BlitSource::Image { .. } => vec![],
        };

        RenderTask::with_dynamic_location(
            padded_size,
            children,
            RenderTaskKind::Blit(BlitTask {
                source,
                padding,
            }),
            ClearMode::Transparent,
        )
    }

    pub fn new_line_decoration(
        size: DeviceIntSize,
        style: LineStyle,
        orientation: LineOrientation,
        wavy_line_thickness: f32,
        local_size: LayoutSize,
    ) -> Self {
        RenderTask::with_dynamic_location(
            size,
            Vec::new(),
            RenderTaskKind::LineDecoration(LineDecorationTask {
                style,
                orientation,
                wavy_line_thickness,
                local_size,
            }),
            ClearMode::Transparent,
        )
    }

    pub fn new_mask(
        outer_rect: DeviceIntRect,
        clip_node_range: ClipNodeRange,
        root_spatial_node_index: SpatialNodeIndex,
        clip_store: &mut ClipStore,
        gpu_cache: &mut GpuCache,
        resource_cache: &mut ResourceCache,
        render_tasks: &mut RenderTaskGraph,
        clip_data_store: &mut ClipDataStore,
        snap_offsets: SnapOffsets,
        device_pixel_scale: DevicePixelScale,
        fb_config: &FrameBuilderConfig,
    ) -> Self {
        // Step through the clip sources that make up this mask. If we find
        // any box-shadow clip sources, request that image from the render
        // task cache. This allows the blurred box-shadow rect to be cached
        // in the texture cache across frames.
        // TODO(gw): Consider moving this logic outside this function, especially
        //           as we add more clip sources that depend on render tasks.
        // TODO(gw): If this ever shows up in a profile, we could pre-calculate
        //           whether a ClipSources contains any box-shadows and skip
        //           this iteration for the majority of cases.
        let mut needs_clear = fb_config.gpu_supports_fast_clears;

        for i in 0 .. clip_node_range.count {
            let clip_instance = clip_store.get_instance_from_range(&clip_node_range, i);
            let clip_node = &mut clip_data_store[clip_instance.handle];
            match clip_node.item {
                ClipItem::BoxShadow(ref mut info) => {
                    let (cache_size, cache_key) = info.cache_key
                        .as_ref()
                        .expect("bug: no cache key set")
                        .clone();
                    let blur_radius_dp = cache_key.blur_radius_dp as f32;
                    let clip_data_address = gpu_cache.get_address(&info.clip_data_handle);

                    // Request a cacheable render task with a blurred, minimal
                    // sized box-shadow rect.
                    info.cache_handle = Some(resource_cache.request_render_task(
                        RenderTaskCacheKey {
                            size: cache_size,
                            kind: RenderTaskCacheKeyKind::BoxShadow(cache_key),
                        },
                        gpu_cache,
                        render_tasks,
                        None,
                        false,
                        |render_tasks| {
                            // Draw the rounded rect.
                            let mask_task = RenderTask::new_rounded_rect_mask(
                                cache_size,
                                clip_data_address,
                                info.minimal_shadow_rect.origin,
                                device_pixel_scale,
                                fb_config,
                            );

                            let mask_task_id = render_tasks.add(mask_task);

                            // Blur it
                            RenderTask::new_blur(
                                DeviceSize::new(blur_radius_dp, blur_radius_dp),
                                mask_task_id,
                                render_tasks,
                                RenderTargetKind::Alpha,
                                ClearMode::Zero,
                                None,
                            )
                        }
                    ));
                }
                ClipItem::Rectangle(_, ClipMode::Clip) => {
                    if !clip_instance.flags.contains(ClipNodeFlags::SAME_COORD_SYSTEM) {
                        // This is conservative - it's only the case that we actually need
                        // a clear here if we end up adding this mask via add_tiled_clip_mask,
                        // but for simplicity we will just clear if any of these are encountered,
                        // since they are rare.
                        needs_clear = true;
                    }
                }
                ClipItem::Rectangle(_, ClipMode::ClipOut) |
                ClipItem::RoundedRectangle(..) |
                ClipItem::Image { .. } => {}
            }
        }

        // If we have a potentially tiled clip mask, clear the mask area first. Otherwise,
        // the first (primary) clip mask will overwrite all the clip mask pixels with
        // blending disabled to set to the initial value.
        let clear_mode = if needs_clear {
            ClearMode::One
        } else {
            ClearMode::DontCare
        };

        RenderTask::with_dynamic_location(
            outer_rect.size,
            vec![],
            RenderTaskKind::CacheMask(CacheMaskTask {
                actual_rect: outer_rect,
                clip_node_range,
                root_spatial_node_index,
                snap_offsets,
                device_pixel_scale,
            }),
            clear_mode,
        )
    }

    pub fn new_rounded_rect_mask(
        size: DeviceIntSize,
        clip_data_address: GpuCacheAddress,
        local_pos: LayoutPoint,
        device_pixel_scale: DevicePixelScale,
        fb_config: &FrameBuilderConfig,
    ) -> Self {
        let clear_mode = if fb_config.gpu_supports_fast_clears {
            ClearMode::One
        } else {
            ClearMode::DontCare
        };

        RenderTask::with_dynamic_location(
            size,
            Vec::new(),
            RenderTaskKind::ClipRegion(ClipRegionTask {
                clip_data_address,
                local_pos,
                device_pixel_scale,
            }),
            clear_mode,
        )
    }

    // In order to do the blur down-scaling passes without introducing errors, we need the
    // source of each down-scale pass to be a multuple of two. If need be, this inflates
    // the source size so that each down-scale pass will sample correctly.
    pub fn adjusted_blur_source_size(original_size: DeviceIntSize, mut std_dev: DeviceSize) -> DeviceIntSize {
        let mut adjusted_size = original_size;
        let mut scale_factor = 1.0;
        while std_dev.width > MAX_BLUR_STD_DEVIATION && std_dev.height > MAX_BLUR_STD_DEVIATION {
            if adjusted_size.width < MIN_DOWNSCALING_RT_SIZE ||
               adjusted_size.height < MIN_DOWNSCALING_RT_SIZE {
                break;
            }
            std_dev = std_dev * 0.5;
            scale_factor *= 2.0;
            adjusted_size = (original_size.to_f32() / scale_factor).ceil().to_i32();
        }

        adjusted_size * scale_factor as i32
    }

    // Construct a render task to apply a blur to a primitive.
    // The render task chain that is constructed looks like:
    //
    //    PrimitiveCacheTask: Draw the primitives.
    //           ^
    //           |
    //    DownscalingTask(s): Each downscaling task reduces the size of render target to
    //           ^            half. Also reduce the std deviation to half until the std
    //           |            deviation less than 4.0.
    //           |
    //           |
    //    VerticalBlurTask: Apply the separable vertical blur to the primitive.
    //           ^
    //           |
    //    HorizontalBlurTask: Apply the separable horizontal blur to the vertical blur.
    //           |
    //           +---- This is stored as the input task to the primitive shader.
    //
    pub fn new_blur(
        blur_std_deviation: DeviceSize,
        src_task_id: RenderTaskId,
        render_tasks: &mut RenderTaskGraph,
        target_kind: RenderTargetKind,
        clear_mode: ClearMode,
        mut blur_cache: Option<&mut BlurTaskCache>,
    ) -> RenderTaskId {
        // Adjust large std deviation value.
        let mut adjusted_blur_std_deviation = blur_std_deviation;
        let (blur_target_size, uv_rect_kind) = {
            let src_task = &render_tasks[src_task_id];
            (src_task.get_dynamic_size(), src_task.uv_rect_kind())
        };
        let mut adjusted_blur_target_size = blur_target_size;
        let mut downscaling_src_task_id = src_task_id;
        let mut scale_factor = 1.0;
        let mut n_downscales = 1;
        while adjusted_blur_std_deviation.width > MAX_BLUR_STD_DEVIATION &&
              adjusted_blur_std_deviation.height > MAX_BLUR_STD_DEVIATION {
            if adjusted_blur_target_size.width < MIN_DOWNSCALING_RT_SIZE ||
               adjusted_blur_target_size.height < MIN_DOWNSCALING_RT_SIZE {
                break;
            }
            adjusted_blur_std_deviation = adjusted_blur_std_deviation * 0.5;
            scale_factor *= 2.0;
            adjusted_blur_target_size = (blur_target_size.to_f32() / scale_factor).to_i32();

            let cached_task = match blur_cache {
                Some(ref mut cache) => cache.get(&BlurTaskKey::DownScale(n_downscales)).cloned(),
                None => None,
            };

            downscaling_src_task_id = cached_task.unwrap_or_else(|| {
                let downscaling_task = RenderTask::new_scaling(
                    downscaling_src_task_id,
                    render_tasks,
                    target_kind,
                    adjusted_blur_target_size,
                );
                render_tasks.add(downscaling_task)
            });

            if let Some(ref mut cache) = blur_cache {
                cache.insert(BlurTaskKey::DownScale(n_downscales), downscaling_src_task_id);
            }

            n_downscales += 1;
        }


        let blur_key = BlurTaskKey::downscale_and_blur(n_downscales, adjusted_blur_std_deviation);

        let cached_task = match blur_cache {
            Some(ref mut cache) => cache.get(&blur_key).cloned(),
            None => None,
        };

        let blur_task_id = cached_task.unwrap_or_else(|| {
            let blur_task_v = RenderTask::with_dynamic_location(
                adjusted_blur_target_size,
                vec![downscaling_src_task_id],
                RenderTaskKind::VerticalBlur(BlurTask {
                    blur_std_deviation: adjusted_blur_std_deviation.height,
                    target_kind,
                    uv_rect_handle: GpuCacheHandle::new(),
                    uv_rect_kind,
                }),
                clear_mode,
            );

            let blur_task_v_id = render_tasks.add(blur_task_v);

            let blur_task_h = RenderTask::with_dynamic_location(
                adjusted_blur_target_size,
                vec![blur_task_v_id],
                RenderTaskKind::HorizontalBlur(BlurTask {
                    blur_std_deviation: adjusted_blur_std_deviation.width,
                    target_kind,
                    uv_rect_handle: GpuCacheHandle::new(),
                    uv_rect_kind,
                }),
                clear_mode,
            );

            render_tasks.add(blur_task_h)
        });

        if let Some(ref mut cache) = blur_cache {
            cache.insert(blur_key, blur_task_id);
        }

        blur_task_id
    }

    pub fn new_border_segment(
        size: DeviceIntSize,
        instances: Vec<BorderInstance>,
    ) -> Self {
        RenderTask::with_dynamic_location(
            size,
            Vec::new(),
            RenderTaskKind::Border(BorderTask {
                instances,
            }),
            ClearMode::Transparent,
        )
    }

    pub fn new_scaling(
        src_task_id: RenderTaskId,
        render_tasks: &mut RenderTaskGraph,
        target_kind: RenderTargetKind,
        size: DeviceIntSize,
    ) -> Self {
        Self::new_scaling_with_padding(
            BlitSource::RenderTask { task_id: src_task_id },
            render_tasks,
            target_kind,
            size,
            DeviceIntSideOffsets::zero(),
        )
    }

    pub fn new_scaling_with_padding(
        source: BlitSource,
        render_tasks: &mut RenderTaskGraph,
        target_kind: RenderTargetKind,
        padded_size: DeviceIntSize,
        padding: DeviceIntSideOffsets,
    ) -> Self {
        let (uv_rect_kind, children, image) = match source {
            BlitSource::RenderTask { task_id } => (render_tasks[task_id].uv_rect_kind(), vec![task_id], None),
            BlitSource::Image { key } => (UvRectKind::Rect, vec![], Some(key)),
        };

        RenderTask::with_dynamic_location(
            padded_size,
            children,
            RenderTaskKind::Scaling(ScalingTask {
                target_kind,
                image,
                uv_rect_kind,
                padding,
            }),
            ClearMode::DontCare,
        )
    }

    pub fn new_svg_filter(
        filter_primitives: &[FilterPrimitive],
        filter_datas: &[SFilterData],
        render_tasks: &mut RenderTaskGraph,
        content_size: DeviceIntSize,
        uv_rect_kind: UvRectKind,
        original_task_id: RenderTaskId,
        device_pixel_scale: DevicePixelScale,
    ) -> RenderTaskId {

        if filter_primitives.is_empty() {
            return original_task_id;
        }

        // Resolves the input to a filter primitive
        let get_task_input = |
            input: &FilterPrimitiveInput,
            filter_primitives: &[FilterPrimitive],
            render_tasks: &mut RenderTaskGraph,
            cur_index: usize,
            outputs: &[RenderTaskId],
            original: RenderTaskId,
            color_space: ColorSpace,
        | {
            // TODO(cbrewster): Not sure we can assume that the original input is sRGB.
            let (mut task_id, input_color_space) = match input.to_index(cur_index) {
                Some(index) => (outputs[index], filter_primitives[index].color_space),
                None => (original, ColorSpace::Srgb),
            };

            match (input_color_space, color_space) {
                (ColorSpace::Srgb, ColorSpace::LinearRgb) => {
                    let task = RenderTask::new_svg_filter_primitive(
                        vec![task_id],
                        content_size,
                        uv_rect_kind,
                        SvgFilterInfo::SrgbToLinear,
                    );
                    task_id = render_tasks.add(task);
                },
                (ColorSpace::LinearRgb, ColorSpace::Srgb) => {
                    let task = RenderTask::new_svg_filter_primitive(
                        vec![task_id],
                        content_size,
                        uv_rect_kind,
                        SvgFilterInfo::LinearToSrgb,
                    );
                    task_id = render_tasks.add(task);
                },
                _ => {},
            }

            task_id
        };

        let mut outputs = vec![];
        let mut cur_filter_data = 0;
        for (cur_index, primitive) in filter_primitives.iter().enumerate() {
            let render_task_id = match primitive.kind {
                FilterPrimitiveKind::Identity(ref identity) => {
                    // Identity does not create a task, it provides its input's render task
                    get_task_input(
                        &identity.input,
                        filter_primitives,
                        render_tasks,
                        cur_index,
                        &outputs,
                        original_task_id,
                        primitive.color_space
                    )
                }
                FilterPrimitiveKind::Blend(ref blend) => {
                    let input_1_task_id = get_task_input(
                        &blend.input1,
                        filter_primitives,
                        render_tasks,
                        cur_index,
                        &outputs,
                        original_task_id,
                        primitive.color_space
                    );
                    let input_2_task_id = get_task_input(
                        &blend.input2,
                        filter_primitives,
                        render_tasks,
                        cur_index,
                        &outputs,
                        original_task_id,
                        primitive.color_space
                    );

                    let task = RenderTask::new_svg_filter_primitive(
                        vec![input_1_task_id, input_2_task_id],
                        content_size,
                        uv_rect_kind,
                        SvgFilterInfo::Blend(blend.mode),
                    );
                    render_tasks.add(task)
                },
                FilterPrimitiveKind::Flood(ref flood) => {
                    let task = RenderTask::new_svg_filter_primitive(
                        vec![],
                        content_size,
                        uv_rect_kind,
                        SvgFilterInfo::Flood(flood.color),
                    );
                    render_tasks.add(task)
                }
                FilterPrimitiveKind::Blur(ref blur) => {
                    let blur_std_deviation = blur.radius * device_pixel_scale.0;
                    let input_task_id = get_task_input(
                        &blur.input,
                        filter_primitives,
                        render_tasks,
                        cur_index,
                        &outputs,
                        original_task_id,
                        primitive.color_space
                    );

                    // TODO: This is a hack to ensure that a blur task's input is always in the blur's previous pass.
                    let svg_task = RenderTask::new_svg_filter_primitive(
                        vec![input_task_id],
                        content_size,
                        uv_rect_kind,
                        SvgFilterInfo::Identity,
                    );

                    RenderTask::new_blur(
                        DeviceSize::new(blur_std_deviation, blur_std_deviation),
                        render_tasks.add(svg_task),
                        render_tasks,
                        RenderTargetKind::Color,
                        ClearMode::Transparent,
                        None,
                    )
                }
                FilterPrimitiveKind::Opacity(ref opacity) => {
                    let input_task_id = get_task_input(
                        &opacity.input,
                        filter_primitives,
                        render_tasks,
                        cur_index,
                        &outputs,
                        original_task_id,
                        primitive.color_space
                    );

                    let task = RenderTask::new_svg_filter_primitive(
                        vec![input_task_id],
                        content_size,
                        uv_rect_kind,
                        SvgFilterInfo::Opacity(opacity.opacity),
                    );
                    render_tasks.add(task)
                }
                FilterPrimitiveKind::ColorMatrix(ref color_matrix) => {
                    let input_task_id = get_task_input(
                        &color_matrix.input,
                        filter_primitives,
                        render_tasks,
                        cur_index,
                        &outputs,
                        original_task_id,
                        primitive.color_space
                    );

                    let task = RenderTask::new_svg_filter_primitive(
                        vec![input_task_id],
                        content_size,
                        uv_rect_kind,
                        SvgFilterInfo::ColorMatrix(Box::new(color_matrix.matrix)),
                    );
                    render_tasks.add(task)
                }
                FilterPrimitiveKind::DropShadow(ref drop_shadow) => {
                    let input_task_id = get_task_input(
                        &drop_shadow.input,
                        filter_primitives,
                        render_tasks,
                        cur_index,
                        &outputs,
                        original_task_id,
                        primitive.color_space
                    );

                    let blur_std_deviation = drop_shadow.shadow.blur_radius * device_pixel_scale.0;
                    let offset = drop_shadow.shadow.offset * LayoutToWorldScale::new(1.0) * device_pixel_scale;

                    let offset_task = RenderTask::new_svg_filter_primitive(
                        vec![input_task_id],
                        content_size,
                        uv_rect_kind,
                        SvgFilterInfo::Offset(offset),
                    );
                    let offset_task_id = render_tasks.add(offset_task);

                    let blur_task_id = RenderTask::new_blur(
                        DeviceSize::new(blur_std_deviation, blur_std_deviation),
                        offset_task_id,
                        render_tasks,
                        RenderTargetKind::Color,
                        ClearMode::Transparent,
                        None,
                    );

                    let task = RenderTask::new_svg_filter_primitive(
                        vec![input_task_id, blur_task_id],
                        content_size,
                        uv_rect_kind,
                        SvgFilterInfo::DropShadow(drop_shadow.shadow.color),
                    );
                    render_tasks.add(task)
                }
                FilterPrimitiveKind::ComponentTransfer(ref component_transfer) => {
                    let input_task_id = get_task_input(
                        &component_transfer.input,
                        filter_primitives,
                        render_tasks,
                        cur_index,
                        &outputs,
                        original_task_id,
                        primitive.color_space
                    );

                    let filter_data = &filter_datas[cur_filter_data];
                    cur_filter_data += 1;
                    if filter_data.is_identity() {
                        input_task_id
                    } else {
                        let task = RenderTask::new_svg_filter_primitive(
                            vec![input_task_id],
                            content_size,
                            uv_rect_kind,
                            SvgFilterInfo::ComponentTransfer(filter_data.clone()),
                        );
                        render_tasks.add(task)
                    }
                }
                FilterPrimitiveKind::Offset(ref info) => {
                    let input_task_id = get_task_input(
                        &info.input,
                        filter_primitives,
                        render_tasks,
                        cur_index,
                        &outputs,
                        original_task_id,
                        primitive.color_space
                    );

                    let offset = info.offset * LayoutToWorldScale::new(1.0) * device_pixel_scale;
                    let offset_task = RenderTask::new_svg_filter_primitive(
                        vec![input_task_id],
                        content_size,
                        uv_rect_kind,
                        SvgFilterInfo::Offset(offset),
                    );
                    render_tasks.add(offset_task)
                }
            };
            outputs.push(render_task_id);
        }

        // The output of a filter is the output of the last primitive in the chain.
        let mut render_task_id = *outputs.last().unwrap();

        // Convert to sRGB if needed
        if filter_primitives.last().unwrap().color_space == ColorSpace::LinearRgb {
            let task = RenderTask::new_svg_filter_primitive(
                vec![render_task_id],
                content_size,
                uv_rect_kind,
                SvgFilterInfo::LinearToSrgb,
            );
            render_task_id = render_tasks.add(task);
        }

        render_task_id
    }

    pub fn new_svg_filter_primitive(
        tasks: Vec<RenderTaskId>,
        target_size: DeviceIntSize,
        uv_rect_kind: UvRectKind,
        info: SvgFilterInfo,
    ) -> Self {
        RenderTask::with_dynamic_location(
            target_size,
            tasks,
            RenderTaskKind::SvgFilter(SvgFilterTask {
                extra_gpu_cache_handle: None,
                uv_rect_handle: GpuCacheHandle::new(),
                uv_rect_kind,
                info,
            }),
            ClearMode::Transparent,
        )
    }

    fn uv_rect_kind(&self) -> UvRectKind {
        match self.kind {
            RenderTaskKind::CacheMask(..) |
            RenderTaskKind::Readback(..) => {
                unreachable!("bug: unexpected render task");
            }

            RenderTaskKind::Picture(ref task) => {
                task.uv_rect_kind
            }

            RenderTaskKind::VerticalBlur(ref task) |
            RenderTaskKind::HorizontalBlur(ref task) => {
                task.uv_rect_kind
            }

            RenderTaskKind::Scaling(ref task) => {
                task.uv_rect_kind
            }

            RenderTaskKind::SvgFilter(ref task) => {
                task.uv_rect_kind
            }

            RenderTaskKind::ClipRegion(..) |
            RenderTaskKind::Border(..) |
            RenderTaskKind::Gradient(..) |
            RenderTaskKind::LineDecoration(..) |
            RenderTaskKind::Blit(..) => {
                UvRectKind::Rect
            }

            #[cfg(test)]
            RenderTaskKind::Test(..) => {
                unreachable!("Unexpected render task");
            }
        }
    }

    // Write (up to) 8 floats of data specific to the type
    // of render task that is provided to the GPU shaders
    // via a vertex texture.
    pub fn write_task_data(&self) -> RenderTaskData {
        // NOTE: The ordering and layout of these structures are
        //       required to match both the GPU structures declared
        //       in prim_shared.glsl, and also the uses in submit_batch()
        //       in renderer.rs.
        // TODO(gw): Maybe there's a way to make this stuff a bit
        //           more type-safe. Although, it will always need
        //           to be kept in sync with the GLSL code anyway.

        let data = match self.kind {
            RenderTaskKind::Picture(ref task) => {
                // Note: has to match `PICTURE_TYPE_*` in shaders
                [
                    task.device_pixel_scale.0,
                    task.content_origin.x as f32,
                    task.content_origin.y as f32,
                ]
            }
            RenderTaskKind::CacheMask(ref task) => {
                [
                    task.device_pixel_scale.0,
                    task.actual_rect.origin.x as f32,
                    task.actual_rect.origin.y as f32,
                ]
            }
            RenderTaskKind::ClipRegion(ref task) => {
                [
                    task.device_pixel_scale.0,
                    0.0,
                    0.0,
                ]
            }
            RenderTaskKind::VerticalBlur(ref task) |
            RenderTaskKind::HorizontalBlur(ref task) => {
                [
                    task.blur_std_deviation,
                    0.0,
                    0.0,
                ]
            }
            RenderTaskKind::Readback(..) |
            RenderTaskKind::Scaling(..) |
            RenderTaskKind::Border(..) |
            RenderTaskKind::LineDecoration(..) |
            RenderTaskKind::Gradient(..) |
            RenderTaskKind::Blit(..) => {
                [0.0; 3]
            }


            RenderTaskKind::SvgFilter(ref task) => {
                match task.info {
                    SvgFilterInfo::Opacity(opacity) => [opacity, 0.0, 0.0],
                    SvgFilterInfo::Offset(offset) => [offset.x, offset.y, 0.0],
                    _ => [0.0; 3]
                }
            }

            #[cfg(test)]
            RenderTaskKind::Test(..) => {
                unreachable!();
            }
        };

        let (mut target_rect, target_index) = self.get_target_rect();
        // The primitives inside a fixed-location render task
        // are already placed to their corresponding positions,
        // so the shader doesn't need to shift by the origin.
        if let RenderTaskLocation::Fixed(_) = self.location {
            target_rect.origin = DeviceIntPoint::origin();
        }

        RenderTaskData {
            data: [
                target_rect.origin.x as f32,
                target_rect.origin.y as f32,
                target_rect.size.width as f32,
                target_rect.size.height as f32,
                target_index.0 as f32,
                data[0],
                data[1],
                data[2],
            ]
        }
    }

    pub fn get_texture_address(&self, gpu_cache: &GpuCache) -> GpuCacheAddress {
        match self.kind {
            RenderTaskKind::Picture(ref info) => {
                gpu_cache.get_address(&info.uv_rect_handle)
            }
            RenderTaskKind::VerticalBlur(ref info) |
            RenderTaskKind::HorizontalBlur(ref info) => {
                gpu_cache.get_address(&info.uv_rect_handle)
            }
            RenderTaskKind::SvgFilter(ref info) => {
                gpu_cache.get_address(&info.uv_rect_handle)
            }
            RenderTaskKind::ClipRegion(..) |
            RenderTaskKind::Readback(..) |
            RenderTaskKind::Scaling(..) |
            RenderTaskKind::Blit(..) |
            RenderTaskKind::Border(..) |
            RenderTaskKind::CacheMask(..) |
            RenderTaskKind::Gradient(..) |
            RenderTaskKind::LineDecoration(..) => {
                panic!("texture handle not supported for this task kind");
            }
            #[cfg(test)]
            RenderTaskKind::Test(..) => {
                panic!("RenderTask tests aren't expected to exercise this code");
            }
        }
    }

    pub fn get_dynamic_size(&self) -> DeviceIntSize {
        match self.location {
            RenderTaskLocation::Fixed(..) => DeviceIntSize::zero(),
            RenderTaskLocation::Dynamic(_, size) => size,
            RenderTaskLocation::TextureCache { rect, .. } => rect.size,
            RenderTaskLocation::PictureCache { size, .. } => size,
        }
    }

    pub fn get_target_rect(&self) -> (DeviceIntRect, RenderTargetIndex) {
        match self.location {
            RenderTaskLocation::Fixed(rect) => {
                (rect, RenderTargetIndex(0))
            }
            // Previously, we only added render tasks after the entire
            // primitive chain was determined visible. This meant that
            // we could assert any render task in the list was also
            // allocated (assigned to passes). Now, we add render
            // tasks earlier, and the picture they belong to may be
            // culled out later, so we can't assert that the task
            // has been allocated.
            // Render tasks that are created but not assigned to
            // passes consume a row in the render task texture, but
            // don't allocate any space in render targets nor
            // draw any pixels.
            // TODO(gw): Consider some kind of tag or other method
            //           to mark a task as unused explicitly. This
            //           would allow us to restore this debug check.
            RenderTaskLocation::Dynamic(Some((origin, target_index)), size) => {
                (DeviceIntRect::new(origin, size), target_index)
            }
            RenderTaskLocation::Dynamic(None, _) => {
                (DeviceIntRect::zero(), RenderTargetIndex(0))
            }
            RenderTaskLocation::TextureCache {layer, rect, .. } => {
                (rect, RenderTargetIndex(layer as usize))
            }
            RenderTaskLocation::PictureCache { size, layer, .. } => {
                (
                    DeviceIntRect::new(
                        DeviceIntPoint::zero(),
                        size,
                    ),
                    RenderTargetIndex(layer as usize),
                )
            }
        }
    }

    pub fn target_kind(&self) -> RenderTargetKind {
        match self.kind {
            RenderTaskKind::LineDecoration(..) |
            RenderTaskKind::Readback(..) |
            RenderTaskKind::Border(..) |
            RenderTaskKind::Gradient(..) |
            RenderTaskKind::Picture(..) |
            RenderTaskKind::Blit(..) |
            RenderTaskKind::SvgFilter(..) => {
                RenderTargetKind::Color
            }

            RenderTaskKind::ClipRegion(..) |
            RenderTaskKind::CacheMask(..) => {
                RenderTargetKind::Alpha
            }

            RenderTaskKind::VerticalBlur(ref task_info) |
            RenderTaskKind::HorizontalBlur(ref task_info) => {
                task_info.target_kind
            }

            RenderTaskKind::Scaling(ref task_info) => {
                task_info.target_kind
            }

            #[cfg(test)]
            RenderTaskKind::Test(kind) => kind,
        }
    }

    pub fn write_gpu_blocks(
        &mut self,
        gpu_cache: &mut GpuCache,
    ) {
        let (target_rect, target_index) = self.get_target_rect();

        let (cache_handle, uv_rect_kind) = match self.kind {
            RenderTaskKind::HorizontalBlur(ref mut info) |
            RenderTaskKind::VerticalBlur(ref mut info) => {
                (&mut info.uv_rect_handle, info.uv_rect_kind)
            }
            RenderTaskKind::Picture(ref mut info) => {
                (&mut info.uv_rect_handle, info.uv_rect_kind)
            }
            RenderTaskKind::SvgFilter(ref mut info) => {
                (&mut info.uv_rect_handle, info.uv_rect_kind)
            }
            RenderTaskKind::Readback(..) |
            RenderTaskKind::Scaling(..) |
            RenderTaskKind::Blit(..) |
            RenderTaskKind::ClipRegion(..) |
            RenderTaskKind::Border(..) |
            RenderTaskKind::CacheMask(..) |
            RenderTaskKind::Gradient(..) |
            RenderTaskKind::LineDecoration(..) => {
                return;
            }
            #[cfg(test)]
            RenderTaskKind::Test(..) => {
                panic!("RenderTask tests aren't expected to exercise this code");
            }
        };

        if let Some(mut request) = gpu_cache.request(cache_handle) {
            let p0 = target_rect.min().to_f32();
            let p1 = target_rect.max().to_f32();
            let image_source = ImageSource {
                p0,
                p1,
                texture_layer: target_index.0 as f32,
                user_data: [0.0; 3],
                uv_rect_kind,
            };
            image_source.write_gpu_blocks(&mut request);
        }

        if let RenderTaskKind::SvgFilter(ref mut filter_task) = self.kind {
            match filter_task.info {
                SvgFilterInfo::ColorMatrix(ref matrix) => {
                    let handle = filter_task.extra_gpu_cache_handle.get_or_insert_with(|| GpuCacheHandle::new());
                    if let Some(mut request) = gpu_cache.request(handle) {
                        for i in 0..5 {
                            request.push([matrix[i*4], matrix[i*4+1], matrix[i*4+2], matrix[i*4+3]]);
                        }
                    }
                }
                SvgFilterInfo::DropShadow(color) |
                SvgFilterInfo::Flood(color) => {
                    let handle = filter_task.extra_gpu_cache_handle.get_or_insert_with(|| GpuCacheHandle::new());
                    if let Some(mut request) = gpu_cache.request(handle) {
                        request.push(color.to_array());
                    }
                }
                SvgFilterInfo::ComponentTransfer(ref data) => {
                    let handle = filter_task.extra_gpu_cache_handle.get_or_insert_with(|| GpuCacheHandle::new());
                    if let Some(request) = gpu_cache.request(handle) {
                        data.update(request);
                    }
                }
                _ => {},
            }
        }
    }

    #[cfg(feature = "debugger")]
    pub fn print_with<T: PrintTreePrinter>(&self, pt: &mut T, tree: &RenderTaskGraph) -> bool {
        match self.kind {
            RenderTaskKind::Picture(ref task) => {
                pt.new_level(format!("Picture of {:?}", task.pic_index));
            }
            RenderTaskKind::CacheMask(ref task) => {
                pt.new_level(format!("CacheMask with {} clips", task.clip_node_range.count));
                pt.add_item(format!("rect: {:?}", task.actual_rect));
            }
            RenderTaskKind::LineDecoration(..) => {
                pt.new_level("LineDecoration".to_owned());
            }
            RenderTaskKind::ClipRegion(..) => {
                pt.new_level("ClipRegion".to_owned());
            }
            RenderTaskKind::VerticalBlur(ref task) => {
                pt.new_level("VerticalBlur".to_owned());
                task.print_with(pt);
            }
            RenderTaskKind::HorizontalBlur(ref task) => {
                pt.new_level("HorizontalBlur".to_owned());
                task.print_with(pt);
            }
            RenderTaskKind::Readback(ref rect) => {
                pt.new_level("Readback".to_owned());
                pt.add_item(format!("rect: {:?}", rect));
            }
            RenderTaskKind::Scaling(ref kind) => {
                pt.new_level("Scaling".to_owned());
                pt.add_item(format!("kind: {:?}", kind));
            }
            RenderTaskKind::Border(..) => {
                pt.new_level("Border".to_owned());
            }
            RenderTaskKind::Blit(ref task) => {
                pt.new_level("Blit".to_owned());
                pt.add_item(format!("source: {:?}", task.source));
            }
            RenderTaskKind::Gradient(..) => {
                pt.new_level("Gradient".to_owned());
            }
            RenderTaskKind::SvgFilter(ref task) => {
                pt.new_level("SvgFilter".to_owned());
                pt.add_item(format!("primitive: {:?}", task.info));
            }
            #[cfg(test)]
            RenderTaskKind::Test(..) => {
                pt.new_level("Test".to_owned());
            }
        }

        pt.add_item(format!("clear to: {:?}", self.clear_mode));
        pt.add_item(format!("dimensions: {:?}", self.location.size()));

        for &child_id in &self.children {
            if tree[child_id].print_with(pt, tree) {
                pt.add_item(format!("self: {:?}", child_id))
            }
        }

        pt.end_level();
        true
    }

    /// Mark this render task for keeping the results alive up until the end of the frame.
    pub fn mark_for_saving(&mut self) {
        match self.location {
            RenderTaskLocation::Fixed(..) |
            RenderTaskLocation::Dynamic(..) => {
                self.saved_index = Some(SavedTargetIndex::PENDING);
            }
            RenderTaskLocation::TextureCache { .. } |
            RenderTaskLocation::PictureCache { .. } => {
                panic!("Unable to mark a permanently cached task for saving!");
            }
        }
    }
}

#[derive(Clone, Debug, Hash, PartialEq, Eq)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub enum RenderTaskCacheKeyKind {
    BoxShadow(BoxShadowCacheKey),
    Image(ImageCacheKey),
    BorderSegment(BorderSegmentCacheKey),
    LineDecoration(LineDecorationCacheKey),
    Gradient(GradientCacheKey),
}

#[derive(Clone, Debug, Hash, PartialEq, Eq)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct RenderTaskCacheKey {
    pub size: DeviceIntSize,
    pub kind: RenderTaskCacheKeyKind,
}

#[derive(Debug)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct RenderTaskCacheEntry {
    user_data: Option<[f32; 3]>,
    is_opaque: bool,
    pub handle: TextureCacheHandle,
}

#[derive(Debug, MallocSizeOf)]
#[cfg_attr(feature = "capture", derive(Serialize))]
pub enum RenderTaskCacheMarker {}

// A cache of render tasks that are stored in the texture
// cache for usage across frames.
#[derive(Debug)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct RenderTaskCache {
    map: FastHashMap<RenderTaskCacheKey, FreeListHandle<RenderTaskCacheMarker>>,
    cache_entries: FreeList<RenderTaskCacheEntry, RenderTaskCacheMarker>,
}

pub type RenderTaskCacheEntryHandle = WeakFreeListHandle<RenderTaskCacheMarker>;

impl RenderTaskCache {
    pub fn new() -> Self {
        RenderTaskCache {
            map: FastHashMap::default(),
            cache_entries: FreeList::new(),
        }
    }

    pub fn clear(&mut self) {
        self.map.clear();
        self.cache_entries.clear();
    }

    pub fn begin_frame(
        &mut self,
        texture_cache: &mut TextureCache,
    ) {
        // Drop any items from the cache that have been
        // evicted from the texture cache.
        //
        // This isn't actually necessary for the texture
        // cache to be able to evict old render tasks.
        // It will evict render tasks as required, since
        // the access time in the texture cache entry will
        // be stale if this task hasn't been requested
        // for a while.
        //
        // Nonetheless, we should remove stale entries
        // from here so that this hash map doesn't
        // grow indefinitely!
        let cache_entries = &mut self.cache_entries;

        self.map.retain(|_, handle| {
            let retain = texture_cache.is_allocated(
                &cache_entries.get(handle).handle,
            );
            if !retain {
                let handle = mem::replace(handle, FreeListHandle::invalid());
                cache_entries.free(handle);
            }
            retain
        });
    }

    fn alloc_render_task(
        render_task: &mut RenderTask,
        entry: &mut RenderTaskCacheEntry,
        gpu_cache: &mut GpuCache,
        texture_cache: &mut TextureCache,
    ) {
        // Find out what size to alloc in the texture cache.
        let size = match render_task.location {
            RenderTaskLocation::Fixed(..) |
            RenderTaskLocation::PictureCache { .. } |
            RenderTaskLocation::TextureCache { .. } => {
                panic!("BUG: dynamic task was expected");
            }
            RenderTaskLocation::Dynamic(_, size) => size,
        };

        // Select the right texture page to allocate from.
        let image_format = match render_task.target_kind() {
            RenderTargetKind::Color => texture_cache.shared_color_expected_format(),
            RenderTargetKind::Alpha => texture_cache.shared_alpha_expected_format(),
        };

        let descriptor = ImageDescriptor::new(
            size.width,
            size.height,
            image_format,
            entry.is_opaque,
            false,
        );

        // Allocate space in the texture cache, but don't supply
        // and CPU-side data to be uploaded.
        //
        // Note that we currently use Eager eviction for cached render
        // tasks, which means that any cached item not used in the last
        // frame is discarded. There's room to be a lot smarter here,
        // especially by considering the relative costs of re-rendering
        // each type of item (box shadow blurs are an order of magnitude
        // more expensive than borders, for example). Telemetry could
        // inform our decisions here as well.
        texture_cache.update(
            &mut entry.handle,
            descriptor,
            TextureFilter::Linear,
            None,
            entry.user_data.unwrap_or([0.0; 3]),
            DirtyRect::All,
            gpu_cache,
            None,
            render_task.uv_rect_kind(),
            Eviction::Eager,
        );

        // Get the allocation details in the texture cache, and store
        // this in the render task. The renderer will draw this
        // task into the appropriate layer and rect of the texture
        // cache on this frame.
        let (texture_id, texture_layer, uv_rect, _, _) =
            texture_cache.get_cache_location(&entry.handle);

        render_task.location = RenderTaskLocation::TextureCache {
            texture: texture_id,
            layer: texture_layer,
            rect: uv_rect.to_i32(),
        };
    }

    pub fn request_render_task<F>(
        &mut self,
        key: RenderTaskCacheKey,
        texture_cache: &mut TextureCache,
        gpu_cache: &mut GpuCache,
        render_tasks: &mut RenderTaskGraph,
        user_data: Option<[f32; 3]>,
        is_opaque: bool,
        f: F,
    ) -> Result<RenderTaskCacheEntryHandle, ()>
    where
        F: FnOnce(&mut RenderTaskGraph) -> Result<RenderTaskId, ()>,
    {
        // Get the texture cache handle for this cache key,
        // or create one.
        let cache_entries = &mut self.cache_entries;
        let entry_handle = self.map.entry(key).or_insert_with(|| {
            let entry = RenderTaskCacheEntry {
                handle: TextureCacheHandle::invalid(),
                user_data,
                is_opaque,
            };
            cache_entries.insert(entry)
        });
        let cache_entry = cache_entries.get_mut(entry_handle);

        // Check if this texture cache handle is valid.
        if texture_cache.request(&cache_entry.handle, gpu_cache) {
            // Invoke user closure to get render task chain
            // to draw this into the texture cache.
            let render_task_id = f(render_tasks)?;
            render_tasks.cacheable_render_tasks.push(render_task_id);

            cache_entry.user_data = user_data;
            cache_entry.is_opaque = is_opaque;

            RenderTaskCache::alloc_render_task(
                &mut render_tasks[render_task_id],
                cache_entry,
                gpu_cache,
                texture_cache,
            );
        }

        Ok(entry_handle.weak())
    }

    pub fn get_cache_entry(
        &self,
        handle: &RenderTaskCacheEntryHandle,
    ) -> &RenderTaskCacheEntry {
        self.cache_entries
            .get_opt(handle)
            .expect("bug: invalid render task cache handle")
    }

    #[allow(dead_code)]
    pub fn get_cache_item_for_render_task(&self,
                                          texture_cache: &TextureCache,
                                          key: &RenderTaskCacheKey)
                                          -> CacheItem {
        // Get the texture cache handle for this cache key.
        let handle = self.map.get(key).unwrap();
        let cache_entry = self.cache_entries.get(handle);
        texture_cache.get(&cache_entry.handle)
    }

    #[allow(dead_code)]
    pub fn get_allocated_size_for_render_task(&self,
                                              texture_cache: &TextureCache,
                                              key: &RenderTaskCacheKey)
                                              -> Option<usize> {
        let handle = self.map.get(key).unwrap();
        let cache_entry = self.cache_entries.get(handle);
        texture_cache.get_allocated_size(&cache_entry.handle)
    }
}

// TODO(gw): Rounding the content rect here to device pixels is not
// technically correct. Ideally we should ceil() here, and ensure that
// the extra part pixel in the case of fractional sizes is correctly
// handled. For now, just use rounding which passes the existing
// Gecko tests.
// Note: zero-square tasks are prohibited in WR task graph, so
// we ensure each dimension to be at least the length of 1 after rounding.
pub fn to_cache_size(size: DeviceSize) -> DeviceIntSize {
    DeviceIntSize::new(
        1.max(size.width.round() as i32),
        1.max(size.height.round() as i32),
    )
}

// Dump an SVG visualization of the render graph for debugging purposes
#[allow(dead_code)]
pub fn dump_render_tasks_as_svg(
    render_tasks: &RenderTaskGraph,
    passes: &[RenderPass],
    output: &mut dyn io::Write,
) -> io::Result<()> {
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
    output: &mut dyn io::Write,
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
