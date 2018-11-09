/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{DeviceIntPoint, DeviceIntRect, DeviceIntSize, DeviceSize, DeviceIntSideOffsets};
use api::{DevicePixelScale, ImageDescriptor, ImageFormat};
use api::{LineStyle, LineOrientation, LayoutSize};
#[cfg(feature = "pathfinder")]
use api::FontRenderMode;
use border::{BorderCornerCacheKey, BorderEdgeCacheKey};
use box_shadow::{BoxShadowCacheKey};
use clip::{ClipDataStore, ClipItem, ClipStore, ClipNodeRange};
use clip_scroll_tree::SpatialNodeIndex;
use device::TextureFilter;
#[cfg(feature = "pathfinder")]
use euclid::{TypedPoint2D, TypedVector2D};
use freelist::{FreeList, FreeListHandle, WeakFreeListHandle};
use glyph_rasterizer::GpuGlyphCacheKey;
use gpu_cache::{GpuCache, GpuCacheAddress, GpuCacheHandle};
use gpu_types::{BorderInstance, ImageSource, UvRectKind};
use internal_types::{CacheTextureId, FastHashMap, LayerIndex, SavedTargetIndex};
#[cfg(feature = "pathfinder")]
use pathfinder_partitioner::mesh::Mesh;
use prim_store::{PictureIndex, ImageCacheKey, LineDecorationCacheKey};
#[cfg(feature = "debugger")]
use print_tree::{PrintTreePrinter};
use render_backend::FrameId;
use resource_cache::{CacheItem, ResourceCache};
use surface::SurfaceCacheKey;
use std::{cmp, ops, mem, usize, f32, i32};
use texture_cache::{TextureCache, TextureCacheHandle, Eviction};
use tiling::{RenderPass, RenderTargetIndex};
use tiling::{RenderTargetKind};
#[cfg(feature = "pathfinder")]
use webrender_api::DevicePixel;

const RENDER_TASK_SIZE_SANITY_CHECK: i32 = 16000;
const FLOATS_PER_RENDER_TASK_INFO: usize = 8;
pub const MAX_BLUR_STD_DEVIATION: f32 = 4.0;
pub const MIN_DOWNSCALING_RT_SIZE: i32 = 128;

fn render_task_sanity_check(size: &DeviceIntSize) {
    if size.width > RENDER_TASK_SIZE_SANITY_CHECK ||
        size.height > RENDER_TASK_SIZE_SANITY_CHECK {
        error!("Attempting to create a render task of size {}x{}", size.width, size.height);
        panic!();
    }
}

#[derive(Copy, Clone, PartialEq, Eq, Hash, Debug)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct RenderTaskId(pub u32, FrameId); // TODO(gw): Make private when using GPU cache!

#[derive(Debug, Copy, Clone, PartialEq)]
#[repr(C)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct RenderTaskAddress(pub u32);

#[derive(Debug)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct RenderTaskTree {
    pub tasks: Vec<RenderTask>,
    pub task_data: Vec<RenderTaskData>,
    next_saved: SavedTargetIndex,
    frame_id: FrameId,
}

impl RenderTaskTree {
    pub fn new(frame_id: FrameId) -> Self {
        RenderTaskTree {
            tasks: Vec::new(),
            task_data: Vec::new(),
            next_saved: SavedTargetIndex(0),
            frame_id,
        }
    }

    pub fn add(&mut self, task: RenderTask) -> RenderTaskId {
        let id = self.tasks.len();
        self.tasks.push(task);
        RenderTaskId(id as _, self.frame_id)
    }

    pub fn max_depth(&self, id: RenderTaskId, depth: usize, max_depth: &mut usize) {
        debug_assert_eq!(self.frame_id, id.1);
        let depth = depth + 1;
        *max_depth = cmp::max(*max_depth, depth);
        let task = &self.tasks[id.0 as usize];
        for child in &task.children {
            self.max_depth(*child, depth, max_depth);
        }
    }

