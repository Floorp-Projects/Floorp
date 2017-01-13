/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use app_units::Au;
use batch_builder::BorderSideHelpers;
use fnv::FnvHasher;
use frame::FrameId;
use gpu_store::GpuStoreAddress;
use internal_types::{ANGLE_FLOAT_TO_FIXED, LowLevelFilterOp, CompositionOp};
use internal_types::{BatchTextures, CacheTextureId, SourceTexture};
use layer::Layer;
use mask_cache::{ClipSource, MaskCacheInfo};
use prim_store::{PrimitiveGeometry, RectanglePrimitive, PrimitiveContainer};
use prim_store::{BorderPrimitiveCpu, BorderPrimitiveGpu, BoxShadowPrimitiveGpu};
use prim_store::{ImagePrimitiveCpu, ImagePrimitiveGpu, YuvImagePrimitiveCpu, YuvImagePrimitiveGpu, ImagePrimitiveKind, };
use prim_store::{PrimitiveKind, PrimitiveIndex, PrimitiveMetadata, TexelRect};
use prim_store::{CLIP_DATA_GPU_SIZE, DeferredResolve};
use prim_store::{GradientPrimitiveCpu, GradientPrimitiveGpu, GradientType};
use prim_store::{PrimitiveCacheKey, TextRunPrimitiveGpu, TextRunPrimitiveCpu};
use prim_store::{PrimitiveStore, GpuBlock16, GpuBlock32, GpuBlock64, GpuBlock128};
use profiler::FrameProfileCounters;
use renderer::BlendMode;
use resource_cache::ResourceCache;
use std::cmp;
use std::collections::{HashMap};
use std::{i32, f32};
use std::mem;
use std::hash::{BuildHasherDefault};
use std::sync::atomic::{AtomicUsize, Ordering};
use std::usize;
use texture_cache::TexturePage;
use util::{self, rect_from_points, rect_from_points_f};
use util::{TransformedRect, TransformedRectKind, subtract_rect, pack_as_float};
use webrender_traits::{ColorF, FontKey, ImageKey, ImageRendering, MixBlendMode};
use webrender_traits::{BorderDisplayItem, BorderSide, BorderStyle, YuvColorSpace};
use webrender_traits::{AuxiliaryLists, ItemRange, BoxShadowClipMode, ClipRegion};
use webrender_traits::{PipelineId, ScrollLayerId, WebGLContextId, FontRenderMode};
use webrender_traits::{DeviceIntRect, DeviceIntPoint, DeviceIntSize, DeviceIntLength, device_length};
use webrender_traits::{DeviceUintSize, DeviceUintPoint, DeviceSize};
use webrender_traits::{LayerRect, LayerPoint, LayerSize};
use webrender_traits::{LayerToScrollTransform, LayerToWorldTransform, WorldToLayerTransform};
use webrender_traits::{WorldPoint4D, ScrollLayerPixel, as_scroll_parent_rect};

// Removes the clip task dependencies and instead
// draws all the clip instances that affect a primitive
const CLIP_TASK_COLLAPSE: bool = true;

// Special sentinel value recognized by the shader. It is considered to be
// a dummy task that doesn't mask out anything.
const OPAQUE_TASK_INDEX: RenderTaskIndex = RenderTaskIndex(i32::MAX as usize);

const FLOATS_PER_RENDER_TASK_INFO: usize = 8;

pub type LayerMap = HashMap<ScrollLayerId,
                            Layer,
                            BuildHasherDefault<FnvHasher>>;
pub type AuxiliaryListsMap = HashMap<PipelineId,
                                     AuxiliaryLists,
                                     BuildHasherDefault<FnvHasher>>;

trait AlphaBatchHelpers {
    fn get_batch_kind(&self, metadata: &PrimitiveMetadata) -> AlphaBatchKind;
    fn get_color_textures(&self, metadata: &PrimitiveMetadata) -> [SourceTexture; 3];
    fn get_blend_mode(&self, needs_blending: bool, metadata: &PrimitiveMetadata) -> BlendMode;
    fn prim_affects_tile(&self,
                         prim_index: PrimitiveIndex,
                         tile_rect: &DeviceIntRect,
                         transform: &LayerToWorldTransform,
                         device_pixel_ratio: f32) -> bool;
    fn add_prim_to_batch(&self,
                         prim_index: PrimitiveIndex,
                         batch: &mut PrimitiveBatch,
                         layer_index: StackingContextIndex,
                         task_index: RenderTaskIndex,
                         tile_id: TileUniqueId,
                         base_mask_task_index: RenderTaskIndex,
                         render_tasks: &RenderTaskCollection,
                         pass_index: RenderPassIndex,
                         z_sort_index: i32);
}

impl AlphaBatchHelpers for PrimitiveStore {
    fn get_batch_kind(&self, metadata: &PrimitiveMetadata) -> AlphaBatchKind {
        let batch_kind = match metadata.prim_kind {
            PrimitiveKind::Border => AlphaBatchKind::Border,
            PrimitiveKind::BoxShadow => AlphaBatchKind::BoxShadow,
            PrimitiveKind::Image => AlphaBatchKind::Image,
            PrimitiveKind::YuvImage => AlphaBatchKind::YuvImage,
            PrimitiveKind::Rectangle => AlphaBatchKind::Rectangle,
            PrimitiveKind::TextRun => {
                let text_run_cpu = &self.cpu_text_runs[metadata.cpu_prim_index.0];
                if text_run_cpu.blur_radius.0 == 0 {
                    AlphaBatchKind::TextRun
                } else {
                    // Select a generic primitive shader that can blit the
                    // results of the cached text blur to the framebuffer,
                    // applying tile clipping etc.
                    AlphaBatchKind::CacheImage
                }
            }
            PrimitiveKind::Gradient => {
                let gradient = &self.cpu_gradients[metadata.cpu_prim_index.0];
                match gradient.kind {
                    GradientType::Horizontal | GradientType::Vertical => {
                        AlphaBatchKind::AlignedGradient
                    }
                    GradientType::Rotated => {
                        AlphaBatchKind::AngleGradient
                    }
                }
            }
        };

        batch_kind
    }

    fn get_color_textures(&self, metadata: &PrimitiveMetadata) -> [SourceTexture; 3] {
        let invalid = SourceTexture::Invalid;
        match metadata.prim_kind {
            PrimitiveKind::Border |
            PrimitiveKind::BoxShadow |
            PrimitiveKind::Rectangle |
            PrimitiveKind::Gradient => [invalid; 3],
            PrimitiveKind::Image => {
                let image_cpu = &self.cpu_images[metadata.cpu_prim_index.0];
                [image_cpu.color_texture_id, invalid, invalid]
            }
            PrimitiveKind::YuvImage => {
                let image_cpu = &self.cpu_yuv_images[metadata.cpu_prim_index.0];
                [image_cpu.y_texture_id, image_cpu.u_texture_id, image_cpu.v_texture_id]
            }
            PrimitiveKind::TextRun => {
                let text_run_cpu = &self.cpu_text_runs[metadata.cpu_prim_index.0];
                [text_run_cpu.color_texture_id, invalid, invalid]
            }
        }
    }

