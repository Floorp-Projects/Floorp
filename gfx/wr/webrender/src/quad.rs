/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{units::*, PremultipliedColorF, ClipMode};
use euclid::point2;

use crate::batch::{BatchKey, BatchKind, BatchTextures};
use crate::clip::{ClipChainInstance, ClipIntern, ClipItemKind, ClipStore};
use crate::command_buffer::{CommandBufferIndex, PrimitiveCommand, QuadFlags};
use crate::frame_builder::{FrameBuildingContext, FrameBuildingState, PictureContext, PictureState};
use crate::gpu_types::{PrimitiveInstanceData, QuadInstance, QuadSegment, TransformPaletteId, ZBufferId};
use crate::intern::DataStore;
use crate::internal_types::TextureSource;
use crate::pattern::{Pattern, PatternKind, PatternShaderInput};
use crate::prim_store::{PrimitiveInstanceIndex, PrimitiveScratchBuffer};
use crate::render_task::{MaskSubPass, RenderTask, RenderTaskAddress, RenderTaskKind, SubPass};
use crate::render_task_graph::{RenderTaskGraph, RenderTaskId};
use crate::renderer::{BlendMode, GpuBufferAddress, GpuBufferBuilder, GpuBufferBuilderF};
use crate::segment::EdgeAaSegmentMask;
use crate::space::SpaceMapper;
use crate::spatial_tree::{SpatialNodeIndex, SpatialTree};
use crate::util::MaxRect;

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

    if let QuadRenderStrategy::Direct = strategy {
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
        return;
    }

    let surface = &frame_state.surfaces[pic_context.surface_index.0];
    let Some(clipped_surface_rect) = surface.get_surface_rect(
        &clip_chain.pic_coverage_rect, frame_context.spatial_tree
    ) else {
        return;
    };

    match strategy {
        QuadRenderStrategy::Direct => {}
        QuadRenderStrategy::Indirect => {
            let segment = add_segment(
                pattern,
                &clipped_surface_rect,
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
                pattern,
                prim_instance_index,
                segment.rect,
                quad_flags,
                frame_state,
                targets,
                &[segment],
            );
        }
        QuadRenderStrategy::Tiled { x_tiles, y_tiles } => {
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
                    let rect = DeviceIntRect {
                        min: point2(x0, y0),
                        max: point2(x1, y1),
                    };

                    let segment = add_segment(
                        pattern,
                        &rect,
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
                pattern,
                prim_instance_index,
                unclipped_surface_rect.cast_unit(),
                quad_flags,
                frame_state,
                targets,
                &scratch.quad_segments,
            );
        }
        QuadRenderStrategy::NinePatch { clip_rect, radius } => {
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

                    // Only create render tasks for the corners.
                    let create_task = x != 1 && y != 1;

                    let rect = DeviceIntRect::new(point2(x0, y0), point2(x1, y1));

                    let rect = match rect.intersection(&clipped_surface_rect) {
                        Some(rect) => rect,
                        None => {
                            continue;
                        }
                    };

                    let segment = add_segment(
                        pattern,
                        &rect,
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
                pattern,
                prim_instance_index,
                unclipped_surface_rect.cast_unit(),
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
    pattern: &Pattern,
    rect: &DeviceIntRect,
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
    let task_size = rect.size();
    let rect = rect.to_f32();
    let content_origin = rect.min;

    let task_id = if create_task {
        let task_id = frame_state.rg_builder.add().init(RenderTask::new_dynamic(
            task_size,
            RenderTaskKind::new_prim(
                pattern.kind,
                pattern.shader_input,
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

    QuadSegment { rect: rect.cast_unit(), task_id }
}

fn add_composite_prim(
    pattern: &Pattern,
    prim_instance_index: PrimitiveInstanceIndex,
    rect: LayoutRect,
    quad_flags: QuadFlags,
    frame_state: &mut FrameBuildingState,
    targets: &[CommandBufferIndex],
    segments: &[QuadSegment],
) {
    let composite_prim_address = write_prim_blocks(
        &mut frame_state.frame_gpu_data.f32,
        rect,
        rect,
        pattern.base_color,
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
            PatternKind::ColorOrTexture,
            pattern.shader_input,
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

pub fn write_prim_blocks(
    builder: &mut GpuBufferBuilderF,
    prim_rect: LayoutRect,
    clip_rect: LayoutRect,
    color: PremultipliedColorF,
    segments: &[QuadSegment],
) -> GpuBufferAddress {
    let mut writer = builder.write_blocks(3 + segments.len() * 2);

    writer.push_one(prim_rect);
    writer.push_one(clip_rect);
    writer.push_one(color);

    for segment in segments {
        writer.push_one(segment.rect);
        match segment.task_id {
            RenderTaskId::INVALID => {
                writer.push_one([0.0; 4]);
            }
            task_id => {
                writer.push_render_task(task_id);
            }
        }
    }

    writer.finish()
}

pub fn add_to_batch<F>(
    kind: PatternKind,
    pattern_input: PatternShaderInput,
    render_task_address: RenderTaskAddress,
    transform_id: TransformPaletteId,
    prim_address_f: GpuBufferAddress,
    quad_flags: QuadFlags,
    edge_flags: EdgeAaSegmentMask,
    segment_index: u8,
    task_id: RenderTaskId,
    z_id: ZBufferId,
    render_tasks: &RenderTaskGraph,
    gpu_buffer_builder: &mut GpuBufferBuilder,
    mut f: F,
) where F: FnMut(BatchKey, PrimitiveInstanceData) {

    // See the corresponfing #defines in ps_quad.glsl
    #[repr(u8)]
    enum PartIndex {
        Center = 0,
        Left = 1,
        Top = 2,
        Right = 3,
        Bottom = 4,
        All = 5,
    }

    // See QuadHeader in ps_quad.glsl
    let mut writer = gpu_buffer_builder.i32.write_blocks(1);
    writer.push_one([
        transform_id.0 as i32,
        z_id.0,
        pattern_input.0,
        pattern_input.1,
    ]);
    let prim_address_i = writer.finish();

    let texture = match task_id {
        RenderTaskId::INVALID => {
            TextureSource::Invalid
        }
        _ => {
            let texture = render_tasks
                .resolve_texture(task_id)
                .expect("bug: valid task id must be resolvable");

            texture
        }
    };

    let textures = BatchTextures::prim_textured(
        texture,
        TextureSource::Invalid,
    );

    let default_blend_mode = if quad_flags.contains(QuadFlags::IS_OPAQUE) && task_id == RenderTaskId::INVALID {
        BlendMode::None
    } else {
        BlendMode::PremultipliedAlpha
    };

    let edge_flags_bits = edge_flags.bits();

    let prim_batch_key = BatchKey {
        blend_mode: default_blend_mode,
        kind: BatchKind::Quad(kind),
        textures,
    };

    let aa_batch_key = BatchKey {
        blend_mode: BlendMode::PremultipliedAlpha,
        kind: BatchKind::Quad(kind),
        textures,
    };

    let mut instance = QuadInstance {
        render_task_address,
        prim_address_i,
        prim_address_f,
        z_id,
        transform_id,
        edge_flags: edge_flags_bits,
        quad_flags: quad_flags.bits(),
        part_index: PartIndex::All as u8,
        segment_index,
    };

    if edge_flags.is_empty() {
        // No antialisaing.
        f(prim_batch_key, instance.into());
    } else if quad_flags.contains(QuadFlags::USE_AA_SEGMENTS) {
        // Add instances for the antialisaing. This gives the center part
        // an opportunity to stay in the opaque pass.
        if edge_flags.contains(EdgeAaSegmentMask::LEFT) {
            let instance = QuadInstance {
                part_index: PartIndex::Left as u8,
                ..instance
            };
            f(aa_batch_key, instance.into());
        }
        if edge_flags.contains(EdgeAaSegmentMask::RIGHT) {
            let instance = QuadInstance {
                part_index: PartIndex::Top as u8,
                ..instance
            };
            f(aa_batch_key, instance.into());
        }
        if edge_flags.contains(EdgeAaSegmentMask::TOP) {
            let instance = QuadInstance {
                part_index: PartIndex::Right as u8,
                ..instance
            };
            f(aa_batch_key, instance.into());
        }
        if edge_flags.contains(EdgeAaSegmentMask::BOTTOM) {
            let instance = QuadInstance {
                part_index: PartIndex::Bottom as u8,
                ..instance
            };
            f(aa_batch_key, instance.into());
        }

        instance = QuadInstance {
            part_index: PartIndex::Center as u8,
            ..instance
        };

        f(prim_batch_key, instance.into());
    } else {
        // Render the anti-aliased quad with a single primitive.
        f(aa_batch_key, instance.into());
    }
}

