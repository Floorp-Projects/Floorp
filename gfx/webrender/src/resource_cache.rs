/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use app_units::Au;
use device::TextureFilter;
use euclid::{Point2D, Size2D};
use fnv::FnvHasher;
use frame::FrameId;
use internal_types::{FontTemplate, SourceTexture, TextureUpdateList};
use platform::font::{FontContext, RasterizedGlyph};
use rayon::prelude::*;
use std::cell::RefCell;
use std::collections::HashMap;
use std::collections::hash_map::Entry::{self, Occupied, Vacant};
use std::fmt::Debug;
use std::hash::BuildHasherDefault;
use std::hash::Hash;
use texture_cache::{TextureCache, TextureCacheItemId};
use webrender_traits::{Epoch, FontKey, GlyphKey, ImageKey, ImageFormat, ImageRendering};
use webrender_traits::{FontRenderMode, GlyphDimensions, WebGLContextId};

thread_local!(pub static FONT_CONTEXT: RefCell<FontContext> = RefCell::new(FontContext::new()));

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
    pub uv0: Point2D<f32>,
    pub uv1: Point2D<f32>,
}

#[derive(Clone, Hash, PartialEq, Eq, Debug)]
pub struct RenderedGlyphKey {
    pub key: GlyphKey,
    pub render_mode: FontRenderMode,
}

impl RenderedGlyphKey {
    pub fn new(font_key: FontKey,
               size: Au,
               index: u32,
               render_mode: FontRenderMode) -> RenderedGlyphKey {
        RenderedGlyphKey {
            key: GlyphKey::new(font_key, size, index),
            render_mode: render_mode,
        }
    }
}

pub struct ImageProperties {
    pub format: ImageFormat,
    pub is_opaque: bool,
}

#[derive(Debug, Copy, Clone, PartialEq)]
enum State {
    Idle,
    AddResources,
    QueryResources,
}

struct ImageResource {
    bytes: Vec<u8>,
    width: u32,
    height: u32,
    stride: Option<u32>,
    format: ImageFormat,
    epoch: Epoch,
    is_opaque: bool,
}

struct GlyphRasterJob {
    glyph_key: RenderedGlyphKey,
    result: Option<RasterizedGlyph>,
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
            resources: HashMap::with_hasher(Default::default()),
            last_access_times: HashMap::with_hasher(Default::default()),
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

struct TextRunResourceRequest {
    key: FontKey,
    size: Au,
    glyph_indices: Vec<u32>,
    render_mode: FontRenderMode,
}

enum ResourceRequest {
    Image(ImageKey, ImageRendering),
    TextRun(TextRunResourceRequest),
}

struct WebGLTexture {
    id: SourceTexture,
    size: Size2D<i32>,
}

pub struct ResourceCache {
    cached_glyphs: ResourceClassCache<RenderedGlyphKey, Option<TextureCacheItemId>>,
    cached_images: ResourceClassCache<(ImageKey, ImageRendering), CachedImageInfo>,

    // TODO(pcwalton): Figure out the lifecycle of these.
    webgl_textures: HashMap<WebGLContextId, WebGLTexture, BuildHasherDefault<FnvHasher>>,

    font_templates: HashMap<FontKey, FontTemplate, BuildHasherDefault<FnvHasher>>,
    image_templates: HashMap<ImageKey, ImageResource, BuildHasherDefault<FnvHasher>>,
    device_pixel_ratio: f32,
    enable_aa: bool,
    state: State,
    current_frame_id: FrameId,

    texture_cache: TextureCache,

    // TODO(gw): We should expire (parts of) this cache semi-regularly!
    cached_glyph_dimensions: HashMap<GlyphKey, Option<GlyphDimensions>, BuildHasherDefault<FnvHasher>>,
    pending_requests: Vec<ResourceRequest>,
    pending_raster_jobs: Vec<GlyphRasterJob>,
}

impl ResourceCache {
    pub fn new(texture_cache: TextureCache,
               device_pixel_ratio: f32,
               enable_aa: bool) -> ResourceCache {
        ResourceCache {
            cached_glyphs: ResourceClassCache::new(),
            cached_images: ResourceClassCache::new(),
            webgl_textures: HashMap::with_hasher(Default::default()),
            font_templates: HashMap::with_hasher(Default::default()),
            image_templates: HashMap::with_hasher(Default::default()),
            cached_glyph_dimensions: HashMap::with_hasher(Default::default()),
            texture_cache: texture_cache,
            state: State::Idle,
            device_pixel_ratio: device_pixel_ratio,
            enable_aa: enable_aa,
            current_frame_id: FrameId(0),
            pending_raster_jobs: Vec::new(),
            pending_requests: Vec::new(),
        }
    }

