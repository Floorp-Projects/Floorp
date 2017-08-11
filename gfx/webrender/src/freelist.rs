/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::marker::PhantomData;
use std::mem;

// TODO(gw): Add a weak free list handle. This is like a strong
//           free list handle below, but will contain an epoch
//           field. Weak handles will use a get_opt style API
//           which returns an Option<T> instead of T.

// TODO(gw): Add an occupied list head, for fast
//           iteration of the occupied list to implement
//           retain() style functionality.

#[derive(Debug)]
pub struct FreeListHandle<T> {
    index: u32,
    _marker: PhantomData<T>,
}

enum SlotValue<T> {
    Free,
    Occupied(T),
}

impl<T> SlotValue<T> {
    fn take(&mut self) -> T {
        match mem::replace(self, SlotValue::Free) {
            SlotValue::Free => unreachable!(),
            SlotValue::Occupied(data) => data,
        }
    }
}

struct Slot<T> {
    next: Option<u32>,
    value: SlotValue<T>,
}

pub struct FreeList<T> {
    slots: Vec<Slot<T>>,
    free_list_head: Option<u32>,
}

impl<T> FreeList<T> {
    pub fn new() -> FreeList<T> {
        FreeList {
            slots: Vec::new(),
            free_list_head: None,
        }
    }

    pub fn get(&self, id: &FreeListHandle<T>) -> &T {
        match self.slots[id.index as usize].value {
            SlotValue::Free => unreachable!(),
            SlotValue::Occupied(ref data) => data,
        }
    }

    pub fn get_mut(&mut self, id: &FreeListHandle<T>) -> &mut T {
        match self.slots[id.index as usize].value {
            SlotValue::Free => unreachable!(),
            SlotValue::Occupied(ref mut data) => data,
        }
    }

    pub fn insert(&mut self, item: T) -> FreeListHandle<T> {
        match self.free_list_head {
            Some(free_index) => {
                let slot = &mut self.slots[free_index as usize];

                // Remove from free list.
                self.free_list_head = slot.next;
                slot.next = None;
                slot.value = SlotValue::Occupied(item);

                FreeListHandle {
                    index: free_index,
                    _marker: PhantomData,
                }
            }
            None => {
                let index = self.slots.len() as u32;

                self.slots.push(Slot {
                    next: None,
                    value: SlotValue::Occupied(item),
                });

                FreeListHandle {
                    index,
                    _marker: PhantomData,
                }
            }
        }
    }

    pub fn free(&mut self, id: FreeListHandle<T>) -> T {
        let slot = &mut self.slots[id.index as usize];
        slot.next = self.free_list_head;
        self.free_list_head = Some(id.index);
        slot.value.take()
    }
}
