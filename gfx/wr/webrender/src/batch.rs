/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{AlphaType, ClipMode, DeviceIntRect, DeviceIntPoint, DeviceIntSize};
use api::{ExternalImageType, FilterOp, ImageRendering, LayoutRect, LayoutSize};
use api::{YuvColorSpace, YuvFormat, PictureRect, ColorDepth, LayoutPoint};
use clip::{ClipDataStore, ClipNodeFlags, ClipNodeRange, ClipItem, ClipStore};
use clip_scroll_tree::{ClipScrollTree, ROOT_SPATIAL_NODE_INDEX, SpatialNodeIndex};
use glyph_rasterizer::GlyphFormat;
use gpu_cache::{GpuCache, GpuCacheHandle, GpuCacheAddress};
use gpu_types::{BrushFlags, BrushInstance, PrimitiveHeaders, ZBufferId, ZBufferIdGenerator};
use gpu_types::{ClipMaskInstance, SplitCompositeInstance};
use gpu_types::{PrimitiveInstanceData, RasterizationSpace, GlyphInstance};
use gpu_types::{PrimitiveHeader, PrimitiveHeaderIndex, TransformPaletteId, TransformPalette};
use internal_types::{FastHashMap, SavedTargetIndex, TextureSource};
use picture::{Picture3DContext, PictureCompositeMode, PicturePrimitive, PictureSurface};
use prim_store::{DeferredResolve, PrimitiveTemplateKind, PrimitiveDataStore};
use prim_store::{EdgeAaSegmentMask, ImageSource, PrimitiveInstanceKind};
use prim_store::{VisibleGradientTile, PrimitiveInstance, PrimitiveOpacity, SegmentInstanceIndex};
use prim_store::{BrushSegment, ClipMaskKind, ClipTaskIndex};
use render_task::{RenderTaskAddress, RenderTaskId, RenderTaskTree};
use renderer::{BlendMode, ImageBufferKind, ShaderColorMode};
use renderer::BLOCKS_PER_UV_RECT;
use resource_cache::{CacheItem, GlyphFetchResult, ImageRequest, ResourceCache, ImageProperties};
use scene::FilterOpHelpers;
use smallvec::SmallVec;
use std::{f32, i32, usize};
use tiling::{RenderTargetContext};
use util::{TransformedRectKind};

// Special sentinel value recognized by the shader. It is considered to be
// a dummy task that doesn't mask out anything.
const OPAQUE_TASK_ADDRESS: RenderTaskAddress = RenderTaskAddress(0x7fff);

/// Used to signal there are no segments provided with this primitive.
const INVALID_SEGMENT_INDEX: i32 = 0xffff;

#[derive(Copy, Clone, PartialEq, Eq, Hash, Debug)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub enum BrushBatchKind {
    Solid,
    Image(ImageBufferKind),
    Blend,
    MixBlend {
        task_id: RenderTaskId,
        source_id: RenderTaskId,
        backdrop_id: RenderTaskId,
    },
    YuvImage(ImageBufferKind, YuvFormat, ColorDepth, YuvColorSpace),
    RadialGradient,
    LinearGradient,
}

#[derive(Copy, Clone, PartialEq, Eq, Hash, Debug)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub enum BatchKind {
    SplitComposite,
    TextRun(GlyphFormat),
    Brush(BrushBatchKind),
}

/// Optional textures that can be used as a source in the shaders.
/// Textures that are not used by the batch are equal to TextureId::invalid().
#[derive(Copy, Clone, Debug)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct BatchTextures {
    pub colors: [TextureSource; 3],
}

impl BatchTextures {
    pub fn no_texture() -> Self {
        BatchTextures {
            colors: [TextureSource::Invalid; 3],
        }
    }

    pub fn render_target_cache() -> Self {
        BatchTextures {
            colors: [
                TextureSource::PrevPassColor,
                TextureSource::PrevPassAlpha,
                TextureSource::Invalid,
            ],
        }
    }

    pub fn color(texture: TextureSource) -> Self {
        BatchTextures {
            colors: [texture, texture, TextureSource::Invalid],
        }
    }
}

#[derive(Copy, Clone, Debug)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct BatchKey {
    pub kind: BatchKind,
    pub blend_mode: BlendMode,
    pub textures: BatchTextures,
}

impl BatchKey {
    pub fn new(kind: BatchKind, blend_mode: BlendMode, textures: BatchTextures) -> Self {
        BatchKey {
            kind,
            blend_mode,
            textures,
        }
    }

    pub fn is_compatible_with(&self, other: &BatchKey) -> bool {
        self.kind == other.kind && self.blend_mode == other.blend_mode &&
            textures_compatible(self.textures.colors[0], other.textures.colors[0]) &&
            textures_compatible(self.textures.colors[1], other.textures.colors[1]) &&
            textures_compatible(self.textures.colors[2], other.textures.colors[2])
    }
}

#[inline]
fn textures_compatible(t1: TextureSource, t2: TextureSource) -> bool {
    t1 == TextureSource::Invalid || t2 == TextureSource::Invalid || t1 == t2
}

pub struct AlphaBatchList {
    pub batches: Vec<PrimitiveBatch>,
    pub item_rects: Vec<Vec<PictureRect>>,
    current_batch_index: usize,
    current_z_id: ZBufferId,
}

impl AlphaBatchList {
    fn new() -> Self {
        AlphaBatchList {
            batches: Vec::new(),
            item_rects: Vec::new(),
            current_z_id: ZBufferId::invalid(),
            current_batch_index: usize::MAX,
        }
    }

    pub fn set_params_and_get_batch(
        &mut self,
        key: BatchKey,
        bounding_rect: &PictureRect,
        z_id: ZBufferId,
    ) -> &mut Vec<PrimitiveInstanceData> {
        if z_id != self.current_z_id ||
           self.current_batch_index == usize::MAX ||
           !self.batches[self.current_batch_index].key.is_compatible_with(&key)
        {
            let mut selected_batch_index = None;

            match key.blend_mode {
                BlendMode::SubpixelWithBgColor => {
                    'outer_multipass: for (batch_index, batch) in self.batches.iter().enumerate().rev().take(10) {
                        // Some subpixel batches are drawn in two passes. Because of this, we need
                        // to check for overlaps with every batch (which is a bit different
                        // than the normal batching below).
                        for item_rect in &self.item_rects[batch_index] {
                            if item_rect.intersects(bounding_rect) {
                                break 'outer_multipass;
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
                        for item_rect in &self.item_rects[batch_index] {
                            if item_rect.intersects(bounding_rect) {
                                break 'outer_default;
                            }
                        }
                    }
                }
            }

            if selected_batch_index.is_none() {
                let new_batch = PrimitiveBatch::new(key);
                selected_batch_index = Some(self.batches.len());
                self.batches.push(new_batch);
                self.item_rects.push(Vec::new());
            }

            self.current_batch_index = selected_batch_index.unwrap();
            self.item_rects[self.current_batch_index].push(*bounding_rect);
            self.current_z_id = z_id;
        }

        &mut self.batches[self.current_batch_index].instances
    }
}

pub struct OpaqueBatchList {
    pub pixel_area_threshold_for_new_batch: f32,
    pub batches: Vec<PrimitiveBatch>,
    pub current_batch_index: usize,
}

impl OpaqueBatchList {
    fn new(pixel_area_threshold_for_new_batch: f32) -> Self {
        OpaqueBatchList {
            batches: Vec::new(),
            pixel_area_threshold_for_new_batch,
            current_batch_index: usize::MAX,
        }
    }

    pub fn set_params_and_get_batch(
        &mut self,
        key: BatchKey,
        bounding_rect: &PictureRect,
    ) -> &mut Vec<PrimitiveInstanceData> {
        if self.current_batch_index == usize::MAX ||
           !self.batches[self.current_batch_index].key.is_compatible_with(&key) {
            let mut selected_batch_index = None;
            let item_area = bounding_rect.size.area();

            // If the area of this primitive is larger than the given threshold,
            // then it is large enough to warrant breaking a batch for. In this
            // case we just see if it can be added to the existing batch or
            // create a new one.
            if item_area > self.pixel_area_threshold_for_new_batch {
                if let Some(batch) = self.batches.last() {
                    if batch.key.is_compatible_with(&key) {
                        selected_batch_index = Some(self.batches.len() - 1);
                    }
                }
            } else {
                // Otherwise, look back through a reasonable number of batches.
                for (batch_index, batch) in self.batches.iter().enumerate().rev().take(10) {
                    if batch.key.is_compatible_with(&key) {
                        selected_batch_index = Some(batch_index);
                        break;
                    }
                }
            }

            if selected_batch_index.is_none() {
                let new_batch = PrimitiveBatch::new(key);
                selected_batch_index = Some(self.batches.len());
                self.batches.push(new_batch);
            }

            self.current_batch_index = selected_batch_index.unwrap();
        }

        &mut self.batches[self.current_batch_index].instances
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
    pub fn new(screen_size: DeviceIntSize) -> Self {
        // The threshold for creating a new batch is
        // one quarter the screen size.
        let batch_area_threshold = (screen_size.width * screen_size.height) as f32 / 4.0;

        BatchList {
            alpha_batch_list: AlphaBatchList::new(),
            opaque_batch_list: OpaqueBatchList::new(batch_area_threshold),
        }
    }

    pub fn push_single_instance(
        &mut self,
        key: BatchKey,
        bounding_rect: &PictureRect,
        z_id: ZBufferId,
        instance: PrimitiveInstanceData,
    ) {
        match key.blend_mode {
            BlendMode::None => {
                self.opaque_batch_list
                    .set_params_and_get_batch(key, bounding_rect)
                    .push(instance);
            }
            BlendMode::Alpha |
            BlendMode::PremultipliedAlpha |
            BlendMode::PremultipliedDestOut |
            BlendMode::SubpixelConstantTextColor(..) |
            BlendMode::SubpixelWithBgColor |
            BlendMode::SubpixelDualSource => {
                self.alpha_batch_list
                    .set_params_and_get_batch(key, bounding_rect, z_id)
                    .push(instance);
            }
        }
    }

    pub fn set_params_and_get_batch(
        &mut self,
        key: BatchKey,
        bounding_rect: &PictureRect,
        z_id: ZBufferId,
    ) -> &mut Vec<PrimitiveInstanceData> {
        match key.blend_mode {
            BlendMode::None => {
                self.opaque_batch_list
                    .set_params_and_get_batch(key, bounding_rect)
            }
            BlendMode::Alpha |
            BlendMode::PremultipliedAlpha |
            BlendMode::PremultipliedDestOut |
            BlendMode::SubpixelConstantTextColor(..) |
            BlendMode::SubpixelWithBgColor |
            BlendMode::SubpixelDualSource => {
                self.alpha_batch_list
                    .set_params_and_get_batch(key, bounding_rect, z_id)
            }
        }
    }

    fn finalize(&mut self) {
        self.opaque_batch_list.finalize()
    }
}

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct PrimitiveBatch {
    pub key: BatchKey,
    pub instances: Vec<PrimitiveInstanceData>,
}

impl PrimitiveBatch {
    fn new(key: BatchKey) -> PrimitiveBatch {
        PrimitiveBatch {
            key,
            instances: Vec::new(),
        }
    }
}

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct AlphaBatchContainer {
    pub opaque_batches: Vec<PrimitiveBatch>,
    pub alpha_batches: Vec<PrimitiveBatch>,
    pub target_rect: Option<DeviceIntRect>,
}

impl AlphaBatchContainer {
    pub fn new(target_rect: Option<DeviceIntRect>) -> AlphaBatchContainer {
        AlphaBatchContainer {
            opaque_batches: Vec::new(),
            alpha_batches: Vec::new(),
            target_rect,
        }
    }

