/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! # Prepare pass
//!
//! TODO: document this!

use std::cmp;
use api::{PremultipliedColorF, PropertyBinding};
use api::{BoxShadowClipMode, BorderStyle, ClipMode};
use api::units::*;
use euclid::Scale;
use smallvec::SmallVec;
use crate::image_tiling::{self, Repetition};
use crate::border::{get_max_scale_for_border, build_border_instances};
use crate::clip::{ClipStore};
use crate::spatial_tree::{SpatialNodeIndex, SpatialTree};
use crate::clip::{ClipDataStore, ClipNodeFlags, ClipChainInstance, ClipItemKind};
use crate::frame_builder::{FrameBuildingContext, FrameBuildingState, PictureContext, PictureState};
use crate::gpu_cache::{GpuCacheHandle, GpuDataRequest};
use crate::gpu_types::{BrushFlags};
use crate::internal_types::{FastHashMap, PlaneSplitAnchor};
use crate::picture::{PicturePrimitive, SliceId, ClusterFlags};
use crate::picture::{PrimitiveList, PrimitiveCluster, SurfaceIndex, TileCacheInstance, SubpixelMode, Picture3DContext};
use crate::prim_store::line_dec::MAX_LINE_DECORATION_RESOLUTION;
use crate::prim_store::*;
use crate::render_backend::DataStores;
use crate::render_task_graph::RenderTaskId;
use crate::render_task_cache::RenderTaskCacheKeyKind;
use crate::render_task_cache::{RenderTaskCacheKey, to_cache_size, RenderTaskParent};
use crate::render_task::{RenderTaskKind, RenderTask};
use crate::segment::SegmentBuilder;
use crate::util::{clamp_to_scale_factor, pack_as_float};
use crate::visibility::{compute_conservative_visible_rect, PrimitiveVisibility, VisibilityState};


const MAX_MASK_SIZE: f32 = 4096.0;

const MIN_BRUSH_SPLIT_AREA: f32 = 128.0 * 128.0;


pub fn prepare_primitives(
    store: &mut PrimitiveStore,
    prim_list: &mut PrimitiveList,
    pic_context: &PictureContext,
    pic_state: &mut PictureState,
    frame_context: &FrameBuildingContext,
    frame_state: &mut FrameBuildingState,
    data_stores: &mut DataStores,
    scratch: &mut PrimitiveScratchBuffer,
    tile_caches: &mut FastHashMap<SliceId, Box<TileCacheInstance>>,
    prim_instances: &mut Vec<PrimitiveInstance>,
) {
    profile_scope!("prepare_primitives");
    for cluster in &mut prim_list.clusters {
        if !cluster.flags.contains(ClusterFlags::IS_VISIBLE) {
            continue;
        }
        profile_scope!("cluster");
        pic_state.map_local_to_pic.set_target_spatial_node(
            cluster.spatial_node_index,
            frame_context.spatial_tree,
        );

        for prim_instance_index in cluster.prim_range() {
            if frame_state.surface_builder.is_prim_visible_and_in_dirty_region(&prim_instances[prim_instance_index].vis) {

                let plane_split_anchor = PlaneSplitAnchor::new(
                    cluster.spatial_node_index,
                    PrimitiveInstanceIndex(prim_instance_index as u32),
                );

                if prepare_prim_for_render(
                    store,
                    prim_instance_index,
                    cluster,
                    pic_context,
                    pic_state,
                    frame_context,
                    frame_state,
                    plane_split_anchor,
                    data_stores,
                    scratch,
                    tile_caches,
                    prim_instances,
                ) {
                    // First check for coarse visibility (if this primitive was completely off-screen)
                    let prim_instance = &mut prim_instances[prim_instance_index];

                    frame_state.surface_builder.push_prim(
                        PrimitiveInstanceIndex(prim_instance_index as u32),
                        cluster.spatial_node_index,
                        &prim_instance.vis,
                        None,
                        frame_state.cmd_buffers,
                    );

                    frame_state.num_visible_primitives += 1;
                    continue;
                }
            }

            // TODO(gw): Technically no need to clear visibility here, since from this point it
            //           only matters if it got added to a command buffer. Kept here for now to
            //           make debugging simpler, but perhaps we can remove / tidy this up.
            prim_instances[prim_instance_index].clear_visibility();
        }
    }
}

fn prepare_prim_for_render(
    store: &mut PrimitiveStore,
    prim_instance_index: usize,
    cluster: &mut PrimitiveCluster,
    pic_context: &PictureContext,
    pic_state: &mut PictureState,
    frame_context: &FrameBuildingContext,
    frame_state: &mut FrameBuildingState,
    plane_split_anchor: PlaneSplitAnchor,
    data_stores: &mut DataStores,
    scratch: &mut PrimitiveScratchBuffer,
    tile_caches: &mut FastHashMap<SliceId, Box<TileCacheInstance>>,
    prim_instances: &mut Vec<PrimitiveInstance>,
) -> bool {
    profile_scope!("prepare_prim_for_render");

    // If we have dependencies, we need to prepare them first, in order
    // to know the actual rect of this primitive.
    // For example, scrolling may affect the location of an item in
    // local space, which may force us to render this item on a larger
    // picture target, if being composited.
    let mut is_passthrough = false;
    if let PrimitiveInstanceKind::Picture { pic_index, .. } = prim_instances[prim_instance_index].kind {
        let pic = &mut store.pictures[pic_index.0];

        // TODO(gw): Plan to remove pictures with no composite mode, so that we don't need
        //           to special case for pass through pictures.
        is_passthrough = pic.composite_mode.is_none();

        match pic.take_context(
            pic_index,
            Some(pic_context.surface_index),
            pic_context.subpixel_mode,
            frame_state,
            frame_context,
            scratch,
            tile_caches,
        ) {
            Some((pic_context_for_children, mut pic_state_for_children, mut prim_list)) => {
                prepare_primitives(
                    store,
                    &mut prim_list,
                    &pic_context_for_children,
                    &mut pic_state_for_children,
                    frame_context,
                    frame_state,
                    data_stores,
                    scratch,
                    tile_caches,
                    prim_instances,
                );

                // Restore the dependencies (borrow check dance)
                store.pictures[pic_context_for_children.pic_index.0]
                    .restore_context(
                        pic_context_for_children.pic_index,
                        prim_list,
                        pic_context_for_children,
                        prim_instances,
                        frame_context,
                        frame_state,
                    );
            }
            None => {
                return false;
            }
        }
    }

    let prim_instance = &mut prim_instances[prim_instance_index];

    if !is_passthrough {
        let prim_rect = data_stores.get_local_prim_rect(
            prim_instance,
            &store.pictures,
            frame_state.surfaces,
        );

        if !update_clip_task(
            prim_instance,
            &prim_rect.min,
            cluster.spatial_node_index,
            pic_context.raster_spatial_node_index,
            pic_context,
            pic_state,
            frame_context,
            frame_state,
            store,
            data_stores,
            scratch,
        ) {
            return false;
        }
    }

    prepare_interned_prim_for_render(
        store,
        prim_instance,
        cluster,
        plane_split_anchor,
        pic_context,
        frame_context,
        frame_state,
        data_stores,
        scratch,
    );

    true
}

