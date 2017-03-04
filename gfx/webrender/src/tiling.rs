/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use app_units::Au;
use fnv::FnvHasher;
use gpu_store::GpuStoreAddress;
use internal_types::{ANGLE_FLOAT_TO_FIXED, BatchTextures, CacheTextureId, LowLevelFilterOp};
use internal_types::SourceTexture;
use mask_cache::{ClipSource, MaskCacheInfo};
use prim_store::{CLIP_DATA_GPU_SIZE, DeferredResolve, GpuBlock128, GpuBlock16, GpuBlock32};
use prim_store::{GpuBlock64, GradientData, PrimitiveCacheKey, PrimitiveGeometry, PrimitiveIndex};
use prim_store::{PrimitiveKind, PrimitiveMetadata, PrimitiveStore, TexelRect};
use profiler::FrameProfileCounters;
use render_task::{AlphaRenderItem, MaskGeometryKind, MaskSegment, RenderTask, RenderTaskData};
use render_task::{RenderTaskId, RenderTaskIndex, RenderTaskKey, RenderTaskKind};
use render_task::RenderTaskLocation;
use renderer::BlendMode;
use resource_cache::ResourceCache;
use std::{f32, i32, mem, usize};
use std::collections::HashMap;
use std::hash::BuildHasherDefault;
use texture_cache::TexturePage;
use util::{TransformedRect, TransformedRectKind};
use webrender_traits::{AuxiliaryLists, ColorF, DeviceIntPoint, DeviceIntRect, DeviceUintPoint};
use webrender_traits::{DeviceUintSize, FontRenderMode, ImageRendering, LayerRect, LayerSize};
use webrender_traits::{LayerToScrollTransform, LayerToWorldTransform, MixBlendMode, PipelineId};
use webrender_traits::{ScrollLayerId, WorldPoint4D, WorldToLayerTransform};

// Special sentinel value recognized by the shader. It is considered to be
// a dummy task that doesn't mask out anything.
const OPAQUE_TASK_INDEX: RenderTaskIndex = RenderTaskIndex(i32::MAX as usize);


pub type AuxiliaryListsMap = HashMap<PipelineId,
                                     AuxiliaryLists,
                                     BuildHasherDefault<FnvHasher>>;

trait AlphaBatchHelpers {
    fn get_batch_kind(&self, metadata: &PrimitiveMetadata) -> AlphaBatchKind;
    fn get_color_textures(&self, metadata: &PrimitiveMetadata) -> [SourceTexture; 3];
    fn get_blend_mode(&self, needs_blending: bool, metadata: &PrimitiveMetadata) -> BlendMode;
    fn add_prim_to_batch(&self,
                         prim_index: PrimitiveIndex,
                         batch: &mut PrimitiveBatch,
                         packed_layer_index: PackedLayerIndex,
                         task_index: RenderTaskIndex,
                         render_tasks: &RenderTaskCollection,
                         pass_index: RenderPassIndex,
                         z_sort_index: i32);
    fn add_blend_to_batch(&self,
                          stacking_context_index: StackingContextIndex,
                          batch: &mut PrimitiveBatch,
                          task_index: RenderTaskIndex,
                          src_task_index: RenderTaskIndex,
                          filter: LowLevelFilterOp,
                          z_sort_index: i32);
    fn add_hardware_composite_to_batch(&self,
                                       stacking_context_index: StackingContextIndex,
                                       batch: &mut PrimitiveBatch,
                                       task_index: RenderTaskIndex,
                                       src_task_index: RenderTaskIndex,
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
            PrimitiveKind::AlignedGradient => AlphaBatchKind::AlignedGradient,
            PrimitiveKind::AngleGradient => AlphaBatchKind::AngleGradient,
            PrimitiveKind::RadialGradient => AlphaBatchKind::RadialGradient,
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
        };

