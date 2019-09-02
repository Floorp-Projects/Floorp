/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{AddFont, BlobImageResources, AsyncBlobImageRasterizer, ResourceUpdate};
use api::{BlobImageDescriptor, BlobImageHandler, BlobImageRequest, RasterizedBlobImage};
use api::{ClearCache, DebugFlags, FontInstanceKey, FontKey, FontTemplate, GlyphIndex};
use api::{ExternalImageData, ExternalImageType, BlobImageResult, BlobImageParams};
use api::{FontInstanceData, FontInstanceOptions, FontInstancePlatformOptions, FontVariation};
use api::{DirtyRect, GlyphDimensions, IdNamespace};
use api::{ImageData, ImageDescriptor, ImageKey, ImageRendering, TileSize};
use api::{BlobImageData, BlobImageKey, MemoryReport, VoidPtrToSizeFn};
use api::units::*;
#[cfg(feature = "capture")]
use crate::capture::ExternalCaptureImage;
#[cfg(feature = "replay")]
use crate::capture::PlainExternalImage;
#[cfg(any(feature = "replay", feature = "png"))]
use crate::capture::CaptureConfig;
use crate::device::TextureFilter;
use euclid::{point2, size2};
use crate::glyph_cache::GlyphCache;
use crate::glyph_cache::GlyphCacheEntry;
use crate::glyph_rasterizer::{BaseFontInstance, FontInstance, GlyphFormat, GlyphKey, GlyphRasterizer};
use crate::gpu_cache::{GpuCache, GpuCacheAddress, GpuCacheHandle};
use crate::gpu_types::UvRectKind;
use crate::image::{compute_tile_size, compute_tile_range, for_each_tile_in_range};
use crate::internal_types::{FastHashMap, FastHashSet, TextureSource, TextureUpdateList};
use crate::profiler::{ResourceProfileCounters, TextureCacheProfileCounters};
use crate::render_backend::{FrameId, FrameStamp};
use crate::render_task_graph::{RenderTaskGraph, RenderTaskId};
use crate::render_task::{RenderTaskCache, RenderTaskCacheKey};
use crate::render_task::{RenderTaskCacheEntry, RenderTaskCacheEntryHandle};
use smallvec::SmallVec;
use std::collections::hash_map::Entry::{self, Occupied, Vacant};
use std::collections::hash_map::{Iter, IterMut};
use std::collections::VecDeque;
use std::{cmp, mem};
use std::fmt::Debug;
use std::hash::Hash;
use std::os::raw::c_void;
#[cfg(any(feature = "capture", feature = "replay"))]
use std::path::PathBuf;
use std::sync::{Arc, RwLock};
use std::time::SystemTime;
use crate::texture_cache::{TextureCache, TextureCacheHandle, Eviction};
use crate::util::drain_filter;

const DEFAULT_TILE_SIZE: TileSize = 512;

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct GlyphFetchResult {
    pub index_in_text_run: i32,
    pub uv_rect_address: GpuCacheAddress,
}

// These coordinates are always in texels.
// They are converted to normalized ST
// values in the vertex shader. The reason
// for this is that the texture may change
// dimensions (e.g. the pages in a texture
// atlas can grow). When this happens, by
// storing the coordinates as texel values
// we don't need to go through and update
// various CPU-side structures.
#[derive(Debug, Clone)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct CacheItem {
    pub texture_id: TextureSource,
    pub uv_rect_handle: GpuCacheHandle,
    pub uv_rect: DeviceIntRect,
    pub texture_layer: i32,
}

impl CacheItem {
    pub fn invalid() -> Self {
        CacheItem {
            texture_id: TextureSource::Invalid,
            uv_rect_handle: GpuCacheHandle::new(),
            uv_rect: DeviceIntRect::zero(),
            texture_layer: 0,
        }
    }
}

/// Represents the backing store of an image in the cache.
/// This storage can take several forms.
#[derive(Clone, Debug)]
pub enum CachedImageData {
    /// A simple series of bytes, provided by the embedding and owned by WebRender.
    /// The format is stored out-of-band, currently in ImageDescriptor.
    Raw(Arc<Vec<u8>>),
    /// An series of commands that can be rasterized into an image via an
    /// embedding-provided callback.
    ///
    /// The commands are stored elsewhere and this variant is used as a placeholder.
    Blob,
    /// An image owned by the embedding, and referenced by WebRender. This may
    /// take the form of a texture or a heap-allocated buffer.
    External(ExternalImageData),
}

impl From<ImageData> for CachedImageData {
    fn from(img_data: ImageData) -> Self {
        match img_data {
            ImageData::Raw(data) => CachedImageData::Raw(data),
            ImageData::External(data) => CachedImageData::External(data),
        }
    }
}

impl CachedImageData {
    /// Returns true if this represents a blob.
    #[inline]
    pub fn is_blob(&self) -> bool {
        match *self {
            CachedImageData::Blob => true,
            _ => false,
        }
    }

    /// Returns true if this variant of CachedImageData should go through the texture
    /// cache.
    #[inline]
    pub fn uses_texture_cache(&self) -> bool {
        match *self {
            CachedImageData::External(ref ext_data) => match ext_data.image_type {
                ExternalImageType::TextureHandle(_) => false,
                ExternalImageType::Buffer => true,
            },
            CachedImageData::Blob => true,
            CachedImageData::Raw(_) => true,
        }
    }
}

#[derive(Debug)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct ImageProperties {
    pub descriptor: ImageDescriptor,
    pub external_image: Option<ExternalImageData>,
    pub tiling: Option<TileSize>,
}

#[derive(Debug, Copy, Clone, PartialEq)]
enum State {
    Idle,
    AddResources,
    QueryResources,
}

/// Post scene building state.
enum RasterizedBlob {
    Tiled(FastHashMap<TileOffset, RasterizedBlobImage>),
    NonTiled(Vec<RasterizedBlobImage>),
}

/// Pre scene building state.
/// We use this to generate the async blob rendering requests.
struct BlobImageTemplate {
    descriptor: ImageDescriptor,
    tiling: Option<TileSize>,
    dirty_rect: BlobDirtyRect,
    viewport_tiles: Option<TileRange>,
}

struct ImageResource {
    data: CachedImageData,
    descriptor: ImageDescriptor,
    tiling: Option<TileSize>,
}

#[derive(Clone, Debug)]
pub struct ImageTiling {
    pub image_size: DeviceIntSize,
    pub tile_size: TileSize,
}

#[derive(Default)]
struct ImageTemplates {
    images: FastHashMap<ImageKey, ImageResource>,
}

impl ImageTemplates {
    fn insert(&mut self, key: ImageKey, resource: ImageResource) {
        self.images.insert(key, resource);
    }

    fn remove(&mut self, key: ImageKey) -> Option<ImageResource> {
        self.images.remove(&key)
    }

    fn get(&self, key: ImageKey) -> Option<&ImageResource> {
        self.images.get(&key)
    }

    fn get_mut(&mut self, key: ImageKey) -> Option<&mut ImageResource> {
        self.images.get_mut(&key)
    }
}

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
struct CachedImageInfo {
    texture_cache_handle: TextureCacheHandle,
    dirty_rect: ImageDirtyRect,
    manual_eviction: bool,
}

impl CachedImageInfo {
    fn mark_unused(&mut self, texture_cache: &mut TextureCache) {
        texture_cache.mark_unused(&self.texture_cache_handle);
        self.manual_eviction = false;
    }
}

#[cfg(debug_assertions)]
impl Drop for CachedImageInfo {
    fn drop(&mut self) {
        debug_assert!(!self.manual_eviction, "Manual eviction requires cleanup");
    }
}

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct ResourceClassCache<K: Hash + Eq, V, U: Default> {
    resources: FastHashMap<K, V>,
    pub user_data: U,
}

impl<K, V, U> ResourceClassCache<K, V, U>
where
    K: Clone + Hash + Eq + Debug,
    U: Default,
{
    pub fn new() -> Self {
        ResourceClassCache {
            resources: FastHashMap::default(),
            user_data: Default::default(),
        }
    }

    pub fn get(&self, key: &K) -> &V {
        self.resources.get(key)
            .expect("Didn't find a cached resource with that ID!")
    }

    pub fn try_get(&self, key: &K) -> Option<&V> {
        self.resources.get(key)
    }

    pub fn insert(&mut self, key: K, value: V) {
        self.resources.insert(key, value);
    }

    pub fn remove(&mut self, key: &K) -> Option<V> {
        self.resources.remove(key)
    }

    pub fn get_mut(&mut self, key: &K) -> &mut V {
        self.resources.get_mut(key)
            .expect("Didn't find a cached resource with that ID!")
    }

    pub fn try_get_mut(&mut self, key: &K) -> Option<&mut V> {
        self.resources.get_mut(key)
    }

    pub fn entry(&mut self, key: K) -> Entry<K, V> {
        self.resources.entry(key)
    }

    pub fn iter(&self) -> Iter<K, V> {
        self.resources.iter()
    }

    pub fn iter_mut(&mut self) -> IterMut<K, V> {
        self.resources.iter_mut()
    }

    pub fn is_empty(&mut self) -> bool {
        self.resources.is_empty()
    }

    pub fn clear(&mut self) {
        self.resources.clear();
    }

    pub fn retain<F>(&mut self, f: F)
    where
        F: FnMut(&K, &mut V) -> bool,
    {
        self.resources.retain(f);
    }
}

#[derive(Debug, Copy, Clone, Eq, PartialEq, Hash)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
struct CachedImageKey {
    pub rendering: ImageRendering,
    pub tile: Option<TileOffset>,
}