/// Prepare an interned primitive for rendering, by requesting
/// resources, render tasks etc. This is equivalent to the
/// prepare_prim_for_render_inner call for old style primitives.
fn prepare_interned_prim_for_render(
    store: &mut PrimitiveStore,
    prim_instance: &mut PrimitiveInstance,
    cluster: &mut PrimitiveCluster,
    plane_split_anchor: PlaneSplitAnchor,
    pic_context: &PictureContext,
    frame_context: &FrameBuildingContext,
    frame_state: &mut FrameBuildingState,
    data_stores: &mut DataStores,
    scratch: &mut PrimitiveScratchBuffer,
) {
    let prim_spatial_node_index = cluster.spatial_node_index;
    let device_pixel_scale = frame_state.surfaces[pic_context.surface_index.0].device_pixel_scale;

    match &mut prim_instance.kind {
        PrimitiveInstanceKind::LineDecoration { data_handle, ref mut render_task, .. } => {
            profile_scope!("LineDecoration");
            let prim_data = &mut data_stores.line_decoration[*data_handle];
            let common_data = &mut prim_data.common;
            let line_dec_data = &mut prim_data.kind;

            // Update the template this instane references, which may refresh the GPU
            // cache with any shared template data.
            line_dec_data.update(common_data, frame_state);

            // Work out the device pixel size to be used to cache this line decoration.

            // If we have a cache key, it's a wavy / dashed / dotted line. Otherwise, it's
            // a simple solid line.
            if let Some(cache_key) = line_dec_data.cache_key.as_ref() {
                // TODO(gw): These scale factors don't do a great job if the world transform
                //           contains perspective
                let scale = frame_context
                    .spatial_tree
                    .get_world_transform(prim_spatial_node_index)
                    .scale_factors();

                // Scale factors are normalized to a power of 2 to reduce the number of
                // resolution changes.
                // For frames with a changing scale transform round scale factors up to
                // nearest power-of-2 boundary so that we don't keep having to redraw
                // the content as it scales up and down. Rounding up to nearest
                // power-of-2 boundary ensures we never scale up, only down --- avoiding
                // jaggies. It also ensures we never scale down by more than a factor of
                // 2, avoiding bad downscaling quality.
                let scale_width = clamp_to_scale_factor(scale.0, false);
                let scale_height = clamp_to_scale_factor(scale.1, false);
                // Pick the maximum dimension as scale
                let world_scale = LayoutToWorldScale::new(scale_width.max(scale_height));

                let scale_factor = world_scale * Scale::new(1.0);
                let mut task_size = (LayoutSize::from_au(cache_key.size) * scale_factor).ceil().to_i32();
                if task_size.width > MAX_LINE_DECORATION_RESOLUTION as i32 ||
                   task_size.height > MAX_LINE_DECORATION_RESOLUTION as i32 {
                     let max_extent = cmp::max(task_size.width, task_size.height);
                     let task_scale_factor = Scale::new(MAX_LINE_DECORATION_RESOLUTION as f32 / max_extent as f32);
                     task_size = (LayoutSize::from_au(cache_key.size) * scale_factor * task_scale_factor)
                                    .ceil().to_i32();
                }

                // Request a pre-rendered image task.
                // TODO(gw): This match is a bit untidy, but it should disappear completely
                //           once the prepare_prims and batching are unified. When that
                //           happens, we can use the cache handle immediately, and not need
                //           to temporarily store it in the primitive instance.
                *render_task = Some(frame_state.resource_cache.request_render_task(
                    RenderTaskCacheKey {
                        size: task_size,
                        kind: RenderTaskCacheKeyKind::LineDecoration(cache_key.clone()),
                    },
                    frame_state.gpu_cache,
                    frame_state.rg_builder,
                    None,
                    false,
                    RenderTaskParent::Surface(pic_context.surface_index),
                    &mut frame_state.surface_builder,
                    |rg_builder| {
                        rg_builder.add().init(RenderTask::new_dynamic(
                            task_size,
                            RenderTaskKind::new_line_decoration(
                                cache_key.style,
                                cache_key.orientation,
                                cache_key.wavy_line_thickness.to_f32_px(),
                                LayoutSize::from_au(cache_key.size),
                            ),
                        ))
                    }
                ));
            }
        }
        PrimitiveInstanceKind::TextRun { run_index, data_handle, .. } => {
            profile_scope!("TextRun");
            let prim_data = &mut data_stores.text_run[*data_handle];
            let run = &mut store.text_runs[*run_index];

            prim_data.common.may_need_repetition = false;

            // The glyph transform has to match `glyph_transform` in "ps_text_run" shader.
            // It's relative to the rasterizing space of a glyph.
            let transform = frame_context.spatial_tree
                .get_relative_transform(
                    prim_spatial_node_index,
                    pic_context.raster_spatial_node_index,
                )
                .into_fast_transform();
            let prim_offset = prim_data.common.prim_rect.min.to_vector() - run.reference_frame_relative_offset;

            let surface = &frame_state.surfaces[pic_context.surface_index.0];

            // If subpixel AA is disabled due to the backing surface the glyphs
            // are being drawn onto, disable it (unless we are using the
            // specifial subpixel mode that estimates background color).
            let allow_subpixel = match prim_instance.vis.state {
                VisibilityState::Culled |
                VisibilityState::Unset |
                VisibilityState::PassThrough => {
                    panic!("bug: invalid visibility state");
                }
                VisibilityState::Visible { sub_slice_index, .. } => {
                    // For now, we only allow subpixel AA on primary sub-slices. In future we
                    // may support other sub-slices if we find content that does this.
                    if sub_slice_index.is_primary() {
                        match pic_context.subpixel_mode {
                            SubpixelMode::Allow => true,
                            SubpixelMode::Deny => false,
                            SubpixelMode::Conditional { allowed_rect } => {
                                // Conditional mode allows subpixel AA to be enabled for this
                                // text run, so long as it's inside the allowed rect.
                                allowed_rect.contains_box(&prim_instance.vis.clip_chain.pic_coverage_rect)
                            }
                        }
                    } else {
                        false
                    }
                }
            };

            run.request_resources(
                prim_offset,
                &prim_data.font,
                &prim_data.glyphs,
                &transform.to_transform().with_destination::<_>(),
                surface,
                prim_spatial_node_index,
                allow_subpixel,
                frame_context.fb_config.low_quality_pinch_zoom,
                frame_state.resource_cache,
                frame_state.gpu_cache,
                frame_context.spatial_tree,
                scratch,
            );

            // Update the template this instane references, which may refresh the GPU
            // cache with any shared template data.
            prim_data.update(frame_state);
        }
        PrimitiveInstanceKind::Clear { data_handle, .. } => {
            profile_scope!("Clear");
            let prim_data = &mut data_stores.prim[*data_handle];

            prim_data.common.may_need_repetition = false;

            // Update the template this instane references, which may refresh the GPU
            // cache with any shared template data.
            prim_data.update(frame_state, frame_context.scene_properties);
        }
        PrimitiveInstanceKind::NormalBorder { data_handle, ref mut render_task_ids, .. } => {
            profile_scope!("NormalBorder");
            let prim_data = &mut data_stores.normal_border[*data_handle];
            let common_data = &mut prim_data.common;
            let border_data = &mut prim_data.kind;

            common_data.may_need_repetition =
                matches!(border_data.border.top.style, BorderStyle::Dotted | BorderStyle::Dashed) ||
                matches!(border_data.border.right.style, BorderStyle::Dotted | BorderStyle::Dashed) ||
                matches!(border_data.border.bottom.style, BorderStyle::Dotted | BorderStyle::Dashed) ||
                matches!(border_data.border.left.style, BorderStyle::Dotted | BorderStyle::Dashed);


            // Update the template this instance references, which may refresh the GPU
            // cache with any shared template data.
            border_data.update(common_data, frame_state);

            // TODO(gw): For now, the scale factors to rasterize borders at are
            //           based on the true world transform of the primitive. When
            //           raster roots with local scale are supported in future,
            //           that will need to be accounted for here.
            let scale = frame_context
                .spatial_tree
                .get_world_transform(prim_spatial_node_index)
                .scale_factors();

            // Scale factors are normalized to a power of 2 to reduce the number of
            // resolution changes.
            // For frames with a changing scale transform round scale factors up to
            // nearest power-of-2 boundary so that we don't keep having to redraw
            // the content as it scales up and down. Rounding up to nearest
            // power-of-2 boundary ensures we never scale up, only down --- avoiding
            // jaggies. It also ensures we never scale down by more than a factor of
            // 2, avoiding bad downscaling quality.
            let scale_width = clamp_to_scale_factor(scale.0, false);
            let scale_height = clamp_to_scale_factor(scale.1, false);
            // Pick the maximum dimension as scale
            let world_scale = LayoutToWorldScale::new(scale_width.max(scale_height));
            let mut scale = world_scale * device_pixel_scale;
            let max_scale = get_max_scale_for_border(border_data);
            scale.0 = scale.0.min(max_scale.0);

            // For each edge and corner, request the render task by content key
            // from the render task cache. This ensures that the render task for
            // this segment will be available for batching later in the frame.
            let mut handles: SmallVec<[RenderTaskId; 8]> = SmallVec::new();

            for segment in &border_data.border_segments {
                // Update the cache key device size based on requested scale.
                let cache_size = to_cache_size(segment.local_task_size, &mut scale);
                let cache_key = RenderTaskCacheKey {
                    kind: RenderTaskCacheKeyKind::BorderSegment(segment.cache_key.clone()),
                    size: cache_size,
                };

                handles.push(frame_state.resource_cache.request_render_task(
                    cache_key,
                    frame_state.gpu_cache,
                    frame_state.rg_builder,
                    None,
                    false,          // TODO(gw): We don't calculate opacity for borders yet!
                    RenderTaskParent::Surface(pic_context.surface_index),
                    &mut frame_state.surface_builder,
                    |rg_builder| {
                        rg_builder.add().init(RenderTask::new_dynamic(
                            cache_size,
                            RenderTaskKind::new_border_segment(
                                build_border_instances(
                                    &segment.cache_key,
                                    cache_size,
                                    &border_data.border,
                                    scale,
                                )
                            ),
                        ))
                    }
                ));
            }

            *render_task_ids = scratch
                .border_cache_handles
                .extend(handles);
        }
        PrimitiveInstanceKind::ImageBorder { data_handle, .. } => {
            profile_scope!("ImageBorder");
            let prim_data = &mut data_stores.image_border[*data_handle];

            // TODO: get access to the ninepatch and to check whether we need support
            // for repetitions in the shader.

            // Update the template this instance references, which may refresh the GPU
            // cache with any shared template data.
            prim_data.kind.update(
                &mut prim_data.common,
                frame_state
            );
        }
        PrimitiveInstanceKind::Rectangle { data_handle, segment_instance_index, color_binding_index, .. } => {
            profile_scope!("Rectangle");
            let prim_data = &mut data_stores.prim[*data_handle];
            prim_data.common.may_need_repetition = false;

            if *color_binding_index != ColorBindingIndex::INVALID {
                match store.color_bindings[*color_binding_index] {
                    PropertyBinding::Binding(..) => {
                        // We explicitly invalidate the gpu cache
                        // if the color is animating.
                        let gpu_cache_handle =
                            if *segment_instance_index == SegmentInstanceIndex::INVALID {
                                None
                            } else if *segment_instance_index == SegmentInstanceIndex::UNUSED {
                                Some(&prim_data.common.gpu_cache_handle)
                            } else {
                                Some(&scratch.segment_instances[*segment_instance_index].gpu_cache_handle)
                            };
                        if let Some(gpu_cache_handle) = gpu_cache_handle {
                            frame_state.gpu_cache.invalidate(gpu_cache_handle);
                        }
                    }
                    PropertyBinding::Value(..) => {},
                }
            }

            // Update the template this instane references, which may refresh the GPU
            // cache with any shared template data.
            prim_data.update(
                frame_state,
                frame_context.scene_properties,
            );

            write_segment(
                *segment_instance_index,
                frame_state,
                &mut scratch.segments,
                &mut scratch.segment_instances,
                |request| {
                    prim_data.kind.write_prim_gpu_blocks(
                        request,
                        frame_context.scene_properties,
                    );
                }
            );
        }
        PrimitiveInstanceKind::YuvImage { data_handle, segment_instance_index, .. } => {
            profile_scope!("YuvImage");
            let prim_data = &mut data_stores.yuv_image[*data_handle];
            let common_data = &mut prim_data.common;
            let yuv_image_data = &mut prim_data.kind;

            common_data.may_need_repetition = false;

            // Update the template this instane references, which may refresh the GPU
            // cache with any shared template data.
            yuv_image_data.update(common_data, frame_state);

            write_segment(
                *segment_instance_index,
                frame_state,
                &mut scratch.segments,
                &mut scratch.segment_instances,
                |request| {
                    yuv_image_data.write_prim_gpu_blocks(request);
                }
            );
        }
        PrimitiveInstanceKind::Image { data_handle, image_instance_index, .. } => {
            profile_scope!("Image");

            let prim_data = &mut data_stores.image[*data_handle];
            let common_data = &mut prim_data.common;
            let image_data = &mut prim_data.kind;
            let image_instance = &mut store.images[*image_instance_index];

            // Update the template this instance references, which may refresh the GPU
            // cache with any shared template data.
            image_data.update(
                common_data,
                image_instance,
                pic_context.surface_index,
                prim_spatial_node_index,
                frame_state,
                frame_context,
                &mut prim_instance.vis,
            );

            write_segment(
                image_instance.segment_instance_index,
                frame_state,
                &mut scratch.segments,
                &mut scratch.segment_instances,
                |request| {
                    image_data.write_prim_gpu_blocks(request);
                },
            );
        }
        PrimitiveInstanceKind::LinearGradient { data_handle, ref mut visible_tiles_range, .. } => {
            profile_scope!("LinearGradient");
            let prim_data = &mut data_stores.linear_grad[*data_handle];

            // Update the template this instane references, which may refresh the GPU
            // cache with any shared template data.
            prim_data.update(frame_state, pic_context.surface_index);

            if prim_data.stretch_size.width >= prim_data.common.prim_rect.width() &&
                prim_data.stretch_size.height >= prim_data.common.prim_rect.height() {

                prim_data.common.may_need_repetition = false;
            }

            if prim_data.tile_spacing != LayoutSize::zero() {
                // We are performing the decomposition on the CPU here, no need to
                // have it in the shader.
                prim_data.common.may_need_repetition = false;

                *visible_tiles_range = decompose_repeated_gradient(
                    &prim_instance.vis,
                    &prim_data.common.prim_rect,
                    prim_spatial_node_index,
                    &prim_data.stretch_size,
                    &prim_data.tile_spacing,
                    frame_state,
                    &mut scratch.gradient_tiles,
                    &frame_context.spatial_tree,
                    Some(&mut |_, mut request| {
                        request.push([
                            prim_data.start_point.x,
                            prim_data.start_point.y,
                            prim_data.end_point.x,
                            prim_data.end_point.y,
                        ]);
                        request.push([
                            pack_as_float(prim_data.extend_mode as u32),
                            prim_data.stretch_size.width,
                            prim_data.stretch_size.height,
                            0.0,
                        ]);
                    }),
                );

                if visible_tiles_range.is_empty() {
                    prim_instance.clear_visibility();
                }
            }

            // TODO(gw): Consider whether it's worth doing segment building
            //           for gradient primitives.
        }
        PrimitiveInstanceKind::CachedLinearGradient { data_handle, ref mut visible_tiles_range, .. } => {
            profile_scope!("CachedLinearGradient");
            let prim_data = &mut data_stores.linear_grad[*data_handle];
            prim_data.common.may_need_repetition = prim_data.stretch_size.width < prim_data.common.prim_rect.width()
                || prim_data.stretch_size.height < prim_data.common.prim_rect.height();

            // Update the template this instance references, which may refresh the GPU
            // cache with any shared template data.
            prim_data.update(frame_state, pic_context.surface_index);

            if prim_data.tile_spacing != LayoutSize::zero() {
                prim_data.common.may_need_repetition = false;

                *visible_tiles_range = decompose_repeated_gradient(
                    &prim_instance.vis,
                    &prim_data.common.prim_rect,
                    prim_spatial_node_index,
                    &prim_data.stretch_size,
                    &prim_data.tile_spacing,
                    frame_state,
                    &mut scratch.gradient_tiles,
                    &frame_context.spatial_tree,
                    None,
                );

                if visible_tiles_range.is_empty() {
                    prim_instance.clear_visibility();
                }
            }
        }
        PrimitiveInstanceKind::RadialGradient { data_handle, ref mut visible_tiles_range, .. } => {
            profile_scope!("RadialGradient");
            let prim_data = &mut data_stores.radial_grad[*data_handle];

            prim_data.common.may_need_repetition = prim_data.stretch_size.width < prim_data.common.prim_rect.width()
                || prim_data.stretch_size.height < prim_data.common.prim_rect.height();

            // Update the template this instane references, which may refresh the GPU
            // cache with any shared template data.
            prim_data.update(frame_state, pic_context.surface_index);

            if prim_data.tile_spacing != LayoutSize::zero() {
                prim_data.common.may_need_repetition = false;

                *visible_tiles_range = decompose_repeated_gradient(
                    &prim_instance.vis,
                    &prim_data.common.prim_rect,
                    prim_spatial_node_index,
                    &prim_data.stretch_size,
                    &prim_data.tile_spacing,
                    frame_state,
                    &mut scratch.gradient_tiles,
                    &frame_context.spatial_tree,
                    None,
                );

                if visible_tiles_range.is_empty() {
                    prim_instance.clear_visibility();
                }
            }

            // TODO(gw): Consider whether it's worth doing segment building
            //           for gradient primitives.
        }
        PrimitiveInstanceKind::ConicGradient { data_handle, ref mut visible_tiles_range, .. } => {
            profile_scope!("ConicGradient");
            let prim_data = &mut data_stores.conic_grad[*data_handle];

            prim_data.common.may_need_repetition = prim_data.stretch_size.width < prim_data.common.prim_rect.width()
                || prim_data.stretch_size.height < prim_data.common.prim_rect.height();

            // Update the template this instane references, which may refresh the GPU
            // cache with any shared template data.
            prim_data.update(frame_state, pic_context.surface_index);

            if prim_data.tile_spacing != LayoutSize::zero() {
                prim_data.common.may_need_repetition = false;

                *visible_tiles_range = decompose_repeated_gradient(
                    &prim_instance.vis,
                    &prim_data.common.prim_rect,
                    prim_spatial_node_index,
                    &prim_data.stretch_size,
                    &prim_data.tile_spacing,
                    frame_state,
                    &mut scratch.gradient_tiles,
                    &frame_context.spatial_tree,
                    None,
                );

                if visible_tiles_range.is_empty() {
                    prim_instance.clear_visibility();
                }
            }

            // TODO(gw): Consider whether it's worth doing segment building
            //           for gradient primitives.
        }
        PrimitiveInstanceKind::Picture { pic_index, segment_instance_index, .. } => {
            profile_scope!("Picture");
            let pic = &mut store.pictures[pic_index.0];

            if pic.prepare_for_render(
                frame_state,
                data_stores,
            ) {
                if let Picture3DContext::In { root_data: None, plane_splitter_index, .. } = pic.context_3d {
                    let dirty_rect = frame_state.current_dirty_region().combined;
                    let splitter = &mut frame_state.plane_splitters[plane_splitter_index.0];
                    let surface_index = pic.raster_config.as_ref().unwrap().surface_index;
                    let surface = &frame_state.surfaces[surface_index.0];
                    let local_prim_rect = surface.clipped_local_rect.cast_unit();

                    PicturePrimitive::add_split_plane(
                        splitter,
                        frame_context.spatial_tree,
                        prim_spatial_node_index,
                        local_prim_rect,
                        &prim_instance.vis.combined_local_clip_rect,
                        dirty_rect,
                        plane_split_anchor,
                    );
                }

                // If this picture uses segments, ensure the GPU cache is
                // up to date with segment local rects.
                // TODO(gw): This entire match statement above can now be
                //           refactored into prepare_interned_prim_for_render.
                if pic.can_use_segments() {
                    write_segment(
                        *segment_instance_index,
                        frame_state,
                        &mut scratch.segments,
                        &mut scratch.segment_instances,
                        |request| {
                            request.push(PremultipliedColorF::WHITE);
                            request.push(PremultipliedColorF::WHITE);
                            request.push([
                                -1.0,       // -ve means use prim rect for stretch size
                                0.0,
                                0.0,
                                0.0,
                            ]);
                        }
                    );
                }
            } else {
                prim_instance.clear_visibility();
            }
        }
        PrimitiveInstanceKind::BackdropCapture { .. } => {
            // Register the owner picture of this backdrop primitive as the
            // target for resolve of the sub-graph
            frame_state.surface_builder.register_resolve_source();
        }
        PrimitiveInstanceKind::BackdropRender { pic_index, .. } => {
            match frame_state.surface_builder.sub_graph_output_map.get(pic_index).cloned() {
                Some(sub_graph_output_id) => {
                    frame_state.surface_builder.add_child_render_task(
                        sub_graph_output_id,
                        frame_state.rg_builder,
                    );
                }
                None => {
                    // Backdrop capture was found not visible, didn't produce a sub-graph
                    // so we can just skip drawing
                    prim_instance.clear_visibility();
                }
            }
        }
    };
}