    pub fn assign_to_passes(
        &self,
        id: RenderTaskId,
        pass_index: usize,
        passes: &mut [RenderPass],
    ) {
        debug_assert_eq!(self.frame_id, id.1);
        let task = &self.tasks[id.0 as usize];

        for child in &task.children {
            self.assign_to_passes(*child, pass_index - 1, passes);
        }

        // Sanity check - can be relaxed if needed
        match task.location {
            RenderTaskLocation::Fixed(..) => {
                debug_assert!(pass_index == passes.len() - 1);
            }
            RenderTaskLocation::Dynamic(..) |
            RenderTaskLocation::TextureCache { .. } => {
                debug_assert!(pass_index < passes.len() - 1);
            }
        }

        let pass_index = if task.is_global_cached_task() {
            0
        } else {
            pass_index
        };

        let pass = &mut passes[pass_index];
        pass.add_render_task(id, task.get_dynamic_size(), task.target_kind(), &task.location);
    }

    pub fn prepare_for_render(&mut self) {
        for task in &mut self.tasks {
            task.prepare_for_render();
        }
    }

    pub fn get_task_address(&self, id: RenderTaskId) -> RenderTaskAddress {
        debug_assert_eq!(self.frame_id, id.1);
        RenderTaskAddress(id.0)
    }

    pub fn write_task_data(&mut self, device_pixel_scale: DevicePixelScale) {
        for task in &self.tasks {
            self.task_data.push(task.write_task_data(device_pixel_scale));
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

impl ops::Index<RenderTaskId> for RenderTaskTree {
    type Output = RenderTask;
    fn index(&self, id: RenderTaskId) -> &RenderTask {
        debug_assert_eq!(self.frame_id, id.1);
        &self.tasks[id.0 as usize]
    }
}

impl ops::IndexMut<RenderTaskId> for RenderTaskTree {
    fn index_mut(&mut self, id: RenderTaskId) -> &mut RenderTask {
        debug_assert_eq!(self.frame_id, id.1);
        &mut self.tasks[id.0 as usize]
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
}

impl RenderTaskLocation {
    /// Returns true if this is a dynamic location.
    pub fn is_dynamic(&self) -> bool {
        match *self {
            RenderTaskLocation::Dynamic(..) => true,
            _ => false,
        }
    }
}

#[derive(Debug)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct CacheMaskTask {
    actual_rect: DeviceIntRect,
    pub root_spatial_node_index: SpatialNodeIndex,
    pub clip_node_range: ClipNodeRange,
}

#[derive(Debug)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct ClipRegionTask {
    pub clip_data_address: GpuCacheAddress,
}

#[derive(Debug)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct PictureTask {
    pub pic_index: PictureIndex,
    pub can_merge: bool,
    pub content_origin: DeviceIntPoint,
    pub uv_rect_handle: GpuCacheHandle,
    pub root_spatial_node_index: SpatialNodeIndex,
    uv_rect_kind: UvRectKind,
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
    pub uv_rect_handle: GpuCacheHandle,
    uv_rect_kind: UvRectKind,
}

#[derive(Debug)]
#[cfg(feature = "pathfinder")]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct GlyphTask {
    /// After job building, this becomes `None`.
    pub mesh: Option<Mesh>,
    pub origin: DeviceIntPoint,
    pub subpixel_offset: TypedPoint2D<f32, DevicePixel>,
    pub render_mode: FontRenderMode,
    pub embolden_amount: TypedVector2D<f32, DevicePixel>,
}

#[cfg(not(feature = "pathfinder"))]
#[derive(Debug)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct GlyphTask;

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
pub struct LineDecorationTask {
    pub wavy_line_thickness: f32,
    pub style: LineStyle,
    pub orientation: LineOrientation,
    pub local_size: LayoutSize,
}

#[derive(Debug)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct RenderTaskData {
    pub data: [f32; FLOATS_PER_RENDER_TASK_INFO],
}

#[derive(Debug)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub enum RenderTaskKind {
    Picture(PictureTask),
    CacheMask(CacheMaskTask),
    ClipRegion(ClipRegionTask),
    VerticalBlur(BlurTask),
    HorizontalBlur(BlurTask),
    #[allow(dead_code)]
    Glyph(GlyphTask),
    Readback(DeviceIntRect),
    Scaling(ScalingTask),
    Blit(BlitTask),
    Border(BorderTask),
    LineDecoration(LineDecorationTask),
}

#[derive(Debug, Copy, Clone, Eq, PartialEq, Hash)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub enum ClearMode {
    // Applicable to color and alpha targets.
    Zero,
    One,

    // Applicable to color targets only.
    Transparent,
}

