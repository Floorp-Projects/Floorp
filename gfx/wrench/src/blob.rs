/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// A very basic BlobImageRenderer that can only render a checkerboard pattern.

use std::collections::HashMap;
use std::sync::Arc;
use std::sync::Mutex;
use webrender::api::*;
use webrender::intersect_for_tile;

// Serialize/deserialize the blob.

pub fn serialize_blob(color: ColorU) -> Vec<u8> {
    vec![color.r, color.g, color.b, color.a]
}

fn deserialize_blob(blob: &[u8]) -> Result<ColorU, ()> {
    let mut iter = blob.iter();
    return match (iter.next(), iter.next(), iter.next(), iter.next()) {
        (Some(&r), Some(&g), Some(&b), Some(&a)) => Ok(ColorU::new(r, g, b, a)),
        (Some(&a), None, None, None) => Ok(ColorU::new(a, a, a, a)),
        _ => Err(()),
    };
}

// This is the function that applies the deserialized drawing commands and generates
// actual image data.
fn render_blob(
    color: ColorU,
    descriptor: &BlobImageDescriptor,
    tile: Option<(TileSize, TileOffset)>,
    dirty_rect: Option<DeviceUintRect>,
) -> BlobImageResult {
    // Allocate storage for the result. Right now the resource cache expects the
    // tiles to have have no stride or offset.
    let mut texels = vec![0u8; (descriptor.width * descriptor.height * descriptor.format.bytes_per_pixel()) as usize];

    // Generate a per-tile pattern to see it in the demo. For a real use case it would not
    // make sense for the rendered content to depend on its tile.
    let tile_checker = match tile {
        Some((_, tile)) => (tile.x % 2 == 0) != (tile.y % 2 == 0),
        None => true,
    };

    let mut dirty_rect = dirty_rect.unwrap_or(DeviceUintRect::new(
        DeviceUintPoint::new(0, 0),
        DeviceUintSize::new(descriptor.width, descriptor.height)));

    if let Some((tile_size, tile)) = tile {
        dirty_rect = intersect_for_tile(dirty_rect, tile_size as u32, tile_size as u32,
                                        tile_size, tile)
            .expect("empty rects should be culled by webrender");
    }


    for y in dirty_rect.min_y() .. dirty_rect.max_y() {
        for x in dirty_rect.min_x() .. dirty_rect.max_x() {
            // Apply the tile's offset. This is important: all drawing commands should be
            // translated by this offset to give correct results with tiled blob images.
            let x2 = x + descriptor.offset.x as u32;
            let y2 = y + descriptor.offset.y as u32;

            // Render a simple checkerboard pattern
            let checker = if (x2 % 20 >= 10) != (y2 % 20 >= 10) {
                1
            } else {
                0
            };
            // ..nested in the per-tile checkerboard pattern
            let tc = if tile_checker { 0 } else { (1 - checker) * 40 };

            match descriptor.format {
                ImageFormat::BGRA8 => {
                    texels[((y * descriptor.width + x) * 4 + 0) as usize] = color.b * checker + tc;
                    texels[((y * descriptor.width + x) * 4 + 1) as usize] = color.g * checker + tc;
                    texels[((y * descriptor.width + x) * 4 + 2) as usize] = color.r * checker + tc;
                    texels[((y * descriptor.width + x) * 4 + 3) as usize] = color.a * checker + tc;
                }
                ImageFormat::R8 => {
                    texels[(y * descriptor.width + x) as usize] = color.a * checker + tc;
                }
                _ => {
                    return Err(BlobImageError::Other(
                        format!("Unsupported image format {:?}", descriptor.format),
                    ));
                }
            }
        }
    }

    Ok(RasterizedBlobImage {
        data: texels,
        width: descriptor.width,
        height: descriptor.height,
    })
}

pub struct BlobCallbacks {
    pub request: Box<Fn(&BlobImageRequest) + Send + 'static>,
    pub resolve: Box<Fn() + Send + 'static>,
}

impl BlobCallbacks {
    pub fn new() -> Self {
        BlobCallbacks { request: Box::new(|_|()), resolve: Box::new(|| (())) }
    }
}

pub struct CheckerboardRenderer {
    image_cmds: HashMap<ImageKey, (ColorU, Option<TileSize>)>,
    callbacks: Arc<Mutex<BlobCallbacks>>,

    // The images rendered in the current frame (not kept here between frames).
    rendered_images: HashMap<BlobImageRequest, BlobImageResult>,
}

impl CheckerboardRenderer {
    pub fn new(callbacks: Arc<Mutex<BlobCallbacks>>) -> Self {
        CheckerboardRenderer {
            callbacks,
            image_cmds: HashMap::new(),
            rendered_images: HashMap::new(),
        }
    }
}

impl BlobImageRenderer for CheckerboardRenderer {
    fn add(&mut self, key: ImageKey, cmds: BlobImageData, tile_size: Option<TileSize>) {
        self.image_cmds
            .insert(key, (deserialize_blob(&cmds[..]).unwrap(), tile_size));
    }

    fn update(&mut self, key: ImageKey, cmds: BlobImageData, _dirty_rect: Option<DeviceUintRect>) {
        // Here, updating is just replacing the current version of the commands with
        // the new one (no incremental updates).
        self.image_cmds.get_mut(&key).unwrap().0 = deserialize_blob(&cmds[..]).unwrap();
    }

    fn delete(&mut self, key: ImageKey) {
        self.image_cmds.remove(&key);
    }

    fn request(
        &mut self,
        _resources: &BlobImageResources,
        request: BlobImageRequest,
        descriptor: &BlobImageDescriptor,
        dirty_rect: Option<DeviceUintRect>,
    ) {
        (self.callbacks.lock().unwrap().request)(&request);
        assert!(!self.rendered_images.contains_key(&request));
        // This method is where we kick off our rendering jobs.
        // It should avoid doing work on the calling thread as much as possible.
        // In this example we will use the thread pool to render individual tiles.

        // Gather the input data to send to a worker thread.
        let &(color, tile_size) = self.image_cmds.get(&request.key).unwrap();

        let tile = request.tile.map(|tile| (tile_size.unwrap(), tile));

        let result = render_blob(color, descriptor, tile, dirty_rect);

        self.rendered_images.insert(request, result);
    }

    fn resolve(&mut self, request: BlobImageRequest) -> BlobImageResult {
        (self.callbacks.lock().unwrap().resolve)();
        self.rendered_images.remove(&request).unwrap()
    }

    fn delete_font(&mut self, _key: FontKey) {}

    fn delete_font_instance(&mut self, _key: FontInstanceKey) {}

    fn clear_namespace(&mut self, _namespace: IdNamespace) {}
}
