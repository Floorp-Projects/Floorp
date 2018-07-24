/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{AddFont, BlobImageResources, AsyncBlobImageRasterizer, ResourceUpdate};
use api::{BlobImageDescriptor, BlobImageHandler, BlobImageRequest};
use api::{ClearCache, ColorF, DevicePoint, DeviceUintPoint, DeviceUintRect, DeviceUintSize};
use api::{FontInstanceKey, FontKey, FontTemplate, GlyphIndex};
use api::{ExternalImageData, ExternalImageType, BlobImageResult, BlobImageParams};
use api::{FontInstanceOptions, FontInstancePlatformOptions, FontVariation};
use api::{GlyphDimensions, IdNamespace};
use api::{ImageData, ImageDescriptor, ImageKey, ImageRendering};
use api::{TileOffset, TileSize, TileRange, NormalizedRect, BlobImageData};
use app_units::Au;
#[cfg(feature = "capture")]
use capture::ExternalCaptureImage;
#[cfg(feature = "replay")]
use capture::PlainExternalImage;
#[cfg(any(feature = "replay", feature = "png"))]
use capture::CaptureConfig;
use device::TextureFilter;
use euclid::{point2, size2};
use glyph_cache::GlyphCache;
#[cfg(not(feature = "pathfinder"))]
use glyph_cache::GlyphCacheEntry;
use glyph_rasterizer::{FontInstance, GlyphFormat, GlyphKey, GlyphRasterizer};
use gpu_cache::{GpuCache, GpuCacheAddress, GpuCacheHandle};
use gpu_types::UvRectKind;
use image::{compute_tile_range, for_each_tile_in_range};
use internal_types::{FastHashMap, FastHashSet, SourceTexture, TextureUpdateList};
use profiler::{ResourceProfileCounters, TextureCacheProfileCounters};
use render_backend::FrameId;
use render_task::{RenderTaskCache, RenderTaskCacheKey, RenderTaskId};
use render_task::{RenderTaskCacheEntry, RenderTaskCacheEntryHandle, RenderTaskTree};
use std::collections::hash_map::Entry::{self, Occupied, Vacant};
use std::collections::hash_map::ValuesMut;
use std::{cmp, mem};
use std::fmt::Debug;
use std::hash::Hash;
#[cfg(any(feature = "capture", feature = "replay"))]
use std::path::PathBuf;
use std::sync::{Arc, RwLock};
use texture_cache::{TextureCache, TextureCacheHandle};
use tiling::SpecialRenderPasses;

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
pub struct CacheItem {
    pub texture_id: SourceTexture,
    pub uv_rect_handle: GpuCacheHandle,
    pub uv_rect: DeviceUintRect,
    pub texture_layer: i32,
}

