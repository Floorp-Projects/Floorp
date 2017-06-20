/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use device::TextureFilter;
use fnv::FnvHasher;
use freelist::{FreeList, FreeListItem, FreeListItemId};
use gpu_cache::GpuCacheHandle;
use internal_types::{TextureUpdate, TextureUpdateOp, UvRect};
use internal_types::{CacheTextureId, RenderTargetMode, TextureUpdateList};
use profiler::TextureCacheProfileCounters;
use std::cmp;
use std::collections::HashMap;
use std::collections::hash_map::Entry;
use std::hash::BuildHasherDefault;
use std::mem;
use std::slice::Iter;
use time;
use util;
use webrender_traits::{ExternalImageType, ImageData, ImageFormat};
use webrender_traits::{DeviceUintRect, DeviceUintSize, DeviceUintPoint};
use webrender_traits::{DevicePoint, ImageDescriptor};

/// The number of bytes we're allowed to use for a texture.
const MAX_BYTES_PER_TEXTURE: u32 = 1024 * 1024 * 256;  // 256MB

/// The number of RGBA pixels we're allowed to use for a texture.
const MAX_RGBA_PIXELS_PER_TEXTURE: u32 = MAX_BYTES_PER_TEXTURE / 4;

/// The desired initial size of each texture, in pixels.
const INITIAL_TEXTURE_SIZE: u32 = 1024;

/// The square root of the number of RGBA pixels we're allowed to use for a texture, rounded down.
/// to the next power of two.
const SQRT_MAX_RGBA_PIXELS_PER_TEXTURE: u32 = 8192;

/// The minimum number of pixels on each side that we require for rects to be classified as
/// "medium" within the free list.
const MINIMUM_MEDIUM_RECT_SIZE: u32 = 16;

/// The minimum number of pixels on each side that we require for rects to be classified as
/// "large" within the free list.
const MINIMUM_LARGE_RECT_SIZE: u32 = 32;

/// The amount of time in milliseconds we give ourselves to coalesce rects before giving up.
const COALESCING_TIMEOUT: u64 = 100;

/// The number of items that we process in the coalescing work list before checking whether we hit
/// the timeout.
const COALESCING_TIMEOUT_CHECKING_INTERVAL: usize = 256;

pub type TextureCacheItemId = FreeListItemId;

enum CoalescingStatus {
    Changed,
    Unchanged,
    Timeout,
}

/// A texture allocator using the guillotine algorithm with the rectangle merge improvement. See
/// sections 2.2 and 2.2.5 in "A Thousand Ways to Pack the Bin - A Practical Approach to Two-
/// Dimensional Rectangle Bin Packing":
///
///    http://clb.demon.fi/files/RectangleBinPack.pdf
///
/// This approach was chosen because of its simplicity, good performance, and easy support for
/// dynamic texture deallocation.
pub struct TexturePage {
    texture_id: CacheTextureId,
    texture_size: DeviceUintSize,
    free_list: FreeRectList,
    coalesce_vec: Vec<DeviceUintRect>,
    allocations: u32,
    dirty: bool,
}

impl TexturePage {
    pub fn new(texture_id: CacheTextureId, texture_size: DeviceUintSize) -> TexturePage {
        let mut page = TexturePage {
            texture_id: texture_id,
            texture_size: texture_size,
            free_list: FreeRectList::new(),
            coalesce_vec: Vec::new(),
            allocations: 0,
            dirty: false,
        };
        page.clear();
        page
    }

    fn find_index_of_best_rect_in_bin(&self, bin: FreeListBin, requested_dimensions: &DeviceUintSize)
                                      -> Option<FreeListIndex> {
        let mut smallest_index_and_area = None;
        for (candidate_index, candidate_rect) in self.free_list.iter(bin).enumerate() {
            if !requested_dimensions.fits_inside(&candidate_rect.size) {
                continue
            }

            let candidate_area = candidate_rect.size.width * candidate_rect.size.height;
            smallest_index_and_area = Some((candidate_index, candidate_area));
            break
        }

        smallest_index_and_area.map(|(index, _)| FreeListIndex(bin, index))
    }

