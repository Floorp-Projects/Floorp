/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{BorderRadiusKind, ColorF, ClipAndScrollInfo, device_length, DeviceIntSize};
use api::{BoxShadowClipMode, LayerPoint, LayerRect, LayerSize, Shadow};
use box_shadow::BLUR_SAMPLE_SCALE;
use frame_builder::PrimitiveContext;
use gpu_cache::GpuDataRequest;
use prim_store::{PrimitiveIndex, PrimitiveRun};
use render_task::{ClearMode, RenderTask, RenderTaskId, RenderTaskTree};
use tiling::RenderTargetKind;

/*
 A picture represents a dynamically rendered image. It consists of:

 * A number of primitives that are drawn onto the picture.
 * A composite operation describing how to composite this
   picture into its parent.
 * A configuration describing how to draw the primitives on
   this picture (e.g. in screen space or local space).
 */

#[derive(Debug)]
pub enum PictureKind {
    TextShadow {
        shadow: Shadow,
    },
    BoxShadow {
        blur_radius: f32,
        color: ColorF,
        blur_regions: Vec<LayerRect>,
        clip_mode: BoxShadowClipMode,
        radii_kind: BorderRadiusKind,
    },
}

#[derive(Debug)]
pub struct PicturePrimitive {
    pub prim_runs: Vec<PrimitiveRun>,
    pub render_task_id: Option<RenderTaskId>,
    pub kind: PictureKind,
    pub content_rect: LayerRect,

    // TODO(gw): Add a mode that specifies if this
    //           picture should be rasterized in
    //           screen-space or local-space.
}

impl PicturePrimitive {
    pub fn new_text_shadow(shadow: Shadow) -> PicturePrimitive {
        PicturePrimitive {
            prim_runs: Vec::new(),
            render_task_id: None,
            content_rect: LayerRect::zero(),
            kind: PictureKind::TextShadow {
                shadow,
            },
        }
    }

    pub fn new_box_shadow(
        blur_radius: f32,
        color: ColorF,
        blur_regions: Vec<LayerRect>,
        clip_mode: BoxShadowClipMode,
        radii_kind: BorderRadiusKind,
    ) -> PicturePrimitive {
        PicturePrimitive {
            prim_runs: Vec::new(),
            render_task_id: None,
            content_rect: LayerRect::zero(),
            kind: PictureKind::BoxShadow {
                blur_radius,
                color: color.premultiplied(),
                blur_regions,
                clip_mode,
                radii_kind,
            },
        }
    }

    pub fn as_text_shadow(&self) -> &Shadow {
        match self.kind {
            PictureKind::TextShadow { ref shadow } => shadow,
            PictureKind::BoxShadow { .. } => panic!("bug: not a text shadow")
        }
    }

    pub fn add_primitive(
        &mut self,
        prim_index: PrimitiveIndex,
        local_rect: &LayerRect,
        clip_and_scroll: ClipAndScrollInfo
    ) {
        // TODO(gw): Accumulating the primitive local rect
        //           into the content rect here is fine, for now.
        //           The only way pictures are currently used,
        //           all the items added to a picture are known
        //           to be in the same local space. Once we start
        //           using pictures for other uses, we will need
        //           to consider the space of a primitive in order
        //           to build a correct contect rect!
        self.content_rect = self.content_rect.union(local_rect);

        if let Some(ref mut run) = self.prim_runs.last_mut() {
            if run.clip_and_scroll == clip_and_scroll &&
               run.base_prim_index.0 + run.count == prim_index.0 {
                run.count += 1;
                return;
            }
        }

        self.prim_runs.push(PrimitiveRun {
            base_prim_index: prim_index,
            count: 1,
            clip_and_scroll,
        });
    }

    pub fn build(&mut self) -> LayerRect {
        match self.kind {
            PictureKind::TextShadow { ref shadow } => {
                let blur_offset = shadow.blur_radius * BLUR_SAMPLE_SCALE;

                self.content_rect = self.content_rect.inflate(
                    blur_offset,
                    blur_offset,
                );

                self.content_rect.translate(&shadow.offset)
            }
            PictureKind::BoxShadow { blur_radius, clip_mode, radii_kind, .. } => {
                // We need to inflate the content rect if outset.
                match clip_mode {
                    BoxShadowClipMode::Outset => {
                        let blur_offset = blur_radius * BLUR_SAMPLE_SCALE;

                        // If the radii are uniform, we can render just the top
                        // left corner and mirror it across the primitive. In
                        // this case, shift the content rect to leave room
                        // for the blur to take effect.
                        match radii_kind {
                            BorderRadiusKind::Uniform => {
                                let origin = LayerPoint::new(
                                    self.content_rect.origin.x - blur_offset,
                                    self.content_rect.origin.y - blur_offset,
                                );
                                let size = LayerSize::new(
                                    self.content_rect.size.width + blur_offset,
                                    self.content_rect.size.height + blur_offset,
                                );
                                self.content_rect = LayerRect::new(origin, size);
                            }
                            BorderRadiusKind::NonUniform => {
                                // For a non-uniform radii, we need to expand
                                // the content rect on all sides for the blur.
                                self.content_rect = self.content_rect.inflate(
                                    blur_offset,
                                    blur_offset,
                                );
                            }
                        }
                    }
                    BoxShadowClipMode::Inset => {}
                }

                self.content_rect
            }
        }
    }

