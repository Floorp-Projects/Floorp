/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

extern crate gleam;
extern crate glutin;
extern crate rayon;
extern crate webrender;

#[path = "common/boilerplate.rs"]
mod boilerplate;

use boilerplate::{Example, HandyDandyRectBuilder};
use rayon::{ThreadPool, ThreadPoolBuilder};
use std::collections::HashMap;
use std::collections::hash_map::Entry;
use std::sync::Arc;
use std::sync::mpsc::{Receiver, Sender, channel};
use webrender::api::{self, DisplayListBuilder, DocumentId, PipelineId, RenderApi, ResourceUpdates};

// This example shows how to implement a very basic BlobImageRenderer that can only render
// a checkerboard pattern.

// The deserialized command list internally used by this example is just a color.
type ImageRenderingCommands = api::ColorU;

// Serialize/deserialize the blob.
// For real usecases you should probably use serde rather than doing it by hand.

fn serialize_blob(color: api::ColorU) -> Vec<u8> {
    vec![color.r, color.g, color.b, color.a]
}

fn deserialize_blob(blob: &[u8]) -> Result<ImageRenderingCommands, ()> {
    let mut iter = blob.iter();
    return match (iter.next(), iter.next(), iter.next(), iter.next()) {
        (Some(&r), Some(&g), Some(&b), Some(&a)) => Ok(api::ColorU::new(r, g, b, a)),
        (Some(&a), None, None, None) => Ok(api::ColorU::new(a, a, a, a)),
        _ => Err(()),
    };
}

// This is the function that applies the deserialized drawing commands and generates
// actual image data.
fn render_blob(
    commands: Arc<ImageRenderingCommands>,
    descriptor: &api::BlobImageDescriptor,
    tile: Option<api::TileOffset>,
) -> api::BlobImageResult {
    let color = *commands;

    // Allocate storage for the result. Right now the resource cache expects the
    // tiles to have have no stride or offset.
    let mut texels = Vec::with_capacity((descriptor.width * descriptor.height * 4) as usize);

    // Generate a per-tile pattern to see it in the demo. For a real use case it would not
    // make sense for the rendered content to depend on its tile.
    let tile_checker = match tile {
        Some(tile) => (tile.x % 2 == 0) != (tile.y % 2 == 0),
        None => true,
    };

    for y in 0 .. descriptor.height {
        for x in 0 .. descriptor.width {
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
                api::ImageFormat::BGRA8 => {
                    texels.push(color.b * checker + tc);
                    texels.push(color.g * checker + tc);
                    texels.push(color.r * checker + tc);
                    texels.push(color.a * checker + tc);
                }
                api::ImageFormat::R8 => {
                    texels.push(color.a * checker + tc);
                }
                _ => {
                    return Err(api::BlobImageError::Other(
                        format!("Unsupported image format"),
                    ));
                }
            }
        }
    }

    Ok(api::RasterizedBlobImage {
        data: texels,
        width: descriptor.width,
        height: descriptor.height,
    })
}

struct CheckerboardRenderer {
    // We are going to defer the rendering work to worker threads.
    // Using a pre-built Arc<ThreadPool> rather than creating our own threads
    // makes it possible to share the same thread pool as the glyph renderer (if we
    // want to).
    workers: Arc<ThreadPool>,

    // the workers will use an mpsc channel to communicate the result.
    tx: Sender<(api::BlobImageRequest, api::BlobImageResult)>,
    rx: Receiver<(api::BlobImageRequest, api::BlobImageResult)>,

    // The deserialized drawing commands.
    // In this example we store them in Arcs. This isn't necessary since in this simplified
    // case the command list is a simple 32 bits value and would be cheap to clone before sending
    // to the workers. But in a more realistic scenario the commands would typically be bigger
    // and more expensive to clone, so let's pretend it is also the case here.
    image_cmds: HashMap<api::ImageKey, Arc<ImageRenderingCommands>>,

    // The images rendered in the current frame (not kept here between frames).
    rendered_images: HashMap<api::BlobImageRequest, Option<api::BlobImageResult>>,
}

impl CheckerboardRenderer {
    fn new(workers: Arc<ThreadPool>) -> Self {
        let (tx, rx) = channel();
        CheckerboardRenderer {
            image_cmds: HashMap::new(),
            rendered_images: HashMap::new(),
            workers,
            tx,
            rx,
        }
    }
}

impl api::BlobImageRenderer for CheckerboardRenderer {
    fn add(&mut self, key: api::ImageKey, cmds: api::BlobImageData, _: Option<api::TileSize>) {
        self.image_cmds
            .insert(key, Arc::new(deserialize_blob(&cmds[..]).unwrap()));
    }

    fn update(&mut self, key: api::ImageKey, cmds: api::BlobImageData, _dirty_rect: Option<api::DeviceUintRect>) {
        // Here, updating is just replacing the current version of the commands with
        // the new one (no incremental updates).
        self.image_cmds
            .insert(key, Arc::new(deserialize_blob(&cmds[..]).unwrap()));
    }