    fn get_blend_mode(&self, needs_blending: bool, metadata: &PrimitiveMetadata) -> BlendMode {
        match metadata.prim_kind {
            PrimitiveKind::TextRun => {
                let text_run_cpu = &self.cpu_text_runs[metadata.cpu_prim_index.0];
                if text_run_cpu.blur_radius.0 == 0 {
                    match text_run_cpu.render_mode {
                        FontRenderMode::Subpixel => BlendMode::Subpixel(text_run_cpu.color),
                        FontRenderMode::Alpha | FontRenderMode::Mono => BlendMode::Alpha,
                    }
                } else {
                    // Text runs drawn to blur never get drawn with subpixel AA.
                    BlendMode::Alpha
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

    // Optional narrow phase intersection test, depending on primitive type.
    fn prim_affects_tile(&self,
                         prim_index: PrimitiveIndex,
                         tile_rect: &DeviceIntRect,
                         transform: &LayerToWorldTransform,
                         device_pixel_ratio: f32) -> bool {
        let metadata = self.get_metadata(prim_index);
        match metadata.prim_kind {
            PrimitiveKind::Rectangle |
            PrimitiveKind::TextRun |
            PrimitiveKind::Image |
            PrimitiveKind::YuvImage |
            PrimitiveKind::Gradient |
            PrimitiveKind::BoxShadow => true,
            PrimitiveKind::Border => {
                let border = &self.cpu_borders[metadata.cpu_prim_index.0];
                let inner_rect = TransformedRect::new(&border.inner_rect,
                                                      transform,
                                                      device_pixel_ratio);

                !inner_rect.bounding_rect.contains_rect(tile_rect)
            }
        }
    }

    fn add_prim_to_batch(&self,
                         prim_index: PrimitiveIndex,
                         batch: &mut PrimitiveBatch,
                         layer_index: StackingContextIndex,
                         task_index: RenderTaskIndex,
                         tile_id: TileUniqueId,
                         base_mask_task_index: RenderTaskIndex,
                         render_tasks: &RenderTaskCollection,
                         child_pass_index: RenderPassIndex,
                         z_sort_index: i32) {
        let metadata = self.get_metadata(prim_index);
        let layer_index = layer_index.0 as i32;
        let global_prim_id = prim_index.0 as i32;
        let prim_address = metadata.gpu_prim_index;
        let clip_task_key = RenderTaskKey::CacheMask(MaskCacheKey::Primitive(prim_index), tile_id);
        let clip_task_index = if metadata.clip_cache_info.is_some() &&
                                 render_tasks.has_dynamic_task(&clip_task_key, child_pass_index) {
            let cache_task_id = RenderTaskId::Dynamic(clip_task_key);
            render_tasks.get_task_index(&cache_task_id, child_pass_index)
        } else {
            base_mask_task_index
        };
        let task_index = task_index.0 as i32;
        let clip_task_index = clip_task_index.0 as i32;

        match &mut batch.data {
            &mut PrimitiveBatchData::Blend(..) |
            &mut PrimitiveBatchData::Composite(..) => unreachable!(),

            &mut PrimitiveBatchData::Rectangles(ref mut data) => {
                data.push(PrimitiveInstance {
                    task_index: task_index,
                    clip_task_index: clip_task_index,
                    layer_index: layer_index,
                    global_prim_id: global_prim_id,
                    prim_address: prim_address,
                    sub_index: 0,
                    user_data: [0, 0],
                    z_sort_index: z_sort_index,
                });
            }
            &mut PrimitiveBatchData::TextRun(ref mut data) => {
                let text_cpu = &self.cpu_text_runs[metadata.cpu_prim_index.0];

                for glyph_index in 0..metadata.gpu_data_count {
                    data.push(PrimitiveInstance {
                        task_index: task_index,
                        clip_task_index: clip_task_index,
                        layer_index: layer_index,
                        global_prim_id: global_prim_id,
                        prim_address: prim_address,
                        sub_index: metadata.gpu_data_address.0 + glyph_index,
                        user_data: [ text_cpu.resource_address.0 + glyph_index, 0 ],
                        z_sort_index: z_sort_index,
                    });
                }
            }
            &mut PrimitiveBatchData::Image(ref mut data) => {
                let image_cpu = &self.cpu_images[metadata.cpu_prim_index.0];

                data.push(PrimitiveInstance {
                    task_index: task_index,
                    clip_task_index: clip_task_index,
                    layer_index: layer_index,
                    global_prim_id: global_prim_id,
                    prim_address: prim_address,
                    sub_index: 0,
                    user_data: [ image_cpu.resource_address.0, 0 ],
                    z_sort_index: z_sort_index,
                });
            }
            &mut PrimitiveBatchData::YuvImage(ref mut data) => {
                data.push(PrimitiveInstance {
                    task_index: task_index,
                    clip_task_index: clip_task_index,
                    layer_index: layer_index,
                    global_prim_id: global_prim_id,
                    prim_address: prim_address,
                    sub_index: 0,
                    user_data: [ 0, 0 ],
                    z_sort_index: z_sort_index,
                });
            }
            &mut PrimitiveBatchData::Borders(ref mut data) => {
                for border_segment in 0..8 {
                    data.push(PrimitiveInstance {
                        task_index: task_index,
                        clip_task_index: clip_task_index,
                        layer_index: layer_index,
                        global_prim_id: global_prim_id,
                        prim_address: prim_address,
                        sub_index: border_segment,
                        user_data: [ 0, 0 ],
                        z_sort_index: z_sort_index,
                    });
                }
            }
            &mut PrimitiveBatchData::AlignedGradient(ref mut data) => {
                for part_index in 0..(metadata.gpu_data_count - 1) {
                    data.push(PrimitiveInstance {
                        task_index: task_index,
                        clip_task_index: clip_task_index,
                        layer_index: layer_index,
                        global_prim_id: global_prim_id,
                        prim_address: prim_address,
                        sub_index: metadata.gpu_data_address.0 + part_index,
                        user_data: [ 0, 0 ],
                        z_sort_index: z_sort_index,
                    });
                }
            }
            &mut PrimitiveBatchData::AngleGradient(ref mut data) => {
                data.push(PrimitiveInstance {
                    task_index: task_index,
                    clip_task_index: clip_task_index,
                    layer_index: layer_index,
                    global_prim_id: global_prim_id,
                    prim_address: prim_address,
                    sub_index: metadata.gpu_data_address.0,
                    user_data: [ metadata.gpu_data_count, 0 ],
                    z_sort_index: z_sort_index,
                });
            }
            &mut PrimitiveBatchData::CacheImage(ref mut data) => {
                // Find the render task index for the render task
                // that this primitive depends on. Pass it to the
                // shader so that it can sample from the cache texture
                // at the correct location.
                let cache_task_id = &metadata.render_task.as_ref().unwrap().id;
                let cache_task_index = render_tasks.get_task_index(cache_task_id,
                                                                   child_pass_index);

                data.push(PrimitiveInstance {
                    task_index: task_index,
                    clip_task_index: clip_task_index,
                    layer_index: layer_index,
                    global_prim_id: global_prim_id,
                    prim_address: prim_address,
                    sub_index: 0,
                    user_data: [ cache_task_index.0 as i32, 0 ],
                    z_sort_index: z_sort_index,
                });
            }
            &mut PrimitiveBatchData::BoxShadow(ref mut data) => {
                let cache_task_id = &metadata.render_task.as_ref().unwrap().id;
                let cache_task_index = render_tasks.get_task_index(cache_task_id,
                                                                   child_pass_index);

                for rect_index in 0..metadata.gpu_data_count {
                    data.push(PrimitiveInstance {
                        task_index: task_index,
                        clip_task_index: clip_task_index,
                        layer_index: layer_index,
                        global_prim_id: global_prim_id,
                        prim_address: prim_address,
                        sub_index: metadata.gpu_data_address.0 + rect_index,
                        user_data: [ cache_task_index.0 as i32, 0 ],
                        z_sort_index: z_sort_index,
                    });
                }
            }
        }
    }
}

#[derive(Debug)]
struct ScrollbarPrimitive {
    scroll_layer_id: ScrollLayerId,
    prim_index: PrimitiveIndex,
    border_radius: f32,
}

enum PrimitiveRunCmd {
    PushStackingContext(StackingContextIndex),
    PrimitiveRun(PrimitiveIndex, usize),
    PopStackingContext,
}

#[derive(Debug, Copy, Clone)]
pub enum PrimitiveFlags {
    None,
    Scrollbar(ScrollLayerId, f32)
}

// TODO(gw): I've had to make several of these types below public
//           with the changes for text-shadow. The proper solution
//           is to split the render task and render target code into
//           its own module. However, I'm avoiding that for now since
//           this PR is large enough already, and other people are working
//           on PRs that make use of render tasks.

#[derive(Debug, Copy, Clone)]
pub struct RenderTargetIndex(usize);

#[derive(Debug, Copy, Clone, Eq, PartialEq, Hash)]
struct RenderPassIndex(isize);

#[derive(Debug, Copy, Clone, Eq, Hash, PartialEq)]
pub struct RenderTaskIndex(usize);

type TileUniqueId = usize;

#[derive(Debug, Copy, Clone, Eq, PartialEq, Hash)]
pub enum MaskCacheKey {
    Primitive(PrimitiveIndex),
    Layer(StackingContextIndex),
}


#[derive(Debug, Copy, Clone, Eq, PartialEq, Hash)]
pub enum RenderTaskKey {
    /// Draw this primitive to a cache target.
    CachePrimitive(PrimitiveCacheKey),
    /// Draw the tile alpha mask for a primitive.
    CacheMask(MaskCacheKey, TileUniqueId),
    /// Apply a vertical blur pass of given radius for this primitive.
    VerticalBlur(i32, PrimitiveIndex),
    /// Apply a horizontal blur pass of given radius for this primitive.
    HorizontalBlur(i32, PrimitiveIndex),
}

#[derive(Debug, Copy, Clone)]
pub enum RenderTaskId {
    Static(RenderTaskIndex),
    Dynamic(RenderTaskKey),
}

struct DynamicTaskInfo {
    index: RenderTaskIndex,
    rect: DeviceIntRect,
}

struct RenderTaskCollection {
    render_task_data: Vec<RenderTaskData>,
    dynamic_tasks: HashMap<(RenderTaskKey, RenderPassIndex), DynamicTaskInfo, BuildHasherDefault<FnvHasher>>,
}

impl RenderTaskCollection {
    fn new(static_render_task_count: usize) -> RenderTaskCollection {
        RenderTaskCollection {
            render_task_data: vec![RenderTaskData::empty(); static_render_task_count],
            dynamic_tasks: HashMap::with_hasher(Default::default()),
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
                        RenderTaskLocation::Fixed(rect) => rect,
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

    fn has_dynamic_task(&self, key: &RenderTaskKey, pass_index: RenderPassIndex) -> bool {
        //TODO: remove clone
        self.dynamic_tasks.contains_key(&(key.clone(), pass_index))
    }
}

#[derive(Debug, Clone)]
pub struct RenderTaskData {
    data: [f32; FLOATS_PER_RENDER_TASK_INFO],
}

impl RenderTaskData {
    fn empty() -> RenderTaskData {
        RenderTaskData {
            data: unsafe { mem::uninitialized() }
        }
    }
}

impl Default for RenderTaskData {
    fn default() -> RenderTaskData {
        RenderTaskData {
            data: unsafe { mem::uninitialized() },
        }
    }
}

impl Default for PrimitiveGeometry {
    fn default() -> PrimitiveGeometry {
        PrimitiveGeometry {
            local_rect: unsafe { mem::uninitialized() },
            local_clip_rect: unsafe { mem::uninitialized() },
        }
    }
}

struct AlphaBatchTask {
    task_id: RenderTaskId,
    opaque_items: Vec<AlphaRenderItem>,
    alpha_items: Vec<AlphaRenderItem>,
    tile_id: TileUniqueId,
}

/// Encapsulates the logic of building batches for items that are blended.
pub struct AlphaBatcher {
    pub alpha_batches: Vec<PrimitiveBatch>,
    pub opaque_batches: Vec<PrimitiveBatch>,
    tasks: Vec<AlphaBatchTask>,
}

impl AlphaBatcher {
    fn new() -> AlphaBatcher {
        AlphaBatcher {
            alpha_batches: Vec::new(),
            opaque_batches: Vec::new(),
            tasks: Vec::new(),
        }
    }

    fn add_task(&mut self, task: AlphaBatchTask) {
        self.tasks.push(task);
    }

    fn build(&mut self,
             ctx: &RenderTargetContext,
             render_tasks: &RenderTaskCollection,
             child_pass_index: RenderPassIndex) {
        let mut alpha_batches: Vec<PrimitiveBatch> = vec![];
        let mut opaque_batches: Vec<PrimitiveBatch> = vec![];

        for task in &mut self.tasks {
            let task_index = render_tasks.get_static_task_index(&task.task_id);
            let mut existing_alpha_batch_index = 0;
            let mut existing_opaque_batch_index = 0;

            for item in &task.alpha_items {
                let batch_key = match item {
                    &AlphaRenderItem::Composite(..) => {
                        AlphaBatchKey::composite()
                    }
                    &AlphaRenderItem::Blend(..) => {
                        AlphaBatchKey::blend()
                    }
                    &AlphaRenderItem::Primitive(sc_index, prim_index, _) => {
                        // See if this task fits into the tile UBO
                        let layer = &ctx.layer_store[sc_index.0];
                        let prim_metadata = ctx.prim_store.get_metadata(prim_index);
                        let transform_kind = layer.xf_rect.as_ref().unwrap().kind;
                        let needs_clipping = prim_metadata.clip_cache_info.is_some() ||
                                             ctx.layer_masks_tasks.get(&(task.tile_id, sc_index)).is_some();
                        let needs_blending = transform_kind == TransformedRectKind::Complex ||
                                             !prim_metadata.is_opaque ||
                                             needs_clipping;
                        let blend_mode = ctx.prim_store.get_blend_mode(needs_blending, prim_metadata);
                        let needs_clipping_flag = if needs_clipping {
                            NEEDS_CLIPPING
                        } else {
                            AlphaBatchKeyFlags::empty()
                        };
                        let flags = match transform_kind {
                            TransformedRectKind::AxisAligned => AXIS_ALIGNED | needs_clipping_flag,
                            _ => needs_clipping_flag,
                        };
                        let batch_kind = ctx.prim_store.get_batch_kind(prim_metadata);

                        let textures = BatchTextures {
                            colors: ctx.prim_store.get_color_textures(prim_metadata),
                        };

                        AlphaBatchKey::primitive(batch_kind,
                                                 flags,
                                                 blend_mode,
                                                 textures)
                    }
                };

                while existing_alpha_batch_index < alpha_batches.len() &&
                        !alpha_batches[existing_alpha_batch_index].key.is_compatible_with(&batch_key) {
                    existing_alpha_batch_index += 1
                }

                if existing_alpha_batch_index == alpha_batches.len() {
                    let new_batch = match item {
                        &AlphaRenderItem::Composite(..) => {
                            PrimitiveBatch::composite()
                        }
                        &AlphaRenderItem::Blend(..) => {
                            PrimitiveBatch::blend()
                        }
                        &AlphaRenderItem::Primitive(_, prim_index, _) => {
                            // See if this task fits into the tile UBO
                            let prim_metadata = ctx.prim_store.get_metadata(prim_index);
                            let batch_kind = ctx.prim_store.get_batch_kind(prim_metadata);
                            PrimitiveBatch::new(batch_kind, batch_key)
                        }
                    };
                    alpha_batches.push(new_batch)
                }

                let batch = &mut alpha_batches[existing_alpha_batch_index];
                match item {
                    &AlphaRenderItem::Composite(src0_id, src1_id, info, z) => {
                        let ok = batch.pack_composite(render_tasks.get_static_task_index(&src0_id),
                                                      render_tasks.get_static_task_index(&src1_id),
                                                      render_tasks.get_static_task_index(&task.task_id),
                                                      info,
                                                      z);
                        debug_assert!(ok)
                    }
                    &AlphaRenderItem::Blend(src_id, info, z) => {
                        let ok = batch.pack_blend(render_tasks.get_static_task_index(&src_id),
                                                  render_tasks.get_static_task_index(&task.task_id),
                                                  info,
                                                  z);
                        debug_assert!(ok)
                    }
                    &AlphaRenderItem::Primitive(sc_index, prim_index, z) => {
                        let mask_task_index = match ctx.layer_masks_tasks.get(&(task.tile_id, sc_index)) {
                            Some(ref mask_task_id) => render_tasks.get_task_index(mask_task_id, child_pass_index),
                            None => OPAQUE_TASK_INDEX,
                        };
                        ctx.prim_store.add_prim_to_batch(prim_index,
                                                         batch,
                                                         sc_index,
                                                         task_index,
                                                         task.tile_id,
                                                         mask_task_index,
                                                         render_tasks,
                                                         child_pass_index,
                                                         z);
                    }
                }
            }

            for item in task.opaque_items.iter().rev() {
                let batch_key = match item {
                    &AlphaRenderItem::Composite(..) => unreachable!(),
                    &AlphaRenderItem::Blend(..) => unreachable!(),
                    &AlphaRenderItem::Primitive(sc_index, prim_index, _) => {
                        // See if this task fits into the tile UBO
                        let layer = &ctx.layer_store[sc_index.0];
                        let prim_metadata = ctx.prim_store.get_metadata(prim_index);
                        let transform_kind = layer.xf_rect.as_ref().unwrap().kind;
                        let needs_clipping = prim_metadata.clip_cache_info.is_some() ||
                                             ctx.layer_masks_tasks.get(&(task.tile_id, sc_index)).is_some();
                        let needs_blending = transform_kind == TransformedRectKind::Complex ||
                                             !prim_metadata.is_opaque ||
                                             needs_clipping;
                        let blend_mode = ctx.prim_store.get_blend_mode(needs_blending, prim_metadata);
                        let needs_clipping_flag = if needs_clipping {
                            NEEDS_CLIPPING
                        } else {
                            AlphaBatchKeyFlags::empty()
                        };
                        let flags = match transform_kind {
                            TransformedRectKind::AxisAligned => AXIS_ALIGNED | needs_clipping_flag,
                            _ => needs_clipping_flag,
                        };
                        let batch_kind = ctx.prim_store.get_batch_kind(prim_metadata);

                        let textures = BatchTextures {
                            colors: ctx.prim_store.get_color_textures(prim_metadata),
                        };

                        AlphaBatchKey::primitive(batch_kind,
                                                 flags,
                                                 blend_mode,
                                                 textures)
                    }
                };

                while existing_opaque_batch_index < opaque_batches.len() &&
                        !opaque_batches[existing_opaque_batch_index].key.is_compatible_with(&batch_key) {
                    existing_opaque_batch_index += 1
                }

                if existing_opaque_batch_index == opaque_batches.len() {
                    let new_batch = match item {
                        &AlphaRenderItem::Composite(..) => unreachable!(),
                        &AlphaRenderItem::Blend(..) => unreachable!(),
                        &AlphaRenderItem::Primitive(_, prim_index, _) => {
                            // See if this task fits into the tile UBO
                            let prim_metadata = ctx.prim_store.get_metadata(prim_index);
                            let batch_kind = ctx.prim_store.get_batch_kind(prim_metadata);
                            PrimitiveBatch::new(batch_kind, batch_key)
                        }
                    };
                    opaque_batches.push(new_batch)
                }

                let batch = &mut opaque_batches[existing_opaque_batch_index];
                match item {
                    &AlphaRenderItem::Composite(..) => unreachable!(),
                    &AlphaRenderItem::Blend(..) => unreachable!(),
                    &AlphaRenderItem::Primitive(sc_index, prim_index, z) => {
                        let mask_task_index = match ctx.layer_masks_tasks.get(&(task.tile_id, sc_index)) {
                            Some(ref mask_task_id) => render_tasks.get_task_index(mask_task_id, child_pass_index),
                            None => OPAQUE_TASK_INDEX,
                        };
                        ctx.prim_store.add_prim_to_batch(prim_index,
                                                         batch,
                                                         sc_index,
                                                         task_index,
                                                         task.tile_id,
                                                         mask_task_index,
                                                         render_tasks,
                                                         child_pass_index,
                                                         z);
                    }
                }
            }
        }

        self.alpha_batches.extend(alpha_batches.into_iter());
        self.opaque_batches.extend(opaque_batches.into_iter());
    }
}

/// Batcher managing draw calls into the clip mask (in the RT cache).
#[derive(Debug)]
pub struct ClipBatcher {
    /// Copy draws get the existing mask from a parent layer.
    pub copies: Vec<CacheClipInstance>,
    /// Rectangle draws fill up the rectangles with rounded corners.
    pub rectangles: Vec<CacheClipInstance>,
    /// Image draws apply the image masking.
    pub images: HashMap<SourceTexture, Vec<CacheClipInstance>>,
}

impl ClipBatcher {
    fn new() -> ClipBatcher {
        ClipBatcher {
            copies: Vec::new(),
            rectangles: Vec::new(),
            images: HashMap::new(),
        }
    }

    fn add(&mut self,
           task_index: RenderTaskIndex,
           base_task_index: Option<RenderTaskIndex>,
           clips: &[(StackingContextIndex, MaskCacheInfo)],
           resource_cache: &ResourceCache) {

        // TODO: don't draw clipping instances covering the whole tile
        for &(layer_id, ref info) in clips.iter() {
            let instance = CacheClipInstance {
                task_id: task_index.0 as i32,
                layer_index: layer_id.0 as i32,
                address: GpuStoreAddress(0),
                base_task_id: 0,
            };
            // copy on the first clip only
            if info as *const _ == &clips[0].1 as *const _ {
                if let Some(layer_task_id) = base_task_index {
                    self.copies.push(CacheClipInstance {
                        base_task_id: layer_task_id.0 as i32,
                        ..instance
                    });
                }
            }

            self.rectangles.extend((0 .. info.clip_range.item_count as usize)
                           .map(|region_id| {
                let offset = info.clip_range.start.0 + ((CLIP_DATA_GPU_SIZE * region_id) as i32);
                CacheClipInstance {
                    address: GpuStoreAddress(offset),
                    ..instance
                }
            }));

            if let Some((ref mask, address)) = info.image {
                let cache_item = resource_cache.get_cached_image(mask.image, ImageRendering::Auto);
                self.images.entry(cache_item.texture_id)
                           .or_insert(Vec::new())
                           .push(CacheClipInstance {
                    address: address,
                    ..instance
                })
            }
        }
    }
}


struct CompileTileContext<'a> {
    layer_store: &'a [StackingContext],
    prim_store: &'a PrimitiveStore,
    tile_id: TileUniqueId,
    render_task_id_counter: AtomicUsize,
}

struct RenderTargetContext<'a> {
    layer_store: &'a [StackingContext],
    prim_store: &'a PrimitiveStore,
    resource_cache: &'a ResourceCache,
    layer_masks_tasks: HashMap<(TileUniqueId, StackingContextIndex), RenderTaskId>,
}

/// A render target represents a number of rendering operations on a surface.
pub struct RenderTarget {
    pub alpha_batcher: AlphaBatcher,
    pub clip_batcher: ClipBatcher,
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
    page_allocator: TexturePage,
}

impl RenderTarget {
    fn new() -> RenderTarget {
        RenderTarget {
            alpha_batcher: AlphaBatcher::new(),
            clip_batcher: ClipBatcher::new(),
            box_shadow_cache_prims: Vec::new(),
            text_run_cache_prims: Vec::new(),
            text_run_textures: BatchTextures::no_texture(),
            vertical_blurs: Vec::new(),
            horizontal_blurs: Vec::new(),
            page_allocator: TexturePage::new(CacheTextureId(0),
                                             RENDERABLE_CACHE_SIZE as u32),
        }
    }

    fn build(&mut self,
             ctx: &RenderTargetContext,
             render_tasks: &mut RenderTaskCollection,
             child_pass_index: RenderPassIndex) {
        self.alpha_batcher.build(ctx,
                                 render_tasks,
                                 child_pass_index);
    }

    fn add_task(&mut self,
                task: RenderTask,
                ctx: &RenderTargetContext,
                render_tasks: &RenderTaskCollection,
                pass_index: RenderPassIndex) {
        match task.kind {
            RenderTaskKind::Alpha(info) => {
                self.alpha_batcher.add_task(AlphaBatchTask {
                    task_id: task.id,
                    opaque_items: info.opaque_items,
                    alpha_items: info.alpha_items,
                    tile_id: info.tile_id,
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

                match prim_metadata.prim_kind {
                    PrimitiveKind::BoxShadow => {
                        self.box_shadow_cache_prims.push(PrimitiveInstance {
                            global_prim_id: prim_index.0 as i32,
                            prim_address: prim_metadata.gpu_prim_index,
                            task_index: render_tasks.get_task_index(&task.id, pass_index).0 as i32,
                            clip_task_index: 0,
                            layer_index: 0,
                            sub_index: 0,
                            user_data: [0; 2],
                            z_sort_index: 0,        // z is disabled for rendering cache primitives
                        });
                    }
                    PrimitiveKind::TextRun => {
                        let text = &ctx.prim_store.cpu_text_runs[prim_metadata.cpu_prim_index.0];
                        // We only cache text runs with a text-shadow (for now).
                        debug_assert!(text.blur_radius.0 != 0);

                        // TODO(gw): This should always be fine for now, since the texture
                        // atlas grows to 4k. However, it won't be a problem soon, once
                        // we switch the texture atlas to use texture layers!
                        let textures = BatchTextures {
                            colors: ctx.prim_store.get_color_textures(prim_metadata),
                        };

                        debug_assert!(textures.colors[0] != SourceTexture::Invalid);
                        debug_assert!(self.text_run_textures.colors[0] == SourceTexture::Invalid ||
                                      self.text_run_textures.colors[0] == textures.colors[0]);
                        self.text_run_textures = textures;

                        for glyph_index in 0..prim_metadata.gpu_data_count {
                            self.text_run_cache_prims.push(PrimitiveInstance {
                                global_prim_id: prim_index.0 as i32,
                                prim_address: prim_metadata.gpu_prim_index,
                                task_index: render_tasks.get_task_index(&task.id, pass_index).0 as i32,
                                clip_task_index: 0,
                                layer_index: 0,
                                sub_index: prim_metadata.gpu_data_address.0 + glyph_index,
                                user_data: [ text.resource_address.0 + glyph_index, 0],
                                z_sort_index: 0,        // z is disabled for rendering cache primitives
                            });
                        }
                    }
                    _ => {
                        // No other primitives make use of primitive caching yet!
                        unreachable!()
                    }
                }
            }
            RenderTaskKind::CacheMask(ref task_info) => {
                let task_index = render_tasks.get_task_index(&task.id, pass_index);
                let base_task_id = task_info.base_task_id.map(|ref task_id|
                    render_tasks.get_task_index(task_id, pass_index)
                );
                self.clip_batcher.add(task_index, base_task_id,
                                      &task_info.clips,
                                      &ctx.resource_cache);
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
    pub targets: Vec<RenderTarget>,
}

impl RenderPass {
    fn new(pass_index: isize, is_framebuffer: bool) -> RenderPass {
        RenderPass {
            pass_index: RenderPassIndex(pass_index),
            is_framebuffer: is_framebuffer,
            targets: vec![ RenderTarget::new() ],
            tasks: vec![],
        }
    }

    fn add_render_task(&mut self, task: RenderTask) {
        self.tasks.push(task);
    }

    fn allocate_target(targets: &mut Vec<RenderTarget>, size: DeviceUintSize) -> DeviceUintPoint {
        let existing_origin = targets.last_mut()
                                     .unwrap()
                                     .page_allocator.allocate(&size);
        match existing_origin {
            Some(origin) => origin,
            None => {
                let mut new_target = RenderTarget::new();
                let origin = new_target.page_allocator
                                       .allocate(&size)
                                       .expect("Each render task must allocate <= size of one target!");
                targets.push(new_target);
                origin
            }
        }
    }


    fn build(&mut self,
             ctx: &RenderTargetContext,
             render_tasks: &mut RenderTaskCollection) {
        // Step through each task, adding to batches as appropriate.
        for mut task in self.tasks.drain(..) {
            // Find a target to assign this task to, or create a new
            // one if required.
            match task.location {
                RenderTaskLocation::Fixed(..) => {}
                RenderTaskLocation::Dynamic(ref mut origin, ref size) => {
                    // See if this task is a duplicate from another tile.
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
                    let alloc_origin = Self::allocate_target(&mut self.targets, alloc_size);

                    *origin = Some((DeviceIntPoint::new(alloc_origin.x as i32,
                                                     alloc_origin.y as i32),
                                    RenderTargetIndex(self.targets.len() - 1)));
                }
            }

            render_tasks.add(&task, self.pass_index);
            self.targets.last_mut().unwrap().add_task(task,
                                                      ctx,
                                                      render_tasks,
                                                      self.pass_index);
        }

        for target in &mut self.targets {
            let child_pass_index = RenderPassIndex(self.pass_index.0 - 1);
            target.build(ctx, render_tasks, child_pass_index);
        }
    }
}

#[derive(Debug, Clone)]
pub enum RenderTaskLocation {
    Fixed(DeviceIntRect),
    Dynamic(Option<(DeviceIntPoint, RenderTargetIndex)>, DeviceIntSize),
}

#[derive(Debug, Clone)]
enum AlphaRenderItem {
    Primitive(StackingContextIndex, PrimitiveIndex, i32),
    Blend(RenderTaskId, LowLevelFilterOp, i32),
    Composite(RenderTaskId, RenderTaskId, MixBlendMode, i32),
}

#[derive(Debug, Clone)]
pub struct AlphaRenderTask {
    actual_rect: DeviceIntRect,
    opaque_items: Vec<AlphaRenderItem>,
    alpha_items: Vec<AlphaRenderItem>,
    tile_id: TileUniqueId,
}

#[derive(Debug, Clone)]
pub struct CacheMaskTask {
    actual_rect: DeviceIntRect,
    base_task_id: Option<RenderTaskId>,
    clips: Vec<(StackingContextIndex, MaskCacheInfo)>,
}

#[derive(Debug)]
enum MaskResult {
    /// The mask is completely outside the region
    Outside,
    /// The mask completely covers the region
    Covering,
    /// The mask is inside and needs to be processed
    Inside(RenderTask),
}

#[derive(Debug, Clone)]
pub enum RenderTaskKind {
    Alpha(AlphaRenderTask),
    CachePrimitive(PrimitiveIndex),
    CacheMask(CacheMaskTask),
    VerticalBlur(DeviceIntLength, PrimitiveIndex),
    HorizontalBlur(DeviceIntLength, PrimitiveIndex),
}

// TODO(gw): Consider storing these in a separate array and having
//           primitives hold indices - this could avoid cloning
//           when adding them as child tasks to tiles.
#[derive(Debug, Clone)]
pub struct RenderTask {
    pub id: RenderTaskId,
    pub location: RenderTaskLocation,
    pub children: Vec<RenderTask>,
    pub kind: RenderTaskKind,
}

impl RenderTask {
    fn new_alpha_batch(actual_rect: DeviceIntRect, ctx: &CompileTileContext) -> RenderTask {
        let task_index = ctx.render_task_id_counter.fetch_add(1, Ordering::Relaxed);

        RenderTask {
            id: RenderTaskId::Static(RenderTaskIndex(task_index)),
            children: Vec::new(),
            location: RenderTaskLocation::Dynamic(None, actual_rect.size),
            kind: RenderTaskKind::Alpha(AlphaRenderTask {
                actual_rect: actual_rect,
                alpha_items: Vec::new(),
                opaque_items: Vec::new(),
                tile_id: ctx.tile_id,
            }),
        }
    }

    pub fn new_prim_cache(key: PrimitiveCacheKey,
                          size: DeviceIntSize,
                          prim_index: PrimitiveIndex) -> RenderTask {
        RenderTask {
            id: RenderTaskId::Dynamic(RenderTaskKey::CachePrimitive(key)),
            children: Vec::new(),
            location: RenderTaskLocation::Dynamic(None, size),
            kind: RenderTaskKind::CachePrimitive(prim_index),
        }
    }

    fn new_mask(actual_rect: DeviceIntRect,
                dependent: Option<&RenderTask>,
                mask_key: MaskCacheKey,
                top_clip: (StackingContextIndex, &MaskCacheInfo),
                layer_clips: &[(StackingContextIndex, MaskCacheInfo)],
                tile_id: TileUniqueId)
                -> MaskResult {

        let extra = (top_clip.0, top_clip.1.clone());

        // We scan through the clip stack and detect if our actual rectangle
        // is in the intersection of all of all the outer bounds,
        // and if it's completely inside the intersection of all of the inner bounds.
        let result = layer_clips.iter().chain(Some(&extra))
                                .fold(Some((actual_rect, true)), |current, clip| {
            current.and_then(|(rect, covering)|
                rect.intersection(&clip.1.outer_rect)
                    .map(|r| (r, covering & clip.1.inner_rect.contains_rect(&actual_rect))))
        });

        let task_rect = match result {
            None => return MaskResult::Outside,
            Some((_, true)) => return MaskResult::Covering,
            Some((rect, false)) => rect,
        };
        let clips = layer_clips.iter()
                               .map(|lc| lc.clone())
                               .chain(Some(extra))
                               .collect();

        MaskResult::Inside(RenderTask {
            id: RenderTaskId::Dynamic(RenderTaskKey::CacheMask(mask_key, tile_id)),
            children: match dependent {
                Some(task) => vec![task.clone()],
                None => Vec::new(),
            },
            location: RenderTaskLocation::Dynamic(None, task_rect.size),
            kind: RenderTaskKind::CacheMask(CacheMaskTask {
                actual_rect: task_rect,
                base_task_id: dependent.map(|task| task.id),
                clips: clips,
            }),
        })
    }

    // Construct a render task to apply a blur to a primitive. For now,
    // this is only used for text runs, but we can probably extend this
    // to handle general blurs to any render task in the future.
    // The render task chain that is constructed looks like:
    //
    //    PrimitiveCacheTask: Draw the text run.
    //           ^
    //           |
    //    VerticalBlurTask: Apply the separable vertical blur to the primitive.
    //           ^
    //           |
    //    HorizontalBlurTask: Apply the separable horizontal blur to the vertical blur.
    //           |
    //           +---- This is stored as the input task to the primitive shader.
    //
    pub fn new_blur(key: PrimitiveCacheKey,
                    size: DeviceIntSize,
                    blur_radius: DeviceIntLength,
                    prim_index: PrimitiveIndex) -> RenderTask {
        let prim_cache_task = RenderTask::new_prim_cache(key,
                                                         size,
                                                         prim_index);

        let blur_target_size = size + DeviceIntSize::new(2 * blur_radius.0,
                                                         2 * blur_radius.0);

        let blur_task_v = RenderTask {
            id: RenderTaskId::Dynamic(RenderTaskKey::VerticalBlur(blur_radius.0, prim_index)),
            children: vec![prim_cache_task],
            location: RenderTaskLocation::Dynamic(None, blur_target_size),
            kind: RenderTaskKind::VerticalBlur(blur_radius, prim_index),
        };

        let blur_task_h = RenderTask {
            id: RenderTaskId::Dynamic(RenderTaskKey::HorizontalBlur(blur_radius.0, prim_index)),
            children: vec![blur_task_v],
            location: RenderTaskLocation::Dynamic(None, blur_target_size),
            kind: RenderTaskKind::HorizontalBlur(blur_radius, prim_index),
        };

        blur_task_h
    }

    fn as_alpha_batch<'a>(&'a mut self) -> &'a mut AlphaRenderTask {
        match self.kind {
            RenderTaskKind::Alpha(ref mut task) => task,
            RenderTaskKind::CachePrimitive(..) |
            RenderTaskKind::CacheMask(..) |
            RenderTaskKind::VerticalBlur(..) |
            RenderTaskKind::HorizontalBlur(..) => unreachable!(),
        }
    }

    // Write (up to) 8 floats of data specific to the type
    // of render task that is provided to the GPU shaders
    // via a vertex texture.
    fn write_task_data(&self) -> RenderTaskData {
        let (target_rect, target_index) = self.get_target_rect();
        match self.kind {
            RenderTaskKind::Alpha(ref task) => {
                debug_assert_eq!(target_rect.size, task.actual_rect.size);
                RenderTaskData {
                    data: [
                        task.actual_rect.origin.x as f32,
                        task.actual_rect.origin.y as f32,
                        target_rect.origin.x as f32,
                        target_rect.origin.y as f32,
                        task.actual_rect.size.width as f32,
                        task.actual_rect.size.height as f32,
                        target_index.0 as f32,
                        0.0,
                    ],
                }
            }
            RenderTaskKind::CachePrimitive(..) => {
                RenderTaskData {
                    data: [
                        target_rect.origin.x as f32,
                        target_rect.origin.y as f32,
                        target_rect.size.width as f32,
                        target_rect.size.height as f32,
                        target_index.0 as f32,
                        0.0,
                        0.0,
                        0.0,
                    ],
                }
            }
            RenderTaskKind::CacheMask(ref task) => {
                debug_assert_eq!(target_rect.size, task.actual_rect.size);
                RenderTaskData {
                    data: [
                        target_rect.origin.x as f32,
                        target_rect.origin.y as f32,
                        (target_rect.origin.x + target_rect.size.width) as f32,
                        (target_rect.origin.y + target_rect.size.height) as f32,
                        task.actual_rect.origin.x as f32,
                        task.actual_rect.origin.y as f32,
                        target_index.0 as f32,
                        0.0,
                    ],
                }
            }
            RenderTaskKind::VerticalBlur(blur_radius, _) |
            RenderTaskKind::HorizontalBlur(blur_radius, _) => {
                RenderTaskData {
                    data: [
                        target_rect.origin.x as f32,
                        target_rect.origin.y as f32,
                        target_rect.size.width as f32,
                        target_rect.size.height as f32,
                        target_index.0 as f32,
                        blur_radius.0 as f32,
                        0.0,
                        0.0,
                    ]
                }
            }
        }
    }

    fn get_target_rect(&self) -> (DeviceIntRect, RenderTargetIndex) {
        match self.location {
            RenderTaskLocation::Fixed(rect) => (rect, RenderTargetIndex(0)),
            RenderTaskLocation::Dynamic(origin_and_target_index, size) => {
                let (origin, target_index) = origin_and_target_index.expect("Should have been allocated by now!");
                (DeviceIntRect::new(origin, size), target_index)
            }
        }
    }

    fn assign_to_passes(mut self,
                        pass_index: usize,
                        passes: &mut Vec<RenderPass>) {
        for child in self.children.drain(..) {
            child.assign_to_passes(pass_index - 1,
                                   passes);
        }

        // Sanity check - can be relaxed if needed
        match self.location {
            RenderTaskLocation::Fixed(..) => {
                debug_assert!(pass_index == passes.len() - 1);
            }
            RenderTaskLocation::Dynamic(..) => {
                debug_assert!(pass_index < passes.len() - 1);
            }
        }

        let pass = &mut passes[pass_index];
        pass.add_render_task(self);
    }

    fn max_depth(&self,
                 depth: usize,
                 max_depth: &mut usize) {
        let depth = depth + 1;
        *max_depth = cmp::max(*max_depth, depth);
        for child in &self.children {
            child.max_depth(depth, max_depth);
        }
    }
}

pub const SCREEN_TILE_SIZE: i32 = 256;
pub const RENDERABLE_CACHE_SIZE: i32 = 2048;

#[derive(Debug, Clone)]
pub struct DebugRect {
    pub label: String,
    pub color: ColorF,
    pub rect: DeviceIntRect,
}

#[derive(Copy, Clone, PartialEq, Eq, Hash, Debug)]
#[repr(u8)]
enum AlphaBatchKind {
    Composite = 0,
    Blend,
    Rectangle,
    TextRun,
    Image,
    YuvImage,
    Border,
    AlignedGradient,
    AngleGradient,
    BoxShadow,
    CacheImage,
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
    kind: AlphaBatchKind,
    pub flags: AlphaBatchKeyFlags,
    pub blend_mode: BlendMode,
    pub textures: BatchTextures,
}

impl AlphaBatchKey {
    fn blend() -> AlphaBatchKey {
        AlphaBatchKey {
            kind: AlphaBatchKind::Blend,
            flags: AXIS_ALIGNED,
            blend_mode: BlendMode::Alpha,
            textures: BatchTextures::no_texture(),
        }
    }

    fn composite() -> AlphaBatchKey {
        AlphaBatchKey {
            kind: AlphaBatchKind::Composite,
            flags: AXIS_ALIGNED,
            blend_mode: BlendMode::Alpha,
            textures: BatchTextures::no_texture(),
        }
    }

    fn primitive(kind: AlphaBatchKind,
                 flags: AlphaBatchKeyFlags,
                 blend_mode: BlendMode,
                 textures: BatchTextures)
                 -> AlphaBatchKey {
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
    base_task_id: i32,
}

#[derive(Debug, Clone)]
pub struct PrimitiveInstance {
    global_prim_id: i32,
    prim_address: GpuStoreAddress,
    task_index: i32,
    clip_task_index: i32,
    layer_index: i32,
    sub_index: i32,
    z_sort_index: i32,
    user_data: [i32; 2],
}

impl PrimitiveInstance {
    fn blend(src_task_id: i32,
             target_task_id: i32,
             op: i32,
             amount: i32,
             z_sort_index: i32) -> PrimitiveInstance {
        PrimitiveInstance {
            global_prim_id: -1,
            prim_address: GpuStoreAddress(0),
            task_index: target_task_id,
            clip_task_index: -1,
            layer_index: -1,
            sub_index: op,
            user_data: [src_task_id, amount],
            z_sort_index: z_sort_index,
        }
    }

    fn composite(src_tasks_id: [i32; 2],
                 target_task_id: i32,
                 op: i32,
                 z_sort_index: i32) -> PrimitiveInstance {
        PrimitiveInstance {
            global_prim_id: -1,
            prim_address: GpuStoreAddress(0),
            task_index: target_task_id,
            clip_task_index: -1,
            layer_index: -1,
            sub_index: op,
            user_data: src_tasks_id,
            z_sort_index: z_sort_index,
        }
    }
}

#[derive(Debug)]
pub enum PrimitiveBatchData {
    Rectangles(Vec<PrimitiveInstance>),
    TextRun(Vec<PrimitiveInstance>),
    Image(Vec<PrimitiveInstance>),
    YuvImage(Vec<PrimitiveInstance>),
    Borders(Vec<PrimitiveInstance>),
    AlignedGradient(Vec<PrimitiveInstance>),
    AngleGradient(Vec<PrimitiveInstance>),
    BoxShadow(Vec<PrimitiveInstance>),
    CacheImage(Vec<PrimitiveInstance>),
    Blend(Vec<PrimitiveInstance>),
    Composite(Vec<PrimitiveInstance>),
}

#[derive(Debug)]
pub struct PrimitiveBatch {
    pub key: AlphaBatchKey,
    pub data: PrimitiveBatchData,
}

impl PrimitiveBatch {
    fn blend() -> PrimitiveBatch {
        PrimitiveBatch {
            key: AlphaBatchKey::blend(),
            data: PrimitiveBatchData::Blend(Vec::new()),
        }
    }

    fn composite() -> PrimitiveBatch {
        PrimitiveBatch {
            key: AlphaBatchKey::composite(),
            data: PrimitiveBatchData::Composite(Vec::new()),
        }
    }

    fn pack_blend(&mut self,
                  src_rect_index: RenderTaskIndex,
                  target_rect_index: RenderTaskIndex,
                  filter: LowLevelFilterOp,
                  z_sort_index: i32) -> bool {
        match &mut self.data {
            &mut PrimitiveBatchData::Blend(ref mut ubo_data) => {
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

                ubo_data.push(PrimitiveInstance::blend(src_rect_index.0 as i32,
                                                       target_rect_index.0 as i32,
                                                       filter_mode,
                                                       (amount * 65535.0).round() as i32,
                                                       z_sort_index));
                true
            }
            _ => false
        }
    }

    fn pack_composite(&mut self,
                      rect0_index: RenderTaskIndex,
                      rect1_index: RenderTaskIndex,
                      target_rect_index: RenderTaskIndex,
                      info: MixBlendMode,
                      z_sort_index: i32) -> bool {
        match &mut self.data {
            &mut PrimitiveBatchData::Composite(ref mut ubo_data) => {
                ubo_data.push(PrimitiveInstance::composite([rect0_index.0 as i32,
                                                            rect1_index.0 as i32],
                                                           target_rect_index.0 as i32,
                                                           info as i32,
                                                           z_sort_index));
                true
            }
            _ => false
        }
    }

    fn new(batch_kind: AlphaBatchKind,
           key: AlphaBatchKey) -> PrimitiveBatch {
        let data = match batch_kind {
            AlphaBatchKind::Rectangle => PrimitiveBatchData::Rectangles(Vec::new()),
            AlphaBatchKind::TextRun => PrimitiveBatchData::TextRun(Vec::new()),
            AlphaBatchKind::Image => PrimitiveBatchData::Image(Vec::new()),
            AlphaBatchKind::YuvImage => PrimitiveBatchData::YuvImage(Vec::new()),
            AlphaBatchKind::Border => PrimitiveBatchData::Borders(Vec::new()),
            AlphaBatchKind::AlignedGradient => PrimitiveBatchData::AlignedGradient(Vec::new()),
            AlphaBatchKind::AngleGradient => PrimitiveBatchData::AngleGradient(Vec::new()),
            AlphaBatchKind::BoxShadow => PrimitiveBatchData::BoxShadow(Vec::new()),
            AlphaBatchKind::Blend | AlphaBatchKind::Composite => unreachable!(),
            AlphaBatchKind::CacheImage => PrimitiveBatchData::CacheImage(Vec::new()),
        };

        PrimitiveBatch {
            key: key,
            data: data,
        }
    }
}

#[derive(Debug, Copy, Clone, Eq, PartialEq, Hash)]
pub struct ScreenTileLayerIndex(usize);

#[derive(Debug, Copy, Clone, Eq, PartialEq, Hash, Ord, PartialOrd)]
pub struct StackingContextIndex(usize);

#[derive(Debug)]
struct TileRange {
    x0: i32,
    y0: i32,
    x1: i32,
    y1: i32,
}

struct StackingContext {
    pipeline_id: PipelineId,
    local_transform: LayerToScrollTransform,
    local_rect: LayerRect,
    scroll_layer_id: ScrollLayerId,
    xf_rect: Option<TransformedRect>,
    composite_kind: CompositeKind,
    tile_range: Option<TileRange>,
    clip_source: ClipSource,
    clip_cache_info: Option<MaskCacheInfo>,
}

#[derive(Debug, Clone)]
pub struct PackedStackingContext {
    transform: LayerToWorldTransform,
    inv_transform: WorldToLayerTransform,
    local_clip_rect: LayerRect,
    screen_vertices: [WorldPoint4D; 4],
}

impl Default for PackedStackingContext {
    fn default() -> PackedStackingContext {
        PackedStackingContext {
            transform: LayerToWorldTransform::identity(),
            inv_transform: WorldToLayerTransform::identity(),
            local_clip_rect: LayerRect::zero(),
            screen_vertices: [WorldPoint4D::zero(); 4],
        }
    }
}

#[derive(Debug, Copy, Clone)]
enum CompositeKind {
    None,
    // Requires only a single texture as input (e.g. most filters)
    Simple(LowLevelFilterOp),
    // Requires two source textures (e.g. mix-blend-mode)
    Complex(MixBlendMode),
}

impl CompositeKind {
    fn new(composition_ops: &[CompositionOp]) -> CompositeKind {
        if composition_ops.is_empty() {
            return CompositeKind::None;
        }

        match composition_ops.first().unwrap() {
            &CompositionOp::Filter(filter_op) => {
                match filter_op {
                    LowLevelFilterOp::Opacity(opacity) => {
                        let opacityf = opacity.to_f32_px();
                        if opacityf == 1.0 {
                            CompositeKind::None
                        } else {
                            CompositeKind::Simple(LowLevelFilterOp::Opacity(opacity))
                        }
                    }
                    other_filter => CompositeKind::Simple(other_filter),
                }
            }
            &CompositionOp::MixBlend(mode) => {
                CompositeKind::Complex(mode)
            }
        }
    }
}

impl StackingContext {
    fn is_visible(&self) -> bool {
        self.xf_rect.is_some()
    }

    fn can_contribute_to_scene(&self) -> bool {
        match self.composite_kind {
            CompositeKind::None | CompositeKind::Complex(..) => true,
            CompositeKind::Simple(LowLevelFilterOp::Opacity(opacity)) => opacity > Au(0),
            CompositeKind::Simple(..) => true,
        }
    }
}

#[derive(Debug, Clone)]
pub struct ClearTile {
    pub rect: DeviceIntRect,
}

#[derive(Clone, Copy)]
pub struct FrameBuilderConfig {
    pub enable_scrollbars: bool,
    pub enable_subpixel_aa: bool,
}

impl FrameBuilderConfig {
    pub fn new(enable_scrollbars: bool,
               enable_subpixel_aa: bool) -> FrameBuilderConfig {
        FrameBuilderConfig {
            enable_scrollbars: enable_scrollbars,
            enable_subpixel_aa: enable_subpixel_aa,
        }
    }
}

pub struct FrameBuilder {
    screen_rect: LayerRect,
    background_color: Option<ColorF>,
    prim_store: PrimitiveStore,
    cmds: Vec<PrimitiveRunCmd>,
    debug: bool,
    config: FrameBuilderConfig,

    layer_store: Vec<StackingContext>,
    packed_layers: Vec<PackedStackingContext>,

    scrollbar_prims: Vec<ScrollbarPrimitive>,
}

/// A rendering-oriented representation of frame::Frame built by the render backend
/// and presented to the renderer.
pub struct Frame {
    pub viewport_size: LayerSize,
    pub background_color: Option<ColorF>,
    pub device_pixel_ratio: f32,
    pub debug_rects: Vec<DebugRect>,
    pub cache_size: DeviceSize,
    pub passes: Vec<RenderPass>,
    pub profile_counters: FrameProfileCounters,

    pub layer_texture_data: Vec<PackedStackingContext>,
    pub render_task_data: Vec<RenderTaskData>,
    pub gpu_data16: Vec<GpuBlock16>,
    pub gpu_data32: Vec<GpuBlock32>,
    pub gpu_data64: Vec<GpuBlock64>,
    pub gpu_data128: Vec<GpuBlock128>,
    pub gpu_geometry: Vec<PrimitiveGeometry>,
    pub gpu_resource_rects: Vec<TexelRect>,

    // List of textures that we don't know about yet
    // from the backend thread. The render thread
    // will use a callback to resolve these and
    // patch the data structures.
    pub deferred_resolves: Vec<DeferredResolve>,
}

#[derive(Debug)]
struct LayerMasksTasks {
    task_ids: Vec<Option<RenderTaskId>>,
}

impl LayerMasksTasks {
    fn new() -> LayerMasksTasks {
        LayerMasksTasks {
            task_ids: Vec::new(),
        }
    }

    fn add(&mut self, index: StackingContextIndex, task_id: RenderTaskId) {
        while self.task_ids.len() <= index.0 {
            self.task_ids.push(None);
        }
        assert!(self.task_ids[index.0].is_none());
        self.task_ids[index.0] = Some(task_id);
    }
}

/// Some extra per-tile information stored for debugging purposes.
#[derive(Debug)]
struct CompiledScreenTileInfo {
    cmd_count: usize,
    prim_count: usize,
}

#[derive(Debug)]
struct CompiledScreenTile {
    main_render_task: RenderTask,
    required_pass_count: usize,
    info: CompiledScreenTileInfo,
    unique_id: TileUniqueId,
    layer_masks_tasks: LayerMasksTasks,
}

impl CompiledScreenTile {
    fn new(main_render_task: RenderTask,
           info: CompiledScreenTileInfo,
           unique_id: TileUniqueId,
           layer_masks_tasks: LayerMasksTasks)
           -> CompiledScreenTile {
        let mut required_pass_count = 0;
        main_render_task.max_depth(0, &mut required_pass_count);

        CompiledScreenTile {
            main_render_task: main_render_task,
            required_pass_count: required_pass_count,
            info: info,
            unique_id: unique_id,
            layer_masks_tasks: layer_masks_tasks,
        }
    }

    fn build(self, passes: &mut Vec<RenderPass>) {
        self.main_render_task.assign_to_passes(passes.len() - 1,
                                               passes);
    }
}

#[derive(Debug, Eq, PartialEq, Copy, Clone)]
enum TileCommand {
    PushLayer(StackingContextIndex),
    PopLayer,
    DrawPrimitive(PrimitiveIndex),
}

#[derive(Debug)]
struct ScreenTile {
    rect: DeviceIntRect,
    cmds: Vec<TileCommand>,
    prim_count: usize,
}

impl ScreenTile {
    fn new(rect: DeviceIntRect) -> ScreenTile {
        ScreenTile {
            rect: rect,
            cmds: Vec::new(),
            prim_count: 0,
        }
    }

    #[inline(always)]
    fn push_layer(&mut self,
                  sc_index: StackingContextIndex) {
        self.cmds.push(TileCommand::PushLayer(sc_index));
    }

    #[inline(always)]
    fn push_primitive(&mut self, prim_index: PrimitiveIndex) {
        self.cmds.push(TileCommand::DrawPrimitive(prim_index));
        self.prim_count += 1;
    }

    #[inline(always)]
    fn pop_layer(&mut self, sc_index: StackingContextIndex) {
        let last_cmd = *self.cmds.last().unwrap();
        if last_cmd == TileCommand::PushLayer(sc_index) {
            self.cmds.pop();
        } else {
            self.cmds.push(TileCommand::PopLayer);
        }
    }

    fn compile(self, ctx: &CompileTileContext) -> Option<CompiledScreenTile> {
        if self.prim_count == 0 {
            return None;
        }

        let cmd_count = self.cmds.len();
        let mut actual_prim_count = 0;
        let mut next_z = 0;

        let mut sc_stack = Vec::new();
        let mut current_task = RenderTask::new_alpha_batch(self.rect, ctx);
        let mut alpha_task_stack = Vec::new();
        let mut clip_info_stack = Vec::new();
        let mut clip_task_stack = Vec::new();
        let mut num_clips_to_skip = 0;
        let mut layer_masks_tasks = LayerMasksTasks::new();

        for cmd in self.cmds {
            match cmd {
                TileCommand::PushLayer(sc_index) => {
                    sc_stack.push(sc_index);

                    let layer = &ctx.layer_store[sc_index.0];
                    match layer.composite_kind {
                        CompositeKind::None => {}
                        CompositeKind::Simple(..) | CompositeKind::Complex(..) => {
                            let layer_rect = layer.xf_rect.as_ref().unwrap().bounding_rect;
                            let needed_rect = layer_rect.intersection(&self.rect)
                                                        .expect("bug if these don't overlap");
                            let prev_task = mem::replace(&mut current_task,
                                                         RenderTask::new_alpha_batch(needed_rect, ctx));
                            alpha_task_stack.push(prev_task);
                        }
                    }

                    // Create a task for the layer mask, if needed,
                    // i.e. if there are rounded corners or image masks for the layer.
                    if let Some(ref clip_info) = layer.clip_cache_info {
                        if CLIP_TASK_COLLAPSE {
                            clip_info_stack.push((sc_index, clip_info.clone()));
                        } else {
                            let mask_opt = RenderTask::new_mask(self.rect,
                                                                clip_task_stack.last(),
                                                                MaskCacheKey::Layer(sc_index),
                                                                (sc_index, clip_info),
                                                                &clip_info_stack,
                                                                ctx.tile_id);
                            match mask_opt {
                                MaskResult::Inside(mask_task) => {
                                    current_task.children.push(mask_task.clone());
                                    clip_task_stack.push(mask_task);
                                    num_clips_to_skip = 0;
                                }
                                _ => num_clips_to_skip += 1,
                            }
                        }
                    }
                    // Register the layer mask task within the context
                    if let Some(ref mask_task) = clip_task_stack.last() {
                        layer_masks_tasks.add(sc_index, mask_task.id);
                    }
                }
                TileCommand::PopLayer => {
                    let sc_index = sc_stack.pop().unwrap();

                    let layer = &ctx.layer_store[sc_index.0];
                    match layer.composite_kind {
                        CompositeKind::None => {}
                        CompositeKind::Simple(info) => {
                            let mut prev_task = alpha_task_stack.pop().unwrap();
                            let item = AlphaRenderItem::Blend(current_task.id, info, next_z);
                            next_z += 1;
                            prev_task.as_alpha_batch().alpha_items.push(item);
                            prev_task.children.push(current_task);
                            current_task = prev_task;
                        }
                        CompositeKind::Complex(info) => {
                            let backdrop = alpha_task_stack.pop().unwrap();

                            let mut composite_task = RenderTask::new_alpha_batch(self.rect, ctx);

                            let item = AlphaRenderItem::Composite(backdrop.id,
                                                                  current_task.id,
                                                                  info,
                                                                  next_z);
                            next_z += 1;
                            composite_task.as_alpha_batch().alpha_items.push(item);

                            composite_task.children.push(backdrop);
                            composite_task.children.push(current_task);

                            current_task = composite_task;
                        }
                    }

                    if layer.clip_cache_info.is_some() {
                        if CLIP_TASK_COLLAPSE {
                            clip_info_stack.pop().unwrap();
                        } else {
                            if num_clips_to_skip > 0 {
                                num_clips_to_skip -= 1;
                            } else {
                                clip_task_stack.pop().unwrap();
                            }
                        }
                    }
                }
                TileCommand::DrawPrimitive(prim_index) => {
                    let sc_index = *sc_stack.last().unwrap();
                    let prim_metadata = ctx.prim_store.get_metadata(prim_index);

                    // Add a task to render the updated image mask
                    if let Some(ref clip_info) = prim_metadata.clip_cache_info {
                        let mask_opt = RenderTask::new_mask(self.rect,
                                                            clip_task_stack.last(),
                                                            MaskCacheKey::Primitive(prim_index),
                                                            (sc_index, clip_info),
                                                            &clip_info_stack,
                                                            ctx.tile_id);
                        match mask_opt {
                            MaskResult::Outside => panic!("Primitive be culled by `assign_prims_to_screen_tiles` already"),
                            MaskResult::Covering => (), //do nothing
                            MaskResult::Inside(task) => current_task.children.push(task),
                        }
                    }

                    // Add any dynamic render tasks needed to render this primitive
                    if let Some(ref render_task) = prim_metadata.render_task {
                        current_task.children.push(render_task.clone());
                    }

                    actual_prim_count += 1;

                    let layer = &ctx.layer_store[sc_index.0];
                    let transform_kind = layer.xf_rect.as_ref().unwrap().kind;
                    let needs_clipping = layer.clip_cache_info.is_some() || prim_metadata.clip_cache_info.is_some();
                    let needs_blending = transform_kind == TransformedRectKind::Complex ||
                                         !prim_metadata.is_opaque ||
                                         needs_clipping;

                    let items = if needs_blending {
                        &mut current_task.as_alpha_batch().alpha_items
                    } else {
                        &mut current_task.as_alpha_batch().opaque_items
                    };
                    items.push(AlphaRenderItem::Primitive(sc_index, prim_index, next_z));
                    next_z += 1;
                }
            }
        }

        debug_assert!(alpha_task_stack.is_empty());
        debug_assert!(clip_task_stack.is_empty());

        let info = CompiledScreenTileInfo {
            cmd_count: cmd_count,
            prim_count: actual_prim_count,
        };

        current_task.location = RenderTaskLocation::Fixed(self.rect);
        Some(CompiledScreenTile::new(current_task, info, ctx.tile_id, layer_masks_tasks))
    }
}

impl FrameBuilder {
    pub fn new(viewport_size: LayerSize,
               background_color: Option<ColorF>,
               debug: bool,
               config: FrameBuilderConfig) -> FrameBuilder {
        FrameBuilder {
            screen_rect: LayerRect::new(LayerPoint::zero(), viewport_size),
            background_color: background_color,
            layer_store: Vec::new(),
            prim_store: PrimitiveStore::new(),
            cmds: Vec::new(),
            debug: debug,
            packed_layers: Vec::new(),
            scrollbar_prims: Vec::new(),
            config: config,
        }
    }

    fn add_primitive(&mut self,
                     rect: &LayerRect,
                     clip_region: &ClipRegion,
                     container: PrimitiveContainer) -> PrimitiveIndex {

        let geometry = PrimitiveGeometry {
            local_rect: *rect,
            local_clip_rect: clip_region.main,
        };
        let clip_source = if clip_region.is_complex() {
            ClipSource::Region(clip_region.clone())
        } else {
            ClipSource::NoClip
        };
        let clip_info = MaskCacheInfo::new(&clip_source,
                                           &mut self.prim_store.gpu_data32);

        let prim_index = self.prim_store.add_primitive(geometry,
                                                       Box::new(clip_source),
                                                       clip_info,
                                                       container);

        match self.cmds.last_mut().unwrap() {
            &mut PrimitiveRunCmd::PrimitiveRun(_run_prim_index, ref mut count) => {
                debug_assert!(_run_prim_index.0 + *count == prim_index.0);
                *count += 1;
                return prim_index;
            }
            &mut PrimitiveRunCmd::PushStackingContext(..) |
            &mut PrimitiveRunCmd::PopStackingContext => {}
        }

        self.cmds.push(PrimitiveRunCmd::PrimitiveRun(prim_index, 1));

        prim_index
    }

    pub fn push_layer(&mut self,
                      rect: LayerRect,
                      clip_region: &ClipRegion,
                      transform: LayerToScrollTransform,
                      pipeline_id: PipelineId,
                      scroll_layer_id: ScrollLayerId,
                      composition_operations: &[CompositionOp]) {
        let sc_index = StackingContextIndex(self.layer_store.len());

        let clip_source = clip_region.into();
        let clip_info = MaskCacheInfo::new(&clip_source,
                                           &mut self.prim_store.gpu_data32);

        let sc = StackingContext {
            local_rect: rect,
            local_transform: transform,
            scroll_layer_id: scroll_layer_id,
            pipeline_id: pipeline_id,
            xf_rect: None,
            composite_kind: CompositeKind::new(composition_operations),
            tile_range: None,
            clip_source: clip_source,
            clip_cache_info: clip_info,
        };
        self.layer_store.push(sc);

        self.packed_layers.push(PackedStackingContext {
            transform: LayerToWorldTransform::identity(),
            inv_transform: WorldToLayerTransform::identity(),
            screen_vertices: [WorldPoint4D::zero(); 4],
            local_clip_rect: LayerRect::zero(),
        });

        self.cmds.push(PrimitiveRunCmd::PushStackingContext(sc_index));
    }

    pub fn pop_layer(&mut self) {
        self.cmds.push(PrimitiveRunCmd::PopStackingContext);
    }

    pub fn add_solid_rectangle(&mut self,
                               rect: &LayerRect,
                               clip_region: &ClipRegion,
                               color: &ColorF,
                               flags: PrimitiveFlags) {
        if color.a == 0.0 {
            return;
        }

        let prim = RectanglePrimitive {
            color: *color,
        };

        let prim_index = self.add_primitive(rect,
                                            clip_region,
                                            PrimitiveContainer::Rectangle(prim));

        match flags {
            PrimitiveFlags::None => {}
            PrimitiveFlags::Scrollbar(scroll_layer_id, border_radius) => {
                self.scrollbar_prims.push(ScrollbarPrimitive {
                    prim_index: prim_index,
                    scroll_layer_id: scroll_layer_id,
                    border_radius: border_radius,
                });
            }
        }
    }

    pub fn supported_style(&mut self, border: &BorderSide) -> bool {
        match border.style {
            BorderStyle::Solid |
            BorderStyle::None |
            BorderStyle::Dotted |
            BorderStyle::Dashed |
            BorderStyle::Inset |
            BorderStyle::Ridge |
            BorderStyle::Groove |
            BorderStyle::Outset |
            BorderStyle::Double => {
                return true;
            }
            _ => {
                println!("TODO: Other border styles {:?}", border.style);
                return false;
            }
        }
    }

    pub fn add_border(&mut self,
                      rect: LayerRect,
                      clip_region: &ClipRegion,
                      border: &BorderDisplayItem) {
        let radius = &border.radius;
        let left = &border.left;
        let right = &border.right;
        let top = &border.top;
        let bottom = &border.bottom;

        if !self.supported_style(left) || !self.supported_style(right) ||
           !self.supported_style(top) || !self.supported_style(bottom) {
            println!("Unsupported border style, not rendering border");
            return;
        }

        // These colors are used during inset/outset scaling.
        let left_color      = left.border_color(1.0, 2.0/3.0, 0.3, 0.7);
        let top_color       = top.border_color(1.0, 2.0/3.0, 0.3, 0.7);
        let right_color     = right.border_color(2.0/3.0, 1.0, 0.7, 0.3);
        let bottom_color    = bottom.border_color(2.0/3.0, 1.0, 0.7, 0.3);

        let tl_outer = LayerPoint::new(rect.origin.x, rect.origin.y);
        let tl_inner = tl_outer + LayerPoint::new(radius.top_left.width.max(left.width),
                                                  radius.top_left.height.max(top.width));

        let tr_outer = LayerPoint::new(rect.origin.x + rect.size.width, rect.origin.y);
        let tr_inner = tr_outer + LayerPoint::new(-radius.top_right.width.max(right.width),
                                                  radius.top_right.height.max(top.width));

        let bl_outer = LayerPoint::new(rect.origin.x, rect.origin.y + rect.size.height);
        let bl_inner = bl_outer + LayerPoint::new(radius.bottom_left.width.max(left.width),
                                                  -radius.bottom_left.height.max(bottom.width));

        let br_outer = LayerPoint::new(rect.origin.x + rect.size.width,
                                       rect.origin.y + rect.size.height);
        let br_inner = br_outer - LayerPoint::new(radius.bottom_right.width.max(right.width),
                                                  radius.bottom_right.height.max(bottom.width));

        //Note: while similar to `ComplexClipRegion::get_inner_rect()` in spirit,
        // this code is a bit more complex and can not there for be merged.
        let inner_rect = rect_from_points_f(tl_inner.x.max(bl_inner.x),
                                            tl_inner.y.max(tr_inner.y),
                                            tr_inner.x.min(br_inner.x),
                                            bl_inner.y.min(br_inner.y));

        let prim_cpu = BorderPrimitiveCpu {
            inner_rect: LayerRect::from_untyped(&inner_rect),
        };

        let prim_gpu = BorderPrimitiveGpu {
            colors: [ left_color, top_color, right_color, bottom_color ],
            widths: [ left.width, top.width, right.width, bottom.width ],
            style: [
                pack_as_float(left.style as u32),
                pack_as_float(top.style as u32),
                pack_as_float(right.style as u32),
                pack_as_float(bottom.style as u32),
            ],
            radii: [
                radius.top_left,
                radius.top_right,
                radius.bottom_right,
                radius.bottom_left,
            ],
        };

        self.add_primitive(&rect,
                           clip_region,
                           PrimitiveContainer::Border(prim_cpu, prim_gpu));
    }

    pub fn add_gradient(&mut self,
                        rect: LayerRect,
                        clip_region: &ClipRegion,
                        start_point: LayerPoint,
                        end_point: LayerPoint,
                        stops: ItemRange) {
        // Fast paths for axis-aligned gradients:
        let mut reverse_stops = false;
        let kind = if start_point.x == end_point.x {
            GradientType::Vertical
        } else if start_point.y == end_point.y {
            GradientType::Horizontal
        } else {
            reverse_stops = start_point.x > end_point.x;
            GradientType::Rotated
        };

        let gradient_cpu = GradientPrimitiveCpu {
            stops_range: stops,
            kind: kind,
            reverse_stops: reverse_stops,
            cache_dirty: true,
        };

        // To get reftests exactly matching with reverse start/end
        // points, it's necessary to reverse the gradient
        // line in some cases.
        let (sp, ep) = if reverse_stops {
            (end_point, start_point)
        } else {
            (start_point, end_point)
        };

        let gradient_gpu = GradientPrimitiveGpu {
            start_point: sp,
            end_point: ep,
            padding: [0.0, 0.0, 0.0],
            kind: pack_as_float(kind as u32),
        };

        self.add_primitive(&rect,
                           clip_region,
                           PrimitiveContainer::Gradient(gradient_cpu, gradient_gpu));
    }

    pub fn add_text(&mut self,
                    rect: LayerRect,
                    clip_region: &ClipRegion,
                    font_key: FontKey,
                    size: Au,
                    blur_radius: Au,
                    color: &ColorF,
                    glyph_range: ItemRange) {
        if color.a == 0.0 {
            return
        }

        if size.0 <= 0 {
            return
        }

        let (render_mode, glyphs_per_run) = if blur_radius == Au(0) {
            // TODO(gw): Use a proper algorithm to select
            // whether this item should be rendered with
            // subpixel AA!
            let render_mode = if self.config.enable_subpixel_aa {
                FontRenderMode::Subpixel
            } else {
                FontRenderMode::Alpha
            };

            (render_mode, 8)
        } else {
            // TODO(gw): Support breaking up text shadow when
            // the size of the text run exceeds the dimensions
            // of the render target texture.
            (FontRenderMode::Alpha, glyph_range.length)
        };

        let text_run_count = (glyph_range.length + glyphs_per_run - 1) / glyphs_per_run;
        for run_index in 0..text_run_count {
            let start = run_index * glyphs_per_run;
            let end = cmp::min(start + glyphs_per_run, glyph_range.length);
            let sub_range = ItemRange {
                start: glyph_range.start + start,
                length: end - start,
            };

            let prim_cpu = TextRunPrimitiveCpu {
                font_key: font_key,
                logical_font_size: size,
                blur_radius: blur_radius,
                glyph_range: sub_range,
                cache_dirty: true,
                glyph_indices: Vec::new(),
                color_texture_id: SourceTexture::Invalid,
                color: *color,
                render_mode: render_mode,
                resource_address: GpuStoreAddress(0),
            };

            let prim_gpu = TextRunPrimitiveGpu {
                color: *color,
            };

            self.add_primitive(&rect,
                               clip_region,
                               PrimitiveContainer::TextRun(prim_cpu, prim_gpu));
        }
    }

    pub fn add_box_shadow(&mut self,
                          box_bounds: &LayerRect,
                          clip_region: &ClipRegion,
                          box_offset: &LayerPoint,
                          color: &ColorF,
                          blur_radius: f32,
                          spread_radius: f32,
                          border_radius: f32,
                          clip_mode: BoxShadowClipMode) {
        if color.a == 0.0 {
            return
        }

        // Fast path.
        if blur_radius == 0.0 && spread_radius == 0.0 && clip_mode == BoxShadowClipMode::None {
            self.add_solid_rectangle(&box_bounds,
                                     clip_region,
                                     color,
                                     PrimitiveFlags::None);
            return;
        }

        let bs_rect = box_bounds.translate(box_offset)
                                .inflate(spread_radius, spread_radius);

        let outside_edge_size = 2.0 * blur_radius;
        let inside_edge_size = outside_edge_size.max(border_radius);
        let edge_size = outside_edge_size + inside_edge_size;
        let outer_rect = bs_rect.inflate(outside_edge_size, outside_edge_size);
        let mut instance_rects = Vec::new();
        let (prim_rect, inverted) = match clip_mode {
            BoxShadowClipMode::Outset | BoxShadowClipMode::None => {
                subtract_rect(&outer_rect, box_bounds, &mut instance_rects);
                (outer_rect, 0.0)
            }
            BoxShadowClipMode::Inset => {
                subtract_rect(box_bounds, &bs_rect, &mut instance_rects);
                (*box_bounds, 1.0)
            }
        };

        if edge_size == 0.0 {
            for rect in &instance_rects {
                self.add_solid_rectangle(rect,
                                         clip_region,
                                         color,
                                         PrimitiveFlags::None)
            }
        } else {
            let prim_gpu = BoxShadowPrimitiveGpu {
                src_rect: *box_bounds,
                bs_rect: bs_rect,
                color: *color,
                blur_radius: blur_radius,
                border_radius: border_radius,
                edge_size: edge_size,
                inverted: inverted,
            };

            self.add_primitive(&prim_rect,
                               clip_region,
                               PrimitiveContainer::BoxShadow(prim_gpu, instance_rects));
        }
    }

    pub fn add_webgl_rectangle(&mut self,
                               rect: LayerRect,
                               clip_region: &ClipRegion,
                               context_id: WebGLContextId) {
        let prim_cpu = ImagePrimitiveCpu {
            kind: ImagePrimitiveKind::WebGL(context_id),
            color_texture_id: SourceTexture::Invalid,
            resource_address: GpuStoreAddress(0),
        };

        let prim_gpu = ImagePrimitiveGpu {
            stretch_size: rect.size,
            tile_spacing: LayerSize::zero(),
        };

        self.add_primitive(&rect,
                           clip_region,
                           PrimitiveContainer::Image(prim_cpu, prim_gpu));
    }

    pub fn add_image(&mut self,
                     rect: LayerRect,
                     clip_region: &ClipRegion,
                     stretch_size: &LayerSize,
                     tile_spacing: &LayerSize,
                     image_key: ImageKey,
                     image_rendering: ImageRendering) {
        let prim_cpu = ImagePrimitiveCpu {
            kind: ImagePrimitiveKind::Image(image_key,
                                            image_rendering,
                                            *tile_spacing),
            color_texture_id: SourceTexture::Invalid,
            resource_address: GpuStoreAddress(0),
        };

        let prim_gpu = ImagePrimitiveGpu {
            stretch_size: *stretch_size,
            tile_spacing: *tile_spacing,
        };

        self.add_primitive(&rect,
                           clip_region,
                           PrimitiveContainer::Image(prim_cpu, prim_gpu));
    }

    pub fn add_yuv_image(&mut self,
                         rect: LayerRect,
                         clip_region: &ClipRegion,
                         y_image_key: ImageKey,
                         u_image_key: ImageKey,
                         v_image_key: ImageKey,
                         color_space: YuvColorSpace) {

        let prim_cpu = YuvImagePrimitiveCpu {
            y_key: y_image_key,
            u_key: u_image_key,
            v_key: v_image_key,
            y_texture_id: SourceTexture::Invalid,
            u_texture_id: SourceTexture::Invalid,
            v_texture_id: SourceTexture::Invalid,
        };

        let prim_gpu = YuvImagePrimitiveGpu::new(rect.size, color_space);

        self.add_primitive(&rect,
                           clip_region,
                           PrimitiveContainer::YuvImage(prim_cpu, prim_gpu));
    }

    /// Compute the contribution (bounding rectangles, and resources) of layers and their
    /// primitives in screen space.
    fn cull_layers(&mut self,
                   screen_rect: &DeviceIntRect,
                   layer_map: &LayerMap,
                   auxiliary_lists_map: &AuxiliaryListsMap,
                   x_tile_count: i32,
                   y_tile_count: i32,
                   resource_cache: &mut ResourceCache,
                   profile_counters: &mut FrameProfileCounters,
                   device_pixel_ratio: f32) {
        // Build layer screen rects.
        // TODO(gw): This can be done earlier once update_layer_transforms() is fixed.

        // TODO(gw): Remove this stack once the layers refactor is done!
        let mut layer_stack: Vec<StackingContextIndex> = Vec::new();

        for cmd in &self.cmds {
            match cmd {
                &PrimitiveRunCmd::PushStackingContext(sc_index) => {
                    layer_stack.push(sc_index);
                    let layer = &mut self.layer_store[sc_index.0];
                    let packed_layer = &mut self.packed_layers[sc_index.0];

                    layer.xf_rect = None;
                    layer.tile_range = None;

                    let scroll_layer = &layer_map[&layer.scroll_layer_id];
                    packed_layer.transform = scroll_layer.world_content_transform
                                                         .with_source::<ScrollLayerPixel>() // the scroll layer is considered a parent of layer
                                                         .pre_mul(&layer.local_transform);
                    packed_layer.inv_transform = packed_layer.transform.inverse().unwrap();

                    if !layer.can_contribute_to_scene() {
                        continue;
                    }

                    let inv_layer_transform = layer.local_transform.inverse().unwrap();
                    let local_viewport_rect = as_scroll_parent_rect(&scroll_layer.combined_local_viewport_rect);
                    let viewport_rect = inv_layer_transform.transform_rect(&local_viewport_rect);
                    let local_clip_rect = layer.clip_source.to_rect().unwrap_or(layer.local_rect);
                    let layer_local_rect = layer.local_rect
                                                .intersection(&viewport_rect)
                                                .and_then(|rect| rect.intersection(&local_clip_rect));

                    if let Some(layer_local_rect) = layer_local_rect {
                        let layer_xf_rect = TransformedRect::new(&layer_local_rect,
                                                                 &packed_layer.transform,
                                                                 device_pixel_ratio);

                        if layer_xf_rect.bounding_rect.intersects(&screen_rect) {
                            packed_layer.screen_vertices = layer_xf_rect.vertices.clone();
                            packed_layer.local_clip_rect = layer_local_rect;

                            let layer_rect = layer_xf_rect.bounding_rect;
                            layer.xf_rect = Some(layer_xf_rect);

                            let tile_x0 = layer_rect.origin.x / SCREEN_TILE_SIZE;
                            let tile_y0 = layer_rect.origin.y / SCREEN_TILE_SIZE;
                            let tile_x1 = (layer_rect.origin.x + layer_rect.size.width + SCREEN_TILE_SIZE - 1) / SCREEN_TILE_SIZE;
                            let tile_y1 = (layer_rect.origin.y + layer_rect.size.height + SCREEN_TILE_SIZE - 1) / SCREEN_TILE_SIZE;

                            let tile_x0 = cmp::min(tile_x0, x_tile_count);
                            let tile_x0 = cmp::max(tile_x0, 0);
                            let tile_x1 = cmp::min(tile_x1, x_tile_count);
                            let tile_x1 = cmp::max(tile_x1, 0);

                            let tile_y0 = cmp::min(tile_y0, y_tile_count);
                            let tile_y0 = cmp::max(tile_y0, 0);
                            let tile_y1 = cmp::min(tile_y1, y_tile_count);
                            let tile_y1 = cmp::max(tile_y1, 0);

                            layer.tile_range = Some(TileRange {
                                x0: tile_x0,
                                y0: tile_y0,
                                x1: tile_x1,
                                y1: tile_y1,
                            });
                        }
                    }

                    if let Some(ref mut clip_info) = layer.clip_cache_info {
                        let auxiliary_lists = auxiliary_lists_map.get(&layer.pipeline_id)
                                                                 .expect("No auxiliary lists?");
                        clip_info.update(&layer.clip_source,
                                         &packed_layer.transform,
                                         &mut self.prim_store.gpu_data32,
                                         device_pixel_ratio,
                                         auxiliary_lists);
                        if let ClipSource::Region(ClipRegion{ image_mask: Some(ref mask), .. }) = layer.clip_source {
                            resource_cache.request_image(mask.image, ImageRendering::Auto);
                            //Note: no need to add the layer for resolve, all layers get resolved
                        }
                    }

                }
                &PrimitiveRunCmd::PrimitiveRun(prim_index, prim_count) => {
                    let sc_index = layer_stack.last().unwrap();
                    let layer = &mut self.layer_store[sc_index.0];
                    if !layer.is_visible() {
                        continue;
                    }

                    let packed_layer = &self.packed_layers[sc_index.0];
                    let auxiliary_lists = auxiliary_lists_map.get(&layer.pipeline_id)
                                                             .expect("No auxiliary lists?");

                    for i in 0..prim_count {
                        let prim_index = PrimitiveIndex(prim_index.0 + i);
                        if self.prim_store.build_bounding_rect(prim_index,
                                                               screen_rect,
                                                               &packed_layer.transform,
                                                               &packed_layer.local_clip_rect,
                                                               device_pixel_ratio) {
                            profile_counters.visible_primitives.inc();

                            if self.prim_store.prepare_prim_for_render(prim_index,
                                                                       resource_cache,
                                                                       &packed_layer.transform,
                                                                       device_pixel_ratio,
                                                                       auxiliary_lists) {
                                self.prim_store.build_bounding_rect(prim_index,
                                                                    screen_rect,
                                                                    &packed_layer.transform,
                                                                    &packed_layer.local_clip_rect,
                                                                    device_pixel_ratio);
                            }
                        }
                    }
                }
                &PrimitiveRunCmd::PopStackingContext => {
                    layer_stack.pop().unwrap();
                }
            }
        }
    }

    fn create_screen_tiles(&self, device_pixel_ratio: f32) -> (i32, i32, Vec<ScreenTile>) {
        let dp_size = DeviceIntSize::from_lengths(device_length(self.screen_rect.size.width as f32,
                                                                device_pixel_ratio),
                                                  device_length(self.screen_rect.size.height as f32,
                                                                device_pixel_ratio));

        let x_tile_size = SCREEN_TILE_SIZE;
        let y_tile_size = SCREEN_TILE_SIZE;
        let x_tile_count = (dp_size.width + x_tile_size - 1) / x_tile_size;
        let y_tile_count = (dp_size.height + y_tile_size - 1) / y_tile_size;

        // Build screen space tiles, which are individual BSP trees.
        let mut screen_tiles = Vec::new();
        for y in 0..y_tile_count {
            let y0 = y * y_tile_size;
            let y1 = y0 + y_tile_size;

            for x in 0..x_tile_count {
                let x0 = x * x_tile_size;
                let x1 = x0 + x_tile_size;

                let tile_rect = rect_from_points(DeviceIntLength::new(x0),
                                                 DeviceIntLength::new(y0),
                                                 DeviceIntLength::new(x1),
                                                 DeviceIntLength::new(y1));

                screen_tiles.push(ScreenTile::new(tile_rect));
            }
        }

        (x_tile_count, y_tile_count, screen_tiles)
    }


    fn assign_prims_to_screen_tiles(&self,
                                    screen_tiles: &mut Vec<ScreenTile>,
                                    x_tile_count: i32,
                                    device_pixel_ratio: f32) {
        let mut layer_stack: Vec<StackingContextIndex> = Vec::new();
        let mut clip_rect_stack = Vec::new();

        for cmd in &self.cmds {
            match cmd {
                &PrimitiveRunCmd::PushStackingContext(sc_index) => {
                    layer_stack.push(sc_index);

                    let layer = &self.layer_store[sc_index.0];
                    if !layer.is_visible() {
                        continue;
                    }

                    if let Some(ref clip_info) = layer.clip_cache_info {
                        clip_rect_stack.push(clip_info.outer_rect);
                    }

                    let tile_range = layer.tile_range.as_ref().unwrap();
                    for ly in tile_range.y0..tile_range.y1 {
                        for lx in tile_range.x0..tile_range.x1 {
                            let tile = &mut screen_tiles[(ly * x_tile_count + lx) as usize];
                            tile.push_layer(sc_index);
                        }
                    }
                }
                &PrimitiveRunCmd::PrimitiveRun(first_prim_index, prim_count) => {
                    let sc_index = layer_stack.last().unwrap();

                    let layer = &self.layer_store[sc_index.0];
                    if !layer.is_visible() {
                        continue;
                    }
                    let packed_layer = &self.packed_layers[sc_index.0];

                    let tile_range = layer.tile_range.as_ref().unwrap();
                    let xf_rect = &layer.xf_rect.as_ref().unwrap();

                    for i in 0..prim_count {
                        let prim_index = PrimitiveIndex(first_prim_index.0 + i);

                        // check the bounding box
                        let mut p_rect = match self.prim_store.get_bounding_rect(prim_index) {
                            &Some(r) => r,
                            &None => continue,
                        };
                        // check the clip bounding rectangle
                        if let Some(ref clip_info) = self.prim_store.get_metadata(prim_index).clip_cache_info {
                            p_rect = match p_rect.intersection(&clip_info.outer_rect) {
                                Some(r) => r,
                                None => continue,
                            }
                        } else
                        // check the parent layer clip rectangle
                        if let Some(clip_rect) = clip_rect_stack.last() {
                            p_rect = match p_rect.intersection(clip_rect) {
                                Some(r) => r,
                                None => continue,
                            }
                        }

                        // TODO(gw): Ensure that certain primitives (such as background-image) only get
                        //           assigned to tiles where their containing layer intersects with.
                        //           Does this cause any problems / demonstrate other bugs?
                        //           Restrict the tiles by clamping to the layer tile indices...

                        let p_tile_x0 = p_rect.origin.x / SCREEN_TILE_SIZE;
                        let p_tile_y0 = p_rect.origin.y / SCREEN_TILE_SIZE;
                        let p_tile_x1 = (p_rect.origin.x + p_rect.size.width + SCREEN_TILE_SIZE - 1) / SCREEN_TILE_SIZE;
                        let p_tile_y1 = (p_rect.origin.y + p_rect.size.height + SCREEN_TILE_SIZE - 1) / SCREEN_TILE_SIZE;

                        for py in cmp::max(p_tile_y0, tile_range.y0) .. cmp::min(p_tile_y1, tile_range.y1) {
                            for px in cmp::max(p_tile_x0, tile_range.x0) .. cmp::min(p_tile_x1, tile_range.x1) {
                                let tile = &mut screen_tiles[(py * x_tile_count + px) as usize];

                                // TODO(gw): Support narrow phase for 3d transform elements!
                                if xf_rect.kind == TransformedRectKind::Complex ||
                                        self.prim_store.prim_affects_tile(prim_index,
                                                                          &tile.rect,
                                                                          &packed_layer.transform,
                                                                          device_pixel_ratio) {
                                    tile.push_primitive(prim_index);
                                }
                            }
                        }
                    }
                }
                &PrimitiveRunCmd::PopStackingContext => {
                    let sc_index = layer_stack.pop().unwrap();

                    let layer = &self.layer_store[sc_index.0];
                    if !layer.is_visible() {
                        continue;
                    }

                    if layer.clip_cache_info.is_some() {
                        clip_rect_stack.pop();
                    }

                    let tile_range = layer.tile_range.as_ref().unwrap();
                    for ly in tile_range.y0..tile_range.y1 {
                        for lx in tile_range.x0..tile_range.x1 {
                            let tile = &mut screen_tiles[(ly * x_tile_count + lx) as usize];
                            tile.pop_layer(sc_index);
                        }
                    }
                }
            }
        }
    }

    fn update_scroll_bars(&mut self,
                          layer_map: &LayerMap) {
        let distance_from_edge = 8.0;

        for scrollbar_prim in &self.scrollbar_prims {
            let mut geom = (*self.prim_store.gpu_geometry.get(GpuStoreAddress(scrollbar_prim.prim_index.0 as i32))).clone();
            let scroll_layer = &layer_map[&scrollbar_prim.scroll_layer_id];

            let scrollable_distance = scroll_layer.content_size.height - scroll_layer.local_viewport_rect.size.height;

            if scrollable_distance <= 0.0 {
                geom.local_clip_rect.size = LayerSize::zero();
                *self.prim_store.gpu_geometry.get_mut(GpuStoreAddress(scrollbar_prim.prim_index.0 as i32)) = geom;
                continue;
            }

            let f = -scroll_layer.scrolling.offset.y / scrollable_distance;

            let min_y = scroll_layer.local_viewport_rect.origin.y -
                        scroll_layer.scrolling.offset.y +
                        distance_from_edge;

            let max_y = scroll_layer.local_viewport_rect.origin.y +
                        scroll_layer.local_viewport_rect.size.height -
                        scroll_layer.scrolling.offset.y -
                        geom.local_rect.size.height -
                        distance_from_edge;

            geom.local_rect.origin.x = scroll_layer.local_viewport_rect.origin.x +
                                       scroll_layer.local_viewport_rect.size.width -
                                       geom.local_rect.size.width -
                                       distance_from_edge;

            geom.local_rect.origin.y = util::lerp(min_y, max_y, f);
            geom.local_clip_rect = geom.local_rect;

            let clip_source = if scrollbar_prim.border_radius == 0.0 {
                ClipSource::NoClip
            } else {
                ClipSource::Complex(geom.local_rect, scrollbar_prim.border_radius)
            };
            self.prim_store.set_clip_source(scrollbar_prim.prim_index, clip_source);
            *self.prim_store.gpu_geometry.get_mut(GpuStoreAddress(scrollbar_prim.prim_index.0 as i32)) = geom;
        }
    }

    pub fn build(&mut self,
                 resource_cache: &mut ResourceCache,
                 frame_id: FrameId,
                 layer_map: &LayerMap,
                 auxiliary_lists_map: &AuxiliaryListsMap,
                 device_pixel_ratio: f32) -> Frame {

        let mut profile_counters = FrameProfileCounters::new();
        profile_counters.total_primitives.set(self.prim_store.prim_count());

        resource_cache.begin_frame(frame_id);

        let screen_rect = DeviceIntRect::new(
            DeviceIntPoint::zero(),
            DeviceIntSize::from_lengths(device_length(self.screen_rect.size.width as f32,
                                                      device_pixel_ratio),
                                        device_length(self.screen_rect.size.height as f32,
                                                      device_pixel_ratio)));

        let mut debug_rects = Vec::new();

        let (x_tile_count, y_tile_count, mut screen_tiles) = self.create_screen_tiles(device_pixel_ratio);

        self.update_scroll_bars(layer_map);

        self.cull_layers(&screen_rect,
                         layer_map,
                         auxiliary_lists_map,
                         x_tile_count,
                         y_tile_count,
                         resource_cache,
                         &mut profile_counters,
                         device_pixel_ratio);

        let mut compiled_screen_tiles = Vec::new();
        let mut max_passes_needed = 0;

        let mut render_tasks = {
            let mut ctx = CompileTileContext {
                layer_store: &self.layer_store,
                prim_store: &self.prim_store,
                tile_id: 0,

                // This doesn't need to be atomic right now (all the screen tiles are
                // compiled on a single thread). However, in the future each of the
                // compile steps below will be run on a worker thread, which will
                // require an atomic int here anyway.
                render_task_id_counter: AtomicUsize::new(0),
            };

            if !self.layer_store.is_empty() {
                self.assign_prims_to_screen_tiles(&mut screen_tiles,
                                                  x_tile_count,
                                                  device_pixel_ratio);
            }

            // Build list of passes, target allocs that each tile needs.
            for (tile_id, screen_tile) in screen_tiles.into_iter().enumerate() {
                ctx.tile_id = tile_id;
                let rect = screen_tile.rect;
                if let Some(compiled_screen_tile) = screen_tile.compile(&ctx) {
                    max_passes_needed = cmp::max(max_passes_needed,
                                                 compiled_screen_tile.required_pass_count);
                    if self.debug {
                        let label = format!("{}|{}", compiled_screen_tile.info.cmd_count, compiled_screen_tile.info.prim_count);
                        let color =  ColorF::new(1.0, 0.0, 0.0, 1.0);
                        debug_rects.push(DebugRect {
                            label: label,
                            color: color,
                            rect: rect,
                        });
                    }
                    compiled_screen_tiles.push(compiled_screen_tile);
                }
            }

            let static_render_task_count = ctx.render_task_id_counter.load(Ordering::SeqCst);
            RenderTaskCollection::new(static_render_task_count)
        };

        resource_cache.block_until_all_resources_added();

        for layer in self.layer_store.iter() {
            if let Some(ref clip_info) = layer.clip_cache_info {
                self.prim_store.resolve_clip_cache(clip_info, resource_cache);
            }
        }


        let deferred_resolves = self.prim_store.resolve_primitives(resource_cache, device_pixel_ratio);

        let mut passes = Vec::new();

        if !compiled_screen_tiles.is_empty() {
            let mut ctx = RenderTargetContext {
                layer_store: &self.layer_store,
                prim_store: &self.prim_store,
                resource_cache: resource_cache,
                layer_masks_tasks: HashMap::new(),
            };

            // Do the allocations now, assigning each tile's tasks to a render
            // pass and target as required.
            for index in 0..max_passes_needed {
                passes.push(RenderPass::new(index as isize,
                                            index == max_passes_needed-1));
            }

            for mut compiled_screen_tile in compiled_screen_tiles {
                // Grab the mask task indices from the compile tile and append into the context map
                for (i, mask_task_opt) in compiled_screen_tile.layer_masks_tasks.task_ids.drain(..).enumerate() {
                    if let Some(mask_task_id) = mask_task_opt {
                        let key = (compiled_screen_tile.unique_id, StackingContextIndex(i));
                        ctx.layer_masks_tasks.insert(key, mask_task_id);
                    }
                }
                compiled_screen_tile.build(&mut passes);
            }

            for pass in &mut passes {
                pass.build(&ctx, &mut render_tasks);

                profile_counters.passes.inc();
                profile_counters.targets.add(pass.targets.len());
            }
        }

        resource_cache.end_frame();

        Frame {
            device_pixel_ratio: device_pixel_ratio,
            background_color: self.background_color,
            viewport_size: self.screen_rect.size,
            debug_rects: debug_rects,
            profile_counters: profile_counters,
            passes: passes,
            cache_size: DeviceSize::new(RENDERABLE_CACHE_SIZE as f32,
                                        RENDERABLE_CACHE_SIZE as f32),
            layer_texture_data: self.packed_layers.clone(),
            render_task_data: render_tasks.render_task_data,
            gpu_data16: self.prim_store.gpu_data16.build(),
            gpu_data32: self.prim_store.gpu_data32.build(),
            gpu_data64: self.prim_store.gpu_data64.build(),
            gpu_data128: self.prim_store.gpu_data128.build(),
            gpu_geometry: self.prim_store.gpu_geometry.build(),
            gpu_resource_rects: self.prim_store.gpu_resource_rects.build(),
            deferred_resolves: deferred_resolves,
        }
    }
}
