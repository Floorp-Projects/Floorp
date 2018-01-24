/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{ClipId, ColorF, DeviceIntPoint, DeviceIntRect, DeviceIntSize};
use api::{DevicePixelScale, DeviceUintPoint, DeviceUintRect, DeviceUintSize};
use api::{DocumentLayer, FilterOp, ImageFormat};
use api::{LayerRect, MixBlendMode, PipelineId};
use batch::{AlphaBatcher, ClipBatcher, resolve_image};
use clip::{ClipStore};
use clip_scroll_tree::{ClipScrollTree};
use device::Texture;
use gpu_cache::{GpuCache, GpuCacheUpdateList};
use gpu_types::{BlurDirection, BlurInstance, BrushInstance, ClipChainRectIndex};
use gpu_types::{ClipScrollNodeData, ClipScrollNodeIndex};
use gpu_types::{PrimitiveInstance};
use internal_types::{FastHashMap, RenderPassIndex, SourceTexture};
use picture::{PictureKind};
use prim_store::{PrimitiveIndex, PrimitiveKind, PrimitiveStore};
use prim_store::{BrushMaskKind, BrushKind, DeferredResolve, EdgeAaSegmentMask};
use profiler::FrameProfileCounters;
use render_task::{BlitSource, RenderTaskAddress, RenderTaskId, RenderTaskKind};
use render_task::{BlurTask, ClearMode, RenderTaskLocation, RenderTaskTree};
use resource_cache::{ResourceCache};
use std::{cmp, usize, f32, i32};
use texture_allocator::GuillotineAllocator;

const MIN_TARGET_SIZE: u32 = 2048;

#[derive(Debug)]
pub struct ScrollbarPrimitive {
    pub clip_id: ClipId,
    pub prim_index: PrimitiveIndex,
    pub frame_rect: LayerRect,
}

#[derive(Debug, Copy, Clone)]
#[cfg_attr(feature = "capture", derive(Deserialize, Serialize))]
pub struct RenderTargetIndex(pub usize);

pub struct RenderTargetContext<'a> {
    pub device_pixel_scale: DevicePixelScale,
    pub prim_store: &'a PrimitiveStore,
    pub resource_cache: &'a ResourceCache,
    pub node_data: &'a [ClipScrollNodeData],
    pub clip_scroll_tree: &'a ClipScrollTree,
    pub use_dual_source_blending: bool,
}

#[cfg_attr(feature = "capture", derive(Deserialize, Serialize))]
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
    // TODO(gw): It's a bit odd that we need the deferred resolves and mutable
    //           GPU cache here. They are typically used by the build step
    //           above. They are used for the blit jobs to allow resolve_image
    //           to be called. It's a bit of extra overhead to store the image
    //           key here and the resolve them in the build step separately.
    //           BUT: if/when we add more texture cache target jobs, we might
    //           want to tidy this up.
    fn add_task(
        &mut self,
        task_id: RenderTaskId,
        ctx: &RenderTargetContext,
        gpu_cache: &mut GpuCache,
        render_tasks: &RenderTaskTree,
        clip_store: &ClipStore,
        deferred_resolves: &mut Vec<DeferredResolve>,
    );
    fn used_rect(&self) -> DeviceIntRect;
    fn needs_depth(&self) -> bool;
}

#[derive(Debug, Copy, Clone, Eq, PartialEq, Hash)]
#[cfg_attr(feature = "capture", derive(Deserialize, Serialize))]
pub enum RenderTargetKind {
    Color, // RGBA32
    Alpha, // R8
}