impl CacheItem {
    pub fn invalid() -> Self {
        CacheItem {
            texture_id: SourceTexture::Invalid,
            uv_rect_handle: GpuCacheHandle::new(),
            uv_rect: DeviceUintRect::zero(),
            texture_layer: 0,
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
struct RasterizedBlobImage {
    data: FastHashMap<Option<TileOffset>, BlobImageResult>,
}

/// Pre scene building state.
/// We use this to generate the async blob rendering requests.
struct BlobImageTemplate {
    descriptor: ImageDescriptor,
    tiling: Option<TileSize>,
    dirty_rect: Option<DeviceUintRect>,
    viewport_tiles: Option<TileRange>,
}

struct ImageResource {
    data: ImageData,
    descriptor: ImageDescriptor,
    tiling: Option<TileSize>,
    viewport_tiles: Option<TileRange>,
}

#[derive(Clone, Debug)]
pub struct ImageTiling {
    pub image_size: DeviceUintSize,
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
    dirty_rect: Option<DeviceUintRect>,
}

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct ResourceClassCache<K: Hash + Eq, V, U: Default> {
    resources: FastHashMap<K, V>,
    pub user_data: U,
}

pub fn intersect_for_tile(
    dirty: DeviceUintRect,
    clipped_tile_size: DeviceUintSize,
    tile_size: TileSize,
    tile_offset: TileOffset,

) -> Option<DeviceUintRect> {
    dirty.intersection(&DeviceUintRect::new(
        DeviceUintPoint::new(
            tile_offset.x as u32 * tile_size as u32,
            tile_offset.y as u32 * tile_size as u32
        ),
        clipped_tile_size,
    )).map(|mut r| {
        // we can't translate by a negative size so do it manually
        r.origin.x -= tile_offset.x as u32 * tile_size as u32;
        r.origin.y -= tile_offset.y as u32 * tile_size as u32;
        r
    })
}

fn merge_dirty_rect(
    prev_dirty_rect: &Option<DeviceUintRect>,
    dirty_rect: &Option<DeviceUintRect>,
    descriptor: &ImageDescriptor,
) -> Option<DeviceUintRect> {
    // It is important to never assume an empty dirty rect implies a full reupload here,
    // although we are able to do so elsewhere. We store the descriptor's full rect instead
    // There are update sequences which could cause us to forget the correct dirty regions
    // regions if we cleared the dirty rect when we received None, e.g.:
    //      1) Update with no dirty rect. We want to reupload everything.
    //      2) Update with dirty rect B. We still want to reupload everything, not just B.
    //      3) Perform the upload some time later.
    match (dirty_rect, prev_dirty_rect) {
        (&Some(ref rect), &Some(ref prev_rect)) => Some(rect.union(&prev_rect)),
        (&Some(ref rect), &None) => Some(*rect),
        (&None, _) => Some(descriptor.full_rect()),
    }
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

    pub fn insert(&mut self, key: K, value: V) {
        self.resources.insert(key, value);
    }

    pub fn remove(&mut self, key: &K) {
        self.resources.remove(key);
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

    pub fn values_mut(&mut self) -> ValuesMut<K, V> {
        self.resources.values_mut()
    }

    pub fn clear(&mut self) {
        self.resources.clear();
    }

    fn clear_keys<F>(&mut self, key_fun: F)
    where
        for<'r> F: Fn(&'r &K) -> bool,
    {
        let resources_to_destroy = self.resources
            .keys()
            .filter(&key_fun)
            .cloned()
            .collect::<Vec<_>>();
        for key in resources_to_destroy {
            let _ = self.resources.remove(&key).unwrap();
        }
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
            key: self.key,
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

type ImageCache = ResourceClassCache<ImageKey, ImageResult, ()>;
pub type FontInstanceMap = Arc<RwLock<FastHashMap<FontInstanceKey, FontInstance>>>;

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
    fn get_image(&self, key: ImageKey) -> Option<(&ImageData, &ImageDescriptor)> {
        self.image_templates
            .get(key)
            .map(|resource| (&resource.data, &resource.descriptor))
    }
}

pub type GlyphDimensionsCache = FastHashMap<(FontInstance, GlyphIndex), Option<GlyphDimensions>>;

pub struct ResourceCache {
    cached_glyphs: GlyphCache,
    cached_images: ImageCache,
    cached_render_tasks: RenderTaskCache,

    resources: Resources,
    state: State,
    current_frame_id: FrameId,

    texture_cache: TextureCache,

    // TODO(gw): We should expire (parts of) this cache semi-regularly!
    cached_glyph_dimensions: GlyphDimensionsCache,
    glyph_rasterizer: GlyphRasterizer,

    // The set of images that aren't present or valid in the texture cache,
    // and need to be rasterized and/or uploaded this frame. This includes
    // both blobs and regular images.
    pending_image_requests: FastHashSet<ImageRequest>,

    blob_image_handler: Option<Box<BlobImageHandler>>,
    rasterized_blob_images: FastHashMap<ImageKey, RasterizedBlobImage>,
    blob_image_templates: FastHashMap<ImageKey, BlobImageTemplate>,

    // If while building a frame we encounter blobs that we didn't already
    // rasterize, add them to this list and rasterize them synchronously.
    missing_blob_images: Vec<BlobImageParams>,
    // The rasterizer associated with the current scene.
    blob_image_rasterizer: Option<Box<AsyncBlobImageRasterizer>>,
}

impl ResourceCache {
    pub fn new(
        texture_cache: TextureCache,
        glyph_rasterizer: GlyphRasterizer,
        blob_image_handler: Option<Box<BlobImageHandler>>,
    ) -> Self {
        ResourceCache {
            cached_glyphs: GlyphCache::new(),
            cached_images: ResourceClassCache::new(),
            cached_render_tasks: RenderTaskCache::new(),
            resources: Resources::default(),
            cached_glyph_dimensions: FastHashMap::default(),
            texture_cache,
            state: State::Idle,
            current_frame_id: FrameId(0),
            pending_image_requests: FastHashSet::default(),
            glyph_rasterizer,
            blob_image_handler,
            rasterized_blob_images: FastHashMap::default(),
            blob_image_templates: FastHashMap::default(),
            missing_blob_images: Vec::new(),
            blob_image_rasterizer: None,
        }
    }

    pub fn max_texture_size(&self) -> u32 {
        self.texture_cache.max_texture_size()
    }

    fn should_tile(limit: u32, descriptor: &ImageDescriptor, data: &ImageData) -> bool {
        let size_check = descriptor.size.width > limit || descriptor.size.height > limit;
        match *data {
            ImageData::Raw(_) | ImageData::Blob(_) => size_check,
            ImageData::External(info) => {
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
        render_tasks: &mut RenderTaskTree,
        user_data: Option<[f32; 3]>,
        is_opaque: bool,
        mut f: F,
    ) -> RenderTaskCacheEntryHandle where F: FnMut(&mut RenderTaskTree) -> RenderTaskId {
        self.cached_render_tasks.request_render_task(
            key,
            &mut self.texture_cache,
            gpu_cache,
            render_tasks,
            user_data,
            is_opaque,
            |render_task_tree| Ok(f(render_task_tree))
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
                    self.add_image_template(img.key, img.descriptor, img.data, img.tiling);
                }
                ResourceUpdate::UpdateImage(img) => {
                    self.update_image_template(img.key, img.descriptor, img.data, img.dirty_rect);
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
                ResourceUpdate::SetImageVisibleArea(key, area) => {
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
        let mut new_updates = Vec::with_capacity(updates.len());
        for update in mem::replace(updates, Vec::new()) {
            match update {
                ResourceUpdate::AddImage(ref img) => {
                    if let ImageData::Blob(ref blob_data) = img.data {
                        self.add_blob_image(
                            img.key,
                            &img.descriptor,
                            img.tiling,
                            Arc::clone(blob_data),
                        );
                    }
                }
                ResourceUpdate::UpdateImage(ref img) => {
                    if let ImageData::Blob(ref blob_data) = img.data {
                        self.update_blob_image(
                            img.key,
                            &img.descriptor,
                            &img.dirty_rect,
                            Arc::clone(blob_data)
                        );
                    }
                }
                ResourceUpdate::SetImageVisibleArea(key, area) => {
                    if let Some(template) = self.blob_image_templates.get_mut(&key) {
                        if let Some(tile_size) = template.tiling {
                            template.viewport_tiles = Some(compute_tile_range(
                                &area,
                                &template.descriptor.size,
                                tile_size,
                            ));
                        }
                    }
                }
                _ => {}
            }

            match update {
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
                ResourceUpdate::AddFontInstance(mut instance) => {
                    self.add_font_instance(
                        instance.key,
                        instance.font_key,
                        instance.glyph_size,
                        instance.options,
                        instance.platform_options,
                        instance.variations,
                    );
                }
                other => {
                    new_updates.push(other);
                }
            }
        }

        *updates = new_updates;
    }

    pub fn set_blob_rasterizer(&mut self, rasterizer: Box<AsyncBlobImageRasterizer>) {
        self.blob_image_rasterizer = Some(rasterizer);
    }

    pub fn add_rasterized_blob_images(&mut self, images: Vec<(BlobImageRequest, BlobImageResult)>) {
        for (request, result) in images {
            let image = self.rasterized_blob_images.entry(request.key).or_insert_with(
                || { RasterizedBlobImage { data: FastHashMap::default() } }
            );
            image.data.insert(request.tile, result);
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
            .clear_fonts(|font| font.font_key == font_key);
        if let Some(ref mut r) = self.blob_image_handler {
            r.delete_font(font_key);
        }
    }

    pub fn add_font_instance(
        &mut self,
        instance_key: FontInstanceKey,
        font_key: FontKey,
        glyph_size: Au,
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
        let instance = FontInstance::new(
            font_key,
            glyph_size,
            ColorF::new(0.0, 0.0, 0.0, 1.0),
            bg_color,
            render_mode,
            flags,
            synthetic_italics,
            platform_options,
            variations,
        );
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

    pub fn get_font_instance(&self, instance_key: FontInstanceKey) -> Option<FontInstance> {
        let instance_map = self.resources.font_instances.read().unwrap();
        instance_map.get(&instance_key).cloned()
    }

    pub fn add_image_template(
        &mut self,
        image_key: ImageKey,
        descriptor: ImageDescriptor,
        data: ImageData,
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
            viewport_tiles: None,
        };

        self.resources.image_templates.insert(image_key, resource);
    }

    pub fn update_image_template(
        &mut self,
        image_key: ImageKey,
        descriptor: ImageDescriptor,
        data: ImageData,
        dirty_rect: Option<DeviceUintRect>,
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
                entry.dirty_rect = merge_dirty_rect(&entry.dirty_rect, &dirty_rect, &descriptor);
            }
            Some(&mut ImageResult::Multi(ref mut entries)) => {
                for entry in entries.values_mut() {
                    entry.dirty_rect = merge_dirty_rect(&entry.dirty_rect, &dirty_rect, &descriptor);
                }
            }
            _ => {}
        }

        *image = ImageResource {
            descriptor,
            data,
            tiling,
            viewport_tiles: image.viewport_tiles,
        };
    }

    // Happens before scene building.
    pub fn add_blob_image(
        &mut self,
        key: ImageKey,
        descriptor: &ImageDescriptor,
        mut tiling: Option<TileSize>,
        data: Arc<BlobImageData>,
    ) {
        let max_texture_size = self.max_texture_size();
        tiling = get_blob_tiling(tiling, descriptor, max_texture_size);

        self.blob_image_handler.as_mut().unwrap().add(key, data, tiling);

        self.blob_image_templates.insert(
            key,
            BlobImageTemplate {
                descriptor: *descriptor,
                tiling,
                dirty_rect: Some(
                    DeviceUintRect::new(
                        DeviceUintPoint::zero(),
                        descriptor.size,
                    )
                ),
                viewport_tiles: None,
            },
        );
    }

    // Happens before scene building.
    pub fn update_blob_image(
        &mut self,
        key: ImageKey,
        descriptor: &ImageDescriptor,
        dirty_rect: &Option<DeviceUintRect>,
        data: Arc<BlobImageData>,
    ) {
        self.blob_image_handler.as_mut().unwrap().update(key, data, *dirty_rect);

        let max_texture_size = self.max_texture_size();

        let image = self.blob_image_templates
            .get_mut(&key)
            .expect("Attempt to update non-existent blob image");

        let tiling = get_blob_tiling(image.tiling, descriptor, max_texture_size);

        *image = BlobImageTemplate {
            descriptor: *descriptor,
            tiling,
            dirty_rect: match (*dirty_rect, image.dirty_rect) {
                (Some(rect), Some(prev_rect)) => Some(rect.union(&prev_rect)),
                (Some(rect), None) => Some(rect),
                (None, _) => None,
            },
            viewport_tiles: image.viewport_tiles,
        };
    }

    pub fn delete_image_template(&mut self, image_key: ImageKey) {
        let value = self.resources.image_templates.remove(image_key);

        self.cached_images.remove(&image_key);

        match value {
            Some(image) => if image.data.is_blob() {
                self.blob_image_handler.as_mut().unwrap().delete(image_key);
                self.blob_image_templates.remove(&image_key);
                self.rasterized_blob_images.remove(&image_key);
            },
            None => {
                warn!("Delete the non-exist key");
                debug!("key={:?}", image_key);
            }
        }
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
                                   |tile_size| tile_size as u32);
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
                                texture_cache_handle: TextureCacheHandle::new(),
                                dirty_rect: None,
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
                        texture_cache_handle: TextureCacheHandle::new(),
                        dirty_rect: Some(template.descriptor.full_rect()),
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
                        texture_cache_handle: TextureCacheHandle::new(),
                        dirty_rect: Some(template.descriptor.full_rect()),
                    })
            },
            ImageResult::Err(_) => panic!("Errors should already have been handled"),
        };

        let needs_upload = self.texture_cache.request(&entry.texture_cache_handle, gpu_cache);

        if !needs_upload && entry.dirty_rect.is_none() {
            return
        }

        if !self.pending_image_requests.insert(request) {
            return
        }

        if template.data.is_blob() {
            let request: BlobImageRequest = request.into();
            let missing = match self.rasterized_blob_images.get(&request.key) {
                Some(img) => !img.data.contains_key(&request.tile),
                None => true,
            };

            // For some reason the blob image is missing. We'll fall back to
            // rasterizing it on the render backend thread.
            if missing {
                let descriptor = match template.tiling {
                    Some(tile_size) => {
                        let tile = request.tile.unwrap();
                        BlobImageDescriptor {
                            offset: DevicePoint::new(
                                tile.x as f32 * tile_size as f32,
                                tile.y as f32 * tile_size as f32,
                            ),
                            size: compute_tile_size(
                                &template.descriptor,
                                tile_size,
                                tile,
                            ),
                            format: template.descriptor.format,
                        }
                    }
                    None => {
                        BlobImageDescriptor {
                            offset: DevicePoint::origin(),
                            size: template.descriptor.size,
                            format: template.descriptor.format,
                        }
                    }
                };

                self.missing_blob_images.push(
                    BlobImageParams {
                        request,
                        descriptor,
                        dirty_rect: None,
                    }
                );
            }
        }
    }

    pub fn create_blob_scene_builder_requests(
        &mut self,
        keys: &[ImageKey]
    ) -> (Option<Box<AsyncBlobImageRasterizer>>, Vec<BlobImageParams>) {
        if self.blob_image_handler.is_none() {
            return (None, Vec::new());
        }

        let mut blob_request_params = Vec::new();
        for key in keys {
            let template = self.blob_image_templates.get_mut(key).unwrap();

            if let Some(tile_size) = template.tiling {
                // If we know that only a portion of the blob image is in the viewport,
                // only request these visible tiles since blob images can be huge.
                let mut tiles = template.viewport_tiles.unwrap_or_else(|| {
                    // Default to requesting the full range of tiles.
                    compute_tile_range(
                        &NormalizedRect {
                            origin: point2(0.0, 0.0),
                            size: size2(1.0, 1.0),
                        },
                        &template.descriptor.size,
                        tile_size,
                    )
                });

                // Don't request tiles that weren't invalidated.
                if let Some(dirty_rect) = template.dirty_rect {
                    let f32_size = template.descriptor.size.to_f32();
                    let normalized_dirty_rect = NormalizedRect {
                        origin: point2(
                            dirty_rect.origin.x as f32 / f32_size.width,
                            dirty_rect.origin.y as f32 / f32_size.height,
                        ),
                        size: size2(
                            dirty_rect.size.width as f32 / f32_size.width,
                            dirty_rect.size.height as f32 / f32_size.height,
                        ),
                    };
                    let dirty_tiles = compute_tile_range(
                        &normalized_dirty_rect,
                        &template.descriptor.size,
                        tile_size,
                    );

                    tiles = tiles.intersection(&dirty_tiles).unwrap_or(TileRange::zero());
                }

                // This code tries to keep things sane if Gecko sends
                // nonsensical blob image requests.
                // Constant here definitely needs to be tweaked.
                const MAX_TILES_PER_REQUEST: u32 = 64;
                while tiles.size.width as u32 * tiles.size.height as u32 > MAX_TILES_PER_REQUEST {
                    // Remove tiles in the largest dimension.
                    if tiles.size.width > tiles.size.height {
                        tiles.size.width -= 2;
                        tiles.origin.x += 1;
                    } else {
                        tiles.size.height -= 2;
                        tiles.origin.y += 1;
                    }
                }

                for_each_tile_in_range(&tiles, &mut|tile| {
                    let descriptor = BlobImageDescriptor {
                        offset: DevicePoint::new(
                            tile.x as f32 * tile_size as f32,
                            tile.y as f32 * tile_size as f32,
                        ),
                        size: compute_tile_size(
                            &template.descriptor,
                            tile_size,
                            tile,
                        ),
                        format: template.descriptor.format,
                    };

                    blob_request_params.push(
                        BlobImageParams {
                            request: BlobImageRequest {
                                key: *key,
                                tile: Some(tile),
                            },
                            descriptor,
                            dirty_rect: None,
                        }
                    );
                });
            } else {
                // TODO: to support partial rendering of non-tiled blobs we
                // need to know that the current version of the blob is uploaded
                // to the texture cache and get the guarantee that it will not
                // get evicted by the time the updated blob is rasterized and
                // uploaded.
                // Alternatively we could make it the responsibility of the blob
                // renderer to always output the full image. This could be based
                // a similar copy-on-write mechanism as gecko tiling.
                blob_request_params.push(
                    BlobImageParams {
                        request: BlobImageRequest {
                            key: *key,
                            tile: None,
                        },
                        descriptor: BlobImageDescriptor {
                            offset: DevicePoint::zero(),
                            size: template.descriptor.size,
                            format: template.descriptor.format,
                        },
                        dirty_rect: None,
                    }
                );
            }
            template.dirty_rect = None;
        }
        let handler = self.blob_image_handler.as_mut().unwrap();
        handler.prepare_resources(&self.resources, &blob_request_params);
        (Some(handler.create_blob_rasterizer()), blob_request_params)
    }

    fn discard_tiles_outside_visible_area(
        &mut self,
        key: ImageKey,
        area: &NormalizedRect
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
        let image = match self.rasterized_blob_images.get_mut(&key) {
            Some(image) => image,
            None => {
                //println!("Missing rasterized blob (key={:?})!", key);
                return;
            }
        };
        let tile_range = compute_tile_range(
            &area,
            &template.descriptor.size,
            tile_size,
        );
        image.data.retain(|tile, _| {
            match *tile {
                Some(offset) => tile_range.contains(&offset),
                // This would be a bug. If we get here the blob should be tiled.
                None => {
                    error!("Blob image template and image data tiling don't match.");
                    false
                }
            }
        });
    }

    pub fn request_glyphs(
        &mut self,
        mut font: FontInstance,
        glyph_keys: &[GlyphKey],
        gpu_cache: &mut GpuCache,
        render_task_tree: &mut RenderTaskTree,
        render_passes: &mut SpecialRenderPasses,
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
            render_passes,
        );
    }

    pub fn pending_updates(&mut self) -> TextureUpdateList {
        self.texture_cache.pending_updates()
    }

    #[cfg(feature = "pathfinder")]
    pub fn fetch_glyphs<F>(
        &self,
        mut font: FontInstance,
        glyph_keys: &[GlyphKey],
        fetch_buffer: &mut Vec<GlyphFetchResult>,
        gpu_cache: &mut GpuCache,
        mut f: F,
    ) where
        F: FnMut(SourceTexture, GlyphFormat, &[GlyphFetchResult]),
    {
        debug_assert_eq!(self.state, State::QueryResources);

        self.glyph_rasterizer.prepare_font(&mut font);

        let mut current_texture_id = SourceTexture::Invalid;
        let mut current_glyph_format = GlyphFormat::Subpixel;
        debug_assert!(fetch_buffer.is_empty());

        for (loop_index, key) in glyph_keys.iter().enumerate() {
           let (cache_item, glyph_format) =
                match self.glyph_rasterizer.get_cache_item_for_glyph(key,
                                                                     &font,
                                                                     &self.cached_glyphs,
                                                                     &self.texture_cache,
                                                                     &self.cached_render_tasks) {
                    None => continue,
                    Some(result) => result,
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

    #[cfg(not(feature = "pathfinder"))]
    pub fn fetch_glyphs<F>(
        &self,
        mut font: FontInstance,
        glyph_keys: &[GlyphKey],
        fetch_buffer: &mut Vec<GlyphFetchResult>,
        gpu_cache: &mut GpuCache,
        mut f: F,
    ) where
        F: FnMut(SourceTexture, GlyphFormat, &[GlyphFetchResult]),
    {
        debug_assert_eq!(self.state, State::QueryResources);

        self.glyph_rasterizer.prepare_font(&mut font);
        let glyph_key_cache = self.cached_glyphs.get_glyph_key_cache_for_font(&font);

        let mut current_texture_id = SourceTexture::Invalid;
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
        match self.cached_glyph_dimensions.entry((font.clone(), glyph_index)) {
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
    pub fn get_cached_image(
        &self,
        request: ImageRequest,
    ) -> Result<CacheItem, ()> {
        debug_assert_eq!(self.state, State::QueryResources);

        // TODO(Jerry): add a debug option to visualize the corresponding area for
        // the Err() case of CacheItem.
        match *self.cached_images.get(&request.key) {
            ImageResult::UntiledAuto(ref image_info) => {
                Ok(self.texture_cache.get(&image_info.texture_cache_handle))
            }
            ImageResult::Multi(ref entries) => {
                let image_info = entries.get(&request.into());
                Ok(self.texture_cache.get(&image_info.texture_cache_handle))
            }
            ImageResult::Err(_) => {
                Err(())
            }
        }
    }

    pub fn get_cached_render_task(
        &self,
        handle: &RenderTaskCacheEntryHandle,
    ) -> &RenderTaskCacheEntry {
        self.cached_render_tasks.get_cache_entry(handle)
    }

    pub fn get_texture_cache_item(&self, handle: &TextureCacheHandle) -> CacheItem {
        self.texture_cache.get(handle)
    }

    pub fn get_image_properties(&self, image_key: ImageKey) -> Option<ImageProperties> {
        let image_template = &self.resources.image_templates.get(image_key);

        image_template.map(|image_template| {
            let external_image = match image_template.data {
                ImageData::External(ext_image) => match ext_image.image_type {
                    ExternalImageType::TextureHandle(_) => Some(ext_image),
                    // external buffer uses resource_cache.
                    ExternalImageType::Buffer => None,
                },
                // raw and blob image are all using resource_cache.
                ImageData::Raw(..) | ImageData::Blob(..) => None,
            };

            ImageProperties {
                descriptor: image_template.descriptor,
                external_image,
                tiling: image_template.tiling,
            }
        })
    }

    pub fn begin_frame(&mut self, frame_id: FrameId) {
        debug_assert_eq!(self.state, State::Idle);
        self.state = State::AddResources;
        self.texture_cache.begin_frame(frame_id);
        self.cached_glyphs.begin_frame(&self.texture_cache, &self.cached_render_tasks);
        self.cached_render_tasks.begin_frame(&mut self.texture_cache);
        self.current_frame_id = frame_id;
    }

    pub fn block_until_all_resources_added(
        &mut self,
        gpu_cache: &mut GpuCache,
        render_tasks: &mut RenderTaskTree,
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
        render_tasks.prepare_for_render();
        self.cached_render_tasks.update(
            gpu_cache,
            &mut self.texture_cache,
            render_tasks,
        );
        self.texture_cache.end_frame(texture_cache_profile);
    }

    fn rasterize_missing_blob_images(&mut self) {
        if self.missing_blob_images.is_empty() {
            return;
        }

        self.blob_image_handler
            .as_mut()
            .unwrap()
            .prepare_resources(&self.resources, &self.missing_blob_images);

        let rasterized_blobs = self.blob_image_rasterizer
            .as_mut()
            .unwrap()
            .rasterize(&self.missing_blob_images);

        self.add_rasterized_blob_images(rasterized_blobs);

        self.missing_blob_images.clear();
    }

    fn update_texture_cache(&mut self, gpu_cache: &mut GpuCache) {
        for request in self.pending_image_requests.drain() {
            let image_template = self.resources.image_templates.get_mut(request.key).unwrap();
            debug_assert!(image_template.data.uses_texture_cache());

            let image_data = match image_template.data {
                ImageData::Raw(..) | ImageData::External(..) => {
                    // Safe to clone here since the Raw image data is an
                    // Arc, and the external image data is small.
                    image_template.data.clone()
                }
                ImageData::Blob(..) => {
                    let blob_image = self.rasterized_blob_images.get(&request.key).unwrap();
                    match blob_image.data.get(&request.tile) {
                        Some(result) => {
                            let result = result
                                .as_ref()
                                .expect("Failed to render a blob image");

                            // TODO: we may want to not panic and show a placeholder instead.

                            ImageData::Raw(Arc::clone(&result.data))
                        }
                        None => {
                            debug_assert!(false, "invalid blob image request during frame building");
                            continue;
                        }
                    }
                }
            };

            let entry = match *self.cached_images.get_mut(&request.key) {
                ImageResult::UntiledAuto(ref mut entry) => entry,
                ImageResult::Multi(ref mut entries) => entries.get_mut(&request.into()),
                ImageResult::Err(_) => panic!("Update requested for invalid entry")
            };
            let mut descriptor = image_template.descriptor.clone();
            let local_dirty_rect;

            if let Some(tile) = request.tile {
                let tile_size = image_template.tiling.unwrap();
                let clipped_tile_size = compute_tile_size(&descriptor, tile_size, tile);

                local_dirty_rect = if let Some(rect) = entry.dirty_rect.take() {
                    // We should either have a dirty rect, or we are re-uploading where the dirty
                    // rect is ignored anyway.
                    let intersection = intersect_for_tile(rect, clipped_tile_size, tile_size, tile);
                    debug_assert!(intersection.is_some() ||
                                  self.texture_cache.needs_upload(&entry.texture_cache_handle));
                    intersection
                } else {
                    None
                };

                // The tiled image could be stored on the CPU as one large image or be
                // already broken up into tiles. This affects the way we compute the stride
                // and offset.
                let tiled_on_cpu = image_template.data.is_blob();
                if !tiled_on_cpu {
                    let bpp = descriptor.format.bytes_per_pixel();
                    let stride = descriptor.compute_stride();
                    descriptor.stride = Some(stride);
                    descriptor.offset +=
                        tile.y as u32 * tile_size as u32 * stride +
                        tile.x as u32 * tile_size as u32 * bpp;
                }

                descriptor.size = clipped_tile_size;
            } else {
                local_dirty_rect = entry.dirty_rect.take();
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

            //Note: at this point, the dirty rectangle is local to the descriptor space
            self.texture_cache.update(
                &mut entry.texture_cache_handle,
                descriptor,
                filter,
                Some(image_data),
                [0.0; 3],
                local_dirty_rect,
                gpu_cache,
                None,
                UvRectKind::Rect,
            );
        }
    }

    pub fn end_frame(&mut self) {
        debug_assert_eq!(self.state, State::QueryResources);
        self.state = State::Idle;
    }

    pub fn clear(&mut self, what: ClearCache) {
        if what.contains(ClearCache::IMAGES) {
            self.cached_images.clear();
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
            self.texture_cache.clear();
        }
    }

    pub fn clear_namespace(&mut self, namespace: IdNamespace) {
        self.resources
            .image_templates
            .images
            .retain(|key, _| key.0 != namespace);
        self.cached_images
            .clear_keys(|key| key.0 == namespace);

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
            .clear_fonts(|font| font.font_key.0 == namespace);

        if let Some(ref mut r) = self.blob_image_handler {
            r.clear_namespace(namespace);
        }
    }
}

pub fn get_blob_tiling(
    tiling: Option<TileSize>,
    descriptor: &ImageDescriptor,
    max_texture_size: u32,
) -> Option<TileSize> {
    if tiling.is_none() &&
        (descriptor.size.width > max_texture_size ||
         descriptor.size.height > max_texture_size) {
        return Some(DEFAULT_TILE_SIZE);
    }

    tiling
}


// Compute the width and height of a tile depending on its position in the image.
pub fn compute_tile_size(
    descriptor: &ImageDescriptor,
    base_size: TileSize,
    tile: TileOffset,
) -> DeviceUintSize {
    let base_size = base_size as u32;
    // Most tiles are going to have base_size as width and height,
    // except for tiles around the edges that are shrunk to fit the mage data
    // (See decompose_tiled_image in frame.rs).
    let actual_width = if (tile.x as u32) < descriptor.size.width / base_size {
        base_size
    } else {
        descriptor.size.width % base_size
    };

    let actual_height = if (tile.y as u32) < descriptor.size.height / base_size {
        base_size
    } else {
        descriptor.size.height % base_size
    };

    size2(actual_width, actual_height)
}

#[cfg(any(feature = "capture", feature = "replay"))]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
enum PlainFontTemplate {
    Raw {
        data: String,
        index: u32,
    },
    Native,
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
    font_instances: FastHashMap<FontInstanceKey, FontInstance>,
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

impl ResourceCache {
    #[cfg(feature = "capture")]
    pub fn save_capture(
        &mut self, root: &PathBuf
    ) -> (PlainResources, Vec<ExternalCaptureImage>) {
        #[cfg(feature = "png")]
        use device::ReadPixelsFormat;
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
                ImageData::Raw(ref arc) => {
                    let image_id = image_paths.len() + 1;
                    let entry = match image_paths.entry(arc.as_ptr()) {
                        Entry::Occupied(_) => continue,
                        Entry::Vacant(e) => e,
                    };

                    #[cfg(feature = "png")]
                    CaptureConfig::save_png(
                        root.join(format!("images/{}.png", image_id)),
                        (desc.size.width, desc.size.height),
                        ReadPixelsFormat::Standard(desc.format),
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
                ImageData::Blob(_) => {
                    assert_eq!(template.tiling, None);
                    let blob_request_params = &[
                        BlobImageParams {
                            request: BlobImageRequest {
                                key,
                                //TODO: support tiled blob images
                                // https://github.com/servo/webrender/issues/2236
                                tile: None,
                            },
                            descriptor: BlobImageDescriptor {
                                size: desc.size,
                                offset: DevicePoint::zero(),
                                format: desc.format,
                            },
                            dirty_rect: None,
                        }
                    ];

                    let blob_handler = self.blob_image_handler.as_mut().unwrap();
                    blob_handler.prepare_resources(&self.resources, blob_request_params);
                    let mut rasterizer = blob_handler.create_blob_rasterizer();
                    let (_, result) = rasterizer.rasterize(blob_request_params).pop().unwrap();
                    let result = result.expect("Blob rasterization failed");

                    assert_eq!(result.size, desc.size);
                    assert_eq!(result.data.len(), desc.compute_total_size() as usize);

                    num_blobs += 1;
                    #[cfg(feature = "png")]
                    CaptureConfig::save_png(
                        root.join(format!("blobs/{}.png", num_blobs)),
                        (desc.size.width, desc.size.height),
                        ReadPixelsFormat::Standard(desc.format),
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
                ImageData::External(ref ext) => {
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
                            PlainFontTemplate::Raw {
                                data: font_paths[&arc.as_ptr()].clone(),
                                index,
                            }
                        }
                        FontTemplate::Native(_) => {
                            PlainFontTemplate::Native
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
                            ImageData::Raw(ref arc) => image_paths[&arc.as_ptr()].clone(),
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
        use std::fs::File;
        use std::io::Read;

        info!("loading resource cache");
        //TODO: instead of filling the local path to Arc<data> map as we process
        // each of the resource types, we could go through all of the local paths
        // and fill out the map as the first step.
        let mut raw_map = FastHashMap::<String, Arc<Vec<u8>>>::default();

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
                self.current_frame_id = FrameId(0);
                self.cached_glyphs.clear();
                self.cached_glyph_dimensions.clear();
                self.cached_images.clear();
                self.cached_render_tasks.clear();
                let max_texture_size = self.texture_cache.max_texture_size();
                self.texture_cache = TextureCache::new(max_texture_size);
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
            let template = match plain_template {
                PlainFontTemplate::Raw { data, index } => {
                    let arc = match raw_map.entry(data) {
                        Entry::Occupied(e) => {
                            e.get().clone()
                        }
                        Entry::Vacant(e) => {
                            let mut buffer = Vec::new();
                            File::open(root.join(e.key()))
                                .expect(&format!("Unable to open {}", e.key()))
                                .read_to_end(&mut buffer)
                                .unwrap();
                            e.insert(Arc::new(buffer))
                                .clone()
                        }
                    };
                    FontTemplate::Raw(arc, index)
                }
                PlainFontTemplate::Native => {
                    FontTemplate::Raw(native_font_replacement.clone(), 0)
                }
            };

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
                    ImageData::External(ext_data)
                }
                None => {
                    let arc = match raw_map.entry(template.data) {
                        Entry::Occupied(e) => {
                            e.get().clone()
                        }
                        Entry::Vacant(e) => {
                            let mut buffer = Vec::new();
                            File::open(root.join(e.key()))
                                .expect(&format!("Unable to open {}", e.key()))
                                .read_to_end(&mut buffer)
                                .unwrap();
                            e.insert(Arc::new(buffer))
                                .clone()
                        }
                    };
                    ImageData::Raw(arc)
                }
            };

            res.image_templates.images.insert(key, ImageResource {
                data,
                descriptor: template.descriptor,
                tiling: template.tiling,
                viewport_tiles: None,
            });
        }

        external_images
    }
}
