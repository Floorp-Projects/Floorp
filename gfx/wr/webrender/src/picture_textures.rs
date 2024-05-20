/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::mem;
use smallvec::SmallVec;
use api::{ImageFormat, ImageBufferKind, DebugFlags};
use api::units::*;
use crate::device::TextureFilter;
use crate::internal_types::{
    CacheTextureId, TextureUpdateList, Swizzle, TextureCacheAllocInfo, TextureCacheCategory,
    TextureSource, FrameStamp, FrameId,
};
use crate::profiler::{self, TransactionProfile};
use crate::gpu_types::{ImageSource, UvRectKind};
use crate::gpu_cache::{GpuCache, GpuCacheHandle};
use crate::freelist::{FreeList, FreeListHandle, WeakFreeListHandle};


#[derive(Debug, PartialEq)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub enum PictureCacheEntryMarker {}

malloc_size_of::malloc_size_of_is_0!(PictureCacheEntryMarker);

pub type PictureCacheTextureHandle = WeakFreeListHandle<PictureCacheEntryMarker>;

use std::cmp;

// Stores information related to a single entry in the texture
// cache. This is stored for each item whether it's in the shared
// cache or a standalone texture.
#[derive(Debug)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct PictureCacheEntry {
    /// Size of the requested tile.
    pub size: DeviceIntSize,
    /// The last frame this item was requested for rendering.
    // TODO(gw): This stamp is only used for picture cache tiles, and some checks
    //           in the glyph cache eviction code. We could probably remove it
    //           entirely in future (or move to EntryDetails::Picture).
    pub last_access: FrameStamp,
    /// Handle to the resource rect in the GPU cache.
    pub uv_rect_handle: GpuCacheHandle,
    /// The actual device texture ID this is part of.
    pub texture_id: CacheTextureId,
}

impl PictureCacheEntry {
    fn update_gpu_cache(&mut self, gpu_cache: &mut GpuCache) {
        if let Some(mut request) = gpu_cache.request(&mut self.uv_rect_handle) {
            let origin = DeviceIntPoint::zero();
            let image_source = ImageSource {
                p0: origin.to_f32(),
                p1: (origin + self.size).to_f32(),
                uv_rect_kind: UvRectKind::Rect,
                user_data: [0.0; 4],
            };
            image_source.write_gpu_blocks(&mut request);
        }
    }
}

/// The textures used to hold picture cache tiles.
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
struct PictureTexture {
    texture_id: CacheTextureId,
    size: DeviceIntSize,
    is_allocated: bool,
    last_frame_used: FrameId,
}

/// The textures used to hold picture cache tiles.
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct PictureTextures {
    /// Current list of textures in the pool
    textures: Vec<PictureTexture>,
    /// Default tile size for content tiles
    default_tile_size: DeviceIntSize,
    /// Number of currently allocated textures in the pool
    allocated_texture_count: usize,
    /// Texture filter to use for picture cache textures
    filter: TextureFilter,

    debug_flags: DebugFlags,

    /// Cache of picture cache entries.
    cache_entries: FreeList<PictureCacheEntry, PictureCacheEntryMarker>,
    /// Strong handles for the picture_cache_entries FreeList.
    cache_handles: Vec<FreeListHandle<PictureCacheEntryMarker>>,

    now: FrameStamp,
}

impl PictureTextures {
    pub fn new(
        default_tile_size: DeviceIntSize,
        filter: TextureFilter,
    ) -> Self {
        PictureTextures {
            textures: Vec::new(),
            default_tile_size,
            allocated_texture_count: 0,
            filter,
            debug_flags: DebugFlags::empty(),
            cache_entries: FreeList::new(),
            cache_handles: Vec::new(),
            now: FrameStamp::INVALID,
        }
    }

    pub fn begin_frame(&mut self, stamp: FrameStamp, pending_updates: &mut TextureUpdateList) {
        self.now = stamp;

        // Expire picture cache tiles that haven't been referenced in the last frame.
        // The picture cache code manually keeps tiles alive by calling `request` on
        // them if it wants to retain a tile that is currently not visible.
        self.expire_old_tiles(pending_updates);
    }