#[derive(Debug, Copy, Clone, Eq, PartialEq, Hash)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct ImageRequest {
    pub key: ImageKey,
    pub rendering: ImageRendering,
    pub tile: Option<TileOffset>,
}

impl ImageRequest {
    pub fn with_tile(&self, offset: TileOffset) -> Self {
        ImageRequest {
            key: self.key,
            rendering: self.rendering,
            tile: Some(offset),
        }
    }

    pub fn is_untiled_auto(&self) -> bool {
        self.tile.is_none() && self.rendering == ImageRendering::Auto
    }
}

impl Into<BlobImageRequest> for ImageRequest {
    fn into(self) -> BlobImageRequest {
        BlobImageRequest {
            key: BlobImageKey(self.key),
            tile: self.tile,
        }
    }
}

impl Into<CachedImageKey> for ImageRequest {
    fn into(self) -> CachedImageKey {
        CachedImageKey {
            rendering: self.rendering,
            tile: self.tile,
        }
    }
}

#[derive(Debug)]
#[cfg_attr(feature = "capture", derive(Clone, Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub enum ImageCacheError {
    OverLimitSize,
}

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
enum ImageResult {
    UntiledAuto(CachedImageInfo),
    Multi(ResourceClassCache<CachedImageKey, CachedImageInfo, ()>),
    Err(ImageCacheError),
}

impl ImageResult {
    /// Releases any texture cache entries held alive by this ImageResult.
    fn drop_from_cache(&mut self, texture_cache: &mut TextureCache) {
        match *self {
            ImageResult::UntiledAuto(ref mut entry) => {
                entry.mark_unused(texture_cache);
            },
            ImageResult::Multi(ref mut entries) => {
                for (_, entry) in &mut entries.resources {
                    entry.mark_unused(texture_cache);
                }
            },
            ImageResult::Err(_) => {},
        }
    }
}

type ImageCache = ResourceClassCache<ImageKey, ImageResult, ()>;
pub type FontInstanceMap = Arc<RwLock<FastHashMap<FontInstanceKey, Arc<BaseFontInstance>>>>;

#[derive(Default)]
struct Resources {
    font_templates: FastHashMap<FontKey, FontTemplate>,
    font_instances: FontInstanceMap,
    image_templates: ImageTemplates,
}

impl BlobImageResources for Resources {
    fn get_font_data(&self, key: FontKey) -> &FontTemplate {
        self.font_templates.get(&key).unwrap()
    }
    fn get_font_instance_data(&self, key: FontInstanceKey) -> Option<FontInstanceData> {
        match self.font_instances.read().unwrap().get(&key) {
            Some(instance) => Some(FontInstanceData {
                font_key: instance.font_key,
                size: instance.size,
                options: Some(FontInstanceOptions {
                  render_mode: instance.render_mode,
                  flags: instance.flags,
                  bg_color: instance.bg_color,
                  synthetic_italics: instance.synthetic_italics,
                }),
                platform_options: instance.platform_options,
                variations: instance.variations.clone(),
            }),
            None => None,
        }
    }
}

// We only use this to report glyph dimensions to the user of the API, so using
// the font instance key should be enough. If we start using it to cache dimensions
// for internal font instances we should change the hash key accordingly.
pub type GlyphDimensionsCache = FastHashMap<(FontInstanceKey, GlyphIndex), Option<GlyphDimensions>>;

#[derive(Clone, Copy, Debug, PartialEq)]
pub struct BlobImageRasterizerEpoch(usize);

/// Stores parameters for clearing blob image tiles.
///
/// The clearing is necessary when originally requested tile range exceeds
/// MAX_TILES_PER_REQUEST. In this case, some tiles are not rasterized by
/// AsyncBlobImageRasterizer. They need to be cleared.
#[derive(Clone, Copy, Debug)]
pub struct BlobImageClearParams {
    pub key: BlobImageKey,
    /// Originally requested tile range to rasterize.
    pub original_tile_range: TileRange,
    /// Actual tile range that is requested to rasterize by
    /// AsyncBlobImageRasterizer.
    pub actual_tile_range: TileRange,
}

/// Information attached to AsyncBlobImageRasterizer.
#[derive(Clone, Debug)]
pub struct AsyncBlobImageInfo {
    pub epoch: BlobImageRasterizerEpoch,
    pub clear_requests: Vec<BlobImageClearParams>,
}

/// High-level container for resources managed by the `RenderBackend`.
///
/// This includes a variety of things, including images, fonts, and glyphs,
/// which may be stored as memory buffers, GPU textures, or handles to resources
/// managed by the OS or other parts of WebRender.
pub struct ResourceCache {
    cached_glyphs: GlyphCache,
    cached_images: ImageCache,
    cached_render_tasks: RenderTaskCache,

    resources: Resources,
    state: State,
    current_frame_id: FrameId,

    pub texture_cache: TextureCache,

    /// TODO(gw): We should expire (parts of) this cache semi-regularly!
    cached_glyph_dimensions: GlyphDimensionsCache,
    glyph_rasterizer: GlyphRasterizer,

    /// The set of images that aren't present or valid in the texture cache,
    /// and need to be rasterized and/or uploaded this frame. This includes
    /// both blobs and regular images.
    pending_image_requests: FastHashSet<ImageRequest>,

    blob_image_handler: Option<Box<dyn BlobImageHandler>>,
    rasterized_blob_images: FastHashMap<BlobImageKey, RasterizedBlob>,
    blob_image_templates: FastHashMap<BlobImageKey, BlobImageTemplate>,

    /// If while building a frame we encounter blobs that we didn't already
    /// rasterize, add them to this list and rasterize them synchronously.
    missing_blob_images: Vec<BlobImageParams>,
    /// The rasterizer associated with the current scene.
    blob_image_rasterizer: Option<Box<dyn AsyncBlobImageRasterizer>>,
    /// An epoch of the stored blob image rasterizer, used to skip the ones
    /// coming from low-priority scene builds if the current one is newer.
    /// This is to be removed when we get rid of the whole "missed" blob
    /// images concept.
    /// The produced one gets bumped whenever we produce a rasteriezer,
    /// which then travels through the scene building and eventually gets
    /// consumed back by us, bumping the consumed epoch.
    blob_image_rasterizer_produced_epoch: BlobImageRasterizerEpoch,
    blob_image_rasterizer_consumed_epoch: BlobImageRasterizerEpoch,
    /// A log of the last three frames worth of deleted image keys kept
    /// for debugging purposes.
    deleted_blob_keys: VecDeque<Vec<BlobImageKey>>,
    /// A set of the image keys that have been requested, and require
    /// updates to the texture cache. Images in this category trigger
    /// invalidations for picture caching tiles.
    dirty_image_keys: FastHashSet<ImageKey>,
    /// A set of the image keys that are used for render.
    active_image_keys: FastHashSet<ImageKey>,
}

impl ResourceCache {
    pub fn new(
        texture_cache: TextureCache,
        glyph_rasterizer: GlyphRasterizer,
        cached_glyphs: GlyphCache,
        blob_image_handler: Option<Box<dyn BlobImageHandler>>,
    ) -> Self {
        ResourceCache {
            cached_glyphs,
            cached_images: ResourceClassCache::new(),
            cached_render_tasks: RenderTaskCache::new(),
            resources: Resources::default(),
            cached_glyph_dimensions: FastHashMap::default(),
            texture_cache,
            state: State::Idle,
            current_frame_id: FrameId::INVALID,
            pending_image_requests: FastHashSet::default(),
            glyph_rasterizer,
            blob_image_handler,
            rasterized_blob_images: FastHashMap::default(),
            blob_image_templates: FastHashMap::default(),
            missing_blob_images: Vec::new(),
            blob_image_rasterizer: None,
            blob_image_rasterizer_produced_epoch: BlobImageRasterizerEpoch(0),
            blob_image_rasterizer_consumed_epoch: BlobImageRasterizerEpoch(0),
            // We want to keep three frames worth of delete blob keys
            deleted_blob_keys: vec![Vec::new(), Vec::new(), Vec::new()].into(),
            dirty_image_keys: FastHashSet::default(),
            active_image_keys: FastHashSet::default(),
        }
    }

    pub fn max_texture_size(&self) -> i32 {
        self.texture_cache.max_texture_size()
    }

    fn should_tile(limit: i32, descriptor: &ImageDescriptor, data: &CachedImageData) -> bool {
        let size_check = descriptor.size.width > limit || descriptor.size.height > limit;
        match *data {
            CachedImageData::Raw(_) | CachedImageData::Blob => size_check,
            CachedImageData::External(info) => {
                // External handles already represent existing textures so it does
                // not make sense to tile them into smaller ones.
                info.image_type == ExternalImageType::Buffer && size_check
            }
        }
    }

    // Request the texture cache item for a cacheable render
    // task. If the item is already cached, the texture cache
    // handle will be returned. Otherwise, the user supplied
    // closure will be invoked to generate the render task
    // chain that is required to draw this task.
    pub fn request_render_task<F>(
        &mut self,
        key: RenderTaskCacheKey,
        gpu_cache: &mut GpuCache,
        render_tasks: &mut RenderTaskGraph,
        user_data: Option<[f32; 3]>,
        is_opaque: bool,
        f: F,
    ) -> RenderTaskCacheEntryHandle
    where
        F: FnOnce(&mut RenderTaskGraph) -> RenderTaskId,
    {
        self.cached_render_tasks.request_render_task(
            key,
            &mut self.texture_cache,
            gpu_cache,
            render_tasks,
            user_data,
            is_opaque,
            |render_graph| Ok(f(render_graph))
        ).expect("Failed to request a render task from the resource cache!")
    }

