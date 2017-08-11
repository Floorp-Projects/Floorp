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
              pipeline_id: PipelineId,
              _document_id: DocumentId) {
        let bounds = LayoutRect::new(LayoutPoint::zero(), layout_size);
        builder.push_stacking_context(ScrollPolicy::Scrollable,
                                      bounds,
                                      None,
                                      TransformStyle::Flat,
                                      None,
                                      MixBlendMode::Normal,
                                      Vec::new());

        let outer_scroll_frame_rect = (100, 100).to(600, 400);
        builder.push_rect(outer_scroll_frame_rect, None, ColorF::new(1.0, 1.0, 1.0, 1.0));

        let nested_clip_id = builder.define_scroll_frame(None,
                                                         (100, 100).to(1000, 1000),
                                                         outer_scroll_frame_rect,
                                                         vec![],
                                                         None,
                                                         ScrollSensitivity::ScriptAndInputEvents);
        builder.push_clip_id(nested_clip_id);

        let mut builder2 = DisplayListBuilder::new(pipeline_id, layout_size);
        let mut builder3 = DisplayListBuilder::new(pipeline_id, layout_size);

        let rect = (110, 110).to(210, 210);
        builder3.push_rect(rect, None, ColorF::new(0.0, 1.0, 0.0, 1.0));

        // A fixed position rectangle should be fixed to the reference frame that starts
        // in the outer display list.
        builder3.push_stacking_context(ScrollPolicy::Fixed,
                                      (220, 110).to(320, 210),
                                      None,
                                      TransformStyle::Flat,
                                      None,
                                      MixBlendMode::Normal,
                                      Vec::new());
        let rect = (0, 0).to(100, 100);
        builder3.push_rect(rect, None, ColorF::new(0.0, 1.0, 0.0, 1.0));
        builder3.pop_stacking_context();

        // Now we push an inner scroll frame that should have the same id as the outer one,
        // but the WebRender nested display list replacement code should convert it into
        // a unique ClipId.
        let inner_scroll_frame_rect = (330, 110).to(530, 360);
        builder3.push_rect(inner_scroll_frame_rect, None, ColorF::new(1.0, 0.0, 1.0, 0.5));
        let inner_nested_clip_id =
            builder3.define_scroll_frame(None,
                                         (330, 110).to(2000, 2000),
                                         inner_scroll_frame_rect,
                                         vec![],
                                         None,
                                         ScrollSensitivity::ScriptAndInputEvents);
        builder3.push_clip_id(inner_nested_clip_id);
        let rect = (340, 120).to(440, 220);
        builder3.push_rect(rect, None, ColorF::new(0.0, 1.0, 0.0, 1.0));
        builder3.pop_clip_id();

        let (_, _, built_list) = builder3.finalize();
        builder2.push_nested_display_list(&built_list);
        let (_, _, built_list) = builder2.finalize();
        builder.push_nested_display_list(&built_list);

        builder.pop_clip_id();

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
