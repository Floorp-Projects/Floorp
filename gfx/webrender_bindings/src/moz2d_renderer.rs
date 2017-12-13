#![allow(improper_ctypes)] // this is needed so that rustc doesn't complain about passing the &Arc<Vec> to an extern function
use webrender::api::*;
use bindings::{ByteSlice, MutByteSlice, wr_moz2d_render_cb, ArcVecU8};
use rayon::ThreadPool;

use std::collections::hash_map::{HashMap, Entry};
use std::mem;
use std::os::raw::c_void;
use std::ptr;
use std::sync::mpsc::{channel, Sender, Receiver};
use std::sync::Arc;

#[cfg(target_os = "windows")]
use dwrote;

#[cfg(target_os = "macos")]
use foreign_types::ForeignType;

#[cfg(not(any(target_os = "macos", target_os = "windows")))]
use std::ffi::CString;

pub struct Moz2dImageRenderer {
    blob_commands: HashMap<ImageKey, (Arc<BlobImageData>, Option<TileSize>)>,

    // The images rendered in the current frame (not kept here between frames)
    rendered_images: HashMap<BlobImageRequest, Option<BlobImageResult>>,

    tx: Sender<(BlobImageRequest, BlobImageResult)>,
    rx: Receiver<(BlobImageRequest, BlobImageResult)>,

    workers: Arc<ThreadPool>,
}

fn option_to_nullable<T>(option: &Option<T>) -> *const T {
    match option {
        &Some(ref x) => { x as *const T }
        &None => { ptr::null() }
    }
}

fn to_usize(slice: &[u8]) -> usize {
    convert_from_bytes(slice)
}

fn convert_from_bytes<T>(slice: &[u8]) -> T {
    assert!(mem::size_of::<T>() <= slice.len());
    let mut ret: T;
    unsafe {
        ret = mem::uninitialized();
        ptr::copy_nonoverlapping(slice.as_ptr(),
                                 &mut ret as *mut T as *mut u8,
                                 mem::size_of::<T>());
    }
    ret
}

struct BufReader<'a>
{
    buf: &'a[u8],
    pos: usize,
}

impl<'a> BufReader<'a> {
    fn new(buf: &'a[u8]) -> BufReader<'a> {
        BufReader{ buf: buf, pos: 0 }
    }

    fn read<T>(&mut self) -> T {
        let ret = convert_from_bytes(&self.buf[self.pos..]);
        self.pos += mem::size_of::<T>();
        ret
    }

    fn read_font_key(&mut self) -> FontKey {
        self.read()
    }

    fn read_usize(&mut self) -> usize {
        self.read()
    }
}

impl BlobImageRenderer for Moz2dImageRenderer {
    fn add(&mut self, key: ImageKey, data: BlobImageData, tiling: Option<TileSize>) {
        self.blob_commands.insert(key, (Arc::new(data), tiling));
    }

    fn update(&mut self, key: ImageKey, data: BlobImageData, _dirty_rect: Option<DeviceUintRect>) {
        let entry = self.blob_commands.get_mut(&key).unwrap();
        entry.0 = Arc::new(data);
    }

    fn delete(&mut self, key: ImageKey) {
        self.blob_commands.remove(&key);
    }

