/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

extern crate gleam;
extern crate glutin;
extern crate webrender;

#[path="common/boilerplate.rs"]
mod boilerplate;

use boilerplate::{Example, HandyDandyRectBuilder};
use webrender::api::*;

// This example creates a 100x100 white rect and allows the user to move it
// around by using the arrow keys. It does this by using the animation API.

struct App {
    transform: LayoutTransform,
}

impl Example for App {
    fn render(&mut self,
              _api: &RenderApi,
              builder: &mut DisplayListBuilder,
              _resources: &mut ResourceUpdates,
              _layout_size: LayoutSize,
              _pipeline_id: PipelineId,
              _document_id: DocumentId) {
        // Create a 100x100 stacking context with an animatable transform property.
        // Note the magic "42" we use as the animation key. That is used to update
        // the transform in the keyboard event handler code.
        let bounds = (0,0).to(100, 100);
        builder.push_stacking_context(ScrollPolicy::Scrollable,
                                      bounds,
                                      Some(PropertyBinding::Binding(PropertyBindingKey::new(42))),
                                      TransformStyle::Flat,
                                      None,
                                      MixBlendMode::Normal,
                                      Vec::new());

        // Fill it with a white rect
        builder.push_rect(bounds, None, ColorF::new(1.0, 1.0, 1.0, 1.0));

        builder.pop_stacking_context();
    }

    fn on_event(&mut self,
                event: glutin::Event,
                api: &RenderApi,
                document_id: DocumentId) -> bool {
        match event {
            glutin::Event::KeyboardInput(glutin::ElementState::Pressed, _, Some(key)) => {
                let offset = match key {
                     glutin::VirtualKeyCode::Down => (0.0, 10.0),
                     glutin::VirtualKeyCode::Up => (0.0, -10.0),
                     glutin::VirtualKeyCode::Right => (10.0, 0.0),
                     glutin::VirtualKeyCode::Left => (-10.0, 0.0),
                     _ => return false,
                };
                // Update the transform based on the keyboard input and push it to
                // webrender using the generate_frame API. This will recomposite with
                // the updated transform.
                let new_transform = self.transform.post_translate(LayoutVector3D::new(offset.0, offset.1, 0.0));
                api.generate_frame(document_id, Some(DynamicProperties {
                    transforms: vec![
                      PropertyValue {
                        key: PropertyBindingKey::new(42),
                        value: new_transform,
                      },
                    ],
                    floats: vec![],
                }));
                self.transform = new_transform;
            }
            _ => ()
        }

        false
    }
}

fn main() {
    let mut app = App {
        transform: LayoutTransform::identity(),
    };
    boilerplate::main_wrapper(&mut app, None);
}
