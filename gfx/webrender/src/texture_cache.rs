/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{DeviceUintPoint, DeviceUintRect, DeviceUintSize};
use api::{ExternalImageType, ImageData, ImageFormat};
use api::ImageDescriptor;
use device::{TextureFilter, total_gpu_bytes_allocated};
use freelist::{FreeList, FreeListHandle, UpsertResult, WeakFreeListHandle};
use gpu_cache::{GpuCache, GpuCacheHandle};
use gpu_types::{ImageSource, UvRectKind};
use internal_types::{CacheTextureId, LayerIndex, TextureUpdateList, TextureUpdateSource};
use internal_types::{TextureSource, TextureCacheAllocInfo, TextureCacheUpdate};
use profiler::{ResourceProfileCounter, TextureCacheProfileCounters};
use render_backend::FrameId;
use resource_cache::CacheItem;
use std::cell::Cell;
use std::cmp;
use std::mem;
use std::rc::Rc;

/// The size of each region/layer in shared cache texture arrays.
const TEXTURE_REGION_DIMENSIONS: u32 = 512;

// Items in the texture cache can either be standalone textures,
// or a sub-rect inside the shared cache.
#[derive(Debug)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
enum EntryKind {
    Standalone,
    Cache {
        // Origin within the texture layer where this item exists.
        origin: DeviceUintPoint,
        // The layer index of the texture array.
        layer_index: usize,
    },
}

impl EntryKind {
    /// Returns true if this corresponds to a standalone cache entry.
    fn is_standalone(&self) -> bool {
        matches!(*self, EntryKind::Standalone)
    }
}

#[derive(Debug)]
pub enum CacheEntryMarker {}

// Stores information related to a single entry in the texture
// cache. This is stored for each item whether it's in the shared
// cache or a standalone texture.
#[derive(Debug)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
struct CacheEntry {
    /// Size the requested item, in device pixels.
    size: DeviceUintSize,
    /// Details specific to standalone or shared items.
    kind: EntryKind,
    /// Arbitrary user data associated with this item.
    user_data: [f32; 3],
    /// The last frame this item was requested for rendering.
    last_access: FrameId,
    /// Handle to the resource rect in the GPU cache.
    uv_rect_handle: GpuCacheHandle,
    /// Image format of the item.
    format: ImageFormat,
    filter: TextureFilter,
    /// The actual device texture ID this is part of.
    texture_id: CacheTextureId,
    /// Optional notice when the entry is evicted from the cache.
    eviction_notice: Option<EvictionNotice>,
    /// The type of UV rect this entry specifies.
    uv_rect_kind: UvRectKind,
    /// If set to `Auto` the cache entry may be evicted if unused for a number of frames.
    eviction: Eviction,
}

impl CacheEntry {
    // Create a new entry for a standalone texture.
    fn new_standalone(
        texture_id: CacheTextureId,
        size: DeviceUintSize,
        format: ImageFormat,
        filter: TextureFilter,
        user_data: [f32; 3],
        last_access: FrameId,
        uv_rect_kind: UvRectKind,
    ) -> Self {
        CacheEntry {
            size,
            user_data,
            last_access,
            kind: EntryKind::Standalone,
            texture_id,
            format,
            filter,
            uv_rect_handle: GpuCacheHandle::new(),
            eviction_notice: None,
            uv_rect_kind,
            eviction: Eviction::Auto,
        }
    }

