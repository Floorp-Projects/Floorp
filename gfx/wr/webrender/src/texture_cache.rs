/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{DirtyRect, ExternalImageType, ImageFormat, ImageBufferKind};
use api::{DebugFlags, ImageDescriptor};
use api::units::*;
#[cfg(test)]
use api::{DocumentId, IdNamespace};
use crate::device::{TextureFilter, TextureFormatPair};
use crate::freelist::{FreeList, FreeListHandle, WeakFreeListHandle};
use crate::gpu_cache::{GpuCache, GpuCacheHandle};
use crate::gpu_types::{ImageSource, UvRectKind};
use crate::internal_types::{
    CacheTextureId, Swizzle, SwizzleSettings, FrameStamp, FrameId,
    TextureUpdateList, TextureUpdateSource, TextureSource,
    TextureCacheAllocInfo, TextureCacheUpdate, TextureCacheCategory,
};
use crate::lru_cache::LRUCache;
use crate::profiler::{self, TransactionProfile};
use crate::resource_cache::{CacheItem, CachedImageData};
use crate::texture_pack::{
    AllocatorList, AllocId, AtlasAllocatorList, ShelfAllocator, ShelfAllocatorOptions,
};
use std::cell::Cell;
use std::mem;
use std::rc::Rc;
use euclid::size2;
use malloc_size_of::{MallocSizeOf, MallocSizeOfOps};

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
#[derive(Clone, Debug)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub enum EntryDetails {
    Standalone {
        /// Number of bytes this entry allocates
        size_in_bytes: usize,
    },
    Cache {
        /// Origin within the texture layer where this item exists.
        origin: DeviceIntPoint,
        /// ID of the allocation specific to its allocator.
        alloc_id: AllocId,
        /// The allocated size in bytes for this entry.
        allocated_size_in_bytes: usize,
    },
}

impl EntryDetails {
    fn describe(&self) -> DeviceIntPoint {
        match *self {
            EntryDetails::Standalone { .. }  => DeviceIntPoint::zero(),
            EntryDetails::Cache { origin, .. } => origin,
        }
    }
}

#[derive(Debug, PartialEq)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub enum AutoCacheEntryMarker {}

#[derive(Debug, PartialEq)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub enum ManualCacheEntryMarker {}

// Stores information related to a single entry in the texture
// cache. This is stored for each item whether it's in the shared
// cache or a standalone texture.
#[derive(Debug)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct CacheEntry {
    /// Size of the requested item, in device pixels. Does not include any
    /// padding for alignment that the allocator may have added to this entry's
    /// allocation.
    pub size: DeviceIntSize,
    /// Details specific to standalone or shared items.
    pub details: EntryDetails,
    /// Arbitrary user data associated with this item.
    pub user_data: [f32; 4],
    /// The last frame this item was requested for rendering.
    // TODO(gw): This stamp is only used for picture cache tiles, and some checks
    //           in the glyph cache eviction code. We could probably remove it
    //           entirely in future (or move to PictureCacheEntry).
    pub last_access: FrameStamp,
    /// Handle to the resource rect in the GPU cache.
    pub uv_rect_handle: GpuCacheHandle,
    /// Image format of the data that the entry expects.
    pub input_format: ImageFormat,
    pub filter: TextureFilter,
    pub swizzle: Swizzle,
    /// The actual device texture ID this is part of.
    pub texture_id: CacheTextureId,
    /// Optional notice when the entry is evicted from the cache.
    pub eviction_notice: Option<EvictionNotice>,
    /// The type of UV rect this entry specifies.
    pub uv_rect_kind: UvRectKind,

    pub shader: TargetShader,
}

malloc_size_of::malloc_size_of_is_0!(
    CacheEntry,
    AutoCacheEntryMarker, ManualCacheEntryMarker
);

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
            let origin = self.details.describe();
            let image_source = ImageSource {
                p0: origin.to_f32(),
                p1: (origin + self.size).to_f32(),
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

#[derive(MallocSizeOf,Clone,PartialEq,Debug)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub enum TextureCacheHandle {
    /// A fresh handle.
    Empty,

    /// A handle for an entry with automatic eviction.
    Auto(WeakFreeListHandle<AutoCacheEntryMarker>),

    /// A handle for an entry with manual eviction.
    Manual(WeakFreeListHandle<ManualCacheEntryMarker>)
}

impl TextureCacheHandle {
    pub fn invalid() -> Self {
        TextureCacheHandle::Empty
    }
}

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

/// The different budget types for the texture cache. Each type has its own
/// memory budget. Once the budget is exceeded, entries with automatic eviction
/// are evicted. Entries with manual eviction share the same budget but are not
/// evicted once the budget is exceeded.
/// Keeping separate budgets ensures that we don't evict entries from unrelated
/// textures if one texture gets full.
#[derive(Copy, Clone, Debug, PartialEq, Eq)]
#[repr(u8)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
enum BudgetType {
    SharedColor8Linear,
    SharedColor8Nearest,
    SharedColor8Glyphs,
    SharedAlpha8,
    SharedAlpha8Glyphs,
    SharedAlpha16,
    Standalone,
}

impl BudgetType {
    pub const COUNT: usize = 7;

    pub const VALUES: [BudgetType; BudgetType::COUNT] = [
        BudgetType::SharedColor8Linear,
        BudgetType::SharedColor8Nearest,
        BudgetType::SharedColor8Glyphs,
        BudgetType::SharedAlpha8,
        BudgetType::SharedAlpha8Glyphs,
        BudgetType::SharedAlpha16,
        BudgetType::Standalone,
    ];

    pub const PRESSURE_COUNTERS: [usize; BudgetType::COUNT] = [
        profiler::ATLAS_COLOR8_LINEAR_PRESSURE,
        profiler::ATLAS_COLOR8_NEAREST_PRESSURE,
        profiler::ATLAS_COLOR8_GLYPHS_PRESSURE,
        profiler::ATLAS_ALPHA8_PRESSURE,
        profiler::ATLAS_ALPHA8_GLYPHS_PRESSURE,
        profiler::ATLAS_ALPHA16_PRESSURE,
        profiler::ATLAS_STANDALONE_PRESSURE,
    ];