    pub fn add_font_template(&mut self, font_key: FontKey, template: FontTemplate) {
        self.font_templates.insert(font_key, template);
    }

    pub fn add_image_template(&mut self,
                              image_key: ImageKey,
                              width: u32,
                              height: u32,
                              stride: Option<u32>,
                              format: ImageFormat,
                              bytes: Vec<u8>) {
        let resource = ImageResource {
            is_opaque: is_image_opaque(format, &bytes),
            width: width,
            height: height,
            stride: stride,
            format: format,
            bytes: bytes,
            epoch: Epoch(0),
        };

        self.image_templates.insert(image_key, resource);
    }

    pub fn update_image_template(&mut self,
                                 image_key: ImageKey,
                                 width: u32,
                                 height: u32,
                                 format: ImageFormat,
                                 bytes: Vec<u8>) {
        let next_epoch = match self.image_templates.get(&image_key) {
            Some(image) => {
                let Epoch(current_epoch) = image.epoch;
                Epoch(current_epoch + 1)
            }
            None => {
                Epoch(0)
            }
        };

        let resource = ImageResource {
            is_opaque: is_image_opaque(format, &bytes),
            width: width,
            height: height,
            stride: None,
            format: format,
            bytes: bytes,
            epoch: next_epoch,
        };

        self.image_templates.insert(image_key, resource);
    }

    pub fn delete_image_template(&mut self, image_key: ImageKey) {
        self.image_templates.remove(&image_key);
    }

    pub fn add_webgl_texture(&mut self, id: WebGLContextId, texture_id: SourceTexture, size: Size2D<i32>) {
        self.webgl_textures.insert(id, WebGLTexture {
            id: texture_id,
            size: size,
        });
    }

    pub fn update_webgl_texture(&mut self, id: WebGLContextId, texture_id: SourceTexture, size: Size2D<i32>) {
        let webgl_texture = self.webgl_textures.get_mut(&id).unwrap();

        // Update new texture id and size
        webgl_texture.id = texture_id;
        webgl_texture.size = size;
    }

    pub fn request_image(&mut self,
                         key: ImageKey,
                         rendering: ImageRendering) {
        debug_assert!(self.state == State::AddResources);
        self.pending_requests.push(ResourceRequest::Image(key, rendering));
    }

    pub fn request_glyphs(&mut self,
                          key: FontKey,
                          size: Au,
                          glyph_indices: &[u32],
                          render_mode: FontRenderMode) {
        debug_assert!(self.state == State::AddResources);
        self.pending_requests.push(ResourceRequest::TextRun(TextRunResourceRequest {
            key: key,
            size: size,
            glyph_indices: glyph_indices.to_vec(),
            render_mode: render_mode,
        }));
    }

    pub fn raster_pending_glyphs(&mut self) {
        // Run raster jobs in parallel
        run_raster_jobs(&mut self.pending_raster_jobs,
                        &self.font_templates,
                        self.device_pixel_ratio,
                        self.enable_aa);

        // Add completed raster jobs to the texture cache
        for job in self.pending_raster_jobs.drain(..) {
            let result = job.result.expect("Failed to rasterize the glyph?");
            let image_id = if result.width > 0 && result.height > 0 {
                let image_id = self.texture_cache.new_item_id();
                let texture_width = result.width;
                let texture_height = result.height;
                self.texture_cache.insert(image_id,
                                          texture_width,
                                          texture_height,
                                          None,
                                          ImageFormat::RGBA8,
                                          TextureFilter::Linear,
                                          result.bytes);
                Some(image_id)
            } else {
                None
            };
            self.cached_glyphs.insert(job.glyph_key, image_id, self.current_frame_id);
        }
    }

    pub fn pending_updates(&mut self) -> TextureUpdateList {
        self.texture_cache.pending_updates()
    }

