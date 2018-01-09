/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{ClipId, ColorF, DeviceIntPoint, DeviceIntRect, DeviceIntSize};
use api::{DevicePixelScale, DeviceUintPoint, DeviceUintRect, DeviceUintSize};
use api::{DocumentLayer, FilterOp, ImageFormat};
use api::{LayerRect, MixBlendMode, PipelineId};
use batch::{AlphaBatcher, ClipBatcher};
use clip::{ClipStore};
use clip_scroll_tree::{ClipScrollTree};
use device::Texture;
use gpu_cache::{GpuCache, GpuCacheUpdateList};
use gpu_types::{BlurDirection, BlurInstance, BrushInstance, ClipChainRectIndex};
use gpu_types::{ClipScrollNodeData, ClipScrollNodeIndex};
use gpu_types::{PrimitiveInstance};
use internal_types::{FastHashMap, RenderPassIndex};
use picture::{PictureKind};
use prim_store::{PrimitiveIndex, PrimitiveKind, PrimitiveStore};
use prim_store::{BrushMaskKind, BrushKind, DeferredResolve};
use profiler::FrameProfileCounters;
use render_task::{RenderTaskAddress, RenderTaskId, RenderTaskKey, RenderTaskKind};
use render_task::{BlurTask, ClearMode, RenderTaskLocation, RenderTaskTree};
use resource_cache::{ResourceCache};
use std::{cmp, usize, f32, i32};
use std::collections::hash_map::Entry;
use texture_allocator::GuillotineAllocator;

const MIN_TARGET_SIZE: u32 = 2048;

#[derive(Debug)]
pub struct ScrollbarPrimitive {
    pub clip_id: ClipId,
    pub prim_index: PrimitiveIndex,
    pub frame_rect: LayerRect,
}

#[derive(Debug, Copy, Clone)]
pub struct RenderTargetIndex(pub usize);

#[derive(Debug)]
struct DynamicTaskInfo {
    task_id: RenderTaskId,
    rect: DeviceIntRect,
}

pub struct RenderTargetContext<'a> {
    pub device_pixel_scale: DevicePixelScale,
    pub prim_store: &'a PrimitiveStore,
    pub resource_cache: &'a ResourceCache,
    pub node_data: &'a [ClipScrollNodeData],
    pub clip_scroll_tree: &'a ClipScrollTree,
    pub use_dual_source_blending: bool,
}

struct TextureAllocator {
    // TODO(gw): Replace this with a simpler allocator for
    // render target allocation - this use case doesn't need
    // to deal with coalescing etc that the general texture
    // cache allocator requires.
    allocator: GuillotineAllocator,

    // Track the used rect of the render target, so that
    // we can set a scissor rect and only clear to the
    // used portion of the target as an optimization.
    used_rect: DeviceIntRect,
}

impl TextureAllocator {
    fn new(size: DeviceUintSize) -> Self {
        TextureAllocator {
            allocator: GuillotineAllocator::new(size),
            used_rect: DeviceIntRect::zero(),
        }
    }

    fn allocate(&mut self, size: &DeviceUintSize) -> Option<DeviceUintPoint> {
        let origin = self.allocator.allocate(size);

        if let Some(origin) = origin {
            // TODO(gw): We need to make all the device rects
            //           be consistent in the use of the
            //           DeviceIntRect and DeviceUintRect types!
            let origin = DeviceIntPoint::new(origin.x as i32, origin.y as i32);
            let size = DeviceIntSize::new(size.width as i32, size.height as i32);
            let rect = DeviceIntRect::new(origin, size);
            self.used_rect = rect.union(&self.used_rect);
        }

        origin
    }
}

pub trait RenderTarget {
    fn new(
        size: Option<DeviceUintSize>,
        screen_size: DeviceIntSize,
    ) -> Self;
    fn allocate(&mut self, size: DeviceUintSize) -> Option<DeviceUintPoint>;
    fn build(
        &mut self,
        _ctx: &RenderTargetContext,
        _gpu_cache: &mut GpuCache,
        _render_tasks: &mut RenderTaskTree,
        _deferred_resolves: &mut Vec<DeferredResolve>,
    ) {
    }
    fn add_task(
        &mut self,
        task_id: RenderTaskId,
        ctx: &RenderTargetContext,
        gpu_cache: &GpuCache,
        render_tasks: &RenderTaskTree,
        clip_store: &ClipStore,
    );
    fn used_rect(&self) -> DeviceIntRect;
    fn needs_depth(&self) -> bool;
}