    pub fn iter() -> impl Iterator<Item = BudgetType> {
        BudgetType::VALUES.iter().cloned()
    }
}

/// A set of lazily allocated, fixed size, texture arrays for each format the
/// texture cache supports.
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
struct SharedTextures {
    color8_nearest: AllocatorList<ShelfAllocator, TextureParameters>,
    alpha8_linear: AllocatorList<ShelfAllocator, TextureParameters>,
    alpha8_glyphs: AllocatorList<ShelfAllocator, TextureParameters>,
    alpha16_linear: AllocatorList<ShelfAllocator, TextureParameters>,
    color8_linear: AllocatorList<ShelfAllocator, TextureParameters>,
    color8_glyphs: AllocatorList<ShelfAllocator, TextureParameters>,
    bytes_per_texture_of_type: [i32 ; BudgetType::COUNT],
    next_compaction_idx: usize,
}

impl SharedTextures {
    /// Mints a new set of shared textures.
    fn new(color_formats: TextureFormatPair<ImageFormat>, config: &TextureCacheConfig) -> Self {
        let mut bytes_per_texture_of_type = [0 ; BudgetType::COUNT];

        // Used primarily for cached shadow masks. There can be lots of
        // these on some pages like francine, but most pages don't use it
        // much.
        // Most content tends to fit into two 512x512 textures. We are
        // conservatively using 1024x1024 to fit everything in a single
        // texture and avoid breaking batches, but it's worth checking
        // whether it would actually lead to a lot of batch breaks in
        // practice.
        let alpha8_linear = AllocatorList::new(
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
        );
        bytes_per_texture_of_type[BudgetType::SharedAlpha8 as usize] =
            config.alpha8_texture_size * config.alpha8_texture_size;

        // The cache for alpha glyphs (separate to help with batching).
        let alpha8_glyphs = AllocatorList::new(
            config.alpha8_glyph_texture_size,
            ShelfAllocatorOptions {
                num_columns: if config.alpha8_glyph_texture_size >= 1024 { 2 } else { 1 },
                alignment: size2(4, 8),
                .. ShelfAllocatorOptions::default()
            },
            TextureParameters {
                formats: TextureFormatPair::from(ImageFormat::R8),
                filter: TextureFilter::Linear,
            },
        );
        bytes_per_texture_of_type[BudgetType::SharedAlpha8Glyphs as usize] =
            config.alpha8_glyph_texture_size * config.alpha8_glyph_texture_size;

        // Used for experimental hdr yuv texture support, but not used in
        // production Firefox.
        let alpha16_linear = AllocatorList::new(
            config.alpha16_texture_size,
            ShelfAllocatorOptions {
                num_columns: if config.alpha16_texture_size >= 1024 { 2 } else { 1 },
                alignment: size2(8, 8),
                .. ShelfAllocatorOptions::default()
            },
            TextureParameters {
                formats: TextureFormatPair::from(ImageFormat::R16),
                filter: TextureFilter::Linear,
            },
        );
        bytes_per_texture_of_type[BudgetType::SharedAlpha16 as usize] =
            ImageFormat::R16.bytes_per_pixel() *
            config.alpha16_texture_size * config.alpha16_texture_size;

        // The primary cache for images, etc.
        let color8_linear = AllocatorList::new(
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
        );
        bytes_per_texture_of_type[BudgetType::SharedColor8Linear as usize] =
            color_formats.internal.bytes_per_pixel() *
            config.color8_linear_texture_size * config.color8_linear_texture_size;

        // The cache for subpixel-AA and bitmap glyphs (separate to help with batching).
        let color8_glyphs = AllocatorList::new(
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
        );
        bytes_per_texture_of_type[BudgetType::SharedColor8Glyphs as usize] =
            color_formats.internal.bytes_per_pixel() *
            config.color8_glyph_texture_size * config.color8_glyph_texture_size;

        // Used for image-rendering: crisp. This is mostly favicons, which
        // are small. Some other images use it too, but those tend to be
        // larger than 512x512 and thus don't use the shared cache anyway.
        let color8_nearest = AllocatorList::new(
            config.color8_nearest_texture_size,
            ShelfAllocatorOptions::default(),
            TextureParameters {
                formats: color_formats.clone(),
                filter: TextureFilter::Nearest,
            }
        );
        bytes_per_texture_of_type[BudgetType::SharedColor8Nearest as usize] =
            color_formats.internal.bytes_per_pixel() *
            config.color8_nearest_texture_size * config.color8_nearest_texture_size;

        Self {
            alpha8_linear,
            alpha8_glyphs,
            alpha16_linear,
            color8_linear,
            color8_glyphs,
            color8_nearest,
            bytes_per_texture_of_type,
            next_compaction_idx: 0,
        }
    }

    /// Clears each texture in the set, with the given set of pending updates.
    fn clear(&mut self, updates: &mut TextureUpdateList) {
        let texture_dealloc_cb = &mut |texture_id| {
            updates.push_free(texture_id);
        };

        self.alpha8_linear.clear(texture_dealloc_cb);
        self.alpha8_glyphs.clear(texture_dealloc_cb);
        self.alpha16_linear.clear(texture_dealloc_cb);
        self.color8_linear.clear(texture_dealloc_cb);
        self.color8_nearest.clear(texture_dealloc_cb);
        self.color8_glyphs.clear(texture_dealloc_cb);
    }

