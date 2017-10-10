/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{DevicePoint, DeviceUintSize, FontInstance, GlyphKey};
use internal_types::FastHashMap;
use resource_cache::ResourceClassCache;
use std::sync::Arc;
use texture_cache::TextureCacheHandle;

pub struct CachedGlyphInfo {
    pub texture_cache_handle: TextureCacheHandle,
    pub glyph_bytes: Arc<Vec<u8>>,
    pub size: DeviceUintSize,
    pub offset: DevicePoint,
    pub scale: f32,
}

pub type GlyphKeyCache = ResourceClassCache<GlyphKey, Option<CachedGlyphInfo>>;

pub struct GlyphCache {
    pub glyph_key_caches: FastHashMap<FontInstance, GlyphKeyCache>,
}

impl GlyphCache {
    pub fn new() -> GlyphCache {
        GlyphCache {
            glyph_key_caches: FastHashMap::default(),
        }
    }

    pub fn get_glyph_key_cache_for_font_mut(&mut self, font: FontInstance) -> &mut GlyphKeyCache {
        self.glyph_key_caches
            .entry(font)
            .or_insert(ResourceClassCache::new())
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
}
