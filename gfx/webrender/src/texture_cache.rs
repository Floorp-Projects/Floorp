/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{DeviceUintPoint, DeviceUintRect, DeviceUintSize};
use api::{ExternalImageType, ImageData, ImageFormat};
use api::ImageDescriptor;
use device::TextureFilter;
use frame::FrameId;
use freelist::{FreeList, FreeListHandle, UpsertResult, WeakFreeListHandle};
use gpu_cache::{GpuCache, GpuCacheHandle};
use internal_types::{CacheTextureId, TextureUpdateList, TextureUpdateSource};
use internal_types::{RenderTargetInfo, SourceTexture, TextureUpdate, TextureUpdateOp};
use profiler::{ResourceProfileCounter, TextureCacheProfileCounters};
use resource_cache::CacheItem;
use std::cmp;
use std::mem;

// The fixed number of layers for the shared texture cache.
// There is one array texture per image format, allocated lazily.
const TEXTURE_ARRAY_LAYERS_LINEAR: usize = 4;
const TEXTURE_ARRAY_LAYERS_NEAREST: usize = 1;

// The dimensions of each layer in the texture cache.
const TEXTURE_LAYER_DIMENSIONS: u32 = 2048;

// The size of each region (page) in a texture layer.
const TEXTURE_REGION_DIMENSIONS: u32 = 512;

// Maintains a simple freelist of texture IDs that are mapped
// to real API-specific texture IDs in the renderer.
struct CacheTextureIdList {
    free_list: Vec<CacheTextureId>,
    next_id: usize,
}

impl CacheTextureIdList {
    fn new() -> CacheTextureIdList {
        CacheTextureIdList {
            next_id: 0,
            free_list: Vec::new(),
        }
    }

    fn allocate(&mut self) -> CacheTextureId {
        // If nothing on the free list of texture IDs,
        // allocate a new one.
        match self.free_list.pop() {
            Some(id) => id,
            None => {
                let id = CacheTextureId(self.next_id);
                self.next_id += 1;
                id
            }
        }
    }

    fn free(&mut self, id: CacheTextureId) {
        self.free_list.push(id);
    }
}

// Items in the texture cache can either be standalone textures,
// or a sub-rect inside the shared cache.
#[derive(Debug)]
enum EntryKind {
    Standalone,
    Cache {
        // Origin within the texture layer where this item exists.
        origin: DeviceUintPoint,
        // The layer index of the texture array.
        layer_index: u16,
        // The region that this entry belongs to in the layer.
        region_index: u16,
    },
}

// Stores information related to a single entry in the texture
// cache. This is stored for each item whether it's in the shared
// cache or a standalone texture.
#[derive(Debug)]
struct CacheEntry {
    // Size the requested item, in device pixels.
    size: DeviceUintSize,
    // Details specific to standalone or shared items.
    kind: EntryKind,
    // Arbitrary user data associated with this item.
    user_data: [f32; 3],
    // The last frame this item was requested for rendering.
    last_access: FrameId,
    // Handle to the resource rect in the GPU cache.
    uv_rect_handle: GpuCacheHandle,
    // Image format of the item.
    format: ImageFormat,
    filter: TextureFilter,
    // The actual device texture ID this is part of.
    texture_id: CacheTextureId,
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
            request.push([
                origin.x as f32,
                origin.y as f32,
                (origin.x + self.size.width) as f32,
                (origin.y + self.size.height) as f32,
            ]);
            request.push([layer_index, self.user_data[0], self.user_data[1], self.user_data[2]]);
        }
    }
}

type WeakCacheEntryHandle = WeakFreeListHandle<CacheEntry>;

// A texture cache handle is a weak reference to a cache entry.
// If the handle has not been inserted into the cache yet, the
// value will be None. Even when the value is Some(), the location
// may not actually be valid if it has been evicted by the cache.
// In this case, the cache handle needs to re-upload this item
// to the texture cache (see request() below).
#[derive(Debug)]
pub struct TextureCacheHandle {
    entry: Option<WeakCacheEntryHandle>,
}