    /// Find a suitable rect in the free list. We choose the smallest such rect
    /// in terms of area (Best-Area-Fit, BAF).
    fn find_index_of_best_rect(&self, requested_dimensions: &DeviceUintSize)
                               -> Option<FreeListIndex> {
        let bin = FreeListBin::for_size(requested_dimensions);
        for &target_bin in &[FreeListBin::Small, FreeListBin::Medium, FreeListBin::Large] {
            if bin <= target_bin {
                if let Some(index) = self.find_index_of_best_rect_in_bin(target_bin,
                                                                         requested_dimensions) {
                    return Some(index);
                }
            }
        }
        None
    }

    pub fn can_allocate(&self, requested_dimensions: &DeviceUintSize) -> bool {
        self.find_index_of_best_rect(requested_dimensions).is_some()
    }

    pub fn allocate(&mut self,
                    requested_dimensions: &DeviceUintSize) -> Option<DeviceUintPoint> {
        if requested_dimensions.width == 0 || requested_dimensions.height == 0 {
            return Some(DeviceUintPoint::new(0, 0))
        }
        let index = match self.find_index_of_best_rect(requested_dimensions) {
            None => return None,
            Some(index) => index,
        };

        // Remove the rect from the free list and decide how to guillotine it. We choose the split
        // that results in the single largest area (Min Area Split Rule, MINAS).
        let chosen_rect = self.free_list.remove(index);
        let candidate_free_rect_to_right =
            DeviceUintRect::new(
                DeviceUintPoint::new(chosen_rect.origin.x + requested_dimensions.width, chosen_rect.origin.y),
                DeviceUintSize::new(chosen_rect.size.width - requested_dimensions.width, requested_dimensions.height));
        let candidate_free_rect_to_bottom =
            DeviceUintRect::new(
                DeviceUintPoint::new(chosen_rect.origin.x, chosen_rect.origin.y + requested_dimensions.height),
                DeviceUintSize::new(requested_dimensions.width, chosen_rect.size.height - requested_dimensions.height));
        let candidate_free_rect_to_right_area = candidate_free_rect_to_right.size.width *
            candidate_free_rect_to_right.size.height;
        let candidate_free_rect_to_bottom_area = candidate_free_rect_to_bottom.size.width *
            candidate_free_rect_to_bottom.size.height;

        // Guillotine the rectangle.
        let new_free_rect_to_right;
        let new_free_rect_to_bottom;
        if candidate_free_rect_to_right_area > candidate_free_rect_to_bottom_area {
            new_free_rect_to_right = DeviceUintRect::new(
                candidate_free_rect_to_right.origin,
                DeviceUintSize::new(candidate_free_rect_to_right.size.width,
                                    chosen_rect.size.height));
            new_free_rect_to_bottom = candidate_free_rect_to_bottom
        } else {
            new_free_rect_to_right = candidate_free_rect_to_right;
            new_free_rect_to_bottom =
                DeviceUintRect::new(candidate_free_rect_to_bottom.origin,
                          DeviceUintSize::new(chosen_rect.size.width,
                                              candidate_free_rect_to_bottom.size.height))
        }

        // Add the guillotined rects back to the free list. If any changes were made, we're now
        // dirty since coalescing might be able to defragment.
        if !util::rect_is_empty(&new_free_rect_to_right) {
            self.free_list.push(&new_free_rect_to_right);
            self.dirty = true
        }
        if !util::rect_is_empty(&new_free_rect_to_bottom) {
            self.free_list.push(&new_free_rect_to_bottom);
            self.dirty = true
        }

        // Bump the allocation counter.
        self.allocations += 1;

        // Return the result.
        Some(chosen_rect.origin)
    }

    fn coalesce_impl<F, U>(rects: &mut [DeviceUintRect], deadline: u64, fun_key: F, fun_union: U)
                    -> CoalescingStatus where
        F: Fn(&DeviceUintRect) -> (u32, u32),
        U: Fn(&mut DeviceUintRect, &mut DeviceUintRect) -> usize,
    {
        let mut num_changed = 0;
        rects.sort_by_key(&fun_key);

        for work_index in 0..rects.len() {
            if work_index % COALESCING_TIMEOUT_CHECKING_INTERVAL == 0 &&
                    time::precise_time_ns() >= deadline {
                return CoalescingStatus::Timeout
            }

            let (left, candidates) = rects.split_at_mut(work_index + 1);
            let mut item = left.last_mut().unwrap();
            if util::rect_is_empty(item) {
                continue
            }

            let key = fun_key(item);
            for candidate in candidates.iter_mut()
                                       .take_while(|r| key == fun_key(r)) {
                num_changed += fun_union(item, candidate);
            }
        }

        if num_changed > 0 {
            CoalescingStatus::Changed
        } else {
            CoalescingStatus::Unchanged
        }
    }

