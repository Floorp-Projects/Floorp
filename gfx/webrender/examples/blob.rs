/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

extern crate app_units;
extern crate euclid;
extern crate gleam;
extern crate glutin;
extern crate webrender;
extern crate webrender_traits;
extern crate rayon;

#[path="common/boilerplate.rs"]
mod boilerplate;

use boilerplate::HandyDandyRectBuilder;
use rayon::ThreadPool;
use rayon::Configuration as ThreadPoolConfig;
use std::collections::HashMap;
use std::collections::hash_map::Entry;
use std::sync::Arc;
use std::sync::mpsc::{channel, Sender, Receiver};
use webrender_traits as wt;

// This example shows how to implement a very basic BlobImageRenderer that can only render
// a checkerboard pattern.

// The deserialized command list internally used by this example is just a color.
type ImageRenderingCommands = wt::ColorU;

// Serialize/deserialze the blob.
// Ror real usecases you should probably use serde rather than doing it by hand.

fn serialize_blob(color: wt::ColorU) -> Vec<u8> {
    vec![color.r, color.g, color.b, color.a]
}

fn deserialize_blob(blob: &[u8]) -> Result<ImageRenderingCommands, ()> {
    let mut iter = blob.iter();
    return match (iter.next(), iter.next(), iter.next(), iter.next()) {
        (Some(&r), Some(&g), Some(&b), Some(&a)) => Ok(wt::ColorU::new(r, g, b, a)),
        (Some(&a), None, None, None) => Ok(wt::ColorU::new(a, a, a, a)),
        _ => Err(()),
    }
}

