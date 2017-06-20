/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use app_units::Au;
use border::{BorderCornerInstance, BorderCornerSide};
use device::TextureId;
use fnv::FnvHasher;
use gpu_cache::{GpuCache, GpuCacheHandle, GpuCacheUpdateList};
use gpu_store::GpuStoreAddress;
use internal_types::{ANGLE_FLOAT_TO_FIXED, BatchTextures, CacheTextureId, LowLevelFilterOp};
use internal_types::SourceTexture;
use mask_cache::MaskCacheInfo;
use prim_store::{CLIP_DATA_GPU_SIZE, DeferredResolve, GpuBlock32};
use prim_store::{ImagePrimitiveKind, PrimitiveCacheKey};
use prim_store::{PrimitiveIndex, PrimitiveKind, PrimitiveMetadata, PrimitiveStore};
use profiler::FrameProfileCounters;
use render_task::{AlphaRenderItem, MaskGeometryKind, MaskSegment, RenderTask, RenderTaskData};
use render_task::{RenderTaskId, RenderTaskIndex, RenderTaskKey, RenderTaskKind};
use render_task::RenderTaskLocation;
use renderer::BlendMode;
use renderer::ImageBufferKind;
use resource_cache::ResourceCache;
use std::{f32, i32, mem, usize};
use std::collections::HashMap;
use std::hash::BuildHasherDefault;
use texture_cache::TexturePage;
use util::{TransformedRect, TransformedRectKind};
use webrender_traits::{BuiltDisplayList, ClipAndScrollInfo, ClipId, ColorF, DeviceIntPoint, ImageKey};
use webrender_traits::{DeviceIntRect, DeviceIntSize, DeviceUintPoint, DeviceUintSize};
use webrender_traits::{ExternalImageType, FontRenderMode, ImageRendering, LayerRect};
use webrender_traits::{LayerToWorldTransform, MixBlendMode, PipelineId, TransformStyle};
use webrender_traits::{TileOffset, WorldToLayerTransform, YuvColorSpace, YuvFormat, LayerVector2D};

// Special sentinel value recognized by the shader. It is considered to be
// a dummy task that doesn't mask out anything.
const OPAQUE_TASK_INDEX: RenderTaskIndex = RenderTaskIndex(i32::MAX as usize);


pub type DisplayListMap = HashMap<PipelineId,
                                  BuiltDisplayList,
                                  BuildHasherDefault<FnvHasher>>;

trait AlphaBatchHelpers {
    fn get_blend_mode(&self,
                      needs_blending: bool,
                      metadata: &PrimitiveMetadata) -> BlendMode;
}