    pub fn post_scene_building_update(
        &mut self,
        updates: Vec<ResourceUpdate>,
        profile_counters: &mut ResourceProfileCounters,
    ) {
        // TODO, there is potential for optimization here, by processing updates in
        // bulk rather than one by one (for example by sorting allocations by size or
        // in a way that reduces fragmentation in the atlas).

        for update in updates {
            match update {
                ResourceUpdate::AddImage(img) => {
                    if let ImageData::Raw(ref bytes) = img.data {
                        profile_counters.image_templates.inc(bytes.len());
                    }
                    self.add_image_template(img.key, img.descriptor, img.data.into(), img.tiling);
                }
                ResourceUpdate::UpdateImage(img) => {
                    self.update_image_template(img.key, img.descriptor, img.data.into(), &img.dirty_rect);
                }
                ResourceUpdate::AddBlobImage(img) => {
                    self.add_image_template(
                        img.key.as_image(),
                        img.descriptor,
                        CachedImageData::Blob,
                        img.tiling,
                    );
                }
                ResourceUpdate::UpdateBlobImage(img) => {
                    self.update_image_template(
                        img.key.as_image(),
                        img.descriptor,
                        CachedImageData::Blob,
                        &to_image_dirty_rect(
                            &img.dirty_rect
                        ),
                    );
                    self.discard_tiles_outside_visible_area(img.key, &img.visible_rect);
                }
                ResourceUpdate::DeleteImage(img) => {
                    self.delete_image_template(img);
                }
                ResourceUpdate::DeleteFont(font) => {
                    self.delete_font_template(font);
                }
                ResourceUpdate::DeleteFontInstance(font) => {
                    self.delete_font_instance(font);
                }
                ResourceUpdate::SetBlobImageVisibleArea(key, area) => {
                    self.discard_tiles_outside_visible_area(key, &area);
                }
                ResourceUpdate::AddFont(_) |
                ResourceUpdate::AddFontInstance(_) => {
                    // Handled in update_resources_pre_scene_building
                }
            }
        }
    }

    pub fn pre_scene_building_update(
        &mut self,
        updates: &mut Vec<ResourceUpdate>,
        profile_counters: &mut ResourceProfileCounters,
    ) {
        for update in updates.iter() {
            match *update {
                ResourceUpdate::AddBlobImage(ref img) => {
                    self.add_blob_image(
                        img.key,
                        &img.descriptor,
                        img.tiling,
                        Arc::clone(&img.data),
                        &img.visible_rect,
                    );
                }
                ResourceUpdate::UpdateBlobImage(ref img) => {
                    self.update_blob_image(
                        img.key,
                        &img.descriptor,
                        &img.dirty_rect,
                        Arc::clone(&img.data),
                        &img.visible_rect,
                    );
                }
                ResourceUpdate::SetBlobImageVisibleArea(ref key, ref area) => {
                    if let Some(template) = self.blob_image_templates.get_mut(&key) {
                        if let Some(tile_size) = template.tiling {
                            template.viewport_tiles = Some(compute_tile_range(
                                &area,
                                tile_size,
                            ));
                        }
                    }
                }
                _ => {}
            }
        }

        drain_filter(
            updates,
            |update| match *update {
                ResourceUpdate::AddFont(_) |
                ResourceUpdate::AddFontInstance(_) => true,
                _ => false,
            },
            // Updates that were moved out of the array:
            |update: ResourceUpdate| match update {
                ResourceUpdate::AddFont(font) => {
                    match font {
                        AddFont::Raw(id, bytes, index) => {
                            profile_counters.font_templates.inc(bytes.len());
                            self.add_font_template(id, FontTemplate::Raw(Arc::new(bytes), index));
                        }
                        AddFont::Native(id, native_font_handle) => {
                            self.add_font_template(id, FontTemplate::Native(native_font_handle));
                        }
                    }
                }
                ResourceUpdate::AddFontInstance(instance) => {
                    self.add_font_instance(
                        instance.key,
                        instance.font_key,
                        instance.glyph_size,
                        instance.options,
                        instance.platform_options,
                        instance.variations,
                    );
                }
                _ => { unreachable!(); }
            }
        );
    }

    pub fn set_blob_rasterizer(
        &mut self, rasterizer: Box<dyn AsyncBlobImageRasterizer>,
        supp: AsyncBlobImageInfo,
    ) {
        if self.blob_image_rasterizer_consumed_epoch.0 < supp.epoch.0 {
            self.blob_image_rasterizer = Some(rasterizer);
            self.blob_image_rasterizer_consumed_epoch = supp.epoch;
        }

        // Discard blob image tiles that are not rendered by AsyncBlobImageRasterizer.
        // It happens when originally requested tile range exceeds MAX_TILES_PER_REQUEST.
        for req in supp.clear_requests {
            let tiles = match self.rasterized_blob_images.get_mut(&req.key) {
                Some(RasterizedBlob::Tiled(tiles)) => tiles,
                _ => { continue; }
            };

            tiles.retain(|tile, _| {
                !req.original_tile_range.contains(*tile) ||
                req.actual_tile_range.contains(*tile)
            });

            let texture_cache = &mut self.texture_cache;
            match self.cached_images.try_get_mut(&req.key.as_image()) {
                Some(&mut ImageResult::Multi(ref mut entries)) => {
                    entries.retain(|key, entry| {
                        if !req.original_tile_range.contains(key.tile.unwrap()) ||
                           req.actual_tile_range.contains(key.tile.unwrap()) {
                            return true;
                        }
                        entry.mark_unused(texture_cache);
                        return false;
                    });
                }
                _ => {}
            }
        }
    }

    pub fn add_rasterized_blob_images(&mut self, images: Vec<(BlobImageRequest, BlobImageResult)>) {
        for (request, result) in images {
            let data = match result {
                Ok(data) => data,
                Err(..) => {
                    warn!("Failed to rasterize a blob image");
                    continue;
                }
            };

            // First make sure we have an entry for this key (using a placeholder
            // if need be).
            let image = self.rasterized_blob_images.entry(request.key).or_insert_with(
                || { RasterizedBlob::Tiled(FastHashMap::default()) }
            );

            if let Some(tile) = request.tile {
                if let RasterizedBlob::NonTiled(..) = *image {
                    *image = RasterizedBlob::Tiled(FastHashMap::default());
                }

                if let RasterizedBlob::Tiled(ref mut tiles) = *image {
                    tiles.insert(tile, data);
                }
            } else {
                if let RasterizedBlob::NonTiled(ref mut queue) = *image {
                    // If our new rasterized rect overwrites items in the queue, discard them.
                    queue.retain(|img| {
                        !data.rasterized_rect.contains_rect(&img.rasterized_rect)
                    });

                    queue.push(data);
                } else {
                    *image = RasterizedBlob::NonTiled(vec![data]);
                }
            }
        }
    }

    pub fn add_font_template(&mut self, font_key: FontKey, template: FontTemplate) {
        // Push the new font to the font renderer, and also store
        // it locally for glyph metric requests.
        self.glyph_rasterizer.add_font(font_key, template.clone());
        self.resources.font_templates.insert(font_key, template);
    }

    pub fn delete_font_template(&mut self, font_key: FontKey) {
        self.glyph_rasterizer.delete_font(font_key);
        self.resources.font_templates.remove(&font_key);
        self.cached_glyphs
            .clear_fonts(&mut self.texture_cache, |font| font.font_key == font_key);
        if let Some(ref mut r) = self.blob_image_handler {
            r.delete_font(font_key);
        }
    }

    pub fn add_font_instance(
        &mut self,
        instance_key: FontInstanceKey,
        font_key: FontKey,
        size: Au,
        options: Option<FontInstanceOptions>,
        platform_options: Option<FontInstancePlatformOptions>,
        variations: Vec<FontVariation>,
    ) {
        let FontInstanceOptions {
            render_mode,
            flags,
            bg_color,
            synthetic_italics,
            ..
        } = options.unwrap_or_default();
        let instance = Arc::new(BaseFontInstance {
            instance_key,
            font_key,
            size,
            bg_color,
            render_mode,
            flags,
            synthetic_italics,
            platform_options,
            variations,
        });
        self.resources.font_instances
            .write()
            .unwrap()
            .insert(instance_key, instance);
    }

    pub fn delete_font_instance(&mut self, instance_key: FontInstanceKey) {
        self.resources.font_instances
            .write()
            .unwrap()
            .remove(&instance_key);
        if let Some(ref mut r) = self.blob_image_handler {
            r.delete_font_instance(instance_key);
        }
    }

    pub fn get_font_instances(&self) -> FontInstanceMap {
        self.resources.font_instances.clone()
    }

    pub fn get_font_instance(&self, instance_key: FontInstanceKey) -> Option<Arc<BaseFontInstance>> {
        let instance_map = self.resources.font_instances.read().unwrap();
        instance_map.get(&instance_key).map(|instance| { Arc::clone(instance) })
    }

    pub fn add_image_template(
        &mut self,
        image_key: ImageKey,
        descriptor: ImageDescriptor,
        data: CachedImageData,
        mut tiling: Option<TileSize>,
    ) {
        if tiling.is_none() && Self::should_tile(self.max_texture_size(), &descriptor, &data) {
            // We aren't going to be able to upload a texture this big, so tile it, even
            // if tiling was not requested.
            tiling = Some(DEFAULT_TILE_SIZE);
        }

        let resource = ImageResource {
            descriptor,
            data,
            tiling,
        };

        self.resources.image_templates.insert(image_key, resource);
    }

