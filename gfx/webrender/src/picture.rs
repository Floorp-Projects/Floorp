/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{ColorF, FilterOp, MixBlendMode, PipelineId};
use api::{DeviceIntRect, LayerRect, LayerToWorldScale, LayerVector2D};
use api::{PremultipliedColorF, Shadow};
use box_shadow::{BLUR_SAMPLE_SCALE};
use clip_scroll_tree::ClipScrollNodeIndex;
use frame_builder::{FrameBuildingContext, FrameBuildingState, PictureState};
use gpu_cache::{GpuCacheHandle, GpuDataRequest};
use gpu_types::{PictureType};
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
pub enum PictureKind {
    TextShadow {
        offset: LayerVector2D,
        color: ColorF,
        blur_radius: f32,
    },
    Image {
        // If a mix-blend-mode, contains the render task for
        // the readback of the framebuffer that we use to sample
        // from in the mix-blend-mode shader.
        // For drop-shadow filter, this will store the original
        // picture task which would be rendered on screen after
        // blur pass.
        secondary_render_task_id: Option<RenderTaskId>,
        /// How this picture should be composited.
        /// If None, don't composite - just draw directly on parent surface.
        composite_mode: Option<PictureCompositeMode>,
        // If true, this picture is part of a 3D context.
        is_in_3d_context: bool,
        // If requested as a frame output (for rendering
        // pages to a texture), this is the pipeline this
        // picture is the root of.
        frame_output_pipeline_id: Option<PipelineId>,
        // The original reference frame ID for this picture.
        // It is only different if this is part of a 3D
        // rendering context.
        reference_frame_index: ClipScrollNodeIndex,
        real_local_rect: LayerRect,
        // An optional cache handle for storing extra data
        // in the GPU cache, depending on the type of
        // picture.
        extra_gpu_data_handle: GpuCacheHandle,
    },
}

#[derive(Debug)]
pub struct PicturePrimitive {
    // If this picture is drawn to an intermediate surface,
    // the associated target information.
    pub surface: Option<RenderTaskId>,

    // Details specific to this type of picture.
    pub kind: PictureKind,

    // List of primitive runs that make up this picture.
    pub runs: Vec<PrimitiveRun>,

    // The pipeline that the primitives on this picture belong to.
    pub pipeline_id: PipelineId,

    // The current screen-space rect of the rendered
    // portion of this picture.
    task_rect: DeviceIntRect,
}

impl PicturePrimitive {
    pub fn new_text_shadow(shadow: Shadow, pipeline_id: PipelineId) -> Self {
        PicturePrimitive {
            runs: Vec::new(),
            surface: None,
            kind: PictureKind::TextShadow {
                offset: shadow.offset,
                color: shadow.color,
                blur_radius: shadow.blur_radius,
            },
            pipeline_id,
            task_rect: DeviceIntRect::zero(),
        }
    }