impl AlphaBatchHelpers for PrimitiveStore {
    fn get_blend_mode(&self, needs_blending: bool, metadata: &PrimitiveMetadata) -> BlendMode {
        match metadata.prim_kind {
            PrimitiveKind::TextRun => {
                let text_run_cpu = &self.cpu_text_runs[metadata.cpu_prim_index.0];
                if text_run_cpu.blur_radius == 0.0 {
                    match text_run_cpu.render_mode {
                        FontRenderMode::Subpixel => BlendMode::Subpixel(text_run_cpu.color),
                        FontRenderMode::Alpha | FontRenderMode::Mono => BlendMode::Alpha,
                    }
                } else {
                    // Text runs drawn to blur never get drawn with subpixel AA.
                    BlendMode::Alpha
                }
            }
            PrimitiveKind::Image |
            PrimitiveKind::AlignedGradient |
            PrimitiveKind::AngleGradient |
            PrimitiveKind::RadialGradient => {
                if needs_blending {
                    BlendMode::PremultipliedAlpha
                } else {
                    BlendMode::None
                }
            }
            _ => {
                if needs_blending {
                    BlendMode::Alpha
                } else {
                    BlendMode::None
                }
            }
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
    Scrollbar(ClipId, f32)
}

#[derive(Debug, Copy, Clone)]
pub struct RenderTargetIndex(pub usize);

#[derive(Debug, Copy, Clone, Eq, PartialEq, Hash)]
pub struct RenderPassIndex(isize);

struct DynamicTaskInfo {
    index: RenderTaskIndex,
    rect: DeviceIntRect,
}

pub struct RenderTaskCollection {
    pub render_task_data: Vec<RenderTaskData>,
    dynamic_tasks: HashMap<(RenderTaskKey, RenderPassIndex), DynamicTaskInfo, BuildHasherDefault<FnvHasher>>,
}

impl RenderTaskCollection {
    pub fn new(static_render_task_count: usize) -> RenderTaskCollection {
        RenderTaskCollection {
            render_task_data: vec![RenderTaskData::empty(); static_render_task_count],
            dynamic_tasks: HashMap::default(),
        }
    }

    fn add(&mut self, task: &RenderTask, pass: RenderPassIndex) -> RenderTaskIndex {
        match task.id {
            RenderTaskId::Static(index) => {
                self.render_task_data[index.0] = task.write_task_data();
                index
            }
            RenderTaskId::Dynamic(key) => {
                let index = RenderTaskIndex(self.render_task_data.len());
                let key = (key, pass);
                debug_assert!(self.dynamic_tasks.contains_key(&key) == false);
                self.dynamic_tasks.insert(key, DynamicTaskInfo {
                    index: index,
                    rect: match task.location {
                        RenderTaskLocation::Fixed => panic!("Dynamic tasks should not have fixed locations!"),
                        RenderTaskLocation::Dynamic(Some((origin, _)), size) => DeviceIntRect::new(origin, size),
                        RenderTaskLocation::Dynamic(None, _) => panic!("Expect the task to be already allocated here"),
                    },
                });
                self.render_task_data.push(task.write_task_data());
                index
            }
        }
    }

    fn get_dynamic_allocation(&self, pass_index: RenderPassIndex, key: RenderTaskKey) -> Option<&DeviceIntRect> {
        let key = (key, pass_index);
        self.dynamic_tasks.get(&key)
                          .map(|task| &task.rect)
    }

    fn get_static_task_index(&self, id: &RenderTaskId) -> RenderTaskIndex {
        match id {
            &RenderTaskId::Static(index) => index,
            &RenderTaskId::Dynamic(..) => panic!("This is a bug - expected a static render task!"),
        }
    }

    fn get_task_index(&self, id: &RenderTaskId, pass_index: RenderPassIndex) -> RenderTaskIndex {
        match id {
            &RenderTaskId::Static(index) => index,
            &RenderTaskId::Dynamic(key) => {
                self.dynamic_tasks[&(key, pass_index)].index
            }
        }
    }
}

struct AlphaBatchTask {
    task_id: RenderTaskId,
    items: Vec<AlphaRenderItem>,
}

pub struct BatchList {
    pub alpha_batches: Vec<PrimitiveBatch>,
    pub opaque_batches: Vec<PrimitiveBatch>,
}

impl BatchList {
    fn new() -> BatchList {
        BatchList {
            alpha_batches: Vec::new(),
            opaque_batches: Vec::new(),
        }
    }

    fn with_suitable_batch<F>(&mut self,
                              key: &AlphaBatchKey,
                              item_bounding_rect: &DeviceIntRect,
                              f: F) where F: Fn(&mut PrimitiveBatch) {
        let batch = self.get_suitable_batch(key, item_bounding_rect);
        f(batch)
    }

    fn get_suitable_batch(&mut self,
                          key: &AlphaBatchKey,
                          item_bounding_rect: &DeviceIntRect) -> &mut PrimitiveBatch {
        let (batches, check_intersections) = match key.blend_mode {
            BlendMode::None => {
                (&mut self.opaque_batches, false)
            }
            BlendMode::Alpha | BlendMode::PremultipliedAlpha | BlendMode::Subpixel(..) => {
                (&mut self.alpha_batches, true)
            }
        };

        let mut selected_batch_index = None;

        // Composites always get added to their own batch.
        // This is because the result of a composite can affect
        // the input to the next composite. Perhaps we can
        // optimize this in the future.
        if key.kind != AlphaBatchKind::Composite {
            'outer: for (batch_index, batch) in batches.iter()
                                                       .enumerate()
                                                       .rev()
                                                       .take(10) {
                if batch.key.is_compatible_with(key) {
                    selected_batch_index = Some(batch_index);
                    break;
                }

                // check for intersections
                if check_intersections {
                    for item_rect in &batch.item_rects {
                        if item_rect.intersects(item_bounding_rect) {
                            break 'outer;
                        }
                    }
                }
            }
        }

        if selected_batch_index.is_none() {
            let new_batch = PrimitiveBatch::new(key.clone());
            selected_batch_index = Some(batches.len());
            batches.push(new_batch);
        }

        let batch = &mut batches[selected_batch_index.unwrap()];
        batch.item_rects.push(*item_bounding_rect);

        batch
    }
}

/// Encapsulates the logic of building batches for items that are blended.
pub struct AlphaBatcher {
    pub batch_list: BatchList,
    tasks: Vec<AlphaBatchTask>,
}

impl AlphaRenderItem {
    fn add_to_batch(&self,
                    batch_list: &mut BatchList,
                    ctx: &RenderTargetContext,
                    gpu_cache: &mut GpuCache,
                    render_tasks: &RenderTaskCollection,
                    child_pass_index: RenderPassIndex,
                    task_index: RenderTaskIndex,
                    deferred_resolves: &mut Vec<DeferredResolve>) {
        match *self {
            AlphaRenderItem::Blend(stacking_context_index, src_id, filter, z) => {
                let stacking_context = &ctx.stacking_context_store[stacking_context_index.0];
                let key = AlphaBatchKey::new(AlphaBatchKind::Blend,
                                             AlphaBatchKeyFlags::empty(),
                                             BlendMode::Alpha,
                                             BatchTextures::no_texture());
                let src_task_index = render_tasks.get_static_task_index(&src_id);

                let (filter_mode, amount) = match filter {
                    LowLevelFilterOp::Blur(..) => (0, 0.0),
                    LowLevelFilterOp::Contrast(amount) => (1, amount.to_f32_px()),
                    LowLevelFilterOp::Grayscale(amount) => (2, amount.to_f32_px()),
                    LowLevelFilterOp::HueRotate(angle) => (3, (angle as f32) / ANGLE_FLOAT_TO_FIXED),
                    LowLevelFilterOp::Invert(amount) => (4, amount.to_f32_px()),
                    LowLevelFilterOp::Saturate(amount) => (5, amount.to_f32_px()),
                    LowLevelFilterOp::Sepia(amount) => (6, amount.to_f32_px()),
                    LowLevelFilterOp::Brightness(amount) => (7, amount.to_f32_px()),
                    LowLevelFilterOp::Opacity(amount) => (8, amount.to_f32_px()),
                };

                let amount = (amount * 65535.0).round() as i32;
                let batch = batch_list.get_suitable_batch(&key, &stacking_context.screen_bounds);

                let instance = CompositePrimitiveInstance::new(task_index,
                                                               src_task_index,
                                                               RenderTaskIndex(0),
                                                               filter_mode,
                                                               amount,
                                                               z);

                batch.add_instance(PrimitiveInstance::from(instance));
            }
            AlphaRenderItem::HardwareComposite(stacking_context_index, src_id, composite_op, z) => {
                let stacking_context = &ctx.stacking_context_store[stacking_context_index.0];
                let src_task_index = render_tasks.get_static_task_index(&src_id);
                let key = AlphaBatchKey::new(AlphaBatchKind::HardwareComposite,
                                             AlphaBatchKeyFlags::empty(),
                                             composite_op.to_blend_mode(),
                                             BatchTextures::no_texture());
                let batch = batch_list.get_suitable_batch(&key, &stacking_context.screen_bounds);

                let instance = CompositePrimitiveInstance::new(task_index,
                                                               src_task_index,
                                                               RenderTaskIndex(0),
                                                               0,
                                                               0,
                                                               z);

                batch.add_instance(PrimitiveInstance::from(instance));
            }
            AlphaRenderItem::Composite(stacking_context_index,
                                       backdrop_id,
                                       src_id,
                                       mode,
                                       z) => {
                let stacking_context = &ctx.stacking_context_store[stacking_context_index.0];
                let key = AlphaBatchKey::new(AlphaBatchKind::Composite,
                                             AlphaBatchKeyFlags::empty(),
                                             BlendMode::Alpha,
                                             BatchTextures::no_texture());
                let batch = batch_list.get_suitable_batch(&key, &stacking_context.screen_bounds);
                let backdrop_task = render_tasks.get_task_index(&backdrop_id, child_pass_index);
                let src_task_index = render_tasks.get_static_task_index(&src_id);

                let instance = CompositePrimitiveInstance::new(task_index,
                                                               src_task_index,
                                                               backdrop_task,
                                                               mode as u32 as i32,
                                                               0,
                                                               z);

                batch.add_instance(PrimitiveInstance::from(instance));
            }
            AlphaRenderItem::Primitive(clip_scroll_group_index_opt, prim_index, z) => {
                let prim_metadata = ctx.prim_store.get_metadata(prim_index);
                let (transform_kind, packed_layer_index) = match clip_scroll_group_index_opt {
                    Some(group_index) => {
                        let group = &ctx.clip_scroll_group_store[group_index.0];
                        let bounding_rect = group.screen_bounding_rect.as_ref().unwrap();
                        (bounding_rect.0, group.packed_layer_index)
                    },
                    None => (TransformedRectKind::AxisAligned, PackedLayerIndex(0)),
                };
                let needs_clipping = prim_metadata.needs_clipping();
                let mut flags = AlphaBatchKeyFlags::empty();
                if needs_clipping {
                    flags |= NEEDS_CLIPPING;
                }
                if transform_kind == TransformedRectKind::AxisAligned {
                    flags |= AXIS_ALIGNED;
                }
                let item_bounding_rect = ctx.prim_store.cpu_bounding_rects[prim_index.0].as_ref().unwrap();
                let clip_task_index = match prim_metadata.clip_task {
                    Some(ref clip_task) => {
                        render_tasks.get_task_index(&clip_task.id, child_pass_index)
                    }
                    None => {
                        OPAQUE_TASK_INDEX
                    }
                };
                let needs_blending = !prim_metadata.is_opaque ||
                                     needs_clipping ||
                                     transform_kind == TransformedRectKind::Complex;
                let blend_mode = ctx.prim_store.get_blend_mode(needs_blending, prim_metadata);

                let prim_cache_address = prim_metadata.gpu_location
                                                      .as_int(gpu_cache);

                let base_instance = SimplePrimitiveInstance::new(prim_cache_address,
                                                                 task_index,
                                                                 clip_task_index,
                                                                 packed_layer_index,
                                                                 z);

                let no_textures = BatchTextures::no_texture();

                match prim_metadata.prim_kind {
                    PrimitiveKind::Border => {
                        let border_cpu = &ctx.prim_store.cpu_borders[prim_metadata.cpu_prim_index.0];
                        // TODO(gw): Select correct blend mode for edges and corners!!
                        let corner_key = AlphaBatchKey::new(AlphaBatchKind::BorderCorner, flags, blend_mode, no_textures);
                        let edge_key = AlphaBatchKey::new(AlphaBatchKind::BorderEdge, flags, blend_mode, no_textures);

                        batch_list.with_suitable_batch(&corner_key, item_bounding_rect, |batch| {
                            for (i, instance_kind) in border_cpu.corner_instances.iter().enumerate() {
                                let sub_index = i as i32;
                                match *instance_kind {
                                    BorderCornerInstance::Single => {
                                        batch.add_instance(base_instance.build(sub_index,
                                                                               BorderCornerSide::Both as i32, 0));
                                    }
                                    BorderCornerInstance::Double => {
                                        batch.add_instance(base_instance.build(sub_index,
                                                                               BorderCornerSide::First as i32, 0));
                                        batch.add_instance(base_instance.build(sub_index,
                                                                               BorderCornerSide::Second as i32, 0));
                                    }
                                }
                            }
                        });

                        batch_list.with_suitable_batch(&edge_key, item_bounding_rect, |batch| {
                            for border_segment in 0..4 {
                                batch.add_instance(base_instance.build(border_segment, 0, 0));
                            }
                        });
                    }
                    PrimitiveKind::Rectangle => {
                        let key = AlphaBatchKey::new(AlphaBatchKind::Rectangle, flags, blend_mode, no_textures);
                        let batch = batch_list.get_suitable_batch(&key, item_bounding_rect);
                        batch.add_instance(base_instance.build(0, 0, 0));
                    }
                    PrimitiveKind::Image => {
                        let image_cpu = &ctx.prim_store.cpu_images[prim_metadata.cpu_prim_index.0];

                        let (color_texture_id, uv_address) = match image_cpu.kind {
                            ImagePrimitiveKind::Image(image_key, image_rendering, tile_offset, _) => {
                                resolve_image(image_key,
                                              image_rendering,
                                              tile_offset,
                                              ctx.resource_cache,
                                              gpu_cache,
                                              deferred_resolves)
                            }
                            ImagePrimitiveKind::WebGL(context_id) => {
                                let webgl_texture = ctx.resource_cache.get_webgl_texture(&context_id);
                                let uv_rect = [ 0.0,
                                                webgl_texture.size.height as f32,
                                                webgl_texture.size.width as f32,
                                                0.0];
                                let cache_handle = gpu_cache.push_per_frame_blocks(&[uv_rect.into()]);
                                (webgl_texture.id, cache_handle)
                            }
                        };

                        let batch_kind = match color_texture_id {
                            SourceTexture::External(ext_image) => {
                                match ext_image.image_type {
                                    ExternalImageType::Texture2DHandle => AlphaBatchKind::Image(ImageBufferKind::Texture2D),
                                    ExternalImageType::TextureRectHandle => AlphaBatchKind::Image(ImageBufferKind::TextureRect),
                                    ExternalImageType::TextureExternalHandle => AlphaBatchKind::Image(ImageBufferKind::TextureExternal),
                                    ExternalImageType::ExternalBuffer => {
                                        // The ExternalImageType::ExternalBuffer should be handled by resource_cache.
                                        // It should go through the non-external case.
                                        panic!("Non-texture handle type should be handled in other way.");
                                    }
                                }
                            }
                            _ => {
                                AlphaBatchKind::Image(ImageBufferKind::Texture2D)
                            }
                        };

                        let textures = BatchTextures {
                            colors: [color_texture_id, SourceTexture::Invalid, SourceTexture::Invalid],
                        };

                        let key = AlphaBatchKey::new(batch_kind, flags, blend_mode, textures);
                        let batch = batch_list.get_suitable_batch(&key, item_bounding_rect);
                        batch.add_instance(base_instance.build(uv_address.as_int(gpu_cache), 0, 0));
                    }
                    PrimitiveKind::TextRun => {
                        let text_cpu = &ctx.prim_store.cpu_text_runs[prim_metadata.cpu_prim_index.0];
                        let batch_kind = if text_cpu.blur_radius == 0.0 {
                            AlphaBatchKind::TextRun
                        } else {
                            // Select a generic primitive shader that can blit the
                            // results of the cached text blur to the framebuffer,
                            // applying tile clipping etc.
                            AlphaBatchKind::CacheImage
                        };

                        let font_size_dp = text_cpu.logical_font_size.scale_by(ctx.device_pixel_ratio);

                        let cache_task_index = match prim_metadata.render_task {
                            Some(ref task) => {
                                let cache_task_id = task.id;
                                render_tasks.get_task_index(&cache_task_id,
                                                            child_pass_index).0 as i32
                            }
                            None => 0,
                        };

                        // TODO(gw): avoid / recycle this allocation in the future.
                        let mut instances = Vec::new();

                        let texture_id = ctx.resource_cache.get_glyphs(text_cpu.font_key,
                                                                       font_size_dp,
                                                                       text_cpu.color,
                                                                       &text_cpu.glyph_instances,
                                                                       text_cpu.render_mode,
                                                                       text_cpu.glyph_options, |index, handle| {
                            let uv_address = handle.as_int(gpu_cache);
                            instances.push(base_instance.build(index as i32, cache_task_index, uv_address));
                        });

                        if texture_id != SourceTexture::Invalid {
                            let textures = BatchTextures {
                                colors: [texture_id, SourceTexture::Invalid, SourceTexture::Invalid],
                            };

                            let key = AlphaBatchKey::new(batch_kind, flags, blend_mode, textures);
                            let batch = batch_list.get_suitable_batch(&key, item_bounding_rect);

                            batch.add_instances(&instances);
                        }
                    }
                    PrimitiveKind::AlignedGradient => {
                        let gradient_cpu = &ctx.prim_store.cpu_gradients[prim_metadata.cpu_prim_index.0];
                        let key = AlphaBatchKey::new(AlphaBatchKind::AlignedGradient, flags, blend_mode, no_textures);
                        let batch = batch_list.get_suitable_batch(&key, item_bounding_rect);
                        for part_index in 0..(gradient_cpu.stops_count - 1) {
                            batch.add_instance(base_instance.build(part_index as i32, 0, 0));
                        }
                    }
                    PrimitiveKind::AngleGradient => {
                        let key = AlphaBatchKey::new(AlphaBatchKind::AngleGradient, flags, blend_mode, no_textures);
                        let batch = batch_list.get_suitable_batch(&key, item_bounding_rect);
                        batch.add_instance(base_instance.build(0, 0, 0));
                    }
                    PrimitiveKind::RadialGradient => {
                        let key = AlphaBatchKey::new(AlphaBatchKind::RadialGradient, flags, blend_mode, no_textures);
                        let batch = batch_list.get_suitable_batch(&key, item_bounding_rect);
                        batch.add_instance(base_instance.build(0, 0, 0));
                    }
                    PrimitiveKind::YuvImage => {
                        let mut textures = BatchTextures::no_texture();
                        let mut uv_rect_addresses = [0; 3];
                        let image_yuv_cpu = &ctx.prim_store.cpu_yuv_images[prim_metadata.cpu_prim_index.0];

                        //yuv channel
                        let channel_count = image_yuv_cpu.format.get_plane_num();
                        debug_assert!(channel_count <= 3);
                        for channel in 0..channel_count {
                            let image_key = image_yuv_cpu.yuv_key[channel];

                            let (texture, address) = resolve_image(image_key,
                                                                   image_yuv_cpu.image_rendering,
                                                                   None,
                                                                   ctx.resource_cache,
                                                                   gpu_cache,
                                                                   deferred_resolves);
                            textures.colors[channel] = texture;
                            uv_rect_addresses[channel] = address.as_int(gpu_cache);
                        }

                        let get_buffer_kind = |texture: SourceTexture| {
                            match texture {
                                SourceTexture::External(ext_image) => {
                                    match ext_image.image_type {
                                        ExternalImageType::Texture2DHandle => ImageBufferKind::Texture2D,
                                        ExternalImageType::TextureRectHandle => ImageBufferKind::TextureRect,
                                        ExternalImageType::TextureExternalHandle => ImageBufferKind::TextureExternal,
                                        ExternalImageType::ExternalBuffer => {
                                            // The ExternalImageType::ExternalBuffer should be handled by resource_cache.
                                            // It should go through the non-external case.
                                            panic!("Non-texture handle type should be handled in other way.");
                                        }
                                    }
                                }
                                _ => {
                                    ImageBufferKind::Texture2D
                                }
                            }
                        };

                        // All yuv textures should be the same type.
                        let buffer_kind = get_buffer_kind(textures.colors[0]);
                        assert!(textures.colors[1.. image_yuv_cpu.format.get_plane_num()].iter().all(
                            |&tid| buffer_kind == get_buffer_kind(tid)
                        ));

                        let key = AlphaBatchKey::new(AlphaBatchKind::YuvImage(buffer_kind, image_yuv_cpu.format, image_yuv_cpu.color_space),
                                                     flags,
                                                     blend_mode,
                                                     textures);
                        let batch = batch_list.get_suitable_batch(&key, item_bounding_rect);

                        batch.add_instance(base_instance.build(uv_rect_addresses[0],
                                                               uv_rect_addresses[1],
                                                               uv_rect_addresses[2]));
                    }
                    PrimitiveKind::BoxShadow => {
                        let box_shadow = &ctx.prim_store.cpu_box_shadows[prim_metadata.cpu_prim_index.0];
                        let cache_task_id = &prim_metadata.render_task.as_ref().unwrap().id;
                        let cache_task_index = render_tasks.get_task_index(cache_task_id,
                                                                           child_pass_index);

                        let key = AlphaBatchKey::new(AlphaBatchKind::BoxShadow, flags, blend_mode, no_textures);
                        let batch = batch_list.get_suitable_batch(&key, item_bounding_rect);

                        for rect_index in 0..box_shadow.rects.len() {
                            batch.add_instance(base_instance.build(rect_index as i32,
                                                                   cache_task_index.0 as i32, 0));
                        }
                    }
                }
            }
            AlphaRenderItem::SplitComposite(sc_index, task_id, gpu_handle, z) => {
                let key = AlphaBatchKey::new(AlphaBatchKind::SplitComposite,
                                             AlphaBatchKeyFlags::empty(),
                                             BlendMode::PremultipliedAlpha,
                                             BatchTextures::no_texture());
                let stacking_context = &ctx.stacking_context_store[sc_index.0];
                let batch = batch_list.get_suitable_batch(&key, &stacking_context.screen_bounds);
                let source_task = render_tasks.get_task_index(&task_id, child_pass_index);
                let gpu_address = gpu_handle.as_int(gpu_cache);

                let instance = CompositePrimitiveInstance::new(task_index,
                                                               source_task,
                                                               RenderTaskIndex(0),
                                                               gpu_address,
                                                               0,
                                                               z);

                batch.add_instance(PrimitiveInstance::from(instance));
            }
        }
    }
}

impl AlphaBatcher {
    fn new() -> AlphaBatcher {
        AlphaBatcher {
            tasks: Vec::new(),
            batch_list: BatchList::new(),
        }
    }

    fn add_task(&mut self, task: AlphaBatchTask) {
        self.tasks.push(task);
    }

    fn build(&mut self,
             ctx: &RenderTargetContext,
             gpu_cache: &mut GpuCache,
             render_tasks: &RenderTaskCollection,
             child_pass_index: RenderPassIndex,
             deferred_resolves: &mut Vec<DeferredResolve>) {
        for task in &self.tasks {
            let task_index = render_tasks.get_static_task_index(&task.task_id);

            for item in &task.items {
                item.add_to_batch(&mut self.batch_list,
                                  ctx,
                                  gpu_cache,
                                  render_tasks,
                                  child_pass_index,
                                  task_index,
                                  deferred_resolves);
            }
        }
    }
}

/// Batcher managing draw calls into the clip mask (in the RT cache).
#[derive(Debug)]
pub struct ClipBatcher {
    /// Rectangle draws fill up the rectangles with rounded corners.
    pub rectangles: Vec<CacheClipInstance>,
    /// Image draws apply the image masking.
    pub images: HashMap<SourceTexture, Vec<CacheClipInstance>>,
    pub border_clears: Vec<CacheClipInstance>,
    pub borders: Vec<CacheClipInstance>,
}

impl ClipBatcher {
    fn new() -> ClipBatcher {
        ClipBatcher {
            rectangles: Vec::new(),
            images: HashMap::new(),
            border_clears: Vec::new(),
            borders: Vec::new(),
        }
    }

    fn add<'a>(&mut self,
               task_index: RenderTaskIndex,
               clips: &[(PackedLayerIndex, MaskCacheInfo)],
               resource_cache: &ResourceCache,
               gpu_cache: &GpuCache,
               geometry_kind: MaskGeometryKind) {

        for &(packed_layer_index, ref info) in clips.iter() {
            let instance = CacheClipInstance {
                task_id: task_index.0 as i32,
                layer_index: packed_layer_index.0 as i32,
                address: GpuStoreAddress(0),
                segment: 0,
                resource_address: 0,
            };

            for clip_index in 0..info.effective_complex_clip_count as usize {
                let offset = info.complex_clip_range.start.0 + ((CLIP_DATA_GPU_SIZE * clip_index) as i32);
                match geometry_kind {
                    MaskGeometryKind::Default => {
                        self.rectangles.push(CacheClipInstance {
                            address: GpuStoreAddress(offset),
                            segment: MaskSegment::All as i32,
                            ..instance
                        });
                    }
                    MaskGeometryKind::CornersOnly => {
                        self.rectangles.extend(&[
                            CacheClipInstance {
                                address: GpuStoreAddress(offset),
                                segment: MaskSegment::TopLeftCorner as i32,
                                ..instance
                            },
                            CacheClipInstance {
                                address: GpuStoreAddress(offset),
                                segment: MaskSegment::TopRightCorner as i32,
                                ..instance
                            },
                            CacheClipInstance {
                                address: GpuStoreAddress(offset),
                                segment: MaskSegment::BottomLeftCorner as i32,
                                ..instance
                            },
                            CacheClipInstance {
                                address: GpuStoreAddress(offset),
                                segment: MaskSegment::BottomRightCorner as i32,
                                ..instance
                            },
                        ]);
                    }
                }
            }

            if let Some((ref mask, address)) = info.image {
                let cache_item = resource_cache.get_cached_image(mask.image, ImageRendering::Auto, None);
                self.images.entry(cache_item.texture_id)
                           .or_insert(Vec::new())
                           .push(CacheClipInstance {
                    address: address,
                    resource_address: cache_item.uv_rect_handle.as_int(gpu_cache),
                    ..instance
                })
            }

            for &(ref source, gpu_address) in &info.border_corners {
                self.border_clears.push(CacheClipInstance {
                    address: gpu_address,
                    segment: 0,
                    ..instance
                });

                for clip_index in 0..source.actual_clip_count {
                    self.borders.push(CacheClipInstance {
                        address: gpu_address,
                        segment: 1 + clip_index as i32,
                        ..instance
                    })
                }
            }
        }
    }
}

pub struct RenderTargetContext<'a> {
    pub device_pixel_ratio: f32,
    pub stacking_context_store: &'a [StackingContext],
    pub clip_scroll_group_store: &'a [ClipScrollGroup],
    pub prim_store: &'a PrimitiveStore,
    pub resource_cache: &'a ResourceCache,
}

