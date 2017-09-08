/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use gpu_cache::GpuCacheHandle;
use internal_types::HardwareCompositeOp;
use mask_cache::MaskCacheInfo;
use prim_store::{BoxShadowPrimitiveCacheKey, PrimitiveIndex};
use std::{cmp, f32, i32, usize};
use tiling::{ClipScrollGroupIndex, PackedLayerIndex, RenderPass, RenderTargetIndex};
use tiling::{RenderTargetKind, StackingContextIndex};
use api::{ClipId, DeviceIntLength, DeviceIntPoint, DeviceIntRect, DeviceIntSize};
use api::{FilterOp, MixBlendMode};

const FLOATS_PER_RENDER_TASK_INFO: usize = 12;

#[derive(Copy, Clone, PartialEq, Eq, Hash, Debug)]
pub struct RenderTaskId(pub u32);       // TODO(gw): Make private when using GPU cache!

#[derive(Debug, Copy, Clone)]
pub struct RenderTaskAddress(pub u32);

#[derive(Debug)]
pub struct RenderTaskTree {
    pub tasks: Vec<RenderTask>,
    pub task_data: Vec<RenderTaskData>,
}

impl RenderTaskTree {
    pub fn new() -> RenderTaskTree {
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

    pub fn assign_to_passes(&self, id: RenderTaskId, pass_index: usize, passes: &mut Vec<RenderPass>) {
        let task = &self.tasks[id.0 as usize];

        for child in &task.children {
            self.assign_to_passes(*child,
                                  pass_index - 1,
                                  passes);
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
        pass.add_render_task(id);
    }

    pub fn get(&self, id: RenderTaskId) -> &RenderTask {
        &self.tasks[id.0 as usize]
    }

    pub fn get_mut(&mut self, id: RenderTaskId) -> &mut RenderTask {
        &mut self.tasks[id.0 as usize]
    }

    pub fn get_task_address(&self, id: RenderTaskId) -> RenderTaskAddress {
        let task = &self.tasks[id.0 as usize];
        match task.kind {
            RenderTaskKind::Alias(alias_id) => {
                RenderTaskAddress(alias_id.0)
            }
            _ => {
                RenderTaskAddress(id.0)
            }
        }
    }

    pub fn build(&mut self) {
        for task in &mut self.tasks {
            self.task_data.push(task.write_task_data());
        }
    }
}

#[derive(Debug, Copy, Clone, Eq, PartialEq, Hash)]
pub enum RenderTaskKey {
    /// Draw this box shadow to a cache target.
    BoxShadow(BoxShadowPrimitiveCacheKey),
    /// Draw the alpha mask for a shared clip.
    CacheMask(ClipId),
}

#[derive(Debug)]
pub enum RenderTaskLocation {
    Fixed,
    Dynamic(Option<(DeviceIntPoint, RenderTargetIndex)>, DeviceIntSize),
}

#[derive(Debug)]
pub enum AlphaRenderItem {
    Primitive(Option<ClipScrollGroupIndex>, PrimitiveIndex, i32),
    Blend(StackingContextIndex, RenderTaskId, FilterOp, i32),
    Composite(StackingContextIndex, RenderTaskId, RenderTaskId, MixBlendMode, i32),
    SplitComposite(StackingContextIndex, RenderTaskId, GpuCacheHandle, i32),
    HardwareComposite(StackingContextIndex, RenderTaskId, HardwareCompositeOp, i32),
}

#[derive(Debug)]
pub struct AlphaRenderTask {
    pub screen_origin: DeviceIntPoint,
    pub items: Vec<AlphaRenderItem>,
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
    Default,        // Draw the entire rect
    CornersOnly,    // Draw the corners (simple axis aligned mask)
    // TODO(gw): Add more types here (e.g. 4 rectangles outside the inner rect)
}

pub type ClipWorkItem = (PackedLayerIndex, MaskCacheInfo);

#[derive(Debug)]
pub struct CacheMaskTask {
    actual_rect: DeviceIntRect,
    inner_rect: DeviceIntRect,
    pub clips: Vec<ClipWorkItem>,
    pub geometry_kind: MaskGeometryKind,
}

#[derive(Debug)]
pub struct RenderTaskData {
    pub data: [f32; FLOATS_PER_RENDER_TASK_INFO],
}

#[derive(Debug)]
pub enum RenderTaskKind {
    Alpha(AlphaRenderTask),
    CachePrimitive(PrimitiveIndex),
    BoxShadow(PrimitiveIndex),
    CacheMask(CacheMaskTask),
    VerticalBlur(DeviceIntLength),
    HorizontalBlur(DeviceIntLength),
    Readback(DeviceIntRect),
    Alias(RenderTaskId),
}

#[derive(Debug)]
pub struct RenderTask {
    pub cache_key: Option<RenderTaskKey>,
    pub location: RenderTaskLocation,
    pub children: Vec<RenderTaskId>,
    pub kind: RenderTaskKind,
}

impl RenderTask {
    pub fn new_alpha_batch(screen_origin: DeviceIntPoint,
                           location: RenderTaskLocation) -> RenderTask {
        RenderTask {
            cache_key: None,
            children: Vec::new(),
            location,
            kind: RenderTaskKind::Alpha(AlphaRenderTask {
                screen_origin,
                items: Vec::new(),
            }),
        }
    }

    pub fn new_dynamic_alpha_batch(rect: &DeviceIntRect) -> RenderTask {
        let location = RenderTaskLocation::Dynamic(None, rect.size);
        Self::new_alpha_batch(rect.origin, location)
    }

    pub fn new_prim_cache(size: DeviceIntSize,
                          prim_index: PrimitiveIndex) -> RenderTask {
        RenderTask {
            cache_key: None,
            children: Vec::new(),
            location: RenderTaskLocation::Dynamic(None, size),
            kind: RenderTaskKind::CachePrimitive(prim_index),
        }
    }

    pub fn new_box_shadow(key: BoxShadowPrimitiveCacheKey,
                          size: DeviceIntSize,
                          prim_index: PrimitiveIndex) -> RenderTask {
        RenderTask {
            cache_key: Some(RenderTaskKey::BoxShadow(key)),
            children: Vec::new(),
            location: RenderTaskLocation::Dynamic(None, size),
            kind: RenderTaskKind::BoxShadow(prim_index),
        }
    }

    pub fn new_readback(screen_rect: DeviceIntRect) -> RenderTask {
        RenderTask {
            cache_key: None,
            children: Vec::new(),
            location: RenderTaskLocation::Dynamic(None, screen_rect.size),
            kind: RenderTaskKind::Readback(screen_rect),
        }
    }

    pub fn new_mask(key: Option<ClipId>,
                    task_rect: DeviceIntRect,
                    raw_clips: &[ClipWorkItem],
                    extra_clip: Option<ClipWorkItem>,
                    prim_rect: DeviceIntRect)
                    -> Option<RenderTask> {
        // Filter out all the clip instances that don't contribute to the result
        let mut inner_rect = Some(task_rect);
        let clips: Vec<_> = raw_clips.iter()
                                     .chain(extra_clip.iter())
                                     .filter(|&&(_, ref clip_info)| {
            // If this clip does not contribute to a mask, then ensure
            // it gets filtered out here. Otherwise, if a mask is
            // created (by a different clip in the list), the allocated
            // rectangle for the mask could end up being much bigger
            // than is actually required.
            if !clip_info.is_masking() {
                return false;
            }

            match clip_info.bounds.inner {
                Some(ref inner) if !inner.device_rect.is_empty() => {
                    inner_rect = inner_rect.and_then(|r| r.intersection(&inner.device_rect));
                    !inner.device_rect.contains_rect(&task_rect)
                }
                _ => {
                    inner_rect = None;
                    true
                }
            }
        }).cloned().collect();

        // Nothing to do, all clips are irrelevant for this case
        if clips.is_empty() {
            return None
        }

        // TODO(gw): This optimization is very conservative for now.
        //           For now, only draw optimized geometry if it is
        //           a single aligned rect mask with rounded corners.
        //           In the future, we'll expand this to handle the
        //           more complex types of clip mask geometry.
        let mut geometry_kind = MaskGeometryKind::Default;
        if let Some(inner_rect) = inner_rect {
            // If the inner rect completely contains the primitive
            // rect, then this mask can't affect the primitive.
            if inner_rect.contains_rect(&prim_rect) {
                return None;
            }
            if clips.len() == 1 {
                let (_, ref info) = clips[0];
                if info.border_corners.is_empty() &&
                   info.image.is_none() &&
                   info.complex_clip_range.get_count() == 1 &&
                   info.layer_clip_range.get_count() == 0 {
                    geometry_kind = MaskGeometryKind::CornersOnly;
                }
            }
        }

        Some(RenderTask {
            cache_key: key.map(RenderTaskKey::CacheMask),
            children: Vec::new(),
            location: RenderTaskLocation::Dynamic(None, task_rect.size),
            kind: RenderTaskKind::CacheMask(CacheMaskTask {
                actual_rect: task_rect,
                inner_rect: inner_rect.unwrap_or(DeviceIntRect::zero()),
                clips,
                geometry_kind,
            }),
        })
    }

    // Construct a render task to apply a blur to a primitive. For now,
    // this is only used for text runs, but we can probably extend this
    // to handle general blurs to any render task in the future.
    // The render task chain that is constructed looks like:
    //
    //    PrimitiveCacheTask: Draw the text run.
    //           ^
    //           |
    //    VerticalBlurTask: Apply the separable vertical blur to the primitive.
    //           ^
    //           |
    //    HorizontalBlurTask: Apply the separable horizontal blur to the vertical blur.
    //           |
    //           +---- This is stored as the input task to the primitive shader.
    //
    pub fn new_blur(size: DeviceIntSize,
                    blur_radius: DeviceIntLength,
                    prim_index: PrimitiveIndex,
                    render_tasks: &mut RenderTaskTree) -> RenderTask {
        let prim_cache_task = RenderTask::new_prim_cache(size,
                                                         prim_index);
        let prim_cache_task_id = render_tasks.add(prim_cache_task);

        let blur_target_size = size + DeviceIntSize::new(2 * blur_radius.0,
                                                         2 * blur_radius.0);

        let blur_task_v = RenderTask {
            cache_key: None,
            children: vec![prim_cache_task_id],
            location: RenderTaskLocation::Dynamic(None, blur_target_size),
            kind: RenderTaskKind::VerticalBlur(blur_radius),
        };

        let blur_task_v_id = render_tasks.add(blur_task_v);

        let blur_task_h = RenderTask {
            cache_key: None,
            children: vec![blur_task_v_id],
            location: RenderTaskLocation::Dynamic(None, blur_target_size),
            kind: RenderTaskKind::HorizontalBlur(blur_radius),
        };

        blur_task_h
    }

    pub fn as_alpha_batch_mut<'a>(&'a mut self) -> &'a mut AlphaRenderTask {
        match self.kind {
            RenderTaskKind::Alpha(ref mut task) => task,
            RenderTaskKind::CachePrimitive(..) |
            RenderTaskKind::BoxShadow(..) |
            RenderTaskKind::CacheMask(..) |
            RenderTaskKind::VerticalBlur(..) |
            RenderTaskKind::Readback(..) |
            RenderTaskKind::HorizontalBlur(..) |
            RenderTaskKind::Alias(..) => unreachable!(),
        }
    }

