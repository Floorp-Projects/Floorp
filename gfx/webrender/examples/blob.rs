/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

extern crate app_units;
extern crate euclid;
extern crate gleam;
extern crate glutin;
extern crate webrender;
extern crate webrender_traits;

use gleam::gl;
use std::collections::HashMap;
use webrender_traits::{BlobImageData, BlobImageDescriptor, BlobImageError, BlobImageRenderer, BlobImageRequest};
use webrender_traits::{BlobImageResult, ImageStore, ClipRegion, ColorF, ColorU, Epoch};
use webrender_traits::{DeviceUintSize, DeviceUintRect, LayoutPoint, LayoutRect, LayoutSize};
use webrender_traits::{ImageData, ImageDescriptor, ImageFormat, ImageRendering, ImageKey, TileSize};
use webrender_traits::{PipelineId, RasterizedBlobImage, TransformStyle};

// This example shows how to implement a very basic BlobImageRenderer that can only render
// a checkerboard pattern.

// The deserialized command list internally used by this example is just a color.
type ImageRenderingCommands = ColorU;

// Serialize/deserialze the blob.
// Ror real usecases you should probably use serde rather than doing it by hand.

fn serialize_blob(color: ColorU) -> Vec<u8> {
    vec![color.r, color.g, color.b, color.a]
}

fn deserialize_blob(blob: &[u8]) -> Result<ImageRenderingCommands, ()> {
    let mut iter = blob.iter();
    return match (iter.next(), iter.next(), iter.next(), iter.next()) {
        (Some(&r), Some(&g), Some(&b), Some(&a)) => Ok(ColorU::new(r, g, b, a)),
        (Some(&a), None, None, None) => Ok(ColorU::new(a, a, a, a)),
        _ => Err(()),
    }
}

struct CheckerboardRenderer {
    // The deserialized drawing commands.
    image_cmds: HashMap<ImageKey, ImageRenderingCommands>,

    // The images rendered in the current frame (not kept here between frames)
    rendered_images: HashMap<BlobImageRequest, BlobImageResult>,
}

impl CheckerboardRenderer {
    fn new() -> Self {
        CheckerboardRenderer {
            image_cmds: HashMap::new(),
            rendered_images: HashMap::new(),
        }
    }
}

impl BlobImageRenderer for CheckerboardRenderer {
    fn add(&mut self, key: ImageKey, cmds: BlobImageData, _: Option<TileSize>) {
        self.image_cmds.insert(key, deserialize_blob(&cmds[..]).unwrap());
    }

    fn update(&mut self, key: ImageKey, cmds: BlobImageData) {
        // Here, updating is just replacing the current version of the commands with
        // the new one (no incremental updates)
        self.image_cmds.insert(key, deserialize_blob(&cmds[..]).unwrap());
    }

    fn delete(&mut self, key: ImageKey) {
        self.image_cmds.remove(&key);
    }

    fn request(&mut self,
               request: BlobImageRequest,
               descriptor: &BlobImageDescriptor,
               _dirty_rect: Option<DeviceUintRect>,
               _images: &ImageStore) {
        let color = self.image_cmds.get(&request.key).unwrap().clone();

        // Allocate storage for the result. Right now the resource cache expects the
        // tiles to have have no stride or offset.
        let mut texels = Vec::with_capacity((descriptor.width * descriptor.height * 4) as usize);

        // Generate a per-tile pattern to see it in the demo. For a real use case it would not
        // make sense for the rendered content to depend on its tile.
        let tile_checker = match request.tile {
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
                    ImageFormat::RGBA8 => {
                        texels.push(color.b * checker + tc);
                        texels.push(color.g * checker + tc);
                        texels.push(color.r * checker + tc);
                        texels.push(color.a * checker + tc);
                    }
                    ImageFormat::A8 => {
                        texels.push(color.a * checker + tc);
                    }
                    _ => {
                        self.rendered_images.insert(request,
                            Err(BlobImageError::Other(format!(
                                "Usupported image format {:?}",
                                descriptor.format
                            )))
                        );
                        return;
                    }
                }
            }
        }

        self.rendered_images.insert(request, Ok(RasterizedBlobImage {
            data: texels,
            width: descriptor.width,
            height: descriptor.height,
        }));
    }

    fn resolve(&mut self, request: BlobImageRequest) -> BlobImageResult {
        self.rendered_images.remove(&request).unwrap_or(Err(BlobImageError::InvalidKey))
    }
}

