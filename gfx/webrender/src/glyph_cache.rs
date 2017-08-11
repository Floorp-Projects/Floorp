/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{FontInstanceKey, GlyphKey};
use frame::FrameId;
use gpu_cache::GpuCache;
use internal_types::FastHashMap;
use resource_cache::{Resource, ResourceClassCache};
use texture_cache::{TextureCache, TextureCacheItemId};

pub struct CachedGlyphInfo {
    pub texture_cache_id: Option<TextureCacheItemId>,
    pub last_access: FrameId,
}

impl Resource for CachedGlyphInfo {
    fn free(self, texture_cache: &mut TextureCache) {
        if let Some(id) = self.texture_cache_id {
            texture_cache.free(id);
        }
    }
    fn get_last_access_time(&self) -> FrameId {
        self.last_access
    }
    fn set_last_access_time(&mut self, frame_id: FrameId) {
        self.last_access = frame_id;
    }
    fn add_to_gpu_cache(&self,
                        texture_cache: &mut TextureCache,
                        gpu_cache: &mut GpuCache) {
        if let Some(texture_cache_id) = self.texture_cache_id.as_ref() {
            let item = texture_cache.get_mut(texture_cache_id);
            if let Some(mut request) = gpu_cache.request(&mut item.uv_rect_handle) {
                request.push(item.uv_rect);
                request.push([item.user_data[0], item.user_data[1], 0.0, 0.0]);
            }
        }
    }
}

pub type GlyphKeyCache = ResourceClassCache<GlyphKey, CachedGlyphInfo>;

pub struct GlyphCache {
    pub glyph_key_caches: FastHashMap<FontInstanceKey, GlyphKeyCache>,
}

impl GlyphCache {
    pub fn new() -> GlyphCache {
        GlyphCache {
            glyph_key_caches: FastHashMap::default(),
        }
    }

    pub fn get_glyph_key_cache_for_font_mut(&mut self,
                                            font: FontInstanceKey) -> &mut GlyphKeyCache {
        self.glyph_key_caches
            .entry(font)
            .or_insert(ResourceClassCache::new())
    }

    pub fn get_glyph_key_cache_for_font(&self,
                                        font: &FontInstanceKey) -> &GlyphKeyCache {
        self.glyph_key_caches
            .get(font)
            .expect("BUG: Unable to find glyph key cache!")
    }

    pub fn update(&mut self,
                  texture_cache: &mut TextureCache,
                  gpu_cache: &mut GpuCache,
                  current_frame_id: FrameId,
                  expiry_frame_id: FrameId) {
        let mut caches_to_remove = Vec::new();

        for (font, glyph_key_cache) in &mut self.glyph_key_caches {
            glyph_key_cache.update(texture_cache,
                                   gpu_cache,
                                   current_frame_id,
                                   expiry_frame_id);

            if glyph_key_cache.is_empty() {
                caches_to_remove.push(font.clone());
            }
        }

        for key in caches_to_remove {
            self.glyph_key_caches.remove(&key).unwrap();
        }
    }

    pub fn clear(&mut self, texture_cache: &mut TextureCache) {
        for (_, glyph_key_cache) in &mut self.glyph_key_caches {
            glyph_key_cache.clear(texture_cache)
        }
        // We use this in on_memory_pressure where retaining memory allocations
        // isn't desirable, so we completely remove the hash map instead of clearing it.
        self.glyph_key_caches = FastHashMap::default();
    }

    pub fn clear_fonts<F>(&mut self, texture_cache: &mut TextureCache, key_fun: F)
    where for<'r> F: Fn(&'r &FontInstanceKey) -> bool
    {
        let caches_to_destroy = self.glyph_key_caches.keys()
            .filter(&key_fun)
            .cloned()
            .collect::<Vec<_>>();
        for key in caches_to_destroy {
            let mut cache = self.glyph_key_caches.remove(&key).unwrap();
            cache.clear(texture_cache);
        }
    }
}
