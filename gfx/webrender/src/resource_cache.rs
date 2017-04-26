/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use app_units::Au;
use device::TextureFilter;
use fnv::FnvHasher;
use frame::FrameId;
use internal_types::{FontTemplate, SourceTexture, TextureUpdateList};
use platform::font::{FontContext, RasterizedGlyph};
use profiler::TextureCacheProfileCounters;
use std::cell::RefCell;
use std::collections::{HashMap, HashSet};
use std::collections::hash_map::Entry::{self, Occupied, Vacant};
use std::fmt::Debug;
use std::hash::BuildHasherDefault;
use std::hash::Hash;
use std::mem;
use std::sync::{Arc, Barrier, Mutex};
use std::sync::mpsc::{channel, Receiver, Sender};
use std::thread;
use texture_cache::{TextureCache, TextureCacheItemId};
use thread_profiler::register_thread_with_profiler;
use webrender_traits::{Epoch, FontKey, GlyphKey, ImageKey, ImageFormat, ImageRendering};
use webrender_traits::{FontRenderMode, ImageData, GlyphDimensions, WebGLContextId};
use webrender_traits::{DevicePoint, DeviceIntSize, DeviceUintRect, ImageDescriptor, ColorF};
use webrender_traits::{GlyphOptions, GlyphInstance, TileOffset, TileSize};
use webrender_traits::{BlobImageRenderer, BlobImageDescriptor, BlobImageError};
use webrender_traits::{ExternalImageData, ExternalImageType, LayoutPoint};
use threadpool::ThreadPool;
use euclid::Point2D;

const DEFAULT_TILE_SIZE: TileSize = 512;

thread_local!(pub static FONT_CONTEXT: RefCell<FontContext> = RefCell::new(FontContext::new()));

type GlyphCache = ResourceClassCache<RenderedGlyphKey, Option<TextureCacheItemId>>;

/// Message sent from the resource cache to the glyph cache thread.
enum GlyphCacheMsg {
    /// Begin the frame - pass ownership of the glyph cache to the thread.
    BeginFrame(FrameId, GlyphCache),
    /// Add a new font.
    AddFont(FontKey, FontTemplate),
    /// Request glyphs for a text run.
    RequestGlyphs(FontKey, Au, ColorF, Vec<GlyphInstance>, FontRenderMode, Option<GlyphOptions>),
    // Remove an existing font.
    DeleteFont(FontKey),
    /// Finished requesting glyphs. Reply with new glyphs.
    EndFrame,
}

/// Results send from glyph cache thread back to main resource cache.
enum GlyphCacheResultMsg {
    /// Return the glyph cache, and a list of newly rasterized glyphs.
    EndFrame(GlyphCache, Vec<GlyphRasterJob>),
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
pub struct CacheItem {
    pub texture_id: SourceTexture,
    pub uv0: DevicePoint,
    pub uv1: DevicePoint,
}

#[derive(Clone, Hash, PartialEq, Eq, Debug, Ord, PartialOrd)]
pub struct RenderedGlyphKey {
    pub key: GlyphKey,
    pub render_mode: FontRenderMode,
    pub glyph_options: Option<GlyphOptions>,
}

impl RenderedGlyphKey {
    pub fn new(font_key: FontKey,
               size: Au,
               color: ColorF,
               index: u32,
               point: LayoutPoint,
               render_mode: FontRenderMode,
               glyph_options: Option<GlyphOptions>) -> RenderedGlyphKey {
        RenderedGlyphKey {
            key: GlyphKey::new(font_key, size, color, index,
                               point, render_mode),
            render_mode: render_mode,
            glyph_options: glyph_options,
        }
    }
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

struct CachedImageInfo {
    texture_cache_id: TextureCacheItemId,
    epoch: Epoch,
}

pub struct ResourceClassCache<K,V> {
    resources: HashMap<K, V, BuildHasherDefault<FnvHasher>>,
    last_access_times: HashMap<K, FrameId, BuildHasherDefault<FnvHasher>>,
}

impl<K,V> ResourceClassCache<K,V> where K: Clone + Hash + Eq + Debug, V: Resource {
    fn new() -> ResourceClassCache<K,V> {
        ResourceClassCache {
            resources: HashMap::default(),
            last_access_times: HashMap::default(),
        }
    }

