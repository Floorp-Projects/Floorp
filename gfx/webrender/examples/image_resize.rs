/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

extern crate gleam;
extern crate glutin;
extern crate webrender;

#[path = "common/boilerplate.rs"]
mod boilerplate;
#[path = "common/image_helper.rs"]
mod image_helper;

use boilerplate::{Example, HandyDandyRectBuilder};
use webrender::api::*;

struct App {
    image_key: ImageKey,
}

impl Example for App {
    fn render(
        &mut self,
        _api: &RenderApi,
        builder: &mut DisplayListBuilder,
        resources: &mut ResourceUpdates,
        _framebuffer_size: DeviceUintSize,
        _pipeline_id: PipelineId,
        _document_id: DocumentId,
    ) {
        let (image_descriptor, image_data) = image_helper::make_checkerboard(32, 32);
        resources.add_image(
            self.image_key,
            image_descriptor,
            image_data,
            None,
        );

        let bounds = (0, 0).to(512, 512);
        let info = LayoutPrimitiveInfo::new(bounds);
        builder.push_stacking_context(
            &info,
            ScrollPolicy::Scrollable,
            None,
            TransformStyle::Flat,
            None,
            MixBlendMode::Normal,
            Vec::new(),
        );

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
            self.image_key,
        );

        let info = LayoutPrimitiveInfo::with_clip_rect(
            LayoutRect::new(LayoutPoint::new(250.0, 100.0), image_size),
            bounds,
        );
        builder.push_image(
            &info,
            image_size,
            LayoutSize::zero(),
            ImageRendering::Pixelated,
            AlphaType::PremultipliedAlpha,
            self.image_key,
        );

        builder.pop_stacking_context();
    }

    fn on_event(&mut self, event: glutin::Event, api: &RenderApi, document_id: DocumentId) -> bool {
        match event {
            glutin::Event::KeyboardInput(glutin::ElementState::Pressed, _, Some(key)) => {
                match key {
                    glutin::VirtualKeyCode::Space => {
                        let mut image_data = Vec::new();
                        for y in 0 .. 64 {
                            for x in 0 .. 64 {
                                let r = 255 * ((y & 32) == 0) as u8;
                                let g = 255 * ((x & 32) == 0) as u8;
                                image_data.extend_from_slice(&[0, g, r, 0xff]);
                            }
                        }

                        let mut updates = ResourceUpdates::new();
                        updates.update_image(
                            self.image_key,
                            ImageDescriptor::new(64, 64, ImageFormat::BGRA8, true),
                            ImageData::new(image_data),
                            None,
                        );
                        let mut txn = Transaction::new();
                        txn.update_resources(updates);
                        txn.generate_frame();
                        api.send_transaction(document_id, txn);
                    }
                    _ => {}
                }
            }
            _ => {}
        }

        false
    }
}

fn main() {
    let mut app = App {
        image_key: ImageKey(IdNamespace(0), 0),
    };
    boilerplate::main_wrapper(&mut app, None);
}