    /// Combine rects that have the same width and are adjacent.
    fn coalesce_horisontal(rects: &mut [DeviceUintRect], deadline: u64) -> CoalescingStatus {
        Self::coalesce_impl(rects, deadline,
                            |item| (item.size.width, item.origin.x),
                            |item, candidate| {
            if item.origin.y == candidate.max_y() || item.max_y() == candidate.origin.y {
                *item = item.union(candidate);
                candidate.size.width = 0;
                1
            } else { 0 }
        })
    }

    /// Combine rects that have the same height and are adjacent.
    fn coalesce_vertical(rects: &mut [DeviceUintRect], deadline: u64) -> CoalescingStatus {
        Self::coalesce_impl(rects, deadline,
                            |item| (item.size.height, item.origin.y),
                            |item, candidate| {
            if item.origin.x == candidate.max_x() || item.max_x() == candidate.origin.x {
                *item = item.union(candidate);
                candidate.size.height = 0;
                1
            } else { 0 }
        })
    }

    pub fn coalesce(&mut self) -> bool {
        if !self.dirty {
            return false
        }

        // Iterate to a fixed point or until a timeout is reached.
        let deadline = time::precise_time_ns() + COALESCING_TIMEOUT;
        self.free_list.copy_to_vec(&mut self.coalesce_vec);
        let mut changed = false;

        //Note: we might want to consider try to use the last sorted order first
        // but the elements get shuffled around a bit anyway during the bin placement

        match Self::coalesce_horisontal(&mut self.coalesce_vec, deadline) {
            CoalescingStatus::Changed => changed = true,
            CoalescingStatus::Unchanged => (),
            CoalescingStatus::Timeout => {
                self.free_list.init_from_slice(&self.coalesce_vec);
                return true
            }
        }

        match Self::coalesce_vertical(&mut self.coalesce_vec, deadline) {
            CoalescingStatus::Changed => changed = true,
            CoalescingStatus::Unchanged => (),
            CoalescingStatus::Timeout => {
                self.free_list.init_from_slice(&self.coalesce_vec);
                return true
            }
        }

        if changed {
            self.free_list.init_from_slice(&self.coalesce_vec);
        }
        self.dirty = changed;
        changed
    }

    pub fn clear(&mut self) {
        self.free_list = FreeRectList::new();
        self.free_list.push(&DeviceUintRect::new(
            DeviceUintPoint::zero(),
            self.texture_size));
        self.allocations = 0;
        self.dirty = false;
    }

    fn free(&mut self, rect: &DeviceUintRect) {
        if util::rect_is_empty(rect) {
            return
        }
        debug_assert!(self.allocations > 0);
        self.allocations -= 1;
        if self.allocations == 0 {
            self.clear();
            return
        }

        self.free_list.push(rect);
        self.dirty = true
    }

    fn grow(&mut self, new_texture_size: DeviceUintSize) {
        assert!(new_texture_size.width >= self.texture_size.width);
        assert!(new_texture_size.height >= self.texture_size.height);

        let new_rects = [
            DeviceUintRect::new(DeviceUintPoint::new(self.texture_size.width, 0),
                                DeviceUintSize::new(new_texture_size.width - self.texture_size.width,
                                                    new_texture_size.height)),

            DeviceUintRect::new(DeviceUintPoint::new(0, self.texture_size.height),
                                DeviceUintSize::new(self.texture_size.width,
                                                    new_texture_size.height - self.texture_size.height)),
        ];

        for rect in &new_rects {
            if rect.size.width > 0 && rect.size.height > 0 {
                self.free_list.push(rect);
            }
        }

        self.texture_size = new_texture_size
    }

    fn can_grow(&self, max_size: u32) -> bool {
        self.texture_size.width < max_size || self.texture_size.height < max_size
    }
}

