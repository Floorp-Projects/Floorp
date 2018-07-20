/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{DeviceRect, FilterOp, MixBlendMode, PipelineId, PremultipliedColorF};
use api::{DeviceIntRect, DeviceIntSize, DevicePoint, LayoutPoint, LayoutRect};
use api::{DevicePixelScale, PictureIntPoint, PictureIntRect, PictureIntSize};
use box_shadow::{BLUR_SAMPLE_SCALE};
use clip_scroll_tree::SpatialNodeIndex;
use frame_builder::{FrameBuildingContext, FrameBuildingState, PictureState, PrimitiveRunContext};
use gpu_cache::{GpuCacheHandle};
use gpu_types::UvRectKind;
use prim_store::{PrimitiveIndex, PrimitiveRun, PrimitiveRunLocalRect};
use prim_store::{PrimitiveMetadata, ScrollNodeAndClipChain, Transform};
use render_task::{ClearMode, RenderTask, RenderTaskCacheEntryHandle};
use render_task::{RenderTaskCacheKey, RenderTaskCacheKeyKind, RenderTaskId, RenderTaskLocation};
use scene::{FilterOpHelpers, SceneProperties};
use std::mem;
use tiling::RenderTargetKind;
use util::TransformedRectKind;

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

// Stores the location of the picture if it is drawn to
// an intermediate surface. This can be a render task if
// it is not persisted, or a texture cache item if the
// picture is cached in the texture cache.
#[derive(Debug)]
pub enum PictureSurface {
    RenderTask(RenderTaskId),
    TextureCache(RenderTaskCacheEntryHandle),
}

// A unique identifier for a Picture. Once we start
// doing deep compares of picture content, these
// may be the same across display lists, but that's
// not currently supported.
#[derive(Copy, Clone, Debug, Hash, PartialEq, Eq)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct PictureId(pub u64);

// Cache key that determines whether a pre-existing
// picture in the texture cache matches the content
// of the current picture.
#[derive(Clone, Debug, Hash, PartialEq, Eq)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct PictureCacheKey {
    // NOTE: We specifically want to ensure that we
    //       don't include the device space origin
    //       of this picture in the cache key, because
    //       we want the cache to remain valid as it
    //       is scrolled and/or translated by animation.
    //       This is valid while we have the restriction
    //       in place that only pictures that use the
    //       root coordinate system are cached - once
    //       we relax that, we'll need to consider some
    //       extra parameters, depending on transform.

    // This is a globally unique id of the scene this picture
    // is associated with, to avoid picture id collisions.
    scene_id: u64,

    // The unique (for the scene_id) identifier for this picture.
    // TODO(gw): Currently, these will not be
    //           shared across new display lists,
    //           so will only remain valid during
    //           scrolling. Next step will be to
    //           allow deep comparisons on pictures
    //           between display lists, allowing
    //           pictures that are the same to be
    //           cached across display lists!
    picture_id: PictureId,

    // Store the rect within the unclipped device
    // rect that we are actually rendering. This ensures
    // that if the 'clipped' rect changes, we will see
    // that the cache is invalid and re-draw the picture.
    // TODO(gw): To reduce the number of invalidations that
    //           happen as a cached picture scrolls off-screen,
    //           we could round up the size of the off-screen
    //           targets we draw (e.g. 512 pixels). This may
    //           also simplify other parts of the code that
    //           deal with clipped/unclipped rects, such as
    //           the code to inflate the device rect for blurs.
    pic_relative_render_rect: PictureIntRect,

    // Ensure that if the overall size of the picture
    // changes, the cache key will not match. This can
    // happen, for example, during zooming or changes
    // in device-pixel-ratio.
    unclipped_size: DeviceIntSize,
}

#[derive(Debug)]
pub struct PicturePrimitive {
    // If this picture is drawn to an intermediate surface,
    // the associated target information.
    pub surface: Option<PictureSurface>,

    // List of primitive runs that make up this picture.
    pub runs: Vec<PrimitiveRun>,

    // The pipeline that the primitives on this picture belong to.
    pub pipeline_id: PipelineId,

    // If true, apply the local clip rect to primitive drawn
    // in this picture.
    pub apply_local_clip_rect: bool,

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
    pub reference_frame_index: SpatialNodeIndex,
    pub real_local_rect: LayoutRect,
    // An optional cache handle for storing extra data
    // in the GPU cache, depending on the type of
    // picture.
    pub extra_gpu_data_handle: GpuCacheHandle,