    pub fn resolve_scene_properties(&mut self, properties: &SceneProperties) -> bool {
        match self.kind {
            PictureKind::Image { ref mut composite_mode, .. } => {
                match composite_mode {
                    &mut Some(PictureCompositeMode::Filter(ref mut filter)) => {
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
            _ => true
        }
    }

    pub fn new_image(
        composite_mode: Option<PictureCompositeMode>,
        is_in_3d_context: bool,
        pipeline_id: PipelineId,
        reference_frame_index: ClipScrollNodeIndex,
        frame_output_pipeline_id: Option<PipelineId>,
    ) -> Self {
        PicturePrimitive {
            runs: Vec::new(),
            surface: None,
            kind: PictureKind::Image {
                secondary_render_task_id: None,
                composite_mode,
                is_in_3d_context,
                frame_output_pipeline_id,
                reference_frame_index,
                real_local_rect: LayerRect::zero(),
                extra_gpu_data_handle: GpuCacheHandle::new(),
            },
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

        match self.kind {
            PictureKind::Image { composite_mode, ref mut real_local_rect, .. } => {
                *real_local_rect = prim_run_rect.local_rect_in_original_parent_space;

                match composite_mode {
                    Some(PictureCompositeMode::Filter(FilterOp::Blur(blur_radius))) => {
                        let inflate_size = blur_radius * BLUR_SAMPLE_SCALE;
                        local_content_rect.inflate(inflate_size, inflate_size)
                    }
                    Some(PictureCompositeMode::Filter(FilterOp::DropShadow(offset, blur_radius, _))) => {
                        let inflate_size = blur_radius * BLUR_SAMPLE_SCALE;
                        local_content_rect.inflate(inflate_size, inflate_size)
                                          .translate(&offset)
                    }
                    _ => {
                        local_content_rect
                    }
                }
            }
            PictureKind::TextShadow { blur_radius, .. } => {
                let blur_offset = blur_radius * BLUR_SAMPLE_SCALE;

                local_content_rect.inflate(
                    blur_offset,
                    blur_offset,
                )
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
        let content_scale = LayerToWorldScale::new(1.0) * frame_context.device_pixel_scale;
        let prim_screen_rect = prim_metadata
                                .screen_rect
                                .as_ref()
                                .expect("bug: trying to draw an off-screen picture!?");
        let device_rect;

        match self.kind {
            PictureKind::Image {
                ref mut secondary_render_task_id,
                ref mut extra_gpu_data_handle,
                composite_mode,
                ..
            } => {
                device_rect = match composite_mode {
                    Some(PictureCompositeMode::Filter(FilterOp::Blur(blur_radius))) => {
                        // If blur radius is 0, we can skip drawing this an an
                        // intermediate surface.
                        if blur_radius == 0.0 {
                            pic_state.tasks.extend(pic_state_for_children.tasks);
                            self.surface = None;

                            DeviceIntRect::zero()
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
                                RenderTargetKind::Color,
                                device_rect.origin,
                                PremultipliedColorF::TRANSPARENT,
                                ClearMode::Transparent,
                                pic_state_for_children.tasks,
                                PictureType::Image,
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

                            device_rect
                        }
                    }
                    Some(PictureCompositeMode::Filter(FilterOp::DropShadow(offset, blur_radius, _))) => {
                        // TODO(gw): This is totally wrong and can never work with
                        //           transformed drop-shadow elements. Fix me!
                        let rect = (prim_metadata.local_rect.translate(&-offset) * content_scale).round().to_i32();
                        let mut picture_task = RenderTask::new_picture(
                            RenderTaskLocation::Dynamic(None, rect.size),
                            prim_index,
                            RenderTargetKind::Color,
                            rect.origin,
                            PremultipliedColorF::TRANSPARENT,
                            ClearMode::Transparent,
                            pic_state_for_children.tasks,
                            PictureType::Image,
                        );
                        picture_task.mark_for_saving();

                        let blur_std_deviation = blur_radius * frame_context.device_pixel_scale.0;
                        let picture_task_id = frame_state.render_tasks.add(picture_task);

                        let blur_render_task = RenderTask::new_blur(
                            blur_std_deviation.round(),
                            picture_task_id,
                            frame_state.render_tasks,
                            RenderTargetKind::Color,
                            ClearMode::Transparent,
                        );

                        *secondary_render_task_id = Some(picture_task_id);

                        let render_task_id = frame_state.render_tasks.add(blur_render_task);
                        pic_state.tasks.push(render_task_id);
                        self.surface = Some(render_task_id);

                        rect
                    }
                    Some(PictureCompositeMode::MixBlend(..)) => {
                        let picture_task = RenderTask::new_picture(
                            RenderTaskLocation::Dynamic(None, prim_screen_rect.clipped.size),
                            prim_index,
                            RenderTargetKind::Color,
                            prim_screen_rect.clipped.origin,
                            PremultipliedColorF::TRANSPARENT,
                            ClearMode::Transparent,
                            pic_state_for_children.tasks,
                            PictureType::Image,
                        );

                        let readback_task_id = frame_state.render_tasks.add(
                            RenderTask::new_readback(prim_screen_rect.clipped)
                        );

                        *secondary_render_task_id = Some(readback_task_id);
                        pic_state.tasks.push(readback_task_id);

                        let render_task_id = frame_state.render_tasks.add(picture_task);
                        pic_state.tasks.push(render_task_id);
                        self.surface = Some(render_task_id);

                        prim_screen_rect.clipped
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
                        } else {

                            if let FilterOp::ColorMatrix(m) = filter {
                                if let Some(mut request) = frame_state.gpu_cache.request(extra_gpu_data_handle) {
                                    for i in 0..5 {
                                        request.push([m[i*4], m[i*4+1], m[i*4+2], m[i*4+3]]);
                                    }
                                }
                            }

                            let picture_task = RenderTask::new_picture(
                                RenderTaskLocation::Dynamic(None, prim_screen_rect.clipped.size),
                                prim_index,
                                RenderTargetKind::Color,
                                prim_screen_rect.clipped.origin,
                                PremultipliedColorF::TRANSPARENT,
                                ClearMode::Transparent,
                                pic_state_for_children.tasks,
                                PictureType::Image,
                            );

                            let render_task_id = frame_state.render_tasks.add(picture_task);
                            pic_state.tasks.push(render_task_id);
                            self.surface = Some(render_task_id);
                        }

                        prim_screen_rect.clipped
                    }
                    Some(PictureCompositeMode::Blit) => {
                        let picture_task = RenderTask::new_picture(
                            RenderTaskLocation::Dynamic(None, prim_screen_rect.clipped.size),
                            prim_index,
                            RenderTargetKind::Color,
                            prim_screen_rect.clipped.origin,
                            PremultipliedColorF::TRANSPARENT,
                            ClearMode::Transparent,
                            pic_state_for_children.tasks,
                            PictureType::Image,
                        );

                        let render_task_id = frame_state.render_tasks.add(picture_task);
                        pic_state.tasks.push(render_task_id);
                        self.surface = Some(render_task_id);

                        prim_screen_rect.clipped
                    }
                    None => {
                        pic_state.tasks.extend(pic_state_for_children.tasks);
                        self.surface = None;

                        DeviceIntRect::zero()
                    }
                };
            }
            PictureKind::TextShadow { blur_radius, color, .. } => {
                // This is a shadow element. Create a render task that will
                // render the text run to a target, and then apply a gaussian
                // blur to that text run in order to build the actual primitive
                // which will be blitted to the framebuffer.

                // Quote from https://drafts.csswg.org/css-backgrounds-3/#shadow-blur
                // "the image that would be generated by applying to the shadow a
                // Gaussian blur with a standard deviation equal to half the blur radius."
                let device_radius = (blur_radius * frame_context.device_pixel_scale.0).round();
                let blur_std_deviation = device_radius * 0.5;

                let blur_range = (blur_std_deviation * BLUR_SAMPLE_SCALE).ceil() as i32;

                device_rect = prim_screen_rect
                    .clipped
                    .inflate(blur_range, blur_range)
                    .intersection(&prim_screen_rect.unclipped)
                    .unwrap();

                let picture_task = RenderTask::new_picture(
                    RenderTaskLocation::Dynamic(None, device_rect.size),
                    prim_index,
                    RenderTargetKind::Color,
                    device_rect.origin,
                    color.premultiplied(),
                    ClearMode::Transparent,
                    Vec::new(),
                    PictureType::TextShadow,
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
            }
        }

        // If scrolling or property animation has resulted in the task
        // rect being different than last time, invalidate the GPU
        // cache entry for this picture to ensure that the correct
        // task rect is provided to the image shader.
        if self.task_rect != device_rect {
            frame_state.gpu_cache.invalidate(&prim_metadata.gpu_location);
            self.task_rect = device_rect;
        }
    }

    pub fn write_gpu_blocks(&self, request: &mut GpuDataRequest) {
        request.push(self.task_rect.to_f32());

        match self.kind {
            PictureKind::TextShadow { .. } => {
                request.push(PremultipliedColorF::WHITE);
            }
            PictureKind::Image { composite_mode, .. } => {
                let color = match composite_mode {
                    Some(PictureCompositeMode::Filter(FilterOp::DropShadow(_, _, color))) => {
                        color.premultiplied()
                    }
                    _ => {
                        PremultipliedColorF::WHITE
                    }
                };

                request.push(color);
            }
        }
    }
}