    fn merge(&mut self, builder: AlphaBatchBuilder) {
        for other_batch in builder.batch_list.opaque_batch_list.batches {
            let batch_index = self.opaque_batches.iter().position(|batch| {
                batch.key.is_compatible_with(&other_batch.key)
            });

            match batch_index {
                Some(batch_index) => {
                    self.opaque_batches[batch_index].instances.extend(other_batch.instances);
                }
                None => {
                    self.opaque_batches.push(other_batch);
                }
            }
        }

        let mut min_batch_index = 0;

        for other_batch in builder.batch_list.alpha_batch_list.batches {
            let batch_index = self.alpha_batches.iter().skip(min_batch_index).position(|batch| {
                batch.key.is_compatible_with(&other_batch.key)
            });

            match batch_index {
                Some(batch_index) => {
                    let batch_index = batch_index + min_batch_index;
                    self.alpha_batches[batch_index].instances.extend(other_batch.instances);
                    min_batch_index = batch_index;
                }
                None => {
                    self.alpha_batches.push(other_batch);
                    min_batch_index = self.alpha_batches.len();
                }
            }
        }
    }
}

/// Each segment can optionally specify a per-segment
/// texture set and one user data field.
#[derive(Debug, Copy, Clone)]
struct SegmentInstanceData {
    textures: BatchTextures,
    user_data: i32,
}

/// Encapsulates the logic of building batches for items that are blended.
pub struct AlphaBatchBuilder {
    pub batch_list: BatchList,
    glyph_fetch_buffer: Vec<GlyphFetchResult>,
    target_rect: DeviceIntRect,
    can_merge: bool,
}

impl AlphaBatchBuilder {
    pub fn new(
        screen_size: DeviceIntSize,
        target_rect: DeviceIntRect,
        can_merge: bool,
    ) -> Self {
        AlphaBatchBuilder {
            batch_list: BatchList::new(screen_size),
            glyph_fetch_buffer: Vec::new(),
            target_rect,
            can_merge,
        }
    }

    pub fn build(mut self, merged_batches: &mut AlphaBatchContainer) -> Option<AlphaBatchContainer> {
        self.batch_list.finalize();

        if self.can_merge {
            merged_batches.merge(self);
            None
        } else {
            Some(AlphaBatchContainer {
                alpha_batches: self.batch_list.alpha_batch_list.batches,
                opaque_batches: self.batch_list.opaque_batch_list.batches,
                target_rect: Some(self.target_rect),
            })
        }
    }

    pub fn add_pic_to_batch(
        &mut self,
        pic: &PicturePrimitive,
        task_id: RenderTaskId,
        ctx: &RenderTargetContext,
        gpu_cache: &mut GpuCache,
        render_tasks: &RenderTaskTree,
        deferred_resolves: &mut Vec<DeferredResolve>,
        prim_headers: &mut PrimitiveHeaders,
        transforms: &mut TransformPalette,
        root_spatial_node_index: SpatialNodeIndex,
        z_generator: &mut ZBufferIdGenerator,
    ) {
        let task_address = render_tasks.get_task_address(task_id);

        // Add each run in this picture to the batch.
        for prim_instance in &pic.prim_list.prim_instances {
            self.add_prim_to_batch(
                prim_instance,
                ctx,
                gpu_cache,
                render_tasks,
                task_id,
                task_address,
                deferred_resolves,
                prim_headers,
                transforms,
                root_spatial_node_index,
                z_generator,
            );
        }
    }