fn write_segment<F>(
    segment_instance_index: SegmentInstanceIndex,
    frame_state: &mut FrameBuildingState,
    segments: &mut SegmentStorage,
    segment_instances: &mut SegmentInstanceStorage,
    f: F,
) where F: Fn(&mut GpuDataRequest) {
    debug_assert_ne!(segment_instance_index, SegmentInstanceIndex::INVALID);
    if segment_instance_index != SegmentInstanceIndex::UNUSED {
        let segment_instance = &mut segment_instances[segment_instance_index];

        if let Some(mut request) = frame_state.gpu_cache.request(&mut segment_instance.gpu_cache_handle) {
            let segments = &segments[segment_instance.segments_range];

            f(&mut request);

            for segment in segments {
                request.write_segment(
                    segment.local_rect,
                    [0.0; 4],
                );
            }
        }
    }
}

fn decompose_repeated_gradient(
    prim_vis: &PrimitiveVisibility,
    prim_local_rect: &LayoutRect,
    prim_spatial_node_index: SpatialNodeIndex,
    stretch_size: &LayoutSize,
    tile_spacing: &LayoutSize,
    frame_state: &mut FrameBuildingState,
    gradient_tiles: &mut GradientTileStorage,
    spatial_tree: &SpatialTree,
    mut callback: Option<&mut dyn FnMut(&LayoutRect, GpuDataRequest)>,
) -> GradientTileRange {
    let tile_range = gradient_tiles.open_range();

    // Tighten the clip rect because decomposing the repeated image can
    // produce primitives that are partially covering the original image
    // rect and we want to clip these extra parts out.
    if let Some(tight_clip_rect) = prim_vis
        .combined_local_clip_rect
        .intersection(prim_local_rect) {

        let visible_rect = compute_conservative_visible_rect(
            &prim_vis.clip_chain,
            frame_state.current_dirty_region().combined,
            prim_spatial_node_index,
            spatial_tree,
        );
        let stride = *stretch_size + *tile_spacing;

        let repetitions = image_tiling::repetitions(prim_local_rect, &visible_rect, stride);
        gradient_tiles.reserve(repetitions.num_repetitions());
        for Repetition { origin, .. } in repetitions {
            let mut handle = GpuCacheHandle::new();
            let rect = LayoutRect::from_origin_and_size(
                origin,
                *stretch_size,
            );

            if let Some(callback) = &mut callback {
                if let Some(request) = frame_state.gpu_cache.request(&mut handle) {
                    callback(&rect, request);
                }
            }

            gradient_tiles.push(VisibleGradientTile {
                local_rect: rect,
                local_clip_rect: tight_clip_rect,
                handle
            });
        }
    }

    // At this point if we don't have tiles to show it means we could probably
    // have done a better a job at culling during an earlier stage.
    gradient_tiles.close_range(tile_range)
}


