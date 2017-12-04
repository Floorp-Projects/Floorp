/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#[cfg(test)]
use api::{IdNamespace, LayoutPoint};
use api::{ColorF, ColorU, DevicePoint, DeviceUintSize};
use api::{FontInstanceFlags, FontInstancePlatformOptions};
use api::{FontKey, FontRenderMode, FontTemplate, FontVariation};
use api::{GlyphDimensions, GlyphKey, SubpixelDirection};
use api::{ImageData, ImageDescriptor, ImageFormat, LayerToWorldTransform};
use app_units::Au;
use device::TextureFilter;
use glyph_cache::{CachedGlyphInfo, GlyphCache};
use gpu_cache::GpuCache;
use internal_types::FastHashSet;
use platform::font::FontContext;
use profiler::TextureCacheProfileCounters;
use rayon::ThreadPool;
use rayon::prelude::*;
use std::cmp;
use std::collections::hash_map::Entry;
use std::hash::{Hash, Hasher};
use std::mem;
use std::sync::{Arc, Mutex, MutexGuard};
use std::sync::mpsc::{channel, Receiver, Sender};
use texture_cache::{TextureCache, TextureCacheHandle};

#[derive(Clone, Copy, Debug, PartialEq, PartialOrd)]
pub struct FontTransform {
    pub scale_x: f32,
    pub skew_x: f32,
    pub skew_y: f32,
    pub scale_y: f32,
}

// Floats don't impl Hash/Eq/Ord...
impl Eq for FontTransform {}
impl Ord for FontTransform {
    fn cmp(&self, other: &Self) -> cmp::Ordering {
        self.partial_cmp(other).unwrap_or(cmp::Ordering::Equal)
    }
}
impl Hash for FontTransform {
    fn hash<H: Hasher>(&self, state: &mut H) {
        // Note: this is inconsistent with the Eq impl for -0.0 (don't care).
        self.scale_x.to_bits().hash(state);
        self.skew_x.to_bits().hash(state);
        self.skew_y.to_bits().hash(state);
        self.scale_y.to_bits().hash(state);
    }
}

impl FontTransform {
    const QUANTIZE_SCALE: f32 = 1024.0;

    pub fn new(scale_x: f32, skew_x: f32, skew_y: f32, scale_y: f32) -> Self {
        FontTransform { scale_x, skew_x, skew_y, scale_y }
    }

    pub fn identity() -> Self {
        FontTransform::new(1.0, 0.0, 0.0, 1.0)
    }

    pub fn is_identity(&self) -> bool {
        *self == FontTransform::identity()
    }

    pub fn quantize(&self) -> Self {
        FontTransform::new(
            (self.scale_x * Self::QUANTIZE_SCALE).round() / Self::QUANTIZE_SCALE,
            (self.skew_x * Self::QUANTIZE_SCALE).round() / Self::QUANTIZE_SCALE,
            (self.skew_y * Self::QUANTIZE_SCALE).round() / Self::QUANTIZE_SCALE,
            (self.scale_y * Self::QUANTIZE_SCALE).round() / Self::QUANTIZE_SCALE,
        )
    }

    pub fn determinant(&self) -> f64 {
        self.scale_x as f64 * self.scale_y as f64 - self.skew_y as f64 * self.skew_x as f64
    }

    pub fn compute_scale(&self) -> Option<(f64, f64)> {
        let det = self.determinant();
        if det != 0.0 {
            let major = (self.scale_x as f64).hypot(self.skew_y as f64);
            let minor = det.abs() / major;
            Some((major, minor))
        } else {
            None
        }
    }

    pub fn pre_scale(&self, scale_x: f32, scale_y: f32) -> Self {
        FontTransform::new(
            self.scale_x * scale_x,
            self.skew_x * scale_y,
            self.skew_y * scale_x,
            self.scale_y * scale_y,
        )
    }

    #[allow(dead_code)]
    pub fn inverse(&self) -> Option<Self> {
        let det = self.determinant();
        if det != 0.0 {
            let inv_det = det.recip() as f32;
            Some(FontTransform::new(
                self.scale_y * inv_det,
                -self.skew_x * inv_det,
                -self.skew_y * inv_det,
                self.scale_x * inv_det
            ))
        } else {
            None
        }
    }

    #[allow(dead_code)]
    pub fn apply(&self, x: f32, y: f32) -> (f32, f32) {
        (self.scale_x * x + self.skew_x * y, self.skew_y * x + self.scale_y * y)
    }
}