// This is the function that applies the deserialized drawing commands and generates
// actual image data.
fn render_blob(
    commands: Arc<ImageRenderingCommands>,
    descriptor: &wt::BlobImageDescriptor,
    tile: Option<wt::TileOffset>,
) -> wt::BlobImageResult {
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

    for y in 0..descriptor.height {
        for x in 0..descriptor.width {
            // Apply the tile's offset. This is important: all drawing commands should be
            // translated by this offset to give correct results with tiled blob images.
            let x2 = x + descriptor.offset.x as u32;
            let y2 = y + descriptor.offset.y as u32;

            // Render a simple checkerboard pattern
            let checker = if (x2 % 20 >= 10) != (y2 % 20 >= 10) { 1 } else { 0 };
            // ..nested in the per-tile cherkerboard pattern
            let tc = if tile_checker { 0 } else { (1 - checker) * 40 };

            match descriptor.format {
                wt::ImageFormat::RGBA8 => {
                    texels.push(color.b * checker + tc);
                    texels.push(color.g * checker + tc);
                    texels.push(color.r * checker + tc);
                    texels.push(color.a * checker + tc);
                }
                wt::ImageFormat::A8 => {
                    texels.push(color.a * checker + tc);
                }
                _ => {
                    return Err(wt::BlobImageError::Other(format!(
                        "Usupported image format {:?}",
                        descriptor.format
                    )));
                }
            }
        }
    }

    Ok(wt::RasterizedBlobImage {
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
    tx: Sender<(wt::BlobImageRequest, wt::BlobImageResult)>,
    rx: Receiver<(wt::BlobImageRequest, wt::BlobImageResult)>,

    // The deserialized drawing commands.
    // In this example we store them in Arcs. This isn't necessary since in this simplified
    // case the command list is a simple 32 bits value and would be cheap to clone before sending
    // to the workers. But in a more realistic scenario the commands would typically be bigger
    // and more expensive to clone, so let's pretend it is also the case here.
    image_cmds: HashMap<wt::ImageKey, Arc<ImageRenderingCommands>>,

    // The images rendered in the current frame (not kept here between frames).
    rendered_images: HashMap<wt::BlobImageRequest, Option<wt::BlobImageResult>>,
}

impl CheckerboardRenderer {
    fn new(workers: Arc<ThreadPool>) -> Self {
        let (tx, rx) = channel();
        CheckerboardRenderer {
            image_cmds: HashMap::new(),
            rendered_images: HashMap::new(),
            workers: workers,
            tx: tx,
            rx: rx,
        }
    }
}

impl wt::BlobImageRenderer for CheckerboardRenderer {
    fn add(&mut self, key: wt::ImageKey, cmds: wt::BlobImageData, _: Option<wt::TileSize>) {
        self.image_cmds.insert(key, Arc::new(deserialize_blob(&cmds[..]).unwrap()));
    }

    fn update(&mut self, key: wt::ImageKey, cmds: wt::BlobImageData) {
        // Here, updating is just replacing the current version of the commands with
        // the new one (no incremental updates).
        self.image_cmds.insert(key, Arc::new(deserialize_blob(&cmds[..]).unwrap()));
    }

    fn delete(&mut self, key: wt::ImageKey) {
        self.image_cmds.remove(&key);
    }

    fn request(&mut self,
               resources: &wt::BlobImageResources,
               request: wt::BlobImageRequest,
               descriptor: &wt::BlobImageDescriptor,
               _dirty_rect: Option<wt::DeviceUintRect>) {
        // This method is where we kick off our rendering jobs.
        // It should avoid doing work on the calling thread as much as possible.
        // In this example we will use the thread pool to render individual tiles.

        // Gather the input data to send to a worker thread.
        let cmds = Arc::clone(&self.image_cmds.get(&request.key).unwrap());
        let tx = self.tx.clone();
        let descriptor = descriptor.clone();

        self.workers.spawn_async(move || {
            let result = render_blob(cmds, &descriptor, request.tile);
            tx.send((request, result)).unwrap();
        });

        // Add None in the map of rendered images. This makes it possible to differentiate
        // between commands that aren't finished yet (entry in the map is equal to None) and
        // keys that have never been requested (entry not in the map), which would cause deadlocks
        // if we were to block upon receing their result in resolve!
        self.rendered_images.insert(request, None);
    }

    fn resolve(&mut self, request: wt::BlobImageRequest) -> wt::BlobImageResult {
        // In this method we wait until the work is complete on the worker threads and
        // gather the results.

        // First look at whether we have already received the rendered image
        // that we are looking for.
        match self.rendered_images.entry(request) {
            Entry::Vacant(_) => {
                return Err(wt::BlobImageError::InvalidKey);
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
                return result
            }
            self.rendered_images.insert(req, Some(result));
        }

        // If we break out of the loop above it means the channel closed unexpectedly.
        Err(wt::BlobImageError::Other("Channel closed".into()))
    }
    fn delete_font(&mut self, font: wt::FontKey) {}
}

fn body(api: &wt::RenderApi,
        builder: &mut wt::DisplayListBuilder,
        _pipeline_id: &wt::PipelineId,
        layout_size: &wt::LayoutSize)
{
    let blob_img1 = api.generate_image_key();
    api.add_image(
        blob_img1,
        wt::ImageDescriptor::new(500, 500, wt::ImageFormat::RGBA8, true),
        wt::ImageData::new_blob_image(serialize_blob(wt::ColorU::new(50, 50, 150, 255))),
        Some(128),
    );

    let blob_img2 = api.generate_image_key();
    api.add_image(
        blob_img2,
        wt::ImageDescriptor::new(200, 200, wt::ImageFormat::RGBA8, true),
        wt::ImageData::new_blob_image(serialize_blob(wt::ColorU::new(50, 150, 50, 255))),
        None,
    );

    let bounds = wt::LayoutRect::new(wt::LayoutPoint::zero(), *layout_size);
    builder.push_stacking_context(wt::ScrollPolicy::Scrollable,
                                  bounds,
                                  None,
                                  wt::TransformStyle::Flat,
                                  None,
                                  wt::MixBlendMode::Normal,
                                  Vec::new());

    let clip = builder.push_clip_region(&bounds, vec![], None);
    builder.push_image(
        (30, 30).by(500, 500),
        clip,
        wt::LayoutSize::new(500.0, 500.0),
        wt::LayoutSize::new(0.0, 0.0),
        wt::ImageRendering::Auto,
        blob_img1,
    );

    let clip = builder.push_clip_region(&bounds, vec![], None);
    builder.push_image(
        (600, 600).by(200, 200),
        clip,
        wt::LayoutSize::new(200.0, 200.0),
        wt::LayoutSize::new(0.0, 0.0),
        wt::ImageRendering::Auto,
        blob_img2,
    );

    builder.pop_stacking_context();
}

fn event_handler(_event: &glutin::Event,
                 _api: &wt::RenderApi)
{
}

fn main() {
    let worker_config = ThreadPoolConfig::new().thread_name(|idx|{
        format!("WebRender:Worker#{}", idx)
    });

    let workers = Arc::new(ThreadPool::new(worker_config).unwrap());

    let opts = webrender::RendererOptions {
        workers: Some(Arc::clone(&workers)),
        // Register our blob renderer, so that WebRender integrates it in the resource cache..
        // Share the same pool of worker threads between WebRender and our blob renderer.
        blob_image_renderer: Some(Box::new(CheckerboardRenderer::new(Arc::clone(&workers)))),
        .. Default::default()
    };

    boilerplate::main_wrapper(body, event_handler, Some(opts));
}