#[derive(Debug)]
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

    pub fn new_picture(
        location: RenderTaskLocation,
        unclipped_size: DeviceSize,
        pic_index: PictureIndex,
        content_origin: DeviceIntPoint,
        children: Vec<RenderTaskId>,
        uv_rect_kind: UvRectKind,
        root_spatial_node_index: SpatialNodeIndex,
    ) -> Self {
        let size = match location {
            RenderTaskLocation::Dynamic(_, size) => size,
            RenderTaskLocation::Fixed(rect) => rect.size,
            RenderTaskLocation::TextureCache { rect, .. } => rect.size,
        };

        render_task_sanity_check(&size);

        let can_merge = size.width as f32 >= unclipped_size.width &&
                        size.height as f32 >= unclipped_size.height;

        RenderTask {
            location,
            children,
            kind: RenderTaskKind::Picture(PictureTask {
                pic_index,
                content_origin,
                can_merge,
                uv_rect_handle: GpuCacheHandle::new(),
                uv_rect_kind,
                root_spatial_node_index,
            }),
            clear_mode: ClearMode::Transparent,
            saved_index: None,
        }
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
        RenderTask::new_blit_with_padding(size, &DeviceIntSideOffsets::zero(), source)
    }

    pub fn new_blit_with_padding(
        mut size: DeviceIntSize,
        padding: &DeviceIntSideOffsets,
        source: BlitSource,
    ) -> Self {
        let mut children = Vec::new();

        // If this blit uses a render task as a source,
        // ensure it's added as a child task. This will
        // ensure it gets allocated in the correct pass
        // and made available as an input when this task
        // executes.
        if let BlitSource::RenderTask { task_id } = source {
            children.push(task_id);
        }

        size.width += padding.horizontal();
        size.height += padding.vertical();

        RenderTask::with_dynamic_location(
            size,
            children,
            RenderTaskKind::Blit(BlitTask {
                source,
                padding: *padding,
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
        render_tasks: &mut RenderTaskTree,
        clip_data_store: &mut ClipDataStore,
    ) -> Self {
        let mut children = Vec::new();

        // Step through the clip sources that make up this mask. If we find
        // any box-shadow clip sources, request that image from the render
        // task cache. This allows the blurred box-shadow rect to be cached
        // in the texture cache across frames.
        // TODO(gw): Consider moving this logic outside this function, especially
        //           as we add more clip sources that depend on render tasks.
        // TODO(gw): If this ever shows up in a profile, we could pre-calculate
        //           whether a ClipSources contains any box-shadows and skip
        //           this iteration for the majority of cases.
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
                            );

                            let mask_task_id = render_tasks.add(mask_task);

                            // Blur it
                            let blur_render_task = RenderTask::new_blur(
                                blur_radius_dp,
                                mask_task_id,
                                render_tasks,
                                RenderTargetKind::Alpha,
                                ClearMode::Zero,
                            );

                            let root_task_id = render_tasks.add(blur_render_task);
                            children.push(root_task_id);

                            root_task_id
                        }
                    ));
                }
                ClipItem::Rectangle(..) |
                ClipItem::RoundedRectangle(..) |
                ClipItem::Image { .. } => {}
            }
        }

        RenderTask::with_dynamic_location(
            outer_rect.size,
            children,
            RenderTaskKind::CacheMask(CacheMaskTask {
                actual_rect: outer_rect,
                clip_node_range,
                root_spatial_node_index,
            }),
            ClearMode::One,
        )
    }

    pub fn new_rounded_rect_mask(
        size: DeviceIntSize,
        clip_data_address: GpuCacheAddress,
    ) -> Self {
        RenderTask::with_dynamic_location(
            size,
            Vec::new(),
            RenderTaskKind::ClipRegion(ClipRegionTask {
                clip_data_address,
            }),
            ClearMode::One,
        )
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
        blur_std_deviation: f32,
        src_task_id: RenderTaskId,
        render_tasks: &mut RenderTaskTree,
        target_kind: RenderTargetKind,
        clear_mode: ClearMode,
    ) -> Self {
        // Adjust large std deviation value.
        let mut adjusted_blur_std_deviation = blur_std_deviation;
        let (blur_target_size, uv_rect_kind) = {
            let src_task = &render_tasks[src_task_id];
            (src_task.get_dynamic_size(), src_task.uv_rect_kind())
        };
        let mut adjusted_blur_target_size = blur_target_size;
        let mut downscaling_src_task_id = src_task_id;
        let mut scale_factor = 1.0;
        while adjusted_blur_std_deviation > MAX_BLUR_STD_DEVIATION {
            if adjusted_blur_target_size.width < MIN_DOWNSCALING_RT_SIZE ||
               adjusted_blur_target_size.height < MIN_DOWNSCALING_RT_SIZE {
                break;
            }
            adjusted_blur_std_deviation *= 0.5;
            scale_factor *= 2.0;
            adjusted_blur_target_size = (blur_target_size.to_f32() / scale_factor).to_i32();
            let downscaling_task = RenderTask::new_scaling(
                downscaling_src_task_id,
                render_tasks,
                target_kind,
                adjusted_blur_target_size,
            );
            downscaling_src_task_id = render_tasks.add(downscaling_task);
        }

        let blur_task_v = RenderTask::with_dynamic_location(
            adjusted_blur_target_size,
            vec![downscaling_src_task_id],
            RenderTaskKind::VerticalBlur(BlurTask {
                blur_std_deviation: adjusted_blur_std_deviation,
                target_kind,
                uv_rect_handle: GpuCacheHandle::new(),
                uv_rect_kind,
            }),
            clear_mode,
        );

        let blur_task_v_id = render_tasks.add(blur_task_v);

        RenderTask::with_dynamic_location(
            adjusted_blur_target_size,
            vec![blur_task_v_id],
            RenderTaskKind::HorizontalBlur(BlurTask {
                blur_std_deviation: adjusted_blur_std_deviation,
                target_kind,
                uv_rect_handle: GpuCacheHandle::new(),
                uv_rect_kind,
            }),
            clear_mode,
        )
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
        render_tasks: &mut RenderTaskTree,
        target_kind: RenderTargetKind,
        target_size: DeviceIntSize,
    ) -> Self {
        let uv_rect_kind = render_tasks[src_task_id].uv_rect_kind();

        RenderTask::with_dynamic_location(
            target_size,
            vec![src_task_id],
            RenderTaskKind::Scaling(ScalingTask {
                target_kind,
                uv_rect_handle: GpuCacheHandle::new(),
                uv_rect_kind,
            }),
            match target_kind {
                RenderTargetKind::Color => ClearMode::Transparent,
                RenderTargetKind::Alpha => ClearMode::One,
            },
        )
    }

    #[cfg(feature = "pathfinder")]
    pub fn new_glyph(location: RenderTaskLocation,
                     mesh: Mesh,
                     origin: &DeviceIntPoint,
                     subpixel_offset: &TypedPoint2D<f32, DevicePixel>,
                     render_mode: FontRenderMode,
                     embolden_amount: &TypedVector2D<f32, DevicePixel>)
                     -> Self {
        RenderTask {
            children: vec![],
            location: location,
            kind: RenderTaskKind::Glyph(GlyphTask {
                mesh: Some(mesh),
                origin: *origin,
                subpixel_offset: *subpixel_offset,
                render_mode: render_mode,
                embolden_amount: *embolden_amount,
            }),
            clear_mode: ClearMode::Transparent,
            saved_index: None,
        }
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

            RenderTaskKind::ClipRegion(..) |
            RenderTaskKind::Glyph(_) |
            RenderTaskKind::Border(..) |
            RenderTaskKind::LineDecoration(..) |
            RenderTaskKind::Blit(..) => {
                UvRectKind::Rect
            }
        }
    }

    // Write (up to) 8 floats of data specific to the type
    // of render task that is provided to the GPU shaders
    // via a vertex texture.
    pub fn write_task_data(&self, device_pixel_scale: DevicePixelScale) -> RenderTaskData {
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
                    task.content_origin.x as f32,
                    task.content_origin.y as f32,
                ]
            }
            RenderTaskKind::CacheMask(ref task) => {
                [
                    task.actual_rect.origin.x as f32,
                    task.actual_rect.origin.y as f32,
                ]
            }
            RenderTaskKind::VerticalBlur(ref task) |
            RenderTaskKind::HorizontalBlur(ref task) => {
                [
                    task.blur_std_deviation,
                    0.0,
                ]
            }
            RenderTaskKind::Glyph(_) => {
                [1.0, 0.0]
            }
            RenderTaskKind::ClipRegion(..) |
            RenderTaskKind::Readback(..) |
            RenderTaskKind::Scaling(..) |
            RenderTaskKind::Border(..) |
            RenderTaskKind::LineDecoration(..) |
            RenderTaskKind::Blit(..) => {
                [0.0; 2]
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
                device_pixel_scale.0,
                data[0],
                data[1],
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
            RenderTaskKind::ClipRegion(..) |
            RenderTaskKind::Readback(..) |
            RenderTaskKind::Scaling(..) |
            RenderTaskKind::Blit(..) |
            RenderTaskKind::Border(..) |
            RenderTaskKind::CacheMask(..) |
            RenderTaskKind::LineDecoration(..) |
            RenderTaskKind::Glyph(..) => {
                panic!("texture handle not supported for this task kind");
            }
        }
    }

    pub fn get_dynamic_size(&self) -> DeviceIntSize {
        match self.location {
            RenderTaskLocation::Fixed(..) => DeviceIntSize::zero(),
            RenderTaskLocation::Dynamic(_, size) => size,
            RenderTaskLocation::TextureCache { rect, .. } => rect.size,
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
        }
    }

    pub fn target_kind(&self) -> RenderTargetKind {
        match self.kind {
            RenderTaskKind::Readback(..) => RenderTargetKind::Color,

            RenderTaskKind::LineDecoration(..) => RenderTargetKind::Color,

            RenderTaskKind::ClipRegion(..) |
            RenderTaskKind::CacheMask(..) => {
                RenderTargetKind::Alpha
            }

            RenderTaskKind::VerticalBlur(ref task_info) |
            RenderTaskKind::HorizontalBlur(ref task_info) => {
                task_info.target_kind
            }

            RenderTaskKind::Glyph(..) => {
                RenderTargetKind::Color
            }

            RenderTaskKind::Scaling(ref task_info) => {
                task_info.target_kind
            }

            RenderTaskKind::Border(..) |
            RenderTaskKind::Picture(..) => {
                RenderTargetKind::Color
            }

            RenderTaskKind::Blit(..) => {
                RenderTargetKind::Color
            }
        }
    }

    /// If true, draw this task in the first pass. This is useful
    /// for simple texture cached render tasks that we want to be made
    /// available to all subsequent render passes.
    pub fn is_global_cached_task(&self) -> bool {
        match self.kind {
            RenderTaskKind::LineDecoration(..) => {
                true
            }

            RenderTaskKind::Readback(..) |
            RenderTaskKind::ClipRegion(..) |
            RenderTaskKind::CacheMask(..) |
            RenderTaskKind::VerticalBlur(..) |
            RenderTaskKind::HorizontalBlur(..) |
            RenderTaskKind::Glyph(..) |
            RenderTaskKind::Scaling(..) |
            RenderTaskKind::Border(..) |
            RenderTaskKind::Picture(..) |
            RenderTaskKind::Blit(..) => {
                false
            }
        }
    }

    // Optionally, prepare the render task for drawing. This is executed
    // after all resource cache items (textures and glyphs) have been
    // resolved and can be queried. It also allows certain render tasks
    // to defer calculating an exact size until now, if desired.
    pub fn prepare_for_render(&mut self) {
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
            RenderTaskKind::Readback(..) |
            RenderTaskKind::Scaling(..) |
            RenderTaskKind::Blit(..) |
            RenderTaskKind::ClipRegion(..) |
            RenderTaskKind::Border(..) |
            RenderTaskKind::CacheMask(..) |
            RenderTaskKind::LineDecoration(..) |
            RenderTaskKind::Glyph(..) => {
                return;
            }
        };

        if let Some(mut request) = gpu_cache.request(cache_handle) {
            let p0 = target_rect.origin.to_f32();
            let p1 = target_rect.bottom_right().to_f32();

            let image_source = ImageSource {
                p0,
                p1,
                texture_layer: target_index.0 as f32,
                user_data: [0.0; 3],
                uv_rect_kind,
            };
            image_source.write_gpu_blocks(&mut request);
        }
    }

    #[cfg(feature = "debugger")]
    pub fn print_with<T: PrintTreePrinter>(&self, pt: &mut T, tree: &RenderTaskTree) -> bool {
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
            RenderTaskKind::Glyph(..) => {
                pt.new_level("Glyph".to_owned());
            }
        }

        pt.add_item(format!("clear to: {:?}", self.clear_mode));

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
            RenderTaskLocation::TextureCache { .. } => {
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
    #[allow(dead_code)]
    Glyph(GpuGlyphCacheKey),
    Picture(SurfaceCacheKey),
    BorderEdge(BorderEdgeCacheKey),
    BorderCorner(BorderCornerCacheKey),
    LineDecoration(LineDecorationCacheKey),
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
    pending_render_task_id: Option<RenderTaskId>,
    user_data: Option<[f32; 3]>,
    is_opaque: bool,
    pub handle: TextureCacheHandle,
}