    /// Returns a mutable borrow for the shared texture array matching the parameters.
    fn select(
        &mut self, external_format: ImageFormat, filter: TextureFilter, shader: TargetShader,
    ) -> (&mut dyn AtlasAllocatorList<TextureParameters>, BudgetType) {
        match external_format {
            ImageFormat::R8 => {
                assert_eq!(filter, TextureFilter::Linear);
                match shader {
                    TargetShader::Text => {
                        (&mut self.alpha8_glyphs, BudgetType::SharedAlpha8Glyphs)
                    },
                    _ => (&mut self.alpha8_linear, BudgetType::SharedAlpha8),
                }
            }
            ImageFormat::R16 => {
                assert_eq!(filter, TextureFilter::Linear);
                (&mut self.alpha16_linear, BudgetType::SharedAlpha16)
            }
            ImageFormat::RGBA8 |
            ImageFormat::BGRA8 => {
                match (filter, shader) {
                    (TextureFilter::Linear, TargetShader::Text) => {
                        (&mut self.color8_glyphs, BudgetType::SharedColor8Glyphs)
                    },
                    (TextureFilter::Linear, _) => {
                        (&mut self.color8_linear, BudgetType::SharedColor8Linear)
                    },
                    (TextureFilter::Nearest, _) => {
                        (&mut self.color8_nearest, BudgetType::SharedColor8Nearest)
                    },
                    _ => panic!("Unexpected filter {:?}", filter),
                }
            }
            _ => panic!("Unexpected format {:?}", external_format),
        }
    }

    /// How many bytes a single texture of the given type takes up, for the
    /// configured texture sizes.
    fn bytes_per_shared_texture(&self, budget_type: BudgetType) -> usize {
        self.bytes_per_texture_of_type[budget_type as usize] as usize
    }

    fn has_multiple_textures(&self, budget_type: BudgetType) -> bool {
        match budget_type {
            BudgetType::SharedColor8Linear => self.color8_linear.allocated_textures() > 1,
            BudgetType::SharedColor8Nearest => self.color8_nearest.allocated_textures() > 1,
            BudgetType::SharedColor8Glyphs => self.color8_glyphs.allocated_textures() > 1,
            BudgetType::SharedAlpha8 => self.alpha8_linear.allocated_textures() > 1,
            BudgetType::SharedAlpha8Glyphs => self.alpha8_glyphs.allocated_textures() > 1,
            BudgetType::SharedAlpha16 => self.alpha16_linear.allocated_textures() > 1,
            BudgetType::Standalone => false,
        }
    }
}

/// Container struct for the various parameters used in cache allocation.
struct CacheAllocParams {
    descriptor: ImageDescriptor,
    filter: TextureFilter,
    user_data: [f32; 4],
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
    pub alpha8_glyph_texture_size: i32,
    pub alpha16_texture_size: i32,
}

impl TextureCacheConfig {
    pub const DEFAULT: Self = TextureCacheConfig {
        color8_linear_texture_size: 2048,
        color8_nearest_texture_size: 512,
        color8_glyph_texture_size: 2048,
        alpha8_texture_size: 1024,
        alpha8_glyph_texture_size: 2048,
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

    /// Maximum texture size supported by hardware.
    max_texture_size: i32,

    /// Maximum texture size before it is considered preferable to break the
    /// texture into tiles.
    tiling_threshold: i32,

    /// Settings on using texture unit swizzling.
    swizzle: Option<SwizzleSettings>,

    /// The current set of debug flags.
    debug_flags: DebugFlags,

    /// The next unused virtual texture ID. Monotonically increasing.
    pub next_id: CacheTextureId,

    /// A list of allocations and updates that need to be applied to the texture
    /// cache in the rendering thread this frame.
    #[cfg_attr(all(feature = "serde", any(feature = "capture", feature = "replay")), serde(skip))]
    pub pending_updates: TextureUpdateList,

    /// The current `FrameStamp`. Used for cache eviction policies.
    now: FrameStamp,

    /// Cache of texture cache handles with automatic lifetime management, evicted
    /// in a least-recently-used order.
    lru_cache: LRUCache<CacheEntry, AutoCacheEntryMarker>,

    /// Cache of texture cache entries with manual liftime management.
    manual_entries: FreeList<CacheEntry, ManualCacheEntryMarker>,

    /// Strong handles for the manual_entries FreeList.
    manual_handles: Vec<FreeListHandle<ManualCacheEntryMarker>>,

    /// Memory usage of allocated entries in all of the shared or standalone
    /// textures. Includes both manually and automatically evicted entries.
    bytes_allocated: [usize ; BudgetType::COUNT],
}

impl TextureCache {
    /// The maximum number of items that will be evicted per frame. This limit helps avoid jank
    /// on frames where we want to evict a large number of items. Instead, we'd prefer to drop
    /// the items incrementally over a number of frames, even if that means the total allocated
    /// size of the cache is above the desired threshold for a small number of frames.
    const MAX_EVICTIONS_PER_FRAME: usize = 32;

    pub fn new(
        max_texture_size: i32,
        tiling_threshold: i32,
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
            max_texture_size,
            tiling_threshold,
            swizzle,
            debug_flags: DebugFlags::empty(),
            next_id: next_texture_id,
            pending_updates,
            now: FrameStamp::INVALID,
            lru_cache: LRUCache::new(BudgetType::COUNT),
            manual_entries: FreeList::new(),
            manual_handles: Vec::new(),
            bytes_allocated: [0 ; BudgetType::COUNT],
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
            max_texture_size,
            TextureFormatPair::from(image_format),
            None,
            &TextureCacheConfig::DEFAULT,
        );
        let mut now = FrameStamp::first(DocumentId::new(IdNamespace(1), 1));
        now.advance();
        cache.begin_frame(now, &mut TransactionProfile::new());
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
            let entry = self.manual_entries.free(handle);
            self.evict_impl(entry);
        }

        // Evict all auto (LRU) cache handles
        for budget_type in BudgetType::iter() {
            while let Some(entry) = self.lru_cache.pop_oldest(budget_type as u8) {
                entry.evict();
                self.free(&entry);
            }
        }