    pub fn as_alpha_batch<'a>(&'a self) -> &'a AlphaRenderTask {
        match self.kind {
            RenderTaskKind::Alpha(ref task) => task,
            RenderTaskKind::CachePrimitive(..) |
            RenderTaskKind::BoxShadow(..) |
            RenderTaskKind::CacheMask(..) |
            RenderTaskKind::VerticalBlur(..) |
            RenderTaskKind::Readback(..) |
            RenderTaskKind::HorizontalBlur(..) |
            RenderTaskKind::Alias(..) => unreachable!(),
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

        match self.kind {
            RenderTaskKind::Alpha(ref task) => {
                let (target_rect, target_index) = self.get_target_rect();
                RenderTaskData {
                    data: [
                        target_rect.origin.x as f32,
                        target_rect.origin.y as f32,
                        target_rect.size.width as f32,
                        target_rect.size.height as f32,
                        task.screen_origin.x as f32,
                        task.screen_origin.y as f32,
                        target_index.0 as f32,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                    ],
                }
            }
            RenderTaskKind::CachePrimitive(..) |
            RenderTaskKind::BoxShadow(..) => {
                let (target_rect, target_index) = self.get_target_rect();
                RenderTaskData {
                    data: [
                        target_rect.origin.x as f32,
                        target_rect.origin.y as f32,
                        target_rect.size.width as f32,
                        target_rect.size.height as f32,
                        target_index.0 as f32,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                    ],
                }
            }
            RenderTaskKind::CacheMask(ref task) => {
                let (target_rect, target_index) = self.get_target_rect();
                RenderTaskData {
                    data: [
                        target_rect.origin.x as f32,
                        target_rect.origin.y as f32,
                        (target_rect.origin.x + target_rect.size.width) as f32,
                        (target_rect.origin.y + target_rect.size.height) as f32,
                        task.actual_rect.origin.x as f32,
                        task.actual_rect.origin.y as f32,
                        target_index.0 as f32,
                        0.0,
                        task.inner_rect.origin.x as f32,
                        task.inner_rect.origin.y as f32,
                        (task.inner_rect.origin.x + task.inner_rect.size.width) as f32,
                        (task.inner_rect.origin.y + task.inner_rect.size.height) as f32,
                    ],
                }
            }
            RenderTaskKind::VerticalBlur(blur_radius) |
            RenderTaskKind::HorizontalBlur(blur_radius) => {
                let (target_rect, target_index) = self.get_target_rect();
                RenderTaskData {
                    data: [
                        target_rect.origin.x as f32,
                        target_rect.origin.y as f32,
                        target_rect.size.width as f32,
                        target_rect.size.height as f32,
                        target_index.0 as f32,
                        blur_radius.0 as f32,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                    ]
                }
            }
            RenderTaskKind::Readback(..) => {
                let (target_rect, target_index) = self.get_target_rect();
                RenderTaskData {
                    data: [
                        target_rect.origin.x as f32,
                        target_rect.origin.y as f32,
                        target_rect.size.width as f32,
                        target_rect.size.height as f32,
                        target_index.0 as f32,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                    ]
                }
            }
            RenderTaskKind::Alias(..) => {
                RenderTaskData {
                    data: [0.0; 12],
                }
            }
        }
    }

    pub fn get_target_rect(&self) -> (DeviceIntRect, RenderTargetIndex) {
        match self.location {
            RenderTaskLocation::Fixed => {
                (DeviceIntRect::zero(), RenderTargetIndex(0))
            },
            RenderTaskLocation::Dynamic(origin_and_target_index, size) => {
                let (origin, target_index) = origin_and_target_index.expect("Should have been allocated by now!");
                (DeviceIntRect::new(origin, size), target_index)
            }
        }
    }

    pub fn target_kind(&self) -> RenderTargetKind {
        match self.kind {
            RenderTaskKind::Alpha(..) |
            RenderTaskKind::CachePrimitive(..) |
            RenderTaskKind::VerticalBlur(..) |
            RenderTaskKind::Readback(..) |
            RenderTaskKind::HorizontalBlur(..) => RenderTargetKind::Color,

            RenderTaskKind::CacheMask(..) |
            RenderTaskKind::BoxShadow(..) => RenderTargetKind::Alpha,

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
            RenderTaskKind::Alpha(..) |
            RenderTaskKind::CachePrimitive(..) |
            RenderTaskKind::VerticalBlur(..) |
            RenderTaskKind::Readback(..) |
            RenderTaskKind::HorizontalBlur(..) => false,

            RenderTaskKind::CacheMask(..) |
            RenderTaskKind::BoxShadow(..) => true,

            RenderTaskKind::Alias(..) => {
                panic!("BUG: is_shared() called on aliased task");
            }
        }
    }

    pub fn set_alias(&mut self, id: RenderTaskId) {
        debug_assert!(self.cache_key.is_some());
        // TODO(gw): We can easily handle invalidation of tasks that
        //           contain children in the future. Since we don't
        //           have any cases of that yet, just assert to simplify
        //           the current implementation.
        debug_assert!(self.children.is_empty());
        self.kind = RenderTaskKind::Alias(id);
    }
}