#[derive(Debug, Copy, Clone, Eq, PartialEq, Hash)]
pub enum RenderTargetKind {
    Color, // RGBA32
    Alpha, // R8
}

pub struct RenderTargetList<T> {
    screen_size: DeviceIntSize,
    pub format: ImageFormat,
    pub max_size: DeviceUintSize,
    pub targets: Vec<T>,
    pub texture: Option<Texture>,
}

impl<T: RenderTarget> RenderTargetList<T> {
    fn new(
        screen_size: DeviceIntSize,
        format: ImageFormat,
    ) -> Self {
        RenderTargetList {
            screen_size,
            format,
            max_size: DeviceUintSize::new(MIN_TARGET_SIZE, MIN_TARGET_SIZE),
            targets: Vec::new(),
            texture: None,
        }
    }

    fn build(
        &mut self,
        ctx: &RenderTargetContext,
        gpu_cache: &mut GpuCache,
        render_tasks: &mut RenderTaskTree,
        deferred_resolves: &mut Vec<DeferredResolve>,
    ) {
        for target in &mut self.targets {
            target.build(ctx, gpu_cache, render_tasks, deferred_resolves);
        }
    }

    fn add_task(
        &mut self,
        task_id: RenderTaskId,
        ctx: &RenderTargetContext,
        gpu_cache: &GpuCache,
        render_tasks: &mut RenderTaskTree,
        clip_store: &ClipStore,
    ) {
        self.targets.last_mut().unwrap().add_task(
            task_id,
            ctx,
            gpu_cache,
            render_tasks,
            clip_store,
        );
    }

    fn allocate(
        &mut self,
        alloc_size: DeviceUintSize,
    ) -> (DeviceUintPoint, RenderTargetIndex) {
        let existing_origin = self.targets
            .last_mut()
            .and_then(|target| target.allocate(alloc_size));

        let origin = match existing_origin {
            Some(origin) => origin,
            None => {
                let mut new_target = T::new(Some(self.max_size), self.screen_size);
                let origin = new_target.allocate(alloc_size).expect(&format!(
                    "Each render task must allocate <= size of one target! ({:?})",
                    alloc_size
                ));
                self.targets.push(new_target);
                origin
            }
        };

        (origin, RenderTargetIndex(self.targets.len() - 1))
    }

    pub fn needs_depth(&self) -> bool {
        self.targets.iter().any(|target| target.needs_depth())
    }
}

/// Frame output information for a given pipeline ID.
/// Storing the task ID allows the renderer to find
/// the target rect within the render target that this
/// pipeline exists at.
pub struct FrameOutput {
    pub task_id: RenderTaskId,
    pub pipeline_id: PipelineId,
}

pub struct ScalingInfo {
    pub src_task_id: RenderTaskId,
    pub dest_task_id: RenderTaskId,
}

/// A render target represents a number of rendering operations on a surface.
pub struct ColorRenderTarget {
    pub alpha_batcher: AlphaBatcher,
    // List of blur operations to apply for this render target.
    pub vertical_blurs: Vec<BlurInstance>,
    pub horizontal_blurs: Vec<BlurInstance>,
    pub readbacks: Vec<DeviceIntRect>,
    pub scalings: Vec<ScalingInfo>,
    // List of frame buffer outputs for this render target.
    pub outputs: Vec<FrameOutput>,
    allocator: Option<TextureAllocator>,
    alpha_tasks: Vec<RenderTaskId>,
}

impl RenderTarget for ColorRenderTarget {
    fn allocate(&mut self, size: DeviceUintSize) -> Option<DeviceUintPoint> {
        self.allocator
            .as_mut()
            .expect("bug: calling allocate on framebuffer")
            .allocate(&size)
    }