impl<'a> From<&'a LayerToWorldTransform> for FontTransform {
    fn from(xform: &'a LayerToWorldTransform) -> Self {
        FontTransform::new(xform.m11, xform.m21, xform.m12, xform.m22)
    }
}

#[derive(Clone, Hash, PartialEq, Eq, Debug, Ord, PartialOrd)]
pub struct FontInstance {
    pub font_key: FontKey,
    // The font size is in *device* pixels, not logical pixels.
    // It is stored as an Au since we need sub-pixel sizes, but
    // can't store as a f32 due to use of this type as a hash key.
    // TODO(gw): Perhaps consider having LogicalAu and DeviceAu
    //           or something similar to that.
    pub size: Au,
    pub color: ColorU,
    pub bg_color: ColorU,
    pub render_mode: FontRenderMode,
    pub subpx_dir: SubpixelDirection,
    pub flags: FontInstanceFlags,
    pub platform_options: Option<FontInstancePlatformOptions>,
    pub variations: Vec<FontVariation>,
    pub transform: FontTransform,
}

impl FontInstance {
    pub fn new(
        font_key: FontKey,
        size: Au,
        color: ColorF,
        bg_color: ColorU,
        render_mode: FontRenderMode,
        subpx_dir: SubpixelDirection,
        flags: FontInstanceFlags,
        platform_options: Option<FontInstancePlatformOptions>,
        variations: Vec<FontVariation>,
    ) -> Self {
        FontInstance {
            font_key,
            size,
            color: color.into(),
            bg_color,
            render_mode,
            subpx_dir,
            flags,
            platform_options,
            variations,
            transform: FontTransform::identity(),
        }
    }

    pub fn get_subpx_offset(&self, glyph: &GlyphKey) -> (f64, f64) {
        match self.subpx_dir {
            SubpixelDirection::None => (0.0, 0.0),
            SubpixelDirection::Horizontal => (glyph.subpixel_offset.into(), 0.0),
            SubpixelDirection::Vertical => (0.0, glyph.subpixel_offset.into()),
        }
    }

    pub fn get_glyph_format(&self, color_bitmaps: bool) -> GlyphFormat {
        match self.render_mode {
            FontRenderMode::Mono | FontRenderMode::Alpha => {
                if self.transform.is_identity() { GlyphFormat::Alpha } else { GlyphFormat::TransformedAlpha }
            }
            FontRenderMode::Subpixel => {
                if self.transform.is_identity() { GlyphFormat::Subpixel } else { GlyphFormat::TransformedSubpixel }
            }
            FontRenderMode::Bitmap => {
                if color_bitmaps { GlyphFormat::ColorBitmap } else { GlyphFormat::Alpha }
            }
        }
    }
}

#[derive(Copy, Clone, PartialEq, Eq, Hash, Debug)]
pub enum GlyphFormat {
    Alpha,
    TransformedAlpha,
    Subpixel,
    TransformedSubpixel,
    ColorBitmap,
}

pub struct RasterizedGlyph {
    pub top: f32,
    pub left: f32,
    pub width: u32,
    pub height: u32,
    pub scale: f32,
    pub format: GlyphFormat,
    pub bytes: Vec<u8>,
}

pub struct FontContexts {
    // These worker are mostly accessed from their corresponding worker threads.
    // The goal is that there should be no noticeable contention on the muteces.
    worker_contexts: Vec<Mutex<FontContext>>,

    // This worker should be accessed by threads that don't belong to thre thread pool
    // (in theory that's only the render backend thread so no contention expected either).
    shared_context: Mutex<FontContext>,

    // Stored here as a convenience to get the current thread index.
    workers: Arc<ThreadPool>,
}

impl FontContexts {
    /// Get access to the font context associated to the current thread.
    pub fn lock_current_context(&self) -> MutexGuard<FontContext> {
        let id = self.current_worker_id();
        self.lock_context(id)
    }

    /// Get access to any particular font context.
    ///
    /// The id is ```Some(i)``` where i is an index between 0 and num_worker_contexts
    /// for font contexts associated to the thread pool, and None for the shared
    /// global font context for use outside of the thread pool.
    pub fn lock_context(&self, id: Option<usize>) -> MutexGuard<FontContext> {
        match id {
            Some(index) => self.worker_contexts[index].lock().unwrap(),
            None => self.shared_context.lock().unwrap(),
        }
    }