    pub fn update_image_template(
        &mut self,
        image_key: ImageKey,
        descriptor: ImageDescriptor,
        data: CachedImageData,
        dirty_rect: &ImageDirtyRect,
    ) {
        let max_texture_size = self.max_texture_size();
        let image = match self.resources.image_templates.get_mut(image_key) {
            Some(res) => res,
            None => panic!("Attempt to update non-existent image"),
        };

        let mut tiling = image.tiling;
        if tiling.is_none() && Self::should_tile(max_texture_size, &descriptor, &data) {
            tiling = Some(DEFAULT_TILE_SIZE);
        }

        // Each cache entry stores its own copy of the image's dirty rect. This allows them to be
        // updated independently.
        match self.cached_images.try_get_mut(&image_key) {
            Some(&mut ImageResult::UntiledAuto(ref mut entry)) => {
                entry.dirty_rect = entry.dirty_rect.union(dirty_rect);
            }
            Some(&mut ImageResult::Multi(ref mut entries)) => {
                for (key, entry) in entries.iter_mut() {
                    // We want the dirty rect relative to the tile and not the whole image.
                    let local_dirty_rect = match (tiling, key.tile) {
                        (Some(tile_size), Some(tile)) => {
                            dirty_rect.map(|mut rect|{
                                let tile_offset = DeviceIntPoint::new(
                                    tile.x as i32,
                                    tile.y as i32,
                                ) * tile_size as i32;
                                rect.origin -= tile_offset.to_vector();

                                let tile_rect = compute_tile_size(
                                    &descriptor.size.into(),
                                    tile_size,
                                    tile,
                                ).into();

                                rect.intersection(&tile_rect).unwrap_or(DeviceIntRect::zero())
                            })
                        }
                        (None, Some(..)) => DirtyRect::All,
                        _ => *dirty_rect,
                    };
                    entry.dirty_rect = entry.dirty_rect.union(&local_dirty_rect);
                }
            }
            _ => {}
        }

        if image.descriptor.format != descriptor.format {
            // could be a stronger warning/error?
            trace!("Format change {:?} -> {:?}", image.descriptor.format, descriptor.format);
        }
        *image = ImageResource {
            descriptor,
            data,
            tiling,
        };
    }

    // Happens before scene building.
    pub fn add_blob_image(
        &mut self,
        key: BlobImageKey,
        descriptor: &ImageDescriptor,
        mut tiling: Option<TileSize>,
        data: Arc<BlobImageData>,
        visible_rect: &DeviceIntRect,
    ) {
        let max_texture_size = self.max_texture_size();
        tiling = get_blob_tiling(tiling, descriptor, max_texture_size);

        let viewport_tiles = tiling.map(|tile_size| compute_tile_range(&visible_rect, tile_size));

        self.blob_image_handler.as_mut().unwrap().add(key, data, visible_rect, tiling);

        self.blob_image_templates.insert(
            key,
            BlobImageTemplate {
                descriptor: *descriptor,
                tiling,
                dirty_rect: DirtyRect::All,
                viewport_tiles,
            },
        );
    }

    // Happens before scene building.
    pub fn update_blob_image(
        &mut self,
        key: BlobImageKey,
        descriptor: &ImageDescriptor,
        dirty_rect: &BlobDirtyRect,
        data: Arc<BlobImageData>,
        visible_rect: &DeviceIntRect,
    ) {
        self.blob_image_handler.as_mut().unwrap().update(key, data, visible_rect, dirty_rect);

        let max_texture_size = self.max_texture_size();

        let image = self.blob_image_templates
            .get_mut(&key)
            .expect("Attempt to update non-existent blob image");

        let tiling = get_blob_tiling(image.tiling, descriptor, max_texture_size);

        let viewport_tiles = image.tiling.map(|tile_size| compute_tile_range(&visible_rect, tile_size));

        *image = BlobImageTemplate {
            descriptor: *descriptor,
            tiling,
            dirty_rect: dirty_rect.union(&image.dirty_rect),
            viewport_tiles,
        };
    }

    pub fn delete_image_template(&mut self, image_key: ImageKey) {
        // Remove the template.
        let value = self.resources.image_templates.remove(image_key);

        // Release the corresponding texture cache entry, if any.
        if let Some(mut cached) = self.cached_images.remove(&image_key) {
            cached.drop_from_cache(&mut self.texture_cache);
        }

        match value {
            Some(image) => if image.data.is_blob() {
                let blob_key = BlobImageKey(image_key);
                self.blob_image_handler.as_mut().unwrap().delete(blob_key);
                self.deleted_blob_keys.back_mut().unwrap().push(blob_key);
                self.blob_image_templates.remove(&blob_key);
                self.rasterized_blob_images.remove(&blob_key);
            },
            None => {
                warn!("Delete the non-exist key");
                debug!("key={:?}", image_key);
            }
        }
    }

    /// Check if an image has changed since it was last requested.
    pub fn is_image_dirty(
        &self,
        image_key: ImageKey,
    ) -> bool {
        self.dirty_image_keys.contains(&image_key)
    }

    pub fn is_image_active(
        &self,
        image_key: ImageKey,
    ) -> bool {
        self.active_image_keys.contains(&image_key)
    }

    pub fn set_image_active(
        &mut self,
        image_key: ImageKey,
    ) {
        self.active_image_keys.insert(image_key);
    }

    pub fn request_image(
        &mut self,
        request: ImageRequest,
        gpu_cache: &mut GpuCache,
    ) {
        debug_assert_eq!(self.state, State::AddResources);

        let template = match self.resources.image_templates.get(request.key) {
            Some(template) => template,
            None => {
                warn!("ERROR: Trying to render deleted / non-existent key");
                debug!("key={:?}", request.key);
                return
            }
        };

        // Images that don't use the texture cache can early out.
        if !template.data.uses_texture_cache() {
            return;
        }

        let side_size =
            template.tiling.map_or(cmp::max(template.descriptor.size.width, template.descriptor.size.height),
                                   |tile_size| tile_size as i32);
        if side_size > self.texture_cache.max_texture_size() {
            // The image or tiling size is too big for hardware texture size.
            warn!("Dropping image, image:(w:{},h:{}, tile:{}) is too big for hardware!",
                  template.descriptor.size.width, template.descriptor.size.height, template.tiling.unwrap_or(0));
            self.cached_images.insert(request.key, ImageResult::Err(ImageCacheError::OverLimitSize));
            return;
        }

        let storage = match self.cached_images.entry(request.key) {
            Occupied(e) => {
                // We might have an existing untiled entry, and need to insert
                // a second entry. In such cases we need to move the old entry
                // out first, replacing it with a dummy entry, and then creating
                // the tiled/multi-entry variant.
                let entry = e.into_mut();
                if !request.is_untiled_auto() {
                    let untiled_entry = match entry {
                        &mut ImageResult::UntiledAuto(ref mut entry) => {
                            Some(mem::replace(entry, CachedImageInfo {
                                texture_cache_handle: TextureCacheHandle::invalid(),
                                dirty_rect: DirtyRect::All,
                                manual_eviction: false,
                            }))
                        }
                        _ => None
                    };

                    if let Some(untiled_entry) = untiled_entry {
                        let mut entries = ResourceClassCache::new();
                        let untiled_key = CachedImageKey {
                            rendering: ImageRendering::Auto,
                            tile: None,
                        };
                        entries.insert(untiled_key, untiled_entry);
                        *entry = ImageResult::Multi(entries);
                    }
                }
                entry
            }
            Vacant(entry) => {
                entry.insert(if request.is_untiled_auto() {
                    ImageResult::UntiledAuto(CachedImageInfo {
                        texture_cache_handle: TextureCacheHandle::invalid(),
                        dirty_rect: DirtyRect::All,
                        manual_eviction: false,
                    })
                } else {
                    ImageResult::Multi(ResourceClassCache::new())
                })
            }
        };

        // If this image exists in the texture cache, *and* the dirty rect
        // in the cache is empty, then it is valid to use as-is.
        let entry = match *storage {
            ImageResult::UntiledAuto(ref mut entry) => entry,
            ImageResult::Multi(ref mut entries) => {
                entries.entry(request.into())
                    .or_insert(CachedImageInfo {
                        texture_cache_handle: TextureCacheHandle::invalid(),
                        dirty_rect: DirtyRect::All,
                        manual_eviction: false,
                    })
            },
            ImageResult::Err(_) => panic!("Errors should already have been handled"),
        };

        let needs_upload = self.texture_cache.request(&entry.texture_cache_handle, gpu_cache);

        if !needs_upload && entry.dirty_rect.is_empty() {
            return
        }

        if !self.pending_image_requests.insert(request) {
            return
        }

        // By this point, we know that the image request is considered dirty, and will
        // require a texture cache modification.
        self.dirty_image_keys.insert(request.key);

        if template.data.is_blob() {
            let request: BlobImageRequest = request.into();
            let missing = match (self.rasterized_blob_images.get(&request.key), request.tile) {
                (Some(RasterizedBlob::Tiled(tiles)), Some(tile)) => !tiles.contains_key(&tile),
                (Some(RasterizedBlob::NonTiled(ref queue)), None) => queue.is_empty(),
                _ => true,
            };

            // For some reason the blob image is missing. We'll fall back to
            // rasterizing it on the render backend thread.
            if missing {
                let descriptor = BlobImageDescriptor {
                    rect: match template.tiling {
                        Some(tile_size) => {
                            let tile = request.tile.unwrap();
                            LayoutIntRect {
                                origin: point2(tile.x, tile.y) * tile_size as i32,
                                size: blob_size(compute_tile_size(
                                    &template.descriptor.size.into(),
                                    tile_size,
                                    tile,
                                )),
                            }
                        }
                        None => blob_size(template.descriptor.size).into(),
                    },
                    format: template.descriptor.format,
                };

                assert!(!descriptor.rect.is_empty());

                if !self.blob_image_templates.contains_key(&request.key) {
                    panic!("already missing blob image key {:?} deleted: {:?}", request, self.deleted_blob_keys);
                }

                self.missing_blob_images.push(
                    BlobImageParams {
                        request,
                        descriptor,
                        dirty_rect: DirtyRect::All,
                    }
                );
            }
        }
    }