fn update_clip_task_for_brush(
    instance: &PrimitiveInstance,
    prim_origin: &LayoutPoint,
    prim_spatial_node_index: SpatialNodeIndex,
    root_spatial_node_index: SpatialNodeIndex,
    pic_context: &PictureContext,
    pic_state: &mut PictureState,
    frame_context: &FrameBuildingContext,
    frame_state: &mut FrameBuildingState,
    prim_store: &PrimitiveStore,
    data_stores: &mut DataStores,
    segments_store: &mut SegmentStorage,
    segment_instances_store: &mut SegmentInstanceStorage,
    clip_mask_instances: &mut Vec<ClipMaskKind>,
    device_pixel_scale: DevicePixelScale,
) -> Option<ClipTaskIndex> {
    let segments = match instance.kind {
        PrimitiveInstanceKind::TextRun { .. } |
        PrimitiveInstanceKind::Clear { .. } |
        PrimitiveInstanceKind::LineDecoration { .. } |
        PrimitiveInstanceKind::BackdropCapture { .. } |
        PrimitiveInstanceKind::BackdropRender { .. } => {
            return None;
        }
        PrimitiveInstanceKind::Image { image_instance_index, .. } => {
            let segment_instance_index = prim_store
                .images[image_instance_index]
                .segment_instance_index;

            if segment_instance_index == SegmentInstanceIndex::UNUSED {
                return None;
            }

            let segment_instance = &segment_instances_store[segment_instance_index];

            &segments_store[segment_instance.segments_range]
        }
        PrimitiveInstanceKind::Picture { segment_instance_index, .. } => {
            // Pictures may not support segment rendering at all (INVALID)
            // or support segment rendering but choose not to due to size
            // or some other factor (UNUSED).
            if segment_instance_index == SegmentInstanceIndex::UNUSED ||
               segment_instance_index == SegmentInstanceIndex::INVALID {
                return None;
            }

            let segment_instance = &segment_instances_store[segment_instance_index];
            &segments_store[segment_instance.segments_range]
        }
        PrimitiveInstanceKind::YuvImage { segment_instance_index, .. } |
        PrimitiveInstanceKind::Rectangle { segment_instance_index, .. } => {
            debug_assert!(segment_instance_index != SegmentInstanceIndex::INVALID);

            if segment_instance_index == SegmentInstanceIndex::UNUSED {
                return None;
            }

            let segment_instance = &segment_instances_store[segment_instance_index];

            &segments_store[segment_instance.segments_range]
        }
        PrimitiveInstanceKind::ImageBorder { data_handle, .. } => {
            let border_data = &data_stores.image_border[data_handle].kind;

            // TODO: This is quite messy - once we remove legacy primitives we
            //       can change this to be a tuple match on (instance, template)
            border_data.brush_segments.as_slice()
        }
        PrimitiveInstanceKind::NormalBorder { data_handle, .. } => {
            let border_data = &data_stores.normal_border[data_handle].kind;

            // TODO: This is quite messy - once we remove legacy primitives we
            //       can change this to be a tuple match on (instance, template)
            border_data.brush_segments.as_slice()
        }
        PrimitiveInstanceKind::LinearGradient { data_handle, .. }
        | PrimitiveInstanceKind::CachedLinearGradient { data_handle, .. } => {
            let prim_data = &data_stores.linear_grad[data_handle];

            // TODO: This is quite messy - once we remove legacy primitives we
            //       can change this to be a tuple match on (instance, template)
            if prim_data.brush_segments.is_empty() {
                return None;
            }

            prim_data.brush_segments.as_slice()
        }
        PrimitiveInstanceKind::RadialGradient { data_handle, .. } => {
            let prim_data = &data_stores.radial_grad[data_handle];

            // TODO: This is quite messy - once we remove legacy primitives we
            //       can change this to be a tuple match on (instance, template)
            if prim_data.brush_segments.is_empty() {
                return None;
            }

            prim_data.brush_segments.as_slice()
        }
        PrimitiveInstanceKind::ConicGradient { data_handle, .. } => {
            let prim_data = &data_stores.conic_grad[data_handle];

            // TODO: This is quite messy - once we remove legacy primitives we
            //       can change this to be a tuple match on (instance, template)
            if prim_data.brush_segments.is_empty() {
                return None;
            }

            prim_data.brush_segments.as_slice()
        }
    };

    // If there are no segments, early out to avoid setting a valid
    // clip task instance location below.
    if segments.is_empty() {
        return None;
    }

    // Set where in the clip mask instances array the clip mask info
    // can be found for this primitive. Each segment will push the
    // clip mask information for itself in update_clip_task below.
    let clip_task_index = ClipTaskIndex(clip_mask_instances.len() as _);

    // If we only built 1 segment, there is no point in re-running
    // the clip chain builder. Instead, just use the clip chain
    // instance that was built for the main primitive. This is a
    // significant optimization for the common case.
    if segments.len() == 1 {
        let clip_mask_kind = update_brush_segment_clip_task(
            &segments[0],
            Some(&instance.vis.clip_chain),
            root_spatial_node_index,
            pic_context.surface_index,
            frame_context,
            frame_state,
            &mut data_stores.clip,
            device_pixel_scale,
        );
        clip_mask_instances.push(clip_mask_kind);
    } else {
        let dirty_world_rect = frame_state.current_dirty_region().combined;

        for segment in segments {
            // Build a clip chain for the smaller segment rect. This will
            // often manage to eliminate most/all clips, and sometimes
            // clip the segment completely.
            frame_state.clip_store.set_active_clips_from_clip_chain(
                &instance.vis.clip_chain,
                prim_spatial_node_index,
                &frame_context.spatial_tree,
                &data_stores.clip,
            );

            let segment_clip_chain = frame_state
                .clip_store
                .build_clip_chain_instance(
                    segment.local_rect.translate(prim_origin.to_vector()),
                    &pic_state.map_local_to_pic,
                    &pic_state.map_pic_to_world,
                    &frame_context.spatial_tree,
                    frame_state.gpu_cache,
                    frame_state.resource_cache,
                    device_pixel_scale,
                    &dirty_world_rect,
                    &mut data_stores.clip,
                    false,
                );

            let clip_mask_kind = update_brush_segment_clip_task(
                &segment,
                segment_clip_chain.as_ref(),
                root_spatial_node_index,
                pic_context.surface_index,
                frame_context,
                frame_state,
                &mut data_stores.clip,
                device_pixel_scale,
            );
            clip_mask_instances.push(clip_mask_kind);
        }
    }

    Some(clip_task_index)
}

