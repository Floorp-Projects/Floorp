/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{ClipId, DeviceIntPoint, DeviceIntRect, DeviceIntSize, DevicePixel};
use api::{LayerPoint, LayerRect, PremultipliedColorF};
use box_shadow::BoxShadowCacheKey;
use clip::{ClipSourcesWeakHandle};
use clip_scroll_tree::CoordinateSystemId;
use euclid::TypedSize2D;
use gpu_types::{ClipScrollNodeIndex};
use internal_types::RenderPassIndex;
use picture::RasterizationSpace;
use prim_store::{PrimitiveIndex};
#[cfg(feature = "debugger")]
use print_tree::{PrintTreePrinter};
use std::{cmp, ops, usize, f32, i32};
use std::rc::Rc;
use tiling::{RenderPass, RenderTargetIndex};
use tiling::{RenderTargetKind};

const FLOATS_PER_RENDER_TASK_INFO: usize = 12;
pub const MAX_BLUR_STD_DEVIATION: f32 = 4.0;
pub const MIN_DOWNSCALING_RT_SIZE: i32 = 128;

#[derive(Copy, Clone, PartialEq, Eq, Hash, Debug)]
pub struct RenderTaskId(pub u32); // TODO(gw): Make private when using GPU cache!

#[derive(Debug, Copy, Clone)]
#[repr(C)]
pub struct RenderTaskAddress(pub u32);

#[derive(Debug)]
pub struct RenderTaskTree {
    pub tasks: Vec<RenderTask>,
    pub task_data: Vec<RenderTaskData>,
}

pub type ClipChain = Option<Rc<ClipChainNode>>;

#[derive(Debug)]
pub struct ClipChainNode {
    pub work_item: ClipWorkItem,
    pub screen_inner_rect: DeviceIntRect,
    pub combined_outer_screen_rect: DeviceIntRect,
    pub combined_inner_screen_rect: DeviceIntRect,
    pub prev: ClipChain,
}

pub struct ClipChainNodeIter {
    pub current: ClipChain,
}

impl Iterator for ClipChainNodeIter {
    type Item = Rc<ClipChainNode>;

    fn next(&mut self) -> ClipChain {
        let previous = self.current.clone();
        self.current = match self.current {
            Some(ref item) => item.prev.clone(),
            None => return None,
        };
        previous
    }
}

impl RenderTaskTree {
    pub fn new() -> Self {
        RenderTaskTree {
            tasks: Vec::new(),
            task_data: Vec::new(),
        }
    }

    pub fn add(&mut self, task: RenderTask) -> RenderTaskId {
        let id = RenderTaskId(self.tasks.len() as u32);
        self.tasks.push(task);
        id
    }

    pub fn max_depth(&self, id: RenderTaskId, depth: usize, max_depth: &mut usize) {
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
        passes: &mut Vec<RenderPass>,
    ) {
        let task = &self.tasks[id.0 as usize];

        for child in &task.children {
            self.assign_to_passes(*child, pass_index - 1, passes);
        }

        // Sanity check - can be relaxed if needed
        match task.location {
            RenderTaskLocation::Fixed => {
                debug_assert!(pass_index == passes.len() - 1);
            }
            RenderTaskLocation::Dynamic(..) => {
                debug_assert!(pass_index < passes.len() - 1);
            }
        }

        // If this task can be shared between multiple
        // passes, render it in the first pass so that
        // it is available to all subsequent passes.
        let pass_index = if task.is_shared() {
            debug_assert!(task.children.is_empty());
            0
        } else {
            pass_index
        };

        let pass = &mut passes[pass_index];
        pass.add_render_task(id, task.get_dynamic_size(), task.target_kind());
    }

    pub fn get_task_address(&self, id: RenderTaskId) -> RenderTaskAddress {
        match self[id].kind {
            RenderTaskKind::Alias(alias_id) => RenderTaskAddress(alias_id.0),
            _ => RenderTaskAddress(id.0),
        }
    }

    pub fn build(&mut self) {
        for task in &mut self.tasks {
            self.task_data.push(task.write_task_data());
        }
    }
}

impl ops::Index<RenderTaskId> for RenderTaskTree {
    type Output = RenderTask;
    fn index(&self, id: RenderTaskId) -> &RenderTask {
        &self.tasks[id.0 as usize]
    }
}

