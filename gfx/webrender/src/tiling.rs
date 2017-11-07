/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{BorderRadiusKind, ClipAndScrollInfo, ClipId, ColorF, DeviceIntPoint, ImageKey};
use api::{DeviceIntRect, DeviceIntSize, DeviceUintPoint, DeviceUintSize};
use api::{ExternalImageType, FilterOp, FontRenderMode, ImageRendering, LayerRect};
use api::{MixBlendMode, PipelineId, PropertyBinding, TransformStyle};
use api::{LayerVector2D, TileOffset, YuvColorSpace, YuvFormat};
use border::{BorderCornerInstance, BorderCornerSide};
use clip::{ClipSource, ClipStore};
use clip_scroll_tree::CoordinateSystemId;
use device::Texture;
use glyph_rasterizer::GlyphFormat;
use gpu_cache::{GpuCache, GpuCacheAddress, GpuCacheHandle, GpuCacheUpdateList};
use gpu_types::{BlurDirection, BlurInstance, BrushInstance, BrushImageKind, ClipMaskInstance};
use gpu_types::{CompositePrimitiveInstance, PrimitiveInstance, SimplePrimitiveInstance};
use gpu_types::{BRUSH_FLAG_USES_PICTURE, ClipScrollNodeIndex, ClipScrollNodeData};
use internal_types::{FastHashMap, SourceTexture};
use internal_types::BatchTextures;
use picture::PictureKind;
use prim_store::{PrimitiveIndex, PrimitiveKind, PrimitiveMetadata, PrimitiveStore};
use prim_store::{BrushMaskKind, BrushKind, DeferredResolve, RectangleContent};
use profiler::FrameProfileCounters;
use render_task::{AlphaRenderItem, ClipWorkItem, MaskGeometryKind, MaskSegment};
use render_task::{RenderTaskAddress, RenderTaskId, RenderTaskKey, RenderTaskKind};
use render_task::{BlurTask, ClearMode, RenderTaskLocation, RenderTaskTree};
use renderer::BlendMode;
use renderer::ImageBufferKind;
use resource_cache::{GlyphFetchResult, ResourceCache};
use std::{cmp, usize, f32, i32};
use texture_allocator::GuillotineAllocator;
use util::{MatrixHelpers, TransformedRectKind};

// Special sentinel value recognized by the shader. It is considered to be
// a dummy task that doesn't mask out anything.
const OPAQUE_TASK_ADDRESS: RenderTaskAddress = RenderTaskAddress(i32::MAX as u32);
const MIN_TARGET_SIZE: u32 = 2048;

trait AlphaBatchHelpers {
    fn get_blend_mode(
        &self,
        metadata: &PrimitiveMetadata,
        transform_kind: TransformedRectKind,
    ) -> BlendMode;
}

impl AlphaBatchHelpers for PrimitiveStore {
    fn get_blend_mode(
        &self,
        metadata: &PrimitiveMetadata,
        transform_kind: TransformedRectKind,
    ) -> BlendMode {
        let needs_blending = !metadata.opacity.is_opaque || metadata.clip_task_id.is_some() ||
            transform_kind == TransformedRectKind::Complex;

        match metadata.prim_kind {
            PrimitiveKind::TextRun => {
                let font = &self.cpu_text_runs[metadata.cpu_prim_index.0].font;
                match font.render_mode {
                    FontRenderMode::Subpixel => {
                        if font.bg_color.a != 0 {
                            BlendMode::SubpixelWithBgColor
                        } else if font.color.a != 255 || metadata.clip_task_id.is_some() {
                            BlendMode::SubpixelWithAlpha
                        } else {
                            BlendMode::SubpixelOpaque(font.color)
                        }
                    }
                    FontRenderMode::Alpha |
                    FontRenderMode::Mono |
                    FontRenderMode::Bitmap => BlendMode::PremultipliedAlpha,
                }
            },
            PrimitiveKind::Rectangle => {
                let rectangle_cpu = &self.cpu_rectangles[metadata.cpu_prim_index.0];
                match rectangle_cpu.content {
                    RectangleContent::Fill(..) => if needs_blending {
                        BlendMode::PremultipliedAlpha
                    } else {
                        BlendMode::None
                    },
                    RectangleContent::Clear => {
                        // TODO: If needs_blending == false, we could use BlendMode::None
                        // to clear the rectangle, but then we'd need to draw the rectangle
                        // with alpha == 0.0 instead of alpha == 1.0, and the RectanglePrimitive
                        // would need to know about that.
                        BlendMode::PremultipliedDestOut
                    },
                }
            },
            PrimitiveKind::Border |
            PrimitiveKind::Image |
            PrimitiveKind::AlignedGradient |
            PrimitiveKind::AngleGradient |
            PrimitiveKind::RadialGradient |
            PrimitiveKind::Picture => if needs_blending {
                BlendMode::PremultipliedAlpha
            } else {
                BlendMode::None
            },
            PrimitiveKind::YuvImage |
            PrimitiveKind::Line |
            PrimitiveKind::Brush => if needs_blending {
                BlendMode::Alpha
            } else {
                BlendMode::None
            },
        }
    }
}

#[derive(Debug)]
pub struct ScrollbarPrimitive {
    pub clip_id: ClipId,
    pub prim_index: PrimitiveIndex,
    pub border_radius: f32,
}

#[derive(Debug)]
pub enum PrimitiveRunCmd {
    PushStackingContext(StackingContextIndex),
    PopStackingContext,
    PrimitiveRun(PrimitiveIndex, usize, ClipAndScrollInfo),
}

#[derive(Debug, Copy, Clone)]
pub enum PrimitiveFlags {
    None,
    Scrollbar(ClipId, f32),
}

#[derive(Debug, Copy, Clone)]
pub struct RenderTargetIndex(pub usize);

#[derive(Debug, Copy, Clone, Eq, PartialEq, Hash)]
pub struct RenderPassIndex(isize);

#[derive(Debug)]
struct DynamicTaskInfo {
    task_id: RenderTaskId,
    rect: DeviceIntRect,
}

pub struct AlphaBatchList {
    pub batches: Vec<AlphaPrimitiveBatch>,
}

impl AlphaBatchList {
    fn new() -> AlphaBatchList {
        AlphaBatchList {
            batches: Vec::new(),
        }
    }

    fn get_suitable_batch(
        &mut self,
        key: BatchKey,
        item_bounding_rect: &DeviceIntRect,
    ) -> &mut Vec<PrimitiveInstance> {
        let mut selected_batch_index = None;

        match (key.kind, key.blend_mode) {
            (BatchKind::Composite { .. }, _) => {
                // Composites always get added to their own batch.
                // This is because the result of a composite can affect
                // the input to the next composite. Perhaps we can
                // optimize this in the future.
            }
            (BatchKind::Transformable(_, TransformBatchKind::TextRun(_)), BlendMode::SubpixelWithBgColor) |
            (BatchKind::Transformable(_, TransformBatchKind::TextRun(_)), BlendMode::SubpixelWithAlpha) => {
                'outer_text: for (batch_index, batch) in self.batches.iter().enumerate().rev().take(10) {
                    // Subpixel text is drawn in two passes. Because of this, we need
                    // to check for overlaps with every batch (which is a bit different
                    // than the normal batching below).
                    for item_rect in &batch.item_rects {
                        if item_rect.intersects(item_bounding_rect) {
                            break 'outer_text;
                        }
                    }

                    if batch.key.is_compatible_with(&key) {
                        selected_batch_index = Some(batch_index);
                        break;
                    }
                }
            }
            _ => {
                'outer_default: for (batch_index, batch) in self.batches.iter().enumerate().rev().take(10) {
                    // For normal batches, we only need to check for overlaps for batches
                    // other than the first batch we consider. If the first batch
                    // is compatible, then we know there isn't any potential overlap
                    // issues to worry about.
                    if batch.key.is_compatible_with(&key) {
                        selected_batch_index = Some(batch_index);
                        break;
                    }

                    // check for intersections
                    for item_rect in &batch.item_rects {
                        if item_rect.intersects(item_bounding_rect) {
                            break 'outer_default;
                        }
                    }
                }
            }
        }

        if selected_batch_index.is_none() {
            let new_batch = AlphaPrimitiveBatch::new(key);
            selected_batch_index = Some(self.batches.len());
            self.batches.push(new_batch);
        }

        let batch = &mut self.batches[selected_batch_index.unwrap()];
        batch.item_rects.push(*item_bounding_rect);

        &mut batch.instances
    }
}

