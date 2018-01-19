/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{DeviceUintPoint, DeviceUintRect, DeviceUintSize};
use std::slice::Iter;
use util;

/// The minimum number of pixels on each side that we require for rects to be classified as
/// "medium" within the free list.
const MINIMUM_MEDIUM_RECT_SIZE: u32 = 16;

/// The minimum number of pixels on each side that we require for rects to be classified as
/// "large" within the free list.
const MINIMUM_LARGE_RECT_SIZE: u32 = 32;

/// A texture allocator using the guillotine algorithm with the rectangle merge improvement. See
/// sections 2.2 and 2.2.5 in "A Thousand Ways to Pack the Bin - A Practical Approach to Two-
/// Dimensional Rectangle Bin Packing":
///
///    http://clb.demon.fi/files/RectangleBinPack.pdf
///
/// This approach was chosen because of its simplicity, good performance, and easy support for
/// dynamic texture deallocation.
#[cfg_attr(feature = "capture", derive(Deserialize, Serialize))]
pub struct GuillotineAllocator {
    texture_size: DeviceUintSize,
    free_list: FreeRectList,
    allocations: u32,
    dirty: bool,
}

impl GuillotineAllocator {
    pub fn new(texture_size: DeviceUintSize) -> GuillotineAllocator {
        let mut page = GuillotineAllocator {
            texture_size,
            free_list: FreeRectList::new(),
            allocations: 0,
            dirty: false,
        };
        page.clear();
        page
    }

    fn find_index_of_best_rect_in_bin(
        &self,
        bin: FreeListBin,
        requested_dimensions: &DeviceUintSize,
    ) -> Option<FreeListIndex> {
        let mut smallest_index_and_area = None;
        for (candidate_index, candidate_rect) in self.free_list.iter(bin).enumerate() {
            if !requested_dimensions.fits_inside(&candidate_rect.size) {
                continue;
            }

            let candidate_area = candidate_rect.size.width * candidate_rect.size.height;
            smallest_index_and_area = Some((candidate_index, candidate_area));
            break;
        }

        smallest_index_and_area.map(|(index, _)| FreeListIndex(bin, index))
    }

    /// Find a suitable rect in the free list. We choose the smallest such rect
    /// in terms of area (Best-Area-Fit, BAF).
    fn find_index_of_best_rect(
        &self,
        requested_dimensions: &DeviceUintSize,
    ) -> Option<FreeListIndex> {
        let bin = FreeListBin::for_size(requested_dimensions);
        for &target_bin in &[FreeListBin::Small, FreeListBin::Medium, FreeListBin::Large] {
            if bin <= target_bin {
                if let Some(index) =
                    self.find_index_of_best_rect_in_bin(target_bin, requested_dimensions)
                {
                    return Some(index);
                }
            }
        }
        None
    }

    pub fn allocate(&mut self, requested_dimensions: &DeviceUintSize) -> Option<DeviceUintPoint> {
        if requested_dimensions.width == 0 || requested_dimensions.height == 0 {
            return Some(DeviceUintPoint::new(0, 0));
        }
        let index = match self.find_index_of_best_rect(requested_dimensions) {
            None => return None,
            Some(index) => index,
        };

        // Remove the rect from the free list and decide how to guillotine it. We choose the split
        // that results in the single largest area (Min Area Split Rule, MINAS).
        let chosen_rect = self.free_list.remove(index);
        let candidate_free_rect_to_right = DeviceUintRect::new(
            DeviceUintPoint::new(
                chosen_rect.origin.x + requested_dimensions.width,
                chosen_rect.origin.y,
            ),
            DeviceUintSize::new(
                chosen_rect.size.width - requested_dimensions.width,
                requested_dimensions.height,
            ),
        );
        let candidate_free_rect_to_bottom = DeviceUintRect::new(
            DeviceUintPoint::new(
                chosen_rect.origin.x,
                chosen_rect.origin.y + requested_dimensions.height,
            ),
            DeviceUintSize::new(
                requested_dimensions.width,
                chosen_rect.size.height - requested_dimensions.height,
            ),
        );
        let candidate_free_rect_to_right_area =
            candidate_free_rect_to_right.size.width * candidate_free_rect_to_right.size.height;
        let candidate_free_rect_to_bottom_area =
            candidate_free_rect_to_bottom.size.width * candidate_free_rect_to_bottom.size.height;

        // Guillotine the rectangle.
        let new_free_rect_to_right;
        let new_free_rect_to_bottom;
        if candidate_free_rect_to_right_area > candidate_free_rect_to_bottom_area {
            new_free_rect_to_right = DeviceUintRect::new(
                candidate_free_rect_to_right.origin,
                DeviceUintSize::new(
                    candidate_free_rect_to_right.size.width,
                    chosen_rect.size.height,
                ),
            );
            new_free_rect_to_bottom = candidate_free_rect_to_bottom
        } else {
            new_free_rect_to_right = candidate_free_rect_to_right;
            new_free_rect_to_bottom = DeviceUintRect::new(
                candidate_free_rect_to_bottom.origin,
                DeviceUintSize::new(
                    chosen_rect.size.width,
                    candidate_free_rect_to_bottom.size.height,
                ),
            )
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

    fn clear(&mut self) {
        self.free_list = FreeRectList::new();
        self.free_list.push(&DeviceUintRect::new(
            DeviceUintPoint::zero(),
            self.texture_size,
        ));
        self.allocations = 0;
        self.dirty = false;
    }
}

/// A binning free list. Binning is important to avoid sifting through lots of small strips when
/// allocating many texture items.
#[cfg_attr(feature = "capture", derive(Deserialize, Serialize))]
struct FreeRectList {
    small: Vec<DeviceUintRect>,
    medium: Vec<DeviceUintRect>,
    large: Vec<DeviceUintRect>,
}

impl FreeRectList {
    fn new() -> Self {
        FreeRectList {
            small: vec![],
            medium: vec![],
            large: vec![],
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
        } else if size.width >= MINIMUM_MEDIUM_RECT_SIZE && size.height >= MINIMUM_MEDIUM_RECT_SIZE
        {
            FreeListBin::Medium
        } else {
            debug_assert!(size.width > 0 && size.height > 0);
            FreeListBin::Small
        }
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
