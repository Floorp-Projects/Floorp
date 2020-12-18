/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{DirtyRect, ExternalImageType, ImageFormat, ImageBufferKind};
use api::{DebugFlags, ImageDescriptor};
use api::units::*;
#[cfg(test)]
use api::{DocumentId, IdNamespace};
use crate::device::{TextureFilter, TextureFormatPair};
use crate::freelist::{FreeListHandle, WeakFreeListHandle};
use crate::gpu_cache::{GpuCache, GpuCacheHandle};
use crate::gpu_types::{ImageSource, UvRectKind};
use crate::internal_types::{
    CacheTextureId, LayerIndex, Swizzle, SwizzleSettings,
    TextureUpdateList, TextureUpdateSource, TextureSource,
    TextureCacheAllocInfo, TextureCacheUpdate,
};
use crate::lru_cache::LRUCache;
use crate::profiler::{self, TransactionProfile};
use crate::render_backend::{FrameStamp, FrameId};
use crate::resource_cache::{CacheItem, CachedImageData};
use crate::atlas_allocator::*;
use crate::slab_allocator::*;
use smallvec::SmallVec;
use std::cell::Cell;
use std::{cmp, mem};
use std::rc::Rc;
use euclid::size2;


/// Information about which shader will use the entry.
///
/// For batching purposes, it's beneficial to group some items in their
/// own textures if we know that they are used by a specific shader.
#[derive(Copy, Clone, Debug, PartialEq, Eq)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub enum TargetShader {
    Default,
    Text,
}

/// The size of each region in shared cache texture arrays.
pub const TEXTURE_REGION_DIMENSIONS: i32 = 512;

/// Items in the texture cache can either be standalone textures,
/// or a sub-rect inside the shared cache.
#[derive(Debug)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
enum EntryDetails {
    Standalone {
        /// Number of bytes this entry allocates
        size_in_bytes: usize,
    },
    Picture {
        /// Size of the tile (used for debug clears only)
        size: DeviceIntSize,
    },
    Cache {
        /// Origin within the texture layer where this item exists.
        origin: DeviceIntPoint,
        /// ID of the allocation specific to its allocator.
        alloc_id: AllocId,
    },
}

impl EntryDetails {
    // TODO(gw): We can remove LayerIndex from here completely in a follow up
    fn describe(&self) -> (LayerIndex, DeviceIntPoint) {
        match *self {
            EntryDetails::Standalone { .. }  => (0, DeviceIntPoint::zero()),
            EntryDetails::Picture { .. } => (0, DeviceIntPoint::zero()),
            EntryDetails::Cache { origin, .. } => (0, origin),
        }
    }
}

#[derive(Debug, PartialEq)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub enum CacheEntryMarker {}

// Stores information related to a single entry in the texture
// cache. This is stored for each item whether it's in the shared
// cache or a standalone texture.
#[derive(Debug)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
struct CacheEntry {
    /// Size the requested item, in device pixels.
    size: DeviceIntSize,
    /// Details specific to standalone or shared items.
    details: EntryDetails,
    /// Arbitrary user data associated with this item.
    user_data: [f32; 3],
    /// The last frame this item was requested for rendering.
    // TODO(gw): This stamp is only used for picture cache tiles, and some checks
    //           in the glyph cache eviction code. We could probably remove it
    //           entirely in future (or move to EntryDetails::Picture).
    last_access: FrameStamp,
    /// Handle to the resource rect in the GPU cache.
    uv_rect_handle: GpuCacheHandle,
    /// Image format of the data that the entry expects.
    input_format: ImageFormat,
    filter: TextureFilter,
    swizzle: Swizzle,
    /// The actual device texture ID this is part of.
    texture_id: CacheTextureId,
    /// Optional notice when the entry is evicted from the cache.
    eviction_notice: Option<EvictionNotice>,
    /// The type of UV rect this entry specifies.
    uv_rect_kind: UvRectKind,

    shader: TargetShader,
}

impl CacheEntry {
    // Create a new entry for a standalone texture.
    fn new_standalone(
        texture_id: CacheTextureId,
        last_access: FrameStamp,
        params: &CacheAllocParams,
        swizzle: Swizzle,
        size_in_bytes: usize,
    ) -> Self {
        CacheEntry {
            size: params.descriptor.size,
            user_data: params.user_data,
            last_access,
            details: EntryDetails::Standalone {
                size_in_bytes,
            },
            texture_id,
            input_format: params.descriptor.format,
            filter: params.filter,
            swizzle,
            uv_rect_handle: GpuCacheHandle::new(),
            eviction_notice: None,
            uv_rect_kind: params.uv_rect_kind,
            shader: TargetShader::Default,
        }
    }

    // Update the GPU cache for this texture cache entry.
    // This ensures that the UV rect, and texture layer index
    // are up to date in the GPU cache for vertex shaders
    // to fetch from.
    fn update_gpu_cache(&mut self, gpu_cache: &mut GpuCache) {
        if let Some(mut request) = gpu_cache.request(&mut self.uv_rect_handle) {
            let (layer_index, origin) = self.details.describe();
            let image_source = ImageSource {
                p0: origin.to_f32(),
                p1: (origin + self.size).to_f32(),
                texture_layer: layer_index as f32,
                user_data: self.user_data,
                uv_rect_kind: self.uv_rect_kind,
            };
            image_source.write_gpu_blocks(&mut request);
        }
    }

    fn evict(&self) {
        if let Some(eviction_notice) = self.eviction_notice.as_ref() {
            eviction_notice.notify();
        }
    }

    fn alternative_input_format(&self) -> ImageFormat {
        match self.input_format {
            ImageFormat::RGBA8 => ImageFormat::BGRA8,
            ImageFormat::BGRA8 => ImageFormat::RGBA8,
            other => other,
        }
    }
}


/// A texture cache handle is a weak reference to a cache entry.
///
/// If the handle has not been inserted into the cache yet, or if the entry was
/// previously inserted and then evicted, lookup of the handle will fail, and
/// the cache handle needs to re-upload this item to the texture cache (see
/// request() below).
pub type TextureCacheHandle = WeakFreeListHandle<CacheEntryMarker>;