impl ops::IndexMut<RenderTaskId> for RenderTaskTree {
    fn index_mut(&mut self, id: RenderTaskId) -> &mut RenderTask {
        &mut self.tasks[id.0 as usize]
    }
}

#[derive(Debug, Copy, Clone, Eq, PartialEq, Hash)]
pub enum RenderTaskKey {
    /// Draw the alpha mask for a shared clip.
    CacheMask(ClipId),
    CacheScaling(BoxShadowCacheKey, TypedSize2D<i32, DevicePixel>),
    CacheBlur(BoxShadowCacheKey, i32),
    CachePicture(BoxShadowCacheKey),
}

#[derive(Debug)]
pub enum RenderTaskLocation {
    Fixed,
    Dynamic(Option<(DeviceIntPoint, RenderTargetIndex)>, DeviceIntSize),
}

#[derive(Debug, Clone)]
pub struct ClipWorkItem {
    pub scroll_node_data_index: ClipScrollNodeIndex,
    pub clip_sources: ClipSourcesWeakHandle,
    pub coordinate_system_id: CoordinateSystemId,
}

#[derive(Debug)]
pub struct CacheMaskTask {
    actual_rect: DeviceIntRect,
    pub clips: Vec<ClipWorkItem>,
    pub coordinate_system_id: CoordinateSystemId,
}

#[derive(Debug)]
pub struct PictureTask {
    pub prim_index: PrimitiveIndex,
    pub target_kind: RenderTargetKind,
    pub content_origin: LayerPoint,
    pub color: PremultipliedColorF,
    pub rasterization_kind: RasterizationSpace,
}

#[derive(Debug)]
pub struct BlurTask {
    pub blur_std_deviation: f32,
    pub target_kind: RenderTargetKind,
    pub regions: Vec<LayerRect>,
    pub color: PremultipliedColorF,
    pub scale_factor: f32,
}

impl BlurTask {
    #[cfg(feature = "debugger")]
    fn print_with<T: PrintTreePrinter>(&self, pt: &mut T) {
        pt.add_item(format!("std deviation: {}", self.blur_std_deviation));
        pt.add_item(format!("target: {:?}", self.target_kind));
        pt.add_item(format!("scale: {}", self.scale_factor));
        for region in &self.regions {
            pt.add_item(format!("region {:?}", region));
        }
    }
}

#[derive(Debug)]
pub struct RenderTaskData {
    pub data: [f32; FLOATS_PER_RENDER_TASK_INFO],
}

#[derive(Debug)]
pub enum RenderTaskKind {
    Picture(PictureTask),
    CacheMask(CacheMaskTask),
    VerticalBlur(BlurTask),
    HorizontalBlur(BlurTask),
    Readback(DeviceIntRect),
    Alias(RenderTaskId),
    Scaling(RenderTargetKind),
}

#[derive(Debug, Copy, Clone, Eq, PartialEq, Hash)]
pub enum ClearMode {
    // Applicable to color and alpha targets.
    Zero,
    One,

    // Applicable to color targets only.
    Transparent,
}

#[derive(Debug)]
pub struct RenderTask {
    pub cache_key: Option<RenderTaskKey>,
    pub location: RenderTaskLocation,
    pub children: Vec<RenderTaskId>,
    pub kind: RenderTaskKind,
    pub clear_mode: ClearMode,
    pub pass_index: Option<RenderPassIndex>,
}

impl RenderTask {
    pub fn new_picture(
        size: Option<DeviceIntSize>,
        prim_index: PrimitiveIndex,
        target_kind: RenderTargetKind,
        content_origin_x: f32,
        content_origin_y: f32,
        color: PremultipliedColorF,
        clear_mode: ClearMode,
        rasterization_kind: RasterizationSpace,
        children: Vec<RenderTaskId>,
        box_shadow_cache_key: Option<BoxShadowCacheKey>,
    ) -> Self {
        let location = match size {
            Some(size) => RenderTaskLocation::Dynamic(None, size),
            None => RenderTaskLocation::Fixed,
        };

        RenderTask {
            cache_key: match box_shadow_cache_key {
                Some(key) => Some(RenderTaskKey::CachePicture(key)),
                None => None,
            },
            children,
            location,
            kind: RenderTaskKind::Picture(PictureTask {
                prim_index,
                target_kind,
                content_origin: LayerPoint::new(content_origin_x, content_origin_y),
                color,
                rasterization_kind,
            }),
            clear_mode,
            pass_index: None,
        }
    }