    pub fn default_tile_size(&self) -> DeviceIntSize {
        self.default_tile_size
    }

    pub fn update(
        &mut self,
        tile_size: DeviceIntSize,
        handle: &mut Option<PictureCacheTextureHandle>,
        gpu_cache: &mut GpuCache,
        next_texture_id: &mut CacheTextureId,
        pending_updates: &mut TextureUpdateList,
    ) {
        debug_assert!(self.now.is_valid());
        debug_assert!(tile_size.width > 0 && tile_size.height > 0);

        let need_alloc = match handle {
            None => true,
            Some(handle) => {
                // Check if the entry has been evicted.
                !self.entry_exists(&handle)
            },
        };

        if need_alloc {
            let new_handle = self.get_or_allocate_tile(
                tile_size,
                next_texture_id,
                pending_updates,
            );

            *handle = Some(new_handle);
        }

        if let Some(handle) = handle {
            // Upload the resource rect and texture array layer.
            self.cache_entries
                .get_opt_mut(handle)
                .expect("BUG: handle must be valid now")
                .update_gpu_cache(gpu_cache);
        } else {
            panic!("The handle should be valid picture cache handle now")
        }
    }

    pub fn get_or_allocate_tile(
        &mut self,
        tile_size: DeviceIntSize,
        next_texture_id: &mut CacheTextureId,
        pending_updates: &mut TextureUpdateList,
    ) -> PictureCacheTextureHandle {
        let mut texture_id = None;
        self.allocated_texture_count += 1;

        for texture in &mut self.textures {
            if texture.size == tile_size && !texture.is_allocated {
                // Found a target that's not currently in use which matches. Update
                // the last_frame_used for GC purposes.
                texture.is_allocated = true;
                texture.last_frame_used = FrameId::INVALID;
                texture_id = Some(texture.texture_id);
                break;
            }
        }

        // Need to create a new render target and add it to the pool

        let texture_id = texture_id.unwrap_or_else(|| {
            let texture_id = *next_texture_id;
            next_texture_id.0 += 1;

            // Push a command to allocate device storage of the right size / format.
            let info = TextureCacheAllocInfo {
                target: ImageBufferKind::Texture2D,
                width: tile_size.width,
                height: tile_size.height,
                format: ImageFormat::RGBA8,
                filter: self.filter,
                is_shared_cache: false,
                has_depth: true,
                category: TextureCacheCategory::PictureTile,
            };

            pending_updates.push_alloc(texture_id, info);

            self.textures.push(PictureTexture {
                texture_id,
                is_allocated: true,
                size: tile_size,
                last_frame_used: FrameId::INVALID,
            });

            texture_id
        });

        let cache_entry = PictureCacheEntry {
            size: tile_size,
            last_access: self.now,
            uv_rect_handle: GpuCacheHandle::new(),
            texture_id,
        };

        // Add the cache entry to the picture_textures.cache_entries FreeList.
        let strong_handle = self.cache_entries.insert(cache_entry);
        let new_handle = strong_handle.weak();

        self.cache_handles.push(strong_handle);

        new_handle        
    }

    pub fn free_tile(
        &mut self,
        id: CacheTextureId,
        current_frame_id: FrameId,
        pending_updates: &mut TextureUpdateList,
    ) {
        self.allocated_texture_count -= 1;

        let texture = self.textures
            .iter_mut()
            .find(|t| t.texture_id == id)
            .expect("bug: invalid texture id");

        assert!(texture.is_allocated);
        texture.is_allocated = false;

        assert_eq!(texture.last_frame_used, FrameId::INVALID);
        texture.last_frame_used = current_frame_id;

        if self.debug_flags.contains(
            DebugFlags::TEXTURE_CACHE_DBG |
            DebugFlags::TEXTURE_CACHE_DBG_CLEAR_EVICTED)
        {
            pending_updates.push_debug_clear(
                id,
                DeviceIntPoint::zero(),
                texture.size.width,
                texture.size.height,
            );
        }
    }