    /// Get access to the font context usable outside of the thread pool.
    pub fn lock_shared_context(&self) -> MutexGuard<FontContext> {
        self.shared_context.lock().unwrap()
    }

    // number of contexts associated to workers
    pub fn num_worker_contexts(&self) -> usize {
        self.worker_contexts.len()
    }

    fn current_worker_id(&self) -> Option<usize> {
        self.workers.current_thread_index()
    }
}

pub struct GlyphRasterizer {
    workers: Arc<ThreadPool>,
    font_contexts: Arc<FontContexts>,

    // Maintain a set of glyphs that have been requested this
    // frame. This ensures the glyph thread won't rasterize
    // the same glyph more than once in a frame. This is required
    // because the glyph cache hash table is not updated
    // until the end of the frame when we wait for glyph requests
    // to be resolved.
    pending_glyphs: FastHashSet<GlyphRequest>,

    // Receives the rendered glyphs.
    glyph_rx: Receiver<Vec<GlyphRasterJob>>,
    glyph_tx: Sender<Vec<GlyphRasterJob>>,

    // We defer removing fonts to the end of the frame so that:
    // - this work is done outside of the critical path,
    // - we don't have to worry about the ordering of events if a font is used on
    //   a frame where it is used (although it seems unlikely).
    fonts_to_remove: Vec<FontKey>,
}

impl GlyphRasterizer {
    pub fn new(workers: Arc<ThreadPool>) -> Self {
        let (glyph_tx, glyph_rx) = channel();

        let num_workers = workers.current_num_threads();
        let mut contexts = Vec::with_capacity(num_workers);

        for _ in 0 .. num_workers {
            contexts.push(Mutex::new(FontContext::new()));
        }

        GlyphRasterizer {
            font_contexts: Arc::new(FontContexts {
                worker_contexts: contexts,
                shared_context: Mutex::new(FontContext::new()),
                workers: Arc::clone(&workers),
            }),
            pending_glyphs: FastHashSet::default(),
            glyph_rx,
            glyph_tx,
            workers,
            fonts_to_remove: Vec::new(),
        }
    }

    pub fn add_font(&mut self, font_key: FontKey, template: FontTemplate) {
        let font_contexts = Arc::clone(&self.font_contexts);
        // It's important to synchronously add the font for the shared context because
        // we use it to check that fonts have been properly added when requesting glyphs.
        font_contexts
            .lock_shared_context()
            .add_font(&font_key, &template);

        // TODO: this locks each font context while adding the font data, probably not a big deal,
        // but if there is contention on this lock we could easily have a queue of per-context
        // operations to add and delete fonts, and have these queues lazily processed by each worker
        // before rendering a glyph.
        // We can also move this into a worker to free up some cycles in the calling (render backend)
        // thread.
        for i in 0 .. font_contexts.num_worker_contexts() {
            font_contexts
                .lock_context(Some(i))
                .add_font(&font_key, &template);
        }
    }

    pub fn delete_font(&mut self, font_key: FontKey) {
        self.fonts_to_remove.push(font_key);
    }

    pub fn prepare_font(&self, font: &mut FontInstance) {
        FontContext::prepare_font(font);
    }

