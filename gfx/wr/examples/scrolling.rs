/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

extern crate euclid;
extern crate gleam;
extern crate glutin;
extern crate webrender;
extern crate winit;

#[path = "common/boilerplate.rs"]
mod boilerplate;

use crate::boilerplate::{Example, HandyDandyRectBuilder};
use euclid::SideOffsets2D;
use webrender::api::*;
use webrender::render_api::*;
use webrender::api::units::*;
use winit::dpi::LogicalPosition;


const EXT_SCROLL_ID_ROOT: u64 = 1;
const EXT_SCROLL_ID_CONTENT: u64 = 2;

struct App {
    cursor_position: WorldPoint,
    scroll_offset: LayoutVector2D,
}

impl Example for App {
    fn render(
        &mut self,
        _api: &mut RenderApi,
        builder: &mut DisplayListBuilder,
        _txn: &mut Transaction,
        _device_size: DeviceIntSize,
        pipeline_id: PipelineId,
        _document_id: DocumentId,
    ) {
        let root_space_and_clip = SpaceAndClipInfo::root_scroll(pipeline_id);
        builder.push_simple_stacking_context(
            LayoutPoint::zero(),
            root_space_and_clip.spatial_id,
            PrimitiveFlags::IS_BACKFACE_VISIBLE,
        );

        if true {
            // scrolling and clips stuff
            // let's make a scrollbox
            let scrollbox = (0, 0).to(300, 400);
            builder.push_simple_stacking_context(
                LayoutPoint::new(10., 10.),
                root_space_and_clip.spatial_id,
                PrimitiveFlags::IS_BACKFACE_VISIBLE,
            );
            // set the scrolling clip
            let space1 = builder.define_scroll_frame(
                root_space_and_clip.spatial_id,
                ExternalScrollId(EXT_SCROLL_ID_ROOT, PipelineId::dummy()),
                (0, 0).by(1000, 1000),
                scrollbox,
                LayoutVector2D::zero(),
                APZScrollGeneration::default(),
                HasScrollLinkedEffect::No,
                SpatialTreeItemKey::new(0, 0),
            );
            let space_and_clip1 = SpaceAndClipInfo {
                spatial_id: space1,
                clip_chain_id: root_space_and_clip.clip_chain_id,
            };

            // now put some content into it.
            // start with a white background
            let info = CommonItemProperties::new((0, 0).to(1000, 1000), space_and_clip1);
            builder.push_hit_test(
                info.clip_rect,
                ClipChainId::INVALID,
                info.spatial_id,
                info.flags,
                (0, 1)
            );
            builder.push_rect(&info, info.clip_rect, ColorF::new(1.0, 1.0, 1.0, 1.0));

            // let's make a 50x50 blue square as a visual reference
            let info = CommonItemProperties::new((0, 0).to(50, 50), space_and_clip1);
            builder.push_hit_test(
                info.clip_rect,
                ClipChainId::INVALID,
                info.spatial_id,
                info.flags,
                (0, 2)
            );
            builder.push_rect(&info, info.clip_rect, ColorF::new(0.0, 0.0, 1.0, 1.0));

            // and a 50x50 green square next to it with an offset clip
            // to see what that looks like
            let info = CommonItemProperties::new(
                (50, 0).to(100, 50).intersection(&(60, 10).to(110, 60)).unwrap(),
                space_and_clip1,
            );
            builder.push_hit_test(
                info.clip_rect,
                ClipChainId::INVALID,
                info.spatial_id,
                info.flags,
                (0, 3)
            );
            builder.push_rect(&info, info.clip_rect, ColorF::new(0.0, 1.0, 0.0, 1.0));

            // Below the above rectangles, set up a nested scrollbox. It's still in
            // the same stacking context, so note that the rects passed in need to
            // be relative to the stacking context.
            let space2 = builder.define_scroll_frame(
                space1,
                ExternalScrollId(EXT_SCROLL_ID_CONTENT, PipelineId::dummy()),
                (0, 100).to(300, 1000),
                (0, 100).to(200, 300),
                LayoutVector2D::zero(),
                APZScrollGeneration::default(),
                HasScrollLinkedEffect::No,
                SpatialTreeItemKey::new(0, 1),
            );
            let space_and_clip2 = SpaceAndClipInfo {
                spatial_id: space2,
                clip_chain_id: root_space_and_clip.clip_chain_id,
            };

            // give it a giant gray background just to distinguish it and to easily
            // visually identify the nested scrollbox
            let info = CommonItemProperties::new(
                (-1000, -1000).to(5000, 5000),
                space_and_clip2,
            );
            builder.push_hit_test(
                info.clip_rect,
                ClipChainId::INVALID,
                info.spatial_id,
                info.flags,
                (0, 4)
            );
            builder.push_rect(&info, info.clip_rect, ColorF::new(0.5, 0.5, 0.5, 1.0));

            // add a teal square to visualize the scrolling/clipping behaviour
            // as you scroll the nested scrollbox
            let info = CommonItemProperties::new((0, 200).to(50, 250), space_and_clip2);
            builder.push_hit_test(
                info.clip_rect,
                ClipChainId::INVALID,
                info.spatial_id,
                info.flags,
                (0, 5)
            );
            builder.push_rect(&info, info.clip_rect, ColorF::new(0.0, 1.0, 1.0, 1.0));

            // Add a sticky frame. It will "stick" twice while scrolling, once
            // at a margin of 10px from the bottom, for 40 pixels of scrolling,
            // and once at a margin of 10px from the top, for 60 pixels of
            // scrolling.
            let sticky_id = builder.define_sticky_frame(
                space_and_clip2.spatial_id,
                (50, 350).by(50, 50),
                SideOffsets2D::new(Some(10.0), None, Some(10.0), None),
                StickyOffsetBounds::new(-40.0, 60.0),
                StickyOffsetBounds::new(0.0, 0.0),
                LayoutVector2D::new(0.0, 0.0),
                SpatialTreeItemKey::new(0, 2),
            );

            let info = CommonItemProperties::new(
                (50, 350).by(50, 50),
                SpaceAndClipInfo {
                    spatial_id: sticky_id,
                    clip_chain_id: space_and_clip2.clip_chain_id,
                },
            );
            builder.push_hit_test(
                info.clip_rect,
                ClipChainId::INVALID,
                info.spatial_id,
                info.flags,
                (0, 6)
            );
            builder.push_rect(
                &info,
                info.clip_rect,
                ColorF::new(0.5, 0.5, 1.0, 1.0),
            );

            // just for good measure add another teal square further down and to
            // the right, which can be scrolled into view by the user
            let info = CommonItemProperties::new(
                (250, 350).to(300, 400),
                space_and_clip2,
            );
            builder.push_hit_test(
                info.clip_rect,
                ClipChainId::INVALID,
                info.spatial_id,
                info.flags,
                (0, 7)
            );
            builder.push_rect(&info, info.clip_rect, ColorF::new(0.0, 1.0, 1.0, 1.0));

            builder.pop_stacking_context();
        }

        builder.pop_stacking_context();
    }