struct TextureAllocator {
    // TODO(gw): Replace this with a simpler allocator for
    // render target allocation - this use case doesn't need
    // to deal with coalescing etc that the general texture
    // cache allocator requires.
    page_allocator: TexturePage,

    // Track the used rect of the render target, so that
    // we can set a scissor rect and only clear to the
    // used portion of the target as an optimization.
    used_rect: DeviceIntRect,
}

impl TextureAllocator {
    fn new(size: DeviceUintSize) -> TextureAllocator {
        TextureAllocator {
            page_allocator: TexturePage::new(CacheTextureId(0), size),
            used_rect: DeviceIntRect::zero(),
        }
    }

    fn allocate(&mut self, size: &DeviceUintSize) -> Option<DeviceUintPoint> {
        let origin = self.page_allocator.allocate(size);

        if let Some(origin) = origin {
            // TODO(gw): We need to make all the device rects
            //           be consistent in the use of the
            //           DeviceIntRect and DeviceUintRect types!
            let origin = DeviceIntPoint::new(origin.x as i32,
                                             origin.y as i32);
            let size = DeviceIntSize::new(size.width as i32,
                                          size.height as i32);
            let rect = DeviceIntRect::new(origin, size);
            self.used_rect = rect.union(&self.used_rect);
        }

        origin
    }
}