/// Describes the eviction policy for a given entry in the texture cache.
#[derive(Copy, Clone, Debug, PartialEq, Eq)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub enum Eviction {
    /// The entry will be evicted under the normal rules (which differ between
    /// standalone and shared entries).
    Auto,
    /// The entry will not be evicted until the policy is explicitly set to a
    /// different value.
    Manual,
}

// An eviction notice is a shared condition useful for detecting
// when a TextureCacheHandle gets evicted from the TextureCache.
// It is optionally installed to the TextureCache when an update()
// is scheduled. A single notice may be shared among any number of
// TextureCacheHandle updates. The notice may then be subsequently
// checked to see if any of the updates using it have been evicted.
#[derive(Clone, Debug, Default)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct EvictionNotice {
    evicted: Rc<Cell<bool>>,
}

impl EvictionNotice {
    fn notify(&self) {
        self.evicted.set(true);
    }

    pub fn check(&self) -> bool {
        if self.evicted.get() {
            self.evicted.set(false);
            true
        } else {
            false
        }
    }
}

/// A set of lazily allocated, fixed size, texture arrays for each format the
/// texture cache supports.
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
struct SharedTextures {
    color8_nearest: AllocatorList<ShelfAllocator, TextureParameters>,
    alpha8_linear: AllocatorList<ShelfAllocator, TextureParameters>,
    alpha16_linear: AllocatorList<SlabAllocator, TextureParameters>,
    color8_linear: AllocatorList<ShelfAllocator, TextureParameters>,
    color8_glyphs: AllocatorList<BucketedShelfAllocator, TextureParameters>,
}

impl SharedTextures {
    /// Mints a new set of shared textures.
    fn new(color_formats: TextureFormatPair<ImageFormat>, config: &TextureCacheConfig) -> Self {
        Self {
            // Used primarily for cached shadow masks. There can be lots of
            // these on some pages like francine, but most pages don't use it
            // much.
            // Most content tends to fit into two 512x512 textures. We are
            // conservatively using 1024x1024 to fit everything in a single
            // texture and avoid breaking batches, but it's worth checking
            // whether it would actually lead to a lot of batch breaks in
            // practice.
            alpha8_linear: AllocatorList::new(
                config.alpha8_texture_size,
                ShelfAllocatorOptions {
                    num_columns: 1,
                    alignment: size2(8, 8),
                    .. ShelfAllocatorOptions::default()
                },
                TextureParameters {
                    formats: TextureFormatPair::from(ImageFormat::R8),
                    filter: TextureFilter::Linear,
                },
            ),
            // Used for experimental hdr yuv texture support, but not used in
            // production Firefox.
            alpha16_linear: AllocatorList::new(
                config.alpha16_texture_size,
                SlabAllocatorParameters {
                    region_size: TEXTURE_REGION_DIMENSIONS,
                },
                TextureParameters {
                    formats: TextureFormatPair::from(ImageFormat::R16),
                    filter: TextureFilter::Linear,
                },
            ),
            // The primary cache for images, etc.
            color8_linear: AllocatorList::new(
                config.color8_linear_texture_size,
                ShelfAllocatorOptions {
                    num_columns: if config.color8_linear_texture_size >= 1024 { 2 } else { 1 },
                    alignment: size2(16, 16),
                    .. ShelfAllocatorOptions::default()
                },
                TextureParameters {
                    formats: color_formats.clone(),
                    filter: TextureFilter::Linear,
                },
            ),
            // The cache for glyphs (separate to help with batching).
            color8_glyphs: AllocatorList::new(
                config.color8_glyph_texture_size,
                ShelfAllocatorOptions {
                    num_columns: if config.color8_glyph_texture_size >= 1024 { 2 } else { 1 },
                    alignment: size2(4, 8),
                    .. ShelfAllocatorOptions::default()
                },
                TextureParameters {
                    formats: color_formats.clone(),
                    filter: TextureFilter::Linear,
                },
            ),
            // Used for image-rendering: crisp. This is mostly favicons, which
            // are small. Some other images use it too, but those tend to be
            // larger than 512x512 and thus don't use the shared cache anyway.
            color8_nearest: AllocatorList::new(
                config.color8_nearest_texture_size,
                ShelfAllocatorOptions::default(),
                TextureParameters {
                    formats: color_formats,
                    filter: TextureFilter::Nearest,
                }
            ),
        }
    }

    /// Clears each texture in the set, with the given set of pending updates.
    fn clear(&mut self, updates: &mut TextureUpdateList) {
        let texture_dealloc_cb = &mut |texture_id| {
            updates.push_free(texture_id);
        };

        self.alpha8_linear.clear(texture_dealloc_cb);
        self.alpha16_linear.clear(texture_dealloc_cb);
        self.color8_linear.clear(texture_dealloc_cb);
        self.color8_nearest.clear(texture_dealloc_cb);
        self.color8_glyphs.clear(texture_dealloc_cb);
    }

    /// Returns a mutable borrow for the shared texture array matching the parameters.
    fn select(
        &mut self, external_format: ImageFormat, filter: TextureFilter, shader: TargetShader,
    ) -> &mut dyn AtlasAllocatorList<TextureParameters> {
        match external_format {
            ImageFormat::R8 => {
                assert_eq!(filter, TextureFilter::Linear);
                &mut self.alpha8_linear
            }
            ImageFormat::R16 => {
                assert_eq!(filter, TextureFilter::Linear);
                &mut self.alpha16_linear
            }
            ImageFormat::RGBA8 |
            ImageFormat::BGRA8 => {
                match (filter, shader) {
                    (TextureFilter::Linear, TargetShader::Text) => &mut self.color8_glyphs,
                    (TextureFilter::Linear, _) => &mut self.color8_linear,
                    (TextureFilter::Nearest, _) => &mut self.color8_nearest,
                    _ => panic!("Unexpexcted filter {:?}", filter),
                }
            }
            _ => panic!("Unexpected format {:?}", external_format),
        }
    }
}

/// The textures used to hold picture cache tiles.
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
struct PictureTexture {
    texture_id: CacheTextureId,
    size: DeviceIntSize,
    format: ImageFormat,
    is_allocated: bool,
    last_frame_used: FrameId,
}

