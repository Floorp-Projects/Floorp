/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{ColorF, ColorU};
use api::{FontInstanceFlags, FontInstancePlatformOptions};
use api::{FontKey, FontRenderMode, FontTemplate, FontVariation};
use api::{GlyphDimensions, GlyphKey, LayoutToWorldTransform, SubpixelDirection};
use app_units::Au;
use internal_types::ResourceCacheError;
use platform::font::FontContext;
use rayon::ThreadPool;
use std::cmp;
use std::hash::{Hash, Hasher};
use std::mem;
use std::sync::{Arc, Mutex, MutexGuard};
use std::sync::mpsc::{channel, Receiver, Sender};

#[cfg(feature = "pathfinder")]
mod pathfinder;
#[cfg(feature = "pathfinder")]
use self::pathfinder::create_pathfinder_font_context;
#[cfg(feature = "pathfinder")]
pub use self::pathfinder::{ThreadSafePathfinderFontContext, NativeFontHandleWrapper};

#[cfg(not(feature = "pathfinder"))]
mod no_pathfinder;

#[derive(Clone, Copy, Debug, PartialEq, PartialOrd)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
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

    #[allow(dead_code)]
    pub fn determinant(&self) -> f64 {
        self.scale_x as f64 * self.scale_y as f64 - self.skew_y as f64 * self.skew_x as f64
    }

    #[allow(dead_code)]
    pub fn compute_scale(&self) -> Option<(f64, f64)> {
        let det = self.determinant();
        if det != 0.0 {
            let x_scale = (self.scale_x as f64).hypot(self.skew_y as f64);
            let y_scale = det.abs() / x_scale;
            Some((x_scale, y_scale))
        } else {
            None
        }
    }

    #[allow(dead_code)]
    pub fn pre_scale(&self, scale_x: f32, scale_y: f32) -> Self {
        FontTransform::new(
            self.scale_x * scale_x,
            self.skew_x * scale_y,
            self.skew_y * scale_x,
            self.scale_y * scale_y,
        )
    }

    #[allow(dead_code)]
    pub fn invert_scale(&self, x_scale: f64, y_scale: f64) -> Self {
        self.pre_scale(x_scale.recip() as f32, y_scale.recip() as f32)
    }

    pub fn synthesize_italics(&self, skew_factor: f32) -> Self {
        FontTransform::new(
            self.scale_x,
            self.skew_x - self.scale_x * skew_factor,
            self.skew_y,
            self.scale_y - self.skew_y * skew_factor,
        )
    }

    pub fn swap_xy(&self) -> Self {
        FontTransform::new(self.skew_x, self.scale_x, self.scale_y, self.skew_y)
    }

    pub fn flip_x(&self) -> Self {
        FontTransform::new(-self.scale_x, self.skew_x, -self.skew_y, self.scale_y)
    }

    pub fn flip_y(&self) -> Self {
        FontTransform::new(self.scale_x, -self.skew_y, self.skew_y, -self.scale_y)
    }
}

impl<'a> From<&'a LayoutToWorldTransform> for FontTransform {
    fn from(xform: &'a LayoutToWorldTransform) -> Self {
        FontTransform::new(xform.m11, xform.m21, xform.m12, xform.m22)
    }
}

#[derive(Clone, Hash, PartialEq, Eq, Debug, Ord, PartialOrd)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
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
        // If a background color is enabled, it only makes sense
        // for it to be completely opaque.
        debug_assert!(bg_color.a == 0 || bg_color.a == 255);

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

    pub fn get_alpha_glyph_format(&self) -> GlyphFormat {
        if self.transform.is_identity() { GlyphFormat::Alpha } else { GlyphFormat::TransformedAlpha }
    }

    pub fn get_subpixel_glyph_format(&self) -> GlyphFormat {
        if self.transform.is_identity() { GlyphFormat::Subpixel } else { GlyphFormat::TransformedSubpixel }
    }

    #[allow(dead_code)]
    pub fn get_subpx_offset(&self, glyph: &GlyphKey) -> (f64, f64) {
        match self.subpx_dir {
            SubpixelDirection::None => (0.0, 0.0),
            SubpixelDirection::Horizontal => (glyph.subpixel_offset.into(), 0.0),
            SubpixelDirection::Vertical => (0.0, glyph.subpixel_offset.into()),
        }
    }

    #[allow(dead_code)]
    pub fn get_glyph_format(&self) -> GlyphFormat {
        match self.render_mode {
            FontRenderMode::Mono | FontRenderMode::Alpha => self.get_alpha_glyph_format(),
            FontRenderMode::Subpixel => self.get_subpixel_glyph_format(),
        }
    }

    #[allow(dead_code)]
    pub fn get_extra_strikes(&self, x_scale: f64) -> usize {
        if self.flags.contains(FontInstanceFlags::SYNTHETIC_BOLD) {
            let mut bold_offset = self.size.to_f64_px() / 48.0;
            if bold_offset < 1.0 {
                bold_offset = 0.25 + 0.75 * bold_offset;
            }
            (bold_offset * x_scale).max(1.0).round() as usize
        } else {
            0
        }
    }
}