pub trait RenderTarget {
    fn new(size: DeviceUintSize) -> Self;
    fn allocate(&mut self, size: DeviceUintSize) -> Option<DeviceUintPoint>;
    fn build(&mut self,
             _ctx: &RenderTargetContext,
             _gpu_cache: &mut GpuCache,
             _render_tasks: &mut RenderTaskCollection,
             _child_pass_index: RenderPassIndex,
             _deferred_resolves: &mut Vec<DeferredResolve>) {}
    fn add_task(&mut self,
                task: RenderTask,
                ctx: &RenderTargetContext,
                gpu_cache: &GpuCache,
                render_tasks: &RenderTaskCollection,
                pass_index: RenderPassIndex);
    fn used_rect(&self) -> DeviceIntRect;
}

#[derive(Debug, Copy, Clone)]
pub enum RenderTargetKind {
    Color,   // RGBA32
    Alpha,   // R8
}

pub struct RenderTargetList<T> {
    target_size: DeviceUintSize,
    pub targets: Vec<T>,
}

impl<T: RenderTarget> RenderTargetList<T> {
    fn new(target_size: DeviceUintSize, create_initial_target: bool) -> RenderTargetList<T> {
        let mut targets = Vec::new();
        if create_initial_target {
            targets.push(T::new(target_size));
        }

        RenderTargetList {
            targets: targets,
            target_size: target_size,
        }
    }