        batch_kind
    }

    fn get_color_textures(&self, metadata: &PrimitiveMetadata) -> [SourceTexture; 3] {
        let invalid = SourceTexture::Invalid;
        match metadata.prim_kind {
            PrimitiveKind::Border |
            PrimitiveKind::BoxShadow |
            PrimitiveKind::Rectangle |
            PrimitiveKind::AlignedGradient |
            PrimitiveKind::AngleGradient |
            PrimitiveKind::RadialGradient => [invalid; 3],
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

    fn add_blend_to_batch(&self,
                          stacking_context_index: StackingContextIndex,
                          batch: &mut PrimitiveBatch,
                          task_index: RenderTaskIndex,
                          src_task_index: RenderTaskIndex,
                          filter: LowLevelFilterOp,
                          z_sort_index: i32) {
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

        batch.items.push(PrimitiveBatchItem::StackingContext(stacking_context_index));

        match batch.data {
            PrimitiveBatchData::Instances(ref mut data) => {
                data.push(PrimitiveInstance {
                    global_prim_id: -1,
                    prim_address: GpuStoreAddress(0),
                    task_index: task_index.0 as i32,
                    clip_task_index: -1,
                    layer_index: -1,
                    sub_index: filter_mode,
                    user_data: [src_task_index.0 as i32, amount],
                    z_sort_index: z_sort_index,
                });
            }
            _ => unreachable!(),
        }
    }

    fn add_hardware_composite_to_batch(&self,
                                       stacking_context_index: StackingContextIndex,
                                       batch: &mut PrimitiveBatch,
                                       task_index: RenderTaskIndex,
                                       src_task_index: RenderTaskIndex,
                                       z_sort_index: i32) {
        batch.items.push(PrimitiveBatchItem::StackingContext(stacking_context_index));

        match batch.data {
            PrimitiveBatchData::Instances(ref mut data) => {
                data.push(PrimitiveInstance {
                    global_prim_id: -1,
                    prim_address: GpuStoreAddress(0),
                    task_index: task_index.0 as i32,
                    clip_task_index: -1,
                    layer_index: -1,
                    sub_index: -1,
                    user_data: [src_task_index.0 as i32, 0],
                    z_sort_index: z_sort_index,
                });
            }
            _ => unreachable!(),
        }
    }

    fn add_prim_to_batch(&self,
                         prim_index: PrimitiveIndex,
                         batch: &mut PrimitiveBatch,
                         packed_layer_index: PackedLayerIndex,
                         task_index: RenderTaskIndex,
                         render_tasks: &RenderTaskCollection,
                         child_pass_index: RenderPassIndex,
                         z_sort_index: i32) {
        let metadata = self.get_metadata(prim_index);
        let packed_layer_index = packed_layer_index.0 as i32;
        let global_prim_id = prim_index.0 as i32;
        let prim_address = metadata.gpu_prim_index;
        let clip_task_index = match metadata.clip_task {
            Some(ref clip_task) => {
                render_tasks.get_task_index(&clip_task.id, child_pass_index)
            }
            None => {
                OPAQUE_TASK_INDEX
            }
        };
        let task_index = task_index.0 as i32;
        let clip_task_index = clip_task_index.0 as i32;
        batch.items.push(PrimitiveBatchItem::Primitive(prim_index));

        match &mut batch.data {
            &mut PrimitiveBatchData::Composite(..) => unreachable!(),
            &mut PrimitiveBatchData::Instances(ref mut data) => {
                match batch.key.kind {
                    AlphaBatchKind::Composite => unreachable!(),
                    AlphaBatchKind::HardwareComposite => unreachable!(),
                    AlphaBatchKind::Blend => unreachable!(),
                    AlphaBatchKind::Rectangle => {
                        data.push(PrimitiveInstance {
                            task_index: task_index,
                            clip_task_index: clip_task_index,
                            layer_index: packed_layer_index,
                            global_prim_id: global_prim_id,
                            prim_address: prim_address,
                            sub_index: 0,
                            user_data: [0, 0],
                            z_sort_index: z_sort_index,
                        });
                    }
                    AlphaBatchKind::TextRun => {
                        let text_cpu = &self.cpu_text_runs[metadata.cpu_prim_index.0];

                        for glyph_index in 0..metadata.gpu_data_count {
                            data.push(PrimitiveInstance {
                                task_index: task_index,
                                clip_task_index: clip_task_index,
                                layer_index: packed_layer_index,
                                global_prim_id: global_prim_id,
                                prim_address: prim_address,
                                sub_index: metadata.gpu_data_address.0 + glyph_index,
                                user_data: [ text_cpu.resource_address.0 + glyph_index, 0 ],
                                z_sort_index: z_sort_index,
                            });
                        }
                    }
                    AlphaBatchKind::Image => {
                        let image_cpu = &self.cpu_images[metadata.cpu_prim_index.0];

                        data.push(PrimitiveInstance {
                            task_index: task_index,
                            clip_task_index: clip_task_index,
                            layer_index: packed_layer_index,
                            global_prim_id: global_prim_id,
                            prim_address: prim_address,
                            sub_index: 0,
                            user_data: [ image_cpu.resource_address.0, 0 ],
                            z_sort_index: z_sort_index,
                        });
                    }
                    AlphaBatchKind::YuvImage => {
                        data.push(PrimitiveInstance {
                            task_index: task_index,
                            clip_task_index: clip_task_index,
                            layer_index: packed_layer_index,
                            global_prim_id: global_prim_id,
                            prim_address: prim_address,
                            sub_index: 0,
                            user_data: [ 0, 0 ],
                            z_sort_index: z_sort_index,
                        });
                    }
                    AlphaBatchKind::Border => {
                        for border_segment in 0..8 {
                            data.push(PrimitiveInstance {
                                task_index: task_index,
                                clip_task_index: clip_task_index,
                                layer_index: packed_layer_index,
                                global_prim_id: global_prim_id,
                                prim_address: prim_address,
                                sub_index: border_segment,
                                user_data: [ 0, 0 ],
                                z_sort_index: z_sort_index,
                            });
                        }
                    }
                    AlphaBatchKind::AlignedGradient => {
                        for part_index in 0..(metadata.gpu_data_count - 1) {
                            data.push(PrimitiveInstance {
                                task_index: task_index,
                                clip_task_index: clip_task_index,
                                layer_index: packed_layer_index,
                                global_prim_id: global_prim_id,
                                prim_address: prim_address,
                                sub_index: metadata.gpu_data_address.0 + part_index,
                                user_data: [ 0, 0 ],
                                z_sort_index: z_sort_index,
                            });
                        }
                    }
                    AlphaBatchKind::AngleGradient => {
                        data.push(PrimitiveInstance {
                            task_index: task_index,
                            clip_task_index: clip_task_index,
                            layer_index: packed_layer_index,
                            global_prim_id: global_prim_id,
                            prim_address: prim_address,
                            sub_index: metadata.gpu_data_address.0,
                            user_data: [ metadata.gpu_data_count, 0 ],
                            z_sort_index: z_sort_index,
                        });
                    }
                    AlphaBatchKind::RadialGradient => {
                        data.push(PrimitiveInstance {
                            task_index: task_index,
                            clip_task_index: clip_task_index,
                            layer_index: packed_layer_index,
                            global_prim_id: global_prim_id,
                            prim_address: prim_address,
                            sub_index: metadata.gpu_data_address.0,
                            user_data: [ metadata.gpu_data_count, 0 ],
                            z_sort_index: z_sort_index,
                        });
                    }
                    AlphaBatchKind::BoxShadow => {
                        let cache_task_id = &metadata.render_task.as_ref().unwrap().id;
                        let cache_task_index = render_tasks.get_task_index(cache_task_id,
                                                                           child_pass_index);

                        for rect_index in 0..metadata.gpu_data_count {
                            data.push(PrimitiveInstance {
                                task_index: task_index,
                                clip_task_index: clip_task_index,
                                layer_index: packed_layer_index,
                                global_prim_id: global_prim_id,
                                prim_address: prim_address,
                                sub_index: metadata.gpu_data_address.0 + rect_index,
                                user_data: [ cache_task_index.0 as i32, 0 ],
                                z_sort_index: z_sort_index,
                            });
                        }
                    }
                    AlphaBatchKind::CacheImage => {
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
                            layer_index: packed_layer_index,
                            global_prim_id: global_prim_id,
                            prim_address: prim_address,
                            sub_index: 0,
                            user_data: [ cache_task_index.0 as i32, 0 ],
                            z_sort_index: z_sort_index,
                        });
                    }
                }
            }
        }
    }
}