    // Adds a primitive to a batch.
    // It can recursively call itself in some situations, for
    // example if it encounters a picture where the items
    // in that picture are being drawn into the same target.
    fn add_prim_to_batch(
        &mut self,
        prim_instance: &PrimitiveInstance,
        ctx: &RenderTargetContext,
        gpu_cache: &mut GpuCache,
        render_tasks: &RenderTaskTree,
        task_id: RenderTaskId,
        task_address: RenderTaskAddress,
        deferred_resolves: &mut Vec<DeferredResolve>,
        prim_headers: &mut PrimitiveHeaders,
        transforms: &mut TransformPalette,
        root_spatial_node_index: SpatialNodeIndex,
        z_generator: &mut ZBufferIdGenerator,
    ) {
        if prim_instance.bounding_rect.is_none() {
            return;
        }

        #[cfg(debug_assertions)] //TODO: why is this needed?
        debug_assert_eq!(prim_instance.prepared_frame_id, render_tasks.frame_id());

        let transform_id = transforms
            .get_id(
                prim_instance.spatial_node_index,
                root_spatial_node_index,
                ctx.clip_scroll_tree,
            );

        // TODO(gw): Calculating this for every primitive is a bit
        //           wasteful. We should probably cache this in
        //           the scroll node...
        let transform_kind = transform_id.transform_kind();
        let bounding_rect = prim_instance.bounding_rect
                                         .as_ref()
                                         .expect("bug");
        let z_id = z_generator.next();

        // Get the clip task address for the global primitive, if one was set.
        let clip_task_address = get_clip_task_address(
            &ctx.scratch.clip_mask_instances,
            prim_instance.clip_task_index,
            0,
            render_tasks,
        ).unwrap_or(OPAQUE_TASK_ADDRESS);

        let prim_data = &ctx.resources.as_common_data(&prim_instance);
        let prim_rect = LayoutRect::new(
            prim_instance.prim_origin,
            prim_data.prim_size,
        );

        match prim_instance.kind {
            PrimitiveInstanceKind::Clear { data_handle } => {
                let prim_data = &ctx.resources.prim_data_store[data_handle];
                let prim_cache_address = gpu_cache.get_address(&prim_data.gpu_cache_handle);

                // TODO(gw): We can abstract some of the common code below into
                //           helper methods, as we port more primitives to make
                //           use of interning.

                let prim_header = PrimitiveHeader {
                    local_rect: prim_rect,
                    local_clip_rect: prim_instance.combined_local_clip_rect,
                    task_address,
                    specific_prim_address: prim_cache_address,
                    clip_task_address,
                    transform_id,
                };

                let prim_header_index = prim_headers.push(
                    &prim_header,
                    z_id,
                    [get_shader_opacity(1.0), 0, 0],
                );

                let batch_key = BatchKey {
                    blend_mode: BlendMode::PremultipliedDestOut,
                    kind: BatchKind::Brush(BrushBatchKind::Solid),
                    textures: BatchTextures::no_texture(),
                };

                let instance = PrimitiveInstanceData::from(BrushInstance {
                    segment_index: INVALID_SEGMENT_INDEX,
                    edge_flags: EdgeAaSegmentMask::all(),
                    clip_task_address,
                    brush_flags: BrushFlags::PERSPECTIVE_INTERPOLATION,
                    prim_header_index,
                    user_data: 0,
                });

                self.batch_list.push_single_instance(
                    batch_key,
                    bounding_rect,
                    z_id,
                    PrimitiveInstanceData::from(instance),
                );
            }
            PrimitiveInstanceKind::NormalBorder { data_handle, ref cache_handles, .. } => {
                let prim_data = &ctx.resources.prim_data_store[data_handle];
                let prim_cache_address = gpu_cache.get_address(&prim_data.gpu_cache_handle);
                let cache_handles = &ctx.scratch.border_cache_handles[*cache_handles];
                let specified_blend_mode = BlendMode::PremultipliedAlpha;
                let mut segment_data: SmallVec<[SegmentInstanceData; 8]> = SmallVec::new();

                // Collect the segment instance data from each render
                // task for each valid edge / corner of the border.

                for handle in cache_handles {
                    let rt_cache_entry = ctx.resource_cache
                        .get_cached_render_task(handle);
                    let cache_item = ctx.resource_cache
                        .get_texture_cache_item(&rt_cache_entry.handle);
                    segment_data.push(
                        SegmentInstanceData {
                            textures: BatchTextures::color(cache_item.texture_id),
                            user_data: cache_item.uv_rect_handle.as_int(gpu_cache),
                        }
                    );
                }

                let non_segmented_blend_mode = if !prim_data.opacity.is_opaque ||
                    prim_instance.clip_task_index != ClipTaskIndex::INVALID ||
                    transform_kind == TransformedRectKind::Complex
                {
                    specified_blend_mode
                } else {
                    BlendMode::None
                };

                let prim_header = PrimitiveHeader {
                    local_rect: prim_rect,
                    local_clip_rect: prim_instance.combined_local_clip_rect,
                    task_address,
                    specific_prim_address: prim_cache_address,
                    clip_task_address,
                    transform_id,
                };

                let batch_params = BrushBatchParameters::instanced(
                    BrushBatchKind::Image(ImageBufferKind::Texture2DArray),
                    [
                        ShaderColorMode::Image as i32 | ((AlphaType::PremultipliedAlpha as i32) << 16),
                        RasterizationSpace::Local as i32,
                        get_shader_opacity(1.0),
                    ],
                    segment_data,
                );

                let prim_header_index = prim_headers.push(
                    &prim_header,
                    z_id,
                    batch_params.prim_user_data,
                );

                let template = match prim_data.kind {
                    PrimitiveTemplateKind::NormalBorder { ref template, .. } => template,
                    _ => unreachable!()
                };
                self.add_segmented_prim_to_batch(
                    Some(template.brush_segments.as_slice()),
                    prim_data.opacity,
                    &batch_params,
                    specified_blend_mode,
                    non_segmented_blend_mode,
                    prim_header_index,
                    clip_task_address,
                    bounding_rect,
                    transform_kind,
                    render_tasks,
                    z_id,
                    prim_instance.clip_task_index,
                    ctx,
                );
            }
            PrimitiveInstanceKind::TextRun { data_handle, run_index, .. } => {
                let run = &ctx.prim_store.text_runs[run_index];
                let subpx_dir = run.used_font.get_subpx_dir();

                // The GPU cache data is stored in the template and reused across
                // frames and display lists.
                let prim_data = &ctx.resources.text_run_data_store[data_handle];
                let glyph_fetch_buffer = &mut self.glyph_fetch_buffer;
                let alpha_batch_list = &mut self.batch_list.alpha_batch_list;
                let prim_cache_address = gpu_cache.get_address(&prim_data.gpu_cache_handle);

                let prim_header = PrimitiveHeader {
                    local_rect: prim_rect,
                    local_clip_rect: prim_instance.combined_local_clip_rect,
                    task_address,
                    specific_prim_address: prim_cache_address,
                    clip_task_address,
                    transform_id,
                };

                let glyph_keys = &ctx.scratch.glyph_keys[run.glyph_keys_range];

                ctx.resource_cache.fetch_glyphs(
                    run.used_font.clone(),
                    &glyph_keys,
                    glyph_fetch_buffer,
                    gpu_cache,
                    |texture_id, mut glyph_format, glyphs| {
                        debug_assert_ne!(texture_id, TextureSource::Invalid);

                        // Ignore color and only sample alpha when shadowing.
                        if run.shadow {
                            glyph_format = glyph_format.ignore_color();
                        }

                        let subpx_dir = subpx_dir.limit_by(glyph_format);

                        let textures = BatchTextures {
                            colors: [
                                texture_id,
                                TextureSource::Invalid,
                                TextureSource::Invalid,
                            ],
                        };

                        let kind = BatchKind::TextRun(glyph_format);

                        let (blend_mode, color_mode) = match glyph_format {
                            GlyphFormat::Subpixel |
                            GlyphFormat::TransformedSubpixel => {
                                if run.used_font.bg_color.a != 0 {
                                    (
                                        BlendMode::SubpixelWithBgColor,
                                        ShaderColorMode::FromRenderPassMode,
                                    )
                                } else if ctx.use_dual_source_blending {
                                    (
                                        BlendMode::SubpixelDualSource,
                                        ShaderColorMode::SubpixelDualSource,
                                    )
                                } else {
                                    (
                                        BlendMode::SubpixelConstantTextColor(run.used_font.color.into()),
                                        ShaderColorMode::SubpixelConstantTextColor,
                                    )
                                }
                            }
                            GlyphFormat::Alpha |
                            GlyphFormat::TransformedAlpha => {
                                (
                                    BlendMode::PremultipliedAlpha,
                                    ShaderColorMode::Alpha,
                                )
                            }
                            GlyphFormat::Bitmap => {
                                (
                                    BlendMode::PremultipliedAlpha,
                                    ShaderColorMode::Bitmap,
                                )
                            }
                            GlyphFormat::ColorBitmap => {
                                (
                                    BlendMode::PremultipliedAlpha,
                                    ShaderColorMode::ColorBitmap,
                                )
                            }
                        };

                        let prim_header_index = prim_headers.push(&prim_header, z_id, [0; 3]);
                        let key = BatchKey::new(kind, blend_mode, textures);
                        let base_instance = GlyphInstance::new(
                            prim_header_index,
                        );
                        let batch = alpha_batch_list.set_params_and_get_batch(
                            key,
                            bounding_rect,
                            z_id,
                        );

                        for glyph in glyphs {
                            batch.push(base_instance.build(
                                glyph.index_in_text_run,
                                glyph.uv_rect_address.as_int(),
                                (subpx_dir as u32 as i32) << 16 |
                                (color_mode as u32 as i32),
                            ));
                        }
                    },
                );
            }
            PrimitiveInstanceKind::LineDecoration { data_handle, ref cache_handle, .. } => {
                // The GPU cache data is stored in the template and reused across
                // frames and display lists.

                let prim_data = &ctx.resources.prim_data_store[data_handle];
                let prim_cache_address = gpu_cache.get_address(&prim_data.gpu_cache_handle);

                let (batch_kind, textures, prim_user_data, segment_user_data) = match cache_handle {
                    Some(cache_handle) => {
                        let rt_cache_entry = ctx
                            .resource_cache
                            .get_cached_render_task(cache_handle);
                        let cache_item = ctx
                            .resource_cache
                            .get_texture_cache_item(&rt_cache_entry.handle);
                        let textures = BatchTextures::color(cache_item.texture_id);
                        (
                            BrushBatchKind::Image(get_buffer_kind(cache_item.texture_id)),
                            textures,
                            [
                                ShaderColorMode::Image as i32 | ((AlphaType::PremultipliedAlpha as i32) << 16),
                                RasterizationSpace::Local as i32,
                                get_shader_opacity(1.0),
                            ],
                            cache_item.uv_rect_handle.as_int(gpu_cache),
                        )
                    }
                    None => {
                        (
                            BrushBatchKind::Solid,
                            BatchTextures::no_texture(),
                            [get_shader_opacity(1.0), 0, 0],
                            0,
                        )
                    }
                };

                // TODO(gw): We can abstract some of the common code below into
                //           helper methods, as we port more primitives to make
                //           use of interning.
                let blend_mode = if !prim_data.opacity.is_opaque ||
                    prim_instance.clip_task_index != ClipTaskIndex::INVALID ||
                    transform_kind == TransformedRectKind::Complex
                {
                    BlendMode::PremultipliedAlpha
                } else {
                    BlendMode::None
                };

                let prim_header = PrimitiveHeader {
                    local_rect: prim_rect,
                    local_clip_rect: prim_instance.combined_local_clip_rect,
                    task_address,
                    specific_prim_address: prim_cache_address,
                    clip_task_address,
                    transform_id,
                };

                let prim_header_index = prim_headers.push(
                    &prim_header,
                    z_id,
                    prim_user_data,
                );

                let batch_key = BatchKey {
                    blend_mode,
                    kind: BatchKind::Brush(batch_kind),
                    textures: textures,
                };

                let instance = PrimitiveInstanceData::from(BrushInstance {
                    segment_index: INVALID_SEGMENT_INDEX,
                    edge_flags: EdgeAaSegmentMask::all(),
                    clip_task_address,
                    brush_flags: BrushFlags::PERSPECTIVE_INTERPOLATION,
                    prim_header_index,
                    user_data: segment_user_data,
                });

                self.batch_list.push_single_instance(
                    batch_key,
                    bounding_rect,
                    z_id,
                    PrimitiveInstanceData::from(instance),
                );
            }
            PrimitiveInstanceKind::Picture { pic_index, .. } => {
                let picture = &ctx.prim_store.pictures[pic_index.0];
                let non_segmented_blend_mode = BlendMode::PremultipliedAlpha;
                let prim_cache_address = gpu_cache.get_address(&picture.gpu_location);

                let prim_header = PrimitiveHeader {
                    local_rect: picture.local_rect,
                    local_clip_rect: prim_instance.combined_local_clip_rect,
                    task_address,
                    specific_prim_address: prim_cache_address,
                    clip_task_address,
                    transform_id,
                };

                match picture.context_3d {
                    // Convert all children of the 3D hierarchy root into batches.
                    Picture3DContext::In { root_data: Some(ref list), .. } => {
                        for child in list {
                            let prim_instance = &picture.prim_list.prim_instances[child.anchor];
                            let pic_index = match prim_instance.kind {
                                PrimitiveInstanceKind::Picture { pic_index, .. } => pic_index,
                                PrimitiveInstanceKind::LineDecoration { .. } |
                                PrimitiveInstanceKind::TextRun { .. } |
                                PrimitiveInstanceKind::NormalBorder { .. } |
                                PrimitiveInstanceKind::ImageBorder { .. } |
                                PrimitiveInstanceKind::Rectangle { .. } |
                                PrimitiveInstanceKind::YuvImage { .. } |
                                PrimitiveInstanceKind::Image { .. } |
                                PrimitiveInstanceKind::LinearGradient { .. } |
                                PrimitiveInstanceKind::RadialGradient { .. } |
                                PrimitiveInstanceKind::Clear { .. } => {
                                    unreachable!();
                                }
                            };
                            let pic = &ctx.prim_store.pictures[pic_index.0];

                            // Get clip task, if set, for the picture primitive.
                            let clip_task_address = get_clip_task_address(
                                &ctx.scratch.clip_mask_instances,
                                prim_instance.clip_task_index,
                                0,
                                render_tasks,
                            ).unwrap_or(OPAQUE_TASK_ADDRESS);

                            let prim_header = PrimitiveHeader {
                                local_rect: pic.local_rect,
                                local_clip_rect: prim_instance.combined_local_clip_rect,
                                task_address,
                                specific_prim_address: GpuCacheAddress::invalid(),
                                clip_task_address,
                                transform_id: child.transform_id,
                            };

                            let surface_index = pic
                                .raster_config
                                .as_ref()
                                .expect("BUG: 3d primitive was not assigned a surface")
                                .surface_index;
                            let (uv_rect_address, _) = ctx
                                .surfaces[surface_index.0]
                                .surface
                                .as_ref()
                                .expect("BUG: no surface")
                                .resolve(
                                    render_tasks,
                                    ctx.resource_cache,
                                    gpu_cache,
                                );

                            let prim_header_index = prim_headers.push(&prim_header, z_id, [
                                uv_rect_address.as_int(),
                                0,
                                0,
                            ]);

                            let key = BatchKey::new(
                                BatchKind::SplitComposite,
                                BlendMode::PremultipliedAlpha,
                                BatchTextures::no_texture(),
                            );

                            let instance = SplitCompositeInstance::new(
                                prim_header_index,
                                child.gpu_address,
                                z_id,
                            );

                            self.batch_list.push_single_instance(
                                key,
                                &prim_instance.bounding_rect.as_ref().expect("bug"),
                                z_id,
                                PrimitiveInstanceData::from(instance),
                            );
                        }
                    }
                    // Ignore the 3D pictures that are not in the root of preserve-3D
                    // hierarchy, since we process them with the root.
                    Picture3DContext::In { root_data: None, .. } => return,
                    // Proceed for non-3D pictures.
                    Picture3DContext::Out => ()
                }

                match picture.raster_config {
                    Some(ref raster_config) => {
                        match raster_config.composite_mode {
                            PictureCompositeMode::TileCache { .. } => {

                                // Step through each tile in the cache, and draw it with an image
                                // brush primitive if visible.

                                let kind = BatchKind::Brush(
                                    BrushBatchKind::Image(ImageBufferKind::Texture2DArray)
                                );

                                let tile_cache = picture.tile_cache.as_ref().unwrap();

                                for y in 0 .. tile_cache.tile_rect.size.height {
                                    for x in 0 .. tile_cache.tile_rect.size.width {
                                        let i = y * tile_cache.tile_rect.size.width + x;
                                        let tile = &tile_cache.tiles[i as usize];

                                        // Check if the tile is visible.
                                        if !tile.is_visible || !tile.in_use {
                                            continue;
                                        }

                                        // Get the local rect of the tile.
                                        let tile_rect = LayoutRect::new(
                                            LayoutPoint::new(
                                                (tile_cache.tile_rect.origin.x + x) as f32 * tile_cache.local_tile_size.width,
                                                (tile_cache.tile_rect.origin.y + y) as f32 * tile_cache.local_tile_size.height,
                                            ),
                                            LayoutSize::new(
                                                tile_cache.local_tile_size.width,
                                                tile_cache.local_tile_size.height,
                                            ),
                                        );

                                        // Construct a local clip rect that ensures we only draw pixels where
                                        // the local bounds of the picture extend to within the edge tiles.
                                        let local_clip_rect = prim_instance
                                            .combined_local_clip_rect
                                            .intersection(&picture.local_rect)
                                            .expect("bug: invalid picture local rect");

                                        let prim_header = PrimitiveHeader {
                                            local_rect: tile_rect,
                                            local_clip_rect,
                                            task_address,
                                            specific_prim_address: prim_cache_address,
                                            clip_task_address,
                                            transform_id,
                                        };

                                        let prim_header_index = prim_headers.push(&prim_header, z_id, [
                                            ShaderColorMode::Image as i32 | ((AlphaType::PremultipliedAlpha as i32) << 16),
                                            RasterizationSpace::Local as i32,
                                            get_shader_opacity(1.0),
                                        ]);

                                        let cache_item = ctx
                                            .resource_cache
                                            .get_texture_cache_item(&tile.handle);

                                        let key = BatchKey::new(
                                            kind,
                                            non_segmented_blend_mode,
                                            BatchTextures::color(cache_item.texture_id),
                                        );

                                        let uv_rect_address = gpu_cache
                                            .get_address(&cache_item.uv_rect_handle)
                                            .as_int();

                                        let instance = BrushInstance {
                                            prim_header_index,
                                            clip_task_address,
                                            segment_index: INVALID_SEGMENT_INDEX,
                                            edge_flags: EdgeAaSegmentMask::empty(),
                                            brush_flags: BrushFlags::empty(),
                                            user_data: uv_rect_address,
                                        };

                                        // Instead of retrieving the batch once and adding each tile instance,
                                        // use this API to get an appropriate batch for each tile, since
                                        // the batch textures may be different. The batch list internally
                                        // caches the current batch if the key hasn't changed.
                                        let batch = self.batch_list.set_params_and_get_batch(
                                            key,
                                            bounding_rect,
                                            z_id,
                                        );

                                        batch.push(PrimitiveInstanceData::from(instance));
                                    }
                                }
                            }
                            PictureCompositeMode::Filter(filter) => {
                                let surface = ctx.surfaces[raster_config.surface_index.0]
                                    .surface
                                    .as_ref()
                                    .expect("bug: surface must be allocated by now");
                                assert!(filter.is_visible());
                                match filter {
                                    FilterOp::Blur(..) => {
                                        let kind = BatchKind::Brush(
                                            BrushBatchKind::Image(ImageBufferKind::Texture2DArray)
                                        );
                                        let (uv_rect_address, textures) = surface
                                            .resolve(
                                                render_tasks,
                                                ctx.resource_cache,
                                                gpu_cache,
                                            );
                                        let key = BatchKey::new(
                                            kind,
                                            non_segmented_blend_mode,
                                            textures,
                                        );
                                        let prim_header_index = prim_headers.push(&prim_header, z_id, [
                                            ShaderColorMode::Image as i32 | ((AlphaType::PremultipliedAlpha as i32) << 16),
                                            RasterizationSpace::Screen as i32,
                                            get_shader_opacity(1.0),
                                        ]);

                                        let instance = BrushInstance {
                                            prim_header_index,
                                            segment_index: INVALID_SEGMENT_INDEX,
                                            edge_flags: EdgeAaSegmentMask::empty(),
                                            brush_flags: BrushFlags::empty(),
                                            clip_task_address,
                                            user_data: uv_rect_address.as_int(),
                                        };

                                        self.batch_list.push_single_instance(
                                            key,
                                            bounding_rect,
                                            z_id,
                                            PrimitiveInstanceData::from(instance),
                                        );
                                    }
                                    FilterOp::DropShadow(offset, ..) => {
                                        // Draw an instance of the shadow first, following by the content.

                                        // Both the shadow and the content get drawn as a brush image.
                                        let kind = BatchKind::Brush(
                                            BrushBatchKind::Image(ImageBufferKind::Texture2DArray),
                                        );

                                        // Gets the saved render task ID of the content, which is
                                        // deeper in the render task tree than the direct child.
                                        let secondary_id = picture.secondary_render_task_id.expect("no secondary!?");
                                        let saved_index = render_tasks[secondary_id].saved_index.expect("no saved index!?");
                                        debug_assert_ne!(saved_index, SavedTargetIndex::PENDING);

                                        // Build BatchTextures for shadow/content
                                        let shadow_textures = BatchTextures::render_target_cache();
                                        let content_textures = BatchTextures {
                                            colors: [
                                                TextureSource::RenderTaskCache(saved_index),
                                                TextureSource::Invalid,
                                                TextureSource::Invalid,
                                            ],
                                        };

                                        // Build batch keys for shadow/content
                                        let shadow_key = BatchKey::new(kind, non_segmented_blend_mode, shadow_textures);
                                        let content_key = BatchKey::new(kind, non_segmented_blend_mode, content_textures);

                                        // Retrieve the UV rect addresses for shadow/content.
                                        let cache_task_id = surface.resolve_render_task_id();
                                        let shadow_uv_rect_address = render_tasks[cache_task_id]
                                            .get_texture_address(gpu_cache)
                                            .as_int();
                                        let content_uv_rect_address = render_tasks[secondary_id]
                                            .get_texture_address(gpu_cache)
                                            .as_int();

                                        // Get the GPU cache address of the extra data handle.
                                        let shadow_prim_address = gpu_cache.get_address(&picture.extra_gpu_data_handle);

                                        let z_id_shadow = z_id;
                                        let z_id_content = z_generator.next();

                                        let content_prim_header_index = prim_headers.push(&prim_header, z_id_content, [
                                            ShaderColorMode::Image as i32 | ((AlphaType::PremultipliedAlpha as i32) << 16),
                                            RasterizationSpace::Screen as i32,
                                            get_shader_opacity(1.0),
                                        ]);

                                        let shadow_rect = prim_header.local_rect.translate(&offset);

                                        let shadow_prim_header = PrimitiveHeader {
                                            local_rect: shadow_rect,
                                            specific_prim_address: shadow_prim_address,
                                            ..prim_header
                                        };

                                        let shadow_prim_header_index = prim_headers.push(&shadow_prim_header, z_id_shadow, [
                                            ShaderColorMode::Alpha as i32 | ((AlphaType::PremultipliedAlpha as i32) << 16),
                                            RasterizationSpace::Screen as i32,
                                            get_shader_opacity(1.0),
                                        ]);

                                        let shadow_instance = BrushInstance {
                                            prim_header_index: shadow_prim_header_index,
                                            clip_task_address,
                                            segment_index: INVALID_SEGMENT_INDEX,
                                            edge_flags: EdgeAaSegmentMask::empty(),
                                            brush_flags: BrushFlags::empty(),
                                            user_data: shadow_uv_rect_address,
                                        };

                                        let content_instance = BrushInstance {
                                            prim_header_index: content_prim_header_index,
                                            clip_task_address,
                                            segment_index: INVALID_SEGMENT_INDEX,
                                            edge_flags: EdgeAaSegmentMask::empty(),
                                            brush_flags: BrushFlags::empty(),
                                            user_data: content_uv_rect_address,
                                        };

                                        self.batch_list.push_single_instance(
                                            shadow_key,
                                            bounding_rect,
                                            z_id_shadow,
                                            PrimitiveInstanceData::from(shadow_instance),
                                        );

                                        self.batch_list.push_single_instance(
                                            content_key,
                                            bounding_rect,
                                            z_id_content,
                                            PrimitiveInstanceData::from(content_instance),
                                        );
                                    }
                                    _ => {
                                        let filter_mode = match filter {
                                            FilterOp::Identity => 1, // matches `Contrast(1)`
                                            FilterOp::Blur(..) => 0,
                                            FilterOp::Contrast(..) => 1,
                                            FilterOp::Grayscale(..) => 2,
                                            FilterOp::HueRotate(..) => 3,
                                            FilterOp::Invert(..) => 4,
                                            FilterOp::Saturate(..) => 5,
                                            FilterOp::Sepia(..) => 6,
                                            FilterOp::Brightness(..) => 7,
                                            FilterOp::Opacity(..) => 8,
                                            FilterOp::DropShadow(..) => 9,
                                            FilterOp::ColorMatrix(..) => 10,
                                            FilterOp::SrgbToLinear => 11,
                                            FilterOp::LinearToSrgb => 12,
                                        };

                                        let user_data = match filter {
                                            FilterOp::Identity => 0x10000i32, // matches `Contrast(1)`
                                            FilterOp::Contrast(amount) |
                                            FilterOp::Grayscale(amount) |
                                            FilterOp::Invert(amount) |
                                            FilterOp::Saturate(amount) |
                                            FilterOp::Sepia(amount) |
                                            FilterOp::Brightness(amount) |
                                            FilterOp::Opacity(_, amount) => {
                                                (amount * 65536.0) as i32
                                            }
                                            FilterOp::SrgbToLinear | FilterOp::LinearToSrgb => 0,
                                            FilterOp::HueRotate(angle) => {
                                                (0.01745329251 * angle * 65536.0) as i32
                                            }
                                            // Go through different paths
                                            FilterOp::Blur(..) |
                                            FilterOp::DropShadow(..) => {
                                                unreachable!();
                                            }
                                            FilterOp::ColorMatrix(_) => {
                                                picture.extra_gpu_data_handle.as_int(gpu_cache)
                                            }
                                        };

                                        let (uv_rect_address, textures) = surface
                                            .resolve(
                                                render_tasks,
                                                ctx.resource_cache,
                                                gpu_cache,
                                            );

                                        let key = BatchKey::new(
                                            BatchKind::Brush(BrushBatchKind::Blend),
                                            BlendMode::PremultipliedAlpha,
                                            textures,
                                        );

                                        let prim_header_index = prim_headers.push(&prim_header, z_id, [
                                            uv_rect_address.as_int(),
                                            filter_mode,
                                            user_data,
                                        ]);

                                        let instance = BrushInstance {
                                            prim_header_index,
                                            clip_task_address,
                                            segment_index: INVALID_SEGMENT_INDEX,
                                            edge_flags: EdgeAaSegmentMask::empty(),
                                            brush_flags: BrushFlags::empty(),
                                            user_data: 0,
                                        };

                                        self.batch_list.push_single_instance(
                                            key,
                                            bounding_rect,
                                            z_id,
                                            PrimitiveInstanceData::from(instance),
                                        );
                                    }
                                }
                            }
                            PictureCompositeMode::MixBlend(mode) => {
                                let surface = ctx.surfaces[raster_config.surface_index.0]
                                    .surface
                                    .as_ref()
                                    .expect("bug: surface must be allocated by now");
                                let cache_task_id = surface.resolve_render_task_id();
                                let backdrop_id = picture.secondary_render_task_id.expect("no backdrop!?");

                                let key = BatchKey::new(
                                    BatchKind::Brush(
                                        BrushBatchKind::MixBlend {
                                            task_id,
                                            source_id: cache_task_id,
                                            backdrop_id,
                                        },
                                    ),
                                    BlendMode::PremultipliedAlpha,
                                    BatchTextures::no_texture(),
                                );
                                let backdrop_task_address = render_tasks.get_task_address(backdrop_id);
                                let source_task_address = render_tasks.get_task_address(cache_task_id);
                                let prim_header_index = prim_headers.push(&prim_header, z_id, [
                                    mode as u32 as i32,
                                    backdrop_task_address.0 as i32,
                                    source_task_address.0 as i32,
                                ]);

                                let instance = BrushInstance {
                                    prim_header_index,
                                    clip_task_address,
                                    segment_index: INVALID_SEGMENT_INDEX,
                                    edge_flags: EdgeAaSegmentMask::empty(),
                                    brush_flags: BrushFlags::empty(),
                                    user_data: 0,
                                };

                                self.batch_list.push_single_instance(
                                    key,
                                    bounding_rect,
                                    z_id,
                                    PrimitiveInstanceData::from(instance),
                                );
                            }
                            PictureCompositeMode::Blit => {
                                let surface = ctx.surfaces[raster_config.surface_index.0]
                                    .surface
                                    .as_ref()
                                    .expect("bug: surface must be allocated by now");
                                let cache_task_id = surface.resolve_render_task_id();
                                let kind = BatchKind::Brush(
                                    BrushBatchKind::Image(ImageBufferKind::Texture2DArray)
                                );
                                let key = BatchKey::new(
                                    kind,
                                    non_segmented_blend_mode,
                                    BatchTextures::render_target_cache(),
                                );

                                let uv_rect_address = render_tasks[cache_task_id]
                                    .get_texture_address(gpu_cache)
                                    .as_int();
                                let prim_header_index = prim_headers.push(&prim_header, z_id, [
                                    ShaderColorMode::Image as i32 | ((AlphaType::PremultipliedAlpha as i32) << 16),
                                    RasterizationSpace::Screen as i32,
                                    get_shader_opacity(1.0),
                                ]);

                                let instance = BrushInstance {
                                    prim_header_index,
                                    clip_task_address,
                                    segment_index: INVALID_SEGMENT_INDEX,
                                    edge_flags: EdgeAaSegmentMask::empty(),
                                    brush_flags: BrushFlags::empty(),
                                    user_data: uv_rect_address,
                                };

                                self.batch_list.push_single_instance(
                                    key,
                                    bounding_rect,
                                    z_id,
                                    PrimitiveInstanceData::from(instance),
                                );
                            }
                        }
                    }
                    None => {
                        // If this picture is being drawn into an existing target (i.e. with
                        // no composition operation), recurse and add to the current batch list.
                        self.add_pic_to_batch(
                            picture,
                            task_id,
                            ctx,
                            gpu_cache,
                            render_tasks,
                            deferred_resolves,
                            prim_headers,
                            transforms,
                            root_spatial_node_index,
                            z_generator,
                        );
                    }
                }
            }
            PrimitiveInstanceKind::ImageBorder { data_handle, .. } => {
                let prim_data = &ctx.resources.prim_data_store[data_handle];
                let (request, brush_segments) = match &prim_data.kind {
                    PrimitiveTemplateKind::ImageBorder { request, brush_segments, .. } => {
                        (request, brush_segments)
                    }
                    _ => unreachable!()
                };

                let cache_item = resolve_image(
                    *request,
                    ctx.resource_cache,
                    gpu_cache,
                    deferred_resolves,
                );
                if cache_item.texture_id == TextureSource::Invalid {
                    return;
                }

                let textures = BatchTextures::color(cache_item.texture_id);
                let prim_cache_address = gpu_cache.get_address(&prim_data.gpu_cache_handle);
                let specified_blend_mode = BlendMode::PremultipliedAlpha;
                let non_segmented_blend_mode = if !prim_data.opacity.is_opaque ||
                    prim_instance.clip_task_index != ClipTaskIndex::INVALID ||
                    transform_kind == TransformedRectKind::Complex
                {
                    specified_blend_mode
                } else {
                    BlendMode::None
                };

                let prim_header = PrimitiveHeader {
                    local_rect: prim_rect,
                    local_clip_rect: prim_instance.combined_local_clip_rect,
                    task_address,
                    specific_prim_address: prim_cache_address,
                    clip_task_address,
                    transform_id,
                };

                let batch_params = BrushBatchParameters::shared(
                    BrushBatchKind::Image(get_buffer_kind(cache_item.texture_id)),
                    textures,
                    [
                        ShaderColorMode::Image as i32 | ((AlphaType::PremultipliedAlpha as i32) << 16),
                        RasterizationSpace::Local as i32,
                        get_shader_opacity(1.0),
                    ],
                    cache_item.uv_rect_handle.as_int(gpu_cache),
                );

                let prim_header_index = prim_headers.push(
                    &prim_header,
                    z_id,
                    batch_params.prim_user_data,
                );

                self.add_segmented_prim_to_batch(
                    Some(brush_segments.as_slice()),
                    prim_data.opacity,
                    &batch_params,
                    specified_blend_mode,
                    non_segmented_blend_mode,
                    prim_header_index,
                    clip_task_address,
                    bounding_rect,
                    transform_kind,
                    render_tasks,
                    z_id,
                    prim_instance.clip_task_index,
                    ctx,
                );
            }
            PrimitiveInstanceKind::Rectangle { data_handle, segment_instance_index, opacity_binding_index, .. } => {
                let prim_data = &ctx.resources.prim_data_store[data_handle];
                let specified_blend_mode = BlendMode::PremultipliedAlpha;
                let opacity_binding = ctx.prim_store.get_opacity_binding(opacity_binding_index);

                let opacity = PrimitiveOpacity::from_alpha(opacity_binding);
                let opacity = opacity.combine(prim_data.opacity);

                let non_segmented_blend_mode = if !opacity.is_opaque ||
                    prim_instance.clip_task_index != ClipTaskIndex::INVALID ||
                    transform_kind == TransformedRectKind::Complex
                {
                    specified_blend_mode
                } else {
                    BlendMode::None
                };

                let batch_params = BrushBatchParameters::shared(
                    BrushBatchKind::Solid,
                    BatchTextures::no_texture(),
                    [get_shader_opacity(opacity_binding), 0, 0],
                    0,
                );

                let (prim_cache_address, segments) = if segment_instance_index == SegmentInstanceIndex::UNUSED {
                    (gpu_cache.get_address(&prim_data.gpu_cache_handle), None)
                } else {
                    let segment_instance = &ctx.scratch.segment_instances[segment_instance_index];
                    let segments = Some(&ctx.scratch.segments[segment_instance.segments_range]);
                    (gpu_cache.get_address(&segment_instance.gpu_cache_handle), segments)
                };

                let prim_header = PrimitiveHeader {
                    local_rect: prim_rect,
                    local_clip_rect: prim_instance.combined_local_clip_rect,
                    task_address,
                    specific_prim_address: prim_cache_address,
                    clip_task_address,
                    transform_id,
                };

                let prim_header_index = prim_headers.push(
                    &prim_header,
                    z_id,
                    batch_params.prim_user_data,
                );

                self.add_segmented_prim_to_batch(
                    segments,
                    opacity,
                    &batch_params,
                    specified_blend_mode,
                    non_segmented_blend_mode,
                    prim_header_index,
                    clip_task_address,
                    bounding_rect,
                    transform_kind,
                    render_tasks,
                    z_id,
                    prim_instance.clip_task_index,
                    ctx,
                );
            }
            PrimitiveInstanceKind::YuvImage { data_handle, segment_instance_index, .. } => {
                let prim_data = &ctx.resources.prim_data_store[data_handle];
                let (format, yuv_key, image_rendering, color_depth, color_space) = match prim_data.kind {
                    PrimitiveTemplateKind::YuvImage { ref format, yuv_key, ref image_rendering, ref color_depth, ref color_space, .. } => {
                        (format, yuv_key, image_rendering, color_depth, color_space)
                    }
                    _ => unreachable!()
                };
                let mut textures = BatchTextures::no_texture();
                let mut uv_rect_addresses = [0; 3];

                //yuv channel
                let channel_count = format.get_plane_num();
                debug_assert!(channel_count <= 3);
                for channel in 0 .. channel_count {
                    let image_key = yuv_key[channel];

                    let cache_item = resolve_image(
                        ImageRequest {
                            key: image_key,
                            rendering: *image_rendering,
                            tile: None,
                        },
                        ctx.resource_cache,
                        gpu_cache,
                        deferred_resolves,
                    );

                    if cache_item.texture_id == TextureSource::Invalid {
                        warn!("Warnings: skip a PrimitiveKind::YuvImage");
                        return;
                    }

                    textures.colors[channel] = cache_item.texture_id;
                    uv_rect_addresses[channel] = cache_item.uv_rect_handle.as_int(gpu_cache);
                }

                // All yuv textures should be the same type.
                let buffer_kind = get_buffer_kind(textures.colors[0]);
                assert!(
                    textures.colors[1 .. format.get_plane_num()]
                        .iter()
                        .all(|&tid| buffer_kind == get_buffer_kind(tid))
                );

                let kind = BrushBatchKind::YuvImage(
                    buffer_kind,
                    *format,
                    *color_depth,
                    *color_space,
                );

                let batch_params = BrushBatchParameters::shared(
                    kind,
                    textures,
                    [
                        uv_rect_addresses[0],
                        uv_rect_addresses[1],
                        uv_rect_addresses[2],
                    ],
                    0,
                );

                let specified_blend_mode = BlendMode::PremultipliedAlpha;

                let non_segmented_blend_mode = if !prim_data.opacity.is_opaque ||
                    prim_instance.clip_task_index != ClipTaskIndex::INVALID ||
                    transform_kind == TransformedRectKind::Complex
                {
                    specified_blend_mode
                } else {
                    BlendMode::None
                };

                debug_assert!(segment_instance_index != SegmentInstanceIndex::INVALID);
                let (prim_cache_address, segments) = if segment_instance_index == SegmentInstanceIndex::UNUSED {
                    (gpu_cache.get_address(&prim_data.gpu_cache_handle), None)
                } else {
                    let segment_instance = &ctx.scratch.segment_instances[segment_instance_index];
                    let segments = Some(&ctx.scratch.segments[segment_instance.segments_range]);
                    (gpu_cache.get_address(&segment_instance.gpu_cache_handle), segments)
                };

                let prim_header = PrimitiveHeader {
                    local_rect: prim_rect,
                    local_clip_rect: prim_instance.combined_local_clip_rect,
                    task_address,
                    specific_prim_address: prim_cache_address,
                    clip_task_address,
                    transform_id,
                };

                let prim_header_index = prim_headers.push(
                    &prim_header,
                    z_id,
                    batch_params.prim_user_data,
                );

                self.add_segmented_prim_to_batch(
                    segments,
                    prim_data.opacity,
                    &batch_params,
                    specified_blend_mode,
                    non_segmented_blend_mode,
                    prim_header_index,
                    clip_task_address,
                    bounding_rect,
                    transform_kind,
                    render_tasks,
                    z_id,
                    prim_instance.clip_task_index,
                    ctx,
                );
            }
            PrimitiveInstanceKind::Image { data_handle, image_instance_index, .. } => {
                let prim_data = &ctx.resources.prim_data_store[data_handle];
                let (source, alpha_type, key, image_rendering) = match prim_data.kind {
                    PrimitiveTemplateKind::Image { ref source, alpha_type, key, image_rendering, .. } => {
                        (source, alpha_type, key, image_rendering)
                    }
                    _ => unreachable!()
                };
                let image_instance = &ctx.prim_store.images[image_instance_index];
                let opacity_binding = ctx.prim_store.get_opacity_binding(image_instance.opacity_binding_index);
                let specified_blend_mode = match alpha_type {
                    AlphaType::PremultipliedAlpha => BlendMode::PremultipliedAlpha,
                    AlphaType::Alpha => BlendMode::Alpha,
                };
                let request = ImageRequest {
                    key: key,
                    rendering: image_rendering,
                    tile: None,
                };

                if image_instance.visible_tiles.is_empty() {
                    let cache_item = match *source {
                        ImageSource::Default => {
                            resolve_image(
                                request,
                                ctx.resource_cache,
                                gpu_cache,
                                deferred_resolves,
                            )
                        }
                        ImageSource::Cache { ref handle, .. } => {
                            let rt_handle = handle
                                .as_ref()
                                .expect("bug: render task handle not allocated");
                            let rt_cache_entry = ctx.resource_cache
                                .get_cached_render_task(rt_handle);
                            ctx.resource_cache.get_texture_cache_item(&rt_cache_entry.handle)
                        }
                    };

                    if cache_item.texture_id == TextureSource::Invalid {
                        return;
                    }

                    let textures = BatchTextures::color(cache_item.texture_id);

                    let opacity = PrimitiveOpacity::from_alpha(opacity_binding);
                    let opacity = opacity.combine(prim_data.opacity);

                    let non_segmented_blend_mode = if !opacity.is_opaque ||
                        prim_instance.clip_task_index != ClipTaskIndex::INVALID ||
                        transform_kind == TransformedRectKind::Complex
                    {
                        specified_blend_mode
                    } else {
                        BlendMode::None
                    };

                    let batch_params = BrushBatchParameters::shared(
                        BrushBatchKind::Image(get_buffer_kind(cache_item.texture_id)),
                        textures,
                        [
                            ShaderColorMode::Image as i32 | ((alpha_type as i32) << 16),
                            RasterizationSpace::Local as i32,
                            get_shader_opacity(opacity_binding),
                        ],
                        cache_item.uv_rect_handle.as_int(gpu_cache),
                    );

                    debug_assert!(image_instance.segment_instance_index != SegmentInstanceIndex::INVALID);
                    let (prim_cache_address, segments) = if image_instance.segment_instance_index == SegmentInstanceIndex::UNUSED {
                        (gpu_cache.get_address(&prim_data.gpu_cache_handle), None)
                    } else {
                        let segment_instance = &ctx.scratch.segment_instances[image_instance.segment_instance_index];
                        let segments = Some(&ctx.scratch.segments[segment_instance.segments_range]);
                        (gpu_cache.get_address(&segment_instance.gpu_cache_handle), segments)
                    };

                    let prim_header = PrimitiveHeader {
                        local_rect: prim_rect,
                        local_clip_rect: prim_instance.combined_local_clip_rect,
                        task_address,
                        specific_prim_address: prim_cache_address,
                        clip_task_address,
                        transform_id,
                    };

                    let prim_header_index = prim_headers.push(
                        &prim_header,
                        z_id,
                        batch_params.prim_user_data,
                    );

                    self.add_segmented_prim_to_batch(
                        segments,
                        opacity,
                        &batch_params,
                        specified_blend_mode,
                        non_segmented_blend_mode,
                        prim_header_index,
                        clip_task_address,
                        bounding_rect,
                        transform_kind,
                        render_tasks,
                        z_id,
                        prim_instance.clip_task_index,
                        ctx,
                    );
                } else {
                    for tile in &image_instance.visible_tiles {
                        if let Some((batch_kind, textures, user_data, uv_rect_address)) = get_image_tile_params(
                            ctx.resource_cache,
                            gpu_cache,
                            deferred_resolves,
                            request.with_tile(tile.tile_offset),
                            alpha_type,
                            get_shader_opacity(opacity_binding),
                        ) {
                            let prim_cache_address = gpu_cache.get_address(&tile.handle);
                            let prim_header = PrimitiveHeader {
                                specific_prim_address: prim_cache_address,
                                local_rect: tile.local_rect,
                                local_clip_rect: tile.local_clip_rect,
                                task_address,
                                clip_task_address,
                                transform_id,
                            };
                            let prim_header_index = prim_headers.push(&prim_header, z_id, user_data);

                            self.add_image_tile_to_batch(
                                batch_kind,
                                specified_blend_mode,
                                textures,
                                prim_header_index,
                                clip_task_address,
                                bounding_rect,
                                tile.edge_flags,
                                uv_rect_address,
                                z_id,
                            );
                        }
                    }
                }
            }
            PrimitiveInstanceKind::LinearGradient { data_handle, ref visible_tiles_range, .. } => {
                let prim_data = &ctx.resources.linear_grad_data_store[data_handle];
                let specified_blend_mode = BlendMode::PremultipliedAlpha;

                let mut prim_header = PrimitiveHeader {
                    local_rect: prim_rect,
                    local_clip_rect: prim_instance.combined_local_clip_rect,
                    task_address,
                    specific_prim_address: GpuCacheAddress::invalid(),
                    clip_task_address,
                    transform_id,
                };

                if visible_tiles_range.is_empty() {
                    let non_segmented_blend_mode = if !prim_data.opacity.is_opaque ||
                        prim_instance.clip_task_index != ClipTaskIndex::INVALID ||
                        transform_kind == TransformedRectKind::Complex
                    {
                        specified_blend_mode
                    } else {
                        BlendMode::None
                    };

                    let batch_params = BrushBatchParameters::shared(
                        BrushBatchKind::LinearGradient,
                        BatchTextures::no_texture(),
                        [
                            prim_data.stops_handle.as_int(gpu_cache),
                            0,
                            0,
                        ],
                        0,
                    );

                    prim_header.specific_prim_address = gpu_cache.get_address(&prim_data.gpu_cache_handle);

                    let prim_header_index = prim_headers.push(
                        &prim_header,
                        z_id,
                        batch_params.prim_user_data,
                    );

                    let segments = if prim_data.brush_segments.is_empty() {
                        None
                    } else {
                        Some(prim_data.brush_segments.as_slice())
                    };

                    self.add_segmented_prim_to_batch(
                        segments,
                        prim_data.opacity,
                        &batch_params,
                        specified_blend_mode,
                        non_segmented_blend_mode,
                        prim_header_index,
                        clip_task_address,
                        bounding_rect,
                        transform_kind,
                        render_tasks,
                        z_id,
                        prim_instance.clip_task_index,
                        ctx,
                    );
                } else {
                    let visible_tiles = &ctx.scratch.gradient_tiles[*visible_tiles_range];

                    add_gradient_tiles(
                        visible_tiles,
                        &prim_data.stops_handle,
                        BrushBatchKind::LinearGradient,
                        specified_blend_mode,
                        bounding_rect,
                        clip_task_address,
                        gpu_cache,
                        &mut self.batch_list,
                        &prim_header,
                        prim_headers,
                        z_id,
                    );
                }
            }
            PrimitiveInstanceKind::RadialGradient { data_handle, ref visible_tiles_range, .. } => {
                let prim_data = &ctx.resources.radial_grad_data_store[data_handle];
                let specified_blend_mode = BlendMode::PremultipliedAlpha;

                let mut prim_header = PrimitiveHeader {
                    local_rect: prim_rect,
                    local_clip_rect: prim_instance.combined_local_clip_rect,
                    task_address,
                    specific_prim_address: GpuCacheAddress::invalid(),
                    clip_task_address,
                    transform_id,
                };

                if visible_tiles_range.is_empty() {
                    let non_segmented_blend_mode = if !prim_data.opacity.is_opaque ||
                        prim_instance.clip_task_index != ClipTaskIndex::INVALID ||
                        transform_kind == TransformedRectKind::Complex
                    {
                        specified_blend_mode
                    } else {
                        BlendMode::None
                    };

                    let batch_params = BrushBatchParameters::shared(
                        BrushBatchKind::RadialGradient,
                        BatchTextures::no_texture(),
                        [
                            prim_data.stops_handle.as_int(gpu_cache),
                            0,
                            0,
                        ],
                        0,
                    );

                    prim_header.specific_prim_address = gpu_cache.get_address(&prim_data.gpu_cache_handle);

                    let prim_header_index = prim_headers.push(
                        &prim_header,
                        z_id,
                        batch_params.prim_user_data,
                    );

                    let segments = if prim_data.brush_segments.is_empty() {
                        None
                    } else {
                        Some(prim_data.brush_segments.as_slice())
                    };

                    self.add_segmented_prim_to_batch(
                        segments,
                        prim_data.opacity,
                        &batch_params,
                        specified_blend_mode,
                        non_segmented_blend_mode,
                        prim_header_index,
                        clip_task_address,
                        bounding_rect,
                        transform_kind,
                        render_tasks,
                        z_id,
                        prim_instance.clip_task_index,
                        ctx,
                    );
                } else {
                    let visible_tiles = &ctx.scratch.gradient_tiles[*visible_tiles_range];

                    add_gradient_tiles(
                        visible_tiles,
                        &prim_data.stops_handle,
                        BrushBatchKind::RadialGradient,
                        specified_blend_mode,
                        bounding_rect,
                        clip_task_address,
                        gpu_cache,
                        &mut self.batch_list,
                        &prim_header,
                        prim_headers,
                        z_id,
                    );
                }
            }
        }
    }