// testing functionality
impl TexturePage {
    #[doc(hidden)]
    pub fn new_dummy(size: DeviceUintSize) -> TexturePage {
        Self::new(CacheTextureId(0), size)
    }

    #[doc(hidden)]
    pub fn fill_from(&mut self, other: &TexturePage) {
        self.dirty = true;
        self.free_list.small.clear();
        self.free_list.small.extend_from_slice(&other.free_list.small);
        self.free_list.medium.clear();
        self.free_list.medium.extend_from_slice(&other.free_list.medium);
        self.free_list.large.clear();
        self.free_list.large.extend_from_slice(&other.free_list.large);
    }
}

/// A binning free list. Binning is important to avoid sifting through lots of small strips when
/// allocating many texture items.
struct FreeRectList {
    small: Vec<DeviceUintRect>,
    medium: Vec<DeviceUintRect>,
    large: Vec<DeviceUintRect>,
}

impl FreeRectList {
    fn new() -> FreeRectList {
        FreeRectList {
            small: vec![],
            medium: vec![],
            large: vec![],
        }
    }

    fn init_from_slice(&mut self, rects: &[DeviceUintRect]) {
        self.small.clear();
        self.medium.clear();
        self.large.clear();
        for rect in rects {
            if !util::rect_is_empty(rect) {
                self.push(rect)
            }
        }
    }

    fn push(&mut self, rect: &DeviceUintRect) {
        match FreeListBin::for_size(&rect.size) {
            FreeListBin::Small => self.small.push(*rect),
            FreeListBin::Medium => self.medium.push(*rect),
            FreeListBin::Large => self.large.push(*rect),
        }
    }

    fn remove(&mut self, index: FreeListIndex) -> DeviceUintRect {
        match index.0 {
            FreeListBin::Small => self.small.swap_remove(index.1),
            FreeListBin::Medium => self.medium.swap_remove(index.1),
            FreeListBin::Large => self.large.swap_remove(index.1),
        }
    }

    fn iter(&self, bin: FreeListBin) -> Iter<DeviceUintRect> {
        match bin {
            FreeListBin::Small => self.small.iter(),
            FreeListBin::Medium => self.medium.iter(),
            FreeListBin::Large => self.large.iter(),
        }
    }

    fn copy_to_vec(&self, rects: &mut Vec<DeviceUintRect>) {
        rects.clear();
        rects.extend_from_slice(&self.small);
        rects.extend_from_slice(&self.medium);
        rects.extend_from_slice(&self.large);
    }
}

#[derive(Debug, Clone, Copy)]
struct FreeListIndex(FreeListBin, usize);

#[derive(Debug, Clone, Copy, PartialEq, PartialOrd)]
enum FreeListBin {
    Small,
    Medium,
    Large,
}

impl FreeListBin {
    fn for_size(size: &DeviceUintSize) -> FreeListBin {
        if size.width >= MINIMUM_LARGE_RECT_SIZE && size.height >= MINIMUM_LARGE_RECT_SIZE {
            FreeListBin::Large
        } else if size.width >= MINIMUM_MEDIUM_RECT_SIZE &&
                size.height >= MINIMUM_MEDIUM_RECT_SIZE {
            FreeListBin::Medium
        } else {
            debug_assert!(size.width > 0 && size.height > 0);
            FreeListBin::Small
        }
    }
}

#[derive(Debug, Clone)]
pub struct TextureCacheItem {
    // Identifies the texture and array slice
    pub texture_id: CacheTextureId,

    // The texture coordinates for this item
    pub uv_rect: UvRect,

    // The size of the allocated rectangle.
    pub allocated_rect: DeviceUintRect,

    // Handle to the location of the UV rect for this item in GPU cache.
    pub uv_rect_handle: GpuCacheHandle,

    // Some arbitrary data associated with this item.
    // In the case of glyphs, it is the top / left offset
    // from the rasterized glyph.
    pub user_data: [f32; 2],
}

// Structure squat the width/height fields to maintain the free list information :)
impl FreeListItem for TextureCacheItem {
    fn take(&mut self) -> Self {
        let data = self.clone();
        self.texture_id = CacheTextureId(0);
        data
    }