    // Unique identifier for this picture.
    pub id: PictureId,
}

impl PicturePrimitive {
    pub fn resolve_scene_properties(&mut self, properties: &SceneProperties) -> bool {
        match self.composite_mode {
            Some(PictureCompositeMode::Filter(ref mut filter)) => {
                match *filter {
                    FilterOp::Opacity(ref binding, ref mut value) => {
                        *value = properties.resolve_float(binding);
                    }
                    _ => {}
                }

                filter.is_visible()
            }
            _ => true,
        }
    }

    pub fn new_image(
        id: PictureId,
        composite_mode: Option<PictureCompositeMode>,
        is_in_3d_context: bool,
        pipeline_id: PipelineId,
        reference_frame_index: SpatialNodeIndex,
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
            real_local_rect: LayoutRect::zero(),
            extra_gpu_data_handle: GpuCacheHandle::new(),
            apply_local_clip_rect,
            pipeline_id,
            id,
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
    ) -> LayoutRect {
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

                // TODO(gw): When we support culling rect being separate from
                //           the task/screen rect, we should include both the
                //           content and shadow rect here, which will prevent
                //           drop-shadows from disappearing if the main content
                //           rect is not visible. Something like:
                // let shadow_rect = local_content_rect
                //     .inflate(inflate_size, inflate_size)
                //     .translate(&offset);
                // shadow_rect.union(&local_content_rect)
            }
            _ => {
                local_content_rect
            }
        }
    }

    pub fn can_draw_directly_to_parent_surface(&self) -> bool {
        match self.composite_mode {
            Some(PictureCompositeMode::Filter(filter)) => {
                filter.is_noop()
            }
            Some(PictureCompositeMode::Blit) |
            Some(PictureCompositeMode::MixBlend(..)) => {
                false
            }
            None => {
                true
            }
        }
    }

    // Disallow subpixel AA if an intermediate surface is needed.
    pub fn allow_subpixel_aa(&self) -> bool {
        self.can_draw_directly_to_parent_surface()
    }

    pub fn prepare_for_render(
        &mut self,
        prim_index: PrimitiveIndex,
        prim_metadata: &mut PrimitiveMetadata,
        prim_run_context: &PrimitiveRunContext,
        mut pic_state_for_children: PictureState,
        pic_state: &mut PictureState,
        frame_context: &FrameBuildingContext,
        frame_state: &mut FrameBuildingState,
    ) {
        let prim_screen_rect = prim_metadata
                                .screen_rect
                                .as_ref()
                                .expect("bug: trying to draw an off-screen picture!?");
        if self.can_draw_directly_to_parent_surface() {
            pic_state.tasks.extend(pic_state_for_children.tasks);
            self.surface = None;
            return;
        }

        // TODO(gw): Almost all of the Picture types below use extra_gpu_cache_data
        //           to store the same type of data. The exception is the filter
        //           with a ColorMatrix, which stores the color matrix here. It's
        //           probably worth tidying this code up to be a bit more consistent.
        //           Perhaps store the color matrix after the common data, even though
        //           it's not used by that shader.
        match self.composite_mode {
            Some(PictureCompositeMode::Filter(FilterOp::Blur(blur_radius))) => {
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

                let uv_rect_kind = calculate_uv_rect_kind(
                    &prim_metadata.local_rect,
                    &prim_run_context.transform,
                    &device_rect,
                    frame_context.device_pixel_scale,
                );

                // If we are drawing a blur that has primitives or clips that contain
                // a complex coordinate system, don't bother caching them (for now).
                // It's likely that they are animating and caching may not help here
                // anyway. In the future we should relax this a bit, so that we can
                // cache tasks with complex coordinate systems if we detect the
                // relevant transforms haven't changed from frame to frame.
                let surface = if pic_state_for_children.has_non_root_coord_system {
                    let picture_task = RenderTask::new_picture(
                        RenderTaskLocation::Dynamic(None, Some(device_rect.size)),
                        prim_index,
                        device_rect.origin,
                        pic_state_for_children.tasks,
                        uv_rect_kind,
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

                    PictureSurface::RenderTask(render_task_id)
                } else {
                    // Get the relative clipped rect within the overall prim rect, that
                    // forms part of the cache key.
                    let pic_relative_render_rect = PictureIntRect::new(
                        PictureIntPoint::new(
                            device_rect.origin.x - prim_screen_rect.unclipped.origin.x,
                            device_rect.origin.y - prim_screen_rect.unclipped.origin.y,
                        ),
                        PictureIntSize::new(
                            device_rect.size.width,
                            device_rect.size.height,
                        ),
                    );

                    // Request a render task that will cache the output in the
                    // texture cache.
                    let cache_item = frame_state.resource_cache.request_render_task(
                        RenderTaskCacheKey {
                            size: device_rect.size,
                            kind: RenderTaskCacheKeyKind::Picture(PictureCacheKey {
                                scene_id: frame_context.scene_id,
                                picture_id: self.id,
                                unclipped_size: prim_screen_rect.unclipped.size,
                                pic_relative_render_rect,
                            }),
                        },
                        frame_state.gpu_cache,
                        frame_state.render_tasks,
                        None,
                        false,
                        |render_tasks| {
                            let child_tasks = mem::replace(&mut pic_state_for_children.tasks, Vec::new());

                            let picture_task = RenderTask::new_picture(
                                RenderTaskLocation::Dynamic(None, Some(device_rect.size)),
                                prim_index,
                                device_rect.origin,
                                child_tasks,
                                uv_rect_kind,
                            );

                            let picture_task_id = render_tasks.add(picture_task);

                            let blur_render_task = RenderTask::new_blur(
                                blur_std_deviation,
                                picture_task_id,
                                render_tasks,
                                RenderTargetKind::Color,
                                ClearMode::Transparent,
                            );

                            let render_task_id = render_tasks.add(blur_render_task);

                            pic_state.tasks.push(render_task_id);

                            render_task_id
                        }
                    );

                    PictureSurface::TextureCache(cache_item)
                };

                self.surface = Some(surface);
            }
            Some(PictureCompositeMode::Filter(FilterOp::DropShadow(offset, blur_radius, color))) => {
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

                let uv_rect_kind = calculate_uv_rect_kind(
                    &prim_metadata.local_rect,
                    &prim_run_context.transform,
                    &device_rect,
                    frame_context.device_pixel_scale,
                );

                let mut picture_task = RenderTask::new_picture(
                    RenderTaskLocation::Dynamic(None, Some(device_rect.size)),
                    prim_index,
                    device_rect.origin,
                    pic_state_for_children.tasks,
                    uv_rect_kind,
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
                self.surface = Some(PictureSurface::RenderTask(render_task_id));

                // If the local rect of the contents changed, force the cache handle
                // to be invalidated so that the primitive data below will get
                // uploaded to the GPU this frame. This can occur during property
                // animation.
                if pic_state.local_rect_changed {
                    frame_state.gpu_cache.invalidate(&mut self.extra_gpu_data_handle);
                }

                if let Some(mut request) = frame_state.gpu_cache.request(&mut self.extra_gpu_data_handle) {
                    // TODO(gw): This is very hacky code below! It stores an extra
                    //           brush primitive below for the special case of a
                    //           drop-shadow where we need a different local
                    //           rect for the shadow. To tidy this up in future,
                    //           we could consider abstracting the code in prim_store.rs
                    //           that writes a brush primitive header.

                    // Basic brush primitive header is (see end of prepare_prim_for_render_inner in prim_store.rs)
                    //  [brush specific data]
                    //  [segment_rect, segment data]
                    let shadow_rect = prim_metadata.local_rect.translate(&offset);

                    // ImageBrush colors
                    request.push(color.premultiplied());
                    request.push(PremultipliedColorF::WHITE);
                    request.push([
                        prim_metadata.local_rect.size.width,
                        prim_metadata.local_rect.size.height,
                        0.0,
                        0.0,
                    ]);

                    // segment rect / extra data
                    request.push(shadow_rect);
                    request.push([0.0, 0.0, 0.0, 0.0]);
                }
            }
            Some(PictureCompositeMode::MixBlend(..)) => {
                let uv_rect_kind = calculate_uv_rect_kind(
                    &prim_metadata.local_rect,
                    &prim_run_context.transform,
                    &prim_screen_rect.clipped,
                    frame_context.device_pixel_scale,
                );

                let picture_task = RenderTask::new_picture(
                    RenderTaskLocation::Dynamic(None, Some(prim_screen_rect.clipped.size)),
                    prim_index,
                    prim_screen_rect.clipped.origin,
                    pic_state_for_children.tasks,
                    uv_rect_kind,
                );

                let readback_task_id = frame_state.render_tasks.add(
                    RenderTask::new_readback(prim_screen_rect.clipped)
                );

                self.secondary_render_task_id = Some(readback_task_id);
                pic_state.tasks.push(readback_task_id);

                let render_task_id = frame_state.render_tasks.add(picture_task);
                pic_state.tasks.push(render_task_id);
                self.surface = Some(PictureSurface::RenderTask(render_task_id));
            }
            Some(PictureCompositeMode::Filter(filter)) => {
                if let FilterOp::ColorMatrix(m) = filter {
                    if let Some(mut request) = frame_state.gpu_cache.request(&mut self.extra_gpu_data_handle) {
                        for i in 0..5 {
                            request.push([m[i*4], m[i*4+1], m[i*4+2], m[i*4+3]]);
                        }
                    }
                }

                let uv_rect_kind = calculate_uv_rect_kind(
                    &prim_metadata.local_rect,
                    &prim_run_context.transform,
                    &prim_screen_rect.clipped,
                    frame_context.device_pixel_scale,
                );

                let picture_task = RenderTask::new_picture(
                    RenderTaskLocation::Dynamic(None, Some(prim_screen_rect.clipped.size)),
                    prim_index,
                    prim_screen_rect.clipped.origin,
                    pic_state_for_children.tasks,
                    uv_rect_kind,
                );

                let render_task_id = frame_state.render_tasks.add(picture_task);
                pic_state.tasks.push(render_task_id);
                self.surface = Some(PictureSurface::RenderTask(render_task_id));
            }
            Some(PictureCompositeMode::Blit) | None => {
                let uv_rect_kind = calculate_uv_rect_kind(
                    &prim_metadata.local_rect,
                    &prim_run_context.transform,
                    &prim_screen_rect.clipped,
                    frame_context.device_pixel_scale,
                );

                let picture_task = RenderTask::new_picture(
                    RenderTaskLocation::Dynamic(None, Some(prim_screen_rect.clipped.size)),
                    prim_index,
                    prim_screen_rect.clipped.origin,
                    pic_state_for_children.tasks,
                    uv_rect_kind,
                );

                let render_task_id = frame_state.render_tasks.add(picture_task);
                pic_state.tasks.push(render_task_id);
                self.surface = Some(PictureSurface::RenderTask(render_task_id));
            }
        }
    }
}

// Calculate a single screen-space UV for a picture.
fn calculate_screen_uv(
    local_pos: &LayoutPoint,
    transform: &Transform,
    rendered_rect: &DeviceRect,
    device_pixel_scale: DevicePixelScale,
) -> DevicePoint {
    let world_pos = match transform.m.transform_point2d(local_pos) {
        Some(pos) => pos,
        None => {
            //Warning: this is incorrect and needs to be fixed properly.
            // The transformation has put a local vertex behind the near clipping plane...
            // Proper solution would be to keep the near-clipping-plane results around
            // (currently produced by calculate_screen_bounding_rect) and use them here.
            return DevicePoint::new(0.5, 0.5);
        }
    };

    let mut device_pos = world_pos * device_pixel_scale;

    // Apply snapping for axis-aligned scroll nodes, as per prim_shared.glsl.
    if transform.transform_kind == TransformedRectKind::AxisAligned {
        device_pos.x = (device_pos.x + 0.5).floor();
        device_pos.y = (device_pos.y + 0.5).floor();
    }

    DevicePoint::new(
        (device_pos.x - rendered_rect.origin.x) / rendered_rect.size.width,
        (device_pos.y - rendered_rect.origin.y) / rendered_rect.size.height,
    )
}

// Calculate a UV rect within an image based on the screen space
// vertex positions of a picture.
fn calculate_uv_rect_kind(
    local_rect: &LayoutRect,
    transform: &Transform,
    rendered_rect: &DeviceIntRect,
    device_pixel_scale: DevicePixelScale,
) -> UvRectKind {
    let rendered_rect = rendered_rect.to_f32();

    let top_left = calculate_screen_uv(
        &local_rect.origin,
        transform,
        &rendered_rect,
        device_pixel_scale,
    );

    let top_right = calculate_screen_uv(
        &local_rect.top_right(),
        transform,
        &rendered_rect,
        device_pixel_scale,
    );

    let bottom_left = calculate_screen_uv(
        &local_rect.bottom_left(),
        transform,
        &rendered_rect,
        device_pixel_scale,
    );

    let bottom_right = calculate_screen_uv(
        &local_rect.bottom_right(),
        transform,
        &rendered_rect,
        device_pixel_scale,
    );

    UvRectKind::Quad {
        top_left,
        top_right,
        bottom_left,
        bottom_right,
    }
}
