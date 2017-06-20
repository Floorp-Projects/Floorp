/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use app_units::Au;
use device::TextureFilter;
use fnv::FnvHasher;
use frame::FrameId;
use gpu_cache::{GpuCache, GpuCacheHandle};
use internal_types::{SourceTexture, TextureUpdateList};
use profiler::TextureCacheProfileCounters;
use std::collections::{HashMap, HashSet};
use std::collections::hash_map::Entry::{self, Occupied, Vacant};
use std::fmt::Debug;
use std::hash::BuildHasherDefault;
use std::hash::Hash;
use std::mem;
use std::sync::Arc;
use texture_cache::{TextureCache, TextureCacheItemId};
use webrender_traits::{Epoch, FontKey, FontTemplate, GlyphKey, ImageKey, ImageRendering};
use webrender_traits::{FontRenderMode, ImageData, GlyphDimensions, WebGLContextId};
use webrender_traits::{DevicePoint, DeviceIntSize, DeviceUintRect, ImageDescriptor, ColorF};
use webrender_traits::{GlyphOptions, GlyphInstance, TileOffset, TileSize};
use webrender_traits::{BlobImageRenderer, BlobImageDescriptor, BlobImageError, BlobImageRequest, BlobImageData};
use webrender_traits::BlobImageResources;
use webrender_traits::{ExternalImageData, ExternalImageType, LayoutPoint};
use rayon::ThreadPool;
use glyph_rasterizer::{GlyphRasterizer, GlyphCache, GlyphRequest};

const DEFAULT_TILE_SIZE: TileSize = 512;

// These coordinates are always in texels.
// They are converted to normalized ST
// values in the vertex shader. The reason
// for this is that the texture may change
// dimensions (e.g. the pages in a texture
// atlas can grow). When this happens, by
// storing the coordinates as texel values
// we don't need to go through and update
// various CPU-side structures.
pub struct CacheItem {
    pub texture_id: SourceTexture,
    pub uv_rect_handle: GpuCacheHandle,
}

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

struct ImageResource {
    data: ImageData,
    descriptor: ImageDescriptor,
    epoch: Epoch,
    tiling: Option<TileSize>,
    dirty_rect: Option<DeviceUintRect>
}

struct ImageTemplates {
    images: HashMap<ImageKey, ImageResource, BuildHasherDefault<FnvHasher>>,
}

impl ImageTemplates {
    fn new() -> Self {
        ImageTemplates {
            images: HashMap::with_hasher(Default::default())
        }
    }

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

struct CachedImageInfo {
    texture_cache_id: TextureCacheItemId,
    epoch: Epoch,
}

pub struct ResourceClassCache<K,V> {
    resources: HashMap<K, V, BuildHasherDefault<FnvHasher>>,
    last_access_times: HashMap<K, FrameId, BuildHasherDefault<FnvHasher>>,
}

impl<K,V> ResourceClassCache<K,V> where K: Clone + Hash + Eq + Debug, V: Resource {
    pub fn new() -> ResourceClassCache<K,V> {
        ResourceClassCache {
            resources: HashMap::default(),
            last_access_times: HashMap::default(),
        }
    }

    fn get(&self, key: &K, frame: FrameId) -> &V {
        // This assert catches cases in which we accidentally request a resource that we forgot to
        // mark as needed this frame.
        debug_assert_eq!(frame, *self.last_access_times
                                     .get(key)
                                     .expect("Didn't find the access time for a cached resource \
                                              with that ID!"));
        self.resources.get(key).expect("Didn't find a cached resource with that ID!")
    }

    pub fn insert(&mut self, key: K, value: V, frame: FrameId) {
        self.last_access_times.insert(key.clone(), frame);
        self.resources.insert(key, value);
    }

    pub fn entry(&mut self, key: K, frame: FrameId) -> Entry<K,V> {
        self.last_access_times.insert(key.clone(), frame);
        self.resources.entry(key)
    }

    pub fn mark_as_needed(&mut self, key: &K, frame: FrameId) {
        self.last_access_times.insert((*key).clone(), frame);
    }