    fn add_image_tile_to_batch(
        &mut self,
        batch_kind: BrushBatchKind,
        blend_mode: BlendMode,
        textures: BatchTextures,
        prim_header_index: PrimitiveHeaderIndex,
        clip_task_address: RenderTaskAddress,
        bounding_rect: &PictureRect,
        edge_flags: EdgeAaSegmentMask,
        uv_rect_address: GpuCacheAddress,
        z_id: ZBufferId,
    ) {
        let base_instance = BrushInstance {
            prim_header_index,
            clip_task_address,
            segment_index: INVALID_SEGMENT_INDEX,
            edge_flags,
            brush_flags: BrushFlags::PERSPECTIVE_INTERPOLATION,
            user_data: uv_rect_address.as_int(),
        };

        let batch_key = BatchKey {
            blend_mode,
            kind: BatchKind::Brush(batch_kind),
            textures,
        };
        self.batch_list.push_single_instance(
            batch_key,
            bounding_rect,
            z_id,
            PrimitiveInstanceData::from(base_instance),
        );
    }

    /// Add a single segment instance to a batch.
    fn add_segment_to_batch(
        &mut self,
        segment: &BrushSegment,
        segment_data: &SegmentInstanceData,
        segment_index: i32,
        batch_kind: BrushBatchKind,
        prim_header_index: PrimitiveHeaderIndex,
        alpha_blend_mode: BlendMode,
        bounding_rect: &PictureRect,
        transform_kind: TransformedRectKind,
        render_tasks: &RenderTaskTree,
        z_id: ZBufferId,
        prim_opacity: PrimitiveOpacity,
        clip_task_index: ClipTaskIndex,
        ctx: &RenderTargetContext,
    ) {
        debug_assert!(clip_task_index != ClipTaskIndex::INVALID);

        // Get GPU address of clip task for this segment, or None if
        // the entire segment is clipped out.
        let clip_task_address = match get_clip_task_address(
            &ctx.scratch.clip_mask_instances,
            clip_task_index,
            segment_index,
            render_tasks,
        ) {
            Some(clip_task_address) => clip_task_address,
            None => return,
        };

        // If a got a valid (or OPAQUE) clip task address, add the segment.
        let is_inner = segment.edge_flags.is_empty();
        let needs_blending = !prim_opacity.is_opaque ||
                             clip_task_address != OPAQUE_TASK_ADDRESS ||
                             (!is_inner && transform_kind == TransformedRectKind::Complex);

        let instance = PrimitiveInstanceData::from(BrushInstance {
            segment_index,
            edge_flags: segment.edge_flags,
            clip_task_address,
            brush_flags: BrushFlags::PERSPECTIVE_INTERPOLATION | segment.brush_flags,
            prim_header_index,
            user_data: segment_data.user_data,
        });

        let batch_key = BatchKey {
            blend_mode: if needs_blending { alpha_blend_mode } else { BlendMode::None },
            kind: BatchKind::Brush(batch_kind),
            textures: segment_data.textures,
        };

        self.batch_list.push_single_instance(
            batch_key,
            bounding_rect,
            z_id,
            instance,
        );
    }