fn main() {
    let window = glutin::WindowBuilder::new()
                .with_title("WebRender Sample (BlobImageRenderer)")
                .with_multitouch()
                .with_gl(glutin::GlRequest::GlThenGles {
                    opengl_version: (3, 2),
                    opengles_version: (3, 0)
                })
                .build()
                .unwrap();

    unsafe {
        window.make_current().ok();
    }

    let gl = match gl::GlType::default() {
        gl::GlType::Gl => unsafe { gl::GlFns::load_with(|symbol| window.get_proc_address(symbol) as *const _) },
        gl::GlType::Gles => unsafe { gl::GlesFns::load_with(|symbol| window.get_proc_address(symbol) as *const _) },
    };

    println!("OpenGL version {}", gl.get_string(gl::VERSION));

    let (width, height) = window.get_inner_size_pixels().unwrap();

    let opts = webrender::RendererOptions {
        debug: true,
        blob_image_renderer: Some(Box::new(CheckerboardRenderer::new())),
        device_pixel_ratio: window.hidpi_factor(),
        .. Default::default()
    };

    let size = DeviceUintSize::new(width, height);
    let (mut renderer, sender) = webrender::renderer::Renderer::new(gl, opts, size).unwrap();
    let api = sender.create_api();

    let notifier = Box::new(Notifier::new(window.create_window_proxy()));
    renderer.set_render_notifier(notifier);

    let epoch = Epoch(0);
    let root_background_color = ColorF::new(0.2, 0.2, 0.2, 1.0);

    let blob_img1 = api.generate_image_key();
    api.add_image(
        blob_img1,
        ImageDescriptor::new(500, 500, ImageFormat::RGBA8, true),
        ImageData::new_blob_image(serialize_blob(ColorU::new(50, 50, 150, 255))),
        Some(128),
    );

    let blob_img2 = api.generate_image_key();
    api.add_image(
        blob_img2,
        ImageDescriptor::new(200, 200, ImageFormat::RGBA8, true),
        ImageData::new_blob_image(serialize_blob(ColorU::new(50, 150, 50, 255))),
        None,
    );

    let pipeline_id = PipelineId(0, 0);
    let mut builder = webrender_traits::DisplayListBuilder::new(pipeline_id);

    let bounds = LayoutRect::new(LayoutPoint::zero(), LayoutSize::new(width as f32, height as f32));
    builder.push_stacking_context(webrender_traits::ScrollPolicy::Scrollable,
                                  bounds,
                                  None,
                                  TransformStyle::Flat,
                                  None,
                                  webrender_traits::MixBlendMode::Normal,
                                  Vec::new());
    builder.push_image(
        LayoutRect::new(LayoutPoint::new(30.0, 30.0), LayoutSize::new(500.0, 500.0)),
        ClipRegion::simple(&bounds),
        LayoutSize::new(500.0, 500.0),
        LayoutSize::new(0.0, 0.0),
        ImageRendering::Auto,
        blob_img1,
    );

    builder.push_image(
        LayoutRect::new(LayoutPoint::new(600.0, 60.0), LayoutSize::new(200.0, 200.0)),
        ClipRegion::simple(&bounds),
        LayoutSize::new(200.0, 200.0),
        LayoutSize::new(0.0, 0.0),
        ImageRendering::Auto,
        blob_img2,
    );

    builder.pop_stacking_context();

    api.set_display_list(
        Some(root_background_color),
        epoch,
        LayoutSize::new(width as f32, height as f32),
        builder.finalize(),
        true);
    api.set_root_pipeline(pipeline_id);
    api.generate_frame(None);

    'outer: for event in window.wait_events() {
        let mut events = Vec::new();
        events.push(event);

        for event in window.poll_events() {
            events.push(event);
        }

        for event in events {
            match event {
                glutin::Event::Closed |
                glutin::Event::KeyboardInput(_, _, Some(glutin::VirtualKeyCode::Escape)) |
                glutin::Event::KeyboardInput(_, _, Some(glutin::VirtualKeyCode::Q)) => break 'outer,
                glutin::Event::KeyboardInput(glutin::ElementState::Pressed,
                                             _, Some(glutin::VirtualKeyCode::P)) => {
                    let enable_profiler = !renderer.get_profiler_enabled();
                    renderer.set_profiler_enabled(enable_profiler);
                    api.generate_frame(None);
                }
                _ => ()
            }
        }

        renderer.update();
        renderer.render(DeviceUintSize::new(width, height));
        window.swap_buffers().ok();
    }
}

struct Notifier {
    window_proxy: glutin::WindowProxy,
}

impl Notifier {
    fn new(window_proxy: glutin::WindowProxy) -> Notifier {
        Notifier {
            window_proxy: window_proxy,
        }
    }
}

impl webrender_traits::RenderNotifier for Notifier {
    fn new_frame_ready(&mut self) {
        #[cfg(not(target_os = "android"))]
        self.window_proxy.wakeup_event_loop();
    }

    fn new_scroll_frame_ready(&mut self, _composite_needed: bool) {
        #[cfg(not(target_os = "android"))]
        self.window_proxy.wakeup_event_loop();
    }
}
