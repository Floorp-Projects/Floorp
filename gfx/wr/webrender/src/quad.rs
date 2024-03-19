/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{units::*, PremultipliedColorF, ClipMode};

use crate::{
    clip::{ClipChainInstance, ClipIntern, ClipItemKind, ClipStore}, command_buffer::{CommandBufferIndex, PrimitiveCommand, QuadFlags}, frame_builder::{FrameBuildingContext, FrameBuildingState, PictureContext, PictureState}, gpu_types::{QuadSegment, TransformPaletteId}, intern::DataStore, pattern::{Pattern, PatternKind, PatternShaderInput}, prepare::write_prim_blocks, prim_store::{PrimitiveInstanceIndex, PrimitiveScratchBuffer}, render_task::{MaskSubPass, RenderTask, RenderTaskKind, SubPass}, render_task_graph::RenderTaskId, renderer::GpuBufferAddress, segment::EdgeAaSegmentMask, space::SpaceMapper, spatial_tree::{SpatialNodeIndex, SpatialTree}, util::MaxRect
};

const MIN_AA_SEGMENTS_SIZE: f32 = 4.0;
const MIN_QUAD_SPLIT_SIZE: f32 = 256.0;

/// Describes how clipping affects the rendering of a quad primitive.
///
/// As a general rule, parts of the quad that require masking are prerendered in an
/// intermediate target and the mask is applied using multiplicative blending to
/// the intermediate result before compositing it into the destination target.
///
/// Each segment can opt in or out of masking independently.
#[derive(Debug, Copy, Clone)]
enum QuadRenderStrategy {
    /// The quad is not affected by any mask and is drawn directly in the destination
    /// target.
    Direct,
    /// The quad is drawn entirely in an intermediate target and a mask is applied
    /// before compositing in the destination target.
    Indirect,
    /// A rounded rectangle clip is applied to the quad primitive via a nine-patch.
    /// The segments of the nine-patch that require a mask are rendered and masked in
    /// an intermediate target, while other segments are drawn directly in the destination
    /// target.
    NinePatch {
        radius: LayoutVector2D,
        clip_rect: LayoutRect,
    },
    /// Split the primitive into coarse tiles so that each tile independently
    /// has the opportunity to be drawn directly in the destination target or
    /// via an intermediate target if it is affected by a mask.
    Tiled {
        x_tiles: u16,
        y_tiles: u16,
    }
}