    /// Add any segment(s) from a brush to batches.
    fn add_segmented_prim_to_batch(
        &mut self,
        brush_segments: Option<&[BrushSegment]>,
        prim_opacity: PrimitiveOpacity,
        params: &BrushBatchParameters,
        alpha_blend_mode: BlendMode,
        non_segmented_blend_mode: BlendMode,
        prim_header_index: PrimitiveHeaderIndex,
        clip_task_address: RenderTaskAddress,
        bounding_rect: &PictureRect,
        transform_kind: TransformedRectKind,
        render_tasks: &RenderTaskTree,
        z_id: ZBufferId,
        clip_task_index: ClipTaskIndex,
        ctx: &RenderTargetContext,
    ) {
        match (brush_segments, &params.segment_data) {
            (Some(ref brush_segments), SegmentDataKind::Instanced(ref segment_data)) => {
                // In this case, we have both a list of segments, and a list of
                // per-segment instance data. Zip them together to build batches.
                debug_assert_eq!(brush_segments.len(), segment_data.len());
                for (segment_index, (segment, segment_data)) in brush_segments
                    .iter()
                    .zip(segment_data.iter())
                    .enumerate()
                {
                    self.add_segment_to_batch(
                        segment,
                        segment_data,
                        segment_index as i32,
                        params.batch_kind,
                        prim_header_index,
                        alpha_blend_mode,
                        bounding_rect,
                        transform_kind,
                        render_tasks,
                        z_id,
                        prim_opacity,
                        clip_task_index,
                        ctx,
                    );
                }
            }
            (Some(ref brush_segments), SegmentDataKind::Shared(ref segment_data)) => {
                // A list of segments, but the per-segment data is common
                // between all segments.
                for (segment_index, segment) in brush_segments
                    .iter()
                    .enumerate()
                {
                    self.add_segment_to_batch(
                        segment,
                        segment_data,
                        segment_index as i32,
                        params.batch_kind,
                        prim_header_index,
                        alpha_blend_mode,
                        bounding_rect,
                        transform_kind,
                        render_tasks,
                        z_id,
                        prim_opacity,
                        clip_task_index,
                        ctx,
                    );
                }
            }
            (None, SegmentDataKind::Shared(ref segment_data)) => {
                // No segments, and thus no per-segment instance data.
                // Note: the blend mode already takes opacity into account
                let batch_key = BatchKey {
                    blend_mode: non_segmented_blend_mode,
                    kind: BatchKind::Brush(params.batch_kind),
                    textures: segment_data.textures,
                };
                let instance = PrimitiveInstanceData::from(BrushInstance {
                    segment_index: INVALID_SEGMENT_INDEX,
                    edge_flags: EdgeAaSegmentMask::all(),
                    clip_task_address,
                    brush_flags: BrushFlags::PERSPECTIVE_INTERPOLATION,
                    prim_header_index,
                    user_data: segment_data.user_data,
                });
                self.batch_list.push_single_instance(
                    batch_key,
                    bounding_rect,
                    z_id,
                    PrimitiveInstanceData::from(instance),
                );
            }
            (None, SegmentDataKind::Instanced(..)) => {
                // We should never hit the case where there are no segments,
                // but a list of segment instance data.
                unreachable!();
            }
        }
    }
}

fn add_gradient_tiles(
    visible_tiles: &[VisibleGradientTile],
    stops_handle: &GpuCacheHandle,
    kind: BrushBatchKind,
    blend_mode: BlendMode,
    bounding_rect: &PictureRect,
    clip_task_address: RenderTaskAddress,
    gpu_cache: &GpuCache,
    batch_list: &mut BatchList,
    base_prim_header: &PrimitiveHeader,
    prim_headers: &mut PrimitiveHeaders,
    z_id: ZBufferId,
) {
    let batch = batch_list.set_params_and_get_batch(
        BatchKey {
            blend_mode: blend_mode,
            kind: BatchKind::Brush(kind),
            textures: BatchTextures::no_texture(),
        },
        bounding_rect,
        z_id,
    );

    let user_data = [stops_handle.as_int(gpu_cache), 0, 0];

    for tile in visible_tiles {
        let prim_header = PrimitiveHeader {
            specific_prim_address: gpu_cache.get_address(&tile.handle),
            local_rect: tile.local_rect,
            local_clip_rect: tile.local_clip_rect,
            ..*base_prim_header
        };
        let prim_header_index = prim_headers.push(&prim_header, z_id, user_data);

        batch.push(PrimitiveInstanceData::from(
            BrushInstance {
                prim_header_index,
                clip_task_address,
                segment_index: INVALID_SEGMENT_INDEX,
                edge_flags: EdgeAaSegmentMask::all(),
                brush_flags: BrushFlags::PERSPECTIVE_INTERPOLATION,
                user_data: 0,
            }
        ));
    }
}

fn get_image_tile_params(
    resource_cache: &ResourceCache,
    gpu_cache: &mut GpuCache,
    deferred_resolves: &mut Vec<DeferredResolve>,
    request: ImageRequest,
    alpha_type: AlphaType,
    shader_opacity: i32,
) -> Option<(BrushBatchKind, BatchTextures, [i32; 3], GpuCacheAddress)> {

    let cache_item = resolve_image(
        request,
        resource_cache,
        gpu_cache,
        deferred_resolves,
    );

    if cache_item.texture_id == TextureSource::Invalid {
        None
    } else {
        let textures = BatchTextures::color(cache_item.texture_id);
        Some((
            BrushBatchKind::Image(get_buffer_kind(cache_item.texture_id)),
            textures,
            [
                ShaderColorMode::Image as i32 | ((alpha_type as i32) << 16),
                RasterizationSpace::Local as i32,
                shader_opacity,
            ],
            gpu_cache.get_address(&cache_item.uv_rect_handle),
        ))
    }
}

/// Either a single texture / user data for all segments,
/// or a list of one per segment.
enum SegmentDataKind {
    Shared(SegmentInstanceData),
    Instanced(SmallVec<[SegmentInstanceData; 8]>),
}

/// The parameters that are specific to a kind of brush,
/// used by the common method to add a brush to batches.
struct BrushBatchParameters {
    batch_kind: BrushBatchKind,
    prim_user_data: [i32; 3],
    segment_data: SegmentDataKind,
}

impl BrushBatchParameters {
    /// This brush instance has a list of per-segment
    /// instance data.
    fn instanced(
        batch_kind: BrushBatchKind,
        prim_user_data: [i32; 3],
        segment_data: SmallVec<[SegmentInstanceData; 8]>,
    ) -> Self {
        BrushBatchParameters {
            batch_kind,
            prim_user_data,
            segment_data: SegmentDataKind::Instanced(segment_data),
        }
    }

