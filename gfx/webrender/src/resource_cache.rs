/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{AddFont, BlobImageData, BlobImageResources, ResourceUpdate, ResourceUpdates};
use api::{BlobImageDescriptor, BlobImageError, BlobImageRenderer, BlobImageRequest};
use api::{ClearCache, ColorF, DevicePoint, DeviceUintPoint, DeviceUintRect, DeviceUintSize};
use api::{Epoch, FontInstanceKey, FontKey, FontTemplate};
use api::{ExternalImageData, ExternalImageType};
use api::{FontInstanceOptions, FontInstancePlatformOptions, FontVariation};
use api::{GlyphDimensions, GlyphKey, IdNamespace};
use api::{ImageData, ImageDescriptor, ImageKey, ImageRendering};
use api::{TileOffset, TileSize};
use app_units::Au;
#[cfg(feature = "capture")]
use capture::ExternalCaptureImage;
#[cfg(feature = "replay")]
use capture::PlainExternalImage;
#[cfg(any(feature = "replay", feature = "png"))]
use capture::CaptureConfig;
use device::TextureFilter;
use glyph_cache::GlyphCache;
#[cfg(not(feature = "pathfinder"))]
use glyph_cache::GlyphCacheEntry;
use glyph_rasterizer::{FontInstance, GlyphFormat, GlyphRasterizer, GlyphRequest};
use gpu_cache::{GpuCache, GpuCacheAddress, GpuCacheHandle};
use internal_types::{FastHashMap, FastHashSet, SourceTexture, TextureUpdateList};
use profiler::{ResourceProfileCounters, TextureCacheProfileCounters};
use render_backend::FrameId;
use render_task::{RenderTaskCache, RenderTaskCacheKey, RenderTaskId};
use render_task::{RenderTaskCacheEntry, RenderTaskCacheEntryHandle, RenderTaskTree};
use std::collections::hash_map::Entry::{self, Occupied, Vacant};
use std::cmp;
use std::fmt::Debug;
use std::hash::Hash;
use std::mem;
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
    pub epoch: Epoch,
}

#[derive(Debug, Copy, Clone, PartialEq)]
enum State {
    Idle,
    AddResources,
    QueryResources,
}

#[derive(Debug)]
struct ImageResource {
    data: ImageData,
    descriptor: ImageDescriptor,
    epoch: Epoch,
    tiling: Option<TileSize>,
    dirty_rect: Option<DeviceUintRect>,
}

#[derive(Clone, Debug)]
pub struct ImageTiling {
    pub image_size: DeviceUintSize,
    pub tile_size: TileSize,
}

pub type TiledImageMap = FastHashMap<ImageKey, ImageTiling>;

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
    epoch: Epoch,
}

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct ResourceClassCache<K: Hash + Eq, V, U: Default> {
    resources: FastHashMap<K, V>,
    pub user_data: U,
}

fn intersect_for_tile(
    dirty: DeviceUintRect,
    width: u32,
    height: u32,
    tile_size: TileSize,
    tile_offset: TileOffset,

) -> Option<DeviceUintRect> {
        dirty.intersection(&DeviceUintRect::new(
            DeviceUintPoint::new(
                tile_offset.x as u32 * tile_size as u32,
                tile_offset.y as u32 * tile_size as u32
            ),
            DeviceUintSize::new(width, height),
        )).map(|mut r| {
                // we can't translate by a negative size so do it manually
                r.origin.x -= tile_offset.x as u32 * tile_size as u32;
                r.origin.y -= tile_offset.y as u32 * tile_size as u32;
                r
            })
}


