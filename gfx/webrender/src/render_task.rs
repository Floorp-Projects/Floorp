/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use internal_types::{HardwareCompositeOp, LowLevelFilterOp};
use mask_cache::{MaskBounds, MaskCacheInfo};
use prim_store::{PrimitiveCacheKey, PrimitiveIndex};
use std::{cmp, f32, i32, mem, usize};
use tiling::{ClipScrollGroupIndex, PackedLayerIndex, RenderPass, RenderTargetIndex};
use tiling::{RenderTargetKind, StackingContextIndex};
use webrender_traits::{ClipId, DeviceIntLength, DeviceIntPoint, DeviceIntRect, DeviceIntSize};
use webrender_traits::MixBlendMode;

const FLOATS_PER_RENDER_TASK_INFO: usize = 12;

#[derive(Debug, Copy, Clone, Eq, Hash, PartialEq)]
pub struct RenderTaskIndex(pub usize);

#[derive(Debug, Copy, Clone, Eq, PartialEq, Hash)]
pub enum RenderTaskKey {
    /// Draw this primitive to a cache target.
    CachePrimitive(PrimitiveCacheKey),
    /// Draw the alpha mask for a primitive.
    CacheMask(MaskCacheKey),
    /// Apply a vertical blur pass of given radius for this primitive.
    VerticalBlur(i32, PrimitiveIndex),
    /// Apply a horizontal blur pass of given radius for this primitive.
    HorizontalBlur(i32, PrimitiveIndex),
    /// Allocate a block of space in target for framebuffer copy.
    CopyFramebuffer(StackingContextIndex),
}

#[derive(Debug, Copy, Clone, Eq, PartialEq, Hash)]
pub enum MaskCacheKey {
    Primitive(PrimitiveIndex),
    ClipNode(ClipId),
}

#[derive(Debug, Copy, Clone)]
pub enum RenderTaskId {
    Static(RenderTaskIndex),
    Dynamic(RenderTaskKey),
}


#[derive(Debug, Clone)]
pub enum RenderTaskLocation {
    Fixed,
    Dynamic(Option<(DeviceIntPoint, RenderTargetIndex)>, DeviceIntSize),
}

#[derive(Debug, Clone)]
pub enum AlphaRenderItem {
    Primitive(ClipScrollGroupIndex, PrimitiveIndex, i32),
    Blend(StackingContextIndex, RenderTaskId, LowLevelFilterOp, i32),
    Composite(StackingContextIndex, RenderTaskId, RenderTaskId, MixBlendMode, i32),
    HardwareComposite(StackingContextIndex, RenderTaskId, HardwareCompositeOp, i32),
}

#[derive(Debug, Clone)]
pub struct AlphaRenderTask {
    screen_origin: DeviceIntPoint,
    pub items: Vec<AlphaRenderItem>,
    pub isolate_clear: bool,
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

#[derive(Debug, Clone)]
pub struct CacheMaskTask {
    actual_rect: DeviceIntRect,
    inner_rect: DeviceIntRect,
    pub clips: Vec<(PackedLayerIndex, MaskCacheInfo)>,
    pub geometry_kind: MaskGeometryKind,
}

#[derive(Debug)]
pub enum MaskResult {
    /// The mask is completely outside the region
    Outside,
    /// The mask is inside and needs to be processed
    Inside(RenderTask),
}

#[derive(Debug, Clone)]
pub struct RenderTaskData {
    pub data: [f32; FLOATS_PER_RENDER_TASK_INFO],
}

impl RenderTaskData {
    pub fn empty() -> RenderTaskData {
        RenderTaskData {
            data: unsafe { mem::uninitialized() }
        }
    }
}

impl Default for RenderTaskData {
    fn default() -> RenderTaskData {
        RenderTaskData {
            data: unsafe { mem::uninitialized() },
        }
    }
}

#[derive(Debug, Clone)]
pub enum RenderTaskKind {
    Alpha(AlphaRenderTask),
    CachePrimitive(PrimitiveIndex),
    CacheMask(CacheMaskTask),
    VerticalBlur(DeviceIntLength, PrimitiveIndex),
    HorizontalBlur(DeviceIntLength, PrimitiveIndex),
    Readback(DeviceIntRect),
}

// TODO(gw): Consider storing these in a separate array and having
//           primitives hold indices - this could avoid cloning
//           when adding them as child tasks to tiles.
#[derive(Debug, Clone)]
pub struct RenderTask {
    pub id: RenderTaskId,
    pub location: RenderTaskLocation,
    pub children: Vec<RenderTask>,
    pub kind: RenderTaskKind,
}

impl RenderTask {
    pub fn new_alpha_batch(task_index: RenderTaskIndex,
                           screen_origin: DeviceIntPoint,
                           isolate_clear: bool,
                           location: RenderTaskLocation) -> RenderTask {
        RenderTask {
            id: RenderTaskId::Static(task_index),
            children: Vec::new(),
            location: location,
            kind: RenderTaskKind::Alpha(AlphaRenderTask {
                screen_origin: screen_origin,
                items: Vec::new(),
                isolate_clear: isolate_clear,
            }),
        }
    }

