/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{FilterOp, LayerVector2D, MixBlendMode, PipelineId, PremultipliedColorF};
use api::{DeviceIntRect, LayerRect};
use box_shadow::{BLUR_SAMPLE_SCALE};
use clip_scroll_tree::ClipScrollNodeIndex;
use frame_builder::{FrameBuildingContext, FrameBuildingState, PictureState};
use gpu_cache::{GpuCacheHandle};
use prim_store::{PrimitiveIndex, PrimitiveRun, PrimitiveRunLocalRect};
use prim_store::{PrimitiveMetadata, ScrollNodeAndClipChain};
use render_task::{ClearMode, RenderTask};
use render_task::{RenderTaskId, RenderTaskLocation};
use scene::{FilterOpHelpers, SceneProperties};
use tiling::RenderTargetKind;

/*
 A picture represents a dynamically rendered image. It consists of:

 * A number of primitives that are drawn onto the picture.
 * A composite operation describing how to composite this
   picture into its parent.
 * A configuration describing how to draw the primitives on
   this picture (e.g. in screen space or local space).
 */

/// Specifies how this Picture should be composited
/// onto the target it belongs to.
#[derive(Debug, Copy, Clone, PartialEq)]
pub enum PictureCompositeMode {
    /// Apply CSS mix-blend-mode effect.
    MixBlend(MixBlendMode),
    /// Apply a CSS filter.
    Filter(FilterOp),
    /// Draw to intermediate surface, copy straight across. This
    /// is used for CSS isolation, and plane splitting.
    Blit,
}

#[derive(Debug)]
pub struct PicturePrimitive {
    // If this picture is drawn to an intermediate surface,
    // the associated target information.
    pub surface: Option<RenderTaskId>,

    // List of primitive runs that make up this picture.
    pub runs: Vec<PrimitiveRun>,

    // The pipeline that the primitives on this picture belong to.
    pub pipeline_id: PipelineId,

    // If true, apply the local clip rect to primitive drawn
    // in this picture.
    pub apply_local_clip_rect: bool,

    // The current screen-space rect of the rendered
    // portion of this picture.
    task_rect: DeviceIntRect,

    // If a mix-blend-mode, contains the render task for
    // the readback of the framebuffer that we use to sample
    // from in the mix-blend-mode shader.
    // For drop-shadow filter, this will store the original
    // picture task which would be rendered on screen after
    // blur pass.
    pub secondary_render_task_id: Option<RenderTaskId>,
    /// How this picture should be composited.
    /// If None, don't composite - just draw directly on parent surface.
    pub composite_mode: Option<PictureCompositeMode>,
    // If true, this picture is part of a 3D context.
    pub is_in_3d_context: bool,
    // If requested as a frame output (for rendering
    // pages to a texture), this is the pipeline this
    // picture is the root of.
    pub frame_output_pipeline_id: Option<PipelineId>,
    // The original reference frame ID for this picture.
    // It is only different if this is part of a 3D
    // rendering context.
    pub reference_frame_index: ClipScrollNodeIndex,
    pub real_local_rect: LayerRect,
    // An optional cache handle for storing extra data
    // in the GPU cache, depending on the type of
    // picture.
    pub extra_gpu_data_handle: GpuCacheHandle,
}

impl PicturePrimitive {
    pub fn resolve_scene_properties(&mut self, properties: &SceneProperties) -> bool {
        match self.composite_mode {
            Some(PictureCompositeMode::Filter(ref mut filter)) => {
                match filter {
                    &mut FilterOp::Opacity(ref binding, ref mut value) => {
                        *value = properties.resolve_float(binding, *value);
                    }
                    _ => {}
                }

                filter.is_visible()
            }
            _ => true,
        }
    }

    pub fn new_image(
        composite_mode: Option<PictureCompositeMode>,
        is_in_3d_context: bool,
        pipeline_id: PipelineId,
        reference_frame_index: ClipScrollNodeIndex,
        frame_output_pipeline_id: Option<PipelineId>,
        apply_local_clip_rect: bool,
    ) -> Self {
        PicturePrimitive {
            runs: Vec::new(),
            surface: None,
            secondary_render_task_id: None,
            composite_mode,
            is_in_3d_context,
            frame_output_pipeline_id,
            reference_frame_index,
            real_local_rect: LayerRect::zero(),
            extra_gpu_data_handle: GpuCacheHandle::new(),
            apply_local_clip_rect,
            pipeline_id,
            task_rect: DeviceIntRect::zero(),
        }
    }

    pub fn add_primitive(
        &mut self,
        prim_index: PrimitiveIndex,
        clip_and_scroll: ScrollNodeAndClipChain
    ) {
        if let Some(ref mut run) = self.runs.last_mut() {
            if run.clip_and_scroll == clip_and_scroll &&
               run.base_prim_index.0 + run.count == prim_index.0 {
                run.count += 1;
                return;
            }
        }

        self.runs.push(PrimitiveRun {
            base_prim_index: prim_index,
            count: 1,
            clip_and_scroll,
        });
    }

