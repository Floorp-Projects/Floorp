/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

extern crate gleam;
extern crate glutin;
extern crate webrender;

#[path = "common/boilerplate.rs"]
mod boilerplate;

use boilerplate::{Example, HandyDandyRectBuilder};
use gleam::gl;
use std::mem;
use webrender::api::*;

struct ImageGenerator {
    patterns: [[u8; 3]; 6],
    next_pattern: usize,
    current_image: Vec<u8>,
}

impl ImageGenerator {
    fn new() -> Self {
        ImageGenerator {
            next_pattern: 0,
            patterns: [
                [1, 0, 0],
                [0, 1, 0],
                [0, 0, 1],
                [1, 1, 0],
                [0, 1, 1],
                [1, 0, 1],
            ],
            current_image: Vec::new(),
        }
    }

    fn generate_image(&mut self, size: u32) {
        let pattern = &self.patterns[self.next_pattern];
        self.current_image.clear();
        for y in 0 .. size {
            for x in 0 .. size {
                let lum = 255 * (1 - (((x & 8) == 0) ^ ((y & 8) == 0)) as u8);
                self.current_image.extend_from_slice(&[
                    lum * pattern[0],
                    lum * pattern[1],
                    lum * pattern[2],
                    0xff,
                ]);
            }
        }

        self.next_pattern = (self.next_pattern + 1) % self.patterns.len();
    }

    fn take(&mut self) -> Vec<u8> {
        mem::replace(&mut self.current_image, Vec::new())
    }
}

impl webrender::ExternalImageHandler for ImageGenerator {
    fn lock(&mut self, _key: ExternalImageId, channel_index: u8) -> webrender::ExternalImage {
        self.generate_image(channel_index as u32);
        webrender::ExternalImage {
            uv: TexelRect::new(0.0, 0.0, 1.0, 1.0),
            source: webrender::ExternalImageSource::RawData(&self.current_image),
        }
    }
    fn unlock(&mut self, _key: ExternalImageId, _channel_index: u8) {}
}

struct App {
    stress_keys: Vec<ImageKey>,
    image_key: Option<ImageKey>,
    image_generator: ImageGenerator,
    swap_keys: Vec<ImageKey>,
    swap_index: usize,
}

impl Example for App {
    fn render(
        &mut self,
        api: &RenderApi,
        builder: &mut DisplayListBuilder,
        resources: &mut ResourceUpdates,
        _framebuffer_size: DeviceUintSize,
        _pipeline_id: PipelineId,
        _document_id: DocumentId,
    ) {
        let bounds = (0, 0).to(512, 512);
        let info = LayoutPrimitiveInfo::new(bounds);
        builder.push_stacking_context(
            &info,
            None,
            ScrollPolicy::Scrollable,
            None,
            TransformStyle::Flat,
            None,
            MixBlendMode::Normal,
            Vec::new(),
            GlyphRasterSpace::Screen,
        );

        let x0 = 50.0;
        let y0 = 50.0;
        let image_size = LayoutSize::new(4.0, 4.0);

        if self.swap_keys.is_empty() {
            let key0 = api.generate_image_key();
            let key1 = api.generate_image_key();

            self.image_generator.generate_image(128);
            resources.add_image(
                key0,
                ImageDescriptor::new(128, 128, ImageFormat::BGRA8, true, false),
                ImageData::new(self.image_generator.take()),
                None,
            );

            self.image_generator.generate_image(128);
            resources.add_image(
                key1,
                ImageDescriptor::new(128, 128, ImageFormat::BGRA8, true, false),
                ImageData::new(self.image_generator.take()),
                None,
            );

            self.swap_keys.push(key0);
            self.swap_keys.push(key1);
        }

        for (i, key) in self.stress_keys.iter().enumerate() {
            let x = (i % 128) as f32;
            let y = (i / 128) as f32;
            let info = LayoutPrimitiveInfo::with_clip_rect(
                LayoutRect::new(
                    LayoutPoint::new(x0 + image_size.width * x, y0 + image_size.height * y),
                    image_size,
                ),
                bounds,
            );

            builder.push_image(
                &info,
                image_size,
                LayoutSize::zero(),
                ImageRendering::Auto,
                AlphaType::PremultipliedAlpha,
                *key,
            );
        }

        if let Some(image_key) = self.image_key {
            let image_size = LayoutSize::new(100.0, 100.0);
            let info = LayoutPrimitiveInfo::with_clip_rect(
                LayoutRect::new(LayoutPoint::new(100.0, 100.0), image_size),
                bounds,
            );
            builder.push_image(
                &info,
                image_size,
                LayoutSize::zero(),
                ImageRendering::Auto,
                AlphaType::PremultipliedAlpha,
                image_key,
            );
        }

        let swap_key = self.swap_keys[self.swap_index];
        let image_size = LayoutSize::new(64.0, 64.0);
        let info = LayoutPrimitiveInfo::with_clip_rect(
            LayoutRect::new(LayoutPoint::new(100.0, 400.0), image_size),
            bounds,
        );
        builder.push_image(
            &info,
            image_size,
            LayoutSize::zero(),
            ImageRendering::Auto,
            AlphaType::PremultipliedAlpha,
            swap_key,
        );
        self.swap_index = 1 - self.swap_index;

        builder.pop_stacking_context();
    }

