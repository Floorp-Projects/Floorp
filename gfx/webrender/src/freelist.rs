/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::marker::PhantomData;
use util::recycle_vec;

// TODO(gw): Add an occupied list head, for fast
//           iteration of the occupied list to implement
//           retain() style functionality.

#[derive(Debug, Copy, Clone, PartialEq)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
struct Epoch(u32);

#[derive(Debug)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct FreeListHandle<T> {
    index: u32,
    epoch: Epoch,
    _marker: PhantomData<T>,
}

impl<T> FreeListHandle<T> {
    pub fn weak(&self) -> WeakFreeListHandle<T> {
        WeakFreeListHandle {
            index: self.index,
            epoch: self.epoch,
            _marker: PhantomData,
        }
    }
}

impl<T> Clone for WeakFreeListHandle<T> {
    fn clone(&self) -> Self {
        WeakFreeListHandle {
            index: self.index,
            epoch: self.epoch,
            _marker: PhantomData,
        }
    }
}

#[derive(Debug)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct WeakFreeListHandle<T> {
    index: u32,
    epoch: Epoch,
    _marker: PhantomData<T>,
}

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
struct Slot<T> {
    next: Option<u32>,
    epoch: Epoch,
    value: Option<T>,
}

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct FreeList<T> {
    slots: Vec<Slot<T>>,
    free_list_head: Option<u32>,
    active_count: usize,
}

pub enum UpsertResult<T> {
    Updated(T),
    Inserted(FreeListHandle<T>),
}

impl<T> FreeList<T> {
    pub fn new() -> Self {
        FreeList {
            slots: Vec::new(),
            free_list_head: None,
            active_count: 0,
        }
    }

    pub fn recycle(self) -> FreeList<T> {
        FreeList {
            slots: recycle_vec(self.slots),
            free_list_head: None,
            active_count: 0,
        }
    }

    pub fn clear(&mut self) {
        self.slots.clear();
        self.free_list_head = None;
        self.active_count = 0;
    }

    #[allow(dead_code)]
    pub fn get(&self, id: &FreeListHandle<T>) -> &T {
        self.slots[id.index as usize].value.as_ref().unwrap()
    }

    #[allow(dead_code)]
    pub fn get_mut(&mut self, id: &FreeListHandle<T>) -> &mut T {
        self.slots[id.index as usize].value.as_mut().unwrap()
    }

    pub fn get_opt(&self, id: &WeakFreeListHandle<T>) -> Option<&T> {
        let slot = &self.slots[id.index as usize];
        if slot.epoch == id.epoch {
            slot.value.as_ref()
        } else {
            None
        }
    }

    pub fn get_opt_mut(&mut self, id: &WeakFreeListHandle<T>) -> Option<&mut T> {
        let slot = &mut self.slots[id.index as usize];
        if slot.epoch == id.epoch {
            slot.value.as_mut()
        } else {
            None
        }
    }

    // Perform a database style UPSERT operation. If the provided
    // handle is a valid entry, update the value and return the
    // previous data. If the provided handle is invalid, then
    // insert the data into a new slot and return the new handle.
    pub fn upsert(&mut self, id: &WeakFreeListHandle<T>, data: T) -> UpsertResult<T> {
        if self.slots[id.index as usize].epoch == id.epoch {
            let slot = &mut self.slots[id.index as usize];
            let result = UpsertResult::Updated(slot.value.take().unwrap());
            slot.value = Some(data);
            result
        } else {
            UpsertResult::Inserted(self.insert(data))
        }
    }

    pub fn insert(&mut self, item: T) -> FreeListHandle<T> {
        self.active_count += 1;

        match self.free_list_head {
            Some(free_index) => {
                let slot = &mut self.slots[free_index as usize];

                // Remove from free list.
                self.free_list_head = slot.next;
                slot.next = None;
                slot.value = Some(item);

                FreeListHandle {
                    index: free_index,
                    epoch: slot.epoch,
                    _marker: PhantomData,
                }
            }
            None => {
                let index = self.slots.len() as u32;
                let epoch = Epoch(0);

                self.slots.push(Slot {
                    next: None,
                    epoch,
                    value: Some(item),
                });

                FreeListHandle {
                    index,
                    epoch,
                    _marker: PhantomData,
                }
            }
        }
    }

    pub fn free(&mut self, id: FreeListHandle<T>) -> T {
        self.active_count -= 1;
        let slot = &mut self.slots[id.index as usize];
        slot.next = self.free_list_head;
        slot.epoch = Epoch(slot.epoch.0 + 1);
        self.free_list_head = Some(id.index);
        slot.value.take().unwrap()
    }

    pub fn len(&self) -> usize {
        self.active_count
    }
}