    fn on_event(
        &mut self,
        event: winit::event::WindowEvent,
        window: &winit::window::Window,
        api: &mut RenderApi,
        document_id: DocumentId,
    ) -> bool {
        let mut txn = Transaction::new();
        match event {
            winit::event::WindowEvent::KeyboardInput {
                input: winit::event::KeyboardInput {
                    state: winit::event::ElementState::Pressed,
                    virtual_keycode: Some(key),
                    ..
                },
                ..
            } => {
                let offset = match key {
                    winit::event::VirtualKeyCode::Down => Some(LayoutVector2D::new(0.0, -10.0)),
                    winit::event::VirtualKeyCode::Up => Some(LayoutVector2D::new(0.0, 10.0)),
                    winit::event::VirtualKeyCode::Right => Some(LayoutVector2D::new(-10.0, 0.0)),
                    winit::event::VirtualKeyCode::Left => Some(LayoutVector2D::new(10.0, 0.0)),
                    _ => None,
                };

                if let Some(offset) = offset {
                    self.scroll_offset += offset;

                    txn.set_scroll_offsets(
                        ExternalScrollId(EXT_SCROLL_ID_CONTENT, PipelineId::dummy()),
                        vec![SampledScrollOffset {
                            offset: self.scroll_offset,
                            generation: APZScrollGeneration::default(),
                        }],
                    );
                    txn.generate_frame(0, RenderReasons::empty());
                }
            }
            winit::event::WindowEvent::CursorMoved { position, .. } => {
                let pos: LogicalPosition<f32> = position.to_logical(window.scale_factor());
                self.cursor_position = WorldPoint::new(pos.x, pos.y);
            }
            winit::event::WindowEvent::MouseWheel { delta, .. } => {
                const LINE_HEIGHT: f32 = 38.0;
                let (dx, dy) = match delta {
                    winit::event::MouseScrollDelta::LineDelta(dx, dy) => (dx, dy * LINE_HEIGHT),
                    winit::event::MouseScrollDelta::PixelDelta(pos) => (pos.x as f32, pos.y as f32),
                };

                self.scroll_offset += LayoutVector2D::new(dx, dy);

                txn.set_scroll_offsets(
                    ExternalScrollId(EXT_SCROLL_ID_CONTENT, PipelineId::dummy()),
                    vec![SampledScrollOffset {
                            offset: self.scroll_offset,
                            generation: APZScrollGeneration::default(),
                    }],
                );

                txn.generate_frame(0, RenderReasons::empty());
            }
            winit::event::WindowEvent::MouseInput { .. } => {
                let results = api.hit_test(
                    document_id,
                    self.cursor_position,
                );

                println!("Hit test results:");
                for item in &results.items {
                    println!("  • {:?}", item);
                }
                println!("");
            }
            _ => (),
        }

        api.send_transaction(document_id, txn);

        false
    }
}

fn main() {
    let mut app = App {
        cursor_position: WorldPoint::zero(),
        scroll_offset: LayoutVector2D::zero(),
    };
    boilerplate::main_wrapper(&mut app, None);
}