#[derive(Copy, Clone, PartialEq, Eq, Hash, Debug)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[allow(dead_code)]
pub enum GlyphFormat {
    Alpha,
    TransformedAlpha,
    Subpixel,
    TransformedSubpixel,
    Bitmap,
    ColorBitmap,
}

impl GlyphFormat {
    pub fn ignore_color(self) -> Self {
        match self {
            GlyphFormat::ColorBitmap => GlyphFormat::Bitmap,
            _ => self,
        }
    }
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
    // The goal is that there should be no noticeable contention on the mutexes.
    worker_contexts: Vec<Mutex<FontContext>>,
    // This worker should be accessed by threads that don't belong to the thread pool
    // (in theory that's only the render backend thread so no contention expected either).
    shared_context: Mutex<FontContext>,
    #[cfg(feature = "pathfinder")]
    pathfinder_context: Box<ThreadSafePathfinderFontContext>,
    // Stored here as a convenience to get the current thread index.
    #[allow(dead_code)]
    workers: Arc<ThreadPool>,
}

impl FontContexts {

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
}

pub struct GlyphRasterizer {
    #[allow(dead_code)]
    workers: Arc<ThreadPool>,
    font_contexts: Arc<FontContexts>,

    // Maintain a set of glyphs that have been requested this
    // frame. This ensures the glyph thread won't rasterize
    // the same glyph more than once in a frame. This is required
    // because the glyph cache hash table is not updated
    // until the end of the frame when we wait for glyph requests
    // to be resolved.
    #[allow(dead_code)]
    pending_glyphs: usize,

    // Receives the rendered glyphs.
    #[allow(dead_code)]
    glyph_rx: Receiver<GlyphRasterJobs>,
    #[allow(dead_code)]
    glyph_tx: Sender<GlyphRasterJobs>,

    // We defer removing fonts to the end of the frame so that:
    // - this work is done outside of the critical path,
    // - we don't have to worry about the ordering of events if a font is used on
    //   a frame where it is used (although it seems unlikely).
    fonts_to_remove: Vec<FontKey>,

    #[allow(dead_code)]
    next_gpu_glyph_cache_key: GpuGlyphCacheKey,
}

impl GlyphRasterizer {
    pub fn new(workers: Arc<ThreadPool>) -> Result<Self, ResourceCacheError> {
        let (glyph_tx, glyph_rx) = channel();

        let num_workers = workers.current_num_threads();
        let mut contexts = Vec::with_capacity(num_workers);

        let shared_context = FontContext::new()?;

        for _ in 0 .. num_workers {
            contexts.push(Mutex::new(FontContext::new()?));
        }

        let font_context = FontContexts {
                worker_contexts: contexts,
                shared_context: Mutex::new(shared_context),
                #[cfg(feature = "pathfinder")]
                pathfinder_context: create_pathfinder_font_context()?,
                workers: Arc::clone(&workers),
        };

        Ok(GlyphRasterizer {
            font_contexts: Arc::new(font_context),
            pending_glyphs: 0,
            glyph_rx,
            glyph_tx,
            workers,
            fonts_to_remove: Vec::new(),
            next_gpu_glyph_cache_key: GpuGlyphCacheKey(0),
        })
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

        #[cfg(feature = "pathfinder")]
        self.add_font_to_pathfinder(&font_key, &template);
    }