    fn expire_old_resources(&mut self, texture_cache: &mut TextureCache, frame_id: FrameId) {
        let mut resources_to_destroy = vec![];
        for (key, this_frame_id) in &self.last_access_times {
            if *this_frame_id < frame_id {
                resources_to_destroy.push((*key).clone())
            }
        }
        for key in resources_to_destroy {
            let resource =
                self.resources
                    .remove(&key)
                    .expect("Resource was in `last_access_times` but not in `resources`!");
            self.last_access_times.remove(&key);
            if let Some(texture_cache_item_id) = resource.texture_cache_item_id() {
                texture_cache.free(texture_cache_item_id)
            }
        }
    }
}


#[derive(Debug, Copy, Clone, Eq, PartialEq, Hash)]
struct ImageRequest {
    key: ImageKey,
    rendering: ImageRendering,
    tile: Option<TileOffset>,
}

impl Into<BlobImageRequest> for ImageRequest {
    fn into(self) -> BlobImageRequest {
        BlobImageRequest {
            key: self.key,
            tile: self.tile,
        }
    }
}

pub struct WebGLTexture {
    pub id: SourceTexture,
    pub size: DeviceIntSize,
}

struct Resources {
    font_templates: HashMap<FontKey, FontTemplate, BuildHasherDefault<FnvHasher>>,
    image_templates: ImageTemplates,
}

impl BlobImageResources for Resources {
    fn get_font_data(&self, key: FontKey) -> &FontTemplate {
        self.font_templates.get(&key).unwrap()
    }
    fn get_image(&self, key: ImageKey) -> Option<(&ImageData, &ImageDescriptor)> {
        self.image_templates.get(key).map(|resource| { (&resource.data, &resource.descriptor) })
    }
}

pub struct ResourceCache {
    cached_glyphs: GlyphCache,
    cached_images: ResourceClassCache<ImageRequest, CachedImageInfo>,

    // TODO(pcwalton): Figure out the lifecycle of these.
    webgl_textures: HashMap<WebGLContextId, WebGLTexture, BuildHasherDefault<FnvHasher>>,

    resources: Resources,
    state: State,
    current_frame_id: FrameId,

    texture_cache: TextureCache,

    // TODO(gw): We should expire (parts of) this cache semi-regularly!
    cached_glyph_dimensions: HashMap<GlyphKey, Option<GlyphDimensions>, BuildHasherDefault<FnvHasher>>,
    pending_image_requests: Vec<ImageRequest>,
    glyph_rasterizer: GlyphRasterizer,

    blob_image_renderer: Option<Box<BlobImageRenderer>>,
    blob_image_requests: HashSet<ImageRequest>,

    requested_glyphs: HashSet<TextureCacheItemId, BuildHasherDefault<FnvHasher>>,
    requested_images: HashSet<TextureCacheItemId, BuildHasherDefault<FnvHasher>>,
}

impl ResourceCache {
    pub fn new(texture_cache: TextureCache,
               workers: Arc<ThreadPool>,
               blob_image_renderer: Option<Box<BlobImageRenderer>>) -> ResourceCache {
        ResourceCache {
            cached_glyphs: ResourceClassCache::new(),
            cached_images: ResourceClassCache::new(),
            webgl_textures: HashMap::default(),
            resources: Resources {
                font_templates: HashMap::default(),
                image_templates: ImageTemplates::new(),
            },
            cached_glyph_dimensions: HashMap::default(),
            texture_cache: texture_cache,
            state: State::Idle,
            current_frame_id: FrameId(0),
            pending_image_requests: Vec::new(),
            glyph_rasterizer: GlyphRasterizer::new(workers),

            blob_image_renderer: blob_image_renderer,
            blob_image_requests: HashSet::new(),

            requested_glyphs: HashSet::default(),
            requested_images: HashSet::default(),
        }
    }

    pub fn max_texture_size(&self) -> u32 {
        self.texture_cache.max_texture_size()
    }