impl TextureCacheHandle {
    pub fn new() -> TextureCacheHandle {
        TextureCacheHandle { entry: None }
    }
}

pub struct TextureCache {
    // A lazily allocated, fixed size, texture array for
    // each format the texture cache supports.
    array_rgba8_nearest: TextureArray,
    array_a8_linear: TextureArray,
    array_rgba8_linear: TextureArray,

    // Maximum texture size supported by hardware.
    max_texture_size: u32,

    // A list of texture IDs that represent native
    // texture handles. This indirection allows the texture
    // cache to create / destroy / reuse texture handles
    // without knowing anything about the device code.
    cache_textures: CacheTextureIdList,

    // A list of updates that need to be applied to the
    // texture cache in the rendering thread this frame.
    pending_updates: TextureUpdateList,

    // The current frame ID. Used for cache eviction policies.
    frame_id: FrameId,

    // Maintains the list of all current items in
    // the texture cache.
    entries: FreeList<CacheEntry>,

    // A list of the strong handles of items that were
    // allocated in the standalone texture pool. Used
    // for evicting old standalone textures.
    standalone_entry_handles: Vec<FreeListHandle<CacheEntry>>,

    // A list of the strong handles of items that were
    // allocated in the shared texture cache. Used
    // for evicting old cache items.
    shared_entry_handles: Vec<FreeListHandle<CacheEntry>>,
}

impl TextureCache {
    pub fn new(max_texture_size: u32) -> Self {
        TextureCache {
            max_texture_size,
            array_a8_linear: TextureArray::new(
                ImageFormat::A8,
                TextureFilter::Linear,
                TEXTURE_ARRAY_LAYERS_LINEAR,
            ),
            array_rgba8_linear: TextureArray::new(
                ImageFormat::BGRA8,
                TextureFilter::Linear,
                TEXTURE_ARRAY_LAYERS_LINEAR,
            ),
            array_rgba8_nearest: TextureArray::new(
                ImageFormat::BGRA8,
                TextureFilter::Nearest,
                TEXTURE_ARRAY_LAYERS_NEAREST
            ),
            cache_textures: CacheTextureIdList::new(),
            pending_updates: TextureUpdateList::new(),
            frame_id: FrameId(0),
            entries: FreeList::new(),
            standalone_entry_handles: Vec::new(),
            shared_entry_handles: Vec::new(),
        }
    }

    pub fn begin_frame(&mut self, frame_id: FrameId) {
        self.frame_id = frame_id;
    }

