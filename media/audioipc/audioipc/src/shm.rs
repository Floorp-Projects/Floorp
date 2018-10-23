// Copyright © 2017 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details.

use errors::*;
use memmap::{Mmap, MmapViewSync, Protection};
use std::fs::{remove_file, File, OpenOptions};
use std::path::Path;
use std::sync::atomic;

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
        let mmap = Mmap::open(&file, Protection::Read)?;
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
            unsafe {
                let len = buf.len();
                buf.copy_from_slice(&self.mmap.as_slice()[..len]);
            }
            Ok(())
        } else {
            bail!("mmap size");
        }
    }
}

pub struct SharedMemSlice {
    view: MmapViewSync,
}

impl SharedMemSlice {
    pub fn from(file: &File, size: usize) -> Result<SharedMemSlice> {
        let mmap = Mmap::open(file, Protection::Read)?;
        assert_eq!(mmap.len(), size);
        let view = mmap.into_view_sync();
        Ok(SharedMemSlice { view })
    }

    pub fn get_slice(&self, size: usize) -> Result<&[u8]> {
        if size == 0 {
            return Ok(&[]);
        }
        // TODO: Track how much is in the shm area.
        if size <= self.view.len() {
            atomic::fence(atomic::Ordering::Acquire);
            let buf = unsafe { &self.view.as_slice()[..size] };
            Ok(buf)
        } else {
            bail!("mmap size");
        }
    }

    /// Clones the view of the memory map.
    ///
    /// The underlying memory map is shared, and thus the caller must ensure that the memory
    /// underlying the view is not illegally aliased.
    pub unsafe fn clone_view(&self) -> Self {
        SharedMemSlice {
            view: self.view.clone(),
        }
    }
}

pub struct SharedMemWriter {
    mmap: Mmap,
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
        let mmap = Mmap::open(&file, Protection::ReadWrite)?;
        Ok((SharedMemWriter { mmap }, file))
    }

    pub fn write(&mut self, buf: &[u8]) -> Result<()> {
        if buf.is_empty() {
            return Ok(());
        }
        // TODO: Track how much is in the shm area.
        if buf.len() <= self.mmap.len() {
            unsafe {
                self.mmap.as_mut_slice()[..buf.len()].copy_from_slice(buf);
            }
            atomic::fence(atomic::Ordering::Release);
            Ok(())
        } else {
            bail!("mmap size");
        }
    }
}

pub struct SharedMemMutSlice {
    view: MmapViewSync,
}

impl SharedMemMutSlice {
    pub fn from(file: &File, size: usize) -> Result<SharedMemMutSlice> {
        let mmap = Mmap::open(file, Protection::ReadWrite)?;
        assert_eq!(mmap.len(), size);
        let view = mmap.into_view_sync();
        Ok(SharedMemMutSlice { view })
    }

    pub fn get_mut_slice(&mut self, size: usize) -> Result<&mut [u8]> {
        if size == 0 {
            return Ok(&mut []);
        }
        // TODO: Track how much is in the shm area.
        if size <= self.view.len() {
            let buf = unsafe { &mut self.view.as_mut_slice()[..size] };
            atomic::fence(atomic::Ordering::Release);
            Ok(buf)
        } else {
            bail!("mmap size");
        }
    }

    /// Clones the view of the memory map.
    ///
    /// The underlying memory map is shared, and thus the caller must
    /// ensure that the memory underlying the view is not illegally
    /// aliased.
    pub unsafe fn clone_view(&self) -> Self {
        SharedMemMutSlice {
            view: self.view.clone(),
        }
    }
}