impl PictureTexture {
    fn size_in_bytes(&self) -> usize {
        let bpp = self.format.bytes_per_pixel() as usize;
        (self.size.width * self.size.height) as usize * bpp
    }
}

/// The textures used to hold picture cache tiles.
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
struct PictureTextures {
    /// Current list of textures in the pool
    textures: Vec<PictureTexture>,
    /// Default tile size for content tiles
    default_tile_size: DeviceIntSize,
    /// Number of currently allocated textures in the pool
    allocated_texture_count: usize,
}

impl PictureTextures {
    fn new(
        default_tile_size: DeviceIntSize,
    ) -> Self {
        PictureTextures {
            textures: Vec::new(),
            default_tile_size,
            allocated_texture_count: 0,
        }
    }

    fn get_or_allocate_tile(
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
                filter: TextureFilter::Nearest,
                layer_count: 1,
                is_shared_cache: false,
                has_depth: true,
            };

            pending_updates.push_alloc(texture_id, info);

            self.textures.push(PictureTexture {
                texture_id,
                is_allocated: true,
                format: ImageFormat::RGBA8,
                size: tile_size,
                last_frame_used: FrameId::INVALID,
            });

            texture_id
        });

        CacheEntry {
            size: tile_size,
            user_data: [0.0; 3],
            last_access: now,
            details: EntryDetails::Picture {
                size: tile_size,
            },
            uv_rect_handle: GpuCacheHandle::new(),
            input_format: ImageFormat::RGBA8,
            filter: TextureFilter::Nearest,
            swizzle: Swizzle::default(),
            texture_id,
            eviction_notice: None,
            uv_rect_kind: UvRectKind::Rect,
            shader: TargetShader::Default,
        }
    }

    fn free_tile(
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

    fn clear(&mut self, pending_updates: &mut TextureUpdateList) {
        for texture in self.textures.drain(..) {
            pending_updates.push_free(texture.texture_id);
        }
    }

    fn update_profile(&self, profile: &mut TransactionProfile) {
        // For now, this profile counter just accumulates the tiles and bytes
        // from all picture cache textures.
        let mut picture_tiles = 0;
        let mut picture_bytes = 0;
        for texture in &self.textures {
            picture_tiles += 1;
            picture_bytes += texture.size_in_bytes();
        }
        profile.set(profiler::PICTURE_TILES, picture_tiles);
        profile.set(profiler::PICTURE_TILES_MEM, profiler::bytes_to_mb(picture_bytes));
    }

    /// Simple garbage collect of picture cache tiles
    fn gc(
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
}

/// Container struct for the various parameters used in cache allocation.
struct CacheAllocParams {
    descriptor: ImageDescriptor,
    filter: TextureFilter,
    user_data: [f32; 3],
    uv_rect_kind: UvRectKind,
    shader: TargetShader,
}

/// Startup parameters for the texture cache.
///
/// Texture sizes must be at least 512.
#[derive(Clone)]
pub struct TextureCacheConfig {
    pub color8_linear_texture_size: i32,
    pub color8_nearest_texture_size: i32,
    pub color8_glyph_texture_size: i32,
    pub alpha8_texture_size: i32,
    pub alpha16_texture_size: i32,
}

impl TextureCacheConfig {
    pub const DEFAULT: Self = TextureCacheConfig {
        color8_linear_texture_size: 2048,
        color8_nearest_texture_size: 512,
        color8_glyph_texture_size: 2048,
        alpha8_texture_size: 1024,
        alpha16_texture_size: 512,
    };
}

/// General-purpose manager for images in GPU memory. This includes images,
/// rasterized glyphs, rasterized blobs, cached render tasks, etc.
///
/// The texture cache is owned and managed by the RenderBackend thread, and
/// produces a series of commands to manipulate the textures on the Renderer
/// thread. These commands are executed before any rendering is performed for
/// a given frame.
///
/// Entries in the texture cache are not guaranteed to live past the end of the
/// frame in which they are requested, and may be evicted. The API supports
/// querying whether an entry is still available.
///
/// The TextureCache is different from the GpuCache in that the former stores
/// images, whereas the latter stores data and parameters for use in the shaders.
/// This means that the texture cache can be visualized, which is a good way to
/// understand how it works. Enabling gfx.webrender.debug.texture-cache shows a
/// live view of its contents in Firefox.
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct TextureCache {
    /// Set of texture arrays in different formats used for the shared cache.
    shared_textures: SharedTextures,

    /// A texture array per tile size for picture caching.
    picture_textures: PictureTextures,

    /// Maximum texture size supported by hardware.
    max_texture_size: i32,

    /// Settings on using texture unit swizzling.
    swizzle: Option<SwizzleSettings>,

    /// The current set of debug flags.
    debug_flags: DebugFlags,

    /// The next unused virtual texture ID. Monotonically increasing.
    next_id: CacheTextureId,

    /// A list of allocations and updates that need to be applied to the texture
    /// cache in the rendering thread this frame.
    #[cfg_attr(all(feature = "serde", any(feature = "capture", feature = "replay")), serde(skip))]
    pending_updates: TextureUpdateList,

    /// The current `FrameStamp`. Used for cache eviction policies.
    now: FrameStamp,

    /// List of picture cache entries. These are maintained separately from regular
    /// texture cache entries.
    picture_cache_handles: Vec<FreeListHandle<CacheEntryMarker>>,

    /// Cache of texture cache handles with automatic lifetime management, evicted
    /// in a least-recently-used order (except those entries with manual eviction enabled).
    lru_cache: LRUCache<CacheEntry, CacheEntryMarker>,

    /// A list of texture cache handles that have been set to explicitly have manual
    /// eviction policy enabled. The handles reference cache entries in the lru_cache
    /// above, but have opted in to manual lifetime management.
    manual_handles: Vec<FreeListHandle<CacheEntryMarker>>,

    /// Estimated memory usage of allocated entries in all of the shared textures. This
    /// is used to decide when to evict old items from the cache.
    shared_bytes_allocated: usize,

    /// Number of bytes allocated in standalone textures. Used as an input to deciding
    /// when to run texture cache eviction.
    standalone_bytes_allocated: usize,
}

impl TextureCache {
    /// If the total bytes allocated in shared / standalone cache is less
    /// than this, then allow the cache to grow without forcing an eviction.
    const EVICTION_THRESHOLD_SIZE: usize = 64 * 1024 * 1024;

    /// The maximum number of items that will be evicted per frame. This limit helps avoid jank
    /// on frames where we want to evict a large number of items. Instead, we'd prefer to drop
    /// the items incrementally over a number of frames, even if that means the total allocated
    /// size of the cache is above the desired threshold for a small number of frames.
    const MAX_EVICTIONS_PER_FRAME: usize = 32;

    pub fn new(
        max_texture_size: i32,
        default_picture_tile_size: DeviceIntSize,
        color_formats: TextureFormatPair<ImageFormat>,
        swizzle: Option<SwizzleSettings>,
        config: &TextureCacheConfig,
    ) -> Self {
        let pending_updates = TextureUpdateList::new();

        // Shared texture cache controls swizzling on a per-entry basis, assuming that
        // the texture as a whole doesn't need to be swizzled (but only some entries do).
        // It would be possible to support this, but not needed at the moment.
        assert!(color_formats.internal != ImageFormat::BGRA8 ||
            swizzle.map_or(true, |s| s.bgra8_sampling_swizzle == Swizzle::default())
        );

        let next_texture_id = CacheTextureId(1);

        TextureCache {
            shared_textures: SharedTextures::new(color_formats, config),
            picture_textures: PictureTextures::new(
                default_picture_tile_size,
            ),
            max_texture_size,
            swizzle,
            debug_flags: DebugFlags::empty(),
            next_id: next_texture_id,
            pending_updates,
            now: FrameStamp::INVALID,
            lru_cache: LRUCache::new(),
            shared_bytes_allocated: 0,
            standalone_bytes_allocated: 0,
            picture_cache_handles: Vec::new(),
            manual_handles: Vec::new(),
        }
    }

    /// Creates a TextureCache and sets it up with a valid `FrameStamp`, which
    /// is useful for avoiding panics when instantiating the `TextureCache`
    /// directly from unit test code.
    #[cfg(test)]
    pub fn new_for_testing(
        max_texture_size: i32,
        image_format: ImageFormat,
    ) -> Self {
        let mut cache = Self::new(
            max_texture_size,
            crate::picture::TILE_SIZE_DEFAULT,
            TextureFormatPair::from(image_format),
            None,
            &TextureCacheConfig::DEFAULT,
        );
        let mut now = FrameStamp::first(DocumentId::new(IdNamespace(1), 1));
        now.advance();
        cache.begin_frame(now);
        cache
    }

    pub fn set_debug_flags(&mut self, flags: DebugFlags) {
        self.debug_flags = flags;
    }

    /// Clear all entries in the texture cache. This is a fairly drastic
    /// step that should only be called very rarely.
    pub fn clear_all(&mut self) {
        // Evict all manual eviction handles
        let manual_handles = mem::replace(
            &mut self.manual_handles,
            Vec::new(),
        );
        for handle in manual_handles {
            self.evict_impl(handle);
        }

        // Evict all picture cache handles
        let picture_handles = mem::replace(
            &mut self.picture_cache_handles,
            Vec::new(),
        );
        for handle in picture_handles {
            self.evict_impl(handle);
        }

        // Evict all auto (LRU) cache handles
        while let Some(entry) = self.lru_cache.pop_oldest() {
            entry.evict();
            self.free(&entry);
        }

        // Free the picture and shared textures
        self.picture_textures.clear(&mut self.pending_updates);
        self.shared_textures.clear(&mut self.pending_updates);
        self.pending_updates.note_clear();
    }

    /// Called at the beginning of each frame.
    pub fn begin_frame(&mut self, stamp: FrameStamp) {
        debug_assert!(!self.now.is_valid());
        profile_scope!("begin_frame");
        self.now = stamp;

        // Texture cache eviction is done at the start of the frame. This ensures that
        // we won't evict items that have been requested on this frame.
        self.evict_items_from_cache_if_required();
    }

    pub fn end_frame(&mut self, profile: &mut TransactionProfile) {
        debug_assert!(self.now.is_valid());
        self.expire_old_picture_cache_tiles();
        self.picture_textures.gc(
            &mut self.pending_updates,
        );

        let updates = &mut self.pending_updates; // To avoid referring to self in the closure.
        let callback = &mut|texture_id| { updates.push_free(texture_id); };

        // Release of empty shared textures is done at the end of the frame. That way, if the
        // eviction at the start of the frame frees up a texture, that is then subsequently
        // used during the frame, we avoid doing a free/alloc for it.
        self.shared_textures.alpha8_linear.release_empty_textures(callback);
        self.shared_textures.alpha16_linear.release_empty_textures(callback);
        self.shared_textures.color8_linear.release_empty_textures(callback);
        self.shared_textures.color8_nearest.release_empty_textures(callback);
        self.shared_textures.color8_glyphs.release_empty_textures(callback);

        profile.set(profiler::TEXTURE_CACHE_A8_PIXELS, self.shared_textures.alpha8_linear.allocated_space());
        profile.set(profiler::TEXTURE_CACHE_A8_TEXTURES, self.shared_textures.alpha8_linear.allocated_textures());
        profile.set(profiler::TEXTURE_CACHE_A16_PIXELS, self.shared_textures.alpha16_linear.allocated_space());
        profile.set(profiler::TEXTURE_CACHE_A16_TEXTURES, self.shared_textures.alpha16_linear.allocated_textures());
        profile.set(profiler::TEXTURE_CACHE_RGBA8_LINEAR_PIXELS, self.shared_textures.color8_linear.allocated_space());
        profile.set(profiler::TEXTURE_CACHE_RGBA8_LINEAR_TEXTURES, self.shared_textures.color8_linear.allocated_textures());
        profile.set(profiler::TEXTURE_CACHE_RGBA8_NEAREST_PIXELS, self.shared_textures.color8_nearest.allocated_space());
        profile.set(profiler::TEXTURE_CACHE_RGBA8_NEAREST_TEXTURES, self.shared_textures.color8_nearest.allocated_textures());
        profile.set(profiler::TEXTURE_CACHE_RGBA8_GLYPHS_PIXELS, self.shared_textures.color8_glyphs.allocated_space());
        profile.set(profiler::TEXTURE_CACHE_RGBA8_GLYPHS_TEXTURES, self.shared_textures.color8_glyphs.allocated_textures());

        self.picture_textures.update_profile(profile);

        profile.set(profiler::TEXTURE_CACHE_SHARED_MEM, profiler::bytes_to_mb(self.shared_bytes_allocated));
        profile.set(profiler::TEXTURE_CACHE_STANDALONE_MEM, profiler::bytes_to_mb(self.standalone_bytes_allocated));

        self.now = FrameStamp::INVALID;
    }

    // Request an item in the texture cache. All images that will
    // be used on a frame *must* have request() called on their
    // handle, to update the last used timestamp and ensure
    // that resources are not flushed from the cache too early.
    //
    // Returns true if the image needs to be uploaded to the
    // texture cache (either never uploaded, or has been
    // evicted on a previous frame).
    pub fn request(&mut self, handle: &TextureCacheHandle, gpu_cache: &mut GpuCache) -> bool {
        match self.lru_cache.touch(handle) {
            // If an image is requested that is already in the cache,
            // refresh the GPU cache data associated with this item.
            Some(entry) => {
                entry.last_access = self.now;
                entry.update_gpu_cache(gpu_cache);
                false
            }
            None => true,
        }
    }

    // Returns true if the image needs to be uploaded to the
    // texture cache (either never uploaded, or has been
    // evicted on a previous frame).
    pub fn needs_upload(&self, handle: &TextureCacheHandle) -> bool {
        self.lru_cache.get_opt(handle).is_none()
    }

    pub fn max_texture_size(&self) -> i32 {
        self.max_texture_size
    }

    #[cfg(feature = "replay")]
    pub fn color_formats(&self) -> TextureFormatPair<ImageFormat> {
        self.shared_textures.color8_linear.texture_parameters().formats.clone()
    }

    #[cfg(feature = "replay")]
    pub fn swizzle_settings(&self) -> Option<SwizzleSettings> {
        self.swizzle
    }

    pub fn pending_updates(&mut self) -> TextureUpdateList {
        mem::replace(&mut self.pending_updates, TextureUpdateList::new())
    }

    // Update the data stored by a given texture cache handle.
    pub fn update(
        &mut self,
        handle: &mut TextureCacheHandle,
        descriptor: ImageDescriptor,
        filter: TextureFilter,
        data: Option<CachedImageData>,
        user_data: [f32; 3],
        mut dirty_rect: ImageDirtyRect,
        gpu_cache: &mut GpuCache,
        eviction_notice: Option<&EvictionNotice>,
        uv_rect_kind: UvRectKind,
        eviction: Eviction,
        shader: TargetShader,
    ) {
        debug_assert!(self.now.is_valid());
        // Determine if we need to allocate texture cache memory
        // for this item. We need to reallocate if any of the following
        // is true:
        // - Never been in the cache
        // - Has been in the cache but was evicted.
        // - Exists in the cache but dimensions / format have changed.
        let realloc = match self.lru_cache.get_opt(handle) {
            Some(entry) => {
                entry.size != descriptor.size || (entry.input_format != descriptor.format &&
                    entry.alternative_input_format() != descriptor.format)
            }
            None => {
                // Not allocated, or was previously allocated but has been evicted.
                true
            }
        };

        if realloc {
            let params = CacheAllocParams { descriptor, filter, user_data, uv_rect_kind, shader };
            self.allocate(&params, handle);

            // If we reallocated, we need to upload the whole item again.
            dirty_rect = DirtyRect::All;
        }

        // Update eviction policy (this is a no-op if it hasn't changed)
        if eviction == Eviction::Manual {
            if let Some(manual_handle) = self.lru_cache.set_manual_eviction(handle) {
                self.manual_handles.push(manual_handle);
            }
        }

        let entry = self.lru_cache.get_opt_mut(handle)
            .expect("BUG: handle must be valid now");

        // Install the new eviction notice for this update, if applicable.
        entry.eviction_notice = eviction_notice.cloned();
        entry.uv_rect_kind = uv_rect_kind;

        // Invalidate the contents of the resource rect in the GPU cache.
        // This ensures that the update_gpu_cache below will add
        // the new information to the GPU cache.
        //TODO: only invalidate if the parameters change?
        gpu_cache.invalidate(&entry.uv_rect_handle);

        // Upload the resource rect and texture array layer.
        entry.update_gpu_cache(gpu_cache);

        // Create an update command, which the render thread processes
        // to upload the new image data into the correct location
        // in GPU memory.
        if let Some(data) = data {
            // If the swizzling is supported, we always upload in the internal
            // texture format (thus avoiding the conversion by the driver).
            // Otherwise, pass the external format to the driver.
            let use_upload_format = self.swizzle.is_none();
            let (_, origin) = entry.details.describe();
            let op = TextureCacheUpdate::new_update(
                data,
                &descriptor,
                origin,
                entry.size,
                use_upload_format,
                &dirty_rect,
            );
            self.pending_updates.push_update(entry.texture_id, op);
        }
    }

    // Check if a given texture handle has a valid allocation
    // in the texture cache.
    pub fn is_allocated(&self, handle: &TextureCacheHandle) -> bool {
        self.lru_cache.get_opt(handle).is_some()
    }

    // Check if a given texture handle was last used as recently
    // as the specified number of previous frames.
    pub fn is_recently_used(&self, handle: &TextureCacheHandle, margin: usize) -> bool {
        self.lru_cache.get_opt(handle).map_or(false, |entry| {
            entry.last_access.frame_id() + margin >= self.now.frame_id()
        })
    }

    // Return the allocated size of the texture handle's associated data,
    // or otherwise indicate the handle is invalid.
    pub fn get_allocated_size(&self, handle: &TextureCacheHandle) -> Option<usize> {
        self.lru_cache.get_opt(handle).map(|entry| {
            (entry.input_format.bytes_per_pixel() * entry.size.area()) as usize
        })
    }

    // Retrieve the details of an item in the cache. This is used
    // during batch creation to provide the resource rect address
    // to the shaders and texture ID to the batching logic.
    // This function will assert in debug modes if the caller
    // tries to get a handle that was not requested this frame.
    pub fn get(&self, handle: &TextureCacheHandle) -> CacheItem {
        let (texture_id, layer_index, uv_rect, swizzle, uv_rect_handle, user_data) = self.get_cache_location(handle);
        CacheItem {
            uv_rect_handle,
            texture_id: TextureSource::TextureCache(
                texture_id,
                swizzle,
            ),
            uv_rect,
            texture_layer: layer_index as i32,
            user_data,
        }
    }

    /// A more detailed version of get(). This allows access to the actual
    /// device rect of the cache allocation.
    ///
    /// Returns a tuple identifying the texture, the layer, the region,
    /// and its GPU handle.
    pub fn get_cache_location(
        &self,
        handle: &TextureCacheHandle,
    ) -> (CacheTextureId, LayerIndex, DeviceIntRect, Swizzle, GpuCacheHandle, [f32; 3]) {
        let entry = self.lru_cache
            .get_opt(handle)
            .expect("BUG: was dropped from cache or not updated!");
        debug_assert_eq!(entry.last_access, self.now);
        let (layer_index, origin) = entry.details.describe();
        (
            entry.texture_id,
            layer_index as usize,
            DeviceIntRect::new(origin, entry.size),
            entry.swizzle,
            entry.uv_rect_handle,
            entry.user_data,
        )
    }

    /// Internal helper function to evict a strong texture cache handle
    fn evict_impl(
        &mut self,
        handle: FreeListHandle<CacheEntryMarker>,
    ) {
        let entry = self.lru_cache.remove_manual_handle(handle);
        entry.evict();
        self.free(&entry);
    }

    /// Evict a texture cache handle that was previously set to be in manual
    /// eviction mode.
    pub fn evict_manual_handle(&mut self, handle: &TextureCacheHandle) {
        // Find the strong handle that matches this weak handle. If this
        // ever shows up in profiles, we can make it a hash (but the number
        // of manual eviction handles is typically small).
        let index = self.manual_handles.iter().position(|strong_handle| {
            strong_handle.matches(handle)
        });

        if let Some(index) = index {
            let handle = self.manual_handles.swap_remove(index);
            self.evict_impl(handle);
        }
    }

    pub fn dump_color8_linear_as_svg(&self, output: &mut dyn std::io::Write) -> std::io::Result<()> {
        self.shared_textures.color8_linear.dump_as_svg(output)
    }

    pub fn dump_glyphs_as_svg(&self, output: &mut dyn std::io::Write) -> std::io::Result<()> {
        self.shared_textures.color8_glyphs.dump_as_svg(output)
    }

    /// Expire picture cache tiles that haven't been referenced in the last frame.
    /// The picture cache code manually keeps tiles alive by calling `request` on
    /// them if it wants to retain a tile that is currently not visible.
    fn expire_old_picture_cache_tiles(&mut self) {
        for i in (0 .. self.picture_cache_handles.len()).rev() {
            let evict = {
                let entry = self.lru_cache.get(&self.picture_cache_handles[i]);

                // Texture cache entries can be evicted at the start of
                // a frame, or at any time during the frame when a cache
                // allocation is occurring. This means that entries tagged
                // with eager eviction may get evicted before they have a
                // chance to be requested on the current frame. Instead,
                // advance the frame id of the entry by one before
                // comparison. This ensures that an eager entry will
                // not be evicted until it is not used for at least
                // one complete frame.
                let mut entry_frame_id = entry.last_access.frame_id();
                entry_frame_id.advance();

                entry_frame_id < self.now.frame_id()
            };

            if evict {
                let handle = self.picture_cache_handles.swap_remove(i);
                self.evict_impl(handle);
            }
        }
    }

    /// Evict old items from the shared and standalone caches, if we're over a
    /// threshold memory usage value
    fn evict_items_from_cache_if_required(&mut self) {
        let mut eviction_count = 0;

        // Keep evicting while memory is above the threshold, and we haven't
        // reached a maximum number of evictions this frame.
        while self.should_continue_evicting(eviction_count) {
            let should_evict = match self.lru_cache.peek_oldest() {
                // Only evict this item if it wasn't used in the previous frame. The reason being that if it
                // was used the previous frame then it will likely be used in this frame too, and we don't
                // want to be continually evicting and reuploading the item every frame.
                Some(entry) => entry.last_access.frame_id() < self.now.frame_id() - 1,
                // It's possible that we could fail to pop an item from the LRU list to evict, if every
                // item in the cache is set to manual eviction mode. In this case, just break out of the
                // loop as there's nothing we can do until the calling code manually evicts items to
                // reduce the allocated cache size.
                None => false,
            };

            if should_evict {
                let entry = self.lru_cache.pop_oldest().unwrap();
                entry.evict();
                self.free(&entry);
                eviction_count += 1;
            } else {
                break;
            }
        }
    }

    /// Returns true if texture cache eviction loop should continue
    fn should_continue_evicting(
        &self,
        eviction_count: usize,
    ) -> bool {
        // Get the total used bytes in standalone and shared textures. Note that
        // this does not include memory allocated to picture cache tiles, which are
        // considered separately for the purposes of texture cache eviction.
        let current_memory_estimate = self.standalone_bytes_allocated + self.shared_bytes_allocated;

        // If current memory usage is below selected threshold, we can stop evicting items
        if current_memory_estimate < Self::EVICTION_THRESHOLD_SIZE {
            return false;
        }

        // If current memory usage is significantly more than the threshold, keep evicting this frame
        if current_memory_estimate > 4 * Self::EVICTION_THRESHOLD_SIZE {
            return true;
        }

        // Otherwise, only allow evicting up to a certain number of items per frame. This allows evictions
        // to be spread over a number of frames, to avoid frame spikes.
        eviction_count < Self::MAX_EVICTIONS_PER_FRAME
    }

    // Free a cache entry from the standalone list or shared cache.
    fn free(&mut self, entry: &CacheEntry) {
        match entry.details {
            EntryDetails::Picture { size } => {
                self.picture_textures.free_tile(entry.texture_id, self.now.frame_id());
                if self.debug_flags.contains(
                    DebugFlags::TEXTURE_CACHE_DBG |
                    DebugFlags::TEXTURE_CACHE_DBG_CLEAR_EVICTED)
                {
                    self.pending_updates.push_debug_clear(
                        entry.texture_id,
                        DeviceIntPoint::zero(),
                        size.width,
                        size.height,
                        0,
                    );
                }
            }
            EntryDetails::Standalone { size_in_bytes, .. } => {
                self.standalone_bytes_allocated -= size_in_bytes;

                // This is a standalone texture allocation. Free it directly.
                self.pending_updates.push_free(entry.texture_id);
            }
            EntryDetails::Cache { origin, alloc_id, .. } => {
                let allocator_list = self.shared_textures.select(
                    entry.input_format,
                    entry.filter,
                    entry.shader,
                );

                allocator_list.deallocate(entry.texture_id, alloc_id);

                let bpp = allocator_list
                    .texture_parameters()
                    .formats
                    .internal
                    .bytes_per_pixel();
                self.shared_bytes_allocated -= (entry.size.area() * bpp) as usize;

                if self.debug_flags.contains(
                    DebugFlags::TEXTURE_CACHE_DBG |
                    DebugFlags::TEXTURE_CACHE_DBG_CLEAR_EVICTED)
                {
                    self.pending_updates.push_debug_clear(
                        entry.texture_id,
                        origin,
                        entry.size.width,
                        entry.size.height,
                        0,
                    );
                }
            }
        }
    }

    /// Allocate a block from the shared cache.
    fn allocate_from_shared_cache(
        &mut self,
        params: &CacheAllocParams,
    ) -> CacheEntry {
        let allocator_list = self.shared_textures.select(
            params.descriptor.format,
            params.filter,
            params.shader,
        );

        // To avoid referring to self in the closure.
        let next_id = &mut self.next_id;
        let pending_updates = &mut self.pending_updates;

        let (texture_id, alloc_id, allocated_rect) = allocator_list.allocate(
            params.descriptor.size,
            &mut |size, parameters| {
                let texture_id = *next_id;
                next_id.0 += 1;
                pending_updates.push_alloc(
                    texture_id,
                    TextureCacheAllocInfo {
                        target: ImageBufferKind::Texture2D,
                        width: size.width,
                        height: size.height,
                        format: parameters.formats.internal,
                        filter: parameters.filter,
                        layer_count: 1,
                        is_shared_cache: true,
                        has_depth: false,
                    },
                );

                texture_id
            },
        );

        let formats = &allocator_list.texture_parameters().formats;

        let swizzle = if formats.external == params.descriptor.format {
            Swizzle::default()
        } else {
            match self.swizzle {
                Some(_) => Swizzle::Bgra,
                None => Swizzle::default(),
            }
        };

        let bpp = formats.internal.bytes_per_pixel();
        self.shared_bytes_allocated += (allocated_rect.size.area() * bpp) as usize;

        CacheEntry {
            size: params.descriptor.size,
            user_data: params.user_data,
            last_access: self.now,
            details: EntryDetails::Cache {
                origin: allocated_rect.origin,
                alloc_id,
            },
            uv_rect_handle: GpuCacheHandle::new(),
            input_format: params.descriptor.format,
            filter: params.filter,
            swizzle,
            texture_id,
            eviction_notice: None,
            uv_rect_kind: params.uv_rect_kind,
            shader: params.shader
        }
    }

    // Returns true if the given image descriptor *may* be
    // placed in the shared texture cache.
    pub fn is_allowed_in_shared_cache(
        &self,
        filter: TextureFilter,
        descriptor: &ImageDescriptor,
    ) -> bool {
        let mut allowed_in_shared_cache = true;

        // Anything larger than TEXTURE_REGION_DIMENSIONS goes in a standalone texture.
        // TODO(gw): If we find pages that suffer from batch breaks in this
        //           case, add support for storing these in a standalone
        //           texture array.
        if descriptor.size.width > TEXTURE_REGION_DIMENSIONS ||
           descriptor.size.height > TEXTURE_REGION_DIMENSIONS
        {
            allowed_in_shared_cache = false;
        }

        // TODO(gw): For now, alpha formats of the texture cache can only be linearly sampled.
        //           Nearest sampling gets a standalone texture.
        //           This is probably rare enough that it can be fixed up later.
        if filter == TextureFilter::Nearest &&
           descriptor.format.bytes_per_pixel() <= 2
        {
            allowed_in_shared_cache = false;
        }

        allowed_in_shared_cache
    }

    /// Allocate a render target via the pending updates sent to the renderer
    pub fn alloc_render_target(
        &mut self,
        size: DeviceIntSize,
        format: ImageFormat,
    ) -> CacheTextureId {
        let texture_id = self.next_id;
        self.next_id.0 += 1;

        // Push a command to allocate device storage of the right size / format.
        let info = TextureCacheAllocInfo {
            target: ImageBufferKind::Texture2D,
            width: size.width,
            height: size.height,
            format,
            filter: TextureFilter::Linear,
            layer_count: 1,
            is_shared_cache: false,
            has_depth: false,
        };

        self.pending_updates.push_alloc(texture_id, info);

        texture_id
    }

    /// Free an existing render target
    pub fn free_render_target(
        &mut self,
        id: CacheTextureId,
    ) {
        self.pending_updates.push_free(id);
    }

    /// Allocates a new standalone cache entry.
    fn allocate_standalone_entry(
        &mut self,
        params: &CacheAllocParams,
    ) -> CacheEntry {
        let texture_id = self.next_id;
        self.next_id.0 += 1;

        // Push a command to allocate device storage of the right size / format.
        let info = TextureCacheAllocInfo {
            target: ImageBufferKind::Texture2D,
            width: params.descriptor.size.width,
            height: params.descriptor.size.height,
            format: params.descriptor.format,
            filter: params.filter,
            layer_count: 1,
            is_shared_cache: false,
            has_depth: false,
        };

        let size_in_bytes = (info.width * info.height * info.format.bytes_per_pixel()) as usize;
        self.standalone_bytes_allocated += size_in_bytes;

        self.pending_updates.push_alloc(texture_id, info);

        // Special handing for BGRA8 textures that may need to be swizzled.
        let swizzle = if params.descriptor.format == ImageFormat::BGRA8 {
            self.swizzle.map(|s| s.bgra8_sampling_swizzle)
        } else {
            None
        };

        CacheEntry::new_standalone(
            texture_id,
            self.now,
            params,
            swizzle.unwrap_or_default(),
            size_in_bytes,
        )
    }

    /// Allocates a cache entry appropriate for the given parameters.
    ///
    /// This allocates from the shared cache unless the parameters do not meet
    /// the shared cache requirements, in which case a standalone texture is
    /// used.
    fn allocate_cache_entry(
        &mut self,
        params: &CacheAllocParams,
    ) -> CacheEntry {
        assert!(!params.descriptor.size.is_empty());

        // If this image doesn't qualify to go in the shared (batching) cache,
        // allocate a standalone entry.
        if self.is_allowed_in_shared_cache(params.filter, &params.descriptor) {
            self.allocate_from_shared_cache(params)
        } else {
            self.allocate_standalone_entry(params)
        }
    }

    /// Allocates a cache entry for the given parameters, and updates the
    /// provided handle to point to the new entry.
    fn allocate(&mut self, params: &CacheAllocParams, handle: &mut TextureCacheHandle) {
        debug_assert!(self.now.is_valid());
        let new_cache_entry = self.allocate_cache_entry(params);

        // If the handle points to a valid cache entry, we want to replace the
        // cache entry with our newly updated location. We also need to ensure
        // that the storage (region or standalone) associated with the previous
        // entry here gets freed.
        //
        // If the handle is invalid, we need to insert the data, and append the
        // result to the corresponding vector.
        if let Some(old_entry) = self.lru_cache.replace_or_insert(handle, new_cache_entry) {
            old_entry.evict();
            self.free(&old_entry);
        }
    }

    // Update the data stored by a given texture cache handle for picture caching specifically.
    pub fn update_picture_cache(
        &mut self,
        tile_size: DeviceIntSize,
        handle: &mut TextureCacheHandle,
        gpu_cache: &mut GpuCache,
    ) {
        debug_assert!(self.now.is_valid());
        debug_assert!(tile_size.width > 0 && tile_size.height > 0);

        if self.lru_cache.get_opt(handle).is_none() {
            let cache_entry = self.picture_textures.get_or_allocate_tile(
                tile_size,
                self.now,
                &mut self.next_id,
                &mut self.pending_updates,
            );

            // Add the cache entry to the LRU cache, then mark it for manual eviction
            // so that the lifetime is controlled by the texture cache.

            *handle = self.lru_cache.push_new(cache_entry);

            let strong_handle = self.lru_cache
                .set_manual_eviction(handle)
                .expect("bug: handle must be valid here");
            self.picture_cache_handles.push(strong_handle);
        }

        // Upload the resource rect and texture array layer.
        self.lru_cache
            .get_opt_mut(handle)
            .expect("BUG: handle must be valid now")
            .update_gpu_cache(gpu_cache);
    }

    pub fn shared_alpha_expected_format(&self) -> ImageFormat {
        self.shared_textures.alpha8_linear.texture_parameters().formats.external
    }

    pub fn shared_color_expected_format(&self) -> ImageFormat {
        self.shared_textures.color8_linear.texture_parameters().formats.external
    }


    pub fn default_picture_tile_size(&self) -> DeviceIntSize {
        self.picture_textures.default_tile_size
    }
}

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct TextureParameters {
    pub formats: TextureFormatPair<ImageFormat>,
    pub filter: TextureFilter,
}

impl TextureCacheUpdate {
    // Constructs a TextureCacheUpdate operation to be passed to the
    // rendering thread in order to do an upload to the right
    // location in the texture cache.
    fn new_update(
        data: CachedImageData,
        descriptor: &ImageDescriptor,
        origin: DeviceIntPoint,
        size: DeviceIntSize,
        use_upload_format: bool,
        dirty_rect: &ImageDirtyRect,
    ) -> TextureCacheUpdate {
        let source = match data {
            CachedImageData::Blob => {
                panic!("The vector image should have been rasterized.");
            }
            CachedImageData::External(ext_image) => match ext_image.image_type {
                ExternalImageType::TextureHandle(_) => {
                    panic!("External texture handle should not go through texture_cache.");
                }
                ExternalImageType::Buffer => TextureUpdateSource::External {
                    id: ext_image.id,
                    channel_index: ext_image.channel_index,
                },
            },
            CachedImageData::Raw(bytes) => {
                let finish = descriptor.offset +
                    descriptor.size.width * descriptor.format.bytes_per_pixel() +
                    (descriptor.size.height - 1) * descriptor.compute_stride();
                assert!(bytes.len() >= finish as usize);

                TextureUpdateSource::Bytes { data: bytes }
            }
        };
        let format_override = if use_upload_format {
            Some(descriptor.format)
        } else {
            None
        };

        match *dirty_rect {
            DirtyRect::Partial(dirty) => {
                // the dirty rectangle doesn't have to be within the area but has to intersect it, at least
                let stride = descriptor.compute_stride();
                let offset = descriptor.offset + dirty.origin.y * stride + dirty.origin.x * descriptor.format.bytes_per_pixel();

                TextureCacheUpdate {
                    rect: DeviceIntRect::new(
                        DeviceIntPoint::new(origin.x + dirty.origin.x, origin.y + dirty.origin.y),
                        DeviceIntSize::new(
                            dirty.size.width.min(size.width - dirty.origin.x),
                            dirty.size.height.min(size.height - dirty.origin.y),
                        ),
                    ),
                    source,
                    stride: Some(stride),
                    offset,
                    format_override,
                    layer_index: 0,
                }
            }
            DirtyRect::All => {
                TextureCacheUpdate {
                    rect: DeviceIntRect::new(origin, size),
                    source,
                    stride: descriptor.stride,
                    offset: descriptor.offset,
                    format_override,
                    layer_index: 0,
                }
            }
        }
    }
}
