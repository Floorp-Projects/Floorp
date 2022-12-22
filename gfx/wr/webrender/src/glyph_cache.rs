/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{FontKey, FontInstanceKey, IdNamespace};
use glyph_rasterizer::{FontInstance, GlyphFormat, GlyphKey, GlyphRasterizer};
use crate::internal_types::{FrameId, FrameStamp, FastHashMap};
use crate::resource_cache::ResourceClassCache;
use std::sync::Arc;
use crate::texture_cache::{EvictionNotice, TextureCache};
use crate::texture_cache::TextureCacheHandle;

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(Clone, Debug)]
pub struct CachedGlyphInfo {
    pub format: GlyphFormat,
    pub texture_cache_handle: TextureCacheHandle,
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
    fn has_been_evicted(&self, texture_cache: &TextureCache) -> bool {
        match *self {
            GlyphCacheEntry::Cached(ref glyph) => {
                !texture_cache.is_allocated(&glyph.texture_cache_handle)
            }
            GlyphCacheEntry::Pending | GlyphCacheEntry::Blank => false,
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

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(Default)]
pub struct GlyphKeyCacheInfo {
    eviction_notice: EvictionNotice,
    #[cfg(debug_assertions)]
    #[allow(dead_code)]
    #[cfg_attr(feature = "replay", serde(default))]
    last_frame_used: FrameId,
}

pub type GlyphKeyCache = ResourceClassCache<GlyphKey, GlyphCacheEntry, GlyphKeyCacheInfo>;

impl GlyphKeyCache {
    pub fn eviction_notice(&self) -> &EvictionNotice {
        &self.user_data.eviction_notice
    }

    fn clear_glyphs(&mut self) {
        self.clear();
    }

    pub fn add_glyph(&mut self, key: GlyphKey, value: GlyphCacheEntry) {
        self.insert(key, value);
    }

    fn clear_evicted(&mut self, texture_cache: &TextureCache) {
        if self.eviction_notice().check() {
            // If there are evictions, filter out any glyphs evicted from the
            // texture cache from the glyph key cache.
            self.retain(|_, entry| !entry.has_been_evicted(texture_cache));
        }
    }
}

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct GlyphCache {
    glyph_key_caches: FastHashMap<FontInstance, GlyphKeyCache>,
    current_frame: FrameId,
}

impl GlyphCache {
    pub fn new() -> Self {
        GlyphCache {
            glyph_key_caches: FastHashMap::default(),
            current_frame: Default::default(),
        }
    }

    pub fn insert_glyph_key_cache_for_font(&mut self, font: &FontInstance) -> &mut GlyphKeyCache {
        let cache = self.glyph_key_caches
                        .entry(font.clone())
                        .or_insert_with(GlyphKeyCache::new);
        #[cfg(debug_assertions)]
        {
            cache.user_data.last_frame_used = self.current_frame;
        }
        cache
    }

    pub fn get_glyph_key_cache_for_font_mut(&mut self, font: &FontInstance) -> &mut GlyphKeyCache {
        self.glyph_key_caches
            .get_mut(font)
            .expect("BUG: Unable to find glyph key cache!")
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

    pub fn delete_font_instances(
        &mut self,
        instance_keys: &[FontInstanceKey],
        glyph_rasterizer: &mut GlyphRasterizer,
    ) {
        self.glyph_key_caches.retain(|k, cache| {
            if instance_keys.contains(&k.instance_key) {
                cache.clear_glyphs();
                glyph_rasterizer.delete_font_instance(k);
                false
            } else {
                true
            }
        });
    }

    pub fn delete_fonts(&mut self, font_keys: &[FontKey]) {
        self.glyph_key_caches.retain(|k, cache| {
            if font_keys.contains(&k.font_key) {
                cache.clear_glyphs();
                false
            } else {
                true
            }
        });
    }

    pub fn clear_namespace(&mut self, namespace: IdNamespace) {
        self.glyph_key_caches.retain(|k, cache| {
            if k.font_key.0 == namespace {
                cache.clear_glyphs();
                false
            } else {
                true
            }
        });
    }

    /// Clear out evicted entries from glyph key caches.
    fn clear_evicted(&mut self, texture_cache: &TextureCache) {
        for cache in self.glyph_key_caches.values_mut() {
            // Scan for any glyph key caches that have evictions.
            cache.clear_evicted(texture_cache);
        }
    }

    /// If possible, remove entirely any empty glyph key caches.
    fn clear_empty_caches(&mut self, glyph_rasterizer: &mut GlyphRasterizer) {
        self.glyph_key_caches.retain(|key, cache| {
            // Discard the glyph key cache if it has no valid glyphs.
            if cache.is_empty() {
                glyph_rasterizer.delete_font_instance(key);
                false
            } else {
                true
            }
        });
    }

    pub fn begin_frame(
        &mut self,
        stamp: FrameStamp,
        texture_cache: &mut TextureCache,
        glyph_rasterizer: &mut GlyphRasterizer,
    ) {
        profile_scope!("begin_frame");
        self.current_frame = stamp.frame_id();
        self.clear_evicted(texture_cache);
        // Clearing evicted glyphs and pruning excess usage might have produced empty caches,
        // so get rid of them if possible.
        self.clear_empty_caches(glyph_rasterizer);
    }
}