    pub fn get_glyphs<F>(&self,
                         font_key: FontKey,
                         size: Au,
                         glyph_indices: &[u32],
                         render_mode: FontRenderMode,
                         mut f: F) -> SourceTexture where F: FnMut(usize, Point2D<f32>, Point2D<f32>) {
        debug_assert!(self.state == State::QueryResources);
        let mut glyph_key = RenderedGlyphKey::new(font_key,
                                                  size,
                                                  0,
                                                  render_mode);
        let mut texture_id = None;
        for (loop_index, glyph_index) in glyph_indices.iter().enumerate() {
            glyph_key.key.index = *glyph_index;
            let image_id = self.cached_glyphs.get(&glyph_key, self.current_frame_id);
            let cache_item = image_id.map(|image_id| self.texture_cache.get(image_id));
            if let Some(cache_item) = cache_item {
                let uv0 = Point2D::new(cache_item.pixel_rect.top_left.x as f32,
                                       cache_item.pixel_rect.top_left.y as f32);
                let uv1 = Point2D::new(cache_item.pixel_rect.bottom_right.x as f32,
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
                let device_pixel_ratio = self.device_pixel_ratio;
                let font_template = &self.font_templates[&glyph_key.font_key];

                FONT_CONTEXT.with(|font_context| {
                    let mut font_context = font_context.borrow_mut();
                    match *font_template {
                        FontTemplate::Raw(ref bytes) => {
                            font_context.add_raw_font(&glyph_key.font_key, &**bytes);
                        }
                        FontTemplate::Native(ref native_font_handle) => {
                            font_context.add_native_font(&glyph_key.font_key,
                                                         (*native_font_handle).clone());
                        }
                    }

                    dimensions = font_context.get_glyph_dimensions(glyph_key.font_key,
                                                                   glyph_key.size,
                                                                   glyph_key.index,
                                                                   device_pixel_ratio);
                });

                *entry.insert(dimensions)
            }
        }
    }

    #[inline]
    pub fn get_image(&self,
                     image_key: ImageKey,
                     image_rendering: ImageRendering) -> CacheItem {
        debug_assert!(self.state == State::QueryResources);
        let image_info = &self.cached_images.get(&(image_key, image_rendering),
                                                 self.current_frame_id);
        let item = self.texture_cache.get(image_info.texture_cache_id);
        CacheItem {
            texture_id: SourceTexture::TextureCache(item.texture_id),
            uv0: Point2D::new(item.pixel_rect.top_left.x as f32,
                              item.pixel_rect.top_left.y as f32),
            uv1: Point2D::new(item.pixel_rect.bottom_right.x as f32,
                              item.pixel_rect.bottom_right.y as f32),
        }
    }

    pub fn get_image_properties(&self, image_key: ImageKey) -> ImageProperties {
        let image_template = &self.image_templates[&image_key];

        ImageProperties {
            format: image_template.format,
            is_opaque: image_template.is_opaque,
        }
    }

    #[inline]
    pub fn get_webgl_texture(&self, context_id: &WebGLContextId) -> CacheItem {
        let webgl_texture = &self.webgl_textures[context_id];
        CacheItem {
            texture_id: webgl_texture.id,
            uv0: Point2D::new(0.0, webgl_texture.size.height as f32),
            uv1: Point2D::new(webgl_texture.size.width as f32, 0.0),
        }
    }

    pub fn expire_old_resources(&mut self, frame_id: FrameId) {
        self.cached_glyphs.expire_old_resources(&mut self.texture_cache, frame_id);
        self.cached_images.expire_old_resources(&mut self.texture_cache, frame_id);
    }

    pub fn begin_frame(&mut self, frame_id: FrameId) {
        debug_assert!(self.state == State::Idle);
        self.state = State::AddResources;
        self.current_frame_id = frame_id;
    }

    pub fn block_until_all_resources_added(&mut self) {
        debug_assert!(self.state == State::AddResources);
        self.state = State::QueryResources;

        for request in self.pending_requests.drain(..) {
            match request {
                ResourceRequest::Image(key, rendering) => {
                    let cached_images = &mut self.cached_images;
                    let image_template = &self.image_templates[&key];

                    match cached_images.entry((key, rendering), self.current_frame_id) {
                        Occupied(entry) => {
                            let image_id = entry.get().texture_cache_id;

                            if entry.get().epoch != image_template.epoch {
                                // TODO: Can we avoid the clone of the bytes here?
                                self.texture_cache.update(image_id,
                                                          image_template.width,
                                                          image_template.height,
                                                          image_template.stride,
                                                          image_template.format,
                                                          image_template.bytes.clone());

                                // Update the cached epoch
                                *entry.into_mut() = CachedImageInfo {
                                    texture_cache_id: image_id,
                                    epoch: image_template.epoch,
                                };
                            }
                        }
                        Vacant(entry) => {
                            let image_id = self.texture_cache.new_item_id();

                            let filter = match rendering {
                                ImageRendering::Pixelated => TextureFilter::Nearest,
                                ImageRendering::Auto | ImageRendering::CrispEdges => TextureFilter::Linear,
                            };

                            // TODO: Can we avoid the clone of the bytes here?
                            self.texture_cache.insert(image_id,
                                                      image_template.width,
                                                      image_template.height,
                                                      image_template.stride,
                                                      image_template.format,
                                                      filter,
                                                      image_template.bytes.clone());

                            entry.insert(CachedImageInfo {
                                texture_cache_id: image_id,
                                epoch: image_template.epoch,
                            });
                        }
                    }
                }
                ResourceRequest::TextRun(ref text_run) => {
                    for glyph_index in &text_run.glyph_indices {
                        let glyph_key = RenderedGlyphKey::new(text_run.key,
                                                              text_run.size,
                                                              *glyph_index,
                                                              text_run.render_mode);

                        self.cached_glyphs.mark_as_needed(&glyph_key, self.current_frame_id);
                        if !self.cached_glyphs.contains_key(&glyph_key) {
                            self.pending_raster_jobs.push(GlyphRasterJob {
                                glyph_key: glyph_key,
                                result: None,
                            });
                        }
                    }
                }
            }
        }

        self.raster_pending_glyphs();
    }

    pub fn end_frame(&mut self) {
        debug_assert!(self.state == State::QueryResources);
        self.state = State::Idle;
    }
}

fn run_raster_jobs(pending_raster_jobs: &mut Vec<GlyphRasterJob>,
                   font_templates: &HashMap<FontKey, FontTemplate, BuildHasherDefault<FnvHasher>>,
                   device_pixel_ratio: f32,
                   enable_aa: bool) {
    if pending_raster_jobs.is_empty() {
        return
    }

    pending_raster_jobs.par_iter_mut().weight_max().for_each(|job| {
        let font_template = &font_templates[&job.glyph_key.key.font_key];
        FONT_CONTEXT.with(move |font_context| {
            let mut font_context = font_context.borrow_mut();
            match *font_template {
                FontTemplate::Raw(ref bytes) => {
                    font_context.add_raw_font(&job.glyph_key.key.font_key, &**bytes);
                }
                FontTemplate::Native(ref native_font_handle) => {
                    font_context.add_native_font(&job.glyph_key.key.font_key,
                                                 (*native_font_handle).clone());
                }
            }
            let render_mode = if enable_aa {
                job.glyph_key.render_mode
            } else {
                FontRenderMode::Mono
            };
            job.result = font_context.rasterize_glyph(job.glyph_key.key.font_key,
                                                      job.glyph_key.key.size,
                                                      job.glyph_key.key.index,
                                                      device_pixel_ratio,
                                                      render_mode);
        });
    });
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

// TODO(gw): If this ever shows up in profiles, consider calculating
// this lazily on demand, possibly via the resource cache thread.
// It can probably be made a lot faster with SIMD too!
// This assumes that A8 textures are never opaque, since they are
// typically used for alpha masks. We could revisit that if it
// ever becomes an issue in real world usage.
fn is_image_opaque(format: ImageFormat, bytes: &[u8]) -> bool {
    match format {
        ImageFormat::RGBA8 => {
            let mut is_opaque = true;
            for i in 0..(bytes.len() / 4) {
                if bytes[i * 4 + 3] != 255 {
                    is_opaque = false;
                    break;
                }
            }
            is_opaque
        }
        ImageFormat::RGB8 => true,
        ImageFormat::A8 => false,
        ImageFormat::Invalid | ImageFormat::RGBAF32 => unreachable!(),
    }
}