pub fn update_clip_task(
    instance: &mut PrimitiveInstance,
    prim_origin: &LayoutPoint,
    prim_spatial_node_index: SpatialNodeIndex,
    root_spatial_node_index: SpatialNodeIndex,
    pic_context: &PictureContext,
    pic_state: &mut PictureState,
    frame_context: &FrameBuildingContext,
    frame_state: &mut FrameBuildingState,
    prim_store: &mut PrimitiveStore,
    data_stores: &mut DataStores,
    scratch: &mut PrimitiveScratchBuffer,
) -> bool {
    let device_pixel_scale = frame_state.surfaces[pic_context.surface_index.0].device_pixel_scale;

    build_segments_if_needed(
        instance,
        frame_state,
        prim_store,
        data_stores,
        &mut scratch.segments,
        &mut scratch.segment_instances,
    );

    // First try to  render this primitive's mask using optimized brush rendering.
    instance.vis.clip_task_index = if let Some(clip_task_index) = update_clip_task_for_brush(
        instance,
        prim_origin,
        prim_spatial_node_index,
        root_spatial_node_index,
        pic_context,
        pic_state,
        frame_context,
        frame_state,
        prim_store,
        data_stores,
        &mut scratch.segments,
        &mut scratch.segment_instances,
        &mut scratch.clip_mask_instances,
        device_pixel_scale,
    ) {
        clip_task_index
    } else if instance.vis.clip_chain.needs_mask {
        // Get a minimal device space rect, clipped to the screen that we
        // need to allocate for the clip mask, as well as interpolated
        // snap offsets.
        let unadjusted_device_rect = match frame_state.surfaces[pic_context.surface_index.0].get_surface_rect(
            &instance.vis.clip_chain.pic_coverage_rect,
            frame_context.spatial_tree,
        ) {
            Some(rect) => rect,
            None => return false,
        };

        let (device_rect, device_pixel_scale) = adjust_mask_scale_for_max_size(
            unadjusted_device_rect,
            device_pixel_scale,
        );
        let clip_task_id = RenderTaskKind::new_mask(
            device_rect,
            instance.vis.clip_chain.clips_range,
            root_spatial_node_index,
            frame_state.clip_store,
            frame_state.gpu_cache,
            frame_state.resource_cache,
            frame_state.rg_builder,
            &mut data_stores.clip,
            device_pixel_scale,
            frame_context.fb_config,
            &mut frame_state.surface_builder,
        );
        // Set the global clip mask instance for this primitive.
        let clip_task_index = ClipTaskIndex(scratch.clip_mask_instances.len() as _);
        scratch.clip_mask_instances.push(ClipMaskKind::Mask(clip_task_id));
        instance.vis.clip_task_index = clip_task_index;
        frame_state.surface_builder.add_child_render_task(
            clip_task_id,
            frame_state.rg_builder,
        );
        clip_task_index
    } else {
        ClipTaskIndex::INVALID
    };

    true
}