        // Free the picture and shared textures
        self.shared_textures.clear(&mut self.pending_updates);
        self.pending_updates.note_clear();
    }

    /// Called at the beginning of each frame.
    pub fn begin_frame(&mut self, stamp: FrameStamp, profile: &mut TransactionProfile) {
        debug_assert!(!self.now.is_valid());
        profile_scope!("begin_frame");
        self.now = stamp;

        // Texture cache eviction is done at the start of the frame. This ensures that
        // we won't evict items that have been requested on this frame.
        // It also frees up space in the cache for items allocated later in the frame
        // potentially reducing texture allocations and fragmentation.
        self.evict_items_from_cache_if_required(profile);
    }

    pub fn end_frame(&mut self, profile: &mut TransactionProfile) {
        debug_assert!(self.now.is_valid());

        let updates = &mut self.pending_updates; // To avoid referring to self in the closure.
        let callback = &mut|texture_id| { updates.push_free(texture_id); };

        // Release of empty shared textures is done at the end of the frame. That way, if the
        // eviction at the start of the frame frees up a texture, that is then subsequently
        // used during the frame, we avoid doing a free/alloc for it.
        self.shared_textures.alpha8_linear.release_empty_textures(callback);
        self.shared_textures.alpha8_glyphs.release_empty_textures(callback);
        self.shared_textures.alpha16_linear.release_empty_textures(callback);
        self.shared_textures.color8_linear.release_empty_textures(callback);
        self.shared_textures.color8_nearest.release_empty_textures(callback);
        self.shared_textures.color8_glyphs.release_empty_textures(callback);

        for budget in BudgetType::iter() {
            let threshold = self.get_eviction_threshold(budget);
            let pressure = self.bytes_allocated[budget as usize] as f32 / threshold as f32;
            profile.set(BudgetType::PRESSURE_COUNTERS[budget as usize], pressure);
        }

        profile.set(profiler::ATLAS_A8_PIXELS, self.shared_textures.alpha8_linear.allocated_space());
        profile.set(profiler::ATLAS_A8_TEXTURES, self.shared_textures.alpha8_linear.allocated_textures());
        profile.set(profiler::ATLAS_A8_GLYPHS_PIXELS, self.shared_textures.alpha8_glyphs.allocated_space());
        profile.set(profiler::ATLAS_A8_GLYPHS_TEXTURES, self.shared_textures.alpha8_glyphs.allocated_textures());
        profile.set(profiler::ATLAS_A16_PIXELS, self.shared_textures.alpha16_linear.allocated_space());
        profile.set(profiler::ATLAS_A16_TEXTURES, self.shared_textures.alpha16_linear.allocated_textures());
        profile.set(profiler::ATLAS_RGBA8_LINEAR_PIXELS, self.shared_textures.color8_linear.allocated_space());
        profile.set(profiler::ATLAS_RGBA8_LINEAR_TEXTURES, self.shared_textures.color8_linear.allocated_textures());
        profile.set(profiler::ATLAS_RGBA8_NEAREST_PIXELS, self.shared_textures.color8_nearest.allocated_space());
        profile.set(profiler::ATLAS_RGBA8_NEAREST_TEXTURES, self.shared_textures.color8_nearest.allocated_textures());
        profile.set(profiler::ATLAS_RGBA8_GLYPHS_PIXELS, self.shared_textures.color8_glyphs.allocated_space());
        profile.set(profiler::ATLAS_RGBA8_GLYPHS_TEXTURES, self.shared_textures.color8_glyphs.allocated_textures());

        let shared_bytes = [
            BudgetType::SharedColor8Linear,
            BudgetType::SharedColor8Nearest,
            BudgetType::SharedColor8Glyphs,
            BudgetType::SharedAlpha8,
            BudgetType::SharedAlpha8Glyphs,
            BudgetType::SharedAlpha16,
        ].iter().map(|b| self.bytes_allocated[*b as usize]).sum();

        profile.set(profiler::ATLAS_ITEMS_MEM, profiler::bytes_to_mb(shared_bytes));

        self.now = FrameStamp::INVALID;
    }

    pub fn run_compaction(&mut self, gpu_cache: &mut GpuCache) {
        // Use the same order as BudgetType::VALUES so that we can index self.bytes_allocated
        // with the same index.
        let allocator_lists = [
            &mut self.shared_textures.color8_linear,
            &mut self.shared_textures.color8_nearest,
            &mut self.shared_textures.color8_glyphs,
            &mut self.shared_textures.alpha8_linear,
            &mut self.shared_textures.alpha8_glyphs,
            &mut self.shared_textures.alpha16_linear,
        ];

        // Pick a texture type on which to try to run the compaction logic this frame.
        let idx = self.shared_textures.next_compaction_idx;

        // Number of moved pixels after which we stop attempting to move more items for this frame.
        // The constant is up for adjustment, the main goal is to avoid causing frame spikes on
        // low end GPUs.
        let area_threshold = 512*512; 

        let mut changes = Vec::new();
        allocator_lists[idx].try_compaction(area_threshold, &mut changes);

        if changes.is_empty() {
            // Nothing to do, we'll try another texture type next frame.
            self.shared_textures.next_compaction_idx = (self.shared_textures.next_compaction_idx + 1) % allocator_lists.len();
        }

        for change in changes {
            let bpp = allocator_lists[idx].texture_parameters().formats.internal.bytes_per_pixel();

            // While the area of the image does not change, the area it occupies in the texture
            // atlas may (in other words the number of wasted pixels can change), so we have
            // to keep track of that.
            let old_bytes = (change.old_rect.area() * bpp) as usize;
            let new_bytes = (change.new_rect.area() * bpp) as usize;
            self.bytes_allocated[idx] -= old_bytes;
            self.bytes_allocated[idx] += new_bytes;

            let entry = match change.handle {
                TextureCacheHandle::Auto(handle) => self.lru_cache.get_opt_mut(&handle).unwrap(),
                TextureCacheHandle::Manual(handle) => self.manual_entries.get_opt_mut(&handle).unwrap(),
                TextureCacheHandle::Empty => { panic!("invalid handle"); }
            };
            entry.texture_id = change.new_tex;
            entry.details = EntryDetails::Cache {
                origin: change.new_rect.min,
                alloc_id: change.new_id,
                allocated_size_in_bytes: new_bytes,
            };

            gpu_cache.invalidate(&entry.uv_rect_handle);
            entry.uv_rect_handle = GpuCacheHandle::new();

            let src_rect = DeviceIntRect::from_origin_and_size(change.old_rect.min, entry.size);
            let dst_rect = DeviceIntRect::from_origin_and_size(change.new_rect.min, entry.size);

            self.pending_updates.push_copy(change.old_tex, &src_rect, change.new_tex, &dst_rect);

            if self.debug_flags.contains(
                DebugFlags::TEXTURE_CACHE_DBG |
                DebugFlags::TEXTURE_CACHE_DBG_CLEAR_EVICTED)
            {
                self.pending_updates.push_debug_clear(
                    change.old_tex,
                    src_rect.min,
                    src_rect.width(),
                    src_rect.height(),
                );
            }
        }
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
        let now = self.now;
        let entry = match handle {
            TextureCacheHandle::Empty => None,
            TextureCacheHandle::Auto(handle) => {
                // Call touch rather than get_opt_mut so that the LRU index
                // knows that the entry has been used.
                self.lru_cache.touch(handle)
            },
            TextureCacheHandle::Manual(handle) => {
                self.manual_entries.get_opt_mut(handle)
            },
        };
        entry.map_or(true, |entry| {
            // If an image is requested that is already in the cache,
            // refresh the GPU cache data associated with this item.
            entry.last_access = now;
            entry.update_gpu_cache(gpu_cache);
            false
        })
    }

    fn get_entry_opt(&self, handle: &TextureCacheHandle) -> Option<&CacheEntry> {
        match handle {
            TextureCacheHandle::Empty => None,
            TextureCacheHandle::Auto(handle) => self.lru_cache.get_opt(handle),
            TextureCacheHandle::Manual(handle) => self.manual_entries.get_opt(handle),
        }
    }

    fn get_entry_opt_mut(&mut self, handle: &TextureCacheHandle) -> Option<&mut CacheEntry> {
        match handle {
            TextureCacheHandle::Empty => None,
            TextureCacheHandle::Auto(handle) => self.lru_cache.get_opt_mut(handle),
            TextureCacheHandle::Manual(handle) => self.manual_entries.get_opt_mut(handle),
        }
    }

    // Returns true if the image needs to be uploaded to the
    // texture cache (either never uploaded, or has been
    // evicted on a previous frame).
    pub fn needs_upload(&self, handle: &TextureCacheHandle) -> bool {
        !self.is_allocated(handle)
    }

    pub fn max_texture_size(&self) -> i32 {
        self.max_texture_size
    }

    pub fn tiling_threshold(&self) -> i32 {
        self.tiling_threshold
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
        user_data: [f32; 4],
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
        let realloc = match self.get_entry_opt(handle) {
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
            self.allocate(&params, handle, eviction);

            // If we reallocated, we need to upload the whole item again.
            dirty_rect = DirtyRect::All;
        }

        let entry = self.get_entry_opt_mut(handle)
            .expect("BUG: There must be an entry at this handle now");

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
            let origin = entry.details.describe();
            let texture_id = entry.texture_id;
            let size = entry.size;
            let use_upload_format = self.swizzle.is_none();
            let op = TextureCacheUpdate::new_update(
                data,
                &descriptor,
                origin,
                size,
                use_upload_format,
                &dirty_rect,
            );
            self.pending_updates.push_update(texture_id, op);
        }
    }

    // Check if a given texture handle has a valid allocation
    // in the texture cache.
    pub fn is_allocated(&self, handle: &TextureCacheHandle) -> bool {
        self.get_entry_opt(handle).is_some()
    }

    // Return the allocated size of the texture handle's associated data,
    // or otherwise indicate the handle is invalid.
    pub fn get_allocated_size(&self, handle: &TextureCacheHandle) -> Option<usize> {
        self.get_entry_opt(handle).map(|entry| {
            (entry.input_format.bytes_per_pixel() * entry.size.area()) as usize
        })
    }

    // Retrieve the details of an item in the cache. This is used
    // during batch creation to provide the resource rect address
    // to the shaders and texture ID to the batching logic.
    // This function will assert in debug modes if the caller
    // tries to get a handle that was not requested this frame.
    pub fn get(&self, handle: &TextureCacheHandle) -> CacheItem {
        let (texture_id, uv_rect, swizzle, uv_rect_handle, user_data) = self.get_cache_location(handle);
        CacheItem {
            uv_rect_handle,
            texture_id: TextureSource::TextureCache(
                texture_id,
                swizzle,
            ),
            uv_rect,
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
    ) -> (CacheTextureId, DeviceIntRect, Swizzle, GpuCacheHandle, [f32; 4]) {
        let entry = self
            .get_entry_opt(handle)
            .expect("BUG: was dropped from cache or not updated!");
        debug_assert_eq!(entry.last_access, self.now);
        let origin = entry.details.describe();
        (
            entry.texture_id,
            DeviceIntRect::from_origin_and_size(origin, entry.size),
            entry.swizzle,
            entry.uv_rect_handle,
            entry.user_data,
        )
    }

    /// Internal helper function to evict a strong texture cache handle
    fn evict_impl(
        &mut self,
        entry: CacheEntry,
    ) {
        entry.evict();
        self.free(&entry);
    }

    /// Evict a texture cache handle that was previously set to be in manual
    /// eviction mode.
    pub fn evict_handle(&mut self, handle: &TextureCacheHandle) {
        match handle {
            TextureCacheHandle::Manual(handle) => {
                // Find the strong handle that matches this weak handle. If this
                // ever shows up in profiles, we can make it a hash (but the number
                // of manual eviction handles is typically small).
                // Alternatively, we could make a more forgiving FreeList variant
                // which does not differentiate between strong and weak handles.
                let index = self.manual_handles.iter().position(|strong_handle| {
                    strong_handle.matches(handle)
                });
                if let Some(index) = index {
                    let handle = self.manual_handles.swap_remove(index);
                    let entry = self.manual_entries.free(handle);
                    self.evict_impl(entry);
                }
            }
            TextureCacheHandle::Auto(handle) => {
                if let Some(entry) = self.lru_cache.remove(handle) {
                    self.evict_impl(entry);
                }
            }
            _ => {}
        }
    }

    pub fn dump_color8_linear_as_svg(&self, output: &mut dyn std::io::Write) -> std::io::Result<()> {
        self.shared_textures.color8_linear.dump_as_svg(output)
    }

    pub fn dump_color8_glyphs_as_svg(&self, output: &mut dyn std::io::Write) -> std::io::Result<()> {
        self.shared_textures.color8_glyphs.dump_as_svg(output)
    }

    pub fn dump_alpha8_glyphs_as_svg(&self, output: &mut dyn std::io::Write) -> std::io::Result<()> {
        self.shared_textures.alpha8_glyphs.dump_as_svg(output)
    }

    pub fn dump_alpha8_linear_as_svg(&self, output: &mut dyn std::io::Write) -> std::io::Result<()> {
        self.shared_textures.alpha8_linear.dump_as_svg(output)
    }

    /// Get the eviction threshold, in bytes, for the given budget type.
    fn get_eviction_threshold(&self, budget_type: BudgetType) -> usize {
        if budget_type == BudgetType::Standalone {
            // For standalone textures, the only reason to evict textures is
            // to save GPU memory. Batching / draw call concerns do not apply
            // to standalone textures, because unused textures don't cause
            // extra draw calls.
            return 8 * 1024 * 1024;
        }

        // For shared textures, evicting an entry only frees up GPU memory if it
        // causes one of the shared textures to become empty, so we want to avoid
        // getting slightly above the capacity of a texture.
        // The other concern for shared textures is batching: The entries that
        // are needed in the current frame should be distributed across as few
        // shared textures as possible, to minimize the number of draw calls.
        // Ideally we only want one texture per type under simple workloads.

        let bytes_per_texture = self.shared_textures.bytes_per_shared_texture(budget_type);

        // Number of allocated bytes under which we don't bother with evicting anything
        // from the cache. Above the threshold we consider evicting the coldest items
        // depending on how cold they are.
        //
        // Above all else we want to make sure that even after a heavy workload, the
        // shared cache settles back to a single texture atlas per type over some reasonable
        // period of time.
        // This is achieved by the compaction logic which will try to consolidate items that
        // are spread over multiple textures into few ones, and by evicting old items
        // so that the compaction logic has room to do its job.
        //
        // The other goal is to leave enough empty space in the texture atlases
        // so that we are not too likely to have to allocate a new texture atlas on
        // the next frame if we switch to a new tab or load a new page. That's why
        // the following thresholds are rather low. Note that even when above the threshold,
        // we only evict cold items and ramp up the eviction pressure depending on the amount
        // of allocated memory (See should_continue_evicting).
        let ideal_utilization = match budget_type {
            BudgetType::SharedAlpha8Glyphs | BudgetType::SharedColor8Glyphs => {
                // Glyphs are usually small and tightly packed so they waste very little
                // space in the cache.
                bytes_per_texture * 2 / 3
            }
            _ => {
                // Other types of images come with a variety of sizes making them more
                // prone to wasting pixels and causing fragmentation issues so we put
                // more pressure on them.
                bytes_per_texture / 3
            }
        };

        ideal_utilization
    }

    /// Returns whether to continue eviction and how cold an item need to be to be evicted.
    ///
    /// If the None is returned, stop evicting.
    /// If the Some(n) is returned, continue evicting if the coldest item hasn't been used
    /// for more than n frames.
    fn should_continue_evicting(
        &self,
        budget_type: BudgetType,
        eviction_count: usize,
    ) -> Option<u64> {

        let threshold = self.get_eviction_threshold(budget_type);
        let bytes_allocated = self.bytes_allocated[budget_type as usize];

        let uses_multiple_atlases = self.shared_textures.has_multiple_textures(budget_type);

        // If current memory usage is below selected threshold, we can stop evicting items
        // except when using shared texture atlases and more than one texture is in use.
        // This is not very common but can happen due to fragmentation and the only way
        // to get rid of that fragmentation is to continue evicting.
        if bytes_allocated < threshold && !uses_multiple_atlases {
            return None;
        }

        // Number of frames since last use that is considered too recent for eviction,
        // depending on the cache pressure.
        let age_theshold = match bytes_allocated / threshold {
            0 => 400,
            1 => 200,
            2 => 100,
            3 => 50,
            4 => 25,
            5 => 10,
            6 => 5,
            _ => 1,
        };

        // If current memory usage is significantly more than the threshold, keep evicting this frame
        if bytes_allocated > 4 * threshold {
            return Some(age_theshold);
        }

        // Otherwise, only allow evicting up to a certain number of items per frame. This allows evictions
        // to be spread over a number of frames, to avoid frame spikes.
        if eviction_count < Self::MAX_EVICTIONS_PER_FRAME {
            return Some(age_theshold)
        }

        None
    }


    /// Evict old items from the shared and standalone caches, if we're over a
    /// threshold memory usage value
    fn evict_items_from_cache_if_required(&mut self, profile: &mut TransactionProfile) {
        let previous_frame_id = self.now.frame_id() - 1;
        let mut eviction_count = 0;
        let mut youngest_evicted = FrameId::first();

        for budget in BudgetType::iter() {
            while let Some(age_threshold) = self.should_continue_evicting(
                budget,
                eviction_count,
            ) {
                if let Some(entry) = self.lru_cache.peek_oldest(budget as u8) {
                    // Only evict this item if it wasn't used in the previous frame. The reason being that if it
                    // was used the previous frame then it will likely be used in this frame too, and we don't
                    // want to be continually evicting and reuploading the item every frame.
                    if entry.last_access.frame_id() + age_threshold > previous_frame_id {
                        // Since the LRU cache is ordered by frame access, we can break out of the loop here because
                        // we know that all remaining items were also used in the previous frame (or more recently).
                        break;
                    }
                    if entry.last_access.frame_id() > youngest_evicted {
                        youngest_evicted = entry.last_access.frame_id();
                    }
                    let entry = self.lru_cache.pop_oldest(budget as u8).unwrap();
                    entry.evict();
                    self.free(&entry);
                    eviction_count += 1;
                } else {
                    // The LRU cache is empty, all remaining items use manual
                    // eviction. In this case, there's nothing we can do until
                    // the calling code manually evicts items to reduce the
                    // allocated cache size.
                    break;
                }
            }
        }

        if eviction_count > 0 {
            profile.set(profiler::TEXTURE_CACHE_EVICTION_COUNT, eviction_count);
            profile.set(
                profiler::TEXTURE_CACHE_YOUNGEST_EVICTION,
                self.now.frame_id().as_u64() - youngest_evicted.as_u64()
            );
        }
    }

    // Free a cache entry from the standalone list or shared cache.
    fn free(&mut self, entry: &CacheEntry) {
        match entry.details {
            EntryDetails::Standalone { size_in_bytes, .. } => {
                self.bytes_allocated[BudgetType::Standalone as usize] -= size_in_bytes;

                // This is a standalone texture allocation. Free it directly.
                self.pending_updates.push_free(entry.texture_id);
            }
            EntryDetails::Cache { origin, alloc_id, allocated_size_in_bytes } => {
                let (allocator_list, budget_type) = self.shared_textures.select(
                    entry.input_format,
                    entry.filter,
                    entry.shader,
                );

                allocator_list.deallocate(entry.texture_id, alloc_id);

                self.bytes_allocated[budget_type as usize] -= allocated_size_in_bytes;

                if self.debug_flags.contains(
                    DebugFlags::TEXTURE_CACHE_DBG |
                    DebugFlags::TEXTURE_CACHE_DBG_CLEAR_EVICTED)
                {
                    self.pending_updates.push_debug_clear(
                        entry.texture_id,
                        origin,
                        entry.size.width,
                        entry.size.height,
                    );
                }
            }
        }
    }

    /// Allocate a block from the shared cache.
    fn allocate_from_shared_cache(
        &mut self,
        params: &CacheAllocParams,
    ) -> (CacheEntry, BudgetType) {
        let (allocator_list, budget_type) = self.shared_textures.select(
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
                        is_shared_cache: true,
                        has_depth: false,
                        category: TextureCacheCategory::Atlas,
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
        let allocated_size_in_bytes = (allocated_rect.area() * bpp) as usize;
        self.bytes_allocated[budget_type as usize] += allocated_size_in_bytes;

        (CacheEntry {
            size: params.descriptor.size,
            user_data: params.user_data,
            last_access: self.now,
            details: EntryDetails::Cache {
                origin: allocated_rect.min,
                alloc_id,
                allocated_size_in_bytes,
            },
            uv_rect_handle: GpuCacheHandle::new(),
            input_format: params.descriptor.format,
            filter: params.filter,
            swizzle,
            texture_id,
            eviction_notice: None,
            uv_rect_kind: params.uv_rect_kind,
            shader: params.shader
        }, budget_type)
    }

    // Returns true if the given image descriptor *may* be
    // placed in the shared texture cache.
    pub fn is_allowed_in_shared_cache(
        &self,
        filter: TextureFilter,
        descriptor: &ImageDescriptor,
    ) -> bool {
        let mut allowed_in_shared_cache = true;

        if matches!(descriptor.format, ImageFormat::RGBA8 | ImageFormat::BGRA8)
            && filter == TextureFilter::Linear
        {
            // Allow the maximum that can fit in the linear color texture's two column layout.
            let max = self.shared_textures.color8_linear.size() / 2;
            allowed_in_shared_cache = descriptor.size.width.max(descriptor.size.height) <= max;
        } else if descriptor.size.width > TEXTURE_REGION_DIMENSIONS {
            allowed_in_shared_cache = false;
        }

        if descriptor.size.height > TEXTURE_REGION_DIMENSIONS {
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
            is_shared_cache: false,
            has_depth: false,
            category: TextureCacheCategory::RenderTarget,
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
    ) -> (CacheEntry, BudgetType) {
        let texture_id = self.next_id;
        self.next_id.0 += 1;

        // Push a command to allocate device storage of the right size / format.
        let info = TextureCacheAllocInfo {
            target: ImageBufferKind::Texture2D,
            width: params.descriptor.size.width,
            height: params.descriptor.size.height,
            format: params.descriptor.format,
            filter: params.filter,
            is_shared_cache: false,
            has_depth: false,
            category: TextureCacheCategory::Standalone,
        };

        let size_in_bytes = (info.width * info.height * info.format.bytes_per_pixel()) as usize;
        self.bytes_allocated[BudgetType::Standalone as usize] += size_in_bytes;

        self.pending_updates.push_alloc(texture_id, info);

        // Special handing for BGRA8 textures that may need to be swizzled.
        let swizzle = if params.descriptor.format == ImageFormat::BGRA8 {
            self.swizzle.map(|s| s.bgra8_sampling_swizzle)
        } else {
            None
        };

        (CacheEntry::new_standalone(
            texture_id,
            self.now,
            params,
            swizzle.unwrap_or_default(),
            size_in_bytes,
        ), BudgetType::Standalone)
    }

    /// Allocates a cache entry for the given parameters, and updates the
    /// provided handle to point to the new entry.
    fn allocate(
        &mut self,
        params: &CacheAllocParams,
        handle: &mut TextureCacheHandle,
        eviction: Eviction,
    ) {
        debug_assert!(self.now.is_valid());
        assert!(!params.descriptor.size.is_empty());

        // If this image doesn't qualify to go in the shared (batching) cache,
        // allocate a standalone entry.
        let use_shared_cache = self.is_allowed_in_shared_cache(params.filter, &params.descriptor);
        let (new_cache_entry, budget_type) = if use_shared_cache {
            self.allocate_from_shared_cache(params)
        } else {
            self.allocate_standalone_entry(params)
        };

        let details = new_cache_entry.details.clone();
        let texture_id = new_cache_entry.texture_id;

        // If the handle points to a valid cache entry, we want to replace the
        // cache entry with our newly updated location. We also need to ensure
        // that the storage (region or standalone) associated with the previous
        // entry here gets freed.
        //
        // If the handle is invalid, we need to insert the data, and append the
        // result to the corresponding vector.
        let old_entry = match (&mut *handle, eviction) {
            (TextureCacheHandle::Auto(handle), Eviction::Auto) => {
                self.lru_cache.replace_or_insert(handle, budget_type as u8, new_cache_entry)
            },
            (TextureCacheHandle::Manual(handle), Eviction::Manual) => {
                let entry = self.manual_entries.get_opt_mut(handle)
                    .expect("Don't call this after evicting");
                Some(mem::replace(entry, new_cache_entry))
            },
            (TextureCacheHandle::Manual(_), Eviction::Auto) |
            (TextureCacheHandle::Auto(_), Eviction::Manual) => {
                panic!("Can't change eviction policy after initial allocation");
            },
            (TextureCacheHandle::Empty, Eviction::Auto) => {
                let new_handle = self.lru_cache.push_new(budget_type as u8, new_cache_entry);
                *handle = TextureCacheHandle::Auto(new_handle);
                None
            },
            (TextureCacheHandle::Empty, Eviction::Manual) => {
                let manual_handle = self.manual_entries.insert(new_cache_entry);
                let new_handle = manual_handle.weak();
                self.manual_handles.push(manual_handle);
                *handle = TextureCacheHandle::Manual(new_handle);
                None
            },
        };
        if let Some(old_entry) = old_entry {
            old_entry.evict();
            self.free(&old_entry);
        }

        if let EntryDetails::Cache { alloc_id, .. } = details {
            let allocator_list = self.shared_textures.select(
                params.descriptor.format,
                params.filter,
                params.shader,
            ).0;

            allocator_list.set_handle(texture_id, alloc_id, handle);
        }
    }

    pub fn shared_alpha_expected_format(&self) -> ImageFormat {
        self.shared_textures.alpha8_linear.texture_parameters().formats.external
    }

    pub fn shared_color_expected_format(&self) -> ImageFormat {
        self.shared_textures.color8_linear.texture_parameters().formats.external
    }


    #[cfg(test)]
    pub fn total_allocated_bytes_for_testing(&self) -> usize {
        BudgetType::iter().map(|b| self.bytes_allocated[b as usize]).sum()
    }

    pub fn report_memory(&self, ops: &mut MallocSizeOfOps) -> usize {
        self.lru_cache.size_of(ops)
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
                let offset = descriptor.offset + dirty.min.y * stride + dirty.min.x * descriptor.format.bytes_per_pixel();

                TextureCacheUpdate {
                    rect: DeviceIntRect::from_origin_and_size(
                        DeviceIntPoint::new(origin.x + dirty.min.x, origin.y + dirty.min.y),
                        DeviceIntSize::new(
                            dirty.width().min(size.width - dirty.min.x),
                            dirty.height().min(size.height - dirty.min.y),
                        ),
                    ),
                    source,
                    stride: Some(stride),
                    offset,
                    format_override,
                }
            }
            DirtyRect::All => {
                TextureCacheUpdate {
                    rect: DeviceIntRect::from_origin_and_size(origin, size),
                    source,
                    stride: descriptor.stride,
                    offset: descriptor.offset,
                    format_override,
                }
            }
        }
    }
}