pub struct OpaqueBatchList {
    pub batches: Vec<OpaquePrimitiveBatch>,
}

impl OpaqueBatchList {
    fn new() -> OpaqueBatchList {
        OpaqueBatchList {
            batches: Vec::new(),
        }
    }

    fn get_suitable_batch(&mut self, key: BatchKey) -> &mut Vec<PrimitiveInstance> {
        let mut selected_batch_index = None;

        for (batch_index, batch) in self.batches.iter().enumerate().rev().take(10) {
            if batch.key.is_compatible_with(&key) {
                selected_batch_index = Some(batch_index);
                break;
            }
        }

        if selected_batch_index.is_none() {
            let new_batch = OpaquePrimitiveBatch::new(key);
            selected_batch_index = Some(self.batches.len());
            self.batches.push(new_batch);
        }

        let batch = &mut self.batches[selected_batch_index.unwrap()];

        &mut batch.instances
    }

    fn finalize(&mut self) {
        // Reverse the instance arrays in the opaque batches
        // to get maximum z-buffer efficiency by drawing
        // front-to-back.
        // TODO(gw): Maybe we can change the batch code to
        //           build these in reverse and avoid having
        //           to reverse the instance array here.
        for batch in &mut self.batches {
            batch.instances.reverse();
        }
    }
}

pub struct BatchList {
    pub alpha_batch_list: AlphaBatchList,
    pub opaque_batch_list: OpaqueBatchList,
}

impl BatchList {
    fn new() -> BatchList {
        BatchList {
            alpha_batch_list: AlphaBatchList::new(),
            opaque_batch_list: OpaqueBatchList::new(),
        }
    }

    fn get_suitable_batch(
        &mut self,
        key: BatchKey,
        item_bounding_rect: &DeviceIntRect,
    ) -> &mut Vec<PrimitiveInstance> {
        match key.blend_mode {
            BlendMode::None => self.opaque_batch_list.get_suitable_batch(key),
            BlendMode::Alpha | BlendMode::PremultipliedAlpha |
            BlendMode::PremultipliedDestOut |
            BlendMode::SubpixelOpaque(..) |
            BlendMode::SubpixelWithAlpha |
            BlendMode::SubpixelWithBgColor => {
                self.alpha_batch_list
                    .get_suitable_batch(key, item_bounding_rect)
            }
        }
    }

    fn finalize(&mut self) {
        self.opaque_batch_list.finalize()
    }
}

/// Encapsulates the logic of building batches for items that are blended.
pub struct AlphaBatcher {
    pub batch_list: BatchList,
    tasks: Vec<RenderTaskId>,
    glyph_fetch_buffer: Vec<GlyphFetchResult>,
}