    fn should_tile(&self, descriptor: &ImageDescriptor, data: &ImageData) -> bool {
        let limit = self.max_texture_size();
        let size_check = descriptor.width > limit || descriptor.height > limit;
        match *data {
            ImageData::Raw(_) | ImageData::Blob(_) => { size_check }
            ImageData::External(info) => {
                // External handles already represent existing textures so it does
                // not make sense to tile them into smaller ones.
                info.image_type == ExternalImageType::ExternalBuffer && size_check
            },
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
        if let Some(ref mut r) = self.blob_image_renderer {
            r.delete_font(font_key);
        }
    }

    pub fn add_image_template(&mut self,
                              image_key: ImageKey,
                              descriptor: ImageDescriptor,
                              mut data: ImageData,
                              mut tiling: Option<TileSize>) {
        if tiling.is_none() && self.should_tile(&descriptor, &data) {
            // We aren't going to be able to upload a texture this big, so tile it, even
            // if tiling was not requested.
            tiling = Some(DEFAULT_TILE_SIZE);
        }

        if let ImageData::Blob(ref mut blob) = data {
            self.blob_image_renderer.as_mut().unwrap().add(
                image_key,
                mem::replace(blob, BlobImageData::new()),
                tiling
            );
        }

        let resource = ImageResource {
            descriptor: descriptor,
            data: data,
            epoch: Epoch(0),
            tiling: tiling,
            dirty_rect: None,
        };

        self.resources.image_templates.insert(image_key, resource);
    }

    pub fn update_image_template(&mut self,
                                 image_key: ImageKey,
                                 descriptor: ImageDescriptor,
                                 mut data: ImageData,
                                 dirty_rect: Option<DeviceUintRect>) {
        let resource = if let Some(image) = self.resources.image_templates.get(image_key) {
            assert_eq!(image.descriptor.width, descriptor.width);
            assert_eq!(image.descriptor.height, descriptor.height);
            assert_eq!(image.descriptor.format, descriptor.format);

            let next_epoch = Epoch(image.epoch.0 + 1);

            let mut tiling = image.tiling;
            if tiling.is_none() && self.should_tile(&descriptor, &data) {
                tiling = Some(DEFAULT_TILE_SIZE);
            }

            if let ImageData::Blob(ref mut blob) = data {
                self.blob_image_renderer.as_mut().unwrap().update(
                    image_key,
                    mem::replace(blob, BlobImageData::new())
                );
            }

            ImageResource {
                descriptor: descriptor,
                data: data,
                epoch: next_epoch,
                tiling: tiling,
                dirty_rect: match (dirty_rect, image.dirty_rect) {
                    (Some(rect), Some(prev_rect)) => Some(rect.union(&prev_rect)),
                    (Some(rect), None) => Some(rect),
                    _ => None,
                },
            }
        } else {
            panic!("Attempt to update non-existant image (key {:?}).", image_key);
        };

        self.resources.image_templates.insert(image_key, resource);
    }

    pub fn delete_image_template(&mut self, image_key: ImageKey) {
        let value = self.resources.image_templates.remove(image_key);

        match value {
            Some(image) => {
                if image.data.is_blob() {
                    self.blob_image_renderer.as_mut().unwrap().delete(image_key);
                }
            }
            None => {
                println!("Delete the non-exist key:{:?}", image_key);
            }
        }
    }

    pub fn add_webgl_texture(&mut self, id: WebGLContextId, texture_id: SourceTexture, size: DeviceIntSize) {
        self.webgl_textures.insert(id, WebGLTexture {
            id: texture_id,
            size: size,
        });
    }

    pub fn update_webgl_texture(&mut self, id: WebGLContextId, texture_id: SourceTexture, size: DeviceIntSize) {
        let webgl_texture = self.webgl_textures.get_mut(&id).unwrap();

        // Update new texture id and size
        webgl_texture.id = texture_id;
        webgl_texture.size = size;
    }

    pub fn request_image(&mut self,
                         key: ImageKey,
                         rendering: ImageRendering,
                         tile: Option<TileOffset>) {

        debug_assert_eq!(self.state, State::AddResources);
        let request = ImageRequest {
            key: key,
            rendering: rendering,
            tile: tile,
        };

        let template = self.resources.image_templates.get(key).unwrap();
        if template.data.uses_texture_cache() {
            self.cached_images.mark_as_needed(&request, self.current_frame_id);
        }
        if template.data.is_blob() {
            if let Some(ref mut renderer) = self.blob_image_renderer {
                let (same_epoch, texture_cache_id) = match self.cached_images.resources
                                                               .get(&request) {
                    Some(entry) => {
                        (entry.epoch == template.epoch, Some(entry.texture_cache_id))
                    }
                    None => {
                        (false, None)
                    }
                };

                // Ensure that blobs are added to the list of requested items
                // foe the GPU cache, even if the cached blob image is up to date.
                if let Some(texture_cache_id) = texture_cache_id {
                    self.requested_images.insert(texture_cache_id);
                }

                if !same_epoch && self.blob_image_requests.insert(request) {
                    let (offset, w, h) = match template.tiling {
                        Some(tile_size) => {
                            let tile_offset = request.tile.unwrap();
                            let (w, h) = compute_tile_size(&template.descriptor, tile_size, tile_offset);
                            let offset = DevicePoint::new(
                                tile_offset.x as f32 * tile_size as f32,
                                tile_offset.y as f32 * tile_size as f32,
                            );

                            (offset, w, h)
                        }
                        None => {
                            (DevicePoint::zero(), template.descriptor.width, template.descriptor.height)
                        }
                    };

                    renderer.request(
                        &self.resources,
                        request.into(),
                        &BlobImageDescriptor {
                            width: w,
                            height: h,
                            offset: offset,
                            format: template.descriptor.format,
                        },
                        template.dirty_rect,
                    );
                }
            }
        } else {
            self.pending_image_requests.push(request);
        }
    }

    pub fn request_glyphs(&mut self,
                          key: FontKey,
                          size: Au,
                          color: ColorF,
                          glyph_instances: &[GlyphInstance],
                          render_mode: FontRenderMode,
                          glyph_options: Option<GlyphOptions>) {
        debug_assert_eq!(self.state, State::AddResources);

        self.glyph_rasterizer.request_glyphs(
            &mut self.cached_glyphs,
            self.current_frame_id,
            key,
            size,
            color,
            glyph_instances,
            render_mode,
            glyph_options,
            &mut self.requested_glyphs,
        );
    }

    pub fn pending_updates(&mut self) -> TextureUpdateList {
        self.texture_cache.pending_updates()
    }

    pub fn get_glyphs<F>(&self,
                         font_key: FontKey,
                         size: Au,
                         color: ColorF,
                         glyph_instances: &[GlyphInstance],
                         render_mode: FontRenderMode,
                         glyph_options: Option<GlyphOptions>,
                         mut f: F) -> SourceTexture where F: FnMut(usize, &GpuCacheHandle) {
        debug_assert_eq!(self.state, State::QueryResources);
        let mut glyph_key = GlyphRequest::new(
            font_key,
            size,
            color,
            0,
            LayoutPoint::new(0.0, 0.0),
            render_mode,
            glyph_options
        );
        let mut texture_id = None;
        for (loop_index, glyph_instance) in glyph_instances.iter().enumerate() {
            glyph_key.key.index = glyph_instance.index;
            glyph_key.key.subpixel_point.set_offset(glyph_instance.point, render_mode);

            let image_id = self.cached_glyphs.get(&glyph_key, self.current_frame_id);
            let cache_item = image_id.map(|image_id| self.texture_cache.get(image_id));
            if let Some(cache_item) = cache_item {
                f(loop_index, &cache_item.uv_rect_handle);
                debug_assert!(texture_id == None ||
                              texture_id == Some(cache_item.texture_id));
                texture_id = Some(cache_item.texture_id);
            }
        }

        texture_id.map_or(SourceTexture::Invalid, SourceTexture::TextureCache)
    }

    pub fn get_glyph_dimensions(&mut self, glyph_key: &GlyphKey) -> Option<GlyphDimensions> {
        match self.cached_glyph_dimensions.entry(glyph_key.clone()) {
            Occupied(entry) => *entry.get(),
            Vacant(entry) => {
                *entry.insert(self.glyph_rasterizer.get_glyph_dimensions(glyph_key))
            }
        }
    }

    #[inline]
    pub fn get_cached_image(&self,
                            image_key: ImageKey,
                            image_rendering: ImageRendering,
                            tile: Option<TileOffset>) -> CacheItem {
        debug_assert_eq!(self.state, State::QueryResources);
        let key = ImageRequest {
            key: image_key,
            rendering: image_rendering,
            tile: tile,
        };
        let image_info = &self.cached_images.get(&key, self.current_frame_id);
        let item = self.texture_cache.get(image_info.texture_cache_id);
        CacheItem {
            texture_id: SourceTexture::TextureCache(item.texture_id),
            uv_rect_handle: item.uv_rect_handle,
        }
    }

    pub fn get_image_properties(&self, image_key: ImageKey) -> ImageProperties {
        let image_template = &self.resources.image_templates.get(image_key).unwrap();

        let external_image = match image_template.data {
            ImageData::External(ext_image) => {
                match ext_image.image_type {
                    ExternalImageType::Texture2DHandle |
                    ExternalImageType::TextureRectHandle |
                    ExternalImageType::TextureExternalHandle => {
                        Some(ext_image)
                    },
                    // external buffer uses resource_cache.
                    ExternalImageType::ExternalBuffer => None,
                }
            },
            // raw and blob image are all using resource_cache.
            ImageData::Raw(..) | ImageData::Blob(..) => None,
        };

        ImageProperties {
            descriptor: image_template.descriptor,
            external_image: external_image,
            tiling: image_template.tiling,
        }
    }

    pub fn get_webgl_texture(&self, context_id: &WebGLContextId) -> &WebGLTexture {
        &self.webgl_textures[context_id]
    }

    pub fn get_webgl_texture_size(&self, context_id: &WebGLContextId) -> DeviceIntSize {
        self.webgl_textures[context_id].size
    }

    pub fn expire_old_resources(&mut self, frame_id: FrameId) {
        self.cached_images.expire_old_resources(&mut self.texture_cache, frame_id);
        self.cached_glyphs.expire_old_resources(&mut self.texture_cache, frame_id);
    }

    pub fn begin_frame(&mut self, frame_id: FrameId) {
        debug_assert_eq!(self.state, State::Idle);
        self.state = State::AddResources;
        self.current_frame_id = frame_id;
        debug_assert!(self.requested_glyphs.is_empty());
        debug_assert!(self.requested_images.is_empty());
    }

    pub fn block_until_all_resources_added(&mut self,
                                           gpu_cache: &mut GpuCache,
                                           texture_cache_profile: &mut TextureCacheProfileCounters) {
        profile_scope!("block_until_all_resources_added");

        debug_assert_eq!(self.state, State::AddResources);
        self.state = State::QueryResources;

        self.glyph_rasterizer.resolve_glyphs(
            self.current_frame_id,
            &mut self.cached_glyphs,
            &mut self.texture_cache,
            &mut self.requested_glyphs,
            texture_cache_profile,
        );

        let mut image_requests = mem::replace(&mut self.pending_image_requests, Vec::new());
        for request in image_requests.drain(..) {
            self.finalize_image_request(request, None, texture_cache_profile);
        }

        let mut blob_image_requests = mem::replace(&mut self.blob_image_requests, HashSet::new());
        if self.blob_image_renderer.is_some() {
            for request in blob_image_requests.drain() {
                match self.blob_image_renderer.as_mut().unwrap().resolve(request.into()) {
                    Ok(image) => {
                        self.finalize_image_request(request,
                                                    Some(ImageData::new(image.data)),
                                                    texture_cache_profile);
                    }
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
        }

        for texture_cache_item_id in self.requested_images.drain() {
            let item = self.texture_cache.get_mut(texture_cache_item_id);
            if let Some(mut request) = gpu_cache.request(&mut item.uv_rect_handle) {
                request.push(item.uv_rect.into());
            }
        }

        for texture_cache_item_id in self.requested_glyphs.drain() {
            let item = self.texture_cache.get_mut(texture_cache_item_id);
            if let Some(mut request) = gpu_cache.request(&mut item.uv_rect_handle) {
                request.push(item.uv_rect.into());
                request.push([item.user_data[0], item.user_data[1], 0.0, 0.0].into());
            }
        }
    }

    fn update_texture_cache(&mut self,
                            request: &ImageRequest,
                            image_data: Option<ImageData>,
                            texture_cache_profile: &mut TextureCacheProfileCounters) {
        let image_template = self.resources.image_templates.get_mut(request.key).unwrap();
        let image_data = image_data.unwrap_or_else(||{
            image_template.data.clone()
        });

        let descriptor = if let Some(tile) = request.tile {
            let tile_size = image_template.tiling.unwrap();
            let image_descriptor = &image_template.descriptor;

            let (actual_width, actual_height) = compute_tile_size(image_descriptor, tile_size, tile);

            // The tiled image could be stored on the CPU as one large image or be
            // already broken up into tiles. This affects the way we compute the stride
            // and offset.
            let tiled_on_cpu = image_template.data.is_blob();

            let (stride, offset) = if tiled_on_cpu {
                (image_descriptor.stride, 0)
            } else {
                let bpp = image_descriptor.format.bytes_per_pixel().unwrap();
                let stride = image_descriptor.compute_stride();
                let offset = image_descriptor.offset + tile.y as u32 * tile_size as u32 * stride
                                                     + tile.x as u32 * tile_size as u32 * bpp;
                (Some(stride), offset)
            };

            ImageDescriptor {
                width: actual_width,
                height: actual_height,
                stride: stride,
                offset: offset,
                format: image_descriptor.format,
                is_opaque: image_descriptor.is_opaque,
            }
        } else {
            image_template.descriptor.clone()
        };

        let image_id = match self.cached_images.entry(*request, self.current_frame_id) {
            Occupied(entry) => {
                let image_id = entry.get().texture_cache_id;

                if entry.get().epoch != image_template.epoch {
                    self.texture_cache.update(image_id,
                                              descriptor,
                                              image_data,
                                              image_template.dirty_rect);

                    // Update the cached epoch
                    *entry.into_mut() = CachedImageInfo {
                        texture_cache_id: image_id,
                        epoch: image_template.epoch,
                    };
                    image_template.dirty_rect = None;
                }

                image_id
            }
            Vacant(entry) => {
                let filter = match request.rendering {
                    ImageRendering::Pixelated => TextureFilter::Nearest,
                    ImageRendering::Auto | ImageRendering::CrispEdges => TextureFilter::Linear,
                };

                let image_id = self.texture_cache.insert(descriptor,
                                                         filter,
                                                         image_data,
                                                         [0.0; 2],
                                                         texture_cache_profile);

                entry.insert(CachedImageInfo {
                    texture_cache_id: image_id,
                    epoch: image_template.epoch,
                });

                image_id
            }
        };

        self.requested_images.insert(image_id);
    }
    fn finalize_image_request(&mut self,
                              request: ImageRequest,
                              image_data: Option<ImageData>,
                              texture_cache_profile: &mut TextureCacheProfileCounters) {
        match self.resources.image_templates.get(request.key).unwrap().data {
            ImageData::External(ext_image) => {
                match ext_image.image_type {
                    ExternalImageType::Texture2DHandle |
                    ExternalImageType::TextureRectHandle |
                    ExternalImageType::TextureExternalHandle => {
                        // external handle doesn't need to update the texture_cache.
                    }
                    ExternalImageType::ExternalBuffer => {
                        self.update_texture_cache(&request,
                                                  image_data,
                                                  texture_cache_profile);
                    }
                }
            }
            ImageData::Raw(..) | ImageData::Blob(..) => {
                self.update_texture_cache(&request,
                                           image_data,
                                           texture_cache_profile);
            }
        }
    }

    pub fn end_frame(&mut self) {
        debug_assert_eq!(self.state, State::QueryResources);
        self.state = State::Idle;
    }
}

pub trait Resource {
    fn texture_cache_item_id(&self) -> Option<TextureCacheItemId>;
}

impl Resource for TextureCacheItemId {
    fn texture_cache_item_id(&self) -> Option<TextureCacheItemId> {
        Some(*self)
    }
}

impl Resource for Option<TextureCacheItemId> {
    fn texture_cache_item_id(&self) -> Option<TextureCacheItemId> {
        *self
    }
}

impl Resource for CachedImageInfo {
    fn texture_cache_item_id(&self) -> Option<TextureCacheItemId> {
        Some(self.texture_cache_id)
    }
}


// Compute the width and height of a tile depending on its position in the image.
pub fn compute_tile_size(descriptor: &ImageDescriptor,
                         base_size: TileSize,
                         tile: TileOffset) -> (u32, u32) {
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
