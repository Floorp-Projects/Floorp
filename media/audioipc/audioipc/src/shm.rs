// Copyright © 2017 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details.

use crate::errors::*;
use memmap::{Mmap, MmapMut, MmapOptions};
use std::cell::UnsafeCell;
use std::convert::TryInto;
use std::env::temp_dir;
use std::fs::{remove_file, File, OpenOptions};
use std::sync::{atomic, Arc};

fn open_shm_file(id: &str) -> Result<File> {
    #[cfg(target_os = "linux")]
    {
        let id_cstring = std::ffi::CString::new(id).unwrap();
        unsafe {
            let r = libc::syscall(libc::SYS_memfd_create, id_cstring.as_ptr(), 0);
            if r >= 0 {
                use std::os::unix::io::FromRawFd as _;
                return Ok(File::from_raw_fd(r.try_into().unwrap()));
            }
        }

        let mut path = std::path::PathBuf::from("/dev/shm");
        path.push(id);

        if let Ok(file) = OpenOptions::new()
            .read(true)
            .write(true)
            .create_new(true)
            .open(&path)
        {
            let _ = remove_file(&path);
            return Ok(file);
        }
    }

    let mut path = temp_dir();
    path.push(id);

    let file = OpenOptions::new()
        .read(true)
        .write(true)
        .create_new(true)
        .open(&path)?;

    let _ = remove_file(&path);
    Ok(file)
}

#[cfg(unix)]
fn handle_enospc(s: &str) -> Result<()> {
    let err = std::io::Error::last_os_error();
    let errno = err.raw_os_error().unwrap_or(0);
    assert_ne!(errno, 0);
    debug!("allocate_file: {} failed errno={}", s, errno);
    if errno == libc::ENOSPC {
        return Err(err.into());
    }
    Ok(())
}

#[cfg(unix)]
fn allocate_file(file: &File, size: usize) -> Result<()> {
    use std::os::unix::io::AsRawFd;

    // First, set the file size.  This may create a sparse file on
    // many systems, which can fail with SIGBUS when accessed via a
    // mapping and the lazy backing allocation fails due to low disk
    // space.  To avoid this, try to force the entire file to be
    // preallocated before mapping using OS-specific approaches below.

    file.set_len(size.try_into().unwrap())?;

    let fd = file.as_raw_fd();
    let size: libc::off_t = size.try_into().unwrap();

    // Try Linux-specific fallocate.
    #[cfg(target_os = "linux")]
    {
        if unsafe { libc::fallocate(fd, 0, 0, size) } == 0 {
            return Ok(());
        }
        handle_enospc("fallocate()")?;
    }

    // Try macOS-specific fcntl.
    #[cfg(target_os = "macos")]
    {
        let params = libc::fstore_t {
            fst_flags: libc::F_ALLOCATEALL,
            fst_posmode: libc::F_PEOFPOSMODE,
            fst_offset: 0,
            fst_length: size,
            fst_bytesalloc: 0,
        };
        if unsafe { libc::fcntl(fd, libc::F_PREALLOCATE, &params) } == 0 {
            return Ok(());
        }
        handle_enospc("fcntl(F_PREALLOCATE)")?;
    }

    // Fall back to portable version, where available.
    #[cfg(any(target_os = "linux", target_os = "freebsd", target_os = "dragonfly"))]
    {
        if unsafe { libc::posix_fallocate(fd, 0, size) } == 0 {
            return Ok(());
        }
        handle_enospc("posix_fallocate()")?;
    }

    Ok(())
}

#[cfg(windows)]
fn allocate_file(file: &File, size: usize) -> Result<()> {
    // CreateFileMapping will ensure the entire file is allocated
    // before it's mapped in, so we simply set the size here.
    file.set_len(size.try_into().unwrap())?;
    Ok(())
}

pub struct SharedMemReader {
    mmap: Mmap,
}

impl SharedMemReader {
    pub fn new(id: &str, size: usize) -> Result<(SharedMemReader, File)> {
        let file = open_shm_file(id)?;
        allocate_file(&file, size)?;
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
    pub fn new(id: &str, size: usize) -> Result<(SharedMemWriter, File)> {
        let file = open_shm_file(id)?;
        allocate_file(&file, size)?;
        let mmap = unsafe { MmapOptions::new().map_mut(&file)? };
        assert_eq!(mmap.len(), size);
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
