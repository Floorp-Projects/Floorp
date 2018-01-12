/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{ColorF, ClipAndScrollInfo, FilterOp, MixBlendMode};
use api::{DeviceIntPoint, DeviceIntRect, LayerToWorldScale, PipelineId};
use api::{BoxShadowClipMode, LayerPoint, LayerRect, LayerVector2D, Shadow};
use api::{ClipId, PremultipliedColorF};
use box_shadow::{BLUR_SAMPLE_SCALE, BoxShadowCacheKey};
use frame_builder::PrimitiveContext;
use gpu_cache::GpuDataRequest;
use gpu_types::{BrushImageKind, PictureType};
use prim_store::{PrimitiveIndex, PrimitiveRun, PrimitiveRunLocalRect};
use render_task::{ClearMode, RenderTask, RenderTaskId, RenderTaskTree};
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

/// Configure whether the content to be drawn by a picture
/// in local space rasterization or the screen space.
#[derive(Debug, Copy, Clone, PartialEq)]
pub enum ContentOrigin {
    Local(LayerPoint),
    Screen(DeviceIntPoint),
}

#[derive(Debug)]
pub enum PictureKind {
    TextShadow {
        offset: LayerVector2D,
        color: ColorF,
        blur_radius: f32,
        content_rect: LayerRect,
    },
    BoxShadow {
        blur_radius: f32,
        color: ColorF,
        blur_regions: Vec<LayerRect>,
        clip_mode: BoxShadowClipMode,
        image_kind: BrushImageKind,
        content_rect: LayerRect,
        cache_key: BoxShadowCacheKey,
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
        reference_frame_id: ClipId,
        real_local_rect: LayerRect,
    },
}

#[derive(Debug)]
pub struct PicturePrimitive {
    // If this picture is drawn to an intermediate surface,
    // the associated render task.
    pub render_task_id: Option<RenderTaskId>,

    // Details specific to this type of picture.
    pub kind: PictureKind,

    // List of primitive runs that make up this picture.
    pub runs: Vec<PrimitiveRun>,

    // The pipeline that the primitives on this picture belong to.
    pub pipeline_id: PipelineId,

    // If true, apply visibility culling to primitives on this
    // picture. For text shadows and box shadows, we want to
    // unconditionally draw them.
    pub cull_children: bool,
}

