/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! This example creates a 200x200 white rect and allows the user to move it
//! around by using the arrow keys and rotate with '<'/'>'.
//! It does this by using the animation API.

//! The example also features seamless opaque/transparent split of a
//! rounded cornered rectangle, which is done automatically during the
//! scene building for render optimization.

extern crate euclid;
extern crate gleam;
extern crate glutin;
extern crate webrender;

#[path = "common/boilerplate.rs"]
mod boilerplate;

use boilerplate::{Example, HandyDandyRectBuilder};
use euclid::Angle;
use webrender::api::*;

struct App {
    property_key: PropertyBindingKey<LayoutTransform>,
    opacity_key: PropertyBindingKey<f32>,
    transform: LayoutTransform,
    opacity: f32,
}

impl Example for App {
    fn render(
        &mut self,
        _api: &RenderApi,
        builder: &mut DisplayListBuilder,
        _resources: &mut ResourceUpdates,
        _framebuffer_size: DeviceUintSize,
        _pipeline_id: PipelineId,
        _document_id: DocumentId,
    ) {
        // Create a 200x200 stacking context with an animated transform property.
        let bounds = (0, 0).to(200, 200);

        let filters = vec![
            FilterOp::Opacity(PropertyBinding::Binding(self.opacity_key), self.opacity),
        ];

        let info = LayoutPrimitiveInfo::new(bounds);
        builder.push_stacking_context(
            &info,
            None,
            Some(PropertyBinding::Binding(self.property_key)),
            TransformStyle::Flat,
            None,
            MixBlendMode::Normal,
            filters,
            GlyphRasterSpace::Screen,
        );

        let complex_clip = ComplexClipRegion {
            rect: bounds,
            radii: BorderRadius::uniform(50.0),
            mode: ClipMode::Clip,
        };
        let clip_id = builder.define_clip(bounds, vec![complex_clip], None);
        builder.push_clip_id(clip_id);

        // Fill it with a white rect
        builder.push_rect(&info, ColorF::new(0.0, 1.0, 0.0, 1.0));

        builder.pop_clip_id();

        builder.pop_stacking_context();
    }

    fn on_event(&mut self, win_event: glutin::WindowEvent, api: &RenderApi, document_id: DocumentId) -> bool {
        match win_event {
            glutin::WindowEvent::KeyboardInput {
                input: glutin::KeyboardInput {
                    state: glutin::ElementState::Pressed,
                    virtual_keycode: Some(key),
                    ..
                },
                ..
            } => {
                let (offset_x, offset_y, angle, delta_opacity) = match key {
                    glutin::VirtualKeyCode::Down => (0.0, 10.0, 0.0, 0.0),
                    glutin::VirtualKeyCode::Up => (0.0, -10.0, 0.0, 0.0),
                    glutin::VirtualKeyCode::Right => (10.0, 0.0, 0.0, 0.0),
                    glutin::VirtualKeyCode::Left => (-10.0, 0.0, 0.0, 0.0),
                    glutin::VirtualKeyCode::Comma => (0.0, 0.0, 0.1, 0.0),
                    glutin::VirtualKeyCode::Period => (0.0, 0.0, -0.1, 0.0),
                    glutin::VirtualKeyCode::Z => (0.0, 0.0, 0.0, -0.1),
                    glutin::VirtualKeyCode::X => (0.0, 0.0, 0.0, 0.1),
                    _ => return false,
                };
                // Update the transform based on the keyboard input and push it to
                // webrender using the generate_frame API. This will recomposite with
                // the updated transform.
                self.opacity += delta_opacity;
                let new_transform = self.transform
                    .pre_rotate(0.0, 0.0, 1.0, Angle::radians(angle))
                    .post_translate(LayoutVector3D::new(offset_x, offset_y, 0.0));
                let mut txn = Transaction::new();
                txn.update_dynamic_properties(
                    DynamicProperties {
                        transforms: vec![
                            PropertyValue {
                                key: self.property_key,
                                value: new_transform,
                            },
                        ],
                        floats: vec![
                            PropertyValue {
                                key: self.opacity_key,
                                value: self.opacity,
                            }
                        ],
                    },
                );
                txn.generate_frame();
                api.send_transaction(document_id, txn);
                self.transform = new_transform;
            }
            _ => (),
        }

        false
    }
}

fn main() {
    let mut app = App {
        property_key: PropertyBindingKey::new(42), // arbitrary magic number
        opacity_key: PropertyBindingKey::new(43),
        transform: LayoutTransform::create_translation(0.0, 0.0, 0.0),
        opacity: 0.5,
    };
    boilerplate::main_wrapper(&mut app, None);
}