    fn new(
        size: Option<DeviceUintSize>,
        screen_size: DeviceIntSize,
    ) -> Self {
        ColorRenderTarget {
            alpha_batcher: AlphaBatcher::new(screen_size),
            vertical_blurs: Vec::new(),
            horizontal_blurs: Vec::new(),
            readbacks: Vec::new(),
            scalings: Vec::new(),
            allocator: size.map(TextureAllocator::new),
            outputs: Vec::new(),
            alpha_tasks: Vec::new(),
        }
    }

    fn build(
        &mut self,
        ctx: &RenderTargetContext,
        gpu_cache: &mut GpuCache,
        render_tasks: &mut RenderTaskTree,
        deferred_resolves: &mut Vec<DeferredResolve>,
    ) {
        self.alpha_batcher.build(
            &self.alpha_tasks,
            ctx,
            gpu_cache,
            render_tasks,
            deferred_resolves,
        );
    }

    fn add_task(
        &mut self,
        task_id: RenderTaskId,
        ctx: &RenderTargetContext,
        _: &GpuCache,
        render_tasks: &RenderTaskTree,
        _: &ClipStore,
    ) {
        let task = &render_tasks[task_id];

        match task.kind {
            RenderTaskKind::Alias(..) => {
                panic!("BUG: add_task() called on invalidated task");
            }
            RenderTaskKind::VerticalBlur(ref info) => {
                info.add_instances(
                    &mut self.vertical_blurs,
                    task_id,
                    task.children[0],
                    BlurDirection::Vertical,
                    render_tasks,
                );
            }
            RenderTaskKind::HorizontalBlur(ref info) => {
                info.add_instances(
                    &mut self.horizontal_blurs,
                    task_id,
                    task.children[0],
                    BlurDirection::Horizontal,
                    render_tasks,
                );
            }
            RenderTaskKind::Picture(ref task_info) => {
                let prim_metadata = ctx.prim_store.get_metadata(task_info.prim_index);
                match prim_metadata.prim_kind {
                    PrimitiveKind::Picture => {
                        let prim = &ctx.prim_store.cpu_pictures[prim_metadata.cpu_prim_index.0];

                        self.alpha_tasks.push(task_id);

                        if let PictureKind::Image { frame_output_pipeline_id, .. } = prim.kind {
                            // If this pipeline is registered as a frame output
                            // store the information necessary to do the copy.
                            if let Some(pipeline_id) = frame_output_pipeline_id {
                                self.outputs.push(FrameOutput {
                                    pipeline_id,
                                    task_id,
                                });
                            }
                        }
                    }
                    _ => {
                        // No other primitives make use of primitive caching yet!
                        unreachable!()
                    }
                }
            }
            RenderTaskKind::CacheMask(..) => {
                panic!("Should not be added to color target!");
            }
            RenderTaskKind::Readback(device_rect) => {
                self.readbacks.push(device_rect);
            }
            RenderTaskKind::Scaling(..) => {
                self.scalings.push(ScalingInfo {
                    src_task_id: task.children[0],
                    dest_task_id: task_id,
                });
            }
        }
    }

    fn used_rect(&self) -> DeviceIntRect {
        self.allocator
            .as_ref()
            .expect("bug: used_rect called on framebuffer")
            .used_rect
    }

    fn needs_depth(&self) -> bool {
        !self.alpha_batcher.batch_list.opaque_batch_list.batches.is_empty()
    }
}

pub struct AlphaRenderTarget {
    pub clip_batcher: ClipBatcher,
    pub brush_mask_corners: Vec<PrimitiveInstance>,
    pub brush_mask_rounded_rects: Vec<PrimitiveInstance>,
    // List of blur operations to apply for this render target.
    pub vertical_blurs: Vec<BlurInstance>,
    pub horizontal_blurs: Vec<BlurInstance>,
    pub scalings: Vec<ScalingInfo>,
    pub zero_clears: Vec<RenderTaskId>,
    allocator: TextureAllocator,
}

impl RenderTarget for AlphaRenderTarget {
    fn allocate(&mut self, size: DeviceUintSize) -> Option<DeviceUintPoint> {
        self.allocator.allocate(&size)
    }