    pub fn request_glyphs(
        &mut self,
        glyph_cache: &mut GlyphCache,
        font: FontInstance,
        glyph_keys: &[GlyphKey],
        texture_cache: &mut TextureCache,
        gpu_cache: &mut GpuCache,
    ) {
        assert!(
            self.font_contexts
                .lock_shared_context()
                .has_font(&font.font_key)
        );
        let mut glyphs = Vec::new();

        let glyph_key_cache = glyph_cache.get_glyph_key_cache_for_font_mut(font.clone());

        // select glyphs that have not been requested yet.
        for key in glyph_keys {
            match glyph_key_cache.entry(key.clone()) {
                Entry::Occupied(mut entry) => {
                    if let Ok(Some(ref mut glyph_info)) = *entry.get_mut() {
                        if texture_cache.request(&mut glyph_info.texture_cache_handle, gpu_cache) {
                            // This case gets hit when we have already rasterized
                            // the glyph and stored it in CPU memory, the the glyph
                            // has been evicted from the texture cache. In which case
                            // we need to re-upload it to the GPU.
                            texture_cache.update(
                                &mut glyph_info.texture_cache_handle,
                                ImageDescriptor {
                                    width: glyph_info.size.width,
                                    height: glyph_info.size.height,
                                    stride: None,
                                    format: ImageFormat::BGRA8,
                                    is_opaque: false,
                                    offset: 0,
                                },
                                TextureFilter::Linear,
                                ImageData::Raw(glyph_info.glyph_bytes.clone()),
                                [glyph_info.offset.x, glyph_info.offset.y, glyph_info.scale],
                                None,
                                gpu_cache,
                            );
                        }
                    }
                }
                Entry::Vacant(..) => {
                    let request = GlyphRequest::new(&font, key);
                    if self.pending_glyphs.insert(request.clone()) {
                        glyphs.push(request);
                    }
                }
            }
        }

        if glyphs.is_empty() {
            return;
        }

        let font_contexts = Arc::clone(&self.font_contexts);
        let glyph_tx = self.glyph_tx.clone();
        // spawn an async task to get off of the render backend thread as early as
        // possible and in that task use rayon's fork join dispatch to rasterize the
        // glyphs in the thread pool.
        self.workers.spawn(move || {
            let jobs = glyphs
                .par_iter()
                .map(|request: &GlyphRequest| {
                    profile_scope!("glyph-raster");
                    let mut context = font_contexts.lock_current_context();
                    let job = GlyphRasterJob {
                        request: request.clone(),
                        result: context.rasterize_glyph(&request.font, &request.key),
                    };

                    // Sanity check.
                    if let Some(ref glyph) = job.result {
                        let bpp = 4; // We always render glyphs in 32 bits RGBA format.
                        assert_eq!(
                            glyph.bytes.len(),
                            bpp * (glyph.width * glyph.height) as usize
                        );
                    }

                    job
                })
                .collect();

            glyph_tx.send(jobs).unwrap();
        });
    }

    pub fn get_glyph_dimensions(
        &mut self,
        font: &FontInstance,
        glyph_key: &GlyphKey,
    ) -> Option<GlyphDimensions> {
        self.font_contexts
            .lock_shared_context()
            .get_glyph_dimensions(font, glyph_key)
    }

    pub fn is_bitmap_font(&self, font: &FontInstance) -> bool {
        self.font_contexts
            .lock_shared_context()
            .is_bitmap_font(font)
    }

    pub fn get_glyph_index(&mut self, font_key: FontKey, ch: char) -> Option<u32> {
        self.font_contexts
            .lock_shared_context()
            .get_glyph_index(font_key, ch)
    }

    pub fn resolve_glyphs(
        &mut self,
        glyph_cache: &mut GlyphCache,
        texture_cache: &mut TextureCache,
        gpu_cache: &mut GpuCache,
        _texture_cache_profile: &mut TextureCacheProfileCounters,
    ) {
        let mut rasterized_glyphs = Vec::with_capacity(self.pending_glyphs.len());

        // Pull rasterized glyphs from the queue.

        while !self.pending_glyphs.is_empty() {
            // TODO: rather than blocking until all pending glyphs are available
            // we could try_recv and steal work from the thread pool to take advantage
            // of the fact that this thread is alive and we avoid the added latency
            // of blocking it.
            let raster_jobs = self.glyph_rx
                .recv()
                .expect("BUG: Should be glyphs pending!");
            for job in raster_jobs {
                debug_assert!(self.pending_glyphs.contains(&job.request));
                self.pending_glyphs.remove(&job.request);

                rasterized_glyphs.push(job);
            }
        }

        // Ensure that the glyphs are always processed in the same
        // order for a given text run (since iterating a hash set doesn't
        // guarantee order). This can show up as very small float inaccuacry
        // differences in rasterizers due to the different coordinates
        // that text runs get associated with by the texture cache allocator.
        rasterized_glyphs.sort_by(|a, b| a.request.cmp(&b.request));

        // Update the caches.
        for job in rasterized_glyphs {
            let glyph_info = job.result
                .and_then(|glyph| if glyph.width > 0 && glyph.height > 0 {
                    assert_eq!((glyph.left.fract(), glyph.top.fract()), (0.0, 0.0));
                    let glyph_bytes = Arc::new(glyph.bytes);
                    let mut texture_cache_handle = TextureCacheHandle::new();
                    texture_cache.request(&mut texture_cache_handle, gpu_cache);
                    texture_cache.update(
                        &mut texture_cache_handle,
                        ImageDescriptor {
                            width: glyph.width,
                            height: glyph.height,
                            stride: None,
                            format: ImageFormat::BGRA8,
                            is_opaque: false,
                            offset: 0,
                        },
                        TextureFilter::Linear,
                        ImageData::Raw(glyph_bytes.clone()),
                        [glyph.left, -glyph.top, glyph.scale],
                        None,
                        gpu_cache,
                    );
                    Some(CachedGlyphInfo {
                        texture_cache_handle,
                        glyph_bytes,
                        size: DeviceUintSize::new(glyph.width, glyph.height),
                        offset: DevicePoint::new(glyph.left, -glyph.top),
                        scale: glyph.scale,
                        format: glyph.format,
                    })
                } else {
                    None
                });

            let glyph_key_cache = glyph_cache.get_glyph_key_cache_for_font_mut(job.request.font);

            glyph_key_cache.insert(job.request.key, Ok(glyph_info));
        }

        // Now that we are done with the critical path (rendering the glyphs),
        // we can schedule removing the fonts if needed.
        if !self.fonts_to_remove.is_empty() {
            let font_contexts = Arc::clone(&self.font_contexts);
            let fonts_to_remove = mem::replace(&mut self.fonts_to_remove, Vec::new());
            self.workers.spawn(move || {
                for font_key in &fonts_to_remove {
                    font_contexts.lock_shared_context().delete_font(font_key);
                }
                for i in 0 .. font_contexts.num_worker_contexts() {
                    let mut context = font_contexts.lock_context(Some(i));
                    for font_key in &fonts_to_remove {
                        context.delete_font(font_key);
                    }
                }
            });
        }
    }
}