    fn next_free_id(&self) -> Option<FreeListItemId> {
        if self.allocated_rect.size.width == 0 {
            debug_assert_eq!(self.allocated_rect.size.height, 0);
            None
        } else {
            debug_assert_eq!(self.allocated_rect.size.width, 1);
            Some(FreeListItemId::new(self.allocated_rect.size.height))
        }
    }

    fn set_next_free_id(&mut self, id: Option<FreeListItemId>) {
        match id {
            Some(id) => {
                self.allocated_rect.size.width = 1;
                self.allocated_rect.size.height = id.value();
            }
            None => {
                self.allocated_rect.size.width = 0;
                self.allocated_rect.size.height = 0;
            }
        }
    }
}

impl TextureCacheItem {
    fn new(texture_id: CacheTextureId,
           rect: DeviceUintRect,
           user_data: [f32; 2])
           -> TextureCacheItem {
        TextureCacheItem {
            texture_id: texture_id,
            uv_rect: UvRect {
                uv0: DevicePoint::new(rect.origin.x as f32,
                                      rect.origin.y as f32),
                uv1: DevicePoint::new((rect.origin.x + rect.size.width) as f32,
                                      (rect.origin.y + rect.size.height) as f32),
            },
            allocated_rect: rect,
            uv_rect_handle: GpuCacheHandle::new(),
            user_data: user_data,
        }
    }
}

struct TextureCacheArena {
    pages_a8: Vec<TexturePage>,
    pages_rgb8: Vec<TexturePage>,
    pages_rgba8: Vec<TexturePage>,
    pages_rg8: Vec<TexturePage>,
}

impl TextureCacheArena {
    fn new() -> TextureCacheArena {
        TextureCacheArena {
            pages_a8: Vec::new(),
            pages_rgb8: Vec::new(),
            pages_rgba8: Vec::new(),
            pages_rg8: Vec::new(),
        }
    }

    fn texture_page_for_id(&mut self, id: CacheTextureId) -> Option<&mut TexturePage> {
        for page in self.pages_a8.iter_mut().chain(self.pages_rgb8.iter_mut())
                                            .chain(self.pages_rgba8.iter_mut())
                                            .chain(self.pages_rg8.iter_mut()) {
            if page.texture_id == id {
                return Some(page)
            }
        }
        None
    }
}

pub struct CacheTextureIdList {
    next_id: usize,
    free_list: Vec<usize>,
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
        if self.free_list.is_empty() {
            self.free_list.push(self.next_id);
            self.next_id += 1;
        }

        let id = self.free_list.pop().unwrap();
        CacheTextureId(id)
    }

    fn free(&mut self, id: CacheTextureId) {
        self.free_list.push(id.0);
    }
}

pub struct TextureCache {
    cache_id_list: CacheTextureIdList,
    free_texture_levels: HashMap<ImageFormat, Vec<FreeTextureLevel>, BuildHasherDefault<FnvHasher>>,
    items: FreeList<TextureCacheItem>,
    arena: TextureCacheArena,
    pending_updates: TextureUpdateList,
    max_texture_size: u32,
}

#[derive(PartialEq, Eq, Debug)]
pub enum AllocationKind {
    TexturePage,
    Standalone,
}

#[derive(Debug)]
pub struct AllocationResult {
    image_id: TextureCacheItemId,
    kind: AllocationKind,
    item: TextureCacheItem,
}

impl TextureCache {
    pub fn new(mut max_texture_size: u32) -> TextureCache {
        if max_texture_size * max_texture_size > MAX_RGBA_PIXELS_PER_TEXTURE {
            max_texture_size = SQRT_MAX_RGBA_PIXELS_PER_TEXTURE;
        }

        TextureCache {
            cache_id_list: CacheTextureIdList::new(),
            free_texture_levels: HashMap::default(),
            items: FreeList::new(),
            pending_updates: TextureUpdateList::new(),
            arena: TextureCacheArena::new(),
            max_texture_size: max_texture_size,
        }
    }

    pub fn max_texture_size(&self) -> u32 {
        self.max_texture_size
    }

    pub fn pending_updates(&mut self) -> TextureUpdateList {
        mem::replace(&mut self.pending_updates, TextureUpdateList::new())
    }

