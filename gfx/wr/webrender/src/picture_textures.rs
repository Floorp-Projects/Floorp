/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use smallvec::SmallVec;
use api::{ImageFormat, ImageBufferKind};
use api::units::*;
use crate::device::TextureFilter;
use crate::internal_types::{CacheTextureId, TextureUpdateList, Swizzle, TextureCacheAllocInfo, TextureCacheCategory};
use crate::texture_cache::{CacheEntry, EntryDetails, TargetShader};
use crate::render_backend::{FrameStamp, FrameId};
use crate::profiler::{self, TransactionProfile};
use crate::gpu_types::UvRectKind;
use crate::gpu_cache::GpuCacheHandle;

use std::cmp;

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
        }
    }

    pub fn default_tile_size(&self) -> DeviceIntSize {
        self.default_tile_size
    }

    pub fn get_or_allocate_tile(
        &mut self,
        tile_size: DeviceIntSize,
        now: FrameStamp,
        next_texture_id: &mut CacheTextureId,
        pending_updates: &mut TextureUpdateList,
    ) -> CacheEntry {
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

        CacheEntry {
            size: tile_size,
            user_data: [0.0; 4],
            last_access: now,
            details: EntryDetails::Picture {
                size: tile_size,
            },
            uv_rect_handle: GpuCacheHandle::new(),
            input_format: ImageFormat::RGBA8,
            filter: self.filter,
            swizzle: Swizzle::default(),
            texture_id,
            eviction_notice: None,
            uv_rect_kind: UvRectKind::Rect,
            shader: TargetShader::Default,
        }
    }

    pub fn free_tile(
        &mut self,
        id: CacheTextureId,
        current_frame_id: FrameId,
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
    }

    pub fn clear(&mut self, pending_updates: &mut TextureUpdateList) {
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

    #[cfg(feature = "replay")]
    pub fn filter(&self) -> TextureFilter {
        self.filter
    }
}