/// Write out to the clip mask instances array the correct clip mask
/// config for this segment.
pub fn update_brush_segment_clip_task(
    segment: &BrushSegment,
    clip_chain: Option<&ClipChainInstance>,
    root_spatial_node_index: SpatialNodeIndex,
    surface_index: SurfaceIndex,
    frame_context: &FrameBuildingContext,
    frame_state: &mut FrameBuildingState,
    clip_data_store: &mut ClipDataStore,
    device_pixel_scale: DevicePixelScale,
) -> ClipMaskKind {
    let clip_chain = match clip_chain {
        Some(chain) => chain,
        None => return ClipMaskKind::Clipped,
    };
    if !clip_chain.needs_mask ||
       (!segment.may_need_clip_mask && !clip_chain.has_non_local_clips) {
        return ClipMaskKind::None;
    }

    let device_rect = match frame_state.surfaces[surface_index.0].get_surface_rect(
        &clip_chain.pic_coverage_rect,
        frame_context.spatial_tree,
    ) {
        Some(rect) => rect,
        None => return ClipMaskKind::Clipped,
    };

    let (device_rect, device_pixel_scale) = adjust_mask_scale_for_max_size(device_rect, device_pixel_scale);

    let clip_task_id = RenderTaskKind::new_mask(
        device_rect,
        clip_chain.clips_range,
        root_spatial_node_index,
        frame_state.clip_store,
        frame_state.gpu_cache,
        frame_state.resource_cache,
        frame_state.rg_builder,
        clip_data_store,
        device_pixel_scale,
        frame_context.fb_config,
        &mut frame_state.surface_builder,
    );

    frame_state.surface_builder.add_child_render_task(
        clip_task_id,
        frame_state.rg_builder,
    );
    ClipMaskKind::Mask(clip_task_id)
}


