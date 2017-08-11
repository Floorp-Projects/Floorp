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

struct App {
    cursor_position: WorldPoint,
}

impl Example for App {
    fn render(&mut self,
              _api: &RenderApi,
              builder: &mut DisplayListBuilder,
              _resources: &mut ResourceUpdates,
              layout_size: LayoutSize,
              _pipeline_id: PipelineId,
              _document_id: DocumentId) {
        let bounds = LayoutRect::new(LayoutPoint::zero(), layout_size);
        builder.push_stacking_context(ScrollPolicy::Scrollable,
                                      bounds,
                                      None,
                                      TransformStyle::Flat,
                                      None,
                                      MixBlendMode::Normal,
                                      Vec::new());

        if true {   // scrolling and clips stuff
            // let's make a scrollbox
            let scrollbox = (0, 0).to(300, 400);
            builder.push_stacking_context(ScrollPolicy::Scrollable,
                                          LayoutRect::new(LayoutPoint::new(10.0, 10.0),
                                                          LayoutSize::zero()),
                                          None,
                                          TransformStyle::Flat,
                                          None,
                                          MixBlendMode::Normal,
                                          Vec::new());
            // set the scrolling clip
            let clip_id = builder.define_scroll_frame(None,
                                                      (0, 0).by(1000, 1000),
                                                      scrollbox,
                                                      vec![],
                                                      None,
                                                      ScrollSensitivity::ScriptAndInputEvents);
            builder.push_clip_id(clip_id);

            // now put some content into it.
            // start with a white background
            builder.push_rect((0, 0).to(1000, 1000), None, ColorF::new(1.0, 1.0, 1.0, 1.0));

            // let's make a 50x50 blue square as a visual reference
            builder.push_rect((0, 0).to(50, 50), None, ColorF::new(0.0, 0.0, 1.0, 1.0));

            // and a 50x50 green square next to it with an offset clip
            // to see what that looks like
            builder.push_rect((50, 0).to(100, 50),
                              Some(LocalClip::from((60, 10).to(110, 60))),
                              ColorF::new(0.0, 1.0, 0.0, 1.0));

            // Below the above rectangles, set up a nested scrollbox. It's still in
            // the same stacking context, so note that the rects passed in need to
            // be relative to the stacking context.
            let nested_clip_id = builder.define_scroll_frame(None,
                                                             (0, 100).to(300, 400),
                                                             (0, 100).to(200, 300),
                                                             vec![],
                                                             None,
                                                             ScrollSensitivity::ScriptAndInputEvents);
            builder.push_clip_id(nested_clip_id);

            // give it a giant gray background just to distinguish it and to easily
            // visually identify the nested scrollbox
            builder.push_rect((-1000, -1000).to(5000, 5000), None, ColorF::new(0.5, 0.5, 0.5, 1.0));

            // add a teal square to visualize the scrolling/clipping behaviour
            // as you scroll the nested scrollbox with WASD keys
            builder.push_rect((0, 100).to(50, 150), None, ColorF::new(0.0, 1.0, 1.0, 1.0));

            // just for good measure add another teal square in the bottom-right
            // corner of the nested scrollframe content, which can be scrolled into
            // view by the user
            builder.push_rect((250, 350).to(300, 400), None, ColorF::new(0.0, 1.0, 1.0, 1.0));

            builder.pop_clip_id(); // nested_clip_id

            builder.pop_clip_id(); // clip_id
            builder.pop_stacking_context();
        }

        builder.pop_stacking_context();
    }

    fn on_event(&mut self,
                event: glutin::Event,
                api: &RenderApi,
                document_id: DocumentId) -> bool {
        match event {
            glutin::Event::KeyboardInput(glutin::ElementState::Pressed, _, Some(key)) => {
                let offset = match key {
                     glutin::VirtualKeyCode::Down => (0.0, -10.0),
                     glutin::VirtualKeyCode::Up => (0.0, 10.0),
                     glutin::VirtualKeyCode::Right => (-10.0, 0.0),
                     glutin::VirtualKeyCode::Left => (10.0, 0.0),
                     _ => return false,
                };

                api.scroll(document_id,
                           ScrollLocation::Delta(LayoutVector2D::new(offset.0, offset.1)),
                           self.cursor_position,
                           ScrollEventPhase::Start);
            }
            glutin::Event::MouseMoved(x, y) => {
                self.cursor_position = WorldPoint::new(x as f32, y as f32);
            }
            glutin::Event::MouseWheel(delta, _, event_cursor_position) => {
                if let Some((x, y)) = event_cursor_position {
                    self.cursor_position = WorldPoint::new(x as f32, y as f32);
                }

                const LINE_HEIGHT: f32 = 38.0;
                let (dx, dy) = match delta {
                    glutin::MouseScrollDelta::LineDelta(dx, dy) => (dx, dy * LINE_HEIGHT),
                    glutin::MouseScrollDelta::PixelDelta(dx, dy) => (dx, dy),
                };

                api.scroll(document_id,
                           ScrollLocation::Delta(LayoutVector2D::new(dx, dy)),
                           self.cursor_position,
                           ScrollEventPhase::Start);
            }
            _ => ()
        }

        false
    }
}

fn main() {
    let mut app = App {
        cursor_position: WorldPoint::zero(),
    };
    boilerplate::main_wrapper(&mut app, None);
}