    fn new(
        size: Option<DeviceUintSize>,
        _: DeviceIntSize,
    ) -> Self {
        AlphaRenderTarget {
            clip_batcher: ClipBatcher::new(),
            brush_mask_corners: Vec::new(),
            brush_mask_rounded_rects: Vec::new(),
            vertical_blurs: Vec::new(),
            horizontal_blurs: Vec::new(),
            scalings: Vec::new(),
            zero_clears: Vec::new(),
            allocator: TextureAllocator::new(size.expect("bug: alpha targets need size")),
        }
    }

    fn add_task(
        &mut self,
        task_id: RenderTaskId,
        ctx: &RenderTargetContext,
        gpu_cache: &GpuCache,
        render_tasks: &RenderTaskTree,
        clip_store: &ClipStore,
    ) {
        let task = &render_tasks[task_id];

        match task.clear_mode {
            ClearMode::Zero => {
                self.zero_clears.push(task_id);
            }
            ClearMode::One => {}
            ClearMode::Transparent => {
                panic!("bug: invalid clear mode for alpha task");
            }
        }

        match task.kind {
            RenderTaskKind::Alias(..) => {
                panic!("BUG: add_task() called on invalidated task");
            }
            RenderTaskKind::Readback(..) => {
                panic!("Should not be added to alpha target!");
            }
            RenderTaskKind::VerticalBlur(ref info) => {
                info.add_instances(
                    &mut self.vertical_blurs,
                    task_id,
                    task.children[0],
                    BlurDirection::Vertical,
                    render_tasks,
                );
            }
            RenderTaskKind::HorizontalBlur(ref info) => {
                info.add_instances(
                    &mut self.horizontal_blurs,
                    task_id,
                    task.children[0],
                    BlurDirection::Horizontal,
                    render_tasks,
                );
            }
            RenderTaskKind::Picture(ref task_info) => {
                let prim_metadata = ctx.prim_store.get_metadata(task_info.prim_index);

                match prim_metadata.prim_kind {
                    PrimitiveKind::Picture => {
                        let prim = &ctx.prim_store.cpu_pictures[prim_metadata.cpu_prim_index.0];

                        let task_index = render_tasks.get_task_address(task_id);

                        for run in &prim.runs {
                            for i in 0 .. run.count {
                                let sub_prim_index = PrimitiveIndex(run.base_prim_index.0 + i);

                                let sub_metadata = ctx.prim_store.get_metadata(sub_prim_index);
                                let sub_prim_address =
                                    gpu_cache.get_address(&sub_metadata.gpu_location);

                                match sub_metadata.prim_kind {
                                    PrimitiveKind::Brush => {
                                        let instance = BrushInstance {
                                            picture_address: task_index,
                                            prim_address: sub_prim_address,
                                            // TODO(gw): In the future, when brush
                                            //           primitives on picture backed
                                            //           tasks support clip masks and
                                            //           transform primitives, these
                                            //           will need to be filled out!
                                            clip_chain_rect_index: ClipChainRectIndex(0),
                                            scroll_id: ClipScrollNodeIndex(0),
                                            clip_task_address: RenderTaskAddress(0),
                                            z: 0,
                                            segment_index: 0,
                                            user_data0: 0,
                                            user_data1: 0,
                                        };
                                        let brush = &ctx.prim_store.cpu_brushes[sub_metadata.cpu_prim_index.0];
                                        let batch = match brush.kind {
                                            BrushKind::Solid { .. } |
                                            BrushKind::Clear => {
                                                unreachable!("bug: unexpected brush here");
                                            }
                                            BrushKind::Mask { ref kind, .. } => {
                                                match *kind {
                                                    BrushMaskKind::Corner(..) => &mut self.brush_mask_corners,
                                                    BrushMaskKind::RoundedRect(..) => &mut self.brush_mask_rounded_rects,
                                                }
                                            }
                                        };
                                        batch.push(PrimitiveInstance::from(instance));
                                    }
                                    _ => {
                                        unreachable!("Unexpected sub primitive type");
                                    }
                                }
                            }
                        }
                    }
                    _ => {
                        // No other primitives make use of primitive caching yet!
                        unreachable!()
                    }
                }
            }
            RenderTaskKind::CacheMask(ref task_info) => {
                let task_address = render_tasks.get_task_address(task_id);
                self.clip_batcher.add(
                    task_address,
                    &task_info.clips,
                    task_info.coordinate_system_id,
                    &ctx.resource_cache,
                    gpu_cache,
                    clip_store,
                );
            }
            RenderTaskKind::Scaling(..) => {
                self.scalings.push(ScalingInfo {
                    src_task_id: task.children[0],
                    dest_task_id: task_id,
                });
            }
        }
    }

