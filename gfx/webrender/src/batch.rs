/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{AlphaType, DeviceIntRect, DeviceIntSize, ImageKey, LayerToWorldScale};
use api::{ExternalImageType, FilterOp, ImageRendering, LayerRect};
use api::{SubpixelDirection, TileOffset, YuvColorSpace, YuvFormat};
use api::{LayerToWorldTransform, WorldPixel};
use border::{BorderCornerInstance, BorderCornerSide, BorderEdgeKind};
use clip::{ClipSource, ClipStore};
use clip_scroll_tree::{CoordinateSystemId};
use euclid::{TypedTransform3D, vec3};
use glyph_rasterizer::GlyphFormat;
use gpu_cache::{GpuCache, GpuCacheAddress, GpuCacheHandle};
use gpu_types::{BrushImageKind, BrushInstance, ClipChainRectIndex};
use gpu_types::{ClipMaskInstance, ClipScrollNodeIndex, PictureType};
use gpu_types::{CompositePrimitiveInstance, PrimitiveInstance, SimplePrimitiveInstance};
use internal_types::{FastHashMap, SourceTexture};
use picture::{PictureCompositeMode, PictureKind, PicturePrimitive, PictureSurface};
use plane_split::{BspSplitter, Polygon, Splitter};
use prim_store::{PrimitiveIndex, PrimitiveKind, PrimitiveMetadata, PrimitiveStore};
use prim_store::{BrushPrimitive, BrushKind, DeferredResolve, EdgeAaSegmentMask, PrimitiveRun};
use render_task::{ClipWorkItem};
use render_task::{RenderTaskAddress, RenderTaskId, RenderTaskKind};
use render_task::{RenderTaskTree};
use renderer::{BlendMode, ImageBufferKind};
use renderer::BLOCKS_PER_UV_RECT;
use resource_cache::{GlyphFetchResult, ResourceCache};
use std::{usize, f32, i32};
use tiling::{RenderTargetContext, RenderTargetKind};
use util::{MatrixHelpers, TransformedRectKind};

// Special sentinel value recognized by the shader. It is considered to be
// a dummy task that doesn't mask out anything.
const OPAQUE_TASK_ADDRESS: RenderTaskAddress = RenderTaskAddress(i32::MAX as u32);

#[derive(Copy, Clone, PartialEq, Eq, Hash, Debug)]
#[cfg_attr(feature = "capture", derive(Deserialize, Serialize))]
pub enum TransformBatchKind {
    TextRun(GlyphFormat),
    Image(ImageBufferKind),
    YuvImage(ImageBufferKind, YuvFormat, YuvColorSpace),
    AlignedGradient,
    AngleGradient,
    RadialGradient,
    BorderCorner,
    BorderEdge,
}

#[derive(Copy, Clone, PartialEq, Eq, Hash, Debug)]
#[cfg_attr(feature = "capture", derive(Deserialize, Serialize))]
pub enum BrushImageSourceKind {
    Alpha,
    Color,
    ColorAlphaMask,
}

impl BrushImageSourceKind {
    pub fn from_render_target_kind(render_target_kind: RenderTargetKind) -> BrushImageSourceKind {
        match render_target_kind {
            RenderTargetKind::Color => BrushImageSourceKind::Color,
            RenderTargetKind::Alpha => BrushImageSourceKind::Alpha,
        }
    }
}

#[derive(Copy, Clone, PartialEq, Eq, Hash, Debug)]
#[cfg_attr(feature = "capture", derive(Deserialize, Serialize))]
pub enum BrushBatchKind {
    Picture(BrushImageSourceKind),
    Solid,
    Line,
}

#[derive(Copy, Clone, PartialEq, Eq, Hash, Debug)]
#[cfg_attr(feature = "capture", derive(Deserialize, Serialize))]
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

/// Optional textures that can be used as a source in the shaders.
/// Textures that are not used by the batch are equal to TextureId::invalid().
#[derive(Copy, Clone, Debug)]
#[cfg_attr(feature = "capture", derive(Deserialize, Serialize))]
pub struct BatchTextures {
    pub colors: [SourceTexture; 3],
}

impl BatchTextures {
    pub fn no_texture() -> Self {
        BatchTextures {
            colors: [SourceTexture::Invalid; 3],
        }
    }

    pub fn render_target_cache() -> Self {
        BatchTextures {
            colors: [
                SourceTexture::CacheRGBA8,
                SourceTexture::CacheA8,
                SourceTexture::Invalid,
            ],
        }
    }

    pub fn color(texture: SourceTexture) -> Self {
        BatchTextures {
            colors: [texture, texture, SourceTexture::Invalid],
        }
    }
}

#[derive(Debug)]
#[cfg_attr(feature = "capture", derive(Deserialize, Serialize))]
pub struct AlphaPrimitiveBatch {
    pub key: BatchKey,
    pub instances: Vec<PrimitiveInstance>,
    pub item_rects: Vec<DeviceIntRect>,
}

