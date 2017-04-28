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
use glutin::TouchPhase;
use std::collections::HashMap;
use std::env;
use std::path::PathBuf;
use webrender_traits::{ClipRegion, ColorF, Epoch};
use webrender_traits::{DeviceIntPoint, DeviceUintSize, LayoutPoint, LayoutRect, LayoutSize};
use webrender_traits::{ImageData, ImageDescriptor, ImageFormat};
use webrender_traits::{PipelineId, TransformStyle};
use webrender_traits::{YuvColorSpace, YuvData};

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
        dist(self.current_x, self.current_y, other.current_x, other.current_y)
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
                self.active_touches.insert(touch.id, Touch {
                    id: touch.id,
                    start_x: touch.location.0 as f32,
                    start_y: touch.location.1 as f32,
                    current_x: touch.location.0 as f32,
                    current_y: touch.location.1 as f32,
                });
                self.current_gesture = Gesture::None;
            }
            TouchPhase::Moved => {
                match self.active_touches.get_mut(&touch.id) {
                    Some(active_touch) => {
                        active_touch.current_x = touch.location.0 as f32;
                        active_touch.current_y = touch.location.1 as f32;
                    }
                    None => panic!("move touch event with unknown touch id!")
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

fn main() {
    let args: Vec<String> = env::args().collect();
    let res_path = if args.len() > 1 {
        Some(PathBuf::from(&args[1]))
    } else {
        None
    };

    let window = glutin::WindowBuilder::new()
                .with_title("WebRender Sample")
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
    println!("Shader resource path: {:?}", res_path);

    let (width, height) = window.get_inner_size_pixels().unwrap();

    let opts = webrender::RendererOptions {
        resource_override_path: res_path,
        debug: true,
        precache_shaders: true,
        blob_image_renderer: None,
        device_pixel_ratio: window.hidpi_factor(),
        .. Default::default()
    };

    let size = DeviceUintSize::new(width, height);
    let (mut renderer, sender) = webrender::renderer::Renderer::new(gl, opts, size).unwrap();
    let api = sender.create_api();

    let notifier = Box::new(Notifier::new(window.create_window_proxy()));
    renderer.set_render_notifier(notifier);

    let epoch = Epoch(0);
    let root_background_color = ColorF::new(0.3, 0.0, 0.0, 1.0);

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


    let yuv_chanel1 = api.generate_image_key();
    let yuv_chanel2 = api.generate_image_key();
    let yuv_chanel2_1 = api.generate_image_key();
    let yuv_chanel3 = api.generate_image_key();
    api.add_image(
        yuv_chanel1,
        ImageDescriptor::new(100, 100, ImageFormat::A8, true),
        ImageData::new(vec![127; 100 * 100]),
        None,
    );
    api.add_image(
        yuv_chanel2,
        ImageDescriptor::new(100, 100, ImageFormat::RG8, true),
        ImageData::new(vec![0; 100 * 100 * 2]),
        None,
    );
    api.add_image(
        yuv_chanel2_1,
        ImageDescriptor::new(100, 100, ImageFormat::A8, true),
        ImageData::new(vec![127; 100 * 100]),
        None,
    );
    api.add_image(
        yuv_chanel3,
        ImageDescriptor::new(100, 100, ImageFormat::A8, true),
        ImageData::new(vec![127; 100 * 100]),
        None,
    );

    builder.push_yuv_image(
        LayoutRect::new(LayoutPoint::new(100.0, 0.0), LayoutSize::new(100.0, 100.0)),
        ClipRegion::simple(&bounds),
        YuvData::NV12(yuv_chanel1, yuv_chanel2),
        YuvColorSpace::Rec601,
    );

    builder.push_yuv_image(
        LayoutRect::new(LayoutPoint::new(300.0, 0.0), LayoutSize::new(100.0, 100.0)),
        ClipRegion::simple(&bounds),
        YuvData::PlanarYCbCr(yuv_chanel1, yuv_chanel2_1, yuv_chanel3),
        YuvColorSpace::Rec601,
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

    let mut touch_state = TouchState::new();

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
                glutin::Event::Touch(touch) => {
                    match touch_state.handle_event(touch) {
                        TouchResult::Pan(pan) => {
                            api.set_pan(pan);
                            api.generate_frame(None);
                        }
                        TouchResult::Zoom(zoom) => {
                            api.set_pinch_zoom(webrender_traits::ZoomFactor::new(zoom));
                            api.generate_frame(None);
                        }
                        TouchResult::None => {}
                    }
                }
                _ => ()
            }
        }

        renderer.update();
        renderer.render(DeviceUintSize::new(width, height));
        window.swap_buffers().ok();
    }
}