impl AlphaRenderItem {
    fn add_to_batch(
        &self,
        batch_list: &mut BatchList,
        ctx: &RenderTargetContext,
        gpu_cache: &mut GpuCache,
        render_tasks: &RenderTaskTree,
        task_id: RenderTaskId,
        task_address: RenderTaskAddress,
        deferred_resolves: &mut Vec<DeferredResolve>,
        glyph_fetch_buffer: &mut Vec<GlyphFetchResult>,
    ) {
        match *self {
            AlphaRenderItem::Blend(stacking_context_index, src_id, filter, z) => {
                let stacking_context = &ctx.stacking_context_store[stacking_context_index.0];
                let key = BatchKey::new(
                    BatchKind::Blend,
                    BlendMode::PremultipliedAlpha,
                    BatchTextures::no_texture(),
                );
                let src_task_address = render_tasks.get_task_address(src_id);

                let (filter_mode, amount) = match filter {
                    FilterOp::Blur(..) => (0, 0.0),
                    FilterOp::Contrast(amount) => (1, amount),
                    FilterOp::Grayscale(amount) => (2, amount),
                    FilterOp::HueRotate(angle) => (3, angle),
                    FilterOp::Invert(amount) => (4, amount),
                    FilterOp::Saturate(amount) => (5, amount),
                    FilterOp::Sepia(amount) => (6, amount),
                    FilterOp::Brightness(amount) => (7, amount),
                    FilterOp::Opacity(PropertyBinding::Value(amount)) => (8, amount),
                    FilterOp::Opacity(_) => unreachable!(),
                };

                let amount = (amount * 65535.0).round() as i32;
                let batch = batch_list.get_suitable_batch(key, &stacking_context.screen_bounds);

                let instance = CompositePrimitiveInstance::new(
                    task_address,
                    src_task_address,
                    RenderTaskAddress(0),
                    filter_mode,
                    amount,
                    z,
                    0,
                    0,
                );

                batch.push(PrimitiveInstance::from(instance));
            }
            AlphaRenderItem::HardwareComposite(
                stacking_context_index,
                src_id,
                composite_op,
                screen_origin,
                z,
                dest_rect,
            ) => {
                let stacking_context = &ctx.stacking_context_store[stacking_context_index.0];
                let src_task_address = render_tasks.get_task_address(src_id);
                let key = BatchKey::new(
                    BatchKind::HardwareComposite,
                    composite_op.to_blend_mode(),
                    BatchTextures::no_texture(),
                );
                let batch = batch_list.get_suitable_batch(key, &stacking_context.screen_bounds);
                let dest_rect = if dest_rect.width > 0 && dest_rect.height > 0 {
                    dest_rect
                } else {
                    render_tasks.get(src_id).get_dynamic_size()
                };

                let instance = CompositePrimitiveInstance::new(
                    task_address,
                    src_task_address,
                    RenderTaskAddress(0),
                    screen_origin.x,
                    screen_origin.y,
                    z,
                    dest_rect.width,
                    dest_rect.height,
                );

                batch.push(PrimitiveInstance::from(instance));
            }
            AlphaRenderItem::Composite(stacking_context_index, source_id, backdrop_id, mode, z) => {
                let stacking_context = &ctx.stacking_context_store[stacking_context_index.0];
                let key = BatchKey::new(
                    BatchKind::Composite {
                        task_id,
                        source_id,
                        backdrop_id,
                    },
                    BlendMode::Alpha,
                    BatchTextures::no_texture(),
                );
                let batch = batch_list.get_suitable_batch(key, &stacking_context.screen_bounds);
                let backdrop_task_address = render_tasks.get_task_address(backdrop_id);
                let source_task_address = render_tasks.get_task_address(source_id);

                let instance = CompositePrimitiveInstance::new(
                    task_address,
                    source_task_address,
                    backdrop_task_address,
                    mode as u32 as i32,
                    0,
                    z,
                    0,
                    0,
                );

                batch.push(PrimitiveInstance::from(instance));
            }
            AlphaRenderItem::Primitive(clip_id, scroll_id, prim_index, z) => {
                let prim_metadata = ctx.prim_store.get_metadata(prim_index);
                let scroll_node = &ctx.node_data[scroll_id.0 as usize];
                // TODO(gw): Calculating this for every primitive is a bit
                //           wasteful. We should probably cache this in
                //           the scroll node...
                let transform_kind = scroll_node.transform.transform_kind();
                let item_bounding_rect = prim_metadata.screen_rect.as_ref().unwrap();
                let prim_cache_address = gpu_cache.get_address(&prim_metadata.gpu_location);
                let no_textures = BatchTextures::no_texture();
                let clip_task_address = prim_metadata
                    .clip_task_id
                    .map_or(OPAQUE_TASK_ADDRESS, |id| render_tasks.get_task_address(id));
                let base_instance = SimplePrimitiveInstance::new(
                    prim_cache_address,
                    task_address,
                    clip_task_address,
                    clip_id,
                    scroll_id,
                    z,
                );

                let blend_mode = ctx.prim_store.get_blend_mode(prim_metadata, transform_kind);

                match prim_metadata.prim_kind {
                    PrimitiveKind::Brush => {
                        panic!("BUG: brush type not expected in an alpha task (yet)");
                    }
                    PrimitiveKind::Border => {
                        let border_cpu =
                            &ctx.prim_store.cpu_borders[prim_metadata.cpu_prim_index.0];
                        // TODO(gw): Select correct blend mode for edges and corners!!
                        let corner_kind = BatchKind::Transformable(
                            transform_kind,
                            TransformBatchKind::BorderCorner,
                        );
                        let corner_key = BatchKey::new(corner_kind, blend_mode, no_textures);
                        let edge_kind = BatchKind::Transformable(
                            transform_kind,
                            TransformBatchKind::BorderEdge,
                        );
                        let edge_key = BatchKey::new(edge_kind, blend_mode, no_textures);

                        // Work around borrow ck on borrowing batch_list twice.
                        {
                            let batch =
                                batch_list.get_suitable_batch(corner_key, item_bounding_rect);
                            for (i, instance_kind) in border_cpu.corner_instances.iter().enumerate()
                            {
                                let sub_index = i as i32;
                                match *instance_kind {
                                    BorderCornerInstance::None => {}
                                    BorderCornerInstance::Single => {
                                        batch.push(base_instance.build(
                                            sub_index,
                                            BorderCornerSide::Both as i32,
                                            0,
                                        ));
                                    }
                                    BorderCornerInstance::Double => {
                                        batch.push(base_instance.build(
                                            sub_index,
                                            BorderCornerSide::First as i32,
                                            0,
                                        ));
                                        batch.push(base_instance.build(
                                            sub_index,
                                            BorderCornerSide::Second as i32,
                                            0,
                                        ));
                                    }
                                }
                            }
                        }

                        let batch = batch_list.get_suitable_batch(edge_key, item_bounding_rect);
                        for border_segment in 0 .. 4 {
                            batch.push(base_instance.build(border_segment, 0, 0));
                        }
                    }
                    PrimitiveKind::Rectangle => {
                        let needs_clipping = prim_metadata.clip_task_id.is_some();
                        let kind = BatchKind::Transformable(
                            transform_kind,
                            TransformBatchKind::Rectangle(needs_clipping),
                        );
                        let key = BatchKey::new(kind, blend_mode, no_textures);
                        let batch = batch_list.get_suitable_batch(key, item_bounding_rect);
                        batch.push(base_instance.build(0, 0, 0));
                    }
                    PrimitiveKind::Line => {
                        let kind =
                            BatchKind::Transformable(transform_kind, TransformBatchKind::Line);
                        let key = BatchKey::new(kind, blend_mode, no_textures);
                        let batch = batch_list.get_suitable_batch(key, item_bounding_rect);
                        batch.push(base_instance.build(0, 0, 0));
                    }
                    PrimitiveKind::Image => {
                        let image_cpu = &ctx.prim_store.cpu_images[prim_metadata.cpu_prim_index.0];

                        let (color_texture_id, uv_address) = resolve_image(
                            image_cpu.image_key,
                            image_cpu.image_rendering,
                            image_cpu.tile_offset,
                            ctx.resource_cache,
                            gpu_cache,
                            deferred_resolves,
                        );

                        if color_texture_id == SourceTexture::Invalid {
                            warn!("Warnings: skip a PrimitiveKind::Image at {:?}.\n", item_bounding_rect);
                            return;
                        }

                        let batch_kind = match color_texture_id {
                            SourceTexture::External(ext_image) => {
                                match ext_image.image_type {
                                    ExternalImageType::Texture2DHandle => {
                                        TransformBatchKind::Image(ImageBufferKind::Texture2D)
                                    }
                                    ExternalImageType::Texture2DArrayHandle => {
                                        TransformBatchKind::Image(ImageBufferKind::Texture2DArray)
                                    }
                                    ExternalImageType::TextureRectHandle => {
                                        TransformBatchKind::Image(ImageBufferKind::TextureRect)
                                    }
                                    ExternalImageType::TextureExternalHandle => {
                                        TransformBatchKind::Image(ImageBufferKind::TextureExternal)
                                    }
                                    ExternalImageType::ExternalBuffer => {
                                        // The ExternalImageType::ExternalBuffer should be handled by resource_cache.
                                        // It should go through the non-external case.
                                        panic!(
                                            "Non-texture handle type should be handled in other way"
                                        );
                                    }
                                }
                            }
                            _ => TransformBatchKind::Image(ImageBufferKind::Texture2DArray),
                        };

                        let textures = BatchTextures {
                            colors: [
                                color_texture_id,
                                SourceTexture::Invalid,
                                SourceTexture::Invalid,
                            ],
                        };

                        let key = BatchKey::new(
                            BatchKind::Transformable(transform_kind, batch_kind),
                            blend_mode,
                            textures,
                        );
                        let batch = batch_list.get_suitable_batch(key, item_bounding_rect);
                        batch.push(base_instance.build(uv_address.as_int(gpu_cache), 0, 0));
                    }
                    PrimitiveKind::TextRun => {
                        let text_cpu =
                            &ctx.prim_store.cpu_text_runs[prim_metadata.cpu_prim_index.0];

                        let font = text_cpu.get_font(ctx.device_pixel_ratio);

                        ctx.resource_cache.fetch_glyphs(
                            font,
                            &text_cpu.glyph_keys,
                            glyph_fetch_buffer,
                            gpu_cache,
                            |texture_id, glyph_format, glyphs| {
                                debug_assert_ne!(texture_id, SourceTexture::Invalid);

                                let textures = BatchTextures {
                                    colors: [
                                        texture_id,
                                        SourceTexture::Invalid,
                                        SourceTexture::Invalid,
                                    ],
                                };

                                let kind = BatchKind::Transformable(
                                    transform_kind,
                                    TransformBatchKind::TextRun(glyph_format),
                                );

                                let key = BatchKey::new(kind, blend_mode, textures);
                                let batch = batch_list.get_suitable_batch(key, item_bounding_rect);

                                for glyph in glyphs {
                                    batch.push(base_instance.build(
                                        glyph.index_in_text_run,
                                        glyph.uv_rect_address.as_int(),
                                        0,
                                    ));
                                }
                            },
                        );
                    }
                    PrimitiveKind::Picture => {
                        let picture =
                            &ctx.prim_store.cpu_pictures[prim_metadata.cpu_prim_index.0];
                        let cache_task_id = picture.render_task_id.expect("no render task!");
                        let cache_task_address = render_tasks.get_task_address(cache_task_id);
                        let textures = BatchTextures::render_target_cache();
                        let kind = BatchKind::Brush(
                            BrushBatchKind::Image(picture.target_kind()),
                        );
                        let key = BatchKey::new(kind, blend_mode, textures);
                        let batch = batch_list.get_suitable_batch(key, item_bounding_rect);
                        let image_kind = match picture.kind {
                            PictureKind::TextShadow { .. } => {
                                BrushImageKind::Simple
                            }
                            PictureKind::BoxShadow { radii_kind, .. } => {
                                match radii_kind {
                                    BorderRadiusKind::Uniform => {
                                        BrushImageKind::Mirror
                                    }
                                    BorderRadiusKind::NonUniform => {
                                        BrushImageKind::NinePatch
                                    }
                                }
                            }
                        };
                        let instance = BrushInstance {
                            picture_address: task_address,
                            prim_address: prim_cache_address,
                            clip_id,
                            scroll_id,
                            clip_task_address,
                            z,
                            flags: 0,
                            user_data0: cache_task_address.0 as i32,
                            user_data1: image_kind as i32,
                        };
                        batch.push(PrimitiveInstance::from(instance));
                    }
                    PrimitiveKind::AlignedGradient => {
                        let gradient_cpu =
                            &ctx.prim_store.cpu_gradients[prim_metadata.cpu_prim_index.0];
                        let kind = BatchKind::Transformable(
                            transform_kind,
                            TransformBatchKind::AlignedGradient,
                        );
                        let key = BatchKey::new(kind, blend_mode, no_textures);
                        let batch = batch_list.get_suitable_batch(key, item_bounding_rect);
                        for part_index in 0 .. (gradient_cpu.stops_count - 1) {
                            batch.push(base_instance.build(part_index as i32, 0, 0));
                        }
                    }
                    PrimitiveKind::AngleGradient => {
                        let kind = BatchKind::Transformable(
                            transform_kind,
                            TransformBatchKind::AngleGradient,
                        );
                        let key = BatchKey::new(kind, blend_mode, no_textures);
                        let batch = batch_list.get_suitable_batch(key, item_bounding_rect);
                        batch.push(base_instance.build(0, 0, 0));
                    }
                    PrimitiveKind::RadialGradient => {
                        let kind = BatchKind::Transformable(
                            transform_kind,
                            TransformBatchKind::RadialGradient,
                        );
                        let key = BatchKey::new(kind, blend_mode, no_textures);
                        let batch = batch_list.get_suitable_batch(key, item_bounding_rect);
                        batch.push(base_instance.build(0, 0, 0));
                    }
                    PrimitiveKind::YuvImage => {
                        let mut textures = BatchTextures::no_texture();
                        let mut uv_rect_addresses = [0; 3];
                        let image_yuv_cpu =
                            &ctx.prim_store.cpu_yuv_images[prim_metadata.cpu_prim_index.0];

                        //yuv channel
                        let channel_count = image_yuv_cpu.format.get_plane_num();
                        debug_assert!(channel_count <= 3);
                        for channel in 0 .. channel_count {
                            let image_key = image_yuv_cpu.yuv_key[channel];

                            let (texture, address) = resolve_image(
                                image_key,
                                image_yuv_cpu.image_rendering,
                                None,
                                ctx.resource_cache,
                                gpu_cache,
                                deferred_resolves,
                            );

                            if texture == SourceTexture::Invalid {
                                warn!("Warnings: skip a PrimitiveKind::YuvImage at {:?}.\n", item_bounding_rect);
                                return;
                            }

                            textures.colors[channel] = texture;
                            uv_rect_addresses[channel] = address.as_int(gpu_cache);
                        }

                        let get_buffer_kind = |texture: SourceTexture| {
                            match texture {
                                SourceTexture::External(ext_image) => {
                                    match ext_image.image_type {
                                        ExternalImageType::Texture2DHandle => {
                                            ImageBufferKind::Texture2D
                                        }
                                        ExternalImageType::Texture2DArrayHandle => {
                                            ImageBufferKind::Texture2DArray
                                        }
                                        ExternalImageType::TextureRectHandle => {
                                            ImageBufferKind::TextureRect
                                        }
                                        ExternalImageType::TextureExternalHandle => {
                                            ImageBufferKind::TextureExternal
                                        }
                                        ExternalImageType::ExternalBuffer => {
                                            // The ExternalImageType::ExternalBuffer should be handled by resource_cache.
                                            // It should go through the non-external case.
                                            panic!("Unexpected non-texture handle type");
                                        }
                                    }
                                }
                                _ => ImageBufferKind::Texture2DArray,
                            }
                        };

                        // All yuv textures should be the same type.
                        let buffer_kind = get_buffer_kind(textures.colors[0]);
                        assert!(
                            textures.colors[1 .. image_yuv_cpu.format.get_plane_num()]
                                .iter()
                                .all(|&tid| buffer_kind == get_buffer_kind(tid))
                        );

                        let kind = BatchKind::Transformable(
                            transform_kind,
                            TransformBatchKind::YuvImage(
                                buffer_kind,
                                image_yuv_cpu.format,
                                image_yuv_cpu.color_space,
                            ),
                        );
                        let key = BatchKey::new(kind, blend_mode, textures);
                        let batch = batch_list.get_suitable_batch(key, item_bounding_rect);

                        batch.push(base_instance.build(
                            uv_rect_addresses[0],
                            uv_rect_addresses[1],
                            uv_rect_addresses[2],
                        ));
                    }
                }
            }
            AlphaRenderItem::SplitComposite(sc_index, task_id, gpu_handle, z) => {
                let key = BatchKey::new(
                    BatchKind::SplitComposite,
                    BlendMode::PremultipliedAlpha,
                    BatchTextures::no_texture(),
                );
                let stacking_context = &ctx.stacking_context_store[sc_index.0];
                let batch = batch_list.get_suitable_batch(key, &stacking_context.screen_bounds);
                let source_task_address = render_tasks.get_task_address(task_id);
                let gpu_address = gpu_handle.as_int(gpu_cache);

                let instance = CompositePrimitiveInstance::new(
                    task_address,
                    source_task_address,
                    RenderTaskAddress(0),
                    gpu_address,
                    0,
                    z,
                    0,
                    0,
                );

                batch.push(PrimitiveInstance::from(instance));
            }
        }
    }
}

