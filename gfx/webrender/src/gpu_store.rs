/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use renderer::MAX_VERTEX_TEXTURE_WIDTH;
use std::mem;

#[derive(Debug, Copy, Clone)]
pub struct GpuStoreAddress(pub i32);

/// A CPU-side buffer storing content to be uploaded to the GPU.
pub struct GpuStore<T> {
    data: Vec<T>,
    // TODO(gw): Could store this intrusively inside
    // the data array free slots.
    //free_list: Vec<GpuStoreAddress>,
}

impl<T: Clone + Default> GpuStore<T> {
    pub fn new() -> GpuStore<T> {
        GpuStore {
            data: Vec::new(),
            //free_list: Vec::new(),
        }
    }

    pub fn push<E>(&mut self, data: E) -> GpuStoreAddress where T: From<E> {
        let address = GpuStoreAddress(self.data.len() as i32);
        self.data.push(T::from(data));
        address
    }

    // TODO(gw): Change this to do incremental updates, which means
    // there is no need to copy all this data during every scroll!
    pub fn build(&self) -> Vec<T> {
        let item_size = mem::size_of::<T>();
        debug_assert!(item_size % 16 == 0);
        let vecs_per_item = item_size / 16;
        let items_per_row = MAX_VERTEX_TEXTURE_WIDTH / vecs_per_item;

        let mut items = self.data.clone();

        // Extend the data array to be a multiple of the row size.
        // This ensures memory safety when the array is passed to
        // OpenGL to upload to the GPU.
        while items.len() % items_per_row != 0 {
            items.push(T::default());
        }

        items
    }

    pub fn alloc(&mut self, count: usize) -> GpuStoreAddress {
        let address = self.get_next_address();

        for _ in 0..count {
            self.data.push(T::default());
        }

        address
    }

    pub fn get_next_address(&self) -> GpuStoreAddress {
        GpuStoreAddress(self.data.len() as i32)
    }

    pub fn get(&mut self, address: GpuStoreAddress) -> &T {
        &self.data[address.0 as usize]
    }

    pub fn get_mut(&mut self, address: GpuStoreAddress) -> &mut T {
        &mut self.data[address.0 as usize]
    }

    pub fn get_slice_mut(&mut self,
                         address: GpuStoreAddress,
                         count: usize) -> &mut [T] {
        let offset = address.0 as usize;
        &mut self.data[offset..offset + count]
    }

    // TODO(gw): Implement incremental updates of
    // GPU backed data, and support freelist for removing
    // dynamic items.

    /*
    pub fn free(&mut self, address: GpuStoreAddress) {

    }

    pub fn update(&mut self, address: GpuStoreAddress, data: T) {

    }*/
}