#[cfg(test)]
mod test_texture_cache {
    #[test]
    fn check_allocation_size_balance() {
        // Allocate some glyphs, observe the total allocation size, and free
        // the glyphs again. Check that the total allocation size is back at the
        // original value.

        use crate::texture_cache::{TextureCache, TextureCacheHandle, Eviction, TargetShader};
        use crate::gpu_cache::GpuCache;
        use crate::device::TextureFilter;
        use crate::gpu_types::UvRectKind;
        use api::{ImageDescriptor, ImageDescriptorFlags, ImageFormat, DirtyRect};
        use api::units::*;
        use euclid::size2;
        let mut texture_cache = TextureCache::new_for_testing(2048, ImageFormat::BGRA8);
        let mut gpu_cache = GpuCache::new_for_testing();

        let sizes: &[DeviceIntSize] = &[
            size2(23, 27),
            size2(15, 22),
            size2(11, 5),
            size2(20, 25),
            size2(38, 41),
            size2(11, 19),
            size2(13, 21),
            size2(37, 40),
            size2(13, 15),
            size2(14, 16),
            size2(10, 9),
            size2(25, 28),
        ];

        let bytes_at_start = texture_cache.total_allocated_bytes_for_testing();

        let handles: Vec<TextureCacheHandle> = sizes.iter().map(|size| {
            let mut texture_cache_handle = TextureCacheHandle::invalid();
            texture_cache.request(&texture_cache_handle, &mut gpu_cache);
            texture_cache.update(
                &mut texture_cache_handle,
                ImageDescriptor {
                    size: *size,
                    stride: None,
                    format: ImageFormat::BGRA8,
                    flags: ImageDescriptorFlags::empty(),
                    offset: 0,
                },
                TextureFilter::Linear,
                None,
                [0.0; 4],
                DirtyRect::All,
                &mut gpu_cache,
                None,
                UvRectKind::Rect,
                Eviction::Manual,
                TargetShader::Text,
            );
            texture_cache_handle
        }).collect();

        let bytes_after_allocating = texture_cache.total_allocated_bytes_for_testing();
        assert!(bytes_after_allocating > bytes_at_start);

        for handle in handles {
            texture_cache.evict_handle(&handle);
        }

        let bytes_at_end = texture_cache.total_allocated_bytes_for_testing();
        assert_eq!(bytes_at_end, bytes_at_start);
    }
}