impl AlphaBatcher {
    fn new() -> AlphaBatcher {
        AlphaBatcher {
            tasks: Vec::new(),
            batch_list: BatchList::new(),
            glyph_fetch_buffer: Vec::new(),
        }
    }

    fn add_task(&mut self, task_id: RenderTaskId) {
        self.tasks.push(task_id);
    }

    fn build(
        &mut self,
        ctx: &RenderTargetContext,
        gpu_cache: &mut GpuCache,
        render_tasks: &RenderTaskTree,
        deferred_resolves: &mut Vec<DeferredResolve>,
    ) {
        for task_id in &self.tasks {
            let task_id = *task_id;
            let task = render_tasks.get(task_id).as_alpha_batch();
            let task_address = render_tasks.get_task_address(task_id);

            for item in &task.items {
                item.add_to_batch(
                    &mut self.batch_list,
                    ctx,
                    gpu_cache,
                    render_tasks,
                    task_id,
                    task_address,
                    deferred_resolves,
                    &mut self.glyph_fetch_buffer,
                );
            }
        }

        self.batch_list.finalize();
    }

    pub fn is_empty(&self) -> bool {
        self.batch_list.opaque_batch_list.batches.is_empty() &&
            self.batch_list.alpha_batch_list.batches.is_empty()
    }
}

/// Batcher managing draw calls into the clip mask (in the RT cache).
#[derive(Debug)]
pub struct ClipBatcher {
    /// Rectangle draws fill up the rectangles with rounded corners.
    pub rectangles: Vec<ClipMaskInstance>,
    /// Image draws apply the image masking.
    pub images: FastHashMap<SourceTexture, Vec<ClipMaskInstance>>,
    pub border_clears: Vec<ClipMaskInstance>,
    pub borders: Vec<ClipMaskInstance>,
}

impl ClipBatcher {
    fn new() -> ClipBatcher {
        ClipBatcher {
            rectangles: Vec::new(),
            images: FastHashMap::default(),
            border_clears: Vec::new(),
            borders: Vec::new(),
        }
    }

    fn add(
        &mut self,
        task_address: RenderTaskAddress,
        clips: &[ClipWorkItem],
        coordinate_system_id: CoordinateSystemId,
        resource_cache: &ResourceCache,
        gpu_cache: &GpuCache,
        geometry_kind: MaskGeometryKind,
        clip_store: &ClipStore,
    ) {
        let mut coordinate_system_id = coordinate_system_id;
        for work_item in clips.iter() {
            let instance = ClipMaskInstance {
                render_task_address: task_address,
                scroll_node_id: work_item.scroll_node_id,
                segment: 0,
                clip_data_address: GpuCacheAddress::invalid(),
                resource_address: GpuCacheAddress::invalid(),
            };
            let info = clip_store
                .get_opt(&work_item.clip_sources)
                .expect("bug: clip handle should be valid");

            for &(ref source, ref handle) in &info.clips {
                let gpu_address = gpu_cache.get_address(handle);

                match *source {
                    ClipSource::Image(ref mask) => {
                        if let Ok(cache_item) = resource_cache.get_cached_image(mask.image, ImageRendering::Auto, None) {
                            self.images
                                .entry(cache_item.texture_id)
                                .or_insert(Vec::new())
                                .push(ClipMaskInstance {
                                    clip_data_address: gpu_address,
                                    resource_address: gpu_cache.get_address(&cache_item.uv_rect_handle),
                                    ..instance
                                });
                        } else {
                            warn!("Warnings: skip a image mask. Key:{:?} Rect::{:?}.\n", mask.image, mask.rect);
                            continue;
                        }
                    }
                    ClipSource::Rectangle(..) => {
                        if work_item.coordinate_system_id != coordinate_system_id {
                            self.rectangles.push(ClipMaskInstance {
                                clip_data_address: gpu_address,
                                segment: MaskSegment::All as i32,
                                ..instance
                            });
                            coordinate_system_id = work_item.coordinate_system_id;
                        }
                    },
                    ClipSource::RoundedRectangle(..) => match geometry_kind {
                        MaskGeometryKind::Default => {
                            self.rectangles.push(ClipMaskInstance {
                                clip_data_address: gpu_address,
                                segment: MaskSegment::All as i32,
                                ..instance
                            });
                        }
                        MaskGeometryKind::CornersOnly => {
                            self.rectangles.push(ClipMaskInstance {
                                clip_data_address: gpu_address,
                                segment: MaskSegment::TopLeftCorner as i32,
                                ..instance
                            });
                            self.rectangles.push(ClipMaskInstance {
                                clip_data_address: gpu_address,
                                segment: MaskSegment::TopRightCorner as i32,
                                ..instance
                            });
                            self.rectangles.push(ClipMaskInstance {
                                clip_data_address: gpu_address,
                                segment: MaskSegment::BottomLeftCorner as i32,
                                ..instance
                            });
                            self.rectangles.push(ClipMaskInstance {
                                clip_data_address: gpu_address,
                                segment: MaskSegment::BottomRightCorner as i32,
                                ..instance
                            });
                        }
                    },
                    ClipSource::BorderCorner(ref source) => {
                        self.border_clears.push(ClipMaskInstance {
                            clip_data_address: gpu_address,
                            segment: 0,
                            ..instance
                        });
                        for clip_index in 0 .. source.actual_clip_count {
                            self.borders.push(ClipMaskInstance {
                                clip_data_address: gpu_address,
                                segment: 1 + clip_index as i32,
                                ..instance
                            })
                        }
                    }
                }
            }
        }
    }
}