impl AlphaPrimitiveBatch {
    pub fn new(key: BatchKey) -> AlphaPrimitiveBatch {
        AlphaPrimitiveBatch {
            key,
            instances: Vec::new(),
            item_rects: Vec::new(),
        }
    }
}

#[derive(Debug)]
#[cfg_attr(feature = "capture", derive(Deserialize, Serialize))]
pub struct OpaquePrimitiveBatch {
    pub key: BatchKey,
    pub instances: Vec<PrimitiveInstance>,
}

impl OpaquePrimitiveBatch {
    pub fn new(key: BatchKey) -> OpaquePrimitiveBatch {
        OpaquePrimitiveBatch {
            key,
            instances: Vec::new(),
        }
    }
}

#[derive(Copy, Clone, Debug)]
#[cfg_attr(feature = "capture", derive(Deserialize, Serialize))]
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
fn textures_compatible(t1: SourceTexture, t2: SourceTexture) -> bool {
    t1 == SourceTexture::Invalid || t2 == SourceTexture::Invalid || t1 == t2
}

#[cfg_attr(feature = "capture", derive(Deserialize, Serialize))]
pub struct AlphaBatchList {
    pub batches: Vec<AlphaPrimitiveBatch>,
}

impl AlphaBatchList {
    fn new() -> Self {
        AlphaBatchList {
            batches: Vec::new(),
        }
    }

