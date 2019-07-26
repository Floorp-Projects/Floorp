// Copyright © 2017 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details.

use crate::errors::*;
use memmap::{Mmap, MmapMut, MmapOptions};
use std::cell::UnsafeCell;
use std::fs::{remove_file, File, OpenOptions};
use std::path::Path;
use std::sync::{atomic, Arc};

pub struct SharedMemReader {
    mmap: Mmap,
}

impl SharedMemReader {
    pub fn new(path: &Path, size: usize) -> Result<(SharedMemReader, File)> {
        let file = OpenOptions::new()
            .read(true)
            .write(true)
            .create_new(true)
            .open(path)?;
        let _ = remove_file(path);
        file.set_len(size as u64)?;
        let mmap = unsafe { MmapOptions::new().map(&file)? };
        assert_eq!(mmap.len(), size);
        Ok((SharedMemReader { mmap }, file))
    }

    pub fn read(&self, buf: &mut [u8]) -> Result<()> {
        if buf.is_empty() {
            return Ok(());
        }
        // TODO: Track how much is in the shm area.
        if buf.len() <= self.mmap.len() {
            atomic::fence(atomic::Ordering::Acquire);
            let len = buf.len();
            buf.copy_from_slice(&self.mmap[..len]);
            Ok(())
        } else {
            bail!("mmap size");
        }
    }
}

pub struct SharedMemSlice {
    mmap: Arc<Mmap>,
}

impl SharedMemSlice {
    pub fn from(file: &File, size: usize) -> Result<SharedMemSlice> {
        let mmap = unsafe { MmapOptions::new().map(file)? };
        assert_eq!(mmap.len(), size);
        let mmap = Arc::new(mmap);
        Ok(SharedMemSlice { mmap })
    }

    pub fn get_slice(&self, size: usize) -> Result<&[u8]> {
        if size == 0 {
            return Ok(&[]);
        }
        // TODO: Track how much is in the shm area.
        if size <= self.mmap.len() {
            atomic::fence(atomic::Ordering::Acquire);
            let buf = &self.mmap[..size];
            Ok(buf)
        } else {
            bail!("mmap size");
        }
    }

    /// Clones the memory map.
    ///
    /// The underlying memory map is shared, and thus the caller must
    /// ensure that the memory is not illegally aliased.
    pub unsafe fn unsafe_clone(&self) -> Self {
        SharedMemSlice {
            mmap: self.mmap.clone(),
        }
    }
}

unsafe impl Send for SharedMemSlice {}

pub struct SharedMemWriter {
    mmap: MmapMut,
}

impl SharedMemWriter {
    pub fn new(path: &Path, size: usize) -> Result<(SharedMemWriter, File)> {
        let file = OpenOptions::new()
            .read(true)
            .write(true)
            .create_new(true)
            .open(path)?;
        let _ = remove_file(path);
        file.set_len(size as u64)?;
        let mmap = unsafe { MmapOptions::new().map_mut(&file)? };
        Ok((SharedMemWriter { mmap }, file))
    }

    pub fn write(&mut self, buf: &[u8]) -> Result<()> {
        if buf.is_empty() {
            return Ok(());
        }
        // TODO: Track how much is in the shm area.
        if buf.len() <= self.mmap.len() {
            self.mmap[..buf.len()].copy_from_slice(buf);
            atomic::fence(atomic::Ordering::Release);
            Ok(())
        } else {
            bail!("mmap size");
        }
    }
}

pub struct SharedMemMutSlice {
    mmap: Arc<UnsafeCell<MmapMut>>,
}

impl SharedMemMutSlice {
    pub fn from(file: &File, size: usize) -> Result<SharedMemMutSlice> {
        let mmap = unsafe { MmapOptions::new().map_mut(file)? };
        assert_eq!(mmap.len(), size);
        let mmap = Arc::new(UnsafeCell::new(mmap));
        Ok(SharedMemMutSlice { mmap })
    }

    pub fn get_mut_slice(&mut self, size: usize) -> Result<&mut [u8]> {
        if size == 0 {
            return Ok(&mut []);
        }
        // TODO: Track how much is in the shm area.
        if size <= self.inner().len() {
            let buf = &mut self.inner_mut()[..size];
            atomic::fence(atomic::Ordering::Release);
            Ok(buf)
        } else {
            bail!("mmap size");
        }
    }

    /// Clones the memory map.
    ///
    /// The underlying memory map is shared, and thus the caller must
    /// ensure that the memory is not illegally aliased.
    pub unsafe fn unsafe_clone(&self) -> Self {
        SharedMemMutSlice {
            mmap: self.mmap.clone(),
        }
    }

    fn inner(&self) -> &MmapMut {
        unsafe { &*self.mmap.get() }
    }

    fn inner_mut(&mut self) -> &mut MmapMut {
        unsafe { &mut *self.mmap.get() }
    }
}

unsafe impl Send for SharedMemMutSlice {}