#[cfg_attr(feature = "capture", derive(Deserialize, Serialize))]
pub struct RenderTargetList<T> {
    screen_size: DeviceIntSize,
    pub format: ImageFormat,
    pub max_size: DeviceUintSize,
    pub targets: Vec<T>,
    #[cfg_attr(feature = "capture", serde(skip))]
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
        gpu_cache: &mut GpuCache,
        render_tasks: &mut RenderTaskTree,
        clip_store: &ClipStore,
        deferred_resolves: &mut Vec<DeferredResolve>,
    ) {
        self.targets.last_mut().unwrap().add_task(
            task_id,
            ctx,
            gpu_cache,
            render_tasks,
            clip_store,
            deferred_resolves,
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

    pub fn check_ready(&self) {
        match self.texture {
            Some(ref t) => {
                assert_eq!(t.get_dimensions(), self.max_size);
                assert_eq!(t.get_format(), self.format);
                assert_eq!(t.get_render_target_layer_count(), self.targets.len());
                assert_eq!(t.get_layer_count() as usize, self.targets.len());
                assert_eq!(t.has_depth(), t.get_rt_info().unwrap().has_depth);
                assert_eq!(t.has_depth(), self.needs_depth());
            }
            None => {
                assert!(self.targets.is_empty())
            }
        }
    }
}

/// Frame output information for a given pipeline ID.
/// Storing the task ID allows the renderer to find
/// the target rect within the render target that this
/// pipeline exists at.
#[cfg_attr(feature = "capture", derive(Deserialize, Serialize))]
pub struct FrameOutput {
    pub task_id: RenderTaskId,
    pub pipeline_id: PipelineId,
}

#[cfg_attr(feature = "capture", derive(Deserialize, Serialize))]
pub struct ScalingInfo {
    pub src_task_id: RenderTaskId,
    pub dest_task_id: RenderTaskId,
}

// Defines where the source data for a blit job can be found.
#[cfg_attr(feature = "capture", derive(Deserialize, Serialize))]
pub enum BlitJobSource {
    Texture(SourceTexture, i32, DeviceIntRect),
    RenderTask(RenderTaskId),
}

// Information required to do a blit from a source to a target.
#[cfg_attr(feature = "capture", derive(Deserialize, Serialize))]
pub struct BlitJob {
    pub source: BlitJobSource,
    pub target_rect: DeviceIntRect,
}

/// A render target represents a number of rendering operations on a surface.
#[cfg_attr(feature = "capture", derive(Deserialize, Serialize))]
pub struct ColorRenderTarget {
    pub alpha_batcher: AlphaBatcher,
    // List of blur operations to apply for this render target.
    pub vertical_blurs: Vec<BlurInstance>,
    pub horizontal_blurs: Vec<BlurInstance>,
    pub readbacks: Vec<DeviceIntRect>,
    pub scalings: Vec<ScalingInfo>,
    pub blits: Vec<BlitJob>,
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
            blits: Vec::new(),
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
        gpu_cache: &mut GpuCache,
        render_tasks: &RenderTaskTree,
        _: &ClipStore,
        deferred_resolves: &mut Vec<DeferredResolve>,
    ) {
        let task = &render_tasks[task_id];

        match task.kind {
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
            RenderTaskKind::Blit(ref task_info) => {
                match task_info.source {
                    BlitSource::Image { key } => {
                        // Get the cache item for the source texture.
                        let cache_item = resolve_image(
                            key.image_key,
                            key.image_rendering,
                            key.tile_offset,
                            ctx.resource_cache,
                            gpu_cache,
                            deferred_resolves,
                        );

                        // Work out a source rect to copy from the texture, depending on whether
                        // a sub-rect is present or not.
                        // TODO(gw): We have much type confusion below - f32, i32 and u32 for
                        //           various representations of the texel rects. We should make
                        //           this consistent!
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
                        let (target_rect, _) = task.get_target_rect();
                        self.blits.push(BlitJob {
                            source: BlitJobSource::Texture(
                                cache_item.texture_id,
                                cache_item.texture_layer,
                                source_rect,
                            ),
                            target_rect,
                        });
                    }
                    BlitSource::RenderTask { .. } => {
                        panic!("BUG: render task blit jobs to render tasks not supported");
                    }
                }
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

#[cfg_attr(feature = "capture", derive(Deserialize, Serialize))]
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
        gpu_cache: &mut GpuCache,
        render_tasks: &RenderTaskTree,
        clip_store: &ClipStore,
        _: &mut Vec<DeferredResolve>,
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
            RenderTaskKind::Readback(..) |
            RenderTaskKind::Blit(..) => {
                panic!("BUG: should not be added to alpha target!");
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
                                            edge_flags: EdgeAaSegmentMask::empty(),
                                            user_data0: 0,
                                            user_data1: 0,
                                        };
                                        let brush = &ctx.prim_store.cpu_brushes[sub_metadata.cpu_prim_index.0];
                                        let batch = match brush.kind {
                                            BrushKind::Solid { .. } |
                                            BrushKind::Clear |
                                            BrushKind::Picture |
                                            BrushKind::Line { .. } => {
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

#[cfg_attr(feature = "capture", derive(Deserialize, Serialize))]
pub struct TextureCacheRenderTarget {
    pub horizontal_blurs: Vec<BlurInstance>,
    pub blits: Vec<BlitJob>,
}

impl TextureCacheRenderTarget {
    fn new(
        _size: Option<DeviceUintSize>,
        _screen_size: DeviceIntSize,
    ) -> Self {
        TextureCacheRenderTarget {
            horizontal_blurs: Vec::new(),
            blits: Vec::new(),
        }
    }

    fn add_task(
        &mut self,
        task_id: RenderTaskId,
        render_tasks: &RenderTaskTree,
    ) {
        let task = &render_tasks[task_id];

        match task.kind {
            RenderTaskKind::HorizontalBlur(ref info) => {
                info.add_instances(
                    &mut self.horizontal_blurs,
                    task_id,
                    task.children[0],
                    BlurDirection::Horizontal,
                    render_tasks,
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
                        let (target_rect, _) = task.get_target_rect();
                        self.blits.push(BlitJob {
                            source: BlitJobSource::RenderTask(task_id),
                            target_rect,
                        });
                    }
                }
            }
            RenderTaskKind::VerticalBlur(..) |
            RenderTaskKind::Picture(..) |
            RenderTaskKind::CacheMask(..) |
            RenderTaskKind::Readback(..) |
            RenderTaskKind::Scaling(..) => {
                panic!("BUG: unexpected task kind for texture cache target");
            }
        }
    }
}

#[cfg_attr(feature = "capture", derive(Deserialize, Serialize))]
pub enum RenderPassKind {
    MainFramebuffer(ColorRenderTarget),
    OffScreen {
        alpha: RenderTargetList<AlphaRenderTarget>,
        color: RenderTargetList<ColorRenderTarget>,
        texture_cache: FastHashMap<(SourceTexture, i32), TextureCacheRenderTarget>,
    },
}

/// A render pass represents a set of rendering operations that don't depend on one
/// another.
///
/// A render pass can have several render targets if there wasn't enough space in one
/// target to do all of the rendering for that pass.
#[cfg_attr(feature = "capture", derive(Deserialize, Serialize))]
pub struct RenderPass {
    pub kind: RenderPassKind,
    tasks: Vec<RenderTaskId>,
}

impl RenderPass {
    pub fn new_main_framebuffer(screen_size: DeviceIntSize) -> Self {
        let target = ColorRenderTarget::new(None, screen_size);
        RenderPass {
            kind: RenderPassKind::MainFramebuffer(target),
            tasks: vec![],
        }
    }

    pub fn new_off_screen(screen_size: DeviceIntSize) -> Self {
        RenderPass {
            kind: RenderPassKind::OffScreen {
                color: RenderTargetList::new(screen_size, ImageFormat::BGRA8),
                alpha: RenderTargetList::new(screen_size, ImageFormat::R8),
                texture_cache: FastHashMap::default(),
            },
            tasks: vec![],
        }
    }

    pub fn add_render_task(
        &mut self,
        task_id: RenderTaskId,
        size: DeviceIntSize,
        target_kind: RenderTargetKind,
    ) {
        if let RenderPassKind::OffScreen { ref mut color, ref mut alpha, .. } = self.kind {
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
                    target.add_task(
                        task_id,
                        ctx,
                        gpu_cache,
                        render_tasks,
                        clip_store,
                        deferred_resolves,
                    );
                }
                target.build(ctx, gpu_cache, render_tasks, deferred_resolves);
            }
            RenderPassKind::OffScreen { ref mut color, ref mut alpha, ref mut texture_cache } => {
                // Step through each task, adding to batches as appropriate.
                for &task_id in &self.tasks {
                    let (target_kind, texture_target) = {
                        let task = &mut render_tasks[task_id];
                        task.pass_index = Some(pass_index);
                        let target_kind = task.target_kind();

                        // Find a target to assign this task to, or create a new
                        // one if required.
                        match task.location {
                            RenderTaskLocation::TextureCache(texture_id, layer, _) => {
                                // TODO(gw): When we support caching color items, we will
                                //           need to calculate that here to get the
                                //           correct target kind.
                                (RenderTargetKind::Alpha, Some((texture_id, layer)))
                            }
                            RenderTaskLocation::Fixed => {
                                (RenderTargetKind::Color, None)
                            }
                            RenderTaskLocation::Dynamic(ref mut origin, size) => {
                                let alloc_size = DeviceUintSize::new(size.width as u32, size.height as u32);
                                let (alloc_origin, target_index) =  match target_kind {
                                    RenderTargetKind::Color => color.allocate(alloc_size),
                                    RenderTargetKind::Alpha => alpha.allocate(alloc_size),
                                };
                                *origin = Some((alloc_origin.to_i32(), target_index));

                                (target_kind, None)
                            }
                        }
                    };

                    match texture_target {
                        Some(texture_target) => {
                            let texture = texture_cache
                                .entry(texture_target)
                                .or_insert(
                                    TextureCacheRenderTarget::new(None, DeviceIntSize::zero())
                                );
                            texture.add_task(task_id, render_tasks);
                        }
                        None => {
                            match target_kind {
                                RenderTargetKind::Color => color.add_task(
                                    task_id,
                                    ctx,
                                    gpu_cache,
                                    render_tasks,
                                    clip_store,
                                    deferred_resolves,
                                ),
                                RenderTargetKind::Alpha => alpha.add_task(
                                    task_id,
                                    ctx,
                                    gpu_cache,
                                    render_tasks,
                                    clip_store,
                                    deferred_resolves,
                                ),
                            }
                        }
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
#[cfg_attr(feature = "capture", derive(Deserialize, Serialize))]
pub struct Frame {
    pub window_size: DeviceUintSize,
    pub inner_rect: DeviceUintRect,
    pub background_color: Option<ColorF>,
    pub layer: DocumentLayer,
    pub device_pixel_ratio: f32,
    pub passes: Vec<RenderPass>,
    #[cfg_attr(feature = "capture", serde(default = "FrameProfileCounters::new", skip))]
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

    // True if this frame contains any render tasks
    // that write to the texture cache.
    pub has_texture_cache_tasks: bool,

    // True if this frame has been drawn by the
    // renderer.
    pub has_been_rendered: bool,
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
        task_id: RenderTaskId,
        source_task_id: RenderTaskId,
        blur_direction: BlurDirection,
        render_tasks: &RenderTaskTree,
    ) {
        let instance = BlurInstance {
            task_address: render_tasks.get_task_address(task_id),
            src_task_address: render_tasks.get_task_address(source_task_id),
            blur_direction,
        };

        instances.push(instance);
    }
}
