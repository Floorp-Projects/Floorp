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
        let FreeListItemId(value) = *self;
        value
    }
}

pub trait FreeListItem {
    fn next_free_id(&self) -> Option<FreeListItemId>;
    fn set_next_free_id(&mut self, id: Option<FreeListItemId>);
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

    #[allow(dead_code)]
    fn assert_not_in_free_list(&self, id: FreeListItemId) {
        let FreeListItemId(id) = id;
        let mut next_free_id = self.first_free_index;

        while let Some(free_id) = next_free_id {
            let FreeListItemId(index) = free_id;
            assert!(index != id);
            let free_item = &self.items[index as usize];
            next_free_id = free_item.next_free_id();
        }
    }

    pub fn get(&self, id: FreeListItemId) -> &T {
        //self.assert_not_in_free_list(id);

        let FreeListItemId(index) = id;
        &self.items[index as usize]
    }

    pub fn get_mut(&mut self, id: FreeListItemId) -> &mut T {
        //self.assert_not_in_free_list(id);

        let FreeListItemId(index) = id;
        &mut self.items[index as usize]
    }

    #[allow(dead_code)]
    pub fn len(&self) -> usize {
        self.alloc_count
    }

    // TODO(gw): Actually free items from the texture cache!!
    #[allow(dead_code)]
    pub fn free(&mut self, id: FreeListItemId) {
        self.alloc_count -= 1;
        let FreeListItemId(index) = id;
        let item = &mut self.items[index as usize];
        item.set_next_free_id(self.first_free_index);
        self.first_free_index = Some(id);
    }

    pub fn for_each_item<F>(&mut self, f: F) where F: Fn(&mut T) {
        let mut free_ids = HashSet::new();

        let mut next_free_id = self.first_free_index;
        while let Some(free_id) = next_free_id {
            free_ids.insert(free_id);
            let FreeListItemId(index) = free_id;
            let free_item = &self.items[index as usize];
            next_free_id = free_item.next_free_id();
        }

        for (index, mut item) in self.items.iter_mut().enumerate() {
            let id = FreeListItemId(index as u32);
            if !free_ids.contains(&id) {
                f(&mut item);
            }
        }
    }
}