pub struct RenderTargetContext<'a> {
    pub device_pixel_ratio: f32,
    pub stacking_context_store: &'a [StackingContext],
    pub prim_store: &'a PrimitiveStore,
    pub resource_cache: &'a ResourceCache,
    pub node_data: &'a [ClipScrollNodeData],
}

struct TextureAllocator {
    // TODO(gw): Replace this with a simpler allocator for
    // render target allocation - this use case doesn't need
    // to deal with coalescing etc that the general texture
    // cache allocator requires.
    allocator: GuillotineAllocator,

    // Track the used rect of the render target, so that
    // we can set a scissor rect and only clear to the
    // used portion of the target as an optimization.
    used_rect: DeviceIntRect,
}

impl TextureAllocator {
    fn new(size: DeviceUintSize) -> TextureAllocator {
        TextureAllocator {
            allocator: GuillotineAllocator::new(size),
            used_rect: DeviceIntRect::zero(),
        }
    }

    fn allocate(&mut self, size: &DeviceUintSize) -> Option<DeviceUintPoint> {
        let origin = self.allocator.allocate(size);

        if let Some(origin) = origin {
            // TODO(gw): We need to make all the device rects
            //           be consistent in the use of the
            //           DeviceIntRect and DeviceUintRect types!
            let origin = DeviceIntPoint::new(origin.x as i32, origin.y as i32);
            let size = DeviceIntSize::new(size.width as i32, size.height as i32);
            let rect = DeviceIntRect::new(origin, size);
            self.used_rect = rect.union(&self.used_rect);
        }

        origin
    }
}

pub trait RenderTarget {
    fn new(size: Option<DeviceUintSize>) -> Self;
    fn allocate(&mut self, size: DeviceUintSize) -> Option<DeviceUintPoint>;
    fn build(
        &mut self,
        _ctx: &RenderTargetContext,
        _gpu_cache: &mut GpuCache,
        _render_tasks: &mut RenderTaskTree,
        _deferred_resolves: &mut Vec<DeferredResolve>,
    ) {
    }
    fn add_task(
        &mut self,
        task_id: RenderTaskId,
        ctx: &RenderTargetContext,
        gpu_cache: &GpuCache,
        render_tasks: &RenderTaskTree,
        clip_store: &ClipStore,
    );
    fn used_rect(&self) -> DeviceIntRect;
}

#[derive(Debug, Copy, Clone, Eq, PartialEq, Hash)]
pub enum RenderTargetKind {
    Color, // RGBA32
    Alpha, // R8
}

pub struct RenderTargetList<T> {
    pub targets: Vec<T>,
}

impl<T: RenderTarget> RenderTargetList<T> {
    fn new(create_initial_target: bool) -> RenderTargetList<T> {
        let mut targets = Vec::new();
        if create_initial_target {
            targets.push(T::new(None));
        }

        RenderTargetList { targets }
    }

    pub fn target_count(&self) -> usize {
        self.targets.len()
    }

    fn build(
        &mut self,
        ctx: &RenderTargetContext,
        gpu_cache: &mut GpuCache,
        render_tasks: &mut RenderTaskTree,
        deferred_resolves: &mut Vec<DeferredResolve>,
    ) {
        for target in &mut self.targets {
            target.build(ctx, gpu_cache, render_tasks, deferred_resolves);
        }
    }

    fn add_task(
        &mut self,
        task_id: RenderTaskId,
        ctx: &RenderTargetContext,
        gpu_cache: &GpuCache,
        render_tasks: &mut RenderTaskTree,
        clip_store: &ClipStore,
    ) {
        self.targets.last_mut().unwrap().add_task(
            task_id,
            ctx,
            gpu_cache,
            render_tasks,
            clip_store,
        );
    }

    fn allocate(
        &mut self,
        alloc_size: DeviceUintSize,
        target_size: DeviceUintSize,
    ) -> (DeviceUintPoint, RenderTargetIndex) {
        let existing_origin = self.targets
            .last_mut()
            .and_then(|target| target.allocate(alloc_size));

        let origin = match existing_origin {
            Some(origin) => origin,
            None => {
                let mut new_target = T::new(Some(target_size));
                let origin = new_target.allocate(alloc_size).expect(&format!(
                    "Each render task must allocate <= size of one target! ({:?})",
                    alloc_size
                ));
                self.targets.push(new_target);
                origin
            }
        };

        (origin, RenderTargetIndex(self.targets.len() - 1))
    }
}

/// Frame output information for a given pipeline ID.
/// Storing the task ID allows the renderer to find
/// the target rect within the render target that this
/// pipeline exists at.
pub struct FrameOutput {
    pub task_id: RenderTaskId,
    pub pipeline_id: PipelineId,
}

pub struct ScalingInfo {
    pub src_task_id: RenderTaskId,
    pub dest_task_id: RenderTaskId,
}

/// A render target represents a number of rendering operations on a surface.
pub struct ColorRenderTarget {
    pub alpha_batcher: AlphaBatcher,
    // List of text runs to be cached to this render target.
    pub text_run_cache_prims: FastHashMap<SourceTexture, Vec<PrimitiveInstance>>,
    pub line_cache_prims: Vec<PrimitiveInstance>,
    // List of blur operations to apply for this render target.
    pub vertical_blurs: Vec<BlurInstance>,
    pub horizontal_blurs: Vec<BlurInstance>,
    pub readbacks: Vec<DeviceIntRect>,
    pub scalings: Vec<ScalingInfo>,
    // List of frame buffer outputs for this render target.
    pub outputs: Vec<FrameOutput>,
    allocator: Option<TextureAllocator>,
    glyph_fetch_buffer: Vec<GlyphFetchResult>,
}

impl RenderTarget for ColorRenderTarget {
    fn allocate(&mut self, size: DeviceUintSize) -> Option<DeviceUintPoint> {
        self.allocator
            .as_mut()
            .expect("bug: calling allocate on framebuffer")
            .allocate(&size)
    }

    fn new(size: Option<DeviceUintSize>) -> ColorRenderTarget {
        ColorRenderTarget {
            alpha_batcher: AlphaBatcher::new(),
            text_run_cache_prims: FastHashMap::default(),
            line_cache_prims: Vec::new(),
            vertical_blurs: Vec::new(),
            horizontal_blurs: Vec::new(),
            readbacks: Vec::new(),
            scalings: Vec::new(),
            allocator: size.map(|size| TextureAllocator::new(size)),
            glyph_fetch_buffer: Vec::new(),
            outputs: Vec::new(),
        }
    }

    fn used_rect(&self) -> DeviceIntRect {
        self.allocator
            .as_ref()
            .expect("bug: used_rect called on framebuffer")
            .used_rect
    }

    fn build(
        &mut self,
        ctx: &RenderTargetContext,
        gpu_cache: &mut GpuCache,
        render_tasks: &mut RenderTaskTree,
        deferred_resolves: &mut Vec<DeferredResolve>,
    ) {
        self.alpha_batcher
            .build(ctx, gpu_cache, render_tasks, deferred_resolves);
    }