    pub fn delete_font(&mut self, font_key: FontKey) {
        self.fonts_to_remove.push(font_key);
    }

    pub fn prepare_font(&self, font: &mut FontInstance) {
        FontContext::prepare_font(font);
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

    pub fn get_glyph_index(&mut self, font_key: FontKey, ch: char) -> Option<u32> {
        self.font_contexts
            .lock_shared_context()
            .get_glyph_index(font_key, ch)
    }

    fn remove_dead_fonts(&mut self) {
        if self.fonts_to_remove.is_empty() {
            return
        }

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

    #[cfg(feature = "replay")]
    pub fn reset(&mut self) {
        //TODO: any signals need to be sent to the workers?
        self.pending_glyphs = 0;
        self.fonts_to_remove.clear();
    }
}

trait AddFont {
    fn add_font(&mut self, font_key: &FontKey, template: &FontTemplate);
}

impl AddFont for FontContext {
    fn add_font(&mut self, font_key: &FontKey, template: &FontTemplate) {
        match *template {
            FontTemplate::Raw(ref bytes, index) => {
                self.add_raw_font(font_key, bytes.clone(), index);
            }
            FontTemplate::Native(ref native_font_handle) => {
                self.add_native_font(font_key, (*native_font_handle).clone());
            }
        }
    }
}

#[derive(Clone, Hash, PartialEq, Eq, Debug, Ord, PartialOrd)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
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

#[allow(dead_code)]
pub(in glyph_rasterizer) struct GlyphRasterJob {
    key: GlyphKey,
    result: GlyphRasterResult,
}

#[allow(dead_code)]
pub enum GlyphRasterResult {
    LoadFailed,
    Bitmap(RasterizedGlyph),
}

#[derive(Debug, Copy, Clone, Eq, Hash, PartialEq)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct GpuGlyphCacheKey(pub u32);

#[allow(dead_code)]
struct GlyphRasterJobs {
    font: FontInstance,
    jobs: Vec<GlyphRasterJob>,
}

#[cfg(test)]
mod test_glyph_rasterizer {
    #[test]
    fn rasterize_200_glyphs() {
        // This test loads a font from disc, the renders 4 requests containing
        // 50 glyphs each, deletes the font and waits for the result.

        use rayon::ThreadPoolBuilder;
        use std::fs::File;
        use std::io::Read;
        use texture_cache::TextureCache;
        use glyph_cache::GlyphCache;
        use gpu_cache::GpuCache;
        use tiling::SpecialRenderPasses;
        use api::DeviceIntSize;
        use render_task::{RenderTaskCache, RenderTaskTree};
        use profiler::TextureCacheProfileCounters;
        use api::{FontKey, FontTemplate, FontRenderMode, GlyphKey,
                  IdNamespace, LayoutPoint, ColorF, ColorU, SubpixelDirection};
        use render_backend::FrameId;
        use app_units::Au;
        use thread_profiler::register_thread_with_profiler;
        use std::sync::Arc;
        use glyph_rasterizer::{GlyphRasterizer, FontInstance};

        let worker = ThreadPoolBuilder::new()
            .thread_name(|idx|{ format!("WRWorker#{}", idx) })
            .start_handler(move |idx| {
                register_thread_with_profiler(format!("WRWorker#{}", idx));
            })
            .build();
        let workers = Arc::new(worker.unwrap());
        let mut glyph_rasterizer = GlyphRasterizer::new(workers).unwrap();
        let mut glyph_cache = GlyphCache::new();
        let mut gpu_cache = GpuCache::new();
        let mut texture_cache = TextureCache::new(2048);
        let mut render_task_cache = RenderTaskCache::new();
        let mut render_task_tree = RenderTaskTree::new(FrameId(0));
        let mut special_render_passes = SpecialRenderPasses::new(&DeviceIntSize::new(1366, 768));

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
                &mut render_task_cache,
                &mut render_task_tree,
                &mut special_render_passes,
            );
        }

        glyph_rasterizer.delete_font(font_key);

        glyph_rasterizer.resolve_glyphs(
            &mut glyph_cache,
            &mut TextureCache::new(4096),
            &mut gpu_cache,
            &mut render_task_cache,
            &mut render_task_tree,
            &mut TextureCacheProfileCounters::new(),
        );
    }
}