    pub fn get_suitable_batch(
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
            (BatchKind::Transformable(_, TransformBatchKind::TextRun(_)), BlendMode::SubpixelVariableTextColor) => {
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

#[cfg_attr(feature = "capture", derive(Deserialize, Serialize))]
pub struct OpaqueBatchList {
    pub pixel_area_threshold_for_new_batch: i32,
    pub batches: Vec<OpaquePrimitiveBatch>,
}

impl OpaqueBatchList {
    fn new(pixel_area_threshold_for_new_batch: i32) -> Self {
        OpaqueBatchList {
            batches: Vec::new(),
            pixel_area_threshold_for_new_batch,
        }
    }

    pub fn get_suitable_batch(
        &mut self,
        key: BatchKey,
        item_bounding_rect: &DeviceIntRect
    ) -> &mut Vec<PrimitiveInstance> {
        let mut selected_batch_index = None;
        let item_area = item_bounding_rect.size.area();

        // If the area of this primitive is larger than the given threshold,
        // then it is large enough to warrant breaking a batch for. In this
        // case we just see if it can be added to the existing batch or
        // create a new one.
        if item_area > self.pixel_area_threshold_for_new_batch {
            if let Some(ref batch) = self.batches.last() {
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

#[cfg_attr(feature = "capture", derive(Deserialize, Serialize))]
pub struct BatchList {
    pub alpha_batch_list: AlphaBatchList,
    pub opaque_batch_list: OpaqueBatchList,
}

impl BatchList {
    pub fn new(screen_size: DeviceIntSize) -> Self {
        // The threshold for creating a new batch is
        // one quarter the screen size.
        let batch_area_threshold = screen_size.width * screen_size.height / 4;

        BatchList {
            alpha_batch_list: AlphaBatchList::new(),
            opaque_batch_list: OpaqueBatchList::new(batch_area_threshold),
        }
    }

    pub fn get_suitable_batch(
        &mut self,
        key: BatchKey,
        item_bounding_rect: &DeviceIntRect,
    ) -> &mut Vec<PrimitiveInstance> {
        match key.blend_mode {
            BlendMode::None => {
                self.opaque_batch_list
                    .get_suitable_batch(key, item_bounding_rect)
            }
            BlendMode::Alpha |
            BlendMode::PremultipliedAlpha |
            BlendMode::PremultipliedDestOut |
            BlendMode::SubpixelConstantTextColor(..) |
            BlendMode::SubpixelVariableTextColor |
            BlendMode::SubpixelWithBgColor |
            BlendMode::SubpixelDualSource => {
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
#[cfg_attr(feature = "capture", derive(Deserialize, Serialize))]
pub struct AlphaBatcher {
    pub batch_list: BatchList,
    pub text_run_cache_prims: FastHashMap<SourceTexture, Vec<PrimitiveInstance>>,
    glyph_fetch_buffer: Vec<GlyphFetchResult>,
}

impl AlphaBatcher {
    pub fn new(screen_size: DeviceIntSize) -> Self {
        AlphaBatcher {
            batch_list: BatchList::new(screen_size),
            glyph_fetch_buffer: Vec::new(),
            text_run_cache_prims: FastHashMap::default(),
        }
    }

    pub fn build(
        &mut self,
        tasks: &[RenderTaskId],
        ctx: &RenderTargetContext,
        gpu_cache: &mut GpuCache,
        render_tasks: &RenderTaskTree,
        deferred_resolves: &mut Vec<DeferredResolve>,
    ) {
        for &task_id in tasks {
            match render_tasks[task_id].kind {
                RenderTaskKind::Picture(ref pic_task) => {
                    let pic_index = ctx.prim_store.cpu_metadata[pic_task.prim_index.0].cpu_prim_index;
                    let pic = &ctx.prim_store.cpu_pictures[pic_index.0];
                    self.add_pic_to_batch(
                        pic,
                        task_id,
                        ctx,
                        gpu_cache,
                        render_tasks,
                        deferred_resolves,
                    );
                }
                _ => {
                    unreachable!();
                }
            }
        }

        self.batch_list.finalize();
    }

    pub fn is_empty(&self) -> bool {
        self.batch_list.opaque_batch_list.batches.is_empty() &&
            self.batch_list.alpha_batch_list.batches.is_empty()
    }

    fn add_pic_to_batch(
        &mut self,
        pic: &PicturePrimitive,
        task_id: RenderTaskId,
        ctx: &RenderTargetContext,
        gpu_cache: &mut GpuCache,
        render_tasks: &RenderTaskTree,
        deferred_resolves: &mut Vec<DeferredResolve>,
    ) {
        let task_address = render_tasks.get_task_address(task_id);

        // Even though most of the time a splitter isn't used or needed,
        // they are cheap to construct so we will always pass one down.
        let mut splitter = BspSplitter::new();

        // Add each run in this picture to the batch.
        for run in &pic.runs {
            let scroll_node = &ctx.clip_scroll_tree.nodes[&run.clip_and_scroll.scroll_node_id];
            let scroll_id = scroll_node.node_data_index;
            self.add_run_to_batch(
                run,
                scroll_id,
                ctx,
                gpu_cache,
                render_tasks,
                task_id,
                task_address,
                deferred_resolves,
                &mut splitter,
                pic.picture_type(),
            );
        }

        // Flush the accumulated plane splits onto the task tree.
        // Z axis is directed at the screen, `sort` is ascending, and we need back-to-front order.
        for poly in splitter.sort(vec3(0.0, 0.0, 1.0)) {
            let prim_index = PrimitiveIndex(poly.anchor);
            debug!("process sorted poly {:?} {:?}", prim_index, poly.points);
            let pp = &poly.points;
            let gpu_blocks = [
                [pp[0].x as f32, pp[0].y as f32, pp[0].z as f32, pp[1].x as f32].into(),
                [pp[1].y as f32, pp[1].z as f32, pp[2].x as f32, pp[2].y as f32].into(),
                [pp[2].z as f32, pp[3].x as f32, pp[3].y as f32, pp[3].z as f32].into(),
            ];
            let gpu_handle = gpu_cache.push_per_frame_blocks(&gpu_blocks);
            let key = BatchKey::new(
                BatchKind::SplitComposite,
                BlendMode::PremultipliedAlpha,
                BatchTextures::no_texture(),
            );
            let pic_metadata = &ctx.prim_store.cpu_metadata[prim_index.0];
            let pic = &ctx.prim_store.cpu_pictures[pic_metadata.cpu_prim_index.0];
            let batch = self.batch_list.get_suitable_batch(key, pic_metadata.screen_rect.as_ref().expect("bug"));

            let render_task_id = match pic.surface {
                Some(PictureSurface::RenderTask(render_task_id)) => render_task_id,
                Some(PictureSurface::TextureCache(..)) | None => panic!("BUG: unexpected surface in splitting"),
            };
            let source_task_address = render_tasks.get_task_address(render_task_id);
            let gpu_address = gpu_handle.as_int(gpu_cache);

            let instance = CompositePrimitiveInstance::new(
                task_address,
                source_task_address,
                RenderTaskAddress(0),
                gpu_address,
                0,
                prim_index.0 as i32,
                0,
                0,
            );

            batch.push(PrimitiveInstance::from(instance));
        }
    }

    // Helper to add an entire primitive run to a batch list.
    // TODO(gw): Restructure this so the param list isn't quite
    //           so daunting!
    fn add_run_to_batch(
        &mut self,
        run: &PrimitiveRun,
        scroll_id: ClipScrollNodeIndex,
        ctx: &RenderTargetContext,
        gpu_cache: &mut GpuCache,
        render_tasks: &RenderTaskTree,
        task_id: RenderTaskId,
        task_address: RenderTaskAddress,
        deferred_resolves: &mut Vec<DeferredResolve>,
        splitter: &mut BspSplitter<f64, WorldPixel>,
        pic_type: PictureType,
    ) {
        for i in 0 .. run.count {
            let prim_index = PrimitiveIndex(run.base_prim_index.0 + i);

            let metadata = &ctx.prim_store.cpu_metadata[prim_index.0];

            // Now that we walk the primitive runs in order to add
            // items to batches, we need to check if they are
            // visible here.
            // We currently only support culling on normal (Image)
            // picture types.
            // TODO(gw): Support culling on shadow image types.
            if pic_type != PictureType::Image || metadata.screen_rect.is_some() {
                self.add_prim_to_batch(
                    metadata.clip_chain_rect_index,
                    scroll_id,
                    prim_index,
                    ctx,
                    gpu_cache,
                    render_tasks,
                    task_id,
                    task_address,
                    deferred_resolves,
                    splitter,
                    pic_type,
                );
            }
        }
    }

    // Adds a primitive to a batch.
    // It can recursively call itself in some situations, for
    // example if it encounters a picture where the items
    // in that picture are being drawn into the same target.
    fn add_prim_to_batch(
        &mut self,
        clip_chain_rect_index: ClipChainRectIndex,
        scroll_id: ClipScrollNodeIndex,
        prim_index: PrimitiveIndex,
        ctx: &RenderTargetContext,
        gpu_cache: &mut GpuCache,
        render_tasks: &RenderTaskTree,
        task_id: RenderTaskId,
        task_address: RenderTaskAddress,
        deferred_resolves: &mut Vec<DeferredResolve>,
        splitter: &mut BspSplitter<f64, WorldPixel>,
        pic_type: PictureType,
    ) {
        let z = prim_index.0 as i32;
        let prim_metadata = ctx.prim_store.get_metadata(prim_index);
        let scroll_node = &ctx.node_data[scroll_id.0 as usize];
        // TODO(gw): Calculating this for every primitive is a bit
        //           wasteful. We should probably cache this in
        //           the scroll node...
        let transform_kind = scroll_node.transform.transform_kind();
        let item_bounding_rect = &match prim_metadata.screen_rect {
            Some(screen_rect) => screen_rect,
            None => {
                debug_assert_ne!(pic_type, PictureType::Image);
                DeviceIntRect::zero()
            }
        };
        let prim_cache_address = gpu_cache.get_address(&prim_metadata.gpu_location);
        let no_textures = BatchTextures::no_texture();
        let clip_task_address = prim_metadata
            .clip_task_id
            .map_or(OPAQUE_TASK_ADDRESS, |id| render_tasks.get_task_address(id));
        let base_instance = SimplePrimitiveInstance::new(
            prim_cache_address,
            task_address,
            clip_task_address,
            clip_chain_rect_index,
            scroll_id,
            z,
        );

        let blend_mode = ctx.prim_store.get_blend_mode(prim_metadata, transform_kind);

        match prim_metadata.prim_kind {
            PrimitiveKind::Brush => {
                let brush = &ctx.prim_store.cpu_brushes[prim_metadata.cpu_prim_index.0];
                let batch_key = brush.get_batch_key(blend_mode);

                self.add_brush_to_batch(
                    brush,
                    prim_metadata,
                    batch_key,
                    clip_chain_rect_index,
                    clip_task_address,
                    item_bounding_rect,
                    prim_cache_address,
                    scroll_id,
                    task_address,
                    transform_kind,
                    z,
                    render_tasks,
                    0,
                    0,
                );
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
                        self.batch_list.get_suitable_batch(corner_key, item_bounding_rect);
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

                let batch = self.batch_list.get_suitable_batch(edge_key, item_bounding_rect);
                for (border_segment, instance_kind) in border_cpu.edges.iter().enumerate() {
                    match *instance_kind {
                        BorderEdgeKind::None => {},
                        _ => {
                          batch.push(base_instance.build(border_segment as i32, 0, 0));
                        }
                    }
                }
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
                let batch = self.batch_list.get_suitable_batch(key, item_bounding_rect);
                batch.push(base_instance.build(uv_address.as_int(gpu_cache), 0, 0));
            }
            PrimitiveKind::TextRun => {
                let text_cpu =
                    &ctx.prim_store.cpu_text_runs[prim_metadata.cpu_prim_index.0];
                let is_shadow = pic_type == PictureType::TextShadow;

                // TODO(gw): It probably makes sense to base this decision on the content
                //           origin field in the future (once that's configurable).
                let font_transform = if is_shadow {
                    None
                } else {
                    Some(&scroll_node.transform)
                };

                let font = text_cpu.get_font(
                    ctx.device_pixel_scale,
                    font_transform,
                );

                let glyph_fetch_buffer = &mut self.glyph_fetch_buffer;
                let batch_list = &mut self.batch_list;
                let text_run_cache_prims = &mut self.text_run_cache_prims;

                ctx.resource_cache.fetch_glyphs(
                    font,
                    &text_cpu.glyph_keys,
                    glyph_fetch_buffer,
                    gpu_cache,
                    |texture_id, mut glyph_format, glyphs| {
                        debug_assert_ne!(texture_id, SourceTexture::Invalid);

                        // Ignore color and only sample alpha when shadowing.
                        if text_cpu.is_shadow() {
                            glyph_format = glyph_format.ignore_color();
                        }

                        let subpx_dir = match glyph_format {
                            GlyphFormat::Bitmap |
                            GlyphFormat::ColorBitmap => SubpixelDirection::None,
                            _ => text_cpu.font.subpx_dir.limit_by(text_cpu.font.render_mode),
                        };

                        let batch = if is_shadow {
                            text_run_cache_prims
                                .entry(texture_id)
                                .or_insert(Vec::new())
                        } else {
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

                            let blend_mode = match glyph_format {
                                GlyphFormat::Subpixel |
                                GlyphFormat::TransformedSubpixel => {
                                    if text_cpu.font.bg_color.a != 0 {
                                        BlendMode::SubpixelWithBgColor
                                    } else if ctx.use_dual_source_blending {
                                        BlendMode::SubpixelDualSource
                                    } else {
                                        BlendMode::SubpixelConstantTextColor(text_cpu.get_color())
                                    }
                                }
                                GlyphFormat::Alpha |
                                GlyphFormat::TransformedAlpha |
                                GlyphFormat::Bitmap |
                                GlyphFormat::ColorBitmap => BlendMode::PremultipliedAlpha,
                            };

                            let key = BatchKey::new(kind, blend_mode, textures);
                            batch_list.get_suitable_batch(key, item_bounding_rect)
                        };

                        for glyph in glyphs {
                            batch.push(base_instance.build(
                                glyph.index_in_text_run,
                                glyph.uv_rect_address.as_int(),
                                subpx_dir as u32 as i32,
                            ));
                        }
                    },
                );
            }
            PrimitiveKind::Picture => {
                let picture =
                    &ctx.prim_store.cpu_pictures[prim_metadata.cpu_prim_index.0];

                match picture.surface {
                    Some(PictureSurface::TextureCache(ref cache_item)) => {
                        match picture.kind {
                            PictureKind::TextShadow { .. } |
                            PictureKind::Image { .. } => {
                                panic!("BUG: only supported as render tasks for now");
                            }
                            PictureKind::BoxShadow { image_kind, .. } => {
                                let textures = BatchTextures::color(cache_item.texture_id);
                                let kind = BatchKind::Brush(
                                    BrushBatchKind::Picture(
                                        BrushImageSourceKind::from_render_target_kind(picture.target_kind())),
                                );
                                let alpha_batch_key = BatchKey::new(
                                    kind,
                                    blend_mode,
                                    textures,
                                );

                                self.add_brush_to_batch(
                                    &picture.brush,
                                    prim_metadata,
                                    alpha_batch_key,
                                    clip_chain_rect_index,
                                    clip_task_address,
                                    item_bounding_rect,
                                    prim_cache_address,
                                    scroll_id,
                                    task_address,
                                    transform_kind,
                                    z,
                                    render_tasks,
                                    cache_item.uv_rect_handle.as_int(gpu_cache),
                                    image_kind as i32,
                                );
                            }
                        }
                    }
                    Some(PictureSurface::RenderTask(cache_task_id)) => {
                        let cache_task_address = render_tasks.get_task_address(cache_task_id);
                        let textures = BatchTextures::render_target_cache();

                        match picture.kind {
                            PictureKind::TextShadow { .. } => {
                                let kind = BatchKind::Brush(
                                    BrushBatchKind::Picture(
                                        BrushImageSourceKind::from_render_target_kind(picture.target_kind())),
                                );
                                let key = BatchKey::new(kind, blend_mode, textures);
                                let batch = self.batch_list.get_suitable_batch(key, item_bounding_rect);

                                let instance = BrushInstance {
                                    picture_address: task_address,
                                    prim_address: prim_cache_address,
                                    clip_chain_rect_index,
                                    scroll_id,
                                    clip_task_address,
                                    z,
                                    segment_index: 0,
                                    edge_flags: EdgeAaSegmentMask::empty(),
                                    user_data0: cache_task_address.0 as i32,
                                    user_data1: BrushImageKind::Simple as i32,
                                };
                                batch.push(PrimitiveInstance::from(instance));
                            }
                            PictureKind::BoxShadow { .. } => {
                                panic!("BUG: should be handled as a texture cache surface");
                            }
                            PictureKind::Image {
                                composite_mode,
                                secondary_render_task_id,
                                is_in_3d_context,
                                reference_frame_id,
                                real_local_rect,
                                ..
                            } => {
                                // If this picture is participating in a 3D rendering context,
                                // then don't add it to any batches here. Instead, create a polygon
                                // for it and add it to the current plane splitter.
                                if is_in_3d_context {
                                    // Push into parent plane splitter.

                                    let real_xf = &ctx.clip_scroll_tree.nodes[&reference_frame_id].world_content_transform;

                                    let polygon = make_polygon(
                                        real_local_rect,
                                        &real_xf,
                                        prim_index.0,
                                    );

                                    splitter.add(polygon);

                                    return;
                                }

                                // Depending on the composite mode of the picture, we generate the
                                // old style Composite primitive instances. In the future, we'll
                                // remove these and pass them through the brush batching pipeline.
                                // This will allow us to unify some of the shaders, apply clip masks
                                // when compositing pictures, and also correctly apply pixel snapping
                                // to picture compositing operations.
                                let source_id = cache_task_id;

                                match composite_mode.expect("bug: only composites here") {
                                    PictureCompositeMode::Filter(filter) => {
                                        match filter {
                                            FilterOp::Blur(..) => {
                                                let src_task_address = render_tasks.get_task_address(source_id);
                                                let key = BatchKey::new(
                                                    BatchKind::HardwareComposite,
                                                    BlendMode::PremultipliedAlpha,
                                                    BatchTextures::render_target_cache(),
                                                );
                                                let batch = self.batch_list.get_suitable_batch(key, &item_bounding_rect);
                                                let instance = CompositePrimitiveInstance::new(
                                                    task_address,
                                                    src_task_address,
                                                    RenderTaskAddress(0),
                                                    item_bounding_rect.origin.x,
                                                    item_bounding_rect.origin.y,
                                                    z,
                                                    item_bounding_rect.size.width,
                                                    item_bounding_rect.size.height,
                                                );

                                                batch.push(PrimitiveInstance::from(instance));
                                            }
                                            FilterOp::DropShadow(offset, _, _) => {
                                                let kind = BatchKind::Brush(
                                                    BrushBatchKind::Picture(BrushImageSourceKind::ColorAlphaMask),
                                                );
                                                let key = BatchKey::new(kind, blend_mode, textures);

                                                let instance = BrushInstance {
                                                    picture_address: task_address,
                                                    prim_address: prim_cache_address,
                                                    clip_chain_rect_index,
                                                    scroll_id,
                                                    clip_task_address,
                                                    z,
                                                    segment_index: 0,
                                                    edge_flags: EdgeAaSegmentMask::empty(),
                                                    user_data0: cache_task_address.0 as i32,
                                                    user_data1: BrushImageKind::Simple as i32,
                                                };

                                                {
                                                    let batch = self.batch_list.get_suitable_batch(key, item_bounding_rect);
                                                    batch.push(PrimitiveInstance::from(instance));
                                                }

                                                let secondary_id = secondary_render_task_id.expect("no secondary!?");
                                                let render_task = &render_tasks[secondary_id];
                                                let secondary_task_address = render_tasks.get_task_address(secondary_id);
                                                let render_pass_index = render_task.pass_index.expect("no render_pass_index!?");
                                                let secondary_textures = BatchTextures {
                                                    colors: [
                                                        SourceTexture::RenderTaskCacheRGBA8(render_pass_index),
                                                        SourceTexture::Invalid,
                                                        SourceTexture::Invalid,
                                                    ],
                                                };
                                                let key = BatchKey::new(
                                                    BatchKind::HardwareComposite,
                                                    BlendMode::PremultipliedAlpha,
                                                    secondary_textures,
                                                );
                                                let batch = self.batch_list.get_suitable_batch(key, &item_bounding_rect);
                                                let content_rect = prim_metadata.local_rect.translate(&-offset);
                                                let rect =
                                                    (content_rect * LayerToWorldScale::new(1.0) * ctx.device_pixel_scale).round()
                                                                                                                         .to_i32();

                                                let instance = CompositePrimitiveInstance::new(
                                                    task_address,
                                                    secondary_task_address,
                                                    RenderTaskAddress(0),
                                                    rect.origin.x,
                                                    rect.origin.y,
                                                    z,
                                                    rect.size.width,
                                                    rect.size.height,
                                                );

                                                batch.push(PrimitiveInstance::from(instance));
                                            }
                                            _ => {
                                                let key = BatchKey::new(
                                                    BatchKind::Blend,
                                                    BlendMode::PremultipliedAlpha,
                                                    BatchTextures::no_texture(),
                                                );
                                                let src_task_address = render_tasks.get_task_address(source_id);

                                                let (filter_mode, amount) = match filter {
                                                    FilterOp::Blur(..) => (0, 0.0),
                                                    FilterOp::Contrast(amount) => (1, amount),
                                                    FilterOp::Grayscale(amount) => (2, amount),
                                                    FilterOp::HueRotate(angle) => (3, angle),
                                                    FilterOp::Invert(amount) => (4, amount),
                                                    FilterOp::Saturate(amount) => (5, amount),
                                                    FilterOp::Sepia(amount) => (6, amount),
                                                    FilterOp::Brightness(amount) => (7, amount),
                                                    FilterOp::Opacity(_, amount) => (8, amount),
                                                    FilterOp::DropShadow(..) => unreachable!(),
                                                };

                                                let amount = (amount * 65535.0).round() as i32;
                                                let batch = self.batch_list.get_suitable_batch(key, &item_bounding_rect);

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
                                        }
                                    }
                                    PictureCompositeMode::MixBlend(mode) => {
                                        let backdrop_id = secondary_render_task_id.expect("no backdrop!?");

                                        let key = BatchKey::new(
                                            BatchKind::Composite {
                                                task_id,
                                                source_id,
                                                backdrop_id,
                                            },
                                            BlendMode::PremultipliedAlpha,
                                            BatchTextures::no_texture(),
                                        );
                                        let batch = self.batch_list.get_suitable_batch(key, &item_bounding_rect);
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
                                    PictureCompositeMode::Blit => {
                                        let src_task_address = render_tasks.get_task_address(source_id);
                                        let key = BatchKey::new(
                                            BatchKind::HardwareComposite,
                                            BlendMode::PremultipliedAlpha,
                                            BatchTextures::render_target_cache(),
                                        );
                                        let batch = self.batch_list.get_suitable_batch(key, &item_bounding_rect);
                                        let instance = CompositePrimitiveInstance::new(
                                            task_address,
                                            src_task_address,
                                            RenderTaskAddress(0),
                                            item_bounding_rect.origin.x,
                                            item_bounding_rect.origin.y,
                                            z,
                                            item_bounding_rect.size.width,
                                            item_bounding_rect.size.height,
                                        );

                                        batch.push(PrimitiveInstance::from(instance));
                                    }
                                }
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
                        );
                    }
                }
            }
            PrimitiveKind::AlignedGradient => {
                let gradient_cpu =
                    &ctx.prim_store.cpu_gradients[prim_metadata.cpu_prim_index.0];
                let kind = BatchKind::Transformable(
                    transform_kind,
                    TransformBatchKind::AlignedGradient,
                );
                let key = BatchKey::new(kind, blend_mode, no_textures);
                let batch = self.batch_list.get_suitable_batch(key, item_bounding_rect);
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
                let batch = self.batch_list.get_suitable_batch(key, item_bounding_rect);
                batch.push(base_instance.build(0, 0, 0));
            }
            PrimitiveKind::RadialGradient => {
                let kind = BatchKind::Transformable(
                    transform_kind,
                    TransformBatchKind::RadialGradient,
                );
                let key = BatchKey::new(kind, blend_mode, no_textures);
                let batch = self.batch_list.get_suitable_batch(key, item_bounding_rect);
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
                let batch = self.batch_list.get_suitable_batch(key, item_bounding_rect);

                batch.push(base_instance.build(
                    uv_rect_addresses[0],
                    uv_rect_addresses[1],
                    uv_rect_addresses[2],
                ));
            }
        }
    }

    fn add_brush_to_batch(
        &mut self,
        brush: &BrushPrimitive,
        prim_metadata: &PrimitiveMetadata,
        batch_key: BatchKey,
        clip_chain_rect_index: ClipChainRectIndex,
        clip_task_address: RenderTaskAddress,
        item_bounding_rect: &DeviceIntRect,
        prim_cache_address: GpuCacheAddress,
        scroll_id: ClipScrollNodeIndex,
        task_address: RenderTaskAddress,
        transform_kind: TransformedRectKind,
        z: i32,
        render_tasks: &RenderTaskTree,
        user_data0: i32,
        user_data1: i32,
    ) {
        let base_instance = BrushInstance {
            picture_address: task_address,
            prim_address: prim_cache_address,
            clip_chain_rect_index,
            scroll_id,
            clip_task_address,
            z,
            segment_index: 0,
            edge_flags: EdgeAaSegmentMask::all(),
            user_data0,
            user_data1,
        };

        match brush.segment_desc {
            Some(ref segment_desc) => {
                let alpha_batch_key = BatchKey {
                    blend_mode: BlendMode::PremultipliedAlpha,
                    ..batch_key
                };

                let alpha_batch = self.batch_list.alpha_batch_list.get_suitable_batch(
                    alpha_batch_key,
                    item_bounding_rect
                );

                let opaque_batch_key = BatchKey {
                    blend_mode: BlendMode::None,
                    ..batch_key
                };

                let opaque_batch = self.batch_list.opaque_batch_list.get_suitable_batch(
                    opaque_batch_key,
                    item_bounding_rect
                );

                for (i, segment) in segment_desc.segments.iter().enumerate() {
                    let is_inner = segment.edge_flags.is_empty();
                    let needs_blending = !prim_metadata.opacity.is_opaque ||
                                         segment.clip_task_id.is_some() ||
                                         (!is_inner && transform_kind == TransformedRectKind::Complex);

                    let clip_task_address = segment
                        .clip_task_id
                        .map_or(OPAQUE_TASK_ADDRESS, |id| render_tasks.get_task_address(id));

                    let instance = PrimitiveInstance::from(BrushInstance {
                        segment_index: i as i32,
                        edge_flags: segment.edge_flags,
                        clip_task_address,
                        ..base_instance
                    });

                    if needs_blending {
                        alpha_batch.push(instance);
                    } else {
                        opaque_batch.push(instance);
                    }
                }
            }
            None => {
                let batch = self.batch_list.get_suitable_batch(batch_key, item_bounding_rect);
                batch.push(PrimitiveInstance::from(base_instance));
            }
        }
    }
}

impl BrushPrimitive {
    fn get_batch_key(&self, blend_mode: BlendMode) -> BatchKey {
        match self.kind {
            BrushKind::Line { .. } => {
                BatchKey::new(
                    BatchKind::Brush(BrushBatchKind::Line),
                    blend_mode,
                    BatchTextures::no_texture(),
                )
            }
            BrushKind::Picture => {
                panic!("bug: get_batch_key is handled at higher level for pictures");
            }
            BrushKind::Solid { .. } => {
                BatchKey::new(
                    BatchKind::Brush(BrushBatchKind::Solid),
                    blend_mode,
                    BatchTextures::no_texture(),
                )
            }
            BrushKind::Clear => {
                BatchKey::new(
                    BatchKind::Brush(BrushBatchKind::Solid),
                    BlendMode::PremultipliedDestOut,
                    BatchTextures::no_texture(),
                )
            }
            BrushKind::Mask { .. } => {
                unreachable!("bug: mask brushes not expected in normal alpha pass");
            }
        }
    }
}

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
            // Can only resolve the TextRun's blend mode once glyphs are fetched.
            PrimitiveKind::TextRun => BlendMode::PremultipliedAlpha,
            PrimitiveKind::Border |
            PrimitiveKind::YuvImage |
            PrimitiveKind::AlignedGradient |
            PrimitiveKind::AngleGradient |
            PrimitiveKind::RadialGradient |
            PrimitiveKind::Brush |
            PrimitiveKind::Picture => if needs_blending {
                BlendMode::PremultipliedAlpha
            } else {
                BlendMode::None
            },
            PrimitiveKind::Image => if needs_blending {
                let image_cpu = &self.cpu_images[metadata.cpu_prim_index.0];
                match image_cpu.alpha_type {
                    AlphaType::PremultipliedAlpha => BlendMode::PremultipliedAlpha,
                    AlphaType::Alpha => BlendMode::Alpha,
                }
            } else {
                BlendMode::None
            },
        }
    }
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
                    let cache_handle = gpu_cache.push_deferred_per_frame_blocks(BLOCKS_PER_UV_RECT);
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

/// Construct a polygon from stacking context boundaries.
/// `anchor` here is an index that's going to be preserved in all the
/// splits of the polygon.
fn make_polygon(
    rect: LayerRect,
    transform: &LayerToWorldTransform,
    anchor: usize,
) -> Polygon<f64, WorldPixel> {
    let mat = TypedTransform3D::row_major(
        transform.m11 as f64,
        transform.m12 as f64,
        transform.m13 as f64,
        transform.m14 as f64,
        transform.m21 as f64,
        transform.m22 as f64,
        transform.m23 as f64,
        transform.m24 as f64,
        transform.m31 as f64,
        transform.m32 as f64,
        transform.m33 as f64,
        transform.m34 as f64,
        transform.m41 as f64,
        transform.m42 as f64,
        transform.m43 as f64,
        transform.m44 as f64);
    Polygon::from_transformed_rect(rect.cast().unwrap(), mat, anchor)
}

/// Batcher managing draw calls into the clip mask (in the RT cache).
#[derive(Debug)]
#[cfg_attr(feature = "capture", derive(Deserialize, Serialize))]
pub struct ClipBatcher {
    /// Rectangle draws fill up the rectangles with rounded corners.
    pub rectangles: Vec<ClipMaskInstance>,
    /// Image draws apply the image masking.
    pub images: FastHashMap<SourceTexture, Vec<ClipMaskInstance>>,
    pub border_clears: Vec<ClipMaskInstance>,
    pub borders: Vec<ClipMaskInstance>,
}

impl ClipBatcher {
    pub fn new() -> Self {
        ClipBatcher {
            rectangles: Vec::new(),
            images: FastHashMap::default(),
            border_clears: Vec::new(),
            borders: Vec::new(),
        }
    }

    pub fn add(
        &mut self,
        task_address: RenderTaskAddress,
        clips: &[ClipWorkItem],
        coordinate_system_id: CoordinateSystemId,
        resource_cache: &ResourceCache,
        gpu_cache: &GpuCache,
        clip_store: &ClipStore,
    ) {
        let mut coordinate_system_id = coordinate_system_id;
        for work_item in clips.iter() {
            let instance = ClipMaskInstance {
                render_task_address: task_address,
                scroll_node_data_index: work_item.scroll_node_data_index,
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
                                ..instance
                            });
                            coordinate_system_id = work_item.coordinate_system_id;
                        }
                    }
                    ClipSource::RoundedRectangle(..) => {
                        self.rectangles.push(ClipMaskInstance {
                            clip_data_address: gpu_address,
                            ..instance
                        });
                    }
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