    pub fn update_local_rect(
        &mut self,
        prim_run_rect: PrimitiveRunLocalRect,
    ) -> LayerRect {
        let local_content_rect = prim_run_rect.local_rect_in_actual_parent_space;

        self.real_local_rect = prim_run_rect.local_rect_in_original_parent_space;

        match self.composite_mode {
            Some(PictureCompositeMode::Filter(FilterOp::Blur(blur_radius))) => {
                let inflate_size = (blur_radius * BLUR_SAMPLE_SCALE).ceil();
                local_content_rect.inflate(inflate_size, inflate_size)
            }
            Some(PictureCompositeMode::Filter(FilterOp::DropShadow(_, blur_radius, _))) => {
                let inflate_size = (blur_radius * BLUR_SAMPLE_SCALE).ceil();
                local_content_rect.inflate(inflate_size, inflate_size)
            }
            _ => {
                local_content_rect
            }
        }
    }

    pub fn prepare_for_render(
        &mut self,
        prim_index: PrimitiveIndex,
        prim_metadata: &mut PrimitiveMetadata,
        pic_state_for_children: PictureState,
        pic_state: &mut PictureState,
        frame_context: &FrameBuildingContext,
        frame_state: &mut FrameBuildingState,
    ) {
        let prim_screen_rect = prim_metadata
                                .screen_rect
                                .as_ref()
                                .expect("bug: trying to draw an off-screen picture!?");

        // TODO(gw): Almost all of the Picture types below use extra_gpu_cache_data
        //           to store the same type of data. The exception is the filter
        //           with a ColorMatrix, which stores the color matrix here. It's
        //           probably worth tidying this code up to be a bit more consistent.
        //           Perhaps store the color matrix after the common data, even though
        //           it's not used by that shader.
        let device_rect = match self.composite_mode {
            Some(PictureCompositeMode::Filter(FilterOp::Blur(blur_radius))) => {
                // If blur radius is 0, we can skip drawing this on an
                // intermediate surface.
                if blur_radius == 0.0 {
                    pic_state.tasks.extend(pic_state_for_children.tasks);
                    self.surface = None;

                    None
                } else {
                    let blur_std_deviation = blur_radius * frame_context.device_pixel_scale.0;
                    let blur_range = (blur_std_deviation * BLUR_SAMPLE_SCALE).ceil() as i32;

                    // The clipped field is the part of the picture that is visible
                    // on screen. The unclipped field is the screen-space rect of
                    // the complete picture, if no screen / clip-chain was applied
                    // (this includes the extra space for blur region). To ensure
                    // that we draw a large enough part of the picture to get correct
                    // blur results, inflate that clipped area by the blur range, and
                    // then intersect with the total screen rect, to minimize the
                    // allocation size.
                    let device_rect = prim_screen_rect
                        .clipped
                        .inflate(blur_range, blur_range)
                        .intersection(&prim_screen_rect.unclipped)
                        .unwrap();

                    let picture_task = RenderTask::new_picture(
                        RenderTaskLocation::Dynamic(None, device_rect.size),
                        prim_index,
                        device_rect.origin,
                        pic_state_for_children.tasks,
                    );

                    let picture_task_id = frame_state.render_tasks.add(picture_task);

                    let blur_render_task = RenderTask::new_blur(
                        blur_std_deviation,
                        picture_task_id,
                        frame_state.render_tasks,
                        RenderTargetKind::Color,
                        ClearMode::Transparent,
                    );

                    let render_task_id = frame_state.render_tasks.add(blur_render_task);
                    pic_state.tasks.push(render_task_id);
                    self.surface = Some(render_task_id);

                    Some(device_rect)
                }
            }
            Some(PictureCompositeMode::Filter(FilterOp::DropShadow(_, blur_radius, _))) => {
                let blur_std_deviation = blur_radius * frame_context.device_pixel_scale.0;
                let blur_range = (blur_std_deviation * BLUR_SAMPLE_SCALE).ceil() as i32;

                // The clipped field is the part of the picture that is visible
                // on screen. The unclipped field is the screen-space rect of
                // the complete picture, if no screen / clip-chain was applied
                // (this includes the extra space for blur region). To ensure
                // that we draw a large enough part of the picture to get correct
                // blur results, inflate that clipped area by the blur range, and
                // then intersect with the total screen rect, to minimize the
                // allocation size.
                let device_rect = prim_screen_rect
                    .clipped
                    .inflate(blur_range, blur_range)
                    .intersection(&prim_screen_rect.unclipped)
                    .unwrap();

                let mut picture_task = RenderTask::new_picture(
                    RenderTaskLocation::Dynamic(None, device_rect.size),
                    prim_index,
                    device_rect.origin,
                    pic_state_for_children.tasks,
                );
                picture_task.mark_for_saving();

                let picture_task_id = frame_state.render_tasks.add(picture_task);

                let blur_render_task = RenderTask::new_blur(
                    blur_std_deviation.round(),
                    picture_task_id,
                    frame_state.render_tasks,
                    RenderTargetKind::Color,
                    ClearMode::Transparent,
                );

                self.secondary_render_task_id = Some(picture_task_id);

                let render_task_id = frame_state.render_tasks.add(blur_render_task);
                pic_state.tasks.push(render_task_id);
                self.surface = Some(render_task_id);

                Some(device_rect)
            }
            Some(PictureCompositeMode::MixBlend(..)) => {
                let picture_task = RenderTask::new_picture(
                    RenderTaskLocation::Dynamic(None, prim_screen_rect.clipped.size),
                    prim_index,
                    prim_screen_rect.clipped.origin,
                    pic_state_for_children.tasks,
                );

                let readback_task_id = frame_state.render_tasks.add(
                    RenderTask::new_readback(prim_screen_rect.clipped)
                );

                self.secondary_render_task_id = Some(readback_task_id);
                pic_state.tasks.push(readback_task_id);

                let render_task_id = frame_state.render_tasks.add(picture_task);
                pic_state.tasks.push(render_task_id);
                self.surface = Some(render_task_id);

                Some(prim_screen_rect.clipped)
            }
            Some(PictureCompositeMode::Filter(filter)) => {
                // If this filter is not currently going to affect
                // the picture, just collapse this picture into the
                // current render task. This most commonly occurs
                // when opacity == 1.0, but can also occur on other
                // filters and be a significant performance win.
                if filter.is_noop() {
                    pic_state.tasks.extend(pic_state_for_children.tasks);
                    self.surface = None;

                    None
                } else {
                    let device_rect = match filter {
                        FilterOp::ColorMatrix(m) => {
                            if let Some(mut request) = frame_state.gpu_cache.request(&mut self.extra_gpu_data_handle) {
                                for i in 0..5 {
                                    request.push([m[i*4], m[i*4+1], m[i*4+2], m[i*4+3]]);
                                }
                            }

                            None
                        }
                        _ => {
                            Some(prim_screen_rect.clipped)
                        }
                    };

                    let picture_task = RenderTask::new_picture(
                        RenderTaskLocation::Dynamic(None, prim_screen_rect.clipped.size),
                        prim_index,
                        prim_screen_rect.clipped.origin,
                        pic_state_for_children.tasks,
                    );

                    let render_task_id = frame_state.render_tasks.add(picture_task);
                    pic_state.tasks.push(render_task_id);
                    self.surface = Some(render_task_id);

                    device_rect
                }
            }
            Some(PictureCompositeMode::Blit) => {
                let picture_task = RenderTask::new_picture(
                    RenderTaskLocation::Dynamic(None, prim_screen_rect.clipped.size),
                    prim_index,
                    prim_screen_rect.clipped.origin,
                    pic_state_for_children.tasks,
                );

                let render_task_id = frame_state.render_tasks.add(picture_task);
                pic_state.tasks.push(render_task_id);
                self.surface = Some(render_task_id);

                Some(prim_screen_rect.clipped)
            }
            None => {
                pic_state.tasks.extend(pic_state_for_children.tasks);
                self.surface = None;

                None
            }
        };

        // If this picture type uses the common / general GPU data
        // format, then write it now.
        if let Some(device_rect) = device_rect {
            // If scrolling or property animation has resulted in the task
            // rect being different than last time, invalidate the GPU
            // cache entry for this picture to ensure that the correct
            // task rect is provided to the image shader.
            if self.task_rect != device_rect {
                frame_state.gpu_cache.invalidate(&self.extra_gpu_data_handle);
                self.task_rect = device_rect;
            }

            if let Some(mut request) = frame_state.gpu_cache.request(&mut self.extra_gpu_data_handle) {
                request.push(self.task_rect.to_f32());

                // TODO(gw): It would make the shaders a bit simpler if the offset
                //           was provided as part of the brush::picture instance,
                //           rather than in the Picture data itself.
                let (offset, color) = match self.composite_mode {
                    Some(PictureCompositeMode::Filter(FilterOp::DropShadow(offset, _, color))) => {
                        (offset, color.premultiplied())
                    }
                    _ => {
                        (LayerVector2D::zero(), PremultipliedColorF::WHITE)
                    }
                };

                request.push([offset.x, offset.y, 0.0, 0.0]);
                request.push(color);
            }
        }
    }
}
