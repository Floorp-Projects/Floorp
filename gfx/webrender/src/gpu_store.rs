/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use device::TextureFilter;
use std::marker::PhantomData;
use std::mem;
use std::ops::Add;
use util::recycle_vec;
use webrender_traits::ImageFormat;

#[derive(Debug, Copy, Clone, Eq, Hash, PartialEq)]
pub struct GpuStoreAddress(pub i32);


impl Add<i32> for GpuStoreAddress {
    type Output = GpuStoreAddress;

    fn add(self, other: i32) -> GpuStoreAddress {
        GpuStoreAddress(self.0 + other)
    }
}

impl Add<usize> for GpuStoreAddress {
    type Output = GpuStoreAddress;

    fn add(self, other: usize) -> GpuStoreAddress {
        GpuStoreAddress(self.0 + other as i32)
    }
}

pub trait GpuStoreLayout {
    fn image_format() -> ImageFormat;

    fn texture_width<T>() -> usize;

    fn texture_filter() -> TextureFilter;

    fn texel_size() -> usize {
        match Self::image_format() {
            ImageFormat::BGRA8 => 4,
            ImageFormat::RGBAF32 => 16,
            _ => unreachable!(),
        }
    }

    fn texels_per_item<T>() -> usize {
        let item_size = mem::size_of::<T>();
        let texel_size = Self::texel_size();
        debug_assert!(item_size % texel_size == 0);
        item_size / texel_size
    }

    fn items_per_row<T>() -> usize {
        Self::texture_width::<T>() / Self::texels_per_item::<T>()
    }

    fn rows_per_item<T>() -> usize {
        Self::texels_per_item::<T>() / Self::texture_width::<T>()
    }
}

/// A CPU-side buffer storing content to be uploaded to the GPU.
pub struct GpuStore<T, L> {
    data: Vec<T>,
    layout: PhantomData<L>,
    // TODO(gw): Could store this intrusively inside
    // the data array free slots.
    //free_list: Vec<GpuStoreAddress>,
}

impl<T: Clone + Default, L: GpuStoreLayout> GpuStore<T, L> {
    pub fn new() -> GpuStore<T, L> {
        GpuStore {
            data: Vec::new(),
            layout: PhantomData,
            //free_list: Vec::new(),
        }
    }

    pub fn recycle(self) -> Self {
        GpuStore {
            data: recycle_vec(self.data),
            layout: PhantomData,
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
        let items_per_row = L::items_per_row::<T>();

        let mut items = self.data.clone();

        // Extend the data array to be a multiple of the row size.
        // This ensures memory safety when the array is passed to
        // OpenGL to upload to the GPU.
        if items_per_row != 0 {
            while items_per_row != 0 && items.len() % items_per_row != 0 {
                items.push(T::default());
            }
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

    pub fn clear(&mut self) {
        self.data.clear()
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