    /// This brush instance shares the per-segment data
    /// across all segments.
    fn shared(
        batch_kind: BrushBatchKind,
        textures: BatchTextures,
        prim_user_data: [i32; 3],
        segment_user_data: i32,
    ) -> Self {
        BrushBatchParameters {
            batch_kind,
            prim_user_data,
            segment_data: SegmentDataKind::Shared(
                SegmentInstanceData {
                    textures,
                    user_data: segment_user_data,
                }
            ),
        }
    }
}

impl PrimitiveInstance {
    pub fn is_cacheable(
        &self,
        prim_data_store: &PrimitiveDataStore,
        resource_cache: &ResourceCache,
    ) -> bool {
        let image_key = match self.kind {
            PrimitiveInstanceKind::Image { data_handle, .. } |
            PrimitiveInstanceKind::YuvImage { data_handle, .. } => {
                let prim_data = &prim_data_store[data_handle];
                match prim_data.kind {
                    PrimitiveTemplateKind::YuvImage { ref yuv_key, .. } => {
                        yuv_key[0]
                    }
                    PrimitiveTemplateKind::Image { key, .. } => {
                        key
                    }
                    _ => unreachable!(),
                }
            }
            PrimitiveInstanceKind::Picture { .. } |
            PrimitiveInstanceKind::TextRun { .. } |
            PrimitiveInstanceKind::LineDecoration { .. } |
            PrimitiveInstanceKind::NormalBorder { .. } |
            PrimitiveInstanceKind::ImageBorder { .. } |
            PrimitiveInstanceKind::Rectangle { .. } |
            PrimitiveInstanceKind::LinearGradient { .. } |
            PrimitiveInstanceKind::RadialGradient { .. } |
            PrimitiveInstanceKind::Clear { .. } => {
                return true;
            }
        };
        match resource_cache.get_image_properties(image_key) {
            Some(ImageProperties { external_image: Some(_), .. }) => {
                false
            }
            _ => true
        }
    }
}

impl PictureSurface {
    // Retrieve the uv rect handle, and texture for a picture surface.
    fn resolve(
        &self,
        render_tasks: &RenderTaskTree,
        resource_cache: &ResourceCache,
        gpu_cache: &GpuCache,
    ) -> (GpuCacheAddress, BatchTextures) {
        match *self {
            PictureSurface::TextureCache(ref handle) => {
                let rt_cache_entry = resource_cache
                    .get_cached_render_task(handle);
                let cache_item = resource_cache
                    .get_texture_cache_item(&rt_cache_entry.handle);

                (
                    gpu_cache.get_address(&cache_item.uv_rect_handle),
                    BatchTextures::color(cache_item.texture_id),
                )
            }
            PictureSurface::RenderTask(task_id) => {
                (
                    render_tasks[task_id].get_texture_address(gpu_cache),
                    BatchTextures::render_target_cache(),
                )
            }
        }
    }