    pub fn allocate(&mut self,
                    requested_width: u32,
                    requested_height: u32,
                    format: ImageFormat,
                    filter: TextureFilter,
                    user_data: [f32; 2],
                    profile: &mut TextureCacheProfileCounters)
                    -> AllocationResult {
        let requested_size = DeviceUintSize::new(requested_width, requested_height);

        // TODO(gw): For now, anything that requests nearest filtering
        //           just fails to allocate in a texture page, and gets a standalone
        //           texture. This isn't ideal, as it causes lots of batch breaks,
        //           but is probably rare enough that it can be fixed up later (it's also
        //           fairly trivial to implement, just tedious).
        if filter == TextureFilter::Nearest {
            // Fall back to standalone texture allocation.
            let texture_id = self.cache_id_list.allocate();
            let cache_item = TextureCacheItem::new(
                texture_id,
                DeviceUintRect::new(DeviceUintPoint::zero(), requested_size),
                user_data);
            let image_id = self.items.insert(cache_item);

            return AllocationResult {
                item: self.items.get(image_id).clone(),
                kind: AllocationKind::Standalone,
                image_id: image_id,
            }
        }

        let mode = RenderTargetMode::SimpleRenderTarget;
        let (page_list, page_profile) = match format {
            ImageFormat::A8 => (&mut self.arena.pages_a8, &mut profile.pages_a8),
            ImageFormat::BGRA8 => (&mut self.arena.pages_rgba8, &mut profile.pages_rgba8),
            ImageFormat::RGB8 => (&mut self.arena.pages_rgb8, &mut profile.pages_rgb8),
            ImageFormat::RG8 => (&mut self.arena.pages_rg8, &mut profile.pages_rg8),
            ImageFormat::Invalid | ImageFormat::RGBAF32 => unreachable!(),
        };

        // TODO(gw): Handle this sensibly (support failing to render items that can't fit?)
        assert!(requested_size.width <= self.max_texture_size);
        assert!(requested_size.height <= self.max_texture_size);

        let mut page_id = None; //using ID here to please the borrow checker
        for (i, page) in page_list.iter_mut().enumerate() {
            if page.can_allocate(&requested_size) {
                page_id = Some(i);
                break;
            }
            // try to coalesce it
            if page.coalesce() && page.can_allocate(&requested_size) {
                page_id = Some(i);
                break;
            }
            if page.can_grow(self.max_texture_size) {
                // try to grow it
                let new_width = cmp::min(page.texture_size.width * 2, self.max_texture_size);
                let new_height = cmp::min(page.texture_size.height * 2, self.max_texture_size);
                let texture_size = DeviceUintSize::new(new_width, new_height);
                self.pending_updates.push(TextureUpdate {
                    id: page.texture_id,
                    op: texture_grow_op(texture_size, format, mode),
                });

                let extra_texels = new_width * new_height - page.texture_size.width * page.texture_size.height;
                let extra_bytes = extra_texels * format.bytes_per_pixel().unwrap_or(0);
                page_profile.inc(extra_bytes as usize);

                page.grow(texture_size);

                if page.can_allocate(&requested_size) {
                    page_id = Some(i);
                    break;
                }
            }
        }

        let mut page = match page_id {
            Some(index) => &mut page_list[index],
            None => {
                let init_texture_size = initial_texture_size(self.max_texture_size);
                let texture_size = DeviceUintSize::new(cmp::max(requested_width, init_texture_size.width),
                                                       cmp::max(requested_height, init_texture_size.height));
                let extra_bytes = texture_size.width * texture_size.height * format.bytes_per_pixel().unwrap_or(0);
                page_profile.inc(extra_bytes as usize);

                let free_texture_levels_entry = self.free_texture_levels.entry(format);
                let mut free_texture_levels = match free_texture_levels_entry {
                    Entry::Vacant(entry) => entry.insert(Vec::new()),
                    Entry::Occupied(entry) => entry.into_mut(),
                };
                if free_texture_levels.is_empty() {
                    let texture_id = self.cache_id_list.allocate();

                    let update_op = TextureUpdate {
                        id: texture_id,
                        op: texture_create_op(texture_size, format, mode),
                    };
                    self.pending_updates.push(update_op);

                    free_texture_levels.push(FreeTextureLevel {
                        texture_id: texture_id,
                    });
                }
                let free_texture_level = free_texture_levels.pop().unwrap();
                let texture_id = free_texture_level.texture_id;

                let page = TexturePage::new(texture_id, texture_size);
                page_list.push(page);
                page_list.last_mut().unwrap()
            },
        };

        let location = page.allocate(&requested_size)
                           .expect("All the checks have passed till now, there is no way back.");
        let cache_item = TextureCacheItem::new(page.texture_id,
                                               DeviceUintRect::new(location, requested_size),
                                               user_data);
        let image_id = self.items.insert(cache_item.clone());

        AllocationResult {
            item: cache_item,
            kind: AllocationKind::TexturePage,
            image_id: image_id,
        }
    }