    pub fn target_count(&self) -> usize {
        self.targets.len()
    }

    fn build(&mut self,
             ctx: &RenderTargetContext,
             gpu_cache: &mut GpuCache,
             render_tasks: &mut RenderTaskCollection,
             pass_index: RenderPassIndex,
             deferred_resolves: &mut Vec<DeferredResolve>) {
        for target in &mut self.targets {
            let child_pass_index = RenderPassIndex(pass_index.0 - 1);
            target.build(ctx, gpu_cache, render_tasks, child_pass_index, deferred_resolves);
        }
    }

    fn add_task(&mut self,
                task: RenderTask,
                ctx: &RenderTargetContext,
                gpu_cache: &GpuCache,
                render_tasks: &mut RenderTaskCollection,
                pass_index: RenderPassIndex) {
        self.targets.last_mut().unwrap().add_task(task, ctx, gpu_cache, render_tasks, pass_index);
    }

    fn allocate(&mut self, alloc_size: DeviceUintSize) -> (DeviceUintPoint, RenderTargetIndex) {
        let existing_origin = self.targets
                                  .last_mut()
                                  .and_then(|target| target.allocate(alloc_size));

        let origin = match existing_origin {
            Some(origin) => origin,
            None => {
                let mut new_target = T::new(self.target_size);
                let origin = new_target.allocate(alloc_size)
                                       .expect(&format!("Each render task must allocate <= size of one target! ({:?})", alloc_size));
                self.targets.push(new_target);
                origin
            }
        };

        (origin, RenderTargetIndex(self.targets.len() - 1))
    }
}

/// A render target represents a number of rendering operations on a surface.
pub struct ColorRenderTarget {
    pub alpha_batcher: AlphaBatcher,
    pub box_shadow_cache_prims: Vec<PrimitiveInstance>,
    // List of text runs to be cached to this render target.
    // TODO(gw): For now, assume that these all come from
    //           the same source texture id. This is almost
    //           always true except for pathological test
    //           cases with more than 4k x 4k of unique
    //           glyphs visible. Once the future glyph / texture
    //           cache changes land, this restriction will
    //           be removed anyway.
    pub text_run_cache_prims: Vec<PrimitiveInstance>,
    pub text_run_textures: BatchTextures,
    // List of blur operations to apply for this render target.
    pub vertical_blurs: Vec<BlurCommand>,
    pub horizontal_blurs: Vec<BlurCommand>,
    pub readbacks: Vec<DeviceIntRect>,
    allocator: TextureAllocator,
}

impl RenderTarget for ColorRenderTarget {
    fn allocate(&mut self, size: DeviceUintSize) -> Option<DeviceUintPoint> {
        self.allocator.allocate(&size)
    }

    fn new(size: DeviceUintSize) -> ColorRenderTarget {
        ColorRenderTarget {
            alpha_batcher: AlphaBatcher::new(),
            box_shadow_cache_prims: Vec::new(),
            text_run_cache_prims: Vec::new(),
            text_run_textures: BatchTextures::no_texture(),
            vertical_blurs: Vec::new(),
            horizontal_blurs: Vec::new(),
            readbacks: Vec::new(),
            allocator: TextureAllocator::new(size),
        }
    }

    fn used_rect(&self) -> DeviceIntRect {
        self.allocator.used_rect
    }

    fn build(&mut self,
             ctx: &RenderTargetContext,
             gpu_cache: &mut GpuCache,
             render_tasks: &mut RenderTaskCollection,
             child_pass_index: RenderPassIndex,
             deferred_resolves: &mut Vec<DeferredResolve>) {
        self.alpha_batcher.build(ctx,
                                 gpu_cache,
                                 render_tasks,
                                 child_pass_index,
                                 deferred_resolves);
    }