    fn request(&mut self,
               resources: &BlobImageResources,
               request: BlobImageRequest,
               descriptor: &BlobImageDescriptor,
               _dirty_rect: Option<DeviceUintRect>) {
        debug_assert!(!self.rendered_images.contains_key(&request));
        // TODO: implement tiling.

        // Add None in the map of rendered images. This makes it possible to differentiate
        // between commands that aren't finished yet (entry in the map is equal to None) and
        // keys that have never been requested (entry not in the map), which would cause deadlocks
        // if we were to block upon receving their result in resolve!
        self.rendered_images.insert(request, None);

        let tx = self.tx.clone();
        let descriptor = descriptor.clone();
        let blob = &self.blob_commands[&request.key];
        let tile_size = blob.1;
        let commands = Arc::clone(&blob.0);

        #[cfg(target_os = "windows")]
        fn process_native_font_handle(key: FontKey, handle: &NativeFontHandle) {
            let system_fc = dwrote::FontCollection::system();
            let font = system_fc.get_font_from_descriptor(handle).unwrap();
            let face = font.create_font_face();
            unsafe { AddNativeFontHandle(key, face.as_ptr() as *mut c_void, 0) };
        }

        #[cfg(target_os = "macos")]
        fn process_native_font_handle(key: FontKey, handle: &NativeFontHandle) {
            unsafe { AddNativeFontHandle(key, handle.0.as_ptr() as *mut c_void, 0) };
        }

        #[cfg(not(any(target_os = "macos", target_os = "windows")))]
        fn process_native_font_handle(key: FontKey, handle: &NativeFontHandle) {
            let cstr = CString::new(handle.pathname.clone()).unwrap();
            unsafe { AddNativeFontHandle(key, cstr.as_ptr() as *mut c_void, handle.index) };
        }

        fn process_fonts(mut extra_data: BufReader, resources: &BlobImageResources) {
            let font_count = extra_data.read_usize();
            for _ in 0..font_count {
                let key = extra_data.read_font_key();
                let template = resources.get_font_data(key);
                match template {
                    &FontTemplate::Raw(ref data, ref index) => {
                        unsafe { AddFontData(key, data.as_ptr(), data.len(), *index, data); }
                    }
                    &FontTemplate::Native(ref handle) => {
                        process_native_font_handle(key, handle);
                    }
                }
                resources.get_font_data(key);
            }
        }
        let index_offset_pos = commands.len()-mem::size_of::<usize>();

        let index_offset = to_usize(&commands[index_offset_pos..]);
        {
            let mut index = BufReader::new(&commands[index_offset..index_offset_pos]);
            while index.pos < index.buf.len() {
                let end = index.read_usize();
                let extra_end = index.read_usize();
                process_fonts(BufReader::new(&commands[end..extra_end]), resources);
            }
        }

        self.workers.spawn(move || {
            let buf_size = (descriptor.width
                * descriptor.height
                * descriptor.format.bytes_per_pixel()) as usize;
            let mut output = vec![0u8; buf_size];

            let result = unsafe {
                if wr_moz2d_render_cb(
                    ByteSlice::new(&commands[..]),
                    descriptor.width,
                    descriptor.height,
                    descriptor.format,
                    option_to_nullable(&tile_size),
                    option_to_nullable(&request.tile),
                    MutByteSlice::new(output.as_mut_slice())
                ) {

                    Ok(RasterizedBlobImage {
                        width: descriptor.width,
                        height: descriptor.height,
                        data: output,
                    })
                } else {
                    Err(BlobImageError::Other("Unknown error".to_string()))
                }
            };

            tx.send((request, result)).unwrap();
        });
    }

    fn resolve(&mut self, request: BlobImageRequest) -> BlobImageResult {

        match self.rendered_images.entry(request) {
            Entry::Vacant(_) => {
                return Err(BlobImageError::InvalidKey);
            }
            Entry::Occupied(entry) => {
                // None means we haven't yet received the result.
                if entry.get().is_some() {
                    let result = entry.remove();
                    return result.unwrap();
                }
            }
        }

        // We haven't received it yet, pull from the channel until we receive it.
        while let Ok((req, result)) = self.rx.recv() {
            if req == request {
                // There it is!
                self.rendered_images.remove(&request);
                return result
            }
            self.rendered_images.insert(req, Some(result));
        }

        // If we break out of the loop above it means the channel closed unexpectedly.
        Err(BlobImageError::Other("Channel closed".into()))
    }
    fn delete_font(&mut self, font: FontKey) {
        unsafe { DeleteFontData(font); }
    }

    fn delete_font_instance(&mut self, _key: FontInstanceKey) {
    }
}

use bindings::WrFontKey;
extern "C" {
    #[allow(improper_ctypes)]
    fn AddFontData(key: WrFontKey, data: *const u8, size: usize, index: u32, vec: &ArcVecU8);
    fn AddNativeFontHandle(key: WrFontKey, handle: *mut c_void, index: u32);
    fn DeleteFontData(key: WrFontKey);
}

impl Moz2dImageRenderer {
    pub fn new(workers: Arc<ThreadPool>) -> Self {
        let (tx, rx) = channel();
        Moz2dImageRenderer {
            blob_commands: HashMap::new(),
            rendered_images: HashMap::new(),
            workers: workers,
            tx: tx,
            rx: rx,
        }
    }
}

