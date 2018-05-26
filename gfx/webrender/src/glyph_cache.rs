/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#[cfg(feature = "pathfinder")]
use api::DeviceIntPoint;
use glyph_rasterizer::{FontInstance, GlyphFormat, GlyphKey};
use internal_types::FastHashMap;
use render_task::RenderTaskCache;
#[cfg(feature = "pathfinder")]
use render_task::RenderTaskCacheKey;
use resource_cache::ResourceClassCache;
use std::sync::Arc;
use texture_cache::{EvictionNotice, TextureCache};
#[cfg(not(feature = "pathfinder"))]
use texture_cache::TextureCacheHandle;

#[cfg(feature = "pathfinder")]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(Clone, Debug)]
pub struct CachedGlyphInfo {
    pub render_task_cache_key: RenderTaskCacheKey,
    pub format: GlyphFormat,
    pub origin: DeviceIntPoint,
}

#[cfg(not(feature = "pathfinder"))]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct CachedGlyphInfo {
    pub texture_cache_handle: TextureCacheHandle,
    pub format: GlyphFormat,
}

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub enum GlyphCacheEntry {
    // A glyph that has been successfully rasterized.
    Cached(CachedGlyphInfo),
    // A glyph that should not be rasterized (i.e. a space).
    Blank,
    // A glyph that has been submitted to the font backend for rasterization,
    // but is still pending a result.
    #[allow(dead_code)]
    Pending,
}

impl GlyphCacheEntry {
    #[cfg(feature = "pathfinder")]
    fn is_allocated(&self, texture_cache: &TextureCache, render_task_cache: &RenderTaskCache)
                    -> bool {
        match *self {
            GlyphCacheEntry::Cached(ref glyph) => {
                let render_task_cache_key = &glyph.render_task_cache_key;
                render_task_cache.cache_item_is_allocated_for_render_task(texture_cache,
                                                                          &render_task_cache_key)
            }
            GlyphCacheEntry::Pending => true,
            // If the cache only has blank glyphs left, just get rid of it.
            GlyphCacheEntry::Blank => false,
        }
    }

    #[cfg(not(feature = "pathfinder"))]
    fn is_allocated(&self, texture_cache: &TextureCache, _: &RenderTaskCache) -> bool {
        match *self {
            GlyphCacheEntry::Cached(ref glyph) => {
                texture_cache.is_allocated(&glyph.texture_cache_handle)
            }
            GlyphCacheEntry::Pending => true,
            // If the cache only has blank glyphs left, just get rid of it.
            GlyphCacheEntry::Blank => false,
        }
    }
}

#[allow(dead_code)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(Clone)]
pub enum CachedGlyphData {
    Memory(Arc<Vec<u8>>),
    Gpu,
}

pub type GlyphKeyCache = ResourceClassCache<GlyphKey, GlyphCacheEntry, EvictionNotice>;

impl GlyphKeyCache {
    pub fn eviction_notice(&self) -> &EvictionNotice {
        &self.user_data
    }
}

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct GlyphCache {
    glyph_key_caches: FastHashMap<FontInstance, GlyphKeyCache>,
}

impl GlyphCache {
    pub fn new() -> Self {
        GlyphCache {
            glyph_key_caches: FastHashMap::default(),
        }
    }

    pub fn get_glyph_key_cache_for_font_mut(&mut self, font: FontInstance) -> &mut GlyphKeyCache {
        self.glyph_key_caches
            .entry(font)
            .or_insert_with(|| GlyphKeyCache::new())
    }

    pub fn get_glyph_key_cache_for_font(&self, font: &FontInstance) -> &GlyphKeyCache {
        self.glyph_key_caches
            .get(font)
            .expect("BUG: Unable to find glyph key cache!")
    }

    pub fn clear(&mut self) {
        for (_, glyph_key_cache) in &mut self.glyph_key_caches {
            glyph_key_cache.clear()
        }
        // We use this in on_memory_pressure where retaining memory allocations
        // isn't desirable, so we completely remove the hash map instead of clearing it.
        self.glyph_key_caches = FastHashMap::default();
    }

    pub fn clear_fonts<F>(&mut self, key_fun: F)
    where
        for<'r> F: Fn(&'r &FontInstance) -> bool,
    {
        let caches_to_destroy = self.glyph_key_caches
            .keys()
            .filter(&key_fun)
            .cloned()
            .collect::<Vec<_>>();
        for key in caches_to_destroy {
            let mut cache = self.glyph_key_caches.remove(&key).unwrap();
            cache.clear();
        }
    }

    // Clear out evicted entries from glyph key caches and, if possible,
    // also remove entirely any subsequently empty glyph key caches.
    fn clear_evicted(&mut self,
                     texture_cache: &TextureCache,
                     render_task_cache: &RenderTaskCache) {
        self.glyph_key_caches.retain(|_, cache| {
            // Scan for any glyph key caches that have evictions.
            if cache.eviction_notice().check() {
                // If there are evictions, filter out any glyphs evicted from the
                // texture cache from the glyph key cache.
                let mut keep_cache = false;
                cache.retain(|_, entry| {
                    let keep_glyph = entry.is_allocated(texture_cache, render_task_cache);
                    keep_cache |= keep_glyph;
                    keep_glyph
                });
                // Only keep the glyph key cache if it still has valid glyphs.
                keep_cache
            } else {
                true
            }
        });
    }

    pub fn begin_frame(&mut self,
                       texture_cache: &TextureCache,
                       render_task_cache: &RenderTaskCache) {
        self.clear_evicted(texture_cache, render_task_cache);
    }
}
