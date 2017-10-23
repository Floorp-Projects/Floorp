use webrender_api::*;
use bindings::{ByteSlice, MutByteSlice, wr_moz2d_render_cb};
use rayon::ThreadPool;

use std::collections::hash_map::{HashMap, Entry};
use std::ptr;
use std::sync::mpsc::{channel, Sender, Receiver};
use std::sync::Arc;

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
               _resources: &BlobImageResources,
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


        self.workers.spawn(move || {
            let buf_size = (descriptor.width
                * descriptor.height
                * descriptor.format.bytes_per_pixel()) as usize;
            let mut output = vec![255u8; buf_size];

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

    fn delete_font(&mut self, _font: FontKey) {
    }

    fn delete_font_instance(&mut self, _key: FontInstanceKey) {
    }
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