    fn on_event(
        &mut self,
        event: glutin::WindowEvent,
        api: &RenderApi,
        _document_id: DocumentId,
    ) -> bool {
        match event {
            glutin::WindowEvent::KeyboardInput {
                input: glutin::KeyboardInput {
                    state: glutin::ElementState::Pressed,
                    virtual_keycode: Some(key),
                    ..
                },
                ..
            } => {
                let mut updates = ResourceUpdates::new();

                match key {
                    glutin::VirtualKeyCode::S => {
                        self.stress_keys.clear();

                        for _ in 0 .. 16 {
                            for _ in 0 .. 16 {
                                let size = 4;

                                let image_key = api.generate_image_key();

                                self.image_generator.generate_image(size);

                                updates.add_image(
                                    image_key,
                                    ImageDescriptor::new(size, size, ImageFormat::BGRA8, true, false),
                                    ImageData::new(self.image_generator.take()),
                                    None,
                                );

                                self.stress_keys.push(image_key);
                            }
                        }
                    }
                    glutin::VirtualKeyCode::D => if let Some(image_key) = self.image_key.take() {
                        updates.delete_image(image_key);
                    },
                    glutin::VirtualKeyCode::U => if let Some(image_key) = self.image_key {
                        let size = 128;
                        self.image_generator.generate_image(size);

                        updates.update_image(
                            image_key,
                            ImageDescriptor::new(size, size, ImageFormat::BGRA8, true, false),
                            ImageData::new(self.image_generator.take()),
                            None,
                        );
                    },
                    glutin::VirtualKeyCode::E => {
                        if let Some(image_key) = self.image_key.take() {
                            updates.delete_image(image_key);
                        }

                        let size = 32;
                        let image_key = api.generate_image_key();

                        let image_data = ExternalImageData {
                            id: ExternalImageId(0),
                            channel_index: size as u8,
                            image_type: ExternalImageType::Buffer,
                        };

                        updates.add_image(
                            image_key,
                            ImageDescriptor::new(size, size, ImageFormat::BGRA8, true, false),
                            ImageData::External(image_data),
                            None,
                        );

                        self.image_key = Some(image_key);
                    }
                    glutin::VirtualKeyCode::R => {
                        if let Some(image_key) = self.image_key.take() {
                            updates.delete_image(image_key);
                        }

                        let image_key = api.generate_image_key();
                        let size = 32;
                        self.image_generator.generate_image(size);

                        updates.add_image(
                            image_key,
                            ImageDescriptor::new(size, size, ImageFormat::BGRA8, true, false),
                            ImageData::new(self.image_generator.take()),
                            None,
                        );

                        self.image_key = Some(image_key);
                    }
                    _ => {}
                }

                api.update_resources(updates);
                return true;
            }
            _ => {}
        }

        false
    }

    fn get_image_handlers(
        &mut self,
        _gl: &gl::Gl,
    ) -> (Option<Box<webrender::ExternalImageHandler>>,
          Option<Box<webrender::OutputImageHandler>>) {
        (Some(Box::new(ImageGenerator::new())), None)
    }
}

fn main() {
    let mut app = App {
        image_key: None,
        stress_keys: Vec::new(),
        image_generator: ImageGenerator::new(),
        swap_keys: Vec::new(),
        swap_index: 0,
    };
    boilerplate::main_wrapper(&mut app, None);
}