    fn contains_key(&self, key: &K) -> bool {
        self.resources.contains_key(key)
    }

    fn get(&self, key: &K, frame: FrameId) -> &V {
        // This assert catches cases in which we accidentally request a resource that we forgot to
        // mark as needed this frame.
        debug_assert!(frame == *self.last_access_times
                                    .get(key)
                                    .expect("Didn't find the access time for a cached resource \
                                             with that ID!"));
        self.resources.get(key).expect("Didn't find a cached resource with that ID!")
    }

    fn insert(&mut self, key: K, value: V, frame: FrameId) {
        self.last_access_times.insert(key.clone(), frame);
        self.resources.insert(key, value);
    }

    fn entry(&mut self, key: K, frame: FrameId) -> Entry<K,V> {
        self.last_access_times.insert(key.clone(), frame);
        self.resources.entry(key)
    }

    fn mark_as_needed(&mut self, key: &K, frame: FrameId) {
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

#[derive(Debug, Clone, Eq, PartialEq, Hash)]
struct ImageRequest {
    key: ImageKey,
    rendering: ImageRendering,
    tile: Option<TileOffset>,
}

struct GlyphRasterJob {
    key: RenderedGlyphKey,
    result: Option<RasterizedGlyph>,
}

struct WebGLTexture {
    id: SourceTexture,
    size: DeviceIntSize,
}

pub struct ResourceCache {
    cached_glyphs: Option<GlyphCache>,
    cached_images: ResourceClassCache<ImageRequest, CachedImageInfo>,

    // TODO(pcwalton): Figure out the lifecycle of these.
    webgl_textures: HashMap<WebGLContextId, WebGLTexture, BuildHasherDefault<FnvHasher>>,

    font_templates: HashMap<FontKey, FontTemplate, BuildHasherDefault<FnvHasher>>,
    image_templates: HashMap<ImageKey, ImageResource, BuildHasherDefault<FnvHasher>>,
    enable_aa: bool,
    state: State,
    current_frame_id: FrameId,

    texture_cache: TextureCache,

    // TODO(gw): We should expire (parts of) this cache semi-regularly!
    cached_glyph_dimensions: HashMap<GlyphKey, Option<GlyphDimensions>, BuildHasherDefault<FnvHasher>>,
    pending_image_requests: Vec<ImageRequest>,
    glyph_cache_tx: Sender<GlyphCacheMsg>,
    glyph_cache_result_queue: Receiver<GlyphCacheResultMsg>,

    blob_image_renderer: Option<Box<BlobImageRenderer>>,
    blob_image_requests: HashSet<ImageRequest>,
}

impl ResourceCache {
    pub fn new(texture_cache: TextureCache,
               workers: Arc<Mutex<ThreadPool>>,
               blob_image_renderer: Option<Box<BlobImageRenderer>>,
               enable_aa: bool) -> ResourceCache {
        let (glyph_cache_tx, glyph_cache_result_queue) = spawn_glyph_cache_thread(workers);

        ResourceCache {
            cached_glyphs: Some(ResourceClassCache::new()),
            cached_images: ResourceClassCache::new(),
            webgl_textures: HashMap::default(),
            font_templates: HashMap::default(),
            image_templates: HashMap::default(),
            cached_glyph_dimensions: HashMap::default(),
            texture_cache: texture_cache,
            state: State::Idle,
            enable_aa: enable_aa,
            current_frame_id: FrameId(0),
            pending_image_requests: Vec::new(),
            glyph_cache_tx: glyph_cache_tx,
            glyph_cache_result_queue: glyph_cache_result_queue,

            blob_image_renderer: blob_image_renderer,
            blob_image_requests: HashSet::new(),
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
        // Push the new font to the glyph cache thread, and also store
        // it locally for glyph metric requests.
        self.glyph_cache_tx
            .send(GlyphCacheMsg::AddFont(font_key, template.clone()))
            .unwrap();
        self.font_templates.insert(font_key, template);
    }

    pub fn delete_font_template(&mut self, font_key: FontKey) {
        self.glyph_cache_tx
            .send(GlyphCacheMsg::DeleteFont(font_key))
            .unwrap();
        self.font_templates.remove(&font_key);
    }

    pub fn add_image_template(&mut self,
                              image_key: ImageKey,
                              descriptor: ImageDescriptor,
                              data: ImageData,
                              mut tiling: Option<TileSize>) {
        if tiling.is_none() && self.should_tile(&descriptor, &data) {
            // We aren't going to be able to upload a texture this big, so tile it, even
            // if tiling was not requested.
            tiling = Some(DEFAULT_TILE_SIZE);
        }

        let resource = ImageResource {
            descriptor: descriptor,
            data: data,
            epoch: Epoch(0),
            tiling: tiling,
            dirty_rect: None,
        };

        self.image_templates.insert(image_key, resource);
    }

    pub fn update_image_template(&mut self,
                                 image_key: ImageKey,
                                 descriptor: ImageDescriptor,
                                 data: ImageData,
                                 dirty_rect: Option<DeviceUintRect>) {
        let resource = if let Some(image) = self.image_templates.get(&image_key) {
            assert_eq!(image.descriptor.width, descriptor.width);
            assert_eq!(image.descriptor.height, descriptor.height);
            assert_eq!(image.descriptor.format, descriptor.format);

            let next_epoch = Epoch(image.epoch.0 + 1);

            let mut tiling = image.tiling;
            if tiling.is_none() && self.should_tile(&descriptor, &data) {
                tiling = Some(DEFAULT_TILE_SIZE);
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

        self.image_templates.insert(image_key, resource);
    }

    pub fn delete_image_template(&mut self, image_key: ImageKey) {
        let value = self.image_templates.remove(&image_key);

        if value.is_none() {
            println!("Delete the non-exist key:{:?}", image_key);
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

        debug_assert!(self.state == State::AddResources);
        let request = ImageRequest {
            key: key,
            rendering: rendering,
            tile: tile,
        };

        let template = self.image_templates.get(&key).unwrap();
        if let ImageData::Blob(ref data) = template.data {
            if let Some(ref mut renderer) = self.blob_image_renderer {
                let same_epoch = match self.cached_images.resources.get(&request) {
                    Some(entry) => entry.epoch == template.epoch,
                    None => false,
                };

                if !same_epoch && self.blob_image_requests.insert(request) {
                    renderer.request_blob_image(
                        key,
                        Arc::clone(data),
                        &BlobImageDescriptor {
                            width: template.descriptor.width,
                            height: template.descriptor.height,
                            format: template.descriptor.format,
                            // TODO(nical): figure out the scale factor (should change with zoom).
                            scale_factor: 1.0,
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
        debug_assert!(self.state == State::AddResources);
        let render_mode = self.get_glyph_render_mode(render_mode);
        // Immediately request that the glyph cache thread start
        // rasterizing glyphs from this request if they aren't
        // already cached.
        let msg = GlyphCacheMsg::RequestGlyphs(key,
                                               size,
                                               color,
                                               glyph_instances.to_vec(),
                                               render_mode,
                                               glyph_options);
        self.glyph_cache_tx.send(msg).unwrap();
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
                         mut f: F) -> SourceTexture where F: FnMut(usize, DevicePoint, DevicePoint) {
        debug_assert!(self.state == State::QueryResources);
        let cache = self.cached_glyphs.as_ref().unwrap();
        let render_mode = self.get_glyph_render_mode(render_mode);
        let mut glyph_key = RenderedGlyphKey::new(font_key,
                                                  size,
                                                  color,
                                                  0,
                                                  LayoutPoint::new(0.0, 0.0),
                                                  render_mode,
                                                  glyph_options);
        let mut texture_id = None;
        for (loop_index, glyph_instance) in glyph_instances.iter().enumerate() {
            glyph_key.key.index = glyph_instance.index;
            glyph_key.key.subpixel_point.set_offset(glyph_instance.point, render_mode);

            let image_id = cache.get(&glyph_key, self.current_frame_id);
            let cache_item = image_id.map(|image_id| self.texture_cache.get(image_id));
            if let Some(cache_item) = cache_item {
                let uv0 = DevicePoint::new(cache_item.pixel_rect.top_left.x as f32,
                                           cache_item.pixel_rect.top_left.y as f32);
                let uv1 = DevicePoint::new(cache_item.pixel_rect.bottom_right.x as f32,
                                           cache_item.pixel_rect.bottom_right.y as f32);
                f(loop_index, uv0, uv1);
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
                let mut dimensions = None;
                let font_template = &self.font_templates[&glyph_key.font_key];

                FONT_CONTEXT.with(|font_context| {
                    let mut font_context = font_context.borrow_mut();
                    match *font_template {
                        FontTemplate::Raw(ref bytes, index) => {
                            font_context.add_raw_font(&glyph_key.font_key, &**bytes, index);
                        }
                        FontTemplate::Native(ref native_font_handle) => {
                            font_context.add_native_font(&glyph_key.font_key,
                                                         (*native_font_handle).clone());
                        }
                    }

                    dimensions = font_context.get_glyph_dimensions(glyph_key);
                });

                *entry.insert(dimensions)
            }
        }
    }

    #[inline]
    pub fn get_cached_image(&self,
                            image_key: ImageKey,
                            image_rendering: ImageRendering,
                            tile: Option<TileOffset>) -> CacheItem {
        debug_assert!(self.state == State::QueryResources);
        let key = ImageRequest {
            key: image_key,
            rendering: image_rendering,
            tile: tile,
        };
        let image_info = &self.cached_images.get(&key, self.current_frame_id);
        let item = self.texture_cache.get(image_info.texture_cache_id);
        CacheItem {
            texture_id: SourceTexture::TextureCache(item.texture_id),
            uv0: DevicePoint::new(item.pixel_rect.top_left.x as f32,
                                  item.pixel_rect.top_left.y as f32),
            uv1: DevicePoint::new(item.pixel_rect.bottom_right.x as f32,
                                  item.pixel_rect.bottom_right.y as f32),
        }
    }

    pub fn get_image_properties(&self, image_key: ImageKey) -> ImageProperties {
        let image_template = &self.image_templates[&image_key];

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

    #[inline]
    pub fn get_webgl_texture(&self, context_id: &WebGLContextId) -> CacheItem {
        let webgl_texture = &self.webgl_textures[context_id];
        CacheItem {
            texture_id: webgl_texture.id,
            uv0: DevicePoint::new(0.0, webgl_texture.size.height as f32),
            uv1: DevicePoint::new(webgl_texture.size.width as f32, 0.0),
        }
    }

    pub fn get_webgl_texture_size(&self, context_id: &WebGLContextId) -> DeviceIntSize {
        self.webgl_textures[context_id].size
    }

    pub fn expire_old_resources(&mut self, frame_id: FrameId) {
        self.cached_images.expire_old_resources(&mut self.texture_cache, frame_id);

        let cached_glyphs = self.cached_glyphs.as_mut().unwrap();
        cached_glyphs.expire_old_resources(&mut self.texture_cache, frame_id);
    }

    pub fn begin_frame(&mut self, frame_id: FrameId) {
        debug_assert!(self.state == State::Idle);
        self.state = State::AddResources;
        self.current_frame_id = frame_id;
        let glyph_cache = self.cached_glyphs.take().unwrap();
        self.glyph_cache_tx.send(GlyphCacheMsg::BeginFrame(frame_id, glyph_cache)).ok();
    }

    pub fn block_until_all_resources_added(&mut self,
                                           texture_cache_profile: &mut TextureCacheProfileCounters) {
        profile_scope!("block_until_all_resources_added");

        debug_assert!(self.state == State::AddResources);
        self.state = State::QueryResources;

        // Tell the glyph cache thread that all glyphs have been requested
        // and block, waiting for any pending glyphs to be rasterized. In the
        // future, we will expand this to have a timeout. If the glyph rasterizing
        // takes longer than the timeout, then we will select the best glyphs
        // available in the cache, render with those, and then re-render at
        // a later point when the correct resolution glyphs finally become
        // available.
        self.glyph_cache_tx.send(GlyphCacheMsg::EndFrame).unwrap();

        // Loop until the end frame message is retrieved here. This loop
        // doesn't serve any real purpose right now, but in the future
        // it will be receiving small amounts of glyphs at a time, up until
        // it decides that it should just render the frame.
        while let Ok(result) = self.glyph_cache_result_queue.recv() {
            match result {
                GlyphCacheResultMsg::EndFrame(mut cache, glyph_jobs) => {
                    // Add any newly rasterized glyphs to the texture cache.
                    for job in glyph_jobs {
                        let image_id = job.result.and_then(|glyph| {
                            if glyph.width > 0 && glyph.height > 0 {
                                let image_id = self.texture_cache.new_item_id();
                                self.texture_cache.insert(image_id,
                                                          ImageDescriptor {
                                                              width: glyph.width,
                                                              height: glyph.height,
                                                              stride: None,
                                                              format: ImageFormat::RGBA8,
                                                              is_opaque: false,
                                                              offset: 0,
                                                          },
                                                          TextureFilter::Linear,
                                                          ImageData::Raw(Arc::new(glyph.bytes)),
                                                          texture_cache_profile);
                                Some(image_id)
                            } else {
                                None
                            }
                        });

                        cache.insert(job.key, image_id, self.current_frame_id);
                    }

                    self.cached_glyphs = Some(cache);
                    break;
                }
            }
        }

        let mut image_requests = mem::replace(&mut self.pending_image_requests, Vec::new());
        for request in image_requests.drain(..) {
            self.finalize_image_request(request, None, texture_cache_profile);
        }

        let mut blob_image_requests = mem::replace(&mut self.blob_image_requests, HashSet::new());
        if self.blob_image_renderer.is_some() {
            for request in blob_image_requests.drain() {
                match self.blob_image_renderer.as_mut().unwrap()
                                                .resolve_blob_image(request.key) {
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
    }

    fn update_texture_cache(&mut self,
                            request: &ImageRequest,
                            image_data: Option<ImageData>,
                            texture_cache_profile: &mut TextureCacheProfileCounters) {
        let image_template = self.image_templates.get_mut(&request.key).unwrap();
        let image_data = image_data.unwrap_or_else(||{
            image_template.data.clone()
        });

        let descriptor = if let Some(tile) = request.tile {
            let tile_size = image_template.tiling.unwrap() as u32;
            let image_descriptor = &image_template.descriptor;
            let stride = image_descriptor.compute_stride();
            let bpp = image_descriptor.format.bytes_per_pixel().unwrap();

            // Storage for the tiles on the right and bottom edges is shrunk to
            // fit the image data (See decompose_tiled_image in frame.rs).
            let actual_width = if (tile.x as u32) < image_descriptor.width / tile_size {
                tile_size
            } else {
                image_descriptor.width % tile_size
            };

            let actual_height = if (tile.y as u32) < image_descriptor.height / tile_size {
                tile_size
            } else {
                image_descriptor.height % tile_size
            };

            let offset = image_descriptor.offset + tile.y as u32 * tile_size * stride
                                                 + tile.x as u32 * tile_size * bpp;

            ImageDescriptor {
                width: actual_width,
                height: actual_height,
                stride: Some(stride),
                offset: offset,
                format: image_descriptor.format,
                is_opaque: image_descriptor.is_opaque,
            }
        } else {
            image_template.descriptor.clone()
        };

        match self.cached_images.entry(request.clone(), self.current_frame_id) {
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
            }
            Vacant(entry) => {
                let image_id = self.texture_cache.new_item_id();

                let filter = match request.rendering {
                    ImageRendering::Pixelated => TextureFilter::Nearest,
                    ImageRendering::Auto | ImageRendering::CrispEdges => TextureFilter::Linear,
                };

                self.texture_cache.insert(image_id,
                                          descriptor,
                                          filter,
                                          image_data,
                                          texture_cache_profile);

                entry.insert(CachedImageInfo {
                    texture_cache_id: image_id,
                    epoch: image_template.epoch,
                });
            }
        }
    }
    fn finalize_image_request(&mut self,
                              request: ImageRequest,
                              image_data: Option<ImageData>,
                              texture_cache_profile: &mut TextureCacheProfileCounters) {
        match self.image_templates.get(&request.key).unwrap().data {
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
        debug_assert!(self.state == State::QueryResources);
        self.state = State::Idle;
    }

    fn get_glyph_render_mode(&self, requested_mode: FontRenderMode) -> FontRenderMode {
        if self.enable_aa {
            requested_mode
        } else {
            FontRenderMode::Mono
        }
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

fn spawn_glyph_cache_thread(workers: Arc<Mutex<ThreadPool>>) -> (Sender<GlyphCacheMsg>, Receiver<GlyphCacheResultMsg>) {
    let worker_count = {
        workers.lock().unwrap().max_count()
    };
    // Used for messages from resource cache -> glyph cache thread.
    let (msg_tx, msg_rx) = channel();
    // Used for returning results from glyph cache thread -> resource cache.
    let (result_tx, result_rx) = channel();
    // Used for rasterizer worker threads to send glyphs -> glyph cache thread.
    let (glyph_tx, glyph_rx) = channel();

    thread::Builder::new().name("GlyphCache".to_string()).spawn(move|| {
        let mut glyph_cache = None;
        let mut current_frame_id = FrameId(0);

        register_thread_with_profiler("GlyphCache".to_string());

        let barrier = Arc::new(Barrier::new(worker_count));
        for i in 0..worker_count {
            let barrier = Arc::clone(&barrier);
            workers.lock().unwrap().execute(move || {
                register_thread_with_profiler(format!("Glyph Worker {}", i));
                barrier.wait();
            });
        }

        // Maintain a set of glyphs that have been requested this
        // frame. This ensures the glyph thread won't rasterize
        // the same glyph more than once in a frame. This is required
        // because the glyph cache hash table is not updated
        // until the glyph cache is passed back to the resource
        // cache which is able to add the items to the texture cache.
        let mut pending_glyphs = HashSet::new();

        while let Ok(msg) = msg_rx.recv() {
            profile_scope!("handle_msg");
            match msg {
                GlyphCacheMsg::BeginFrame(frame_id, cache) => {
                    profile_scope!("BeginFrame");

                    // We are beginning a new frame. Take ownership of the glyph
                    // cache hash map, so we can easily see which glyph requests
                    // actually need to be rasterized.
                    current_frame_id = frame_id;
                    glyph_cache = Some(cache);
                }
                GlyphCacheMsg::AddFont(font_key, font_template) => {
                    profile_scope!("AddFont");

                    // Add a new font to the font context in each worker thread.
                    // Use a barrier to ensure that each worker in the pool handles
                    // one of these messages, to ensure that the new font gets
                    // added to each worker thread.
                    let barrier = Arc::new(Barrier::new(worker_count));
                    for _ in 0..worker_count {
                        let barrier = Arc::clone(&barrier);
                        let font_template = font_template.clone();
                        workers.lock().unwrap().execute(move || {
                            FONT_CONTEXT.with(|font_context| {
                                let mut font_context = font_context.borrow_mut();
                                match font_template {
                                    FontTemplate::Raw(ref bytes, index) => {
                                        font_context.add_raw_font(&font_key, &**bytes, index);
                                    }
                                    FontTemplate::Native(ref native_font_handle) => {
                                        font_context.add_native_font(&font_key,
                                                                     (*native_font_handle).clone());
                                    }
                                }
                            });

                            barrier.wait();
                        });
                    }
                }
                GlyphCacheMsg::DeleteFont(font_key) => {
                    profile_scope!("DeleteFont");

                    // Delete a font from the font context in each worker thread.
                    let barrier = Arc::new(Barrier::new(worker_count));
                    for _ in 0..worker_count {
                        let barrier = Arc::clone(&barrier);
                        workers.lock().unwrap().execute(move || {
                            FONT_CONTEXT.with(|font_context| {
                                let mut font_context = font_context.borrow_mut();
                                font_context.delete_font(&font_key);
                            });
                            barrier.wait();
                        });
                    }

                }
                GlyphCacheMsg::RequestGlyphs(key, size, color, glyph_instances, render_mode, glyph_options) => {
                    profile_scope!("RequestGlyphs");

                    // Request some glyphs for a text run.
                    // For any glyph that isn't currently in the cache,
                    // immeediately push a job to the worker thread pool
                    // to start rasterizing this glyph now!
                    let glyph_cache = glyph_cache.as_mut().unwrap();

                    for glyph_instance in glyph_instances {
                        let glyph_key = RenderedGlyphKey::new(key,
                                                              size,
                                                              color,
                                                              glyph_instance.index,
                                                              glyph_instance.point,
                                                              render_mode,
                                                              glyph_options);

                        glyph_cache.mark_as_needed(&glyph_key, current_frame_id);
                        if !glyph_cache.contains_key(&glyph_key) &&
                           !pending_glyphs.contains(&glyph_key) {
                            let glyph_tx = glyph_tx.clone();
                            pending_glyphs.insert(glyph_key.clone());
                            workers.lock().unwrap().execute(move || {
                                profile_scope!("glyph");
                                FONT_CONTEXT.with(move |font_context| {
                                    let mut font_context = font_context.borrow_mut();
                                    let result = font_context.rasterize_glyph(&glyph_key.key,
                                                                              render_mode,
                                                                              glyph_options);
                                    glyph_tx.send((glyph_key, result)).unwrap();
                                });
                            });
                        }
                    }
                }
                GlyphCacheMsg::EndFrame => {
                    profile_scope!("EndFrame");

                    // The resource cache has finished requesting glyphs. Block
                    // on completion of any pending glyph rasterizing jobs, and then
                    // return the list of new glyphs to the resource cache.
                    let cache = glyph_cache.take().unwrap();
                    let mut rasterized_glyphs = Vec::new();
                    while !pending_glyphs.is_empty() {
                        let (key, glyph) = glyph_rx.recv()
                                                   .expect("BUG: Should be glyphs pending!");
                        debug_assert!(pending_glyphs.contains(&key));
                        pending_glyphs.remove(&key);
                        rasterized_glyphs.push(GlyphRasterJob {
                            key: key,
                            result: glyph,
                        });
                    }
                    // Ensure that the glyphs are always processed in the same
                    // order for a given text run (since iterating a hash set doesn't
                    // guarantee order). This can show up as very small float inaccuacry
                    // differences in rasterizers due to the different coordinates
                    // that text runs get associated with by the texture cache allocator.
                    rasterized_glyphs.sort_by(|a, b| {
                        a.key.cmp(&b.key)
                    });
                    result_tx.send(GlyphCacheResultMsg::EndFrame(cache, rasterized_glyphs)).unwrap();
                }
            }
        }
    }).unwrap();

    (msg_tx, result_rx)
}