#[derive(Debug)]
pub struct ScrollbarPrimitive {
    pub scroll_layer_id: ScrollLayerId,
    pub prim_index: PrimitiveIndex,
    pub border_radius: f32,
}

pub enum PrimitiveRunCmd {
    PushStackingContext(StackingContextIndex),
    PopStackingContext,

    PushScrollLayer(ScrollLayerIndex),
    PopScrollLayer,

    PrimitiveRun(PrimitiveIndex, usize),
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
pub struct RenderTargetIndex(pub usize);

#[derive(Debug, Copy, Clone, Eq, PartialEq, Hash)]
struct RenderPassIndex(isize);

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
            let mut existing_opaque_batch_index = 0;

            for item in &task.alpha_items {
                let (batch_key, item_bounding_rect) = match item {
                    &AlphaRenderItem::Blend(stacking_context_index, ..) => {
                        let stacking_context =
                            &ctx.stacking_context_store[stacking_context_index.0];
                        (AlphaBatchKey::new(AlphaBatchKind::Blend,
                                            AlphaBatchKeyFlags::empty(),
                                            BlendMode::Alpha,
                                            BatchTextures::no_texture()),
                         &stacking_context.bounding_rect)
                    }
                    &AlphaRenderItem::HardwareComposite(stacking_context_index, _, composite_op, ..) => {
                        let stacking_context = &ctx.stacking_context_store[stacking_context_index.0];
                        (AlphaBatchKey::new(AlphaBatchKind::HardwareComposite,
                                            AlphaBatchKeyFlags::empty(),
                                            composite_op.to_blend_mode(),
                                            BatchTextures::no_texture()),
                         &stacking_context.bounding_rect)
                    }
                    &AlphaRenderItem::Composite(stacking_context_index,
                                                backdrop_id,
                                                src_id,
                                                info,
                                                z) => {
                        // Composites always get added to their own batch.
                        // This is because the result of a composite can affect
                        // the input to the next composite. Perhaps we can
                        // optimize this in the future.
                        let batch = PrimitiveBatch::new_composite(stacking_context_index,
                                                                  task_index,
                                                                  render_tasks.get_task_index(&backdrop_id, child_pass_index),
                                                                  render_tasks.get_static_task_index(&src_id),
                                                                  info,
                                                                  z);
                        alpha_batches.push(batch);
                        continue;
                    }
                    &AlphaRenderItem::Primitive(clip_scroll_group_index, prim_index, _) => {
                        let group = &ctx.clip_scroll_group_store[clip_scroll_group_index.0];
                        let prim_metadata = ctx.prim_store.get_metadata(prim_index);
                        let transform_kind = group.xf_rect.as_ref().unwrap().kind;
                        let needs_clipping = prim_metadata.clip_task.is_some();
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

                        (AlphaBatchKey::new(batch_kind,
                                            flags,
                                            blend_mode,
                                            textures),
                         ctx.prim_store.cpu_bounding_rects[prim_index.0].as_ref().unwrap())
                    }
                };

                let mut alpha_batch_index = None;
                'outer: for (batch_index, batch) in alpha_batches.iter()
                                                         .enumerate()
                                                         .rev()
                                                         .take(10) {
                    if batch.key.is_compatible_with(&batch_key) {
                        alpha_batch_index = Some(batch_index);
                        break;
                    }

                    // check for intersections
                    for item in &batch.items {
                        let intersects = match *item {
                            PrimitiveBatchItem::StackingContext(stacking_context_index) => {
                                let stacking_context =
                                    &ctx.stacking_context_store[stacking_context_index.0];
                                stacking_context.bounding_rect.intersects(item_bounding_rect)
                            }
                            PrimitiveBatchItem::Primitive(prim_index) => {
                                let bounding_rect = &ctx.prim_store.cpu_bounding_rects[prim_index.0];
                                bounding_rect.as_ref().unwrap().intersects(item_bounding_rect)
                            }
                        };

                        if intersects {
                            break 'outer;
                        }
                    }
                }

                if alpha_batch_index.is_none() {
                    let new_batch = match item {
                        &AlphaRenderItem::Composite(..) => unreachable!(),
                        &AlphaRenderItem::HardwareComposite(..) => {
                            PrimitiveBatch::new_instances(AlphaBatchKind::HardwareComposite, batch_key)
                        }
                        &AlphaRenderItem::Blend(..) => {
                            PrimitiveBatch::new_instances(AlphaBatchKind::Blend, batch_key)
                        }
                        &AlphaRenderItem::Primitive(_, prim_index, _) => {
                            let prim_metadata = ctx.prim_store.get_metadata(prim_index);
                            let batch_kind = ctx.prim_store.get_batch_kind(prim_metadata);
                            PrimitiveBatch::new_instances(batch_kind, batch_key)
                        }
                    };
                    alpha_batch_index = Some(alpha_batches.len());
                    alpha_batches.push(new_batch);
                }

                let batch = &mut alpha_batches[alpha_batch_index.unwrap()];
                match item {
                    &AlphaRenderItem::Composite(..) => unreachable!(),
                    &AlphaRenderItem::Blend(stacking_context_index, src_id, info, z) => {
                        ctx.prim_store.add_blend_to_batch(stacking_context_index,
                                                          batch,
                                                          task_index,
                                                          render_tasks.get_static_task_index(&src_id),
                                                          info,
                                                          z);
                    }
                    &AlphaRenderItem::HardwareComposite(stacking_context_index, src_id, _, z) => {
                        ctx.prim_store.add_hardware_composite_to_batch(
                            stacking_context_index,
                            batch,
                            task_index,
                            render_tasks.get_static_task_index(&src_id),
                            z);
                    }
                    &AlphaRenderItem::Primitive(clip_scroll_group_index, prim_index, z) => {
                        let packed_layer = ctx.clip_scroll_group_store[clip_scroll_group_index.0]
                                              .packed_layer_index;
                        ctx.prim_store.add_prim_to_batch(prim_index,
                                                         batch,
                                                         packed_layer,
                                                         task_index,
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
                    &AlphaRenderItem::HardwareComposite(..) => unreachable!(),
                    &AlphaRenderItem::Primitive(clip_scroll_group_index, prim_index, _) => {
                        let group = &ctx.clip_scroll_group_store[clip_scroll_group_index.0];
                        let transform_kind = group.xf_rect.as_ref().unwrap().kind;
                        let prim_metadata = ctx.prim_store.get_metadata(prim_index);
                        let needs_clipping = prim_metadata.clip_task.is_some();
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

                        AlphaBatchKey::new(batch_kind,
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
                        &AlphaRenderItem::HardwareComposite(..) => unreachable!(),
                        &AlphaRenderItem::Primitive(_, prim_index, _) => {
                            let prim_metadata = ctx.prim_store.get_metadata(prim_index);
                            let batch_kind = ctx.prim_store.get_batch_kind(prim_metadata);
                            PrimitiveBatch::new_instances(batch_kind, batch_key)
                        }
                    };
                    opaque_batches.push(new_batch)
                }

                let batch = &mut opaque_batches[existing_opaque_batch_index];
                match item {
                    &AlphaRenderItem::Composite(..) => unreachable!(),
                    &AlphaRenderItem::Blend(..) => unreachable!(),
                    &AlphaRenderItem::HardwareComposite(..) => unreachable!(),
                    &AlphaRenderItem::Primitive(clip_scroll_group_index, prim_index, z) => {
                        let packed_layer_index =
                            ctx.clip_scroll_group_store[clip_scroll_group_index.0]
                               .packed_layer_index;
                        ctx.prim_store.add_prim_to_batch(prim_index,
                                                         batch,
                                                         packed_layer_index,
                                                         task_index,
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
    /// Rectangle draws fill up the rectangles with rounded corners.
    pub rectangles: Vec<CacheClipInstance>,
    /// Image draws apply the image masking.
    pub images: HashMap<SourceTexture, Vec<CacheClipInstance>>,
}

impl ClipBatcher {
    fn new() -> ClipBatcher {
        ClipBatcher {
            rectangles: Vec::new(),
            images: HashMap::new(),
        }
    }

    fn add<'a>(&mut self,
               task_index: RenderTaskIndex,
               clips: &[(PackedLayerIndex, MaskCacheInfo)],
               resource_cache: &ResourceCache,
               geometry_kind: MaskGeometryKind) {

        for &(packed_layer_index, ref info) in clips.iter() {
            let instance = CacheClipInstance {
                task_id: task_index.0 as i32,
                layer_index: packed_layer_index.0 as i32,
                address: GpuStoreAddress(0),
                segment: 0,
            };

            for clip_index in 0..info.effective_clip_count as usize {
                let offset = info.clip_range.start.0 + ((CLIP_DATA_GPU_SIZE * clip_index) as i32);
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
                    ..instance
                })
            }
        }
    }
}

pub struct RenderTargetContext<'a> {
    pub stacking_context_store: &'a [StackingContext],
    pub clip_scroll_group_store: &'a [ClipScrollGroup],
    pub prim_store: &'a PrimitiveStore,
    pub resource_cache: &'a ResourceCache,
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
    pub readbacks: Vec<DeviceIntRect>,
    page_allocator: TexturePage,
}

impl RenderTarget {
    fn new(size: DeviceUintSize) -> RenderTarget {
        RenderTarget {
            alpha_batcher: AlphaBatcher::new(),
            clip_batcher: ClipBatcher::new(),
            box_shadow_cache_prims: Vec::new(),
            text_run_cache_prims: Vec::new(),
            text_run_textures: BatchTextures::no_texture(),
            vertical_blurs: Vec::new(),
            horizontal_blurs: Vec::new(),
            readbacks: Vec::new(),
            page_allocator: TexturePage::new(CacheTextureId(0), size),
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
                self.clip_batcher.add(task_index,
                                      &task_info.clips,
                                      &ctx.resource_cache,
                                      task_info.geometry_kind);
            }
            RenderTaskKind::Readback(device_rect) => {
                self.readbacks.push(device_rect);
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
    size: DeviceUintSize,
}

impl RenderPass {
    pub fn new(pass_index: isize, is_framebuffer: bool, size: DeviceUintSize) -> RenderPass {
        RenderPass {
            pass_index: RenderPassIndex(pass_index),
            is_framebuffer: is_framebuffer,
            targets: vec![ RenderTarget::new(size) ],
            tasks: vec![],
            size: size,
        }
    }

    pub fn add_render_task(&mut self, task: RenderTask) {
        self.tasks.push(task);
    }

    fn allocate_target(&mut self, alloc_size: DeviceUintSize) -> DeviceUintPoint {
        let existing_origin = self.targets
                                  .last_mut()
                                  .unwrap()
                                  .page_allocator
                                  .allocate(&alloc_size);
        match existing_origin {
            Some(origin) => origin,
            None => {
                let mut new_target = RenderTarget::new(self.size);
                let origin = new_target.page_allocator
                                       .allocate(&alloc_size)
                                       .expect(&format!("Each render task must allocate <= size of one target! ({:?})", alloc_size));
                self.targets.push(new_target);
                origin
            }
        }
    }


    pub fn build(&mut self, ctx: &RenderTargetContext, render_tasks: &mut RenderTaskCollection) {
        profile_scope!("RenderPass::build");

        // Step through each task, adding to batches as appropriate.
        let tasks = mem::replace(&mut self.tasks, Vec::new());
        for mut task in tasks {
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
                    let alloc_origin = self.allocate_target(alloc_size);

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

#[derive(Copy, Clone, PartialEq, Eq, Hash, Debug)]
#[repr(u8)]
pub enum AlphaBatchKind {
    Composite = 0,
    HardwareComposite,
    Blend,
    Rectangle,
    TextRun,
    Image,
    YuvImage,
    Border,
    AlignedGradient,
    AngleGradient,
    RadialGradient,
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
}

#[derive(Debug, Clone)]
pub struct PrimitiveInstance {
    global_prim_id: i32,
    prim_address: GpuStoreAddress,
    pub task_index: i32,
    clip_task_index: i32,
    layer_index: i32,
    sub_index: i32,
    z_sort_index: i32,
    pub user_data: [i32; 2],
}

#[derive(Debug)]
pub enum PrimitiveBatchData {
    Instances(Vec<PrimitiveInstance>),
    Composite(PrimitiveInstance),
}

#[derive(Debug)]
pub enum PrimitiveBatchItem {
    Primitive(PrimitiveIndex),
    StackingContext(StackingContextIndex),
}

#[derive(Debug)]
pub struct PrimitiveBatch {
    pub key: AlphaBatchKey,
    pub data: PrimitiveBatchData,
    pub items: Vec<PrimitiveBatchItem>,
}

impl PrimitiveBatch {
    fn new_instances(batch_kind: AlphaBatchKind, key: AlphaBatchKey) -> PrimitiveBatch {
        let data = match batch_kind {
            AlphaBatchKind::Rectangle |
            AlphaBatchKind::TextRun |
            AlphaBatchKind::Image |
            AlphaBatchKind::YuvImage |
            AlphaBatchKind::Border |
            AlphaBatchKind::AlignedGradient |
            AlphaBatchKind::AngleGradient |
            AlphaBatchKind::RadialGradient |
            AlphaBatchKind::BoxShadow |
            AlphaBatchKind::Blend |
            AlphaBatchKind::HardwareComposite |
            AlphaBatchKind::CacheImage => {
                PrimitiveBatchData::Instances(Vec::new())
            }
            AlphaBatchKind::Composite => unreachable!(),
        };

        PrimitiveBatch {
            key: key,
            data: data,
            items: Vec::new(),
        }
    }

    fn new_composite(stacking_context_index: StackingContextIndex,
                     task_index: RenderTaskIndex,
                     backdrop_task: RenderTaskIndex,
                     src_task_index: RenderTaskIndex,
                     mode: MixBlendMode,
                     z_sort_index: i32) -> PrimitiveBatch {
        let data = PrimitiveBatchData::Composite(PrimitiveInstance {
            global_prim_id: -1,
            prim_address: GpuStoreAddress(0),
            task_index: task_index.0 as i32,
            clip_task_index: -1,
            layer_index: -1,
            sub_index: mode as u32 as i32,
            user_data: [ backdrop_task.0 as i32,
                         src_task_index.0 as i32 ],
            z_sort_index: z_sort_index,
        });
        let key = AlphaBatchKey::new(AlphaBatchKind::Composite,
                                     AlphaBatchKeyFlags::empty(),
                                     BlendMode::Alpha,
                                     BatchTextures::no_texture());

        PrimitiveBatch {
            key: key,
            data: data,
            items: vec![PrimitiveBatchItem::StackingContext(stacking_context_index)],
        }
    }
}

#[derive(Debug, Copy, Clone, Eq, PartialEq, Hash, Ord, PartialOrd)]
pub struct PackedLayerIndex(pub usize);

#[derive(Debug, Copy, Clone, Eq, PartialEq, Hash, Ord, PartialOrd)]
pub struct StackingContextIndex(pub usize);

#[derive(Debug)]
pub struct StackingContext {
    pub pipeline_id: PipelineId,
    pub local_transform: LayerToScrollTransform,
    pub local_rect: LayerRect,
    pub bounding_rect: DeviceIntRect,
    pub composite_ops: CompositeOps,
    pub clip_scroll_groups: Vec<ClipScrollGroupIndex>,
    pub is_visible: bool,
}

impl StackingContext {
    pub fn new(pipeline_id: PipelineId,
               local_transform: LayerToScrollTransform,
               local_rect: LayerRect,
               composite_ops: CompositeOps,
               clip_scroll_group_index: ClipScrollGroupIndex)
               -> StackingContext {
        StackingContext {
            pipeline_id: pipeline_id,
            local_transform: local_transform,
            local_rect: local_rect,
            bounding_rect: DeviceIntRect::zero(),
            composite_ops: composite_ops,
            clip_scroll_groups: vec![clip_scroll_group_index],
            is_visible: false,
        }
    }

    pub fn clip_scroll_group(&self) -> ClipScrollGroupIndex {
        // Currently there is only one scrolled stacking context per context,
        // but eventually this will be selected from the vector based on the
        // scroll layer of this primitive.
        self.clip_scroll_groups[0]
    }

    pub fn can_contribute_to_scene(&self) -> bool {
        !self.composite_ops.will_make_invisible()
    }
}

#[derive(Debug, Copy, Clone, Eq, PartialEq, Hash, Ord, PartialOrd)]
pub struct ClipScrollGroupIndex(pub usize);

#[derive(Debug)]
pub struct ClipScrollGroup {
    pub stacking_context_index: StackingContextIndex,
    pub scroll_layer_id: ScrollLayerId,
    pub packed_layer_index: PackedLayerIndex,
    pub pipeline_id: PipelineId,
    pub xf_rect: Option<TransformedRect>,
}

impl ClipScrollGroup {
    pub fn is_visible(&self) -> bool {
        self.xf_rect.is_some()
    }
}

#[derive(Debug, Copy, Clone, Eq, PartialEq, Hash, Ord, PartialOrd)]
pub struct ScrollLayerIndex(pub usize);

pub struct ScrollLayer {
    pub scroll_layer_id: ScrollLayerId,
    pub parent_index: ScrollLayerIndex,
    pub clip_source: ClipSource,
    pub clip_cache_info: Option<MaskCacheInfo>,
    pub packed_layer_index: PackedLayerIndex,
    pub xf_rect: Option<TransformedRect>,
}

#[derive(Debug, Clone)]
#[repr(C)]
pub struct PackedLayer {
    pub transform: LayerToWorldTransform,
    pub inv_transform: WorldToLayerTransform,
    pub local_clip_rect: LayerRect,
    pub screen_vertices: [WorldPoint4D; 4],
}

impl Default for PackedLayer {
    fn default() -> PackedLayer {
        PackedLayer {
            transform: LayerToWorldTransform::identity(),
            inv_transform: WorldToLayerTransform::identity(),
            local_clip_rect: LayerRect::zero(),
            screen_vertices: [WorldPoint4D::zero(); 4],
        }
    }
}

impl PackedLayer {
    pub fn empty() -> PackedLayer {
        Default::default()
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

    pub fn empty() -> CompositeOps {
        CompositeOps {
            filters: Vec::new(),
            mix_blend_mode: None,
        }
    }

    pub fn count(&self) -> usize {
        self.filters.len() + if self.mix_blend_mode.is_some() { 1 } else { 0 }
    }

    pub fn will_make_invisible(&self) -> bool {
        for op in &self.filters {
            match op {
                &LowLevelFilterOp::Opacity(Au(0)) => return true,
                _ => {}
            }
        }
        false
    }
}

/// A rendering-oriented representation of frame::Frame built by the render backend
/// and presented to the renderer.
pub struct Frame {
    pub viewport_size: LayerSize,
    pub background_color: Option<ColorF>,
    pub device_pixel_ratio: f32,
    pub cache_size: DeviceUintSize,
    pub passes: Vec<RenderPass>,
    pub profile_counters: FrameProfileCounters,

    pub layer_texture_data: Vec<PackedLayer>,
    pub render_task_data: Vec<RenderTaskData>,
    pub gpu_data16: Vec<GpuBlock16>,
    pub gpu_data32: Vec<GpuBlock32>,
    pub gpu_data64: Vec<GpuBlock64>,
    pub gpu_data128: Vec<GpuBlock128>,
    pub gpu_geometry: Vec<PrimitiveGeometry>,
    pub gpu_gradient_data: Vec<GradientData>,
    pub gpu_resource_rects: Vec<TexelRect>,

    // List of textures that we don't know about yet
    // from the backend thread. The render thread
    // will use a callback to resolve these and
    // patch the data structures.
    pub deferred_resolves: Vec<DeferredResolve>,
}