impl FontContext {
    fn add_font(&mut self, font_key: &FontKey, template: &FontTemplate) {
        match template {
            &FontTemplate::Raw(ref bytes, index) => {
                self.add_raw_font(&font_key, bytes.clone(), index);
            }
            &FontTemplate::Native(ref native_font_handle) => {
                self.add_native_font(&font_key, (*native_font_handle).clone());
            }
        }
    }
}

#[derive(Clone, Hash, PartialEq, Eq, Debug, Ord, PartialOrd)]
pub struct GlyphRequest {
    pub key: GlyphKey,
    pub font: FontInstance,
}

impl GlyphRequest {
    pub fn new(font: &FontInstance, key: &GlyphKey) -> Self {
        GlyphRequest {
            key: key.clone(),
            font: font.clone(),
        }
    }
}

struct GlyphRasterJob {
    request: GlyphRequest,
    result: Option<RasterizedGlyph>,
}

#[test]
fn raterize_200_glyphs() {
    // This test loads a font from disc, the renders 4 requests containing
    // 50 glyphs each, deletes the font and waits for the result.

    use rayon::Configuration;
    use std::fs::File;
    use std::io::Read;

    let workers = Arc::new(ThreadPool::new(Configuration::new()).unwrap());
    let mut glyph_rasterizer = GlyphRasterizer::new(workers);
    let mut glyph_cache = GlyphCache::new();
    let mut gpu_cache = GpuCache::new();
    let mut texture_cache = TextureCache::new(2048);

    let mut font_file =
        File::open("../wrench/reftests/text/VeraBd.ttf").expect("Couldn't open font file");
    let mut font_data = vec![];
    font_file
        .read_to_end(&mut font_data)
        .expect("failed to read font file");

    let font_key = FontKey::new(IdNamespace(0), 0);
    glyph_rasterizer.add_font(font_key, FontTemplate::Raw(Arc::new(font_data), 0));

    let font = FontInstance::new(
        font_key,
        Au::from_px(32),
        ColorF::new(0.0, 0.0, 0.0, 1.0),
        ColorU::new(0, 0, 0, 0),
        FontRenderMode::Subpixel,
        SubpixelDirection::Horizontal,
        Default::default(),
        None,
        Vec::new(),
    );

    let mut glyph_keys = Vec::with_capacity(200);
    for i in 0 .. 200 {
        glyph_keys.push(GlyphKey::new(
            i,
            LayoutPoint::zero(),
            font.render_mode,
            font.subpx_dir,
        ));
    }

    for i in 0 .. 4 {
        glyph_rasterizer.request_glyphs(
            &mut glyph_cache,
            font.clone(),
            &glyph_keys[(50 * i) .. (50 * (i + 1))],
            &mut texture_cache,
            &mut gpu_cache,
        );
    }

    glyph_rasterizer.delete_font(font_key);

    glyph_rasterizer.resolve_glyphs(
        &mut glyph_cache,
        &mut TextureCache::new(4096),
        &mut gpu_cache,
        &mut TextureCacheProfileCounters::new(),
    );
}