    fn delete(&mut self, key: api::ImageKey) {
        self.image_cmds.remove(&key);
    }

    fn request(
        &mut self,
        _resources: &api::BlobImageResources,
        request: api::BlobImageRequest,
        descriptor: &api::BlobImageDescriptor,
        _dirty_rect: Option<api::DeviceUintRect>,
    ) {
        // This method is where we kick off our rendering jobs.
        // It should avoid doing work on the calling thread as much as possible.
        // In this example we will use the thread pool to render individual tiles.

        // Gather the input data to send to a worker thread.
        let cmds = Arc::clone(&self.image_cmds.get(&request.key).unwrap());
        let tx = self.tx.clone();
        let descriptor = descriptor.clone();

        self.workers.spawn(move || {
            let result = render_blob(cmds, &descriptor, request.tile);
            tx.send((request, result)).unwrap();
        });

        // Add None in the map of rendered images. This makes it possible to differentiate
        // between commands that aren't finished yet (entry in the map is equal to None) and
        // keys that have never been requested (entry not in the map), which would cause deadlocks
        // if we were to block upon receiving their result in resolve!
        self.rendered_images.insert(request, None);
    }

    fn resolve(&mut self, request: api::BlobImageRequest) -> api::BlobImageResult {
        // In this method we wait until the work is complete on the worker threads and
        // gather the results.

        // First look at whether we have already received the rendered image
        // that we are looking for.
        match self.rendered_images.entry(request) {
            Entry::Vacant(_) => {
                return Err(api::BlobImageError::InvalidKey);
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
                return result;
            }
            self.rendered_images.insert(req, Some(result));
        }

        // If we break out of the loop above it means the channel closed unexpectedly.
        Err(api::BlobImageError::Other("Channel closed".into()))
    }
    fn delete_font(&mut self, _font: api::FontKey) {}
    fn delete_font_instance(&mut self, _instance: api::FontInstanceKey) {}
    fn clear_namespace(&mut self, _namespace: api::IdNamespace) {}
}

struct App {}

impl Example for App {
    fn render(
        &mut self,
        api: &RenderApi,
        builder: &mut DisplayListBuilder,
        resources: &mut ResourceUpdates,
        _framebuffer_size: api::DeviceUintSize,
        _pipeline_id: PipelineId,
        _document_id: DocumentId,
    ) {
        let blob_img1 = api.generate_image_key();
        resources.add_image(
            blob_img1,
            api::ImageDescriptor::new(500, 500, api::ImageFormat::BGRA8, true, false),
            api::ImageData::new_blob_image(serialize_blob(api::ColorU::new(50, 50, 150, 255))),
            Some(128),
        );

        let blob_img2 = api.generate_image_key();
        resources.add_image(
            blob_img2,
            api::ImageDescriptor::new(200, 200, api::ImageFormat::BGRA8, true, false),
            api::ImageData::new_blob_image(serialize_blob(api::ColorU::new(50, 150, 50, 255))),
            None,
        );

        let bounds = api::LayoutRect::new(api::LayoutPoint::zero(), builder.content_size());
        let info = api::LayoutPrimitiveInfo::new(bounds);
        builder.push_stacking_context(
            &info,
            None,
            api::ScrollPolicy::Scrollable,
            None,
            api::TransformStyle::Flat,
            None,
            api::MixBlendMode::Normal,
            Vec::new(),
            api::GlyphRasterSpace::Screen,
        );

        let info = api::LayoutPrimitiveInfo::new((30, 30).by(500, 500));
        builder.push_image(
            &info,
            api::LayoutSize::new(500.0, 500.0),
            api::LayoutSize::new(0.0, 0.0),
            api::ImageRendering::Auto,
            api::AlphaType::PremultipliedAlpha,
            blob_img1,
        );

        let info = api::LayoutPrimitiveInfo::new((600, 600).by(200, 200));
        builder.push_image(
            &info,
            api::LayoutSize::new(200.0, 200.0),
            api::LayoutSize::new(0.0, 0.0),
            api::ImageRendering::Auto,
            api::AlphaType::PremultipliedAlpha,
            blob_img2,
        );

        builder.pop_stacking_context();
    }
}

fn main() {
    let workers =
        ThreadPoolBuilder::new().thread_name(|idx| format!("WebRender:Worker#{}", idx))
                                .build();

    let workers = Arc::new(workers.unwrap());

    let opts = webrender::RendererOptions {
        workers: Some(Arc::clone(&workers)),
        // Register our blob renderer, so that WebRender integrates it in the resource cache..
        // Share the same pool of worker threads between WebRender and our blob renderer.
        blob_image_renderer: Some(Box::new(CheckerboardRenderer::new(Arc::clone(&workers)))),
        ..Default::default()
    };

    let mut app = App {};

    boilerplate::main_wrapper(&mut app, Some(opts));
}