impl PicturePrimitive {
    pub fn new_text_shadow(shadow: Shadow, pipeline_id: PipelineId) -> Self {
        PicturePrimitive {
            runs: Vec::new(),
            render_task_id: None,
            kind: PictureKind::TextShadow {
                offset: shadow.offset,
                color: shadow.color,
                blur_radius: shadow.blur_radius,
                content_rect: LayerRect::zero(),
            },
            pipeline_id,
            cull_children: false,
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

    pub fn new_box_shadow(
        blur_radius: f32,
        color: ColorF,
        blur_regions: Vec<LayerRect>,
        clip_mode: BoxShadowClipMode,
        image_kind: BrushImageKind,
        cache_key: BoxShadowCacheKey,
        pipeline_id: PipelineId,
    ) -> Self {
        PicturePrimitive {
            runs: Vec::new(),
            render_task_id: None,
            kind: PictureKind::BoxShadow {
                blur_radius,
                color,
                blur_regions,
                clip_mode,
                image_kind,
                content_rect: LayerRect::zero(),
                cache_key,
            },
            pipeline_id,
            cull_children: false,
        }
    }

    pub fn new_image(
        composite_mode: Option<PictureCompositeMode>,
        is_in_3d_context: bool,
        pipeline_id: PipelineId,
        reference_frame_id: ClipId,
        frame_output_pipeline_id: Option<PipelineId>,
    ) -> Self {
        PicturePrimitive {
            runs: Vec::new(),
            render_task_id: None,
            kind: PictureKind::Image {
                secondary_render_task_id: None,
                composite_mode,
                is_in_3d_context,
                frame_output_pipeline_id,
                reference_frame_id,
                real_local_rect: LayerRect::zero(),
            },
            pipeline_id,
            cull_children: true,
        }
    }

    pub fn add_primitive(
        &mut self,
        prim_index: PrimitiveIndex,
        clip_and_scroll: ClipAndScrollInfo
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

    pub fn update_local_rect(&mut self,
        prim_local_rect: LayerRect,
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
            PictureKind::TextShadow { offset, blur_radius, ref mut content_rect, .. } => {
                let blur_offset = blur_radius * BLUR_SAMPLE_SCALE;

                *content_rect = local_content_rect.inflate(
                    blur_offset,
                    blur_offset,
                );

                content_rect.translate(&offset)
            }
            PictureKind::BoxShadow { blur_radius, clip_mode, image_kind, ref mut content_rect, .. } => {
                // We need to inflate the content rect if outset.
                *content_rect = match clip_mode {
                    BoxShadowClipMode::Outset => {
                        match image_kind {
                            BrushImageKind::Mirror => {
                                let half_offset = 0.5 * blur_radius * BLUR_SAMPLE_SCALE;
                                // If the radii are uniform, we can render just the top
                                // left corner and mirror it across the primitive. In
                                // this case, shift the content rect to leave room
                                // for the blur to take effect.
                                local_content_rect
                                    .translate(&-LayerVector2D::new(half_offset, half_offset))
                                    .inflate(half_offset, half_offset)
                            }
                            BrushImageKind::NinePatch | BrushImageKind::Simple => {
                                let full_offset = blur_radius * BLUR_SAMPLE_SCALE;
                                // For a non-uniform radii, we need to expand
                                // the content rect on all sides for the blur.
                                local_content_rect.inflate(
                                    full_offset,
                                    full_offset,
                                )
                            }
                        }
                    }
                    BoxShadowClipMode::Inset => {
                        local_content_rect
                    }
                };

                prim_local_rect
            }
        }
    }

    pub fn picture_type(&self) -> PictureType {
        match self.kind {
            PictureKind::Image { .. } => PictureType::Image,
            PictureKind::BoxShadow { .. } => PictureType::BoxShadow,
            PictureKind::TextShadow { .. } => PictureType::TextShadow,
        }
    }

    pub fn prepare_for_render(
        &mut self,
        prim_index: PrimitiveIndex,
        prim_context: &PrimitiveContext,
        render_tasks: &mut RenderTaskTree,
        prim_screen_rect: &DeviceIntRect,
        prim_local_rect: &LayerRect,
        child_tasks: Vec<RenderTaskId>,
        parent_tasks: &mut Vec<RenderTaskId>,
    ) {
        let content_scale = LayerToWorldScale::new(1.0) * prim_context.device_pixel_scale;

        match self.kind {
            PictureKind::Image {
                ref mut secondary_render_task_id,
                composite_mode,
                ..
            } => {
                let content_origin = ContentOrigin::Screen(prim_screen_rect.origin);
                match composite_mode {
                    Some(PictureCompositeMode::Filter(FilterOp::Blur(blur_radius))) => {
                        let picture_task = RenderTask::new_picture(
                            Some(prim_screen_rect.size),
                            prim_index,
                            RenderTargetKind::Color,
                            content_origin,
                            PremultipliedColorF::TRANSPARENT,
                            ClearMode::Transparent,
                            child_tasks,
                            None,
                            PictureType::Image,
                        );

                        let blur_std_deviation = blur_radius * prim_context.device_pixel_scale.0;
                        let picture_task_id = render_tasks.add(picture_task);

                        let blur_render_task = RenderTask::new_blur(
                            blur_std_deviation,
                            picture_task_id,
                            render_tasks,
                            RenderTargetKind::Color,
                            &[],
                            ClearMode::Transparent,
                            PremultipliedColorF::TRANSPARENT,
                            None,
                        );

                        let blur_render_task_id = render_tasks.add(blur_render_task);
                        self.render_task_id = Some(blur_render_task_id);
                    }
                    Some(PictureCompositeMode::Filter(FilterOp::DropShadow(offset, blur_radius, color))) => {
                        let rect = (prim_local_rect.translate(&-offset) * content_scale).round().to_i32();
                        let picture_task = RenderTask::new_picture(
                            Some(rect.size),
                            prim_index,
                            RenderTargetKind::Color,
                            ContentOrigin::Screen(rect.origin),
                            PremultipliedColorF::TRANSPARENT,
                            ClearMode::Transparent,
                            child_tasks,
                            None,
                            PictureType::Image,
                        );

                        let blur_std_deviation = blur_radius * prim_context.device_pixel_scale.0;
                        let picture_task_id = render_tasks.add(picture_task);

                        let blur_render_task = RenderTask::new_blur(
                            blur_std_deviation.round(),
                            picture_task_id,
                            render_tasks,
                            RenderTargetKind::Color,
                            &[],
                            ClearMode::Transparent,
                            color.premultiplied(),
                            None,
                        );

                        *secondary_render_task_id = Some(picture_task_id);
                        self.render_task_id = Some(render_tasks.add(blur_render_task));
                    }
                    Some(PictureCompositeMode::MixBlend(..)) => {
                        let picture_task = RenderTask::new_picture(
                            Some(prim_screen_rect.size),
                            prim_index,
                            RenderTargetKind::Color,
                            content_origin,
                            PremultipliedColorF::TRANSPARENT,
                            ClearMode::Transparent,
                            child_tasks,
                            None,
                            PictureType::Image,
                        );

                        let readback_task_id = render_tasks.add(RenderTask::new_readback(*prim_screen_rect));

                        *secondary_render_task_id = Some(readback_task_id);
                        parent_tasks.push(readback_task_id);

                        self.render_task_id = Some(render_tasks.add(picture_task));
                    }
                    Some(PictureCompositeMode::Filter(filter)) => {
                        // If this filter is not currently going to affect
                        // the picture, just collapse this picture into the
                        // current render task. This most commonly occurs
                        // when opacity == 1.0, but can also occur on other
                        // filters and be a significant performance win.
                        if filter.is_noop() {
                            parent_tasks.extend(child_tasks);
                            self.render_task_id = None;
                        } else {
                            let picture_task = RenderTask::new_picture(
                                Some(prim_screen_rect.size),
                                prim_index,
                                RenderTargetKind::Color,
                                content_origin,
                                PremultipliedColorF::TRANSPARENT,
                                ClearMode::Transparent,
                                child_tasks,
                                None,
                                PictureType::Image,
                            );

                            self.render_task_id = Some(render_tasks.add(picture_task));
                        }
                    }
                    Some(PictureCompositeMode::Blit) => {
                        let picture_task = RenderTask::new_picture(
                            Some(prim_screen_rect.size),
                            prim_index,
                            RenderTargetKind::Color,
                            content_origin,
                            PremultipliedColorF::TRANSPARENT,
                            ClearMode::Transparent,
                            child_tasks,
                            None,
                            PictureType::Image,
                        );

                        self.render_task_id = Some(render_tasks.add(picture_task));
                    }
                    None => {
                        parent_tasks.extend(child_tasks);
                        self.render_task_id = None;
                    }
                }
            }
            PictureKind::TextShadow { blur_radius, color, content_rect, .. } => {
                // This is a shadow element. Create a render task that will
                // render the text run to a target, and then apply a gaussian
                // blur to that text run in order to build the actual primitive
                // which will be blitted to the framebuffer.

                // TODO(gw): Rounding the content rect here to device pixels is not
                // technically correct. Ideally we should ceil() here, and ensure that
                // the extra part pixel in the case of fractional sizes is correctly
                // handled. For now, just use rounding which passes the existing
                // Gecko tests.
                let cache_size = (content_rect.size * content_scale).round().to_i32();

                // Quote from https://drafts.csswg.org/css-backgrounds-3/#shadow-blur
                // "the image that would be generated by applying to the shadow a
                // Gaussian blur with a standard deviation equal to half the blur radius."
                let device_radius = (blur_radius * prim_context.device_pixel_scale.0).round();
                let blur_std_deviation = device_radius * 0.5;

                let picture_task = RenderTask::new_picture(
                    Some(cache_size),
                    prim_index,
                    RenderTargetKind::Color,
                    ContentOrigin::Local(content_rect.origin),
                    color.premultiplied(),
                    ClearMode::Transparent,
                    Vec::new(),
                    None,
                    PictureType::TextShadow,
                );

                let picture_task_id = render_tasks.add(picture_task);

                let render_task = RenderTask::new_blur(
                    blur_std_deviation,
                    picture_task_id,
                    render_tasks,
                    RenderTargetKind::Color,
                    &[],
                    ClearMode::Transparent,
                    color.premultiplied(),
                    None,
                );

                self.render_task_id = Some(render_tasks.add(render_task));
            }
            PictureKind::BoxShadow { blur_radius, clip_mode, ref blur_regions, color, content_rect, cache_key, .. } => {
                // TODO(gw): Rounding the content rect here to device pixels is not
                // technically correct. Ideally we should ceil() here, and ensure that
                // the extra part pixel in the case of fractional sizes is correctly
                // handled. For now, just use rounding which passes the existing
                // Gecko tests.
                let cache_size = (content_rect.size * content_scale).round().to_i32();

                // Quote from https://drafts.csswg.org/css-backgrounds-3/#shadow-blur
                // "the image that would be generated by applying to the shadow a
                // Gaussian blur with a standard deviation equal to half the blur radius."
                let device_radius = (blur_radius * prim_context.device_pixel_scale.0).round();
                let blur_std_deviation = device_radius * 0.5;

                let blur_clear_mode = match clip_mode {
                    BoxShadowClipMode::Outset => {
                        ClearMode::One
                    }
                    BoxShadowClipMode::Inset => {
                        ClearMode::Zero
                    }
                };

                let picture_task = RenderTask::new_picture(
                    Some(cache_size),
                    prim_index,
                    RenderTargetKind::Alpha,
                    ContentOrigin::Local(content_rect.origin),
                    color.premultiplied(),
                    ClearMode::Zero,
                    Vec::new(),
                    Some(cache_key),
                    PictureType::BoxShadow,
                );

                let picture_task_id = render_tasks.add(picture_task);

                let render_task = RenderTask::new_blur(
                    blur_std_deviation,
                    picture_task_id,
                    render_tasks,
                    RenderTargetKind::Alpha,
                    blur_regions,
                    blur_clear_mode,
                    color.premultiplied(),
                    Some(cache_key),
                );

                self.render_task_id = Some(render_tasks.add(render_task));
            }
        }

        if let Some(render_task_id) = self.render_task_id {
            parent_tasks.push(render_task_id);
        }
    }

    pub fn write_gpu_blocks(&self, _request: &mut GpuDataRequest) {
        // TODO(gw): We'll need to write the GPU blocks
        //           here specific to a brush primitive
        //           once we start drawing pictures as brushes!
    }

    pub fn target_kind(&self) -> RenderTargetKind {
        match self.kind {
            PictureKind::TextShadow { .. } => RenderTargetKind::Color,
            PictureKind::BoxShadow { .. } => RenderTargetKind::Alpha,
            PictureKind::Image { .. } => RenderTargetKind::Color,
        }
    }
}