impl<K, V, U> ResourceClassCache<K, V, U>
where
    K: Clone + Hash + Eq + Debug,
    U: Default,
{
    pub fn new() -> ResourceClassCache<K, V, U> {
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

    pub fn get_mut(&mut self, key: &K) -> &mut V {
        self.resources.get_mut(key)
            .expect("Didn't find a cached resource with that ID!")
    }

    pub fn entry(&mut self, key: K) -> Entry<K, V> {
        self.resources.entry(key)
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
pub struct ImageRequest {
    pub key: ImageKey,
    pub rendering: ImageRendering,
    pub tile: Option<TileOffset>,
}

impl Into<BlobImageRequest> for ImageRequest {
    fn into(self) -> BlobImageRequest {
        BlobImageRequest {
            key: self.key,
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

type ImageCache = ResourceClassCache<ImageRequest, Result<CachedImageInfo, ImageCacheError>, ()>;
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

pub type GlyphDimensionsCache = FastHashMap<GlyphRequest, Option<GlyphDimensions>>;

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

    blob_image_renderer: Option<Box<BlobImageRenderer>>,
}

impl ResourceCache {
    pub fn new(
        texture_cache: TextureCache,
        glyph_rasterizer: GlyphRasterizer,
        blob_image_renderer: Option<Box<BlobImageRenderer>>,
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
            blob_image_renderer,
        }
    }

    pub fn max_texture_size(&self) -> u32 {
        self.texture_cache.max_texture_size()
    }

    fn should_tile(limit: u32, descriptor: &ImageDescriptor, data: &ImageData) -> bool {
        let size_check = descriptor.width > limit || descriptor.height > limit;
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

    pub fn update_resources(
        &mut self,
        updates: ResourceUpdates,
        profile_counters: &mut ResourceProfileCounters,
    ) {
        // TODO, there is potential for optimization here, by processing updates in
        // bulk rather than one by one (for example by sorting allocations by size or
        // in a way that reduces fragmentation in the atlas).

        for update in updates.updates {
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
                ResourceUpdate::AddFont(font) => match font {
                    AddFont::Raw(id, bytes, index) => {
                        profile_counters.font_templates.inc(bytes.len());
                        self.add_font_template(id, FontTemplate::Raw(Arc::new(bytes), index));
                    }
                    AddFont::Native(id, native_font_handle) => {
                        self.add_font_template(id, FontTemplate::Native(native_font_handle));
                    }
                },
                ResourceUpdate::DeleteFont(font) => {
                    self.delete_font_template(font);
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
                ResourceUpdate::DeleteFontInstance(instance) => {
                    self.delete_font_instance(instance);
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
            .clear_fonts(|font| font.font_key == font_key);
        if let Some(ref mut r) = self.blob_image_renderer {
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
            subpx_dir,
            flags,
            bg_color,
            ..
        } = options.unwrap_or_default();
        let instance = FontInstance::new(
            font_key,
            glyph_size,
            ColorF::new(0.0, 0.0, 0.0, 1.0),
            bg_color,
            render_mode,
            subpx_dir,
            flags,
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
        if let Some(ref mut r) = self.blob_image_renderer {
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
        mut data: ImageData,
        mut tiling: Option<TileSize>,
    ) {
        if tiling.is_none() && Self::should_tile(self.max_texture_size(), &descriptor, &data) {
            // We aren't going to be able to upload a texture this big, so tile it, even
            // if tiling was not requested.
            tiling = Some(DEFAULT_TILE_SIZE);
        }

        if let ImageData::Blob(ref mut blob) = data {
            self.blob_image_renderer.as_mut().unwrap().add(
                image_key,
                mem::replace(blob, BlobImageData::new()),
                tiling,
            );
        }

        let resource = ImageResource {
            descriptor,
            data,
            epoch: Epoch(0),
            tiling,
            dirty_rect: Some(DeviceUintRect::new(DeviceUintPoint::zero(),
                                                 DeviceUintSize::new(descriptor.width, descriptor.height))),
        };

        self.resources.image_templates.insert(image_key, resource);
    }

    pub fn update_image_template(
        &mut self,
        image_key: ImageKey,
        descriptor: ImageDescriptor,
        mut data: ImageData,
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

        if let ImageData::Blob(ref mut blob) = data {
            self.blob_image_renderer
                .as_mut()
                .unwrap()
                .update(image_key, mem::replace(blob, BlobImageData::new()), dirty_rect);
        }

        *image = ImageResource {
            descriptor,
            data,
            epoch: Epoch(image.epoch.0 + 1),
            tiling,
            dirty_rect: match (dirty_rect, image.dirty_rect) {
                (Some(rect), Some(prev_rect)) => Some(rect.union(&prev_rect)),
                (Some(rect), None) => Some(rect),
                (None, _) => None,
            },
        };
    }

    pub fn delete_image_template(&mut self, image_key: ImageKey) {
        let value = self.resources.image_templates.remove(image_key);

        self.cached_images
            .clear_keys(|request| request.key == image_key);

        match value {
            Some(image) => if image.data.is_blob() {
                self.blob_image_renderer.as_mut().unwrap().delete(image_key);
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
            template.tiling.map_or(cmp::max(template.descriptor.width, template.descriptor.height),
                                   |tile_size| tile_size as u32);
        if side_size > self.texture_cache.max_texture_size() {
            // The image or tiling size is too big for hardware texture size.
            warn!("Dropping image, image:(w:{},h:{}, tile:{}) is too big for hardware!",
                  template.descriptor.width, template.descriptor.height, template.tiling.unwrap_or(0));
            self.cached_images.insert(request, Err(ImageCacheError::OverLimitSize));
            return;
        }

        // If this image exists in the texture cache, *and* the epoch
        // in the cache matches that of the template, then it is
        // valid to use as-is.
        let (entry, needs_update) = match self.cached_images.entry(request) {
            Occupied(entry) => {
                let info = entry.into_mut();
                let needs_update = info.as_mut().unwrap().epoch != template.epoch;
                info.as_mut().unwrap().epoch = template.epoch;
                (info, needs_update)
            }
            Vacant(entry) => (
                entry.insert(Ok(
                    CachedImageInfo {
                        epoch: template.epoch,
                        texture_cache_handle: TextureCacheHandle::new(),
                    }
                )),
                true,
            ),
        };

        let needs_upload = self.texture_cache
            .request(&entry.as_ref().unwrap().texture_cache_handle, gpu_cache);

        if !needs_upload && !needs_update {
            return;
        }

        // We can start a worker thread rasterizing right now, if:
        //  - The image is a blob.
        //  - The blob hasn't already been requested this frame.
        if self.pending_image_requests.insert(request) && template.data.is_blob() {
            if let Some(ref mut renderer) = self.blob_image_renderer {
                let mut dirty_rect = template.dirty_rect;
                let (offset, w, h) = match template.tiling {
                    Some(tile_size) => {
                        let tile_offset = request.tile.unwrap();
                        let (w, h) = compute_tile_size(
                            &template.descriptor,
                            tile_size,
                            tile_offset,
                        );
                        let offset = DevicePoint::new(
                            tile_offset.x as f32 * tile_size as f32,
                            tile_offset.y as f32 * tile_size as f32,
                        );

                        if let Some(dirty) = dirty_rect {
                            dirty_rect = intersect_for_tile(dirty, w, h, tile_size, tile_offset);
                            if dirty_rect.is_none() {
                                return
                            }
                        }

                        (offset, w, h)
                    }
                    None => (
                        DevicePoint::zero(),
                        template.descriptor.width,
                        template.descriptor.height,
                    ),
                };

                renderer.request(
                    &self.resources,
                    request.into(),
                    &BlobImageDescriptor {
                        width: w,
                        height: h,
                        offset,
                        format: template.descriptor.format,
                    },
                    dirty_rect,
                );
            }
        }
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
        key: &GlyphKey,
    ) -> Option<GlyphDimensions> {
        let key = GlyphRequest::new(font, key);

        match self.cached_glyph_dimensions.entry(key.clone()) {
            Occupied(entry) => *entry.get(),
            Vacant(entry) => *entry.insert(
                self.glyph_rasterizer
                    .get_glyph_dimensions(&key.font, &key.key),
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
        match *self.cached_images.get(&request) {
            Ok(ref image_info) => {
                Ok(self.texture_cache.get(&image_info.texture_cache_handle))
            }
            Err(_) => {
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
                epoch: image_template.epoch,
            }
        })
    }

    pub fn get_tiled_image_map(&self) -> TiledImageMap {
        self.resources
            .image_templates
            .images
            .iter()
            .filter_map(|(&key, template)| {
                template.tiling.map(|tile_size| {
                    (
                        key,
                        ImageTiling {
                            image_size: DeviceUintSize::new(
                                template.descriptor.width,
                                template.descriptor.height,
                            ),
                            tile_size,
                        },
                    )
                })
            })
            .collect()
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

    fn update_texture_cache(&mut self, gpu_cache: &mut GpuCache) {
        for request in self.pending_image_requests.drain() {
            let image_template = self.resources.image_templates.get_mut(request.key).unwrap();
            debug_assert!(image_template.data.uses_texture_cache());
            let mut dirty_rect = image_template.dirty_rect;

            let image_data = match image_template.data {
                ImageData::Raw(..) | ImageData::External(..) => {
                    // Safe to clone here since the Raw image data is an
                    // Arc, and the external image data is small.
                    image_template.data.clone()
                }
                ImageData::Blob(..) => {
                    // Extract the rasterized image from the blob renderer.
                    match self.blob_image_renderer
                        .as_mut()
                        .unwrap()
                        .resolve(request.into())
                    {
                        Ok(image) => ImageData::new(image.data),
                        // TODO(nical): I think that we should handle these somewhat gracefully,
                        // at least in the out-of-memory scenario.
                        Err(BlobImageError::Oom) => {
                            // This one should be recoverable-ish.
                            panic!("Failed to render a vector image (OOM)");
                        }
                        Err(BlobImageError::InvalidKey) => {
                            panic!("Invalid vector image key");
                        }
                        Err(BlobImageError::InvalidData) => {
                            // TODO(nical): If we run into this we should kill the content process.
                            panic!("Invalid vector image data");
                        }
                        Err(BlobImageError::Other(msg)) => {
                            panic!("Vector image error {}", msg);
                        }
                    }
                }
            };

            let descriptor = if let Some(tile) = request.tile {
                let tile_size = image_template.tiling.unwrap();
                let image_descriptor = &image_template.descriptor;

                let (actual_width, actual_height) =
                    compute_tile_size(image_descriptor, tile_size, tile);

                if let Some(dirty) = dirty_rect {
                    dirty_rect = intersect_for_tile(dirty, actual_width, actual_height, tile_size, tile);
                    if dirty_rect.is_none() {
                        continue
                    }
                }

                // The tiled image could be stored on the CPU as one large image or be
                // already broken up into tiles. This affects the way we compute the stride
                // and offset.
                let tiled_on_cpu = image_template.data.is_blob();

                let (stride, offset) = if tiled_on_cpu {
                    (image_descriptor.stride, 0)
                } else {
                    let bpp = image_descriptor.format.bytes_per_pixel();
                    let stride = image_descriptor.compute_stride();
                    let offset = image_descriptor.offset +
                        tile.y as u32 * tile_size as u32 * stride +
                        tile.x as u32 * tile_size as u32 * bpp;
                    (Some(stride), offset)
                };

                ImageDescriptor {
                    width: actual_width,
                    height: actual_height,
                    stride,
                    offset,
                    ..*image_descriptor
                }
            } else {
                image_template.descriptor.clone()
            };

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
                       descriptor.width > 512 &&
                       descriptor.height > 512 &&
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

            let entry = self.cached_images.get_mut(&request).as_mut().unwrap();
            self.texture_cache.update(
                &mut entry.texture_cache_handle,
                descriptor,
                filter,
                Some(image_data),
                [0.0; 3],
                dirty_rect,
                gpu_cache,
                None,
            );
            image_template.dirty_rect = None;
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
            .clear_keys(|request| request.key.0 == namespace);

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
    }
}

// Compute the width and height of a tile depending on its position in the image.
pub fn compute_tile_size(
    descriptor: &ImageDescriptor,
    base_size: TileSize,
    tile: TileOffset,
) -> (u32, u32) {
    let base_size = base_size as u32;
    // Most tiles are going to have base_size as width and height,
    // except for tiles around the edges that are shrunk to fit the mage data
    // (See decompose_tiled_image in frame.rs).
    let actual_width = if (tile.x as u32) < descriptor.width / base_size {
        base_size
    } else {
        descriptor.width % base_size
    };

    let actual_height = if (tile.y as u32) < descriptor.height / base_size {
        base_size
    } else {
        descriptor.height % base_size
    };

    (actual_width, actual_height)
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
    epoch: Epoch,
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
                        (desc.width, desc.height),
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
                    let request = BlobImageRequest {
                        key,
                        //TODO: support tiled blob images
                        // https://github.com/servo/webrender/issues/2236
                        tile: None,
                    };
                    let renderer = self.blob_image_renderer.as_mut().unwrap();
                    renderer.request(
                        &self.resources,
                        request,
                        &BlobImageDescriptor {
                            width: desc.width,
                            height: desc.height,
                            offset: DevicePoint::zero(),
                            format: desc.format,
                        },
                        None,
                    );
                    let result = renderer.resolve(request)
                        .expect("Blob resolve failed");
                    assert_eq!((result.width, result.height), (desc.width, desc.height));
                    assert_eq!(result.data.len(), desc.compute_total_size() as usize);

                    num_blobs += 1;
                    #[cfg(feature = "png")]
                    CaptureConfig::save_png(
                        root.join(format!("blobs/{}.png", num_blobs)),
                        (desc.width, desc.height),
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
                        epoch: template.epoch,
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
                epoch: template.epoch,
                dirty_rect: None,
            });
        }

        external_images
    }
}