    pub fn create_blob_scene_builder_requests(
        &mut self,
        keys: &[BlobImageKey]
    ) -> (Option<(Box<dyn AsyncBlobImageRasterizer>, AsyncBlobImageInfo)>, Vec<BlobImageParams>) {
        if self.blob_image_handler.is_none() || keys.is_empty() {
            return (None, Vec::new());
        }

        let mut blob_tiles_clear_requests = Vec::new();
        let mut blob_request_params = Vec::new();
        for key in keys {
            let template = self.blob_image_templates.get_mut(key).unwrap();

            if let Some(tile_size) = template.tiling {
                // If we know that only a portion of the blob image is in the viewport,
                // only request these visible tiles since blob images can be huge.
                let mut tiles = template.viewport_tiles.unwrap_or_else(|| {
                    // Default to requesting the full range of tiles.
                    compute_tile_range(
                        &DeviceIntRect {
                            origin: point2(0, 0),
                            size: template.descriptor.size,
                        },
                        tile_size,
                    )
                });

                let image_dirty_rect = to_image_dirty_rect(&template.dirty_rect);
                // Don't request tiles that weren't invalidated.
                if let DirtyRect::Partial(dirty_rect) = image_dirty_rect {
                    let dirty_rect = DeviceIntRect {
                        origin: point2(
                            dirty_rect.origin.x,
                            dirty_rect.origin.y,
                        ),
                        size: size2(
                            dirty_rect.size.width,
                            dirty_rect.size.height,
                        ),
                    };
                    let dirty_tiles = compute_tile_range(
                        &dirty_rect,
                        tile_size,
                    );

                    tiles = tiles.intersection(&dirty_tiles).unwrap_or_else(TileRange::zero);
                }

                let original_tile_range = tiles;

                // This code tries to keep things sane if Gecko sends
                // nonsensical blob image requests.
                // Constant here definitely needs to be tweaked.
                const MAX_TILES_PER_REQUEST: i32 = 512;
                // For truly nonsensical requests, we might run into overflow
                // when computing width * height, so we first check each extent
                // individually.
                while !tiles.size.is_empty_or_negative()
                    && (tiles.size.width > MAX_TILES_PER_REQUEST
                        || tiles.size.height > MAX_TILES_PER_REQUEST
                        || tiles.size.width * tiles.size.height > MAX_TILES_PER_REQUEST) {
                    let limit = 46340; // sqrt(i32::MAX) rounded down to avoid overflow.
                    let w = tiles.size.width.min(limit);
                    let h = tiles.size.height.min(limit);
                    let diff = w * h - MAX_TILES_PER_REQUEST;
                    // Remove tiles in the largest dimension.
                    if tiles.size.width > tiles.size.height {
                        tiles.size.width -= diff / h + 1;
                        tiles.origin.x += diff / (2 * h);
                    } else {
                        tiles.size.height -= diff / w + 1;
                        tiles.origin.y += diff / (2 * w);
                    }
                }

                // When originally requested tile range exceeds MAX_TILES_PER_REQUEST,
                // some tiles are not rasterized by AsyncBlobImageRasterizer.
                // They need to be cleared.
                if original_tile_range != tiles {
                    let clear_params = BlobImageClearParams {
                        key: *key,
                        original_tile_range,
                        actual_tile_range: tiles,
                    };
                    blob_tiles_clear_requests.push(clear_params);
                }

                for_each_tile_in_range(&tiles, |tile| {
                    let descriptor = BlobImageDescriptor {
                        rect: LayoutIntRect {
                            origin: point2(tile.x, tile.y) * tile_size as i32,
                            size: blob_size(compute_tile_size(
                                &template.descriptor.size.into(),
                                tile_size,
                                tile,
                            )),
                        },
                        format: template.descriptor.format,
                    };

                    assert!(descriptor.rect.size.width > 0 && descriptor.rect.size.height > 0);
                    // TODO: We only track dirty rects for non-tiled blobs but we
                    // should also do it with tiled ones unless we settle for a small
                    // tile size.
                    blob_request_params.push(
                        BlobImageParams {
                            request: BlobImageRequest {
                                key: *key,
                                tile: Some(tile),
                            },
                            descriptor,
                            dirty_rect: DirtyRect::All,
                        }
                    );
                });
            } else {
                let mut needs_upload = match self.cached_images.try_get(&key.as_image()) {
                    Some(&ImageResult::UntiledAuto(ref entry)) => {
                        self.texture_cache.needs_upload(&entry.texture_cache_handle)
                    }
                    _ => true,
                };

                // If the queue of ratserized updates is growing it probably means that
                // the texture is not getting uploaded because the display item is off-screen.
                // In that case we are better off
                // - Either not kicking rasterization for that image (avoid wasted cpu work
                //   but will jank next time the item is visible because of lazy rasterization.
                // - Clobber the update queue by pushing an update with a larger dirty rect
                //   to prevent it from accumulating.
                //
                // We do the latter here but it's not ideal and might want to revisit and do
                // the former instead.
                match self.rasterized_blob_images.get(key) {
                    Some(RasterizedBlob::NonTiled(ref queue)) => {
                        if queue.len() > 2 {
                            needs_upload = true;
                        }
                    }
                    _ => {},
                };

                let dirty_rect = if needs_upload {
                    // The texture cache entry has been evicted, treat it as all dirty.
                    DirtyRect::All
                } else {
                    template.dirty_rect
                };

                assert!(template.descriptor.size.width > 0 && template.descriptor.size.height > 0);
                blob_request_params.push(
                    BlobImageParams {
                        request: BlobImageRequest {
                            key: *key,
                            tile: None,
                        },
                        descriptor: BlobImageDescriptor {
                            rect: blob_size(template.descriptor.size).into(),
                            format: template.descriptor.format,
                        },
                        dirty_rect,
                    }
                );
            }
            template.dirty_rect = DirtyRect::empty();
        }
        self.blob_image_rasterizer_produced_epoch.0 += 1;
        let info = AsyncBlobImageInfo {
            epoch: self.blob_image_rasterizer_produced_epoch,
            clear_requests: blob_tiles_clear_requests,
        };
        let handler = self.blob_image_handler.as_mut().unwrap();
        handler.prepare_resources(&self.resources, &blob_request_params);
        (Some((handler.create_blob_rasterizer(), info)), blob_request_params)
    }

    fn discard_tiles_outside_visible_area(
        &mut self,
        key: BlobImageKey,
        area: &DeviceIntRect
    ) {
        let template = match self.blob_image_templates.get(&key) {
            Some(template) => template,
            None => {
                //println!("Missing image template (key={:?})!", key);
                return;
            }
        };
        let tile_size = match template.tiling {
            Some(size) => size,
            None => { return; }
        };

        let tiles = match self.rasterized_blob_images.get_mut(&key) {
            Some(RasterizedBlob::Tiled(tiles)) => tiles,
            _ => { return; }
        };

        let tile_range = compute_tile_range(
            &area,
            tile_size,
        );

        tiles.retain(|tile, _| { tile_range.contains(*tile) });

        let texture_cache = &mut self.texture_cache;
        match self.cached_images.try_get_mut(&key.as_image()) {
            Some(&mut ImageResult::Multi(ref mut entries)) => {
                entries.retain(|key, entry| {
                    if key.tile.is_none() || tile_range.contains(key.tile.unwrap()) {
                        return true;
                    }
                    entry.mark_unused(texture_cache);
                    return false;
                });
            }
            _ => {}
        }
    }

    pub fn request_glyphs(
        &mut self,
        mut font: FontInstance,
        glyph_keys: &[GlyphKey],
        gpu_cache: &mut GpuCache,
        render_task_tree: &mut RenderTaskGraph,
    ) {
        debug_assert_eq!(self.state, State::AddResources);

        self.glyph_rasterizer.prepare_font(&mut font);
        self.glyph_rasterizer.request_glyphs(
            &mut self.cached_glyphs,
            font,
            glyph_keys,
            &mut self.texture_cache,
            gpu_cache,
            &mut self.cached_render_tasks,
            render_task_tree,
        );
    }

    pub fn pending_updates(&mut self) -> TextureUpdateList {
        self.texture_cache.pending_updates()
    }