pub fn push_quad(
    pattern: &Pattern,
    local_rect: &LayoutRect,
    prim_instance_index: PrimitiveInstanceIndex,
    prim_spatial_node_index: SpatialNodeIndex,
    clip_chain: &ClipChainInstance,
    device_pixel_scale: DevicePixelScale,

    frame_context: &FrameBuildingContext,
    pic_context: &PictureContext,
    targets: &[CommandBufferIndex],
    interned_clips: &DataStore<ClipIntern>,

    frame_state: &mut FrameBuildingState,
    pic_state: &mut PictureState,
    scratch: &mut PrimitiveScratchBuffer,

) {
    let map_prim_to_surface = frame_context.spatial_tree.get_relative_transform(
        prim_spatial_node_index,
        pic_context.raster_spatial_node_index,
    );
    let prim_is_2d_scale_translation = map_prim_to_surface.is_2d_scale_translation();
    let prim_is_2d_axis_aligned = map_prim_to_surface.is_2d_axis_aligned();

    let strategy = get_prim_render_strategy(
        prim_spatial_node_index,
        clip_chain,
        frame_state.clip_store,
        interned_clips,
        prim_is_2d_scale_translation,
        frame_context.spatial_tree,
    );

    let mut quad_flags = QuadFlags::empty();

    // Only use AA edge instances if the primitive is large enough to require it
    let prim_size = local_rect.size();
    if prim_size.width > MIN_AA_SEGMENTS_SIZE && prim_size.height > MIN_AA_SEGMENTS_SIZE {
        quad_flags |= QuadFlags::USE_AA_SEGMENTS;
    }

    if pattern.is_opaque {
        quad_flags |= QuadFlags::IS_OPAQUE;
    }
    let needs_scissor = !prim_is_2d_scale_translation;
    if !needs_scissor {
        quad_flags |= QuadFlags::APPLY_DEVICE_CLIP;
    }

    // TODO(gw): For now, we don't select per-edge AA at all if the primitive
    //           has a 2d transform, which matches existing behavior. However,
    //           as a follow up, we can now easily check if we have a 2d-aligned
    //           primitive on a subpixel boundary, and enable AA along those edge(s).
    let aa_flags = if prim_is_2d_axis_aligned {
        EdgeAaSegmentMask::empty()
    } else {
        EdgeAaSegmentMask::all()
    };

    let transform_id = frame_state.transforms.get_id(
        prim_spatial_node_index,
        pic_context.raster_spatial_node_index,
        frame_context.spatial_tree,
    );

    // TODO(gw): Perhaps rather than writing untyped data here (we at least do validate
    //           the written block count) to gpu-buffer, we could add a trait for
    //           writing typed data?
    let main_prim_address = write_prim_blocks(
        &mut frame_state.frame_gpu_data.f32,
        *local_rect,
        clip_chain.local_clip_rect,
        pattern.base_color,
        &[],
    );

    match strategy {
        QuadRenderStrategy::Direct => {
            frame_state.push_prim(
                &PrimitiveCommand::quad(
                    pattern.kind,
                    pattern.shader_input,
                    prim_instance_index,
                    main_prim_address,
                    transform_id,
                    quad_flags,
                    aa_flags,
                ),
                prim_spatial_node_index,
                targets,
            );
        }
        QuadRenderStrategy::Indirect => {
            let surface = &frame_state.surfaces[pic_context.surface_index.0];
            let Some(clipped_surface_rect) = surface.get_surface_rect(
                &clip_chain.pic_coverage_rect, frame_context.spatial_tree
            ) else {
                return;
            };

            let p0 = clipped_surface_rect.min.to_f32();
            let p1 = clipped_surface_rect.max.to_f32();

            let segment = add_segment(
                pattern.kind,
                pattern.shader_input,
                p0.x,
                p0.y,
                p1.x,
                p1.y,
                true,
                clip_chain,
                prim_spatial_node_index,
                pic_context.raster_spatial_node_index,
                main_prim_address,
                transform_id,
                aa_flags,
                quad_flags,
                device_pixel_scale,
                needs_scissor,
                frame_state,
            );

            add_composite_prim(
                pattern.kind,
                pattern.shader_input,
                prim_instance_index,
                LayoutRect::new(p0.cast_unit(), p1.cast_unit()),
                pattern.base_color,
                quad_flags,
                frame_state,
                targets,
                &[segment],
            );
        }
        QuadRenderStrategy::Tiled { x_tiles, y_tiles } => {
            let surface = &frame_state.surfaces[pic_context.surface_index.0];
            let Some(clipped_surface_rect) = surface.get_surface_rect(
                &clip_chain.pic_coverage_rect, frame_context.spatial_tree
            ) else {
                return;
            };

            let unclipped_surface_rect = surface
                .map_to_device_rect(&clip_chain.pic_coverage_rect, frame_context.spatial_tree);

            scratch.quad_segments.clear();

            let mut x_coords = vec![clipped_surface_rect.min.x];
            let mut y_coords = vec![clipped_surface_rect.min.y];

            let dx = (clipped_surface_rect.max.x - clipped_surface_rect.min.x) as f32 / x_tiles as f32;
            let dy = (clipped_surface_rect.max.y - clipped_surface_rect.min.y) as f32 / y_tiles as f32;

            for x in 1 .. (x_tiles as i32) {
                x_coords.push((clipped_surface_rect.min.x as f32 + x as f32 * dx).round() as i32);
            }
            for y in 1 .. (y_tiles as i32) {
                y_coords.push((clipped_surface_rect.min.y as f32 + y as f32 * dy).round() as i32);
            }

            x_coords.push(clipped_surface_rect.max.x);
            y_coords.push(clipped_surface_rect.max.y);

            for y in 0 .. y_coords.len()-1 {
                let y0 = y_coords[y];
                let y1 = y_coords[y+1];

                if y1 <= y0 {
                    continue;
                }

                for x in 0 .. x_coords.len()-1 {
                    let x0 = x_coords[x];
                    let x1 = x_coords[x+1];

                    if x1 <= x0 {
                        continue;
                    }

                    let create_task = true;

                    let segment = add_segment(
                        pattern.kind,
                        pattern.shader_input,
                        x0 as f32,
                        y0 as f32,
                        x1 as f32,
                        y1 as f32,
                        create_task,
                        clip_chain,
                        prim_spatial_node_index,
                        pic_context.raster_spatial_node_index,
                        main_prim_address,
                        transform_id,
                        aa_flags,
                        quad_flags,
                        device_pixel_scale,
                        needs_scissor,
                        frame_state,
                    );
                    scratch.quad_segments.push(segment);
                }
            }

            add_composite_prim(
                pattern.kind,
                pattern.shader_input,
                prim_instance_index,
                unclipped_surface_rect.cast_unit(),
                pattern.base_color,
                quad_flags,
                frame_state,
                targets,
                &scratch.quad_segments,
            );
        }
        QuadRenderStrategy::NinePatch { clip_rect, radius } => {
            let surface = &frame_state.surfaces[pic_context.surface_index.0];
            let Some(clipped_surface_rect) = surface.get_surface_rect(
                &clip_chain.pic_coverage_rect, frame_context.spatial_tree
            ) else {
                return;
            };

            let unclipped_surface_rect = surface
                .map_to_device_rect(&clip_chain.pic_coverage_rect, frame_context.spatial_tree);

            let local_corner_0 = LayoutRect::new(
                clip_rect.min,
                clip_rect.min + radius,
            );

            let local_corner_1 = LayoutRect::new(
                clip_rect.max - radius,
                clip_rect.max,
            );

            let pic_corner_0 = pic_state.map_local_to_pic.map(&local_corner_0).unwrap();
            let pic_corner_1 = pic_state.map_local_to_pic.map(&local_corner_1).unwrap();

            let surface_rect_0 = surface.map_to_device_rect(
                &pic_corner_0,
                frame_context.spatial_tree,
            ).round_out().to_i32();

            let surface_rect_1 = surface.map_to_device_rect(
                &pic_corner_1,
                frame_context.spatial_tree,
            ).round_out().to_i32();

            let p0 = surface_rect_0.min;
            let p1 = surface_rect_0.max;
            let p2 = surface_rect_1.min;
            let p3 = surface_rect_1.max;

            let mut x_coords = [p0.x, p1.x, p2.x, p3.x];
            let mut y_coords = [p0.y, p1.y, p2.y, p3.y];

            x_coords.sort_by(|a, b| a.partial_cmp(b).unwrap());
            y_coords.sort_by(|a, b| a.partial_cmp(b).unwrap());

            scratch.quad_segments.clear();

            for y in 0 .. y_coords.len()-1 {
                let y0 = y_coords[y];
                let y1 = y_coords[y+1];

                if y1 <= y0 {
                    continue;
                }

                for x in 0 .. x_coords.len()-1 {
                    let x0 = x_coords[x];
                    let x1 = x_coords[x+1];

                    if x1 <= x0 {
                        continue;
                    }

                    let create_task = if x == 1 || y == 1 {
                        false
                    } else {
                        true
                    };

                    let r = DeviceIntRect::new(
                        DeviceIntPoint::new(x0, y0),
                        DeviceIntPoint::new(x1, y1),
                    );

                    let r = match r.intersection(&clipped_surface_rect) {
                        Some(r) => r,
                        None => {
                            continue;
                        }
                    };

                    let segment = add_segment(
                        pattern.kind,
                        pattern.shader_input,
                        r.min.x as f32,
                        r.min.y as f32,
                        r.max.x as f32,
                        r.max.y as f32,
                        create_task,
                        clip_chain,
                        prim_spatial_node_index,
                        pic_context.raster_spatial_node_index,
                        main_prim_address,
                        transform_id,
                        aa_flags,
                        quad_flags,
                        device_pixel_scale,
                        false,
                        frame_state,
                    );
                    scratch.quad_segments.push(segment);
                }
            }

            add_composite_prim(
                pattern.kind,
                pattern.shader_input,
                prim_instance_index,
                unclipped_surface_rect.cast_unit(),
                pattern.base_color,
                quad_flags,
                frame_state,
                targets,
                &scratch.quad_segments,
            );
        }
    }
}