    fn used_rect(&self) -> DeviceIntRect {
        self.allocator.used_rect
    }

    fn needs_depth(&self) -> bool {
        false
    }
}


pub enum RenderPassKind {
    MainFramebuffer(ColorRenderTarget),
    OffScreen {
        alpha: RenderTargetList<AlphaRenderTarget>,
        color: RenderTargetList<ColorRenderTarget>,
    },
}

/// A render pass represents a set of rendering operations that don't depend on one
/// another.
///
/// A render pass can have several render targets if there wasn't enough space in one
/// target to do all of the rendering for that pass.
pub struct RenderPass {
    pub kind: RenderPassKind,
    tasks: Vec<RenderTaskId>,
    dynamic_tasks: FastHashMap<RenderTaskKey, DynamicTaskInfo>,
}

impl RenderPass {
    pub fn new_main_framebuffer(screen_size: DeviceIntSize) -> Self {
        let target = ColorRenderTarget::new(None, screen_size);
        RenderPass {
            kind: RenderPassKind::MainFramebuffer(target),
            tasks: vec![],
            dynamic_tasks: FastHashMap::default(),
        }
    }

    pub fn new_off_screen(screen_size: DeviceIntSize) -> Self {
        RenderPass {
            kind: RenderPassKind::OffScreen {
                color: RenderTargetList::new(screen_size, ImageFormat::BGRA8),
                alpha: RenderTargetList::new(screen_size, ImageFormat::A8),
            },
            tasks: vec![],
            dynamic_tasks: FastHashMap::default(),
        }
    }

    pub fn add_render_task(
        &mut self,
        task_id: RenderTaskId,
        size: DeviceIntSize,
        target_kind: RenderTargetKind,
    ) {
        if let RenderPassKind::OffScreen { ref mut color, ref mut alpha } = self.kind {
            let max_size = match target_kind {
                RenderTargetKind::Color => &mut color.max_size,
                RenderTargetKind::Alpha => &mut alpha.max_size,
            };
            max_size.width = cmp::max(max_size.width, size.width as u32);
            max_size.height = cmp::max(max_size.height, size.height as u32);
        }

        self.tasks.push(task_id);
    }