    pub fn fetch_glyphs<F>(
        &self,
        mut font: FontInstance,
        glyph_keys: &[GlyphKey],
        fetch_buffer: &mut Vec<GlyphFetchResult>,
        gpu_cache: &mut GpuCache,
        mut f: F,
    ) where
        F: FnMut(TextureSource, GlyphFormat, &[GlyphFetchResult]),
    {
        debug_assert_eq!(self.state, State::QueryResources);

        self.glyph_rasterizer.prepare_font(&mut font);
        let glyph_key_cache = self.cached_glyphs.get_glyph_key_cache_for_font(&font);

        let mut current_texture_id = TextureSource::Invalid;
        let mut current_glyph_format = GlyphFormat::Subpixel;
        debug_assert!(fetch_buffer.is_empty());

        for (loop_index, key) in glyph_keys.iter().enumerate() {
            let (cache_item, glyph_format) = match *glyph_key_cache.get(key) {
                GlyphCacheEntry::Cached(ref glyph) => {
                    (self.texture_cache.get(&glyph.texture_cache_handle), glyph.format)
                }
                GlyphCacheEntry::Blank | GlyphCacheEntry::Pending => continue,
            };
            if current_texture_id != cache_item.texture_id ||
                current_glyph_format != glyph_format {
                if !fetch_buffer.is_empty() {
                    f(current_texture_id, current_glyph_format, fetch_buffer);
                    fetch_buffer.clear();
                }
                current_texture_id = cache_item.texture_id;
                current_glyph_format = glyph_format;
            }
            fetch_buffer.push(GlyphFetchResult {
                index_in_text_run: loop_index as i32,
                uv_rect_address: gpu_cache.get_address(&cache_item.uv_rect_handle),
            });
        }

        if !fetch_buffer.is_empty() {
            f(current_texture_id, current_glyph_format, fetch_buffer);
            fetch_buffer.clear();
        }
    }

    pub fn get_glyph_dimensions(
        &mut self,
        font: &FontInstance,
        glyph_index: GlyphIndex,
    ) -> Option<GlyphDimensions> {
        match self.cached_glyph_dimensions.entry((font.instance_key, glyph_index)) {
            Occupied(entry) => *entry.get(),
            Vacant(entry) => *entry.insert(
                self.glyph_rasterizer
                    .get_glyph_dimensions(font, glyph_index),
            ),
        }
    }

    pub fn get_glyph_index(&mut self, font_key: FontKey, ch: char) -> Option<u32> {
        self.glyph_rasterizer.get_glyph_index(font_key, ch)
    }

    #[inline]
    pub fn get_cached_image(&self, request: ImageRequest) -> Result<CacheItem, ()> {
        debug_assert_eq!(self.state, State::QueryResources);
        let image_info = self.get_image_info(request)?;
        Ok(self.get_texture_cache_item(&image_info.texture_cache_handle))
    }

    pub fn get_cached_render_task(
        &self,
        handle: &RenderTaskCacheEntryHandle,
    ) -> &RenderTaskCacheEntry {
        self.cached_render_tasks.get_cache_entry(handle)
    }

    #[inline]
    fn get_image_info(&self, request: ImageRequest) -> Result<&CachedImageInfo, ()> {
        // TODO(Jerry): add a debug option to visualize the corresponding area for
        // the Err() case of CacheItem.
        match *self.cached_images.get(&request.key) {
            ImageResult::UntiledAuto(ref image_info) => Ok(image_info),
            ImageResult::Multi(ref entries) => Ok(entries.get(&request.into())),
            ImageResult::Err(_) => Err(()),
        }
    }

    #[inline]
    pub fn get_texture_cache_item(&self, handle: &TextureCacheHandle) -> CacheItem {
        self.texture_cache.get(handle)
    }

    pub fn get_image_properties(&self, image_key: ImageKey) -> Option<ImageProperties> {
        let image_template = &self.resources.image_templates.get(image_key);

        image_template.map(|image_template| {
            let external_image = match image_template.data {
                CachedImageData::External(ext_image) => match ext_image.image_type {
                    ExternalImageType::TextureHandle(_) => Some(ext_image),
                    // external buffer uses resource_cache.
                    ExternalImageType::Buffer => None,
                },
                // raw and blob image are all using resource_cache.
                CachedImageData::Raw(..) | CachedImageData::Blob => None,
            };

            ImageProperties {
                descriptor: image_template.descriptor,
                external_image,
                tiling: image_template.tiling,
            }
        })
    }

    pub fn prepare_for_frames(&mut self, time: SystemTime) {
        self.texture_cache.prepare_for_frames(time);
    }

    pub fn bookkeep_after_frames(&mut self) {
        self.texture_cache.bookkeep_after_frames();
    }

    pub fn requires_frame_build(&self) -> bool {
        self.texture_cache.requires_frame_build()
    }

    pub fn begin_frame(&mut self, stamp: FrameStamp) {
        debug_assert_eq!(self.state, State::Idle);
        self.state = State::AddResources;
        self.texture_cache.begin_frame(stamp);
        self.cached_glyphs.begin_frame(
            stamp,
            &mut self.texture_cache,
            &self.cached_render_tasks,
            &mut self.glyph_rasterizer,
        );
        self.cached_render_tasks.begin_frame(&mut self.texture_cache);
        self.current_frame_id = stamp.frame_id();
        self.active_image_keys.clear();

        // pop the old frame and push a new one
        self.deleted_blob_keys.pop_front();
        self.deleted_blob_keys.push_back(Vec::new());
    }

    pub fn block_until_all_resources_added(
        &mut self,
        gpu_cache: &mut GpuCache,
        render_tasks: &mut RenderTaskGraph,
        texture_cache_profile: &mut TextureCacheProfileCounters,
    ) {
        profile_scope!("block_until_all_resources_added");

        debug_assert_eq!(self.state, State::AddResources);
        self.state = State::QueryResources;

        self.glyph_rasterizer.resolve_glyphs(
            &mut self.cached_glyphs,
            &mut self.texture_cache,
            gpu_cache,
            &mut self.cached_render_tasks,
            render_tasks,
            texture_cache_profile,
        );

        self.rasterize_missing_blob_images();

        // Apply any updates of new / updated images (incl. blobs) to the texture cache.
        self.update_texture_cache(gpu_cache);
    }

    fn rasterize_missing_blob_images(&mut self) {
        if self.missing_blob_images.is_empty() {
            return;
        }

        self.blob_image_handler
            .as_mut()
            .unwrap()
            .prepare_resources(&self.resources, &self.missing_blob_images);


        for blob_image in &self.missing_blob_images {
            if !self.blob_image_templates.contains_key(&blob_image.request.key) {
                panic!("missing blob image key {:?} deleted: {:?}", blob_image, self.deleted_blob_keys);
            }
        }
        let is_low_priority = false;
        let rasterized_blobs = self.blob_image_rasterizer
            .as_mut()
            .unwrap()
            .rasterize(&self.missing_blob_images, is_low_priority);

        self.add_rasterized_blob_images(rasterized_blobs);

        self.missing_blob_images.clear();
    }

    fn update_texture_cache(&mut self, gpu_cache: &mut GpuCache) {
        for request in self.pending_image_requests.drain() {
            let image_template = self.resources.image_templates.get_mut(request.key).unwrap();
            debug_assert!(image_template.data.uses_texture_cache());

            let mut updates: SmallVec<[(CachedImageData, Option<DeviceIntRect>); 1]> = SmallVec::new();

            match image_template.data {
                CachedImageData::Raw(..) | CachedImageData::External(..) => {
                    // Safe to clone here since the Raw image data is an
                    // Arc, and the external image data is small.
                    updates.push((image_template.data.clone(), None));
                }
                CachedImageData::Blob => {

                    let blob_image = self.rasterized_blob_images.get_mut(&BlobImageKey(request.key)).unwrap();
                    match (blob_image, request.tile) {
                        (RasterizedBlob::Tiled(ref tiles), Some(tile)) => {
                            let img = &tiles[&tile];
                            updates.push((
                                CachedImageData::Raw(Arc::clone(&img.data)),
                                Some(img.rasterized_rect)
                            ));
                        }
                        (RasterizedBlob::NonTiled(ref mut queue), None) => {
                            for img in queue.drain(..) {
                                updates.push((
                                    CachedImageData::Raw(img.data),
                                    Some(img.rasterized_rect)
                                ));
                            }
                        }
                        _ =>  {
                            debug_assert!(false, "invalid blob image request during frame building");
                            continue;
                        }
                    }
                }
            };

            for (image_data, blob_rasterized_rect) in updates {
                let entry = match *self.cached_images.get_mut(&request.key) {
                    ImageResult::UntiledAuto(ref mut entry) => entry,
                    ImageResult::Multi(ref mut entries) => entries.get_mut(&request.into()),
                    ImageResult::Err(_) => panic!("Update requested for invalid entry")
                };

                let mut descriptor = image_template.descriptor.clone();
                let mut dirty_rect = entry.dirty_rect.replace_with_empty();

                if let Some(tile) = request.tile {
                    let tile_size = image_template.tiling.unwrap();
                    let clipped_tile_size = compute_tile_size(&descriptor.size.into(), tile_size, tile);

                    // The tiled image could be stored on the CPU as one large image or be
                    // already broken up into tiles. This affects the way we compute the stride
                    // and offset.
                    let tiled_on_cpu = image_template.data.is_blob();
                    if !tiled_on_cpu {
                        let bpp = descriptor.format.bytes_per_pixel();
                        let stride = descriptor.compute_stride();
                        descriptor.stride = Some(stride);
                        descriptor.offset +=
                            tile.y as i32 * tile_size as i32 * stride +
                            tile.x as i32 * tile_size as i32 * bpp;
                    }

                    descriptor.size = clipped_tile_size;
                }

                // If we are uploading the dirty region of a blob image we might have several
                // rects to upload so we use each of these rasterized rects rather than the
                // overall dirty rect of the image.
                if let Some(rect) = blob_rasterized_rect {
                    dirty_rect = DirtyRect::Partial(rect);
                }

                let filter = match request.rendering {
                    ImageRendering::Pixelated => {
                        TextureFilter::Nearest
                    }
                    ImageRendering::Auto | ImageRendering::CrispEdges => {
                        // If the texture uses linear filtering, enable mipmaps and
                        // trilinear filtering, for better image quality. We only
                        // support this for now on textures that are not placed
                        // into the shared cache. This accounts for any image
                        // that is > 512 in either dimension, so it should cover
                        // the most important use cases. We may want to support
                        // mip-maps on shared cache items in the future.
                        if descriptor.allow_mipmaps &&
                           descriptor.size.width > 512 &&
                           descriptor.size.height > 512 &&
                           !self.texture_cache.is_allowed_in_shared_cache(
                            TextureFilter::Linear,
                            &descriptor,
                        ) {
                            TextureFilter::Trilinear
                        } else {
                            TextureFilter::Linear
                        }
                    }
                };

                let eviction = if image_template.data.is_blob() {
                    entry.manual_eviction = true;
                    Eviction::Manual
                } else {
                    Eviction::Auto
                };

                //Note: at this point, the dirty rectangle is local to the descriptor space
                self.texture_cache.update(
                    &mut entry.texture_cache_handle,
                    descriptor,
                    filter,
                    Some(image_data),
                    [0.0; 3],
                    dirty_rect,
                    gpu_cache,
                    None,
                    UvRectKind::Rect,
                    eviction,
                );
            }
        }
    }