    // Update the GPU cache for this texture cache entry.
    // This ensures that the UV rect, and texture layer index
    // are up to date in the GPU cache for vertex shaders
    // to fetch from.
    fn update_gpu_cache(&mut self, gpu_cache: &mut GpuCache) {
        if let Some(mut request) = gpu_cache.request(&mut self.uv_rect_handle) {
            let (origin, layer_index) = match self.kind {
                EntryKind::Standalone { .. } => (DeviceUintPoint::zero(), 0.0),
                EntryKind::Cache {
                    origin,
                    layer_index,
                    ..
                } => (origin, layer_index as f32),
            };
            let image_source = ImageSource {
                p0: origin.to_f32(),
                p1: (origin + self.size).to_f32(),
                texture_layer: layer_index,
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
    /// The entry will be evicted if it was not used in the last frame.
    ///
    /// FIXME(bholley): Currently this only applies to the standalone case.
    Eager,
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
    array_rgba8_nearest: TextureArray,
    array_a8_linear: TextureArray,
    array_a16_linear: TextureArray,
    array_rgba8_linear: TextureArray,
}

impl SharedTextures {
    /// Mints a new set of shared textures.
    fn new() -> Self {
        Self {
            // Used primarily for cached shadow masks. There can be lots of
            // these on some pages like francine, but most pages don't use it
            // much.
            array_a8_linear: TextureArray::new(
                ImageFormat::R8,
                TextureFilter::Linear,
                4,
            ),
            // Used for experimental hdr yuv texture support, but not used in
            // production Firefox.
            array_a16_linear: TextureArray::new(
                ImageFormat::R16,
                TextureFilter::Linear,
                4,
            ),
            // The primary cache for images, glyphs, etc.
            array_rgba8_linear: TextureArray::new(
                ImageFormat::BGRA8,
                TextureFilter::Linear,
                16 * 4,
            ),
            // Used for image-rendering: crisp. This is mostly favicons, which
            // are small. Some other images use it too, but those tend to be
            // larger than 512x512 and thus don't use the shared cache anyway.
            //
            // Even though most of the buckets will be sparsely-used, we still
            // need a few to account for different favicon sizes. 4 seems enough
            // in practice, though we might also be able to get away with 2 or 3.
            array_rgba8_nearest: TextureArray::new(
                ImageFormat::BGRA8,
                TextureFilter::Nearest,
                4,
            ),
        }
    }

    /// Clears each texture in the set, with the given set of pending updates.
    fn clear(&mut self, updates: &mut TextureUpdateList) {
        self.array_a8_linear.clear(updates);
        self.array_a16_linear.clear(updates);
        self.array_rgba8_linear.clear(updates);
        self.array_rgba8_nearest.clear(updates);
    }

    /// Returns a mutable borrow for the shared texture array matching the parameters.
    fn select(&mut self, format: ImageFormat, filter: TextureFilter) -> &mut TextureArray {
        match (format, filter) {
            (ImageFormat::R8, TextureFilter::Linear) => &mut self.array_a8_linear,
            (ImageFormat::R16, TextureFilter::Linear) => &mut self.array_a16_linear,
            (ImageFormat::BGRA8, TextureFilter::Linear) => &mut self.array_rgba8_linear,
            (ImageFormat::BGRA8, TextureFilter::Nearest) => &mut self.array_rgba8_nearest,
            (_, _) => unreachable!(),
        }
    }
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

    // Maximum texture size supported by hardware.
    max_texture_size: u32,

    // The next unused virtual texture ID. Monotonically increasing.
    next_id: CacheTextureId,

    // A list of allocations and updates that need to be
    // applied to the texture cache in the rendering thread
    // this frame.
    #[cfg_attr(all(feature = "serde", any(feature = "capture", feature = "replay")), serde(skip))]
    pending_updates: TextureUpdateList,

    // The current frame ID. Used for cache eviction policies.
    frame_id: FrameId,

    // Maintains the list of all current items in
    // the texture cache.
    entries: FreeList<CacheEntry, CacheEntryMarker>,

    // A list of the strong handles of items that were
    // allocated in the standalone texture pool. Used
    // for evicting old standalone textures.
    standalone_entry_handles: Vec<FreeListHandle<CacheEntryMarker>>,

    // A list of the strong handles of items that were
    // allocated in the shared texture cache. Used
    // for evicting old cache items.
    shared_entry_handles: Vec<FreeListHandle<CacheEntryMarker>>,
}

impl TextureCache {
    pub fn new(max_texture_size: u32) -> Self {
        TextureCache {
            shared_textures: SharedTextures::new(),
            max_texture_size,
            next_id: CacheTextureId(1),
            pending_updates: TextureUpdateList::new(),
            frame_id: FrameId::invalid(),
            entries: FreeList::new(),
            standalone_entry_handles: Vec::new(),
            shared_entry_handles: Vec::new(),
        }
    }

    pub fn clear(&mut self) {
        let standalone_entry_handles = mem::replace(
            &mut self.standalone_entry_handles,
            Vec::new(),
        );

        for handle in standalone_entry_handles {
            let entry = self.entries.free(handle);
            entry.evict();
            self.free(entry);
        }

        let shared_entry_handles = mem::replace(
            &mut self.shared_entry_handles,
            Vec::new(),
        );

        for handle in shared_entry_handles {
            let entry = self.entries.free(handle);
            entry.evict();
            self.free(entry);
        }

        assert!(self.entries.len() == 0);

        self.shared_textures.clear(&mut self.pending_updates);
    }

    pub fn begin_frame(&mut self, frame_id: FrameId) {
        self.frame_id = frame_id;
    }

    pub fn end_frame(&mut self, texture_cache_profile: &mut TextureCacheProfileCounters) {
        self.expire_old_standalone_entries();

        self.shared_textures.array_a8_linear
            .update_profile(&mut texture_cache_profile.pages_a8_linear);
        self.shared_textures.array_a16_linear
            .update_profile(&mut texture_cache_profile.pages_a16_linear);
        self.shared_textures.array_rgba8_linear
            .update_profile(&mut texture_cache_profile.pages_rgba8_linear);
        self.shared_textures.array_rgba8_nearest
            .update_profile(&mut texture_cache_profile.pages_rgba8_nearest);
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
        match self.entries.get_opt_mut(handle) {
            // If an image is requested that is already in the cache,
            // refresh the GPU cache data associated with this item.
            Some(entry) => {
                entry.last_access = self.frame_id;
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
        self.entries.get_opt(handle).is_none()
    }

    pub fn max_texture_size(&self) -> u32 {
        self.max_texture_size
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
        data: Option<ImageData>,
        user_data: [f32; 3],
        mut dirty_rect: Option<DeviceUintRect>,
        gpu_cache: &mut GpuCache,
        eviction_notice: Option<&EvictionNotice>,
        uv_rect_kind: UvRectKind,
        eviction: Eviction,
    ) {
        // Determine if we need to allocate texture cache memory
        // for this item. We need to reallocate if any of the following
        // is true:
        // - Never been in the cache
        // - Has been in the cache but was evicted.
        // - Exists in the cache but dimensions / format have changed.
        let realloc = match self.entries.get_opt(handle) {
            Some(entry) => {
                entry.size != descriptor.size || entry.format != descriptor.format
            }
            None => {
                // Not allocated, or was previously allocated but has been evicted.
                true
            }
        };

        if realloc {
            self.allocate(
                handle,
                descriptor,
                filter,
                user_data,
                uv_rect_kind,
            );

            // If we reallocated, we need to upload the whole item again.
            dirty_rect = None;
        }

        let entry = self.entries.get_opt_mut(handle)
            .expect("BUG: handle must be valid now");

        // Install the new eviction notice for this update, if applicable.
        entry.eviction_notice = eviction_notice.cloned();

        // Invalidate the contents of the resource rect in the GPU cache.
        // This ensures that the update_gpu_cache below will add
        // the new information to the GPU cache.
        gpu_cache.invalidate(&entry.uv_rect_handle);

        // Upload the resource rect and texture array layer.
        entry.update_gpu_cache(gpu_cache);

        entry.eviction = eviction;

        // Create an update command, which the render thread processes
        // to upload the new image data into the correct location
        // in GPU memory.
        if let Some(data) = data {
            let (layer_index, origin) = match entry.kind {
                EntryKind::Standalone { .. } => (0, DeviceUintPoint::zero()),
                EntryKind::Cache {
                    layer_index,
                    origin,
                    ..
                } => (layer_index, origin),
            };

            let op = TextureCacheUpdate::new_update(
                data,
                &descriptor,
                origin,
                entry.size,
                entry.texture_id,
                layer_index as i32,
                dirty_rect,
            );
            self.pending_updates.push_update(op);
        }
    }

    // Get a specific region by index from a shared texture array.
    fn get_region_mut(&mut self,
        format: ImageFormat,
        filter: TextureFilter,
        layer_index: usize,
    ) -> &mut TextureRegion {
        let texture_array = self.shared_textures.select(format, filter);
        &mut texture_array.regions[layer_index]
    }

    // Check if a given texture handle has a valid allocation
    // in the texture cache.
    pub fn is_allocated(&self, handle: &TextureCacheHandle) -> bool {
        self.entries.get_opt(handle).is_some()
    }

    // Retrieve the details of an item in the cache. This is used
    // during batch creation to provide the resource rect address
    // to the shaders and texture ID to the batching logic.
    // This function will assert in debug modes if the caller
    // tries to get a handle that was not requested this frame.
    pub fn get(&self, handle: &TextureCacheHandle) -> CacheItem {
        let entry = self.entries
            .get_opt(handle)
            .expect("BUG: was dropped from cache or not updated!");
        debug_assert_eq!(entry.last_access, self.frame_id);
        let (layer_index, origin) = match entry.kind {
            EntryKind::Standalone { .. } => {
                (0, DeviceUintPoint::zero())
            }
            EntryKind::Cache {
                layer_index,
                origin,
                ..
            } => (layer_index, origin),
        };
        CacheItem {
            uv_rect_handle: entry.uv_rect_handle,
            texture_id: TextureSource::TextureCache(entry.texture_id),
            uv_rect: DeviceUintRect::new(origin, entry.size),
            texture_layer: layer_index as i32,
        }
    }

    /// A more detailed version of get(). This allows access to the actual
    /// device rect of the cache allocation.
    ///
    /// Returns a tuple identifying the texture, the layer, and the region.
    pub fn get_cache_location(
        &self,
        handle: &TextureCacheHandle,
    ) -> (CacheTextureId, LayerIndex, DeviceUintRect) {
        let entry = self.entries
            .get_opt(handle)
            .expect("BUG: was dropped from cache or not updated!");
        debug_assert_eq!(entry.last_access, self.frame_id);
        let (layer_index, origin) = match entry.kind {
            EntryKind::Standalone { .. } => {
                (0, DeviceUintPoint::zero())
            }
            EntryKind::Cache {
                layer_index,
                origin,
                ..
            } => (layer_index, origin),
        };
        (entry.texture_id,
         layer_index as usize,
         DeviceUintRect::new(origin, entry.size))
    }

    pub fn mark_unused(&mut self, handle: &TextureCacheHandle) {
        if let Some(entry) = self.entries.get_opt_mut(handle) {
            // Set last accessed frame to invalid to ensure it gets cleaned up
            // next time we expire entries.
            entry.last_access = FrameId::invalid();
            entry.eviction = Eviction::Auto;
        }
    }

    /// Expires old standalone textures. Called at the end of every frame.
    ///
    /// Most of the time, standalone cache entries correspond to images whose
    /// width or height is greater than the region size in the shared cache, i.e.
    /// 512 pixels. Cached render tasks also frequently get standalone entries,
    /// but those use the Eviction::Eager policy (for now). So the tradeoff here
    /// is largely around reducing texture upload jank while keeping memory usage
    /// at an acceptable level.
    ///
    /// Our eviction scheme is based on the age of the entry, i.e. the difference
    /// between the current frame index and that of the last frame in
    /// which the entry was used. It does not directly consider the size of the
    /// entry, but does consider overall memory usage by WebRender, by making
    /// eviction increasingly aggressive as overall memory usage increases.
    fn expire_old_standalone_entries(&mut self) {
        // These parameters are based on some discussion and local tuning, but
        // no hard measurement. There may be room for improvement.
        //
        // See discussion at https://mozilla.logbot.info/gfx/20181030#c15541654
        const MAX_FRAME_AGE_WITHOUT_PRESSURE: f64 = 75.0;
        const MAX_MEMORY_PRESSURE_BYTES: f64 = (500 * 1024 * 1024) as f64;

        // Compute the memory pressure factor in the range of [0, 1.0].
        let pressure_factor =
            (total_gpu_bytes_allocated() as f64 / MAX_MEMORY_PRESSURE_BYTES as f64).min(1.0);

        // Use the pressure factor to compute the maximum number of frames that
        // a standalone texture can go unused before being evicted.
        let max_frame_age_raw =
            ((1.0 - pressure_factor) * MAX_FRAME_AGE_WITHOUT_PRESSURE) as usize;

        // We clamp max_frame_age to frame_id - 1 so that entries with FrameId(0)
        // always get evicted, even early in the lifetime of the Renderer.
        let max_frame_age = max_frame_age_raw.min(self.frame_id.as_usize() - 1);

        // Compute the oldest FrameId for which we will not evict.
        let frame_id_threshold = self.frame_id - max_frame_age;

        // Iterate over the entries in reverse order, evicting the ones older than
        // the frame age threshold. Reverse order avoids iterator invalidation when
        // removing entries.
        for i in (0..self.standalone_entry_handles.len()).rev() {
            let evict = {
                let entry = self.entries.get(&self.standalone_entry_handles[i]);
                match entry.eviction {
                    Eviction::Manual => false,
                    Eviction::Auto => entry.last_access < frame_id_threshold,
                    Eviction::Eager => entry.last_access < self.frame_id,
                }
            };
            if evict {
                let handle = self.standalone_entry_handles.swap_remove(i);
                let entry = self.entries.free(handle);
                entry.evict();
                self.free(entry);
            }
        }
    }

    // Expire old shared items. Pass in the allocation size
    // that is being requested, so we know when we've evicted
    // enough items to guarantee we can fit this allocation in
    // the cache.
    fn expire_old_shared_entries(&mut self, required_alloc: &ImageDescriptor) {
        let mut eviction_candidates = Vec::new();
        let mut retained_entries = Vec::new();

        // Build a list of eviction candidates (which are
        // anything not used this frame).
        for handle in self.shared_entry_handles.drain(..) {
            let entry = self.entries.get(&handle);
            if entry.eviction == Eviction::Manual || entry.last_access == self.frame_id {
                retained_entries.push(handle);
            } else {
                eviction_candidates.push(handle);
            }
        }

        // Sort by access time so we remove the oldest ones first.
        eviction_candidates.sort_by_key(|handle| {
            let entry = self.entries.get(handle);
            entry.last_access
        });

        // Doing an eviction is quite expensive, so we don't want to
        // do it all the time. To avoid this, try and evict a
        // significant number of items each cycle. However, we don't
        // want to evict everything we can, since that will result in
        // more items being uploaded than necessary.
        // Instead, we say we will keep evicting until both of these
        // conditions are met:
        // - We have evicted some arbitrary number of items (512 currently).
        //   AND
        // - We have freed an item that will definitely allow us to
        //   fit the currently requested allocation.
        let needed_slab_size = SlabSize::new(required_alloc.size);
        let mut found_matching_slab = false;
        let mut freed_complete_page = false;
        let mut evicted_items = 0;

        for handle in eviction_candidates {
            if evicted_items > 512 && (found_matching_slab || freed_complete_page) {
                retained_entries.push(handle);
            } else {
                let entry = self.entries.free(handle);
                entry.evict();
                if let Some(region) = self.free(entry) {
                    found_matching_slab |= region.slab_size == needed_slab_size;
                    freed_complete_page |= region.is_empty();
                }
                evicted_items += 1;
            }
        }

        // Keep a record of the remaining handles for next eviction cycle.
        self.shared_entry_handles = retained_entries;
    }

    // Free a cache entry from the standalone list or shared cache.
    fn free(&mut self, entry: CacheEntry) -> Option<&TextureRegion> {
        match entry.kind {
            EntryKind::Standalone { .. } => {
                // This is a standalone texture allocation. Free it directly.
                self.pending_updates.push_free(entry.texture_id);
                None
            }
            EntryKind::Cache {
                origin,
                layer_index,
            } => {
                // Free the block in the given region.
                let region = self.get_region_mut(
                    entry.format,
                    entry.filter,
                    layer_index,
                );
                region.free(origin);
                Some(region)
            }
        }
    }

    // Attempt to allocate a block from the shared cache.
    fn allocate_from_shared_cache(
        &mut self,
        descriptor: &ImageDescriptor,
        filter: TextureFilter,
        user_data: [f32; 3],
        uv_rect_kind: UvRectKind,
    ) -> Option<CacheEntry> {
        // Mutably borrow the correct texture.
        let texture_array = self.shared_textures.select(descriptor.format, filter);

        // Lazy initialize this texture array if required.
        if texture_array.texture_id.is_none() {
            assert!(texture_array.regions.is_empty());
            let texture_id = self.next_id;
            self.next_id.0 += 1;

            let info = TextureCacheAllocInfo {
                width: TEXTURE_REGION_DIMENSIONS,
                height: TEXTURE_REGION_DIMENSIONS,
                format: descriptor.format,
                filter: texture_array.filter,
                layer_count: 1,
            };
            self.pending_updates.push_alloc(texture_id, info);

            texture_array.texture_id = Some(texture_id);
            texture_array.regions.push(TextureRegion::new(0));
        }

        // Do the allocation. This can fail and return None
        // if there are no free slots or regions available.
        texture_array.alloc(
            descriptor.size,
            user_data,
            self.frame_id,
            uv_rect_kind,
        )
    }

    // Returns true if the given image descriptor *may* be
    // placed in the shared texture cache.
    pub fn is_allowed_in_shared_cache(
        &self,
        filter: TextureFilter,
        descriptor: &ImageDescriptor,
    ) -> bool {
        let mut allowed_in_shared_cache = true;

        // TODO(gw): For now, anything that requests nearest filtering and isn't BGRA8
        //           just fails to allocate in a texture page, and gets a standalone
        //           texture. This is probably rare enough that it can be fixed up later.
        if filter == TextureFilter::Nearest &&
           descriptor.format != ImageFormat::BGRA8 {
            allowed_in_shared_cache = false;
        }

        // Anything larger than TEXTURE_REGION_DIMENSIONS goes in a standalone texture.
        // TODO(gw): If we find pages that suffer from batch breaks in this
        //           case, add support for storing these in a standalone
        //           texture array.
        if descriptor.size.width > TEXTURE_REGION_DIMENSIONS ||
           descriptor.size.height > TEXTURE_REGION_DIMENSIONS {
            allowed_in_shared_cache = false;
        }

        allowed_in_shared_cache
    }

    // Allocate storage for a given image. This attempts to allocate
    // from the shared cache, but falls back to standalone texture
    // if the image is too large, or the cache is full.
    fn allocate(
        &mut self,
        handle: &mut TextureCacheHandle,
        descriptor: ImageDescriptor,
        filter: TextureFilter,
        user_data: [f32; 3],
        uv_rect_kind: UvRectKind,
    ) {
        assert!(descriptor.size.width > 0 && descriptor.size.height > 0);

        // Work out if this image qualifies to go in the shared (batching) cache.
        let allowed_in_shared_cache = self.is_allowed_in_shared_cache(
            filter,
            &descriptor,
        );
        let mut allocated_standalone = false;
        let mut new_cache_entry = None;
        let frame_id = self.frame_id;

        // If it's allowed in the cache, see if there is a spot for it.
        if allowed_in_shared_cache {

            new_cache_entry = self.allocate_from_shared_cache(
                &descriptor,
                filter,
                user_data,
                uv_rect_kind,
            );

            // If we failed to allocate in the shared cache, make some space
            // and then try to allocate again. If we still have room to grow
            // the cache, we do that. Otherwise, we evict.
            //
            // We should improve this logic to support some degree of eviction
            // before the cache fills up eintirely.
            if new_cache_entry.is_none() {
                let reallocated = {
                    let texture_array = self.shared_textures.select(descriptor.format, filter);
                    let num_regions = texture_array.regions.len();
                    if num_regions < texture_array.max_layer_count {
                        // We have room to grow.
                        let info = TextureCacheAllocInfo {
                            width: TEXTURE_REGION_DIMENSIONS,
                            height: TEXTURE_REGION_DIMENSIONS,
                            format: descriptor.format,
                            filter: texture_array.filter,
                            layer_count: (num_regions + 1) as i32,
                        };
                        self.pending_updates.push_realloc(texture_array.texture_id.unwrap(), info);
                        texture_array.regions.push(TextureRegion::new(num_regions));
                        true
                    } else {
                        false
                    }
                };
                if !reallocated {
                    // Out of room. Evict.
                    self.expire_old_shared_entries(&descriptor);
                }

                new_cache_entry = self.allocate_from_shared_cache(
                    &descriptor,
                    filter,
                    user_data,
                    uv_rect_kind,
                );
            }
        }

        // If not allowed in the cache, or if the shared cache is full, then it
        // will just have to be in a unique texture. This hurts batching but should
        // only occur on a small number of images (or pathological test cases!).
        if new_cache_entry.is_none() {
            let texture_id = self.next_id;
            self.next_id.0 += 1;

            // Push a command to allocate device storage of the right size / format.
            let info = TextureCacheAllocInfo {
                width: descriptor.size.width,
                height: descriptor.size.height,
                format: descriptor.format,
                filter,
                layer_count: 1,
            };
            self.pending_updates.push_alloc(texture_id, info);

            new_cache_entry = Some(CacheEntry::new_standalone(
                texture_id,
                descriptor.size,
                descriptor.format,
                filter,
                user_data,
                frame_id,
                uv_rect_kind,
            ));

            allocated_standalone = true;
        }
        let new_cache_entry = new_cache_entry.expect("BUG: must have allocated by now");

        // If the handle points to a valid cache entry, we want to replace the
        // cache entry with our newly updated location. We also need to ensure
        // that the storage (region or standalone) associated with the previous
        // entry here gets freed.
        //
        // If the handle is invalid, we need to insert the data, and append the
        // result to the corresponding vector.
        //
        // This is managed with a database style upsert operation.
        match self.entries.upsert(handle, new_cache_entry) {
            UpsertResult::Updated(old_entry) => {
                if allocated_standalone != old_entry.kind.is_standalone() {
                    // Handle the rare case than an update moves an entry from
                    // shared to standalone or vice versa. This involves a linear
                    // search, but should be rare enough not to matter.
                    let (from, to) = if allocated_standalone {
                        (&mut self.shared_entry_handles, &mut self.standalone_entry_handles)
                    } else {
                        (&mut self.standalone_entry_handles, &mut self.shared_entry_handles)
                    };
                    let idx = from.iter().position(|h| h.weak() == *handle).unwrap();
                    to.push(from.remove(idx));
                }
                self.free(old_entry);
            }
            UpsertResult::Inserted(new_handle) => {
                *handle = new_handle.weak();
                if allocated_standalone {
                    self.standalone_entry_handles.push(new_handle);
                } else {
                    self.shared_entry_handles.push(new_handle);
                }
            }
        }
    }
}

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(Copy, Clone, PartialEq)]
struct SlabSize {
    width: u32,
    height: u32,
}

impl SlabSize {
    fn new(size: DeviceUintSize) -> SlabSize {
        let x_size = quantize_dimension(size.width);
        let y_size = quantize_dimension(size.height);

        assert!(x_size > 0 && x_size <= TEXTURE_REGION_DIMENSIONS);
        assert!(y_size > 0 && y_size <= TEXTURE_REGION_DIMENSIONS);

        let (width, height) = match (x_size, y_size) {
            // Special cased rectangular slab pages.
            (512, 256) => (512, 256),
            (512, 128) => (512, 128),
            (512,  64) => (512,  64),
            (256, 512) => (256, 512),
            (128, 512) => (128, 512),
            ( 64, 512) => ( 64, 512),

            // If none of those fit, use a square slab size.
            (x_size, y_size) => {
                let square_size = cmp::max(x_size, y_size);
                (square_size, square_size)
            }
        };

        SlabSize {
            width,
            height,
        }
    }

    fn invalid() -> SlabSize {
        SlabSize {
            width: 0,
            height: 0,
        }
    }
}

// The x/y location within a texture region of an allocation.
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
struct TextureLocation(u8, u8);

impl TextureLocation {
    fn new(x: u32, y: u32) -> Self {
        debug_assert!(x < 0x100 && y < 0x100);
        TextureLocation(x as u8, y as u8)
    }
}

/// A region corresponds to a layer in a shared cache texture.
///
/// All allocations within a region are of the same size.
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
struct TextureRegion {
    layer_index: usize,
    slab_size: SlabSize,
    free_slots: Vec<TextureLocation>,
    total_slot_count: usize,
}

impl TextureRegion {
    fn new(layer_index: usize) -> Self {
        TextureRegion {
            layer_index,
            slab_size: SlabSize::invalid(),
            free_slots: Vec::new(),
            total_slot_count: 0,
        }
    }

    // Initialize a region to be an allocator for a specific slab size.
    fn init(&mut self, slab_size: SlabSize) {
        debug_assert!(self.slab_size == SlabSize::invalid());
        debug_assert!(self.free_slots.is_empty());

        self.slab_size = slab_size;
        let slots_per_x_axis = TEXTURE_REGION_DIMENSIONS / self.slab_size.width;
        let slots_per_y_axis = TEXTURE_REGION_DIMENSIONS / self.slab_size.height;

        // Add each block to a freelist.
        for y in 0 .. slots_per_y_axis {
            for x in 0 .. slots_per_x_axis {
                self.free_slots.push(TextureLocation::new(x, y));
            }
        }

        self.total_slot_count = self.free_slots.len();
    }

    // Deinit a region, allowing it to become a region with
    // a different allocator size.
    fn deinit(&mut self) {
        self.slab_size = SlabSize::invalid();
        self.free_slots.clear();
        self.total_slot_count = 0;
    }

    fn is_empty(&self) -> bool {
        self.slab_size == SlabSize::invalid()
    }

    // Attempt to allocate a fixed size block from this region.
    fn alloc(&mut self) -> Option<DeviceUintPoint> {
        debug_assert!(self.slab_size != SlabSize::invalid());

        self.free_slots.pop().map(|location| {
            DeviceUintPoint::new(
                self.slab_size.width * location.0 as u32,
                self.slab_size.height * location.1 as u32,
            )
        })
    }

    // Free a block in this region.
    fn free(&mut self, point: DeviceUintPoint) {
        let x = point.x / self.slab_size.width;
        let y = point.y / self.slab_size.height;
        self.free_slots.push(TextureLocation::new(x, y));

        // If this region is completely unused, deinit it
        // so that it can become a different slab size
        // as required.
        if self.free_slots.len() == self.total_slot_count {
            self.deinit();
        }
    }
}

/// A texture array contains a number of texture layers, where each layer
/// contains a region that can act as a slab allocator.
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
struct TextureArray {
    filter: TextureFilter,
    max_layer_count: usize,
    format: ImageFormat,
    regions: Vec<TextureRegion>,
    texture_id: Option<CacheTextureId>,
}

impl TextureArray {
    fn new(
        format: ImageFormat,
        filter: TextureFilter,
        max_layer_count: usize,
    ) -> Self {
        TextureArray {
            format,
            filter,
            max_layer_count,
            regions: Vec::new(),
            texture_id: None,
        }
    }

    fn clear(&mut self, updates: &mut TextureUpdateList) {
        self.regions.clear();
        if let Some(id) = self.texture_id.take() {
            updates.push_free(id);
        }
    }

    fn update_profile(&self, counter: &mut ResourceProfileCounter) {
        let layer_count = self.regions.len();
        if layer_count != 0 {
            let size = layer_count as u32 * TEXTURE_REGION_DIMENSIONS *
                TEXTURE_REGION_DIMENSIONS * self.format.bytes_per_pixel();
            counter.set(layer_count as usize, size as usize);
        } else {
            counter.set(0, 0);
        }
    }

    // Allocate space in this texture array.
    fn alloc(
        &mut self,
        size: DeviceUintSize,
        user_data: [f32; 3],
        frame_id: FrameId,
        uv_rect_kind: UvRectKind,
    ) -> Option<CacheEntry> {
        // Quantize the size of the allocation to select a region to
        // allocate from.
        let slab_size = SlabSize::new(size);

        // TODO(gw): For simplicity, the initial implementation just
        //           has a single vec<> of regions. We could easily
        //           make this more efficient by storing a list of
        //           regions for each slab size specifically...

        // Keep track of the location of an empty region,
        // in case we need to select a new empty region
        // after the loop.
        let mut empty_region_index = None;
        let mut entry_kind = None;

        // Run through the existing regions of this size, and see if
        // we can find a free block in any of them.
        for (i, region) in self.regions.iter_mut().enumerate() {
            if region.is_empty() {
                empty_region_index = Some(i);
            } else if region.slab_size == slab_size {
                if let Some(location) = region.alloc() {
                    entry_kind = Some(EntryKind::Cache {
                        layer_index: region.layer_index,
                        origin: location,
                    });
                    break;
                }
            }
        }

        // Find a region of the right size and try to allocate from it.
        if entry_kind.is_none() {
            if let Some(empty_region_index) = empty_region_index {
                let region = &mut self.regions[empty_region_index];
                region.init(slab_size);
                entry_kind = region.alloc().map(|location| {
                    EntryKind::Cache {
                        layer_index: region.layer_index,
                        origin: location,
                    }
                });
            }
        }

        entry_kind.map(|kind| {
            CacheEntry {
                size,
                user_data,
                last_access: frame_id,
                kind,
                uv_rect_handle: GpuCacheHandle::new(),
                format: self.format,
                filter: self.filter,
                texture_id: self.texture_id.unwrap(),
                eviction_notice: None,
                uv_rect_kind,
                eviction: Eviction::Auto,
            }
        })
    }
}

impl TextureCacheUpdate {
    // Constructs a TextureCacheUpdate operation to be passed to the
    // rendering thread in order to do an upload to the right
    // location in the texture cache.
    fn new_update(
        data: ImageData,
        descriptor: &ImageDescriptor,
        origin: DeviceUintPoint,
        size: DeviceUintSize,
        texture_id: CacheTextureId,
        layer_index: i32,
        dirty_rect: Option<DeviceUintRect>,
    ) -> TextureCacheUpdate {
        let source = match data {
            ImageData::Blob(..) => {
                panic!("The vector image should have been rasterized.");
            }
            ImageData::External(ext_image) => match ext_image.image_type {
                ExternalImageType::TextureHandle(_) => {
                    panic!("External texture handle should not go through texture_cache.");
                }
                ExternalImageType::Buffer => TextureUpdateSource::External {
                    id: ext_image.id,
                    channel_index: ext_image.channel_index,
                },
            },
            ImageData::Raw(bytes) => {
                let finish = descriptor.offset +
                    descriptor.size.width * descriptor.format.bytes_per_pixel() +
                    (descriptor.size.height - 1) * descriptor.compute_stride();
                assert!(bytes.len() >= finish as usize);

                TextureUpdateSource::Bytes { data: bytes }
            }
        };

        let update_op = match dirty_rect {
            Some(dirty) => {
                // the dirty rectangle doesn't have to be within the area but has to intersect it, at least
                let stride = descriptor.compute_stride();
                let offset = descriptor.offset + dirty.origin.y * stride + dirty.origin.x * descriptor.format.bytes_per_pixel();

                TextureCacheUpdate {
                    id: texture_id,
                    rect: DeviceUintRect::new(
                        DeviceUintPoint::new(origin.x + dirty.origin.x, origin.y + dirty.origin.y),
                        DeviceUintSize::new(
                            dirty.size.width.min(size.width - dirty.origin.x),
                            dirty.size.height.min(size.height - dirty.origin.y),
                        ),
                    ),
                    source,
                    stride: Some(stride),
                    offset,
                    layer_index,
                }
            }
            None => {
                TextureCacheUpdate {
                    id: texture_id,
                    rect: DeviceUintRect::new(origin, size),
                    source,
                    stride: descriptor.stride,
                    offset: descriptor.offset,
                    layer_index,
                }
            }
        };

        update_op
    }
}

fn quantize_dimension(size: u32) -> u32 {
    match size {
        0 => unreachable!(),
        1...16 => 16,
        17...32 => 32,
        33...64 => 64,
        65...128 => 128,
        129...256 => 256,
        257...512 => 512,
        _ => panic!("Invalid dimensions for cache!"),
    }
}