    pub fn end_frame(&mut self, texture_cache_profile: &mut TextureCacheProfileCounters) {
        self.expire_old_standalone_entries();

        self.array_a8_linear
            .update_profile(&mut texture_cache_profile.pages_a8_linear);
        self.array_rgba8_linear
            .update_profile(&mut texture_cache_profile.pages_rgba8_linear);
        self.array_rgba8_nearest
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
    pub fn request(&mut self, handle: &mut TextureCacheHandle, gpu_cache: &mut GpuCache) -> bool {
        match handle.entry {
            Some(ref handle) => {
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
            None => true,
        }
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
        data: ImageData,
        user_data: [f32; 3],
        mut dirty_rect: Option<DeviceUintRect>,
        gpu_cache: &mut GpuCache,
    ) {
        // Determine if we need to allocate texture cache memory
        // for this item. We need to reallocate if any of the following
        // is true:
        // - Never been in the cache
        // - Has been in the cache but was evicted.
        // - Exists in the cache but dimensions / format have changed.
        let realloc = match handle.entry {
            Some(ref handle) => {
                match self.entries.get_opt(handle) {
                    Some(entry) => {
                        entry.size.width != descriptor.width ||
                            entry.size.height != descriptor.height ||
                            entry.format != descriptor.format
                    }
                    None => {
                        // Was previously allocated but has been evicted.
                        true
                    }
                }
            }
            None => {
                // This handle has not been allocated yet.
                true
            }
        };

        if realloc {
            self.allocate(handle, descriptor, filter, user_data);

            // If we reallocated, we need to upload the whole item again.
            dirty_rect = None;
        }

        let entry = self.entries
            .get_opt_mut(handle.entry.as_ref().unwrap())
            .expect("BUG: handle must be valid now");

        // Invalidate the contents of the resource rect in the GPU cache.
        // This ensures that the update_gpu_cache below will add
        // the new information to the GPU cache.
        gpu_cache.invalidate(&entry.uv_rect_handle);

        // Upload the resource rect and texture array layer.
        entry.update_gpu_cache(gpu_cache);

        // Create an update command, which the render thread processes
        // to upload the new image data into the correct location
        // in GPU memory.
        let (layer_index, origin) = match entry.kind {
            EntryKind::Standalone { .. } => (0, DeviceUintPoint::zero()),
            EntryKind::Cache {
                layer_index,
                origin,
                ..
            } => (layer_index, origin),
        };

        let op = TextureUpdate::new_update(
            data,
            &descriptor,
            origin,
            entry.size,
            entry.texture_id,
            layer_index as i32,
            dirty_rect,
        );
        self.pending_updates.push(op);
    }

    // Get a specific region by index from a shared texture array.
    fn get_region_mut(&mut self,
        format: ImageFormat,
        filter: TextureFilter,
        region_index: u16
    ) -> &mut TextureRegion {
        let texture_array = match (format, filter) {
            (ImageFormat::A8, TextureFilter::Linear) => &mut self.array_a8_linear,
            (ImageFormat::BGRA8, TextureFilter::Linear) => &mut self.array_rgba8_linear,
            (ImageFormat::BGRA8, TextureFilter::Nearest) => &mut self.array_rgba8_nearest,
            (ImageFormat::Invalid, _) |
            (ImageFormat::RGBAF32, _) |
            (ImageFormat::RG8, _) |
            (ImageFormat::A8, TextureFilter::Nearest) => unreachable!(),
        };

        &mut texture_array.regions[region_index as usize]
    }

    // Retrieve the details of an item in the cache. This is used
    // during batch creation to provide the resource rect address
    // to the shaders and texture ID to the batching logic.
    // This function will asssert in debug modes if the caller
    // tries to get a handle that was not requested this frame.
    pub fn get(&self, handle: &TextureCacheHandle) -> CacheItem {
        match handle.entry {
            Some(ref handle) => {
                let entry = self.entries
                    .get_opt(handle)
                    .expect("BUG: was dropped from cache or not updated!");
                debug_assert_eq!(entry.last_access, self.frame_id);
                CacheItem {
                    uv_rect_handle: entry.uv_rect_handle,
                    texture_id: SourceTexture::TextureCache(entry.texture_id),
                }
            }
            None => panic!("BUG: handle not requested earlier in frame"),
        }
    }

    // Expire old standalone textures.
    fn expire_old_standalone_entries(&mut self) {
        let mut eviction_candidates = Vec::new();
        let mut retained_entries = Vec::new();

        // Build a list of eviction candidates (which are
        // anything not used this frame).
        for handle in self.standalone_entry_handles.drain(..) {
            let entry = self.entries.get(&handle);
            if entry.last_access == self.frame_id {
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

        // We only allow an arbitrary number of unused
        // standalone textures to remain in GPU memory.
        // TODO(gw): We should make this a better heuristic,
        //           for example based on total memory size.
        if eviction_candidates.len() > 32 {
            let entries_to_keep = eviction_candidates.split_off(32);
            retained_entries.extend(entries_to_keep);
        }

        // Free the selected items
        for handle in eviction_candidates {
            let entry = self.entries.free(handle);
            self.free(entry);
        }

        // Keep a record of the remaining handles for next frame.
        self.standalone_entry_handles = retained_entries;
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
            if entry.last_access == self.frame_id {
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
        // consitions are met:
        // - We have evicted some arbitrary number of items (512 currently).
        //   AND
        // - We have freed an item that will definitely allow us to
        //   fit the currently requested allocation.
        let needed_slab_size =
            SlabSize::new(required_alloc.width, required_alloc.height).get_size();
        let mut found_matching_slab = false;
        let mut freed_complete_page = false;
        let mut evicted_items = 0;

        for handle in eviction_candidates {
            if evicted_items > 512 && (found_matching_slab || freed_complete_page) {
                retained_entries.push(handle);
            } else {
                let entry = self.entries.free(handle);
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
                // This is a standalone texture allocation. Just push it back onto the free
                // list.
                self.pending_updates.push(TextureUpdate {
                    id: entry.texture_id,
                    op: TextureUpdateOp::Free,
                });
                self.cache_textures.free(entry.texture_id);
                None
            }
            EntryKind::Cache {
                origin,
                region_index,
                ..
            } => {
                // Free the block in the given region.
                let region = self.get_region_mut(
                    entry.format,
                    entry.filter,
                    region_index
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
    ) -> Option<CacheEntry> {
        // Work out which cache it goes in, based on format.
        let texture_array = match (descriptor.format, filter) {
            (ImageFormat::A8, TextureFilter::Linear) => &mut self.array_a8_linear,
            (ImageFormat::BGRA8, TextureFilter::Linear) => &mut self.array_rgba8_linear,
            (ImageFormat::BGRA8, TextureFilter::Nearest) => &mut self.array_rgba8_nearest,
            (ImageFormat::Invalid, _) |
            (ImageFormat::RGBAF32, _) |
            (ImageFormat::A8, TextureFilter::Nearest) |
            (ImageFormat::RG8, _) => unreachable!(),
        };

        // Lazy initialize this texture array if required.
        if texture_array.texture_id.is_none() {
            let texture_id = self.cache_textures.allocate();

            let update_op = TextureUpdate {
                id: texture_id,
                op: TextureUpdateOp::Create {
                    width: TEXTURE_LAYER_DIMENSIONS,
                    height: TEXTURE_LAYER_DIMENSIONS,
                    format: descriptor.format,
                    filter: texture_array.filter,
                    // TODO(gw): Creating a render target here is only used
                    //           for the texture cache debugger display. In
                    //           the future, we should change the debug
                    //           display to use a shader that blits the
                    //           texture, and then we can remove this
                    //           memory allocation (same for the other
                    //           standalone texture below).
                    render_target: Some(RenderTargetInfo { has_depth: false }),
                    layer_count: texture_array.layer_count as i32,
                },
            };
            self.pending_updates.push(update_op);

            texture_array.texture_id = Some(texture_id);
        }

        // Do the allocation. This can fail and return None
        // if there are no free slots or regions available.
        texture_array.alloc(
            descriptor.width,
            descriptor.height,
            user_data,
            self.frame_id,
        )
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
    ) {
        assert!(descriptor.width > 0 && descriptor.height > 0);

        // Work out if this image qualifies to go in the shared (batching) cache.
        let mut allowed_in_shared_cache = true;
        let mut allocated_in_shared_cache = true;
        let mut new_cache_entry = None;
        let size = DeviceUintSize::new(descriptor.width, descriptor.height);
        let frame_id = self.frame_id;

        // TODO(gw): For now, anything that requests nearest filtering and isn't BGRA8
        //           just fails to allocate in a texture page, and gets a standalone
        //           texture. This is probably rare enough that it can be fixed up later.
        if filter == TextureFilter::Nearest && descriptor.format != ImageFormat::BGRA8 {
            allowed_in_shared_cache = false;
        }

        // Anything larger than 512 goes in a standalone texture.
        // TODO(gw): If we find pages that suffer from batch breaks in this
        //           case, add support for storing these in a standalone
        //           texture array.
        if descriptor.width > 512 || descriptor.height > 512 {
            allowed_in_shared_cache = false;
        }

        // If it's allowed in the cache, see if there is a spot for it.
        if allowed_in_shared_cache {
            new_cache_entry = self.allocate_from_shared_cache(
                &descriptor,
                filter,
                user_data
            );

            // If we failed to allocate in the shared cache, run an
            // eviction cycle, and then try to allocate again.
            if new_cache_entry.is_none() {
                self.expire_old_shared_entries(&descriptor);

                new_cache_entry = self.allocate_from_shared_cache(
                    &descriptor,
                    filter,
                    user_data
                );
            }
        }

        // If not allowed in the cache, or if the shared cache is full, then it
        // will just have to be in a unique texture. This hurts batching but should
        // only occur on a small number of images (or pathological test cases!).
        if new_cache_entry.is_none() {
            let texture_id = self.cache_textures.allocate();

            // Create an update operation to allocate device storage
            // of the right size / format.
            let update_op = TextureUpdate {
                id: texture_id,
                op: TextureUpdateOp::Create {
                    width: descriptor.width,
                    height: descriptor.height,
                    format: descriptor.format,
                    filter,
                    render_target: Some(RenderTargetInfo { has_depth: false }),
                    layer_count: 1,
                },
            };
            self.pending_updates.push(update_op);

            new_cache_entry = Some(CacheEntry::new_standalone(
                texture_id,
                size,
                descriptor.format,
                filter,
                user_data,
                frame_id,
            ));

            allocated_in_shared_cache = false;
        }

        let new_cache_entry = new_cache_entry.expect("BUG: must have allocated by now");

        // We need to update the texture cache handle now, so that it
        // points to the correct location.
        let new_entry_handle = match handle.entry {
            Some(ref existing_entry) => {
                // If the handle already exists, there's two possibilities:
                // 1) It points to a valid entry in the freelist.
                // 2) It points to a stale entry in the freelist (i.e. has been evicted).
                //
                // For (1) we want to replace the cache entry with our
                // newly updated location. We also need to ensure that
                // the storage (region or standalone) associated with the
                // previous entry here gets freed.
                //
                // For (2) we need to add the data to a new location
                // in the freelist.
                //
                // This is managed with a database style upsert operation.
                match self.entries.upsert(existing_entry, new_cache_entry) {
                    UpsertResult::Updated(old_entry) => {
                        self.free(old_entry);
                        None
                    }
                    UpsertResult::Inserted(new_handle) => Some(new_handle),
                }
            }
            None => {
                // This handle has never been allocated, so just
                // insert a new cache entry.
                Some(self.entries.insert(new_cache_entry))
            }
        };

        // If the cache entry is new, update it in the cache handle.
        if let Some(new_entry_handle) = new_entry_handle {
            handle.entry = Some(new_entry_handle.weak());
            // Store the strong handle in the list that we scan for
            // cache evictions.
            if allocated_in_shared_cache {
                self.shared_entry_handles.push(new_entry_handle);
            } else {
                self.standalone_entry_handles.push(new_entry_handle);
            }
        }
    }
}

// A list of the block sizes that a region can be initialized with.
#[derive(Copy, Clone, PartialEq)]
enum SlabSize {
    Size16x16,
    Size32x32,
    Size64x64,
    Size128x128,
    Size256x256,
    Size512x512,
}

impl SlabSize {
    fn new(width: u32, height: u32) -> SlabSize {
        // TODO(gw): Consider supporting non-square
        //           allocator sizes here.
        let max_dim = cmp::max(width, height);

        match max_dim {
            0 => unreachable!(),
            1...16 => SlabSize::Size16x16,
            17...32 => SlabSize::Size32x32,
            33...64 => SlabSize::Size64x64,
            65...128 => SlabSize::Size128x128,
            129...256 => SlabSize::Size256x256,
            257...512 => SlabSize::Size512x512,
            _ => panic!("Invalid dimensions for cache!"),
        }
    }

    fn get_size(&self) -> u32 {
        match *self {
            SlabSize::Size16x16 => 16,
            SlabSize::Size32x32 => 32,
            SlabSize::Size64x64 => 64,
            SlabSize::Size128x128 => 128,
            SlabSize::Size256x256 => 256,
            SlabSize::Size512x512 => 512,
        }
    }
}

// The x/y location within a texture region of an allocation.
struct TextureLocation {
    x: u8,
    y: u8,
}

impl TextureLocation {
    fn new(x: u32, y: u32) -> TextureLocation {
        debug_assert!(x < 256 && y < 256);
        TextureLocation {
            x: x as u8,
            y: y as u8,
        }
    }
}

// A region is a sub-rect of a texture array layer.
// All allocations within a region are of the same size.
struct TextureRegion {
    layer_index: i32,
    region_size: u32,
    slab_size: u32,
    free_slots: Vec<TextureLocation>,
    slots_per_axis: u32,
    total_slot_count: usize,
    origin: DeviceUintPoint,
}

impl TextureRegion {
    fn new(region_size: u32, layer_index: i32, origin: DeviceUintPoint) -> TextureRegion {
        TextureRegion {
            layer_index,
            region_size,
            slab_size: 0,
            free_slots: Vec::new(),
            slots_per_axis: 0,
            total_slot_count: 0,
            origin,
        }
    }

    // Initialize a region to be an allocator for a specific slab size.
    fn init(&mut self, slab_size: SlabSize) {
        debug_assert!(self.slab_size == 0);
        debug_assert!(self.free_slots.is_empty());

        self.slab_size = slab_size.get_size();
        self.slots_per_axis = self.region_size / self.slab_size;

        // Add each block to a freelist.
        for y in 0 .. self.slots_per_axis {
            for x in 0 .. self.slots_per_axis {
                self.free_slots.push(TextureLocation::new(x, y));
            }
        }

        self.total_slot_count = self.free_slots.len();
    }

    // Deinit a region, allowing it to become a region with
    // a different allocator size.
    fn deinit(&mut self) {
        self.slab_size = 0;
        self.free_slots.clear();
        self.slots_per_axis = 0;
        self.total_slot_count = 0;
    }

    fn is_empty(&self) -> bool {
        self.slab_size == 0
    }

    // Attempt to allocate a fixed size block from this region.
    fn alloc(&mut self) -> Option<DeviceUintPoint> {
        self.free_slots.pop().map(|location| {
            DeviceUintPoint::new(
                self.origin.x + self.slab_size * location.x as u32,
                self.origin.y + self.slab_size * location.y as u32,
            )
        })
    }

    // Free a block in this region.
    fn free(&mut self, point: DeviceUintPoint) {
        let x = (point.x - self.origin.x) / self.slab_size;
        let y = (point.y - self.origin.y) / self.slab_size;
        self.free_slots.push(TextureLocation::new(x, y));

        // If this region is completely unused, deinit it
        // so that it can become a different slab size
        // as required.
        if self.free_slots.len() == self.total_slot_count {
            self.deinit();
        }
    }
}

// A texture array contains a number of texture layers, where
// each layer contains one or more regions that can act
// as slab allocators.
struct TextureArray {
    filter: TextureFilter,
    layer_count: usize,
    format: ImageFormat,
    is_allocated: bool,
    regions: Vec<TextureRegion>,
    texture_id: Option<CacheTextureId>,
}

impl TextureArray {
    fn new(
        format: ImageFormat,
        filter: TextureFilter,
        layer_count: usize
    ) -> TextureArray {
        TextureArray {
            format,
            filter,
            layer_count,
            is_allocated: false,
            regions: Vec::new(),
            texture_id: None,
        }
    }

    fn update_profile(&self, counter: &mut ResourceProfileCounter) {
        if self.is_allocated {
            let size = self.layer_count as u32 * TEXTURE_LAYER_DIMENSIONS *
                TEXTURE_LAYER_DIMENSIONS * self.format.bytes_per_pixel();
            counter.set(self.layer_count as usize, size as usize);
        } else {
            counter.set(0, 0);
        }
    }

    // Allocate space in this texture array.
    fn alloc(
        &mut self,
        width: u32,
        height: u32,
        user_data: [f32; 3],
        frame_id: FrameId,
    ) -> Option<CacheEntry> {
        // Lazily allocate the regions if not already created.
        // This means that very rarely used image formats can be
        // added but won't allocate a cache if never used.
        if !self.is_allocated {
            debug_assert!(TEXTURE_LAYER_DIMENSIONS % TEXTURE_REGION_DIMENSIONS == 0);
            let regions_per_axis = TEXTURE_LAYER_DIMENSIONS / TEXTURE_REGION_DIMENSIONS;
            for layer_index in 0 .. self.layer_count {
                for y in 0 .. regions_per_axis {
                    for x in 0 .. regions_per_axis {
                        let origin = DeviceUintPoint::new(
                            x * TEXTURE_REGION_DIMENSIONS,
                            y * TEXTURE_REGION_DIMENSIONS,
                        );
                        let region = TextureRegion::new(
                            TEXTURE_REGION_DIMENSIONS,
                            layer_index as i32,
                            origin
                        );
                        self.regions.push(region);
                    }
                }
            }
            self.is_allocated = true;
        }

        // Quantize the size of the allocation to select a region to
        // allocate from.
        let slab_size = SlabSize::new(width, height);
        let slab_size_dim = slab_size.get_size();

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
            if region.slab_size == 0 {
                empty_region_index = Some(i);
            } else if region.slab_size == slab_size_dim {
                if let Some(location) = region.alloc() {
                    entry_kind = Some(EntryKind::Cache {
                        layer_index: region.layer_index as u16,
                        region_index: i as u16,
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
                        layer_index: region.layer_index as u16,
                        region_index: empty_region_index as u16,
                        origin: location,
                    }
                });
            }
        }

        entry_kind.map(|kind| {
            CacheEntry {
                size: DeviceUintSize::new(width, height),
                user_data,
                last_access: frame_id,
                kind,
                uv_rect_handle: GpuCacheHandle::new(),
                format: self.format,
                filter: self.filter,
                texture_id: self.texture_id.unwrap(),
            }
        })
    }
}

impl TextureUpdate {
    // Constructs a TextureUpdate operation to be passed to the
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
    ) -> TextureUpdate {
        let data_src = match data {
            ImageData::Blob(..) => {
                panic!("The vector image should have been rasterized.");
            }
            ImageData::External(ext_image) => match ext_image.image_type {
                ExternalImageType::Texture2DHandle |
                ExternalImageType::Texture2DArrayHandle |
                ExternalImageType::TextureRectHandle |
                ExternalImageType::TextureExternalHandle => {
                    panic!("External texture handle should not go through texture_cache.");
                }
                ExternalImageType::ExternalBuffer => TextureUpdateSource::External {
                    id: ext_image.id,
                    channel_index: ext_image.channel_index,
                },
            },
            ImageData::Raw(bytes) => {
                let finish = descriptor.offset +
                    descriptor.width * descriptor.format.bytes_per_pixel() +
                    (descriptor.height - 1) * descriptor.compute_stride();
                assert!(bytes.len() >= finish as usize);

                TextureUpdateSource::Bytes { data: bytes }
            }
        };

        let update_op = match dirty_rect {
            Some(dirty) => {
                let stride = descriptor.compute_stride();
                let offset = descriptor.offset + dirty.origin.y * stride + dirty.origin.x * descriptor.format.bytes_per_pixel();
                let origin =
                    DeviceUintPoint::new(origin.x + dirty.origin.x, origin.y + dirty.origin.y);
                TextureUpdateOp::Update {
                    rect: DeviceUintRect::new(origin, dirty.size),
                    source: data_src,
                    stride: Some(stride),
                    offset,
                    layer_index,
                }
            }
            None => TextureUpdateOp::Update {
                rect: DeviceUintRect::new(origin, size),
                source: data_src,
                stride: descriptor.stride,
                offset: descriptor.offset,
                layer_index,
            },
        };

        TextureUpdate {
            id: texture_id,
            op: update_op,
        }
    }
}