    // Retrieve the render task id for a picture surface. Should only
    // be used where it's known that this picture surface will never
    // be persisted in the texture cache.
    fn resolve_render_task_id(&self) -> RenderTaskId {
        match *self {
            PictureSurface::TextureCache(..) => {
                panic!("BUG: unexpectedly cached render task");
            }
            PictureSurface::RenderTask(task_id) => {
                task_id
            }
        }
    }
}

pub fn resolve_image(
    request: ImageRequest,
    resource_cache: &ResourceCache,
    gpu_cache: &mut GpuCache,
    deferred_resolves: &mut Vec<DeferredResolve>,
) -> CacheItem {
    match resource_cache.get_image_properties(request.key) {
        Some(image_properties) => {
            // Check if an external image that needs to be resolved
            // by the render thread.
            match image_properties.external_image {
                Some(external_image) => {
                    // This is an external texture - we will add it to
                    // the deferred resolves list to be patched by
                    // the render thread...
                    let cache_handle = gpu_cache.push_deferred_per_frame_blocks(BLOCKS_PER_UV_RECT);
                    let cache_item = CacheItem {
                        texture_id: TextureSource::External(external_image),
                        uv_rect_handle: cache_handle,
                        uv_rect: DeviceIntRect::new(
                            DeviceIntPoint::zero(),
                            image_properties.descriptor.size,
                        ),
                        texture_layer: 0,
                    };

                    deferred_resolves.push(DeferredResolve {
                        image_properties,
                        address: gpu_cache.get_address(&cache_handle),
                        rendering: request.rendering,
                    });

                    cache_item
                }
                None => {
                    if let Ok(cache_item) = resource_cache.get_cached_image(request) {
                        cache_item
                    } else {
                        // There is no usable texture entry for the image key. Just return an invalid texture here.
                        CacheItem::invalid()
                    }
                }
            }
        }
        None => {
            CacheItem::invalid()
        }
    }
}


/// Batcher managing draw calls into the clip mask (in the RT cache).
#[derive(Debug)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct ClipBatcher {
    /// Rectangle draws fill up the rectangles with rounded corners.
    pub rectangles: Vec<ClipMaskInstance>,
    /// Image draws apply the image masking.
    pub images: FastHashMap<TextureSource, Vec<ClipMaskInstance>>,
    pub box_shadows: FastHashMap<TextureSource, Vec<ClipMaskInstance>>,
}

impl ClipBatcher {
    pub fn new() -> Self {
        ClipBatcher {
            rectangles: Vec::new(),
            images: FastHashMap::default(),
            box_shadows: FastHashMap::default(),
        }
    }