fn write_brush_segment_description(
    prim_local_rect: LayoutRect,
    prim_local_clip_rect: LayoutRect,
    clip_chain: &ClipChainInstance,
    segment_builder: &mut SegmentBuilder,
    clip_store: &ClipStore,
    data_stores: &DataStores,
) -> bool {
    // If the brush is small, we want to skip building segments
    // and just draw it as a single primitive with clip mask.
    if prim_local_rect.area() < MIN_BRUSH_SPLIT_AREA {
        return false;
    }

    segment_builder.initialize(
        prim_local_rect,
        None,
        prim_local_clip_rect
    );

    // Segment the primitive on all the local-space clip sources that we can.
    for i in 0 .. clip_chain.clips_range.count {
        let clip_instance = clip_store
            .get_instance_from_range(&clip_chain.clips_range, i);
        let clip_node = &data_stores.clip[clip_instance.handle];

        // If this clip item is positioned by another positioning node, its relative position
        // could change during scrolling. This means that we would need to resegment. Instead
        // of doing that, only segment with clips that have the same positioning node.
        // TODO(mrobinson, #2858): It may make sense to include these nodes, resegmenting only
        // when necessary while scrolling.
        if !clip_instance.flags.contains(ClipNodeFlags::SAME_SPATIAL_NODE) {
            continue;
        }

        let (local_clip_rect, radius, mode) = match clip_node.item.kind {
            ClipItemKind::RoundedRectangle { rect, radius, mode } => {
                (rect, Some(radius), mode)
            }
            ClipItemKind::Rectangle { rect, mode } => {
                (rect, None, mode)
            }
            ClipItemKind::BoxShadow { ref source } => {
                // For inset box shadows, we can clip out any
                // pixels that are inside the shadow region
                // and are beyond the inner rect, as they can't
                // be affected by the blur radius.
                let inner_clip_mode = match source.clip_mode {
                    BoxShadowClipMode::Outset => None,
                    BoxShadowClipMode::Inset => Some(ClipMode::ClipOut),
                };

                // Push a region into the segment builder where the
                // box-shadow can have an effect on the result. This
                // ensures clip-mask tasks get allocated for these
                // pixel regions, even if no other clips affect them.
                segment_builder.push_mask_region(
                    source.prim_shadow_rect,
                    source.prim_shadow_rect.inflate(
                        -0.5 * source.original_alloc_size.width,
                        -0.5 * source.original_alloc_size.height,
                    ),
                    inner_clip_mode,
                );

                continue;
            }
            ClipItemKind::Image { .. } => {
                // If we encounter an image mask, bail out from segment building.
                // It's not possible to know which parts of the primitive are affected
                // by the mask (without inspecting the pixels). We could do something
                // better here in the future if it ever shows up as a performance issue
                // (for instance, at least segment based on the bounding rect of the
                // image mask if it's non-repeating).
                return false;
            }
        };

        segment_builder.push_clip_rect(local_clip_rect, radius, mode);
    }

    true
}