    fn add_task(
        &mut self,
        task_id: RenderTaskId,
        ctx: &RenderTargetContext,
        gpu_cache: &GpuCache,
        render_tasks: &RenderTaskTree,
        _: &ClipStore,
    ) {
        let task = render_tasks.get(task_id);

        match task.kind {
            RenderTaskKind::Alias(..) => {
                panic!("BUG: add_task() called on invalidated task");
            }
            RenderTaskKind::Alpha(ref info) => {
                self.alpha_batcher.add_task(task_id);

                // If this pipeline is registered as a frame output
                // store the information necessary to do the copy.
                if let Some(pipeline_id) = info.frame_output_pipeline_id {
                    self.outputs.push(FrameOutput {
                        pipeline_id,
                        task_id,
                    });
                }
            }
            RenderTaskKind::VerticalBlur(ref info) => {
                info.add_instances(
                    &mut self.vertical_blurs,
                    task_id,
                    task.children[0],
                    BlurDirection::Vertical,
                    render_tasks,
                );
            }
            RenderTaskKind::HorizontalBlur(ref info) => {
                info.add_instances(
                    &mut self.horizontal_blurs,
                    task_id,
                    task.children[0],
                    BlurDirection::Horizontal,
                    render_tasks,
                );
            }
            RenderTaskKind::Picture(ref task_info) => {
                let prim_metadata = ctx.prim_store.get_metadata(task_info.prim_index);
                match prim_metadata.prim_kind {
                    PrimitiveKind::Picture => {
                        let prim = &ctx.prim_store.cpu_pictures[prim_metadata.cpu_prim_index.0];

                        let task_index = render_tasks.get_task_address(task_id);

                        for run in &prim.prim_runs {
                            for i in 0 .. run.count {
                                let sub_prim_index = PrimitiveIndex(run.prim_index.0 + i);

                                let sub_metadata = ctx.prim_store.get_metadata(sub_prim_index);
                                let sub_prim_address =
                                    gpu_cache.get_address(&sub_metadata.gpu_location);
                                let instance = SimplePrimitiveInstance::new(
                                    sub_prim_address,
                                    task_index,
                                    RenderTaskAddress(0),
                                    ClipScrollNodeIndex(0),
                                    ClipScrollNodeIndex(0),
                                    0,
                                ); // z is disabled for rendering cache primitives

                                match sub_metadata.prim_kind {
                                    PrimitiveKind::TextRun => {
                                        // Add instances that reference the text run GPU location. Also supply
                                        // the parent shadow prim address as a user data field, allowing
                                        // the shader to fetch the shadow parameters.
                                        let text = &ctx.prim_store.cpu_text_runs
                                            [sub_metadata.cpu_prim_index.0];
                                        let text_run_cache_prims = &mut self.text_run_cache_prims;

                                        let font = text.get_font(ctx.device_pixel_ratio);

                                        ctx.resource_cache.fetch_glyphs(
                                            font,
                                            &text.glyph_keys,
                                            &mut self.glyph_fetch_buffer,
                                            gpu_cache,
                                            |texture_id, _glyph_format, glyphs| {
                                                let batch = text_run_cache_prims
                                                    .entry(texture_id)
                                                    .or_insert(Vec::new());

                                                for glyph in glyphs {
                                                    batch.push(instance.build(
                                                        glyph.index_in_text_run,
                                                        glyph.uv_rect_address.as_int(),
                                                        0
                                                    ));
                                                }
                                            },
                                        );
                                    }
                                    PrimitiveKind::Line => {
                                        self.line_cache_prims
                                            .push(instance.build(0, 0, 0));
                                    }
                                    _ => {
                                        unreachable!("Unexpected sub primitive type");
                                    }
                                }
                            }
                        }
                    }
                    _ => {
                        // No other primitives make use of primitive caching yet!
                        unreachable!()
                    }
                }
            }
            RenderTaskKind::CacheMask(..) => {
                panic!("Should not be added to color target!");
            }
            RenderTaskKind::Readback(device_rect) => {
                self.readbacks.push(device_rect);
            }
            RenderTaskKind::Scaling(..) => {
                self.scalings.push(ScalingInfo {
                    src_task_id: task.children[0],
                    dest_task_id: task_id,
                });
            }
        }
    }
}

pub struct AlphaRenderTarget {
    pub clip_batcher: ClipBatcher,
    pub brush_mask_corners: Vec<PrimitiveInstance>,
    pub brush_mask_rounded_rects: Vec<PrimitiveInstance>,
    // List of blur operations to apply for this render target.
    pub vertical_blurs: Vec<BlurInstance>,
    pub horizontal_blurs: Vec<BlurInstance>,
    pub scalings: Vec<ScalingInfo>,
    pub zero_clears: Vec<RenderTaskId>,
    allocator: TextureAllocator,
}

impl RenderTarget for AlphaRenderTarget {
    fn allocate(&mut self, size: DeviceUintSize) -> Option<DeviceUintPoint> {
        self.allocator.allocate(&size)
    }

    fn new(size: Option<DeviceUintSize>) -> AlphaRenderTarget {
        AlphaRenderTarget {
            clip_batcher: ClipBatcher::new(),
            brush_mask_corners: Vec::new(),
            brush_mask_rounded_rects: Vec::new(),
            vertical_blurs: Vec::new(),
            horizontal_blurs: Vec::new(),
            scalings: Vec::new(),
            zero_clears: Vec::new(),
            allocator: TextureAllocator::new(size.expect("bug: alpha targets need size")),
        }
    }

    fn used_rect(&self) -> DeviceIntRect {
        self.allocator.used_rect
    }

    fn add_task(
        &mut self,
        task_id: RenderTaskId,
        ctx: &RenderTargetContext,
        gpu_cache: &GpuCache,
        render_tasks: &RenderTaskTree,
        clip_store: &ClipStore,
    ) {
        let task = render_tasks.get(task_id);

        match task.clear_mode {
            ClearMode::Zero => {
                self.zero_clears.push(task_id);
            }
            ClearMode::One => {}
            ClearMode::Transparent => {
                panic!("bug: invalid clear mode for alpha task");
            }
        }

        match task.kind {
            RenderTaskKind::Alias(..) => {
                panic!("BUG: add_task() called on invalidated task");
            }
            RenderTaskKind::Alpha(..) |
            RenderTaskKind::Readback(..) => {
                panic!("Should not be added to alpha target!");
            }
            RenderTaskKind::VerticalBlur(ref info) => {
                info.add_instances(
                    &mut self.vertical_blurs,
                    task_id,
                    task.children[0],
                    BlurDirection::Vertical,
                    render_tasks,
                );
            }
            RenderTaskKind::HorizontalBlur(ref info) => {
                info.add_instances(
                    &mut self.horizontal_blurs,
                    task_id,
                    task.children[0],
                    BlurDirection::Horizontal,
                    render_tasks,
                );
            }
            RenderTaskKind::Picture(ref task_info) => {
                let prim_metadata = ctx.prim_store.get_metadata(task_info.prim_index);

                match prim_metadata.prim_kind {
                    PrimitiveKind::Picture => {
                        let prim = &ctx.prim_store.cpu_pictures[prim_metadata.cpu_prim_index.0];

                        let task_index = render_tasks.get_task_address(task_id);

                        for run in &prim.prim_runs {
                            for i in 0 .. run.count {
                                let sub_prim_index = PrimitiveIndex(run.prim_index.0 + i);

                                let sub_metadata = ctx.prim_store.get_metadata(sub_prim_index);
                                let sub_prim_address =
                                    gpu_cache.get_address(&sub_metadata.gpu_location);

                                match sub_metadata.prim_kind {
                                    PrimitiveKind::Brush => {
                                        let instance = BrushInstance {
                                            picture_address: task_index,
                                            prim_address: sub_prim_address,
                                            // TODO(gw): In the future, when brush
                                            //           primitives on picture backed
                                            //           tasks support clip masks and
                                            //           transform primitives, these
                                            //           will need to be filled out!
                                            clip_id: ClipScrollNodeIndex(0),
                                            scroll_id: ClipScrollNodeIndex(0),
                                            clip_task_address: RenderTaskAddress(0),
                                            z: 0,
                                            flags: BRUSH_FLAG_USES_PICTURE,
                                            user_data0: 0,
                                            user_data1: 0,
                                        };
                                        let brush = &ctx.prim_store.cpu_brushes[sub_metadata.cpu_prim_index.0];
                                        let batch = match brush.kind {
                                            BrushKind::Mask { ref kind, .. } => {
                                                match *kind {
                                                    BrushMaskKind::Corner(..) => &mut self.brush_mask_corners,
                                                    BrushMaskKind::RoundedRect(..) => &mut self.brush_mask_rounded_rects,
                                                }
                                            }
                                        };
                                        batch.push(PrimitiveInstance::from(instance));
                                    }
                                    _ => {
                                        unreachable!("Unexpected sub primitive type");
                                    }
                                }
                            }
                        }
                    }
                    _ => {
                        // No other primitives make use of primitive caching yet!
                        unreachable!()
                    }
                }
            }
            RenderTaskKind::CacheMask(ref task_info) => {
                let task_address = render_tasks.get_task_address(task_id);
                self.clip_batcher.add(
                    task_address,
                    &task_info.clips,
                    task_info.coordinate_system_id,
                    &ctx.resource_cache,
                    gpu_cache,
                    task_info.geometry_kind,
                    clip_store,
                );
            }
            RenderTaskKind::Scaling(..) => {
                self.scalings.push(ScalingInfo {
                    src_task_id: task.children[0],
                    dest_task_id: task_id,
                });
            }
        }
    }
}