    pub fn build(
        &mut self,
        ctx: &RenderTargetContext,
        gpu_cache: &mut GpuCache,
        render_tasks: &mut RenderTaskTree,
        deferred_resolves: &mut Vec<DeferredResolve>,
        clip_store: &ClipStore,
        pass_index: RenderPassIndex,
    ) {
        profile_scope!("RenderPass::build");

        match self.kind {
            RenderPassKind::MainFramebuffer(ref mut target) => {
                for &task_id in &self.tasks {
                    assert_eq!(render_tasks[task_id].target_kind(), RenderTargetKind::Color);
                    render_tasks[task_id].pass_index = Some(pass_index);
                    target.add_task(task_id, ctx, gpu_cache, render_tasks, clip_store);
                }
                target.build(ctx, gpu_cache, render_tasks, deferred_resolves);
            }
            RenderPassKind::OffScreen { ref mut color, ref mut alpha } => {
                // Step through each task, adding to batches as appropriate.
                for &task_id in &self.tasks {
                    let target_kind = {
                        let task = &mut render_tasks[task_id];
                        task.pass_index = Some(pass_index);
                        let target_kind = task.target_kind();

                        // Find a target to assign this task to, or create a new
                        // one if required.
                        match task.location {
                            RenderTaskLocation::Fixed => {}
                            RenderTaskLocation::Dynamic(ref mut origin, size) => {
                                let dynamic_entry = match task.cache_key {
                                    // See if this task is a duplicate.
                                    // If so, just skip adding it!
                                    Some(cache_key) => match self.dynamic_tasks.entry(cache_key) {
                                        Entry::Occupied(entry) => {
                                            debug_assert_eq!(entry.get().rect.size, size);
                                            task.kind = RenderTaskKind::Alias(entry.get().task_id);
                                            continue;
                                        },
                                        Entry::Vacant(entry) => Some(entry),
                                    },
                                    None => None,
                                };

                                let alloc_size = DeviceUintSize::new(size.width as u32, size.height as u32);
                                let (alloc_origin, target_index) =  match target_kind {
                                    RenderTargetKind::Color => color.allocate(alloc_size),
                                    RenderTargetKind::Alpha => alpha.allocate(alloc_size),
                                };
                                *origin = Some((alloc_origin.to_i32(), target_index));

                                // If this task is cacheable / sharable, store it in the task hash
                                // for this pass.
                                if let Some(entry) = dynamic_entry {
                                    entry.insert(DynamicTaskInfo {
                                        task_id,
                                        rect: DeviceIntRect::new(alloc_origin.to_i32(), size),
                                    });
                                }
                            }
                        }

                        target_kind
                    };

                    match target_kind {
                        RenderTargetKind::Color => color.add_task(task_id, ctx, gpu_cache, render_tasks, clip_store),
                        RenderTargetKind::Alpha => alpha.add_task(task_id, ctx, gpu_cache, render_tasks, clip_store),
                    }
                }

                color.build(ctx, gpu_cache, render_tasks, deferred_resolves);
                alpha.build(ctx, gpu_cache, render_tasks, deferred_resolves);
            }
        }
    }
}

#[derive(Debug, Clone, Default)]
pub struct CompositeOps {
    // Requires only a single texture as input (e.g. most filters)
    pub filters: Vec<FilterOp>,

    // Requires two source textures (e.g. mix-blend-mode)
    pub mix_blend_mode: Option<MixBlendMode>,
}

impl CompositeOps {
    pub fn new(filters: Vec<FilterOp>, mix_blend_mode: Option<MixBlendMode>) -> Self {
        CompositeOps {
            filters,
            mix_blend_mode,
        }
    }

    pub fn count(&self) -> usize {
        self.filters.len() + if self.mix_blend_mode.is_some() { 1 } else { 0 }
    }
}

/// A rendering-oriented representation of frame::Frame built by the render backend
/// and presented to the renderer.
pub struct Frame {
    pub window_size: DeviceUintSize,
    pub inner_rect: DeviceUintRect,
    pub background_color: Option<ColorF>,
    pub layer: DocumentLayer,
    pub device_pixel_ratio: f32,
    pub passes: Vec<RenderPass>,
    pub profile_counters: FrameProfileCounters,

    pub node_data: Vec<ClipScrollNodeData>,
    pub clip_chain_local_clip_rects: Vec<LayerRect>,
    pub render_tasks: RenderTaskTree,

    // List of updates that need to be pushed to the
    // gpu resource cache.
    pub gpu_cache_updates: Option<GpuCacheUpdateList>,

    // List of textures that we don't know about yet
    // from the backend thread. The render thread
    // will use a callback to resolve these and
    // patch the data structures.
    pub deferred_resolves: Vec<DeferredResolve>,
}

impl BlurTask {
    fn add_instances(
        &self,
        instances: &mut Vec<BlurInstance>,
        task_id: RenderTaskId,
        source_task_id: RenderTaskId,
        blur_direction: BlurDirection,
        render_tasks: &RenderTaskTree,
    ) {
        let instance = BlurInstance {
            task_address: render_tasks.get_task_address(task_id),
            src_task_address: render_tasks.get_task_address(source_task_id),
            blur_direction,
            region: LayerRect::zero(),
        };

        if self.regions.is_empty() {
            instances.push(instance);
        } else {
            for region in &self.regions {
                instances.push(BlurInstance {
                    region: *region,
                    ..instance
                });
            }
        }
    }
}