    pub fn end_frame(&mut self, texture_cache_profile: &mut TextureCacheProfileCounters) {
        debug_assert_eq!(self.state, State::QueryResources);
        self.state = State::Idle;
        self.texture_cache.end_frame(texture_cache_profile);
        self.dirty_image_keys.clear();
    }

    pub fn set_debug_flags(&mut self, flags: DebugFlags) {
        self.texture_cache.set_debug_flags(flags);
    }

    pub fn clear(&mut self, what: ClearCache) {
        if what.contains(ClearCache::IMAGES) {
            for (_key, mut cached) in self.cached_images.resources.drain() {
                cached.drop_from_cache(&mut self.texture_cache);
            }
        }
        if what.contains(ClearCache::GLYPHS) {
            self.cached_glyphs.clear();
        }
        if what.contains(ClearCache::GLYPH_DIMENSIONS) {
            self.cached_glyph_dimensions.clear();
        }
        if what.contains(ClearCache::RENDER_TASKS) {
            self.cached_render_tasks.clear();
        }
        if what.contains(ClearCache::TEXTURE_CACHE) {
            self.texture_cache.clear_all();
        }
        if what.contains(ClearCache::RASTERIZED_BLOBS) {
            self.rasterized_blob_images.clear();
        }
    }

    pub fn clear_namespace(&mut self, namespace: IdNamespace) {
        self.clear_images(|k| k.0 == namespace);

        self.resources.font_instances
            .write()
            .unwrap()
            .retain(|key, _| key.0 != namespace);
        for &key in self.resources.font_templates.keys().filter(|key| key.0 == namespace) {
            self.glyph_rasterizer.delete_font(key);
        }
        self.resources
            .font_templates
            .retain(|key, _| key.0 != namespace);
        self.cached_glyphs
            .clear_fonts(&mut self.texture_cache, |font| font.font_key.0 == namespace);

        if let Some(ref mut r) = self.blob_image_handler {
            r.clear_namespace(namespace);
        }
    }

    /// Reports the CPU heap usage of this ResourceCache.
    ///
    /// NB: It would be much better to use the derive(MallocSizeOf) machinery
    /// here, but the Arcs complicate things. The two ways to handle that would
    /// be to either (a) Implement MallocSizeOf manually for the things that own
    /// them and manually avoid double-counting, or (b) Use the "seen this pointer
    /// yet" machinery from the proper malloc_size_of crate. We can do this if/when
    /// more accurate memory reporting on these resources becomes a priority.
    pub fn report_memory(&self, op: VoidPtrToSizeFn) -> MemoryReport {
        let mut report = MemoryReport::default();

        // Measure fonts. We only need the templates here, because the instances
        // don't have big buffers.
        for (_, font) in self.resources.font_templates.iter() {
            if let FontTemplate::Raw(ref raw, _) = font {
                report.fonts += unsafe { op(raw.as_ptr() as *const c_void) };
            }
        }

        // Measure images.
        for (_, image) in self.resources.image_templates.images.iter() {
            report.images += match image.data {
                CachedImageData::Raw(ref v) => unsafe { op(v.as_ptr() as *const c_void) },
                CachedImageData::Blob | CachedImageData::External(..) => 0,
            }
        }

        // Mesure rasterized blobs.
        // TODO(gw): Temporarily disabled while we roll back a crash. We can re-enable
        //           these when that crash is fixed.
        /*
        for (_, image) in self.rasterized_blob_images.iter() {
            let mut accumulate = |b: &RasterizedBlobImage| {
                report.rasterized_blobs += unsafe { op(b.data.as_ptr() as *const c_void) };
            };
            match image {
                RasterizedBlob::Tiled(map) => map.values().for_each(&mut accumulate),
                RasterizedBlob::NonTiled(vec) => vec.iter().for_each(&mut accumulate),
            };
        }
        */

        report
    }

    /// Properly deletes all images matching the predicate.
    fn clear_images<F: Fn(&ImageKey) -> bool>(&mut self, f: F) {
        let keys = self.resources.image_templates.images.keys().filter(|k| f(*k))
            .cloned().collect::<SmallVec<[ImageKey; 16]>>();

        for key in keys {
            self.delete_image_template(key);
        }

        let blob_f = |key: &BlobImageKey| { f(&key.as_image()) };
        debug_assert!(!self.resources.image_templates.images.keys().any(&f));
        debug_assert!(!self.cached_images.resources.keys().any(&f));
        debug_assert!(!self.blob_image_templates.keys().any(&blob_f));
        debug_assert!(!self.rasterized_blob_images.keys().any(&blob_f));
    }
}

impl Drop for ResourceCache {
    fn drop(&mut self) {
        self.clear_images(|_| true);
    }
}

pub fn get_blob_tiling(
    tiling: Option<TileSize>,
    descriptor: &ImageDescriptor,
    max_texture_size: i32,
) -> Option<TileSize> {
    if tiling.is_none() &&
        (descriptor.size.width > max_texture_size ||
         descriptor.size.height > max_texture_size) {
        return Some(DEFAULT_TILE_SIZE);
    }

    tiling
}

#[cfg(any(feature = "capture", feature = "replay"))]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
struct PlainFontTemplate {
    data: String,
    index: u32,
}

#[cfg(any(feature = "capture", feature = "replay"))]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
struct PlainImageTemplate {
    data: String,
    descriptor: ImageDescriptor,
    tiling: Option<TileSize>,
}

#[cfg(any(feature = "capture", feature = "replay"))]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct PlainResources {
    font_templates: FastHashMap<FontKey, PlainFontTemplate>,
    font_instances: FastHashMap<FontInstanceKey, Arc<BaseFontInstance>>,
    image_templates: FastHashMap<ImageKey, PlainImageTemplate>,
}

#[cfg(feature = "capture")]
#[derive(Serialize)]
pub struct PlainCacheRef<'a> {
    current_frame_id: FrameId,
    glyphs: &'a GlyphCache,
    glyph_dimensions: &'a GlyphDimensionsCache,
    images: &'a ImageCache,
    render_tasks: &'a RenderTaskCache,
    textures: &'a TextureCache,
}

#[cfg(feature = "replay")]
#[derive(Deserialize)]
pub struct PlainCacheOwn {
    current_frame_id: FrameId,
    glyphs: GlyphCache,
    glyph_dimensions: GlyphDimensionsCache,
    images: ImageCache,
    render_tasks: RenderTaskCache,
    textures: TextureCache,
}

#[cfg(feature = "replay")]
const NATIVE_FONT: &'static [u8] = include_bytes!("../res/Proggy.ttf");

// This currently only casts the unit but will soon apply an offset
fn to_image_dirty_rect(blob_dirty_rect: &BlobDirtyRect) -> ImageDirtyRect {
    match *blob_dirty_rect {
        DirtyRect::Partial(rect) => DirtyRect::Partial(
            DeviceIntRect {
                origin: DeviceIntPoint::new(rect.origin.x, rect.origin.y),
                size: DeviceIntSize::new(rect.size.width, rect.size.height),
            }
        ),
        DirtyRect::All => DirtyRect::All,
    }
}

