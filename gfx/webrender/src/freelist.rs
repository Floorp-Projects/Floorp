/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::collections::HashSet;

#[derive(Debug, Copy, Clone, PartialEq, Eq, Hash)]
pub struct FreeListItemId(u32);

impl FreeListItemId {
    #[inline]
    pub fn new(value: u32) -> FreeListItemId {
        FreeListItemId(value)
    }

    #[inline]
    pub fn value(&self) -> u32 {
        self.0
    }
}

pub trait FreeListItem {
    fn take(&mut self) -> Self;
    fn next_free_id(&self) -> Option<FreeListItemId>;
    fn set_next_free_id(&mut self, id: Option<FreeListItemId>);
}

struct FreeListIter<'a, T: 'a> {
    items: &'a [T],
    cur_index: Option<FreeListItemId>,
}

impl<'a, T: FreeListItem> Iterator for FreeListIter<'a, T> {
    type Item = FreeListItemId;
    fn next(&mut self) -> Option<Self::Item> {
        self.cur_index.map(|free_id| {
            self.cur_index = self.items[free_id.0 as usize].next_free_id();
            free_id
        })
    }
}

pub struct FreeList<T> {
    items: Vec<T>,
    first_free_index: Option<FreeListItemId>,
    alloc_count: usize,
}

impl<T: FreeListItem> FreeList<T> {
    pub fn new() -> FreeList<T> {
        FreeList {
            items: Vec::new(),
            first_free_index: None,
            alloc_count: 0,
        }
    }

    fn free_iter(&self) -> FreeListIter<T> {
        FreeListIter {
            items: &self.items,
            cur_index: self.first_free_index,
        }
    }

    pub fn insert(&mut self, item: T) -> FreeListItemId {
        self.alloc_count += 1;
        match self.first_free_index {
            Some(free_index) => {
                let FreeListItemId(index) = free_index;
                let free_item = &mut self.items[index as usize];
                self.first_free_index = free_item.next_free_id();
                *free_item = item;
                free_index
            }
            None => {
                let item_id = FreeListItemId(self.items.len() as u32);
                self.items.push(item);
                item_id
            }
        }
    }

    pub fn get(&self, id: FreeListItemId) -> &T {
        debug_assert_eq!(self.free_iter().find(|&fid| fid==id), None);
        &self.items[id.0 as usize]
    }

    pub fn get_mut(&mut self, id: FreeListItemId) -> &mut T {
        debug_assert_eq!(self.free_iter().find(|&fid| fid==id), None);
        &mut self.items[id.0 as usize]
    }

    #[allow(dead_code)]
    pub fn len(&self) -> usize {
        self.alloc_count
    }

    pub fn free(&mut self, id: FreeListItemId) -> T {
        self.alloc_count -= 1;
        let FreeListItemId(index) = id;
        let item = &mut self.items[index as usize];
        let data = item.take();
        item.set_next_free_id(self.first_free_index);
        self.first_free_index = Some(id);
        data
    }

    pub fn for_each_item<F>(&mut self, f: F) where F: Fn(&mut T) {
        //TODO: this could be done much faster. Instead of gathering the free
        // indices into a set, we could re-order the free list to be ascending.
        // That is an one-time operation with at most O(nf^2), where
        //    nf = number of elements in the free list
        // Then this code would just walk both `items` and the ascending free
        // list, essentially skipping the free indices for free.
        let free_ids: HashSet<_> = self.free_iter().collect();

        for (index, mut item) in self.items.iter_mut().enumerate() {
            if !free_ids.contains(&FreeListItemId(index as u32)) {
                f(&mut item);
            }
        }
    }
}
