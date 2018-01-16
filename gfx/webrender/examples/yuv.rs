/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

extern crate gleam;
extern crate glutin;
extern crate webrender;

#[path = "common/boilerplate.rs"]
mod boilerplate;

use boilerplate::Example;
use glutin::TouchPhase;
use std::collections::HashMap;
use webrender::api::*;

#[derive(Debug)]
enum Gesture {
    None,
    Pan,
    Zoom,
}

#[derive(Debug)]
struct Touch {
    id: u64,
    start_x: f32,
    start_y: f32,
    current_x: f32,
    current_y: f32,
}

fn dist(x0: f32, y0: f32, x1: f32, y1: f32) -> f32 {
    let dx = x0 - x1;
    let dy = y0 - y1;
    ((dx * dx) + (dy * dy)).sqrt()
}

impl Touch {
    fn distance_from_start(&self) -> f32 {
        dist(self.start_x, self.start_y, self.current_x, self.current_y)
    }

    fn initial_distance_from_other(&self, other: &Touch) -> f32 {
        dist(self.start_x, self.start_y, other.start_x, other.start_y)
    }

    fn current_distance_from_other(&self, other: &Touch) -> f32 {
        dist(
            self.current_x,
            self.current_y,
            other.current_x,
            other.current_y,
        )
    }
}

struct TouchState {
    active_touches: HashMap<u64, Touch>,
    current_gesture: Gesture,
    start_zoom: f32,
    current_zoom: f32,
    start_pan: DeviceIntPoint,
    current_pan: DeviceIntPoint,
}

enum TouchResult {
    None,
    Pan(DeviceIntPoint),
    Zoom(f32),
}

impl TouchState {
    fn new() -> TouchState {
        TouchState {
            active_touches: HashMap::new(),
            current_gesture: Gesture::None,
            start_zoom: 1.0,
            current_zoom: 1.0,
            start_pan: DeviceIntPoint::zero(),
            current_pan: DeviceIntPoint::zero(),
        }
    }

    fn handle_event(&mut self, touch: glutin::Touch) -> TouchResult {
        match touch.phase {
            TouchPhase::Started => {
                debug_assert!(!self.active_touches.contains_key(&touch.id));
                self.active_touches.insert(
                    touch.id,
                    Touch {
                        id: touch.id,
                        start_x: touch.location.0 as f32,
                        start_y: touch.location.1 as f32,
                        current_x: touch.location.0 as f32,
                        current_y: touch.location.1 as f32,
                    },
                );
                self.current_gesture = Gesture::None;
            }
            TouchPhase::Moved => {
                match self.active_touches.get_mut(&touch.id) {
                    Some(active_touch) => {
                        active_touch.current_x = touch.location.0 as f32;
                        active_touch.current_y = touch.location.1 as f32;
                    }
                    None => panic!("move touch event with unknown touch id!"),
                }

                match self.current_gesture {
                    Gesture::None => {
                        let mut over_threshold_count = 0;
                        let active_touch_count = self.active_touches.len();

                        for (_, touch) in &self.active_touches {
                            if touch.distance_from_start() > 8.0 {
                                over_threshold_count += 1;
                            }
                        }

                        if active_touch_count == over_threshold_count {
                            if active_touch_count == 1 {
                                self.start_pan = self.current_pan;
                                self.current_gesture = Gesture::Pan;
                            } else if active_touch_count == 2 {
                                self.start_zoom = self.current_zoom;
                                self.current_gesture = Gesture::Zoom;
                            }
                        }
                    }
                    Gesture::Pan => {
                        let keys: Vec<u64> = self.active_touches.keys().cloned().collect();
                        debug_assert!(keys.len() == 1);
                        let active_touch = &self.active_touches[&keys[0]];
                        let x = active_touch.current_x - active_touch.start_x;
                        let y = active_touch.current_y - active_touch.start_y;
                        self.current_pan.x = self.start_pan.x + x.round() as i32;
                        self.current_pan.y = self.start_pan.y + y.round() as i32;
                        return TouchResult::Pan(self.current_pan);
                    }
                    Gesture::Zoom => {
                        let keys: Vec<u64> = self.active_touches.keys().cloned().collect();
                        debug_assert!(keys.len() == 2);
                        let touch0 = &self.active_touches[&keys[0]];
                        let touch1 = &self.active_touches[&keys[1]];
                        let initial_distance = touch0.initial_distance_from_other(touch1);
                        let current_distance = touch0.current_distance_from_other(touch1);
                        self.current_zoom = self.start_zoom * current_distance / initial_distance;
                        return TouchResult::Zoom(self.current_zoom);
                    }
                }
            }
            TouchPhase::Ended | TouchPhase::Cancelled => {
                self.active_touches.remove(&touch.id).unwrap();
                self.current_gesture = Gesture::None;
            }
        }

        TouchResult::None
    }
}

struct App {
    touch_state: TouchState,
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
        let bounds = LayoutRect::new(LayoutPoint::zero(), builder.content_size());
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

        let yuv_chanel1 = api.generate_image_key();
        let yuv_chanel2 = api.generate_image_key();
        let yuv_chanel2_1 = api.generate_image_key();
        let yuv_chanel3 = api.generate_image_key();
        resources.add_image(
            yuv_chanel1,
            ImageDescriptor::new(100, 100, ImageFormat::R8, true),
            ImageData::new(vec![127; 100 * 100]),
            None,
        );
        resources.add_image(
            yuv_chanel2,
            ImageDescriptor::new(100, 100, ImageFormat::RG8, true),
            ImageData::new(vec![0; 100 * 100 * 2]),
            None,
        );
        resources.add_image(
            yuv_chanel2_1,
            ImageDescriptor::new(100, 100, ImageFormat::R8, true),
            ImageData::new(vec![127; 100 * 100]),
            None,
        );
        resources.add_image(
            yuv_chanel3,
            ImageDescriptor::new(100, 100, ImageFormat::R8, true),
            ImageData::new(vec![127; 100 * 100]),
            None,
        );

        let info = LayoutPrimitiveInfo::with_clip_rect(
            LayoutRect::new(LayoutPoint::new(100.0, 0.0), LayoutSize::new(100.0, 100.0)),
            bounds,
        );
        builder.push_yuv_image(
            &info,
            YuvData::NV12(yuv_chanel1, yuv_chanel2),
            YuvColorSpace::Rec601,
            ImageRendering::Auto,
        );

        let info = LayoutPrimitiveInfo::with_clip_rect(
            LayoutRect::new(LayoutPoint::new(300.0, 0.0), LayoutSize::new(100.0, 100.0)),
            bounds,
        );
        builder.push_yuv_image(
            &info,
            YuvData::PlanarYCbCr(yuv_chanel1, yuv_chanel2_1, yuv_chanel3),
            YuvColorSpace::Rec601,
            ImageRendering::Auto,
        );

        builder.pop_stacking_context();
    }

    fn on_event(&mut self, event: glutin::Event, api: &RenderApi, document_id: DocumentId) -> bool {
        match event {
            glutin::Event::Touch(touch) => match self.touch_state.handle_event(touch) {
                TouchResult::Pan(pan) => {
                    api.set_pan(document_id, pan);
                    api.generate_frame(document_id, None);
                }
                TouchResult::Zoom(zoom) => {
                    api.set_pinch_zoom(document_id, ZoomFactor::new(zoom));
                    api.generate_frame(document_id, None);
                }
                TouchResult::None => {}
            },
            _ => (),
        }

        false
    }
}

fn main() {
    let mut app = App {
        touch_state: TouchState::new(),
    };
    boilerplate::main_wrapper(&mut app, None);
}