impl ResourceCache {
    #[cfg(feature = "capture")]
    pub fn save_capture(
        &mut self, root: &PathBuf
    ) -> (PlainResources, Vec<ExternalCaptureImage>) {
        use std::fs;
        use std::io::Write;

        info!("saving resource cache");
        let res = &self.resources;
        let path_fonts = root.join("fonts");
        if !path_fonts.is_dir() {
            fs::create_dir(&path_fonts).unwrap();
        }
        let path_images = root.join("images");
        if !path_images.is_dir() {
            fs::create_dir(&path_images).unwrap();
        }
        let path_blobs = root.join("blobs");
        if !path_blobs.is_dir() {
            fs::create_dir(&path_blobs).unwrap();
        }
        let path_externals = root.join("externals");
        if !path_externals.is_dir() {
            fs::create_dir(&path_externals).unwrap();
        }

        info!("\tfont templates");
        let mut font_paths = FastHashMap::default();
        for template in res.font_templates.values() {
            let data: &[u8] = match *template {
                FontTemplate::Raw(ref arc, _) => arc,
                FontTemplate::Native(_) => continue,
            };
            let font_id = res.font_templates.len() + 1;
            let entry = match font_paths.entry(data.as_ptr()) {
                Entry::Occupied(_) => continue,
                Entry::Vacant(e) => e,
            };
            let file_name = format!("{}.raw", font_id);
            let short_path = format!("fonts/{}", file_name);
            fs::File::create(path_fonts.join(file_name))
                .expect(&format!("Unable to create {}", short_path))
                .write_all(data)
                .unwrap();
            entry.insert(short_path);
        }

        info!("\timage templates");
        let mut image_paths = FastHashMap::default();
        let mut other_paths = FastHashMap::default();
        let mut num_blobs = 0;
        let mut external_images = Vec::new();
        for (&key, template) in res.image_templates.images.iter() {
            let desc = &template.descriptor;
            match template.data {
                CachedImageData::Raw(ref arc) => {
                    let image_id = image_paths.len() + 1;
                    let entry = match image_paths.entry(arc.as_ptr()) {
                        Entry::Occupied(_) => continue,
                        Entry::Vacant(e) => e,
                    };

                    #[cfg(feature = "png")]
                    CaptureConfig::save_png(
                        root.join(format!("images/{}.png", image_id)),
                        desc.size,
                        desc.format,
                        &arc,
                    );
                    let file_name = format!("{}.raw", image_id);
                    let short_path = format!("images/{}", file_name);
                    fs::File::create(path_images.join(file_name))
                        .expect(&format!("Unable to create {}", short_path))
                        .write_all(&*arc)
                        .unwrap();
                    entry.insert(short_path);
                }
                CachedImageData::Blob => {
                    let blob_request_params = &[
                        BlobImageParams {
                            request: BlobImageRequest {
                                key: BlobImageKey(key),
                                //TODO: support tiled blob images
                                // https://github.com/servo/webrender/issues/2236
                                tile: template.tiling.map(|_| TileOffset::origin()),
                            },
                            descriptor: BlobImageDescriptor {
                                rect: blob_size(desc.size).into(),
                                format: desc.format,
                            },
                            dirty_rect: DirtyRect::All,
                        }
                    ];

                    let requires_tiling = match template.tiling {
                        Some(tile_size) => desc.size.width > tile_size as i32 || desc.size.height > tile_size as i32,
                        None => false,
                    };

                    let result = if requires_tiling {
                        warn!("Tiled blob images aren't supported yet");
                        RasterizedBlobImage {
                            rasterized_rect: desc.size.into(),
                            data: Arc::new(vec![0; desc.compute_total_size() as usize])
                        }
                    } else {
                        let blob_handler = self.blob_image_handler.as_mut().unwrap();
                        blob_handler.prepare_resources(&self.resources, blob_request_params);
                        let mut rasterizer = blob_handler.create_blob_rasterizer();
                        let (_, result) = rasterizer.rasterize(blob_request_params, false).pop().unwrap();
                        result.expect("Blob rasterization failed")
                    };

                    assert_eq!(result.rasterized_rect.size, desc.size);
                    assert_eq!(result.data.len(), desc.compute_total_size() as usize);

                    num_blobs += 1;
                    #[cfg(feature = "png")]
                    CaptureConfig::save_png(
                        root.join(format!("blobs/{}.png", num_blobs)),
                        desc.size,
                        desc.format,
                        &result.data,
                    );
                    let file_name = format!("{}.raw", num_blobs);
                    let short_path = format!("blobs/{}", file_name);
                    let full_path = path_blobs.clone().join(&file_name);
                    fs::File::create(full_path)
                        .expect(&format!("Unable to create {}", short_path))
                        .write_all(&result.data)
                        .unwrap();
                    other_paths.insert(key, short_path);
                }
                CachedImageData::External(ref ext) => {
                    let short_path = format!("externals/{}", external_images.len() + 1);
                    other_paths.insert(key, short_path.clone());
                    external_images.push(ExternalCaptureImage {
                        short_path,
                        descriptor: desc.clone(),
                        external: ext.clone(),
                    });
                }
            }
        }

        let resources = PlainResources {
            font_templates: res.font_templates
                .iter()
                .map(|(key, template)| {
                    (*key, match *template {
                        FontTemplate::Raw(ref arc, index) => {
                            PlainFontTemplate {
                                data: font_paths[&arc.as_ptr()].clone(),
                                index,
                            }
                        }
                        #[cfg(not(target_os = "macos"))]
                        FontTemplate::Native(ref native) => {
                            PlainFontTemplate {
                                data: native.path.to_string_lossy().to_string(),
                                index: native.index,
                            }
                        }
                        #[cfg(target_os = "macos")]
                        FontTemplate::Native(ref native) => {
                            PlainFontTemplate {
                                data: native.0.postscript_name().to_string(),
                                index: 0,
                            }
                        }
                    })
                })
                .collect(),
            font_instances: res.font_instances.read().unwrap().clone(),
            image_templates: res.image_templates.images
                .iter()
                .map(|(key, template)| {
                    (*key, PlainImageTemplate {
                        data: match template.data {
                            CachedImageData::Raw(ref arc) => image_paths[&arc.as_ptr()].clone(),
                            _ => other_paths[key].clone(),
                        },
                        descriptor: template.descriptor.clone(),
                        tiling: template.tiling,
                    })
                })
                .collect(),
        };

        (resources, external_images)
    }

    #[cfg(feature = "capture")]
    pub fn save_caches(&self, _root: &PathBuf) -> PlainCacheRef {
        PlainCacheRef {
            current_frame_id: self.current_frame_id,
            glyphs: &self.cached_glyphs,
            glyph_dimensions: &self.cached_glyph_dimensions,
            images: &self.cached_images,
            render_tasks: &self.cached_render_tasks,
            textures: &self.texture_cache,
        }
    }

    #[cfg(feature = "replay")]
    pub fn load_capture(
        &mut self,
        resources: PlainResources,
        caches: Option<PlainCacheOwn>,
        root: &PathBuf,
    ) -> Vec<PlainExternalImage> {
        use std::{fs, path::Path};

        info!("loading resource cache");
        //TODO: instead of filling the local path to Arc<data> map as we process
        // each of the resource types, we could go through all of the local paths
        // and fill out the map as the first step.
        let mut raw_map = FastHashMap::<String, Arc<Vec<u8>>>::default();

        self.clear(ClearCache::all());
        self.clear_images(|_| true);

        match caches {
            Some(cached) => {
                self.current_frame_id = cached.current_frame_id;
                self.cached_glyphs = cached.glyphs;
                self.cached_glyph_dimensions = cached.glyph_dimensions;
                self.cached_images = cached.images;
                self.cached_render_tasks = cached.render_tasks;
                self.texture_cache = cached.textures;
            }
            None => {
                self.current_frame_id = FrameId::INVALID;
                self.texture_cache = TextureCache::new(
                    self.texture_cache.max_texture_size(),
                    self.texture_cache.max_texture_layers(),
                    &self.texture_cache.picture_tile_sizes(),
                    DeviceIntSize::zero(),
                    self.texture_cache.color_formats(),
                    self.texture_cache.swizzle_settings(),
                );
            }
        }

        self.glyph_rasterizer.reset();
        let res = &mut self.resources;
        res.font_templates.clear();
        *res.font_instances.write().unwrap() = resources.font_instances;
        res.image_templates.images.clear();

        info!("\tfont templates...");
        let native_font_replacement = Arc::new(NATIVE_FONT.to_vec());
        for (key, plain_template) in resources.font_templates {
            let arc = match raw_map.entry(plain_template.data) {
                Entry::Occupied(e) => {
                    e.get().clone()
                }
                Entry::Vacant(e) => {
                    let file_path = if Path::new(e.key()).is_absolute() {
                        PathBuf::from(e.key())
                    } else {
                        root.join(e.key())
                    };
                    let arc = match fs::read(file_path) {
                        Ok(buffer) => Arc::new(buffer),
                        Err(err) => {
                            error!("Unable to open font template {:?}: {:?}", e.key(), err);
                            Arc::clone(&native_font_replacement)
                        }
                    };
                    e.insert(arc).clone()
                }
            };

            let template = FontTemplate::Raw(arc, plain_template.index);
            self.glyph_rasterizer.add_font(key, template.clone());
            res.font_templates.insert(key, template);
        }

        info!("\timage templates...");
        let mut external_images = Vec::new();
        for (key, template) in resources.image_templates {
            let data = match CaptureConfig::deserialize::<PlainExternalImage, _>(root, &template.data) {
                Some(plain) => {
                    let ext_data = plain.external;
                    external_images.push(plain);
                    CachedImageData::External(ext_data)
                }
                None => {
                    let arc = match raw_map.entry(template.data) {
                        Entry::Occupied(e) => {
                            e.get().clone()
                        }
                        Entry::Vacant(e) => {
                            let buffer = fs::read(root.join(e.key()))
                                .expect(&format!("Unable to open {}", e.key()));
                            e.insert(Arc::new(buffer))
                                .clone()
                        }
                    };
                    CachedImageData::Raw(arc)
                }
            };

            res.image_templates.images.insert(key, ImageResource {
                data,
                descriptor: template.descriptor,
                tiling: template.tiling,
            });
        }

        external_images
    }
}

/// For now the blob's coordinate space have the same pixel sizes as the
/// rendered texture's device space (only a translation is applied).
/// So going from one to the other is only a matter of casting away the unit
/// for sizes.
#[inline]
fn blob_size(device_size: DeviceIntSize) -> LayoutIntSize {
    size2(device_size.width, device_size.height)
}
