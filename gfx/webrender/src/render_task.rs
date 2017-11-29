/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{ClipId, DeviceIntPoint, DeviceIntRect, DeviceIntSize};
use api::{LayerPoint, LayerRect, PremultipliedColorF};
use clip::{ClipSource, ClipSourcesWeakHandle, ClipStore};
use clip_scroll_tree::CoordinateSystemId;
use gpu_types::{ClipScrollNodeIndex};
use picture::RasterizationSpace;
use prim_store::{PrimitiveIndex};
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
}

#[derive(Debug)]
pub enum RenderTaskLocation {
    Fixed,
    Dynamic(Option<(DeviceIntPoint, RenderTargetIndex)>, DeviceIntSize),
}

#[derive(Debug, Copy, Clone)]
#[repr(C)]
pub enum MaskSegment {
    // This must match the SEGMENT_ values in clip_shared.glsl!
    All = 0,
    TopLeftCorner,
    TopRightCorner,
    BottomLeftCorner,
    BottomRightCorner,
}

#[derive(Debug, Copy, Clone)]
#[repr(C)]
pub enum MaskGeometryKind {
    Default, // Draw the entire rect
    CornersOnly, // Draw the corners (simple axis aligned mask)
             // TODO(gw): Add more types here (e.g. 4 rectangles outside the inner rect)
}

#[derive(Debug, Clone)]
pub struct ClipWorkItem {
    pub scroll_node_data_index: ClipScrollNodeIndex,
    pub clip_sources: ClipSourcesWeakHandle,
    pub coordinate_system_id: CoordinateSystemId,
}

impl ClipWorkItem {
    fn get_geometry_kind(
        &self,
        clip_store: &ClipStore,
        prim_coordinate_system_id: CoordinateSystemId
    ) -> MaskGeometryKind {
        let clips = clip_store
            .get_opt(&self.clip_sources)
            .expect("bug: clip handle should be valid")
            .clips();
        let mut rounded_rect_count = 0;

        for &(ref clip, _) in clips {
            match *clip {
                ClipSource::Rectangle(..) => {
                    if !self.has_compatible_coordinate_system(prim_coordinate_system_id) {
                        return MaskGeometryKind::Default;
                    }
                },
                ClipSource::RoundedRectangle(..) => {
                    rounded_rect_count += 1;
                }
                ClipSource::Image(..) | ClipSource::BorderCorner(..) => {
                    return MaskGeometryKind::Default;
                }
            }
        }

        if rounded_rect_count == 1 {
            MaskGeometryKind::CornersOnly
        } else {
            MaskGeometryKind::Default
        }
    }

    fn has_compatible_coordinate_system(&self, other_id: CoordinateSystemId) -> bool {
        self.coordinate_system_id == other_id
    }
}

#[derive(Debug)]
pub struct CacheMaskTask {
    actual_rect: DeviceIntRect,
    inner_rect: DeviceIntRect,
    pub clips: Vec<ClipWorkItem>,
    pub geometry_kind: MaskGeometryKind,
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

#[derive(Debug, Copy, Clone)]
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
    ) -> Self {
        let location = match size {
            Some(size) => RenderTaskLocation::Dynamic(None, size),
            None => RenderTaskLocation::Fixed,
        };

        RenderTask {
            cache_key: None,
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
        }
    }

    pub fn new_readback(screen_rect: DeviceIntRect) -> Self {
        RenderTask {
            cache_key: None,
            children: Vec::new(),
            location: RenderTaskLocation::Dynamic(None, screen_rect.size),
            kind: RenderTaskKind::Readback(screen_rect),
            clear_mode: ClearMode::Transparent,
        }
    }

    pub fn new_mask(
        key: Option<ClipId>,
        outer_rect: DeviceIntRect,
        inner_rect: DeviceIntRect,
        clips: Vec<ClipWorkItem>,
        clip_store: &ClipStore,
        is_axis_aligned: bool,
        prim_coordinate_system_id: CoordinateSystemId,
    ) -> Option<RenderTask> {
        // TODO(gw): This optimization is very conservative for now.
        //           For now, only draw optimized geometry if it is
        //           a single aligned rect mask with rounded corners.
        //           In the future, we'll expand this to handle the
        //           more complex types of clip mask geometry.
        let geometry_kind = if is_axis_aligned &&
            clips.len() == 1 &&
            inner_rect.size != DeviceIntSize::zero() {
            clips[0].get_geometry_kind(clip_store, prim_coordinate_system_id)
        } else {
            MaskGeometryKind::Default
        };

        Some(RenderTask {
            cache_key: key.map(RenderTaskKey::CacheMask),
            children: Vec::new(),
            location: RenderTaskLocation::Dynamic(None, outer_rect.size),
            kind: RenderTaskKind::CacheMask(CacheMaskTask {
                actual_rect: outer_rect,
                inner_rect: inner_rect,
                clips,
                geometry_kind,
                coordinate_system_id: prim_coordinate_system_id,
            }),
            clear_mode: ClearMode::One,
        })
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
                adjusted_blur_target_size
            );
            downscaling_src_task_id = render_tasks.add(downscaling_task);
        }
        scale_factor = blur_target_size.width as f32 / adjusted_blur_target_size.width as f32;

        let blur_task_v = RenderTask {
            cache_key: None,
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
        };

        let blur_task_v_id = render_tasks.add(blur_task_v);

        let blur_task_h = RenderTask {
            cache_key: None,
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
        };

        blur_task_h
    }

    pub fn new_scaling(
        target_kind: RenderTargetKind,
        src_task_id: RenderTaskId,
        target_size: DeviceIntSize,
    ) -> Self {
        RenderTask {
            cache_key: None,
            children: vec![src_task_id],
            location: RenderTaskLocation::Dynamic(None, target_size),
            kind: RenderTaskKind::Scaling(target_kind),
            clear_mode: match target_kind {
                RenderTargetKind::Color => ClearMode::Transparent,
                RenderTargetKind::Alpha => ClearMode::One,
            },
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
                    [
                        task.inner_rect.origin.x as f32,
                        task.inner_rect.origin.y as f32,
                        (task.inner_rect.origin.x + task.inner_rect.size.width) as f32,
                        (task.inner_rect.origin.y + task.inner_rect.size.height) as f32,
                    ],
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
}
