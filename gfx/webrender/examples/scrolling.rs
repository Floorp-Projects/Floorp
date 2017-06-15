/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

extern crate gleam;
extern crate glutin;
extern crate webrender;
extern crate webrender_traits;

#[macro_use]
extern crate lazy_static;

#[path="common/boilerplate.rs"]
mod boilerplate;

use boilerplate::HandyDandyRectBuilder;
use std::sync::Mutex;
use webrender_traits::*;

fn body(_api: &RenderApi,
        builder: &mut DisplayListBuilder,
        pipeline_id: &PipelineId,
        layout_size: &LayoutSize)
{
    let bounds = LayoutRect::new(LayoutPoint::zero(), *layout_size);
    builder.push_stacking_context(webrender_traits::ScrollPolicy::Scrollable,
                                  bounds,
                                  None,
                                  TransformStyle::Flat,
                                  None,
                                  webrender_traits::MixBlendMode::Normal,
                                  Vec::new());

    if true {   // scrolling and clips stuff
        // let's make a scrollbox
        let scrollbox = (0, 0).to(300, 400);
        builder.push_stacking_context(webrender_traits::ScrollPolicy::Scrollable,
                                      LayoutRect::new(LayoutPoint::new(10.0, 10.0),
                                                      LayoutSize::zero()),
                                      None,
                                      TransformStyle::Flat,
                                      None,
                                      webrender_traits::MixBlendMode::Normal,
                                      Vec::new());
        // set the scrolling clip
        let clip = builder.push_clip_region(&scrollbox, vec![], None);
        let clip_id = builder.define_clip((0, 0).to(1000, 1000),
                                          clip,
                                          Some(ClipId::new(42, *pipeline_id)));
        builder.push_clip_id(clip_id);
        // now put some content into it.
        // start with a white background
        let clip = builder.push_clip_region(&(0, 0).to(1000, 1000), vec![], None);
        builder.push_rect((0, 0).to(500, 500),
                          clip,
                          ColorF::new(1.0, 1.0, 1.0, 1.0));
        // let's make a 50x50 blue square as a visual reference
        let clip = builder.push_clip_region(&(0, 0).to(50, 50), vec![], None);
        builder.push_rect((0, 0).to(50, 50),
                          clip,
                          ColorF::new(0.0, 0.0, 1.0, 1.0));
        // and a 50x50 green square next to it with an offset clip
        // to see what that looks like
        let clip = builder.push_clip_region(&(60, 10).to(110, 60), vec![], None);
        builder.push_rect((50, 0).to(100, 50),
                          clip,
                          ColorF::new(0.0, 1.0, 0.0, 1.0));

        // Below the above rectangles, set up a nested scrollbox. It's still in
        // the same stacking context, so note that the rects passed in need to
        // be relative to the stacking context.
        let clip = builder.push_clip_region(&(0, 100).to(200, 300), vec![], None);
        let nested_clip_id = builder.define_clip((0, 100).to(300, 400),
                                                 clip,
                                                 Some(ClipId::new(43, *pipeline_id)));
        builder.push_clip_id(nested_clip_id);
        // give it a giant gray background just to distinguish it and to easily
        // visually identify the nested scrollbox
        let clip = builder.push_clip_region(&(-1000, -1000).to(5000, 5000), vec![], None);
        builder.push_rect((-1000, -1000).to(5000, 5000),
                          clip,
                          ColorF::new(0.5, 0.5, 0.5, 1.0));
        // add a teal square to visualize the scrolling/clipping behaviour
        // as you scroll the nested scrollbox with WASD keys
        let clip = builder.push_clip_region(&(0, 100).to(50, 150), vec![], None);
        builder.push_rect((0, 100).to(50, 150),
                          clip,
                          ColorF::new(0.0, 1.0, 1.0, 1.0));
        // just for good measure add another teal square in the bottom-right
        // corner of the nested scrollframe content, which can be scrolled into
        // view by the user
        let clip = builder.push_clip_region(&(250, 350).to(300, 400), vec![], None);
        builder.push_rect((250, 350).to(300, 400),
                          clip,
                          ColorF::new(0.0, 1.0, 1.0, 1.0));
        builder.pop_clip_id(); // nested_clip_id

        builder.pop_clip_id(); // clip_id
        builder.pop_stacking_context();
    }

    builder.pop_stacking_context();
}

lazy_static! {
    static ref CURSOR_POSITION: Mutex<WorldPoint> = Mutex::new(WorldPoint::zero());
}

fn event_handler(event: &glutin::Event,
                 api: &RenderApi)
{
    match *event {
        glutin::Event::KeyboardInput(glutin::ElementState::Pressed, _, Some(key)) => {
            let offset = match key {
                 glutin::VirtualKeyCode::Down => (0.0, -10.0),
                 glutin::VirtualKeyCode::Up => (0.0, 10.0),
                 glutin::VirtualKeyCode::Right => (-10.0, 0.0),
                 glutin::VirtualKeyCode::Left => (10.0, 0.0),
                 _ => return,
            };

            api.scroll(ScrollLocation::Delta(LayoutVector2D::new(offset.0, offset.1)),
                       *CURSOR_POSITION.lock().unwrap(),
                       ScrollEventPhase::Start);
        }
        glutin::Event::MouseMoved(x, y) => {
            *CURSOR_POSITION.lock().unwrap() = WorldPoint::new(x as f32, y as f32);
        }
        glutin::Event::MouseWheel(delta, _, event_cursor_position) => {
            if let Some((x, y)) = event_cursor_position {
                *CURSOR_POSITION.lock().unwrap() = WorldPoint::new(x as f32, y as f32);
            }

            const LINE_HEIGHT: f32 = 38.0;
            let (dx, dy) = match delta {
                glutin::MouseScrollDelta::LineDelta(dx, dy) => (dx, dy * LINE_HEIGHT),
                glutin::MouseScrollDelta::PixelDelta(dx, dy) => (dx, dy),
            };

            api.scroll(ScrollLocation::Delta(LayoutVector2D::new(dx, dy)),
                       *CURSOR_POSITION.lock().unwrap(),
                       ScrollEventPhase::Start);
        }
        _ => ()
    }
}

fn main() {
    boilerplate::main_wrapper(body, event_handler, None);
}