    pub fn update(&mut self,
                  image_id: TextureCacheItemId,
                  descriptor: ImageDescriptor,
                  data: ImageData,
                  dirty_rect: Option<DeviceUintRect>) {
        let existing_item = self.items.get(image_id);

        // TODO(gw): Handle updates to size/format!
        debug_assert_eq!(existing_item.allocated_rect.size.width, descriptor.width);
        debug_assert_eq!(existing_item.allocated_rect.size.height, descriptor.height);

        let op = match data {
            ImageData::External(..) => {
                panic!("Doesn't support Update() for external image.");
            }
            ImageData::Blob(..) => {
                panic!("The vector image should have been rasterized into a raw image.");
            }
            ImageData::Raw(bytes) => {
                match dirty_rect {
                    Some(dirty) => {
                        let stride = descriptor.compute_stride();
                        let offset = descriptor.offset + dirty.origin.y * stride + dirty.origin.x;
                        TextureUpdateOp::Update {
                            page_pos_x: existing_item.allocated_rect.origin.x + dirty.origin.x,
                            page_pos_y: existing_item.allocated_rect.origin.y + dirty.origin.y,
                            width: dirty.size.width,
                            height: dirty.size.height,
                            data: bytes,
                            stride: Some(stride),
                            offset: offset,
                        }
                    }
                    None => {
                        TextureUpdateOp::Update {
                            page_pos_x: existing_item.allocated_rect.origin.x,
                            page_pos_y: existing_item.allocated_rect.origin.y,
                            width: descriptor.width,
                            height: descriptor.height,
                            data: bytes,
                            stride: descriptor.stride,
                            offset: descriptor.offset,
                        }
                    }
                }
            }
        };

        let update_op = TextureUpdate {
            id: existing_item.texture_id,
            op: op,
        };

        self.pending_updates.push(update_op);
    }

