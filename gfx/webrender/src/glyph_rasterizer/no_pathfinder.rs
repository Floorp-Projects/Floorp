/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! Module only available when pathfinder is deactivated when webrender is
//! compiled regularly (i.e. any configuration without feature = "pathfinder")

use api::{GlyphKey, ImageData, ImageDescriptor, ImageFormat};
use device::TextureFilter;
use euclid::size2;
use gpu_types::UvRectKind;
use rayon::prelude::*;
use std::sync::{Arc, MutexGuard};
use platform::font::FontContext;
use glyph_rasterizer::{FontInstance, FontContexts, GlyphRasterizer, GlyphRasterJob, GlyphRasterJobs, GlyphRasterResult};
use glyph_cache::{GlyphCache, CachedGlyphInfo, GlyphCacheEntry};
use texture_cache::{TextureCache, TextureCacheHandle};
use gpu_cache::GpuCache;
use render_task::{RenderTaskTree, RenderTaskCache};
use tiling::SpecialRenderPasses;
use profiler::TextureCacheProfileCounters;
use std::collections::hash_map::Entry;

impl FontContexts {
    /// Get access to the font context associated to the current thread.
    pub fn lock_current_context(&self) -> MutexGuard<FontContext> {
        let id = self.current_worker_id();
        self.lock_context(id)
    }

    pub(in super) fn current_worker_id(&self) -> Option<usize> {
        self.workers.current_thread_index()
    }
}

impl GlyphRasterizer {

    pub fn request_glyphs(
        &mut self,
        glyph_cache: &mut GlyphCache,
        font: FontInstance,
        glyph_keys: &[GlyphKey],
        texture_cache: &mut TextureCache,
        gpu_cache: &mut GpuCache,
        _: &mut RenderTaskCache,
        _: &mut RenderTaskTree,
        _: &mut SpecialRenderPasses,
    ) {
        assert!(
            self.font_contexts
                .lock_shared_context()
                .has_font(&font.font_key)
        );
        let mut new_glyphs = Vec::new();

        let glyph_key_cache = glyph_cache.get_glyph_key_cache_for_font_mut(font.clone());

        // select glyphs that have not been requested yet.
        for key in glyph_keys {
            match glyph_key_cache.entry(key.clone()) {
                Entry::Occupied(mut entry) => {
                    let value = entry.into_mut();
                    match *value {
                        GlyphCacheEntry::Cached(ref glyph) => {
                            // Skip the glyph if it is already has a valid texture cache handle.
                            if !texture_cache.request(&glyph.texture_cache_handle, gpu_cache) {
                                continue;
                            }
                        }
                        // Otherwise, skip the entry if it is blank or pending.
                        GlyphCacheEntry::Blank | GlyphCacheEntry::Pending => continue,
                    }

                    // This case gets hit when we already rasterized the glyph, but the
                    // glyph has been evicted from the texture cache. Just force it to
                    // pending so it gets rematerialized.
                    *value = GlyphCacheEntry::Pending;
                    new_glyphs.push((*key).clone());
                }
                Entry::Vacant(entry) => {
                    // This is the first time we've seen the glyph, so mark it as pending.
                    entry.insert(GlyphCacheEntry::Pending);
                    new_glyphs.push((*key).clone());
                }
            }
        }

        if new_glyphs.is_empty() {
            return;
        }

        self.pending_glyphs += 1;

        self.request_glyphs_from_backend(font, new_glyphs);
    }

    pub(in super) fn request_glyphs_from_backend(&mut self, font: FontInstance, glyphs: Vec<GlyphKey>) {
        let font_contexts = Arc::clone(&self.font_contexts);
        let glyph_tx = self.glyph_tx.clone();

        // spawn an async task to get off of the render backend thread as early as
        // possible and in that task use rayon's fork join dispatch to rasterize the
        // glyphs in the thread pool.
        self.workers.spawn(move || {
            let jobs = glyphs
                .par_iter()
                .map(|key: &GlyphKey| {
                    profile_scope!("glyph-raster");
                    let mut context = font_contexts.lock_current_context();
                    let job = GlyphRasterJob {
                        key: key.clone(),
                        result: context.rasterize_glyph(&font, key),
                    };

                    // Sanity check.
                    if let GlyphRasterResult::Bitmap(ref glyph) = job.result {
                        let bpp = 4; // We always render glyphs in 32 bits RGBA format.
                        assert_eq!(
                            glyph.bytes.len(),
                            bpp * (glyph.width * glyph.height) as usize
                        );
                    }

                    job
                })
                .collect();

            glyph_tx.send(GlyphRasterJobs { font, jobs }).unwrap();
        });
    }

    pub fn resolve_glyphs(
        &mut self,
        glyph_cache: &mut GlyphCache,
        texture_cache: &mut TextureCache,
        gpu_cache: &mut GpuCache,
        _: &mut RenderTaskCache,
        _: &mut RenderTaskTree,
        _: &mut TextureCacheProfileCounters,
    ) {
        // Pull rasterized glyphs from the queue and update the caches.
        while self.pending_glyphs > 0 {
            self.pending_glyphs -= 1;

            // TODO: rather than blocking until all pending glyphs are available
            // we could try_recv and steal work from the thread pool to take advantage
            // of the fact that this thread is alive and we avoid the added latency
            // of blocking it.
            let GlyphRasterJobs { font, mut jobs } = self.glyph_rx
                .recv()
                .expect("BUG: Should be glyphs pending!");

            // Ensure that the glyphs are always processed in the same
            // order for a given text run (since iterating a hash set doesn't
            // guarantee order). This can show up as very small float inaccuracy
            // differences in rasterizers due to the different coordinates
            // that text runs get associated with by the texture cache allocator.
            jobs.sort_by(|a, b| a.key.cmp(&b.key));

            let glyph_key_cache = glyph_cache.get_glyph_key_cache_for_font_mut(font);

            for GlyphRasterJob { key, result } in jobs {
                let glyph_info = match result {
                    GlyphRasterResult::LoadFailed => GlyphCacheEntry::Blank,
                    GlyphRasterResult::Bitmap(ref glyph) if glyph.width == 0 ||
                                                            glyph.height == 0 => {
                        GlyphCacheEntry::Blank
                    }
                    GlyphRasterResult::Bitmap(glyph) => {
                        assert_eq!((glyph.left.fract(), glyph.top.fract()), (0.0, 0.0));
                        let mut texture_cache_handle = TextureCacheHandle::new();
                        texture_cache.request(&texture_cache_handle, gpu_cache);
                        texture_cache.update(
                            &mut texture_cache_handle,
                            ImageDescriptor {
                                size: size2(glyph.width, glyph.height),
                                stride: None,
                                format: ImageFormat::BGRA8,
                                is_opaque: false,
                                allow_mipmaps: false,
                                offset: 0,
                            },
                            TextureFilter::Linear,
                            Some(ImageData::Raw(Arc::new(glyph.bytes))),
                            [glyph.left, -glyph.top, glyph.scale],
                            None,
                            gpu_cache,
                            Some(glyph_key_cache.eviction_notice()),
                            UvRectKind::Rect,
                        );
                        GlyphCacheEntry::Cached(CachedGlyphInfo {
                            texture_cache_handle,
                            format: glyph.format,
                        })
                    }
                };
                glyph_key_cache.insert(key, glyph_info);
            }
        }

        // Now that we are done with the critical path (rendering the glyphs),
        // we can schedule removing the fonts if needed.
        self.remove_dead_fonts();
    }
}