fn build_segments_if_needed(
    instance: &mut PrimitiveInstance,
    frame_state: &mut FrameBuildingState,
    prim_store: &mut PrimitiveStore,
    data_stores: &DataStores,
    segments_store: &mut SegmentStorage,
    segment_instances_store: &mut SegmentInstanceStorage,
) {
    let prim_clip_chain = &instance.vis.clip_chain;

    // Usually, the primitive rect can be found from information
    // in the instance and primitive template.
    let prim_local_rect = data_stores.get_local_prim_rect(
        instance,
        &prim_store.pictures,
        frame_state.surfaces,
    );

    let segment_instance_index = match instance.kind {
        PrimitiveInstanceKind::Rectangle { ref mut segment_instance_index, .. } |
        PrimitiveInstanceKind::YuvImage { ref mut segment_instance_index, .. } => {
            segment_instance_index
        }
        PrimitiveInstanceKind::Image { data_handle, image_instance_index, .. } => {
            let image_data = &data_stores.image[data_handle].kind;
            let image_instance = &mut prim_store.images[image_instance_index];
            //Note: tiled images don't support automatic segmentation,
            // they strictly produce one segment per visible tile instead.
            if frame_state
                .resource_cache
                .get_image_properties(image_data.key)
                .and_then(|properties| properties.tiling)
                .is_some()
            {
                image_instance.segment_instance_index = SegmentInstanceIndex::UNUSED;
                return;
            }
            &mut image_instance.segment_instance_index
        }
        PrimitiveInstanceKind::Picture { ref mut segment_instance_index, pic_index, .. } => {
            let pic = &mut prim_store.pictures[pic_index.0];

            // If this picture supports segment rendering
            if pic.can_use_segments() {
                // If the segments have been invalidated, ensure the current
                // index of segments is invalid. This ensures that the segment
                // building logic below will be run.
                if !pic.segments_are_valid {
                    *segment_instance_index = SegmentInstanceIndex::INVALID;
                    pic.segments_are_valid = true;
                }

                segment_instance_index
            } else {
                return;
            }
        }
        PrimitiveInstanceKind::TextRun { .. } |
        PrimitiveInstanceKind::NormalBorder { .. } |
        PrimitiveInstanceKind::ImageBorder { .. } |
        PrimitiveInstanceKind::Clear { .. } |
        PrimitiveInstanceKind::LinearGradient { .. } |
        PrimitiveInstanceKind::CachedLinearGradient { .. } |
        PrimitiveInstanceKind::RadialGradient { .. } |
        PrimitiveInstanceKind::ConicGradient { .. } |
        PrimitiveInstanceKind::LineDecoration { .. } |
        PrimitiveInstanceKind::BackdropCapture { .. } |
        PrimitiveInstanceKind::BackdropRender { .. } => {
            // These primitives don't support / need segments.
            return;
        }
    };

    if *segment_instance_index == SegmentInstanceIndex::INVALID {
        let mut segments: SmallVec<[BrushSegment; 8]> = SmallVec::new();

        if write_brush_segment_description(
            prim_local_rect,
            instance.clip_set.local_clip_rect,
            prim_clip_chain,
            &mut frame_state.segment_builder,
            frame_state.clip_store,
            data_stores,
        ) {
            frame_state.segment_builder.build(|segment| {
                segments.push(
                    BrushSegment::new(
                        segment.rect.translate(-prim_local_rect.min.to_vector()),
                        segment.has_mask,
                        segment.edge_flags,
                        [0.0; 4],
                        BrushFlags::PERSPECTIVE_INTERPOLATION,
                    ),
                );
            });
        }

        // If only a single segment is produced, there is no benefit to writing
        // a segment instance array. Instead, just use the main primitive rect
        // written into the GPU cache.
        // TODO(gw): This is (sortof) a bandaid - due to a limitation in the current
        //           brush encoding, we can only support a total of up to 2^16 segments.
        //           This should be (more than) enough for any real world case, so for
        //           now we can handle this by skipping cases where we were generating
        //           segments where there is no benefit. The long term / robust fix
        //           for this is to move the segment building to be done as a more
        //           limited nine-patch system during scene building, removing arbitrary
        //           segmentation during frame-building (see bug #1617491).
        if segments.len() <= 1 {
            *segment_instance_index = SegmentInstanceIndex::UNUSED;
        } else {
            let segments_range = segments_store.extend(segments);

            let instance = SegmentedInstance {
                segments_range,
                gpu_cache_handle: GpuCacheHandle::new(),
            };

            *segment_instance_index = segment_instances_store.push(instance);
        };
    }
}

// Ensures that the size of mask render tasks are within MAX_MASK_SIZE.
fn adjust_mask_scale_for_max_size(device_rect: DeviceRect, device_pixel_scale: DevicePixelScale) -> (DeviceRect, DevicePixelScale) {
    if device_rect.width() > MAX_MASK_SIZE || device_rect.height() > MAX_MASK_SIZE {
        // round_out will grow by 1 integer pixel if origin is on a
        // fractional position, so keep that margin for error with -1:
        let scale = (MAX_MASK_SIZE - 1.0) /
            f32::max(device_rect.width(), device_rect.height());
        let new_device_pixel_scale = device_pixel_scale * Scale::new(scale);
        let new_device_rect = (device_rect.to_f32() * Scale::new(scale))
            .round_out();
        (new_device_rect, new_device_pixel_scale)
    } else {
        (device_rect, device_pixel_scale)
    }
}