#[derive(Debug)]
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

    pub fn update(
        &mut self,
        gpu_cache: &mut GpuCache,
        texture_cache: &mut TextureCache,
        render_tasks: &mut RenderTaskTree,
    ) {
        // Iterate the list of render task cache entries,
        // and allocate / update the texture cache location
        // if the entry has been evicted or not yet allocated.
        for (_, handle) in &self.map {
            let entry = self.cache_entries.get_mut(handle);

            if let Some(pending_render_task_id) = entry.pending_render_task_id.take() {
                let render_task = &mut render_tasks[pending_render_task_id];
                let target_kind = render_task.target_kind();

                // Find out what size to alloc in the texture cache.
                let size = match render_task.location {
                    RenderTaskLocation::Fixed(..) |
                    RenderTaskLocation::TextureCache { .. } => {
                        panic!("BUG: dynamic task was expected");
                    }
                    RenderTaskLocation::Dynamic(_, size) => size,
                };

                // Select the right texture page to allocate from.
                let image_format = match target_kind {
                    RenderTargetKind::Color => ImageFormat::BGRA8,
                    RenderTargetKind::Alpha => ImageFormat::R8,
                };

                let descriptor = ImageDescriptor::new(
                    size.width as u32,
                    size.height as u32,
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
                    None,
                    gpu_cache,
                    None,
                    render_task.uv_rect_kind(),
                    Eviction::Eager,
                );

                // Get the allocation details in the texture cache, and store
                // this in the render task. The renderer will draw this
                // task into the appropriate layer and rect of the texture
                // cache on this frame.
                let (texture_id, texture_layer, uv_rect) =
                    texture_cache.get_cache_location(&entry.handle);

                render_task.location = RenderTaskLocation::TextureCache {
                    texture: texture_id,
                    layer: texture_layer,
                    rect: uv_rect.to_i32(),
                };
            }
        }
    }

    pub fn request_render_task<F>(
        &mut self,
        key: RenderTaskCacheKey,
        texture_cache: &mut TextureCache,
        gpu_cache: &mut GpuCache,
        render_tasks: &mut RenderTaskTree,
        user_data: Option<[f32; 3]>,
        is_opaque: bool,
        f: F,
    ) -> Result<RenderTaskCacheEntryHandle, ()>
         where F: FnOnce(&mut RenderTaskTree) -> Result<RenderTaskId, ()> {
        // Get the texture cache handle for this cache key,
        // or create one.
        let cache_entries = &mut self.cache_entries;
        let entry_handle = self.map
                               .entry(key)
                               .or_insert_with(|| {
                                    let entry = RenderTaskCacheEntry {
                                        handle: TextureCacheHandle::invalid(),
                                        pending_render_task_id: None,
                                        user_data,
                                        is_opaque,
                                    };
                                    cache_entries.insert(entry)
                                });
        let cache_entry = cache_entries.get_mut(entry_handle);

        if cache_entry.pending_render_task_id.is_none() {
            // Check if this texture cache handle is valid.
            if texture_cache.request(&cache_entry.handle, gpu_cache) {
                // Invoke user closure to get render task chain
                // to draw this into the texture cache.
                let render_task_id = try!(f(render_tasks));

                cache_entry.pending_render_task_id = Some(render_task_id);
                cache_entry.user_data = user_data;
                cache_entry.is_opaque = is_opaque;
            }
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
    pub fn cache_item_is_allocated_for_render_task(&self,
                                                   texture_cache: &TextureCache,
                                                   key: &RenderTaskCacheKey)
                                                   -> bool {
        let handle = self.map.get(key).unwrap();
        let cache_entry = self.cache_entries.get(handle);
        texture_cache.is_allocated(&cache_entry.handle)
    }
}

// TODO(gw): Rounding the content rect here to device pixels is not
// technically correct. Ideally we should ceil() here, and ensure that
// the extra part pixel in the case of fractional sizes is correctly
// handled. For now, just use rounding which passes the existing
// Gecko tests.
// Note: zero-square tasks are prohibited in WR task tree, so
// we ensure each dimension to be at least the length of 1 after rounding.
pub fn to_cache_size(size: DeviceSize) -> DeviceIntSize {
    DeviceIntSize::new(
        1.max(size.width.round() as i32),
        1.max(size.height.round() as i32),
    )
}