    pub fn prepare_for_render(
        &mut self,
        prim_index: PrimitiveIndex,
        prim_context: &PrimitiveContext,
        render_tasks: &mut RenderTaskTree,
    ) {
        // This is a shadow element. Create a render task that will
        // render the text run to a target, and then apply a gaussian
        // blur to that text run in order to build the actual primitive
        // which will be blitted to the framebuffer.

        // TODO(gw): Rounding the content rect here to device pixels is not
        // technically correct. Ideally we should ceil() here, and ensure that
        // the extra part pixel in the case of fractional sizes is correctly
        // handled. For now, just use rounding which passes the existing
        // Gecko tests.
        let cache_width =
            (self.content_rect.size.width * prim_context.device_pixel_ratio).round() as i32;
        let cache_height =
            (self.content_rect.size.height * prim_context.device_pixel_ratio).round() as i32;
        let cache_size = DeviceIntSize::new(cache_width, cache_height);

        match self.kind {
            PictureKind::TextShadow { ref shadow } => {
                let blur_radius = device_length(shadow.blur_radius, prim_context.device_pixel_ratio);

                // Quote from https://drafts.csswg.org/css-backgrounds-3/#shadow-blur
                // "the image that would be generated by applying to the shadow a
                // Gaussian blur with a standard deviation equal to half the blur radius."
                let blur_std_deviation = blur_radius.0 as f32 * 0.5;

                let picture_task = RenderTask::new_picture(
                    cache_size,
                    prim_index,
                    RenderTargetKind::Color,
                    self.content_rect.origin,
                    shadow.color,
                    ClearMode::Transparent,
                );

                let picture_task_id = render_tasks.add(picture_task);

                let render_task = RenderTask::new_blur(
                    blur_std_deviation,
                    picture_task_id,
                    render_tasks,
                    RenderTargetKind::Color,
                    &[],
                    ClearMode::Transparent,
                    shadow.color,
                );

                self.render_task_id = Some(render_tasks.add(render_task));
            }
            PictureKind::BoxShadow { blur_radius, clip_mode, ref blur_regions, color, .. } => {
                let blur_radius = device_length(blur_radius, prim_context.device_pixel_ratio);

                // Quote from https://drafts.csswg.org/css-backgrounds-3/#shadow-blur
                // "the image that would be generated by applying to the shadow a
                // Gaussian blur with a standard deviation equal to half the blur radius."
                let blur_std_deviation = blur_radius.0 as f32 * 0.5;

                let blur_clear_mode = match clip_mode {
                    BoxShadowClipMode::Outset => {
                        ClearMode::One
                    }
                    BoxShadowClipMode::Inset => {
                        ClearMode::Zero
                    }
                };

                let picture_task = RenderTask::new_picture(
                    cache_size,
                    prim_index,
                    RenderTargetKind::Alpha,
                    self.content_rect.origin,
                    color,
                    ClearMode::Zero,
                );

                let picture_task_id = render_tasks.add(picture_task);

                let render_task = RenderTask::new_blur(
                    blur_std_deviation,
                    picture_task_id,
                    render_tasks,
                    RenderTargetKind::Alpha,
                    blur_regions,
                    blur_clear_mode,
                    color,
                );

                self.render_task_id = Some(render_tasks.add(render_task));
            }
        }
    }

    pub fn write_gpu_blocks(&self, mut _request: GpuDataRequest) {
        // TODO(gw): We'll need to write the GPU blocks
        //           here specific to a brush primitive
        //           once we start drawing pictures as brushes!
    }

    pub fn target_kind(&self) -> RenderTargetKind {
        match self.kind {
            PictureKind::TextShadow { .. } => RenderTargetKind::Color,
            PictureKind::BoxShadow { .. } => RenderTargetKind::Alpha,
        }
    }
}