    pub fn insert(&mut self,
                  descriptor: ImageDescriptor,
                  filter: TextureFilter,
                  data: ImageData,
                  user_data: [f32; 2],
                  profile: &mut TextureCacheProfileCounters) -> TextureCacheItemId {
        if let ImageData::Blob(..) = data {
            panic!("must rasterize the vector image before adding to the cache");
        }

        let width = descriptor.width;
        let height = descriptor.height;
        let format = descriptor.format;
        let stride = descriptor.stride;

        if let ImageData::Raw(ref vec) = data {
            let finish = descriptor.offset +
                         width * format.bytes_per_pixel().unwrap_or(0) +
                         (height-1) * descriptor.compute_stride();
            assert!(vec.len() >= finish as usize);
        }

        let result = self.allocate(width,
                                   height,
                                   format,
                                   filter,
                                   user_data,
                                   profile);

        match result.kind {
            AllocationKind::TexturePage => {
                match data {
                    ImageData::External(ext_image) => {
                        match ext_image.image_type {
                            ExternalImageType::Texture2DHandle |
                            ExternalImageType::TextureRectHandle |
                            ExternalImageType::TextureExternalHandle => {
                                panic!("External texture handle should not go through texture_cache.");
                            }
                            ExternalImageType::ExternalBuffer => {
                                let update_op = TextureUpdate {
                                    id: result.item.texture_id,
                                    op: TextureUpdateOp::UpdateForExternalBuffer {
                                        rect: result.item.allocated_rect,
                                        id: ext_image.id,
                                        channel_index: ext_image.channel_index,
                                        stride: stride,
                                        offset: descriptor.offset,
                                    },
                                };

                                self.pending_updates.push(update_op);
                            }
                        }
                    }
                    ImageData::Blob(..) => {
                        panic!("The vector image should have been rasterized.");
                    }
                    ImageData::Raw(bytes) => {
                        let update_op = TextureUpdate {
                            id: result.item.texture_id,
                            op: TextureUpdateOp::Update {
                                page_pos_x: result.item.allocated_rect.origin.x,
                                page_pos_y: result.item.allocated_rect.origin.y,
                                width: result.item.allocated_rect.size.width,
                                height: result.item.allocated_rect.size.height,
                                data: bytes,
                                stride: stride,
                                offset: descriptor.offset,
                            },
                        };

                        self.pending_updates.push(update_op);
                    }
                }
            }
            AllocationKind::Standalone => {
                match data {
                    ImageData::External(ext_image) => {
                        match ext_image.image_type {
                            ExternalImageType::Texture2DHandle |
                            ExternalImageType::TextureRectHandle |
                            ExternalImageType::TextureExternalHandle => {
                                panic!("External texture handle should not go through texture_cache.");
                            }
                            ExternalImageType::ExternalBuffer => {
                                let update_op = TextureUpdate {
                                    id: result.item.texture_id,
                                    op: TextureUpdateOp::Create {
                                        width: width,
                                        height: height,
                                        format: format,
                                        filter: filter,
                                        mode: RenderTargetMode::None,
                                        data: Some(data),
                                    },
                                };

                                self.pending_updates.push(update_op);
                            }
                        }
                    }
                    _ => {
                        let update_op = TextureUpdate {
                            id: result.item.texture_id,
                            op: TextureUpdateOp::Create {
                                width: width,
                                height: height,
                                format: format,
                                filter: filter,
                                mode: RenderTargetMode::None,
                                data: Some(data),
                            },
                        };

                        self.pending_updates.push(update_op);
                    }
                }
            }
        }

        result.image_id
    }

    pub fn get(&self, id: TextureCacheItemId) -> &TextureCacheItem {
        self.items.get(id)
    }

    pub fn get_mut(&mut self, id: TextureCacheItemId) -> &mut TextureCacheItem {
        self.items.get_mut(id)
    }

    pub fn free(&mut self, id: TextureCacheItemId) {
        let item = self.items.free(id);
        match self.arena.texture_page_for_id(item.texture_id) {
            Some(texture_page) => texture_page.free(&item.allocated_rect),
            None => {
                // This is a standalone texture allocation. Just push it back onto the free
                // list.
                self.pending_updates.push(TextureUpdate {
                    id: item.texture_id,
                    op: TextureUpdateOp::Free,
                });
                self.cache_id_list.free(item.texture_id);
            }
        }
    }
}

fn texture_create_op(texture_size: DeviceUintSize, format: ImageFormat, mode: RenderTargetMode)
                     -> TextureUpdateOp {
    TextureUpdateOp::Create {
        width: texture_size.width,
        height: texture_size.height,
        format: format,
        filter: TextureFilter::Linear,
        mode: mode,
        data: None,
    }
}

fn texture_grow_op(texture_size: DeviceUintSize,
                   format: ImageFormat,
                   mode: RenderTargetMode)
                   -> TextureUpdateOp {
    TextureUpdateOp::Grow {
        width: texture_size.width,
        height: texture_size.height,
        format: format,
        filter: TextureFilter::Linear,
        mode: mode,
    }
}

trait FitsInside {
    fn fits_inside(&self, other: &Self) -> bool;
}

impl FitsInside for DeviceUintSize {
    fn fits_inside(&self, other: &DeviceUintSize) -> bool {
        self.width <= other.width && self.height <= other.height
    }
}

/// FIXME(pcwalton): Would probably be more efficient as a bit vector.
#[derive(Clone, Copy)]
pub struct FreeTextureLevel {
    texture_id: CacheTextureId,
}

/// Returns the number of pixels on a side we start out with for our texture atlases.
fn initial_texture_size(max_texture_size: u32) -> DeviceUintSize {
    let initial_size = cmp::min(max_texture_size, INITIAL_TEXTURE_SIZE);
    DeviceUintSize::new(initial_size, initial_size)
}