    pub fn new_prim_cache(key: PrimitiveCacheKey,
                          size: DeviceIntSize,
                          prim_index: PrimitiveIndex) -> RenderTask {
        RenderTask {
            id: RenderTaskId::Dynamic(RenderTaskKey::CachePrimitive(key)),
            children: Vec::new(),
            location: RenderTaskLocation::Dynamic(None, size),
            kind: RenderTaskKind::CachePrimitive(prim_index),
        }
    }

    pub fn new_readback(key: StackingContextIndex,
                    screen_rect: DeviceIntRect) -> RenderTask {
        RenderTask {
            id: RenderTaskId::Dynamic(RenderTaskKey::CopyFramebuffer(key)),
            children: Vec::new(),
            location: RenderTaskLocation::Dynamic(None, screen_rect.size),
            kind: RenderTaskKind::Readback(screen_rect),
        }
    }

    pub fn new_mask(actual_rect: DeviceIntRect,
                    mask_key: MaskCacheKey,
                    clips: &[(PackedLayerIndex, MaskCacheInfo)])
                    -> MaskResult {
        if clips.is_empty() {
            return MaskResult::Outside;
        }

        // We scan through the clip stack and detect if our actual rectangle
        // is in the intersection of all of all the outer bounds,
        // and if it's completely inside the intersection of all of the inner bounds.

        // TODO(gw): If we encounter a clip with unknown bounds, we'll just use
        // the original rect. This is overly conservative, but can
        // be optimized later.
        let mut result = Some(actual_rect);
        for &(_, ref clip) in clips {
            match *clip.bounds.as_ref().unwrap() {
                MaskBounds::OuterInner(ref outer, _) |
                MaskBounds::Outer(ref outer) => {
                    result = result.and_then(|rect| {
                        rect.intersection(&outer.bounding_rect)
                    });
                }
                MaskBounds::None => {
                    result = Some(actual_rect);
                    break;
                }
            }
        }

        let task_rect = match result {
            None => return MaskResult::Outside,
            Some(rect) => rect,
        };

        // Accumulate inner rects. As soon as we encounter
        // a clip mask where we don't have or don't know
        // the inner rect, this will become None.
        let inner_rect = clips.iter()
                              .fold(Some(task_rect), |current, clip| {
            current.and_then(|rect| {
                let inner_rect = match *clip.1.bounds.as_ref().unwrap() {
                    MaskBounds::Outer(..) |
                    MaskBounds::None => DeviceIntRect::zero(),
                    MaskBounds::OuterInner(_, ref inner) => inner.bounding_rect
                };
                rect.intersection(&inner_rect)
            })
        });

        // TODO(gw): This optimization is very conservative for now.
        //           For now, only draw optimized geometry if it is
        //           a single aligned rect mask with rounded corners.
        //           In the future, we'll expand this to handle the
        //           more complex types of clip mask geometry.
        let mut geometry_kind = MaskGeometryKind::Default;
        if inner_rect.is_some() && clips.len() == 1 {
            let (_, ref clip_info) = clips[0];
            if clip_info.image.is_none() &&
               clip_info.effective_clip_count == 1 &&
               clip_info.is_aligned {
                geometry_kind = MaskGeometryKind::CornersOnly;
            }
        }

        let inner_rect = inner_rect.unwrap_or(DeviceIntRect::zero());

        MaskResult::Inside(RenderTask {
            id: RenderTaskId::Dynamic(RenderTaskKey::CacheMask(mask_key)),
            children: Vec::new(),
            location: RenderTaskLocation::Dynamic(None, task_rect.size),
            kind: RenderTaskKind::CacheMask(CacheMaskTask {
                actual_rect: task_rect,
                inner_rect: inner_rect,
                clips: clips.to_vec(),
                geometry_kind: geometry_kind,
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
    pub fn new_blur(key: PrimitiveCacheKey,
                    size: DeviceIntSize,
                    blur_radius: DeviceIntLength,
                    prim_index: PrimitiveIndex) -> RenderTask {
        let prim_cache_task = RenderTask::new_prim_cache(key,
                                                         size,
                                                         prim_index);

        let blur_target_size = size + DeviceIntSize::new(2 * blur_radius.0,
                                                         2 * blur_radius.0);

        let blur_task_v = RenderTask {
            id: RenderTaskId::Dynamic(RenderTaskKey::VerticalBlur(blur_radius.0, prim_index)),
            children: vec![prim_cache_task],
            location: RenderTaskLocation::Dynamic(None, blur_target_size),
            kind: RenderTaskKind::VerticalBlur(blur_radius, prim_index),
        };

        let blur_task_h = RenderTask {
            id: RenderTaskId::Dynamic(RenderTaskKey::HorizontalBlur(blur_radius.0, prim_index)),
            children: vec![blur_task_v],
            location: RenderTaskLocation::Dynamic(None, blur_target_size),
            kind: RenderTaskKind::HorizontalBlur(blur_radius, prim_index),
        };

        blur_task_h
    }

    pub fn as_alpha_batch<'a>(&'a mut self) -> &'a mut AlphaRenderTask {
        match self.kind {
            RenderTaskKind::Alpha(ref mut task) => task,
            RenderTaskKind::CachePrimitive(..) |
            RenderTaskKind::CacheMask(..) |
            RenderTaskKind::VerticalBlur(..) |
            RenderTaskKind::Readback(..) |
            RenderTaskKind::HorizontalBlur(..) => unreachable!(),
        }
    }

    // Write (up to) 8 floats of data specific to the type
    // of render task that is provided to the GPU shaders
    // via a vertex texture.
    pub fn write_task_data(&self) -> RenderTaskData {
        let (target_rect, target_index) = self.get_target_rect();

        // NOTE: The ordering and layout of these structures are
        //       required to match both the GPU structures declared
        //       in prim_shared.glsl, and also the uses in submit_batch()
        //       in renderer.rs.
        // TODO(gw): Maybe there's a way to make this stuff a bit
        //           more type-safe. Although, it will always need
        //           to be kept in sync with the GLSL code anyway.

        match self.kind {
            RenderTaskKind::Alpha(ref task) => {
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
            RenderTaskKind::CachePrimitive(..) => {
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
            RenderTaskKind::VerticalBlur(blur_radius, _) |
            RenderTaskKind::HorizontalBlur(blur_radius, _) => {
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
        }
    }

    fn get_target_rect(&self) -> (DeviceIntRect, RenderTargetIndex) {
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

    pub fn assign_to_passes(mut self, pass_index: usize, passes: &mut Vec<RenderPass>) {
        for child in self.children.drain(..) {
            child.assign_to_passes(pass_index - 1,
                                   passes);
        }

        // Sanity check - can be relaxed if needed
        match self.location {
            RenderTaskLocation::Fixed => {
                debug_assert!(pass_index == passes.len() - 1);
            }
            RenderTaskLocation::Dynamic(..) => {
                debug_assert!(pass_index < passes.len() - 1);
            }
        }

        let pass = &mut passes[pass_index];
        pass.add_render_task(self);
    }

    pub fn max_depth(&self, depth: usize, max_depth: &mut usize) {
        let depth = depth + 1;
        *max_depth = cmp::max(*max_depth, depth);
        for child in &self.children {
            child.max_depth(depth, max_depth);
        }
    }

    pub fn target_kind(&self) -> RenderTargetKind {
        match self.kind {
            RenderTaskKind::Alpha(..) |
            RenderTaskKind::CachePrimitive(..) |
            RenderTaskKind::VerticalBlur(..) |
            RenderTaskKind::Readback(..) |
            RenderTaskKind::HorizontalBlur(..) => RenderTargetKind::Color,
            RenderTaskKind::CacheMask(..) => RenderTargetKind::Alpha,
        }
    }
}