    pub fn new_readback(screen_rect: DeviceIntRect) -> Self {
        RenderTask {
            cache_key: None,
            children: Vec::new(),
            location: RenderTaskLocation::Dynamic(None, screen_rect.size),
            kind: RenderTaskKind::Readback(screen_rect),
            clear_mode: ClearMode::Transparent,
            pass_index: None,
        }
    }

    pub fn new_mask(
        key: Option<ClipId>,
        outer_rect: DeviceIntRect,
        clips: Vec<ClipWorkItem>,
        prim_coordinate_system_id: CoordinateSystemId,
    ) -> RenderTask {
        RenderTask {
            cache_key: key.map(RenderTaskKey::CacheMask),
            children: Vec::new(),
            location: RenderTaskLocation::Dynamic(None, outer_rect.size),
            kind: RenderTaskKind::CacheMask(CacheMaskTask {
                actual_rect: outer_rect,
                clips,
                coordinate_system_id: prim_coordinate_system_id,
            }),
            clear_mode: ClearMode::One,
            pass_index: None,
        }
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
        regions: &[LayerRect],
        clear_mode: ClearMode,
        color: PremultipliedColorF,
        box_shadow_cache_key: Option<BoxShadowCacheKey>,
    ) -> Self {
        // Adjust large std deviation value.
        let mut adjusted_blur_std_deviation = blur_std_deviation;
        let blur_target_size = render_tasks[src_task_id].get_dynamic_size();
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
                target_kind,
                downscaling_src_task_id,
                adjusted_blur_target_size,
                box_shadow_cache_key,
            );
            downscaling_src_task_id = render_tasks.add(downscaling_task);
        }
        scale_factor = blur_target_size.width as f32 / adjusted_blur_target_size.width as f32;

        let blur_task_v = RenderTask {
            cache_key: match box_shadow_cache_key {
                Some(key) => Some(RenderTaskKey::CacheBlur(key, 0)),
                None => None,
            },
            children: vec![downscaling_src_task_id],
            location: RenderTaskLocation::Dynamic(None, adjusted_blur_target_size),
            kind: RenderTaskKind::VerticalBlur(BlurTask {
                blur_std_deviation: adjusted_blur_std_deviation,
                target_kind,
                regions: regions.to_vec(),
                color,
                scale_factor,
            }),
            clear_mode,
            pass_index: None,
        };

        let blur_task_v_id = render_tasks.add(blur_task_v);

        let blur_task_h = RenderTask {
            cache_key: match box_shadow_cache_key {
                Some(key) => Some(RenderTaskKey::CacheBlur(key, 1)),
                None => None,
            },
            children: vec![blur_task_v_id],
            location: RenderTaskLocation::Dynamic(None, adjusted_blur_target_size),
            kind: RenderTaskKind::HorizontalBlur(BlurTask {
                blur_std_deviation: adjusted_blur_std_deviation,
                target_kind,
                regions: regions.to_vec(),
                color,
                scale_factor,
            }),
            clear_mode,
            pass_index: None,
        };

        blur_task_h
    }

    pub fn new_scaling(
        target_kind: RenderTargetKind,
        src_task_id: RenderTaskId,
        target_size: DeviceIntSize,
        box_shadow_cache_key: Option<BoxShadowCacheKey>,
    ) -> Self {
        RenderTask {
            cache_key: match box_shadow_cache_key {
                Some(key) => Some(RenderTaskKey::CacheScaling(key, target_size)),
                None => None,
            },
            children: vec![src_task_id],
            location: RenderTaskLocation::Dynamic(None, target_size),
            kind: RenderTaskKind::Scaling(target_kind),
            clear_mode: match target_kind {
                RenderTargetKind::Color => ClearMode::Transparent,
                RenderTargetKind::Alpha => ClearMode::One,
            },
            pass_index: None,
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

        let (data1, data2) = match self.kind {
            RenderTaskKind::Picture(ref task) => {
                (
                    [
                        task.content_origin.x,
                        task.content_origin.y,
                        task.rasterization_kind as u32 as f32,
                    ],
                    task.color.to_array()
                )
            }
            RenderTaskKind::CacheMask(ref task) => {
                (
                    [
                        task.actual_rect.origin.x as f32,
                        task.actual_rect.origin.y as f32,
                        0.0,
                    ],
                    [0.0; 4],
                )
            }
            RenderTaskKind::VerticalBlur(ref task) |
            RenderTaskKind::HorizontalBlur(ref task) => {
                (
                    [
                        task.blur_std_deviation,
                        task.scale_factor,
                        0.0,
                    ],
                    task.color.to_array()
                )
            }
            RenderTaskKind::Readback(..) |
            RenderTaskKind::Scaling(..) |
            RenderTaskKind::Alias(..) => {
                (
                    [0.0; 3],
                    [0.0; 4],
                )
            }
        };

        let (target_rect, target_index) = self.get_target_rect();

        RenderTaskData {
            data: [
                target_rect.origin.x as f32,
                target_rect.origin.y as f32,
                target_rect.size.width as f32,
                target_rect.size.height as f32,
                target_index.0 as f32,
                data1[0],
                data1[1],
                data1[2],
                data2[0],
                data2[1],
                data2[2],
                data2[3],
            ]
        }
    }

    pub fn get_dynamic_size(&self) -> DeviceIntSize {
        match self.location {
            RenderTaskLocation::Fixed => DeviceIntSize::zero(),
            RenderTaskLocation::Dynamic(_, size) => size,
        }
    }

    pub fn get_target_rect(&self) -> (DeviceIntRect, RenderTargetIndex) {
        match self.location {
            RenderTaskLocation::Fixed => {
                (DeviceIntRect::zero(), RenderTargetIndex(0))
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
        }
    }

    pub fn target_kind(&self) -> RenderTargetKind {
        match self.kind {
            RenderTaskKind::Readback(..) => RenderTargetKind::Color,

            RenderTaskKind::CacheMask(..) => {
                RenderTargetKind::Alpha
            }

            RenderTaskKind::VerticalBlur(ref task_info) |
            RenderTaskKind::HorizontalBlur(ref task_info) => {
                task_info.target_kind
            }

            RenderTaskKind::Scaling(target_kind) => {
                target_kind
            }

            RenderTaskKind::Picture(ref task_info) => {
                task_info.target_kind
            }

            RenderTaskKind::Alias(..) => {
                panic!("BUG: target_kind() called on invalidated task");
            }
        }
    }

    // Check if this task wants to be made available as an input
    // to all passes (except the first) in the render task tree.
    // To qualify for this, the task needs to have no children / dependencies.
    // Currently, this is only supported for A8 targets, but it can be
    // trivially extended to also support RGBA8 targets in the future
    // if we decide that is useful.
    pub fn is_shared(&self) -> bool {
        match self.kind {
            RenderTaskKind::Picture(..) |
            RenderTaskKind::VerticalBlur(..) |
            RenderTaskKind::Readback(..) |
            RenderTaskKind::HorizontalBlur(..) |
            RenderTaskKind::Scaling(..) => false,

            RenderTaskKind::CacheMask(..) => true,

            RenderTaskKind::Alias(..) => {
                panic!("BUG: is_shared() called on aliased task");
            }
        }
    }

    #[cfg(feature = "debugger")]
    pub fn print_with<T: PrintTreePrinter>(&self, pt: &mut T, tree: &RenderTaskTree) -> bool {
        match self.kind {
            RenderTaskKind::Picture(ref task) => {
                pt.new_level(format!("Picture of {:?}", task.prim_index));
                pt.add_item(format!("kind: {:?}", task.target_kind));
                pt.add_item(format!("space: {:?}", task.rasterization_kind));
            }
            RenderTaskKind::CacheMask(ref task) => {
                pt.new_level(format!("CacheMask with {} clips", task.clips.len()));
                pt.add_item(format!("rect: {:?}", task.actual_rect));
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
            RenderTaskKind::Alias(ref alias_id) => {
                pt.add_item(format!("Alias of {:?}", alias_id));
                return false
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
}