/// A render pass represents a set of rendering operations that don't depend on one
/// another.
///
/// A render pass can have several render targets if there wasn't enough space in one
/// target to do all of the rendering for that pass.
pub struct RenderPass {
    pub is_framebuffer: bool,
    tasks: Vec<RenderTaskId>,
    pub color_targets: RenderTargetList<ColorRenderTarget>,
    pub alpha_targets: RenderTargetList<AlphaRenderTarget>,
    pub color_texture: Option<Texture>,
    pub alpha_texture: Option<Texture>,
    dynamic_tasks: FastHashMap<RenderTaskKey, DynamicTaskInfo>,
    pub max_color_target_size: DeviceUintSize,
    pub max_alpha_target_size: DeviceUintSize,
}

impl RenderPass {
    pub fn new(is_framebuffer: bool) -> RenderPass {
        RenderPass {
            is_framebuffer,
            color_targets: RenderTargetList::new(is_framebuffer),
            alpha_targets: RenderTargetList::new(false),
            tasks: vec![],
            color_texture: None,
            alpha_texture: None,
            dynamic_tasks: FastHashMap::default(),
            max_color_target_size: DeviceUintSize::new(MIN_TARGET_SIZE, MIN_TARGET_SIZE),
            max_alpha_target_size: DeviceUintSize::new(MIN_TARGET_SIZE, MIN_TARGET_SIZE),
        }
    }

    pub fn add_render_task(
        &mut self,
        task_id: RenderTaskId,
        size: DeviceIntSize,
        target_kind: RenderTargetKind,
    ) {
        match target_kind {
            RenderTargetKind::Color => {
                self.max_color_target_size.width =
                    cmp::max(self.max_color_target_size.width, size.width as u32);
                self.max_color_target_size.height =
                    cmp::max(self.max_color_target_size.height, size.height as u32);
            }
            RenderTargetKind::Alpha => {
                self.max_alpha_target_size.width =
                    cmp::max(self.max_alpha_target_size.width, size.width as u32);
                self.max_alpha_target_size.height =
                    cmp::max(self.max_alpha_target_size.height, size.height as u32);
            }
        }

        self.tasks.push(task_id);
    }

    pub fn needs_render_target_kind(&self, kind: RenderTargetKind) -> bool {
        if self.is_framebuffer {
            false
        } else {
            self.required_target_count(kind) > 0
        }
    }

    pub fn required_target_count(&self, kind: RenderTargetKind) -> usize {
        match kind {
            RenderTargetKind::Color => self.color_targets.target_count(),
            RenderTargetKind::Alpha => self.alpha_targets.target_count(),
        }
    }

    pub fn build(
        &mut self,
        ctx: &RenderTargetContext,
        gpu_cache: &mut GpuCache,
        render_tasks: &mut RenderTaskTree,
        deferred_resolves: &mut Vec<DeferredResolve>,
        clip_store: &ClipStore,
    ) {
        profile_scope!("RenderPass::build");

        // Step through each task, adding to batches as appropriate.
        for task_id in &self.tasks {
            let task_id = *task_id;

            let target_kind = {
                let task = render_tasks.get_mut(task_id);
                let target_kind = task.target_kind();

                // Find a target to assign this task to, or create a new
                // one if required.
                match task.location {
                    RenderTaskLocation::Fixed => {}
                    RenderTaskLocation::Dynamic(_, size) => {
                        if let Some(cache_key) = task.cache_key {
                            // See if this task is a duplicate.
                            // If so, just skip adding it!
                            if let Some(task_info) = self.dynamic_tasks.get(&cache_key) {
                                task.set_alias(task_info.task_id);
                                debug_assert_eq!(task_info.rect.size, size);
                                continue;
                            }
                        }

                        let alloc_size = DeviceUintSize::new(size.width as u32, size.height as u32);
                        let (alloc_origin, target_index) = match target_kind {
                            RenderTargetKind::Color => self.color_targets
                                .allocate(alloc_size, self.max_color_target_size),
                            RenderTargetKind::Alpha => self.alpha_targets
                                .allocate(alloc_size, self.max_alpha_target_size),
                        };

                        let origin = Some((
                            DeviceIntPoint::new(alloc_origin.x as i32, alloc_origin.y as i32),
                            target_index,
                        ));
                        task.location = RenderTaskLocation::Dynamic(origin, size);

                        // If this task is cacheable / sharable, store it in the task hash
                        // for this pass.
                        if let Some(cache_key) = task.cache_key {
                            self.dynamic_tasks.insert(
                                cache_key,
                                DynamicTaskInfo {
                                    task_id,
                                    rect: match task.location {
                                        RenderTaskLocation::Fixed => {
                                            panic!("Dynamic tasks should not have fixed locations!")
                                        }
                                        RenderTaskLocation::Dynamic(Some((origin, _)), size) => {
                                            DeviceIntRect::new(origin, size)
                                        }
                                        RenderTaskLocation::Dynamic(None, _) => {
                                            panic!("Expect the task to be already allocated here")
                                        }
                                    },
                                },
                            );
                        }
                    }
                }

                target_kind
            };

            match target_kind {
                RenderTargetKind::Color => {
                    self.color_targets
                        .add_task(task_id, ctx, gpu_cache, render_tasks, clip_store)
                }
                RenderTargetKind::Alpha => {
                    self.alpha_targets
                        .add_task(task_id, ctx, gpu_cache, render_tasks, clip_store)
                }
            }
        }

        self.color_targets
            .build(ctx, gpu_cache, render_tasks, deferred_resolves);
        self.alpha_targets
            .build(ctx, gpu_cache, render_tasks, deferred_resolves);
    }
}

#[derive(Copy, Clone, PartialEq, Eq, Hash, Debug)]
pub enum TransformBatchKind {
    Rectangle(bool),
    TextRun(GlyphFormat),
    Image(ImageBufferKind),
    YuvImage(ImageBufferKind, YuvFormat, YuvColorSpace),
    AlignedGradient,
    AngleGradient,
    RadialGradient,
    BorderCorner,
    BorderEdge,
    Line,
}

#[derive(Copy, Clone, PartialEq, Eq, Hash, Debug)]
pub enum BrushBatchKind {
    Image(RenderTargetKind)
}

#[derive(Copy, Clone, PartialEq, Eq, Hash, Debug)]
pub enum BatchKind {
    Composite {
        task_id: RenderTaskId,
        source_id: RenderTaskId,
        backdrop_id: RenderTaskId,
    },
    HardwareComposite,
    SplitComposite,
    Blend,
    Transformable(TransformedRectKind, TransformBatchKind),
    Brush(BrushBatchKind),
}

#[derive(Copy, Clone, Debug)]
pub struct BatchKey {
    pub kind: BatchKind,
    pub blend_mode: BlendMode,
    pub textures: BatchTextures,
}

impl BatchKey {
    fn new(kind: BatchKind, blend_mode: BlendMode, textures: BatchTextures) -> BatchKey {
        BatchKey {
            kind,
            blend_mode,
            textures,
        }
    }

    fn is_compatible_with(&self, other: &BatchKey) -> bool {
        self.kind == other.kind && self.blend_mode == other.blend_mode &&
            textures_compatible(self.textures.colors[0], other.textures.colors[0]) &&
            textures_compatible(self.textures.colors[1], other.textures.colors[1]) &&
            textures_compatible(self.textures.colors[2], other.textures.colors[2])
    }
}