fn get_prim_render_strategy(
    prim_spatial_node_index: SpatialNodeIndex,
    clip_chain: &ClipChainInstance,
    clip_store: &ClipStore,
    interned_clips: &DataStore<ClipIntern>,
    can_use_nine_patch: bool,
    spatial_tree: &SpatialTree,
) -> QuadRenderStrategy {
    if !clip_chain.needs_mask {
        return QuadRenderStrategy::Direct
    }

    fn tile_count_for_size(size: f32) -> u16 {
        (size / MIN_QUAD_SPLIT_SIZE).min(4.0).max(1.0).ceil() as u16
    }

    let prim_coverage_size = clip_chain.pic_coverage_rect.size();
    let x_tiles = tile_count_for_size(prim_coverage_size.width);
    let y_tiles = tile_count_for_size(prim_coverage_size.height);
    let try_split_prim = x_tiles > 1 || y_tiles > 1;

    if !try_split_prim {
        return QuadRenderStrategy::Indirect;
    }

    if can_use_nine_patch && clip_chain.clips_range.count == 1 {
        let clip_instance = clip_store.get_instance_from_range(&clip_chain.clips_range, 0);
        let clip_node = &interned_clips[clip_instance.handle];

        if let ClipItemKind::RoundedRectangle { ref radius, mode: ClipMode::Clip, rect, .. } = clip_node.item.kind {
            let max_corner_width = radius.top_left.width
                                        .max(radius.bottom_left.width)
                                        .max(radius.top_right.width)
                                        .max(radius.bottom_right.width);
            let max_corner_height = radius.top_left.height
                                        .max(radius.bottom_left.height)
                                        .max(radius.top_right.height)
                                        .max(radius.bottom_right.height);

            if max_corner_width <= 0.5 * rect.size().width &&
                max_corner_height <= 0.5 * rect.size().height {

                let clip_prim_coords_match = spatial_tree.is_matching_coord_system(
                    prim_spatial_node_index,
                    clip_node.item.spatial_node_index,
                );

                if clip_prim_coords_match {
                    let map_clip_to_prim = SpaceMapper::new_with_target(
                        prim_spatial_node_index,
                        clip_node.item.spatial_node_index,
                        LayoutRect::max_rect(),
                        spatial_tree,
                    );

                    if let Some(rect) = map_clip_to_prim.map(&rect) {
                        return QuadRenderStrategy::NinePatch {
                            radius: LayoutVector2D::new(max_corner_width, max_corner_height),
                            clip_rect: rect,
                        };
                    }
                }
            }
        }
    }

    QuadRenderStrategy::Tiled {
        x_tiles,
        y_tiles,
    }
}