    pub fn request(&mut self, handle: &PictureCacheTextureHandle, gpu_cache: &mut GpuCache) -> bool {
        let entry = self.cache_entries.get_opt_mut(handle);
        let now = self.now;
        entry.map_or(true, |entry| {
            // If an image is requested that is already in the cache,
            // refresh the GPU cache data associated with this item.
            entry.last_access = now;
            entry.update_gpu_cache(gpu_cache);
            false
        })
    }

    pub fn get_texture_source(&self, handle: &PictureCacheTextureHandle) -> TextureSource {
        let entry = self.cache_entries.get_opt(handle)
            .expect("BUG: was dropped from cache or not updated!");

        debug_assert_eq!(entry.last_access, self.now);

        TextureSource::TextureCache(entry.texture_id, Swizzle::default())
    }

    /// Expire picture cache tiles that haven't been referenced in the last frame.
    /// The picture cache code manually keeps tiles alive by calling `request` on
    /// them if it wants to retain a tile that is currently not visible.
    pub fn expire_old_tiles(&mut self, pending_updates: &mut TextureUpdateList) {
        for i in (0 .. self.cache_handles.len()).rev() {
            let evict = {
                let entry = self.cache_entries.get(
                    &self.cache_handles[i]
                );

                // This function is called at the beginning of the frame,
                // so we don't yet know which picture cache tiles will be
                // requested this frame. Therefore only evict picture cache
                // tiles which weren't requested in the *previous* frame.
                entry.last_access.frame_id() < self.now.frame_id() - 1
            };

            if evict {
                let handle = self.cache_handles.swap_remove(i);
                let entry = self.cache_entries.free(handle);
                self.free_tile(entry.texture_id, self.now.frame_id(), pending_updates);
            }
        }
    }

    pub fn clear(&mut self, pending_updates: &mut TextureUpdateList) {
        for handle in mem::take(&mut self.cache_handles) {
            let entry = self.cache_entries.free(handle);
            self.free_tile(entry.texture_id, self.now.frame_id(), pending_updates);
        }

        for texture in self.textures.drain(..) {
            pending_updates.push_free(texture.texture_id);
        }
    }

    pub fn update_profile(&self, profile: &mut TransactionProfile) {
        profile.set(profiler::PICTURE_TILES, self.textures.len());
    }

    /// Simple garbage collect of picture cache tiles
    pub fn gc(
        &mut self,
        pending_updates: &mut TextureUpdateList,
    ) {
        // Allow the picture cache pool to keep 25% of the current allocated tile count
        // as free textures to be reused. This ensures the allowed tile count is appropriate
        // based on current window size.
        let free_texture_count = self.textures.len() - self.allocated_texture_count;
        let allowed_retained_count = (self.allocated_texture_count as f32 * 0.25).ceil() as usize;
        let do_gc = free_texture_count > allowed_retained_count;

        if do_gc {
            // Sort the current pool by age, so that we remove oldest textures first
            self.textures.sort_unstable_by_key(|t| cmp::Reverse(t.last_frame_used));

            // We can't just use retain() because `PictureTexture` requires manual cleanup.
            let mut allocated_targets = SmallVec::<[PictureTexture; 32]>::new();
            let mut retained_targets = SmallVec::<[PictureTexture; 32]>::new();

            for target in self.textures.drain(..) {
                if target.is_allocated {
                    // Allocated targets can't be collected
                    allocated_targets.push(target);
                } else if retained_targets.len() < allowed_retained_count {
                    // Retain the most recently used targets up to the allowed count
                    retained_targets.push(target);
                } else {
                    // The rest of the targets get freed
                    assert_ne!(target.last_frame_used, FrameId::INVALID);
                    pending_updates.push_free(target.texture_id);
                }
            }

            self.textures.extend(retained_targets);
            self.textures.extend(allocated_targets);
        }
    }

    pub fn entry_exists(&self, handle: &PictureCacheTextureHandle) -> bool {
        self.cache_entries.get_opt(handle).is_some()
    }

    pub fn set_debug_flags(&mut self, flags: DebugFlags) {
        self.debug_flags = flags;
    }

    #[cfg(feature = "replay")]
    pub fn filter(&self) -> TextureFilter {
        self.filter
    }
}