#[inline]
fn textures_compatible(t1: SourceTexture, t2: SourceTexture) -> bool {
    t1 == SourceTexture::Invalid || t2 == SourceTexture::Invalid || t1 == t2
}

#[derive(Debug)]
pub struct AlphaPrimitiveBatch {
    pub key: BatchKey,
    pub instances: Vec<PrimitiveInstance>,
    pub item_rects: Vec<DeviceIntRect>,
}

impl AlphaPrimitiveBatch {
    fn new(key: BatchKey) -> AlphaPrimitiveBatch {
        AlphaPrimitiveBatch {
            key,
            instances: Vec::new(),
            item_rects: Vec::new(),
        }
    }
}

#[derive(Debug)]
pub struct OpaquePrimitiveBatch {
    pub key: BatchKey,
    pub instances: Vec<PrimitiveInstance>,
}

impl OpaquePrimitiveBatch {
    fn new(key: BatchKey) -> OpaquePrimitiveBatch {
        OpaquePrimitiveBatch {
            key,
            instances: Vec::new(),
        }
    }
}

#[derive(Debug, Copy, Clone, Eq, PartialEq, Hash, Ord, PartialOrd)]
pub struct StackingContextIndex(pub usize);

#[derive(Debug, Copy, Clone, Eq, PartialEq, Hash, Ord, PartialOrd)]
pub enum ContextIsolation {
    /// No isolation - the content is mixed up with everything else.
    None,
    /// Items are isolated and drawn into a separate render target.
    /// Child contexts are exposed.
    Items,
    /// All the content inside is isolated and drawn into a separate target.
    Full,
}

#[derive(Debug)]
pub struct StackingContext {
    pub pipeline_id: PipelineId,

    /// Offset in the parent reference frame to the origin of this stacking
    /// context's coordinate system.
    pub reference_frame_offset: LayerVector2D,

    /// The `ClipId` of the owning reference frame.
    pub reference_frame_id: ClipId,

    /// Screen space bounding rectangle for this stacking context,
    /// calculated based on the size and position of all its children.
    pub screen_bounds: DeviceIntRect,

    /// Local bounding rectangle of this stacking context,
    /// computed as the union of all contained items that are not
    /// `ContextIsolation::Items` on their own
    pub isolated_items_bounds: LayerRect,

    pub composite_ops: CompositeOps,

    /// Type of the isolation of the content.
    pub isolation: ContextIsolation,

    /// Set for the root stacking context of a display list or an iframe. Used for determining
    /// when to isolate a mix-blend-mode composite.
    pub is_page_root: bool,

    /// Set to true if this is the root stacking context for a pipeline.
    pub is_pipeline_root: bool,

    /// Whether or not this stacking context has any visible components, calculated
    /// based on the size and position of all children and how they are clipped.
    pub is_visible: bool,

    /// Current stacking context visibility of backface.
    pub is_backface_visible: bool,

    /// Allow subpixel AA for text runs on this stacking context.
    /// This is a temporary hack while we don't support subpixel AA
    /// on transparent stacking contexts.
    pub allow_subpixel_aa: bool,

    /// Indicate that if any pritimive contained in this stacking context.
    pub has_any_primitive: bool,

    /// Union of all stacking context bounds of all children.
    pub children_sc_bounds: LayerRect,
}

impl StackingContext {
    pub fn new(
        pipeline_id: PipelineId,
        reference_frame_offset: LayerVector2D,
        is_page_root: bool,
        is_pipeline_root: bool,
        reference_frame_id: ClipId,
        transform_style: TransformStyle,
        composite_ops: CompositeOps,
        is_backface_visible: bool,
    ) -> StackingContext {
        let isolation = match transform_style {
            TransformStyle::Flat => ContextIsolation::None,
            TransformStyle::Preserve3D => ContextIsolation::Items,
        };
        let allow_subpixel_aa = composite_ops.count() == 0 &&
                                isolation == ContextIsolation::None;
        StackingContext {
            pipeline_id,
            reference_frame_offset,
            reference_frame_id,
            screen_bounds: DeviceIntRect::zero(),
            isolated_items_bounds: LayerRect::zero(),
            composite_ops,
            isolation,
            is_page_root,
            is_pipeline_root,
            is_visible: false,
            is_backface_visible,
            allow_subpixel_aa,
            has_any_primitive: false,
            children_sc_bounds: LayerRect::zero(),
        }
    }

    pub fn can_contribute_to_scene(&self) -> bool {
        !self.composite_ops.will_make_invisible()
    }
}

#[derive(Debug, Clone, Default)]
pub struct CompositeOps {
    // Requires only a single texture as input (e.g. most filters)
    pub filters: Vec<FilterOp>,

    // Requires two source textures (e.g. mix-blend-mode)
    pub mix_blend_mode: Option<MixBlendMode>,
}

impl CompositeOps {
    pub fn new(filters: Vec<FilterOp>, mix_blend_mode: Option<MixBlendMode>) -> CompositeOps {
        CompositeOps {
            filters,
            mix_blend_mode: mix_blend_mode,
        }
    }

    pub fn count(&self) -> usize {
        self.filters.len() + if self.mix_blend_mode.is_some() { 1 } else { 0 }
    }

    pub fn will_make_invisible(&self) -> bool {
        for op in &self.filters {
            if op == &FilterOp::Opacity(PropertyBinding::Value(0.0)) {
                return true;
            }
        }
        false
    }
}

/// A rendering-oriented representation of frame::Frame built by the render backend
/// and presented to the renderer.
pub struct Frame {
    pub window_size: DeviceUintSize,
    pub background_color: Option<ColorF>,
    pub device_pixel_ratio: f32,
    pub passes: Vec<RenderPass>,
    pub profile_counters: FrameProfileCounters,

    pub node_data: Vec<ClipScrollNodeData>,
    pub render_tasks: RenderTaskTree,

    // List of updates that need to be pushed to the
    // gpu resource cache.
    pub gpu_cache_updates: Option<GpuCacheUpdateList>,

    // List of textures that we don't know about yet
    // from the backend thread. The render thread
    // will use a callback to resolve these and
    // patch the data structures.
    pub deferred_resolves: Vec<DeferredResolve>,
}

fn resolve_image(
    image_key: ImageKey,
    image_rendering: ImageRendering,
    tile_offset: Option<TileOffset>,
    resource_cache: &ResourceCache,
    gpu_cache: &mut GpuCache,
    deferred_resolves: &mut Vec<DeferredResolve>,
) -> (SourceTexture, GpuCacheHandle) {
    match resource_cache.get_image_properties(image_key) {
        Some(image_properties) => {
            // Check if an external image that needs to be resolved
            // by the render thread.
            match image_properties.external_image {
                Some(external_image) => {
                    // This is an external texture - we will add it to
                    // the deferred resolves list to be patched by
                    // the render thread...
                    let cache_handle = gpu_cache.push_deferred_per_frame_blocks(1);
                    deferred_resolves.push(DeferredResolve {
                        image_properties,
                        address: gpu_cache.get_address(&cache_handle),
                    });

                    (SourceTexture::External(external_image), cache_handle)
                }
                None => {
                    if let Ok(cache_item) = resource_cache.get_cached_image(image_key, image_rendering, tile_offset) {
                        (cache_item.texture_id, cache_item.uv_rect_handle)
                    } else {
                        // There is no usable texture entry for the image key. Just return an invalid texture here.
                        (SourceTexture::Invalid, GpuCacheHandle::new())
                    }
                }
            }
        }
        None => (SourceTexture::Invalid, GpuCacheHandle::new()),
    }
}

impl BlurTask {
    fn add_instances(
        &self,
        instances: &mut Vec<BlurInstance>,
        task_id: RenderTaskId,
        source_task_id: RenderTaskId,
        blur_direction: BlurDirection,
        render_tasks: &RenderTaskTree,
    ) {
        let instance = BlurInstance {
            task_address: render_tasks.get_task_address(task_id),
            src_task_address: render_tasks.get_task_address(source_task_id),
            blur_direction,
            region: LayerRect::zero(),
        };

        if self.regions.is_empty() {
            instances.push(instance);
        } else {
            for region in &self.regions {
                instances.push(BlurInstance {
                    region: *region,
                    ..instance
                });
            }
        }
    }
}