    pub fn add_clip_region(
        &mut self,
        task_address: RenderTaskAddress,
        clip_data_address: GpuCacheAddress,
        local_pos: LayoutPoint,
    ) {
        let instance = ClipMaskInstance {
            render_task_address: task_address,
            clip_transform_id: TransformPaletteId::IDENTITY,
            prim_transform_id: TransformPaletteId::IDENTITY,
            segment: 0,
            clip_data_address,
            resource_address: GpuCacheAddress::invalid(),
            local_pos,
            tile_rect: LayoutRect::zero(),
        };

        self.rectangles.push(instance);
    }

    pub fn add(
        &mut self,
        task_address: RenderTaskAddress,
        clip_node_range: ClipNodeRange,
        root_spatial_node_index: SpatialNodeIndex,
        resource_cache: &ResourceCache,
        gpu_cache: &GpuCache,
        clip_store: &ClipStore,
        clip_scroll_tree: &ClipScrollTree,
        transforms: &mut TransformPalette,
        clip_data_store: &ClipDataStore,
    ) {
        for i in 0 .. clip_node_range.count {
            let clip_instance = clip_store.get_instance_from_range(&clip_node_range, i);
            let clip_node = &clip_data_store[clip_instance.handle];

            let clip_transform_id = transforms.get_id(
                clip_instance.spatial_node_index,
                ROOT_SPATIAL_NODE_INDEX,
                clip_scroll_tree,
            );

            let prim_transform_id = transforms.get_id(
                root_spatial_node_index,
                ROOT_SPATIAL_NODE_INDEX,
                clip_scroll_tree,
            );

            let instance = ClipMaskInstance {
                render_task_address: task_address,
                clip_transform_id,
                prim_transform_id,
                segment: 0,
                clip_data_address: GpuCacheAddress::invalid(),
                resource_address: GpuCacheAddress::invalid(),
                local_pos: clip_instance.local_pos,
                tile_rect: LayoutRect::zero(),
            };

            match clip_node.item {
                ClipItem::Image { image, size, .. } => {
                    let request = ImageRequest {
                        key: image,
                        rendering: ImageRendering::Auto,
                        tile: None,
                    };

                    let clip_data_address =
                        gpu_cache.get_address(&clip_node.gpu_cache_handle);

                    let mut add_image = |request: ImageRequest, local_tile_rect: LayoutRect| {
                        let cache_item = match resource_cache.get_cached_image(request) {
                            Ok(item) => item,
                            Err(..) => {
                                warn!("Warnings: skip a image mask");
                                debug!("request: {:?}", request);
                                return;
                            }
                        };
                        self.images
                            .entry(cache_item.texture_id)
                            .or_insert(Vec::new())
                            .push(ClipMaskInstance {
                                clip_data_address,
                                resource_address: gpu_cache.get_address(&cache_item.uv_rect_handle),
                                tile_rect: local_tile_rect,
                                ..instance
                            });
                    };

                    match clip_instance.visible_tiles {
                        Some(ref tiles) => {
                            for tile in tiles {
                                add_image(
                                    request.with_tile(tile.tile_offset),
                                    tile.tile_rect,
                                )
                            }
                        }
                        None => {
                            let mask_rect = LayoutRect::new(clip_instance.local_pos, size);
                            add_image(request, mask_rect)
                        }
                    }
                }
                ClipItem::BoxShadow(ref info) => {
                    let gpu_address =
                        gpu_cache.get_address(&clip_node.gpu_cache_handle);
                    let rt_handle = info
                        .cache_handle
                        .as_ref()
                        .expect("bug: render task handle not allocated");
                    let rt_cache_entry = resource_cache
                        .get_cached_render_task(rt_handle);
                    let cache_item = resource_cache
                        .get_texture_cache_item(&rt_cache_entry.handle);
                    debug_assert_ne!(cache_item.texture_id, TextureSource::Invalid);

                    self.box_shadows
                        .entry(cache_item.texture_id)
                        .or_insert(Vec::new())
                        .push(ClipMaskInstance {
                            clip_data_address: gpu_address,
                            resource_address: gpu_cache.get_address(&cache_item.uv_rect_handle),
                            ..instance
                        });
                }
                ClipItem::Rectangle(_, mode) => {
                    if !clip_instance.flags.contains(ClipNodeFlags::SAME_COORD_SYSTEM) ||
                        mode == ClipMode::ClipOut {
                        let gpu_address =
                            gpu_cache.get_address(&clip_node.gpu_cache_handle);
                        self.rectangles.push(ClipMaskInstance {
                            clip_data_address: gpu_address,
                            ..instance
                        });
                    }
                }
                ClipItem::RoundedRectangle(..) => {
                    let gpu_address =
                        gpu_cache.get_address(&clip_node.gpu_cache_handle);
                    self.rectangles.push(ClipMaskInstance {
                        clip_data_address: gpu_address,
                        ..instance
                    });
                }
            }
        }
    }
}

fn get_buffer_kind(texture: TextureSource) -> ImageBufferKind {
    match texture {
        TextureSource::External(ext_image) => {
            match ext_image.image_type {
                ExternalImageType::TextureHandle(target) => {
                    target.into()
                }
                ExternalImageType::Buffer => {
                    // The ExternalImageType::Buffer should be handled by resource_cache.
                    // It should go through the non-external case.
                    panic!("Unexpected non-texture handle type");
                }
            }
        }
        _ => ImageBufferKind::Texture2DArray,
    }
}

fn get_shader_opacity(opacity: f32) -> i32 {
    (opacity * 65535.0).round() as i32
}

/// Retrieve the GPU task address for a given clip task instance.
/// Returns None if the segment was completely clipped out.
/// Returns Some(OPAQUE_TASK_ADDRESS) if no clip mask is needed.
/// Returns Some(task_address) if there was a valid clip mask.
fn get_clip_task_address(
    clip_mask_instances: &[ClipMaskKind],
    clip_task_index: ClipTaskIndex,
    offset: i32,
    render_tasks: &RenderTaskTree,
) -> Option<RenderTaskAddress> {
    let address = match clip_mask_instances[clip_task_index.0 as usize + offset as usize] {
        ClipMaskKind::Mask(task_id) => {
            render_tasks.get_task_address(task_id)
        }
        ClipMaskKind::None => {
            OPAQUE_TASK_ADDRESS
        }
        ClipMaskKind::Clipped => {
            return None;
        }
    };

    Some(address)
}