fn add_segment(
    kind: PatternKind,
    pattern_input: PatternShaderInput,
    x0: f32,
    y0: f32,
    x1: f32,
    y1: f32,
    create_task: bool,
    clip_chain: &ClipChainInstance,
    prim_spatial_node_index: SpatialNodeIndex,
    raster_spatial_node_index: SpatialNodeIndex,
    prim_address_f: GpuBufferAddress,
    transform_id: TransformPaletteId,
    aa_flags: EdgeAaSegmentMask,
    quad_flags: QuadFlags,
    device_pixel_scale: DevicePixelScale,
    needs_scissor_rect: bool,
    frame_state: &mut FrameBuildingState,
) -> QuadSegment {
    let task_size = DeviceSize::new(x1 - x0, y1 - y0).round().to_i32();
    let content_origin = DevicePoint::new(x0, y0);

    let rect = LayoutRect::new(LayoutPoint::new(x0, y0), LayoutPoint::new(x1, y1));

    let task_id = if create_task {
        let task_id = frame_state.rg_builder.add().init(RenderTask::new_dynamic(
            task_size,
            RenderTaskKind::new_prim(
                kind,
                pattern_input,
                prim_spatial_node_index,
                raster_spatial_node_index,
                device_pixel_scale,
                content_origin,
                prim_address_f,
                transform_id,
                aa_flags,
                quad_flags,
                clip_chain.clips_range,
                needs_scissor_rect,
            ),
        ));

        let masks = MaskSubPass {
            clip_node_range: clip_chain.clips_range,
            prim_spatial_node_index,
            prim_address_f,
        };

        let task = frame_state.rg_builder.get_task_mut(task_id);
        task.add_sub_pass(SubPass::Masks { masks });

        frame_state
            .surface_builder
            .add_child_render_task(task_id, frame_state.rg_builder);

        task_id
    } else {
        RenderTaskId::INVALID
    };

    QuadSegment { rect, task_id }
}

fn add_composite_prim(
    pattern_kind: PatternKind,
    pattern_input: PatternShaderInput,
    prim_instance_index: PrimitiveInstanceIndex,
    rect: LayoutRect,
    color: PremultipliedColorF,
    quad_flags: QuadFlags,
    frame_state: &mut FrameBuildingState,
    targets: &[CommandBufferIndex],
    segments: &[QuadSegment],
) {
    let composite_prim_address = write_prim_blocks(
        &mut frame_state.frame_gpu_data.f32,
        rect,
        rect,
        color,
        segments,
    );

    frame_state.set_segments(segments, targets);

    let mut composite_quad_flags =
        QuadFlags::IGNORE_DEVICE_PIXEL_SCALE | QuadFlags::APPLY_DEVICE_CLIP;
    if quad_flags.contains(QuadFlags::IS_OPAQUE) {
        composite_quad_flags |= QuadFlags::IS_OPAQUE;
    }

    frame_state.push_cmd(
        &PrimitiveCommand::quad(
            pattern_kind,
            pattern_input,
            prim_instance_index,
            composite_prim_address,
            TransformPaletteId::IDENTITY,
            composite_quad_flags,
            // TODO(gw): No AA on composite, unless we use it to apply 2d clips
            EdgeAaSegmentMask::empty(),
        ),
        targets,
    );
}