    fn add_task(&mut self,
                task: RenderTask,
                ctx: &RenderTargetContext,
                gpu_cache: &GpuCache,
                render_tasks: &RenderTaskCollection,
                pass_index: RenderPassIndex) {
        match task.kind {
            RenderTaskKind::Alpha(mut info) => {
                self.alpha_batcher.add_task(AlphaBatchTask {
                    task_id: task.id,
                    items: mem::replace(&mut info.items, Vec::new()),
                });
            }
            RenderTaskKind::VerticalBlur(_, prim_index) => {
                // Find the child render task that we are applying
                // a vertical blur on.
                // TODO(gw): Consider a simpler way for render tasks to find
                //           their child tasks than having to construct the
                //           correct id here.
                let child_pass_index = RenderPassIndex(pass_index.0 - 1);
                let task_key = RenderTaskKey::CachePrimitive(PrimitiveCacheKey::TextShadow(prim_index));
                let src_id = RenderTaskId::Dynamic(task_key);
                self.vertical_blurs.push(BlurCommand {
                    task_id: render_tasks.get_task_index(&task.id, pass_index).0 as i32,
                    src_task_id: render_tasks.get_task_index(&src_id, child_pass_index).0 as i32,
                    blur_direction: BlurDirection::Vertical as i32,
                    padding: 0,
                });
            }
            RenderTaskKind::HorizontalBlur(blur_radius, prim_index) => {
                // Find the child render task that we are applying
                // a horizontal blur on.
                let child_pass_index = RenderPassIndex(pass_index.0 - 1);
                let src_id = RenderTaskId::Dynamic(RenderTaskKey::VerticalBlur(blur_radius.0, prim_index));
                self.horizontal_blurs.push(BlurCommand {
                    task_id: render_tasks.get_task_index(&task.id, pass_index).0 as i32,
                    src_task_id: render_tasks.get_task_index(&src_id, child_pass_index).0 as i32,
                    blur_direction: BlurDirection::Horizontal as i32,
                    padding: 0,
                });
            }
            RenderTaskKind::CachePrimitive(prim_index) => {
                let prim_metadata = ctx.prim_store.get_metadata(prim_index);

                let prim_address = prim_metadata.gpu_location.as_int(gpu_cache);

                match prim_metadata.prim_kind {
                    PrimitiveKind::BoxShadow => {
                        let instance = SimplePrimitiveInstance::new(prim_address,
                                                                    render_tasks.get_task_index(&task.id, pass_index),
                                                                    RenderTaskIndex(0),
                                                                    PackedLayerIndex(0),
                                                                    0);     // z is disabled for rendering cache primitives
                        self.box_shadow_cache_prims.push(instance.build(0, 0, 0));
                    }
                    PrimitiveKind::TextRun => {
                        let text = &ctx.prim_store.cpu_text_runs[prim_metadata.cpu_prim_index.0];
                        // We only cache text runs with a text-shadow (for now).
                        debug_assert!(text.blur_radius != 0.0);

                        // todo(gw): avoid / recycle this allocation...
                        let mut instances = Vec::new();

                        let font_size_dp = text.logical_font_size.scale_by(ctx.device_pixel_ratio);

                        let instance = SimplePrimitiveInstance::new(prim_address,
                                                                    render_tasks.get_task_index(&task.id, pass_index),
                                                                    RenderTaskIndex(0),
                                                                    PackedLayerIndex(0),
                                                                    0);     // z is disabled for rendering cache primitives

                        let texture_id = ctx.resource_cache.get_glyphs(text.font_key,
                                                                       font_size_dp,
                                                                       text.color,
                                                                       &text.glyph_instances,
                                                                       text.render_mode,
                                                                       text.glyph_options, |index, handle| {
                            let uv_address = handle.as_int(gpu_cache);
                            instances.push(instance.build(index as i32, 0, uv_address));
                        });

                        if texture_id != SourceTexture::Invalid {
                            let textures = BatchTextures {
                                colors: [texture_id, SourceTexture::Invalid, SourceTexture::Invalid],
                            };

                            self.text_run_cache_prims.extend_from_slice(&instances);

                            debug_assert!(textures.colors[0] != SourceTexture::Invalid);
                            debug_assert!(self.text_run_textures.colors[0] == SourceTexture::Invalid ||
                                          self.text_run_textures.colors[0] == textures.colors[0]);
                            self.text_run_textures = textures;
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
        }
    }
}

pub struct AlphaRenderTarget {
    pub clip_batcher: ClipBatcher,
    allocator: TextureAllocator,
}

impl RenderTarget for AlphaRenderTarget {
    fn allocate(&mut self, size: DeviceUintSize) -> Option<DeviceUintPoint> {
        self.allocator.allocate(&size)
    }

    fn new(size: DeviceUintSize) -> AlphaRenderTarget {
        AlphaRenderTarget {
            clip_batcher: ClipBatcher::new(),
            allocator: TextureAllocator::new(size),
        }
    }

    fn used_rect(&self) -> DeviceIntRect {
        self.allocator.used_rect
    }

    fn add_task(&mut self,
                task: RenderTask,
                ctx: &RenderTargetContext,
                gpu_cache: &GpuCache,
                render_tasks: &RenderTaskCollection,
                pass_index: RenderPassIndex) {
        match task.kind {
            RenderTaskKind::Alpha(..) |
            RenderTaskKind::VerticalBlur(..) |
            RenderTaskKind::HorizontalBlur(..) |
            RenderTaskKind::CachePrimitive(..) |
            RenderTaskKind::Readback(..) => {
                panic!("Should not be added to alpha target!");
            }
            RenderTaskKind::CacheMask(ref task_info) => {
                let task_index = render_tasks.get_task_index(&task.id, pass_index);
                self.clip_batcher.add(task_index,
                                      &task_info.clips,
                                      &ctx.resource_cache,
                                      gpu_cache,
                                      task_info.geometry_kind);
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
    pass_index: RenderPassIndex,
    pub is_framebuffer: bool,
    tasks: Vec<RenderTask>,
    pub color_targets: RenderTargetList<ColorRenderTarget>,
    pub alpha_targets: RenderTargetList<AlphaRenderTarget>,
    pub color_texture_id: Option<TextureId>,
    pub alpha_texture_id: Option<TextureId>,
}

impl RenderPass {
    pub fn new(pass_index: isize, is_framebuffer: bool, size: DeviceUintSize) -> RenderPass {
        RenderPass {
            pass_index: RenderPassIndex(pass_index),
            is_framebuffer: is_framebuffer,
            color_targets: RenderTargetList::new(size, is_framebuffer),
            alpha_targets: RenderTargetList::new(size, false),
            tasks: vec![],
            color_texture_id: None,
            alpha_texture_id: None,
        }
    }

    pub fn add_render_task(&mut self, task: RenderTask) {
        self.tasks.push(task);
    }

    fn add_task(&mut self,
                task: RenderTask,
                ctx: &RenderTargetContext,
                gpu_cache: &GpuCache,
                render_tasks: &mut RenderTaskCollection) {
        match task.target_kind() {
            RenderTargetKind::Color => self.color_targets.add_task(task, ctx, gpu_cache, render_tasks, self.pass_index),
            RenderTargetKind::Alpha => self.alpha_targets.add_task(task, ctx, gpu_cache, render_tasks, self.pass_index),
        }
    }

    fn allocate_target(&mut self,
                       kind: RenderTargetKind,
                       alloc_size: DeviceUintSize) -> (DeviceUintPoint, RenderTargetIndex) {
        match kind {
            RenderTargetKind::Color => self.color_targets.allocate(alloc_size),
            RenderTargetKind::Alpha => self.alpha_targets.allocate(alloc_size),
        }
    }

    pub fn needs_render_target_kind(&self, kind: RenderTargetKind) -> bool {
        if self.is_framebuffer {
            false
        } else {
            self.required_target_count(kind) > 0
        }
    }

    pub fn required_target_count(&self, kind: RenderTargetKind) -> usize {
        debug_assert!(!self.is_framebuffer);        // framebuffer never needs targets
        match kind {
            RenderTargetKind::Color => self.color_targets.target_count(),
            RenderTargetKind::Alpha => self.alpha_targets.target_count(),
        }
    }

    pub fn build(&mut self,
                 ctx: &RenderTargetContext,
                 gpu_cache: &mut GpuCache,
                 render_tasks: &mut RenderTaskCollection,
                 deferred_resolves: &mut Vec<DeferredResolve>) {
        profile_scope!("RenderPass::build");

        // Step through each task, adding to batches as appropriate.
        let tasks = mem::replace(&mut self.tasks, Vec::new());
        for mut task in tasks {
            let target_kind = task.target_kind();

            // Find a target to assign this task to, or create a new
            // one if required.
            match task.location {
                RenderTaskLocation::Fixed => {}
                RenderTaskLocation::Dynamic(ref mut origin, ref size) => {
                    // See if this task is a duplicate.
                    // If so, just skip adding it!
                    match task.id {
                        RenderTaskId::Static(..) => {}
                        RenderTaskId::Dynamic(key) => {
                            // Look up cache primitive key in the render
                            // task data array. If a matching key exists
                            // (that is in this pass) there is no need
                            // to draw it again!
                            if let Some(rect) = render_tasks.get_dynamic_allocation(self.pass_index, key) {
                                debug_assert_eq!(rect.size, *size);
                                continue;
                            }
                        }
                    }

                    let alloc_size = DeviceUintSize::new(size.width as u32, size.height as u32);
                    let (alloc_origin, target_index) = self.allocate_target(target_kind, alloc_size);

                    *origin = Some((DeviceIntPoint::new(alloc_origin.x as i32,
                                                        alloc_origin.y as i32),
                                    target_index));
                }
            }

            render_tasks.add(&task, self.pass_index);
            self.add_task(task, ctx, gpu_cache, render_tasks);
        }

        self.color_targets.build(ctx, gpu_cache, render_tasks, self.pass_index, deferred_resolves);
        self.alpha_targets.build(ctx, gpu_cache, render_tasks, self.pass_index, deferred_resolves);
    }
}

#[derive(Copy, Clone, PartialEq, Eq, Hash, Debug)]
pub enum AlphaBatchKind {
    Composite,
    HardwareComposite,
    SplitComposite,
    Blend,
    Rectangle,
    TextRun,
    Image(ImageBufferKind),
    YuvImage(ImageBufferKind, YuvFormat, YuvColorSpace),
    AlignedGradient,
    AngleGradient,
    RadialGradient,
    BoxShadow,
    CacheImage,
    BorderCorner,
    BorderEdge,
}

bitflags! {
    pub flags AlphaBatchKeyFlags: u8 {
        const NEEDS_CLIPPING  = 0b00000001,
        const AXIS_ALIGNED    = 0b00000010,
    }
}

impl AlphaBatchKeyFlags {
    pub fn transform_kind(&self) -> TransformedRectKind {
        if self.contains(AXIS_ALIGNED) {
            TransformedRectKind::AxisAligned
        } else {
            TransformedRectKind::Complex
        }
    }

    pub fn needs_clipping(&self) -> bool {
        self.contains(NEEDS_CLIPPING)
    }
}

#[derive(Copy, Clone, Debug)]
pub struct AlphaBatchKey {
    pub kind: AlphaBatchKind,
    pub flags: AlphaBatchKeyFlags,
    pub blend_mode: BlendMode,
    pub textures: BatchTextures,
}

impl AlphaBatchKey {
    fn new(kind: AlphaBatchKind,
           flags: AlphaBatchKeyFlags,
           blend_mode: BlendMode,
           textures: BatchTextures) -> AlphaBatchKey {
        AlphaBatchKey {
            kind: kind,
            flags: flags,
            blend_mode: blend_mode,
            textures: textures,
        }
    }

    fn is_compatible_with(&self, other: &AlphaBatchKey) -> bool {
        self.kind == other.kind &&
            self.flags == other.flags &&
            self.blend_mode == other.blend_mode &&
            textures_compatible(self.textures.colors[0], other.textures.colors[0]) &&
            textures_compatible(self.textures.colors[1], other.textures.colors[1]) &&
            textures_compatible(self.textures.colors[2], other.textures.colors[2])
    }
}

#[repr(C)]
#[derive(Debug)]
pub enum BlurDirection {
    Horizontal = 0,
    Vertical,
}

#[inline]
fn textures_compatible(t1: SourceTexture, t2: SourceTexture) -> bool {
    t1 == SourceTexture::Invalid || t2 == SourceTexture::Invalid || t1 == t2
}

// All Packed Primitives below must be 16 byte aligned.
#[derive(Debug)]
pub struct BlurCommand {
    task_id: i32,
    src_task_id: i32,
    blur_direction: i32,
    padding: i32,
}

/// A clipping primitive drawn into the clipping mask.
/// Could be an image or a rectangle, which defines the
/// way `address` is treated.
#[derive(Clone, Copy, Debug)]
pub struct CacheClipInstance {
    task_id: i32,
    layer_index: i32,
    address: GpuStoreAddress,
    segment: i32,
    resource_address: i32,
}

// 32 bytes per instance should be enough for anyone!
#[derive(Debug, Clone)]
pub struct PrimitiveInstance {
    data: [i32; 8],
}

struct SimplePrimitiveInstance {
    // TODO(gw): specific_prim_address is encoded as an i32, since
    //           some primitives use GPU Cache and some still use
    //           GPU Store. Once everything is converted to use the
    //           on-demand GPU cache, then we change change this to
    //           be an ivec2 of u16 - and encode the UV directly
    //           so that the vertex shader can fetch directly.
    pub specific_prim_address: i32,
    pub task_index: i32,
    pub clip_task_index: i32,
    pub layer_index: i32,
    pub z_sort_index: i32,
}

impl SimplePrimitiveInstance {
    fn new(specific_prim_address: i32,
           task_index: RenderTaskIndex,
           clip_task_index: RenderTaskIndex,
           layer_index: PackedLayerIndex,
           z_sort_index: i32) -> SimplePrimitiveInstance {
        SimplePrimitiveInstance {
            specific_prim_address: specific_prim_address,
            task_index: task_index.0 as i32,
            clip_task_index: clip_task_index.0 as i32,
            layer_index: layer_index.0 as i32,
            z_sort_index: z_sort_index,
        }
    }

    fn build(&self, data0: i32, data1: i32, data2: i32) -> PrimitiveInstance {
        PrimitiveInstance {
            data: [
                self.specific_prim_address,
                self.task_index,
                self.clip_task_index,
                self.layer_index,
                self.z_sort_index,
                data0,
                data1,
                data2,
            ]
        }
    }
}

pub struct CompositePrimitiveInstance {
    pub task_index: RenderTaskIndex,
    pub src_task_index: RenderTaskIndex,
    pub backdrop_task_index: RenderTaskIndex,
    pub data0: i32,
    pub data1: i32,
    pub z: i32,
}

impl CompositePrimitiveInstance {
    fn new(task_index: RenderTaskIndex,
           src_task_index: RenderTaskIndex,
           backdrop_task_index: RenderTaskIndex,
           data0: i32,
           data1: i32,
           z: i32) -> CompositePrimitiveInstance {
        CompositePrimitiveInstance {
            task_index: task_index,
            src_task_index: src_task_index,
            backdrop_task_index: backdrop_task_index,
            data0: data0,
            data1: data1,
            z: z,
        }
    }
}

impl From<CompositePrimitiveInstance> for PrimitiveInstance {
    fn from(instance: CompositePrimitiveInstance) -> PrimitiveInstance {
        PrimitiveInstance {
            data: [
                instance.task_index.0 as i32,
                instance.src_task_index.0 as i32,
                instance.backdrop_task_index.0 as i32,
                instance.z,
                instance.data0,
                instance.data1,
                0,
                0,
            ]
        }
    }
}

impl<'a> From<&'a PrimitiveInstance> for CompositePrimitiveInstance {
    fn from(instance: &'a PrimitiveInstance) -> CompositePrimitiveInstance {
        CompositePrimitiveInstance {
            task_index: RenderTaskIndex(instance.data[0] as usize),
            src_task_index: RenderTaskIndex(instance.data[1] as usize),
            backdrop_task_index: RenderTaskIndex(instance.data[2] as usize),
            z: instance.data[3],
            data0: instance.data[4],
            data1: instance.data[5],
        }
    }
}

#[derive(Debug)]
pub struct PrimitiveBatch {
    pub key: AlphaBatchKey,
    pub instances: Vec<PrimitiveInstance>,
    pub item_rects: Vec<DeviceIntRect>,
}

impl PrimitiveBatch {
    fn new(key: AlphaBatchKey) -> PrimitiveBatch {
        PrimitiveBatch {
            key: key,
            instances: Vec::new(),
            item_rects: Vec::new(),
        }
    }

    fn add_instance(&mut self, instance: PrimitiveInstance) {
        self.instances.push(instance);
    }

    fn add_instances(&mut self, instances: &[PrimitiveInstance]) {
        self.instances.extend_from_slice(instances);
    }
}

#[derive(Debug, Copy, Clone, Eq, PartialEq, Hash, Ord, PartialOrd)]
pub struct PackedLayerIndex(pub usize);

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

    /// Local bounding rectangle for this stacking context.
    pub local_bounds: LayerRect,

    /// Screen space bounding rectangle for this stacking context,
    /// calculated based on the size and position of all its children.
    pub screen_bounds: DeviceIntRect,

    pub composite_ops: CompositeOps,
    pub clip_scroll_groups: Vec<ClipScrollGroupIndex>,

    /// Type of the isolation of the content.
    pub isolation: ContextIsolation,

    /// Set for the root stacking context of a display list or an iframe. Used for determining
    /// when to isolate a mix-blend-mode composite.
    pub is_page_root: bool,

    /// Whether or not this stacking context has any visible components, calculated
    /// based on the size and position of all children and how they are clipped.
    pub is_visible: bool,
}

impl StackingContext {
    pub fn new(pipeline_id: PipelineId,
               reference_frame_offset: LayerVector2D,
               is_page_root: bool,
               reference_frame_id: ClipId,
               local_bounds: LayerRect,
               transform_style: TransformStyle,
               composite_ops: CompositeOps)
               -> StackingContext {
        let isolation = match transform_style {
            TransformStyle::Flat => ContextIsolation::None,
            TransformStyle::Preserve3D => ContextIsolation::Items,
        };
        StackingContext {
            pipeline_id: pipeline_id,
            reference_frame_offset: reference_frame_offset,
            reference_frame_id: reference_frame_id,
            local_bounds: local_bounds,
            screen_bounds: DeviceIntRect::zero(),
            composite_ops: composite_ops,
            clip_scroll_groups: Vec::new(),
            isolation: isolation,
            is_page_root: is_page_root,
            is_visible: false,
        }
    }

    pub fn clip_scroll_group(&self, clip_and_scroll: ClipAndScrollInfo) -> ClipScrollGroupIndex {
        // Currently there is only one scrolled stacking context per context,
        // but eventually this will be selected from the vector based on the
        // scroll layer of this primitive.
        for group in &self.clip_scroll_groups {
            if group.1 == clip_and_scroll {
                return *group;
            }
        }
        unreachable!("Looking for non-existent ClipScrollGroup");
    }

    pub fn can_contribute_to_scene(&self) -> bool {
        !self.composite_ops.will_make_invisible()
    }

    pub fn has_clip_scroll_group(&self, clip_and_scroll: ClipAndScrollInfo) -> bool {
        self.clip_scroll_groups.iter().rev().any(|index| index.1 == clip_and_scroll)
    }
}

#[derive(Debug, Copy, Clone, Eq, PartialEq, Hash)]
pub struct ClipScrollGroupIndex(pub usize, pub ClipAndScrollInfo);

#[derive(Debug)]
pub struct ClipScrollGroup {
    pub stacking_context_index: StackingContextIndex,
    pub scroll_node_id: ClipId,
    pub clip_node_id: ClipId,
    pub packed_layer_index: PackedLayerIndex,
    pub screen_bounding_rect: Option<(TransformedRectKind, DeviceIntRect)>,
}

impl ClipScrollGroup {
    pub fn is_visible(&self) -> bool {
        self.screen_bounding_rect.is_some()
    }
}

#[derive(Debug, Clone)]
#[repr(C)]
pub struct PackedLayer {
    pub transform: LayerToWorldTransform,
    pub inv_transform: WorldToLayerTransform,
    pub local_clip_rect: LayerRect,
}

impl Default for PackedLayer {
    fn default() -> PackedLayer {
        PackedLayer {
            transform: LayerToWorldTransform::identity(),
            inv_transform: WorldToLayerTransform::identity(),
            local_clip_rect: LayerRect::zero(),
        }
    }
}

impl PackedLayer {
    pub fn empty() -> PackedLayer {
        Default::default()
    }

    pub fn set_transform(&mut self, transform: LayerToWorldTransform) {
        self.transform = transform;
        self.inv_transform = self.transform.inverse().unwrap();
    }

    pub fn set_rect(&mut self,
                    local_rect: &LayerRect,
                    screen_rect: &DeviceIntRect,
                    device_pixel_ratio: f32)
                    -> Option<(TransformedRectKind, DeviceIntRect)> {
        let xf_rect = TransformedRect::new(&local_rect, &self.transform, device_pixel_ratio);
        xf_rect.bounding_rect.intersection(screen_rect).map(|rect| {
            self.local_clip_rect = *local_rect;
            (xf_rect.kind, rect)
        })
    }
}

#[derive(Debug, Clone)]
pub struct CompositeOps {
    // Requires only a single texture as input (e.g. most filters)
    pub filters: Vec<LowLevelFilterOp>,

    // Requires two source textures (e.g. mix-blend-mode)
    pub mix_blend_mode: Option<MixBlendMode>,
}

impl CompositeOps {
    pub fn new(filters: Vec<LowLevelFilterOp>, mix_blend_mode: Option<MixBlendMode>) -> CompositeOps {
        CompositeOps {
            filters: filters,
            mix_blend_mode: mix_blend_mode
        }
    }

    pub fn count(&self) -> usize {
        self.filters.len() + if self.mix_blend_mode.is_some() { 1 } else { 0 }
    }

    pub fn will_make_invisible(&self) -> bool {
        for op in &self.filters {
            if op == &LowLevelFilterOp::Opacity(Au(0)) {
                return true;
            }
        }
        false
    }
}

impl Default for CompositeOps {
    fn default() -> CompositeOps {
        CompositeOps {
            filters: Vec::new(),
            mix_blend_mode: None,
        }
    }
}

/// A rendering-oriented representation of frame::Frame built by the render backend
/// and presented to the renderer.
pub struct Frame {
    pub window_size: DeviceUintSize,
    pub background_color: Option<ColorF>,
    pub device_pixel_ratio: f32,
    pub cache_size: DeviceUintSize,
    pub passes: Vec<RenderPass>,
    pub profile_counters: FrameProfileCounters,

    pub layer_texture_data: Vec<PackedLayer>,
    pub render_task_data: Vec<RenderTaskData>,
    pub gpu_data32: Vec<GpuBlock32>,

    // List of updates that need to be pushed to the
    // gpu resource cache.
    pub gpu_cache_updates: Option<GpuCacheUpdateList>,

    // List of textures that we don't know about yet
    // from the backend thread. The render thread
    // will use a callback to resolve these and
    // patch the data structures.
    pub deferred_resolves: Vec<DeferredResolve>,
}

fn resolve_image(image_key: ImageKey,
                 image_rendering: ImageRendering,
                 tile_offset: Option<TileOffset>,
                 resource_cache: &ResourceCache,
                 gpu_cache: &mut GpuCache,
                 deferred_resolves: &mut Vec<DeferredResolve>) -> (SourceTexture, GpuCacheHandle) {
    let image_properties = resource_cache.get_image_properties(image_key);

    // Check if an external image that needs to be resolved
    // by the render thread.
    match image_properties.external_image {
        Some(external_image) => {
            // This is an external texture - we will add it to
            // the deferred resolves list to be patched by
            // the render thread...
            let cache_handle = gpu_cache.push_deferred_per_frame_blocks(1);
            deferred_resolves.push(DeferredResolve {
                image_properties: image_properties,
                address: gpu_cache.get_address(&cache_handle),
            });

            (SourceTexture::External(external_image), cache_handle)
        }
        None => {
            let cache_item = resource_cache.get_cached_image(image_key,
                                                             image_rendering,
                                                             tile_offset);

            (cache_item.texture_id, cache_item.uv_rect_handle)
        }
    }
}

