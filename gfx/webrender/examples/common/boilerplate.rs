/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

extern crate env_logger;
extern crate euclid;

use gleam::gl;
use glutin::{self, GlContext};
use std::env;
use std::path::PathBuf;
use webrender;
use webrender::api::*;

struct Notifier {
    events_proxy: glutin::EventsLoopProxy,
}

impl Notifier {
    fn new(events_proxy: glutin::EventsLoopProxy) -> Notifier {
        Notifier { events_proxy }
    }
}

impl RenderNotifier for Notifier {
    fn clone(&self) -> Box<RenderNotifier> {
        Box::new(Notifier {
            events_proxy: self.events_proxy.clone(),
        })
    }

    fn wake_up(&self) {
        #[cfg(not(target_os = "android"))]
        let _ = self.events_proxy.wakeup();
    }

    fn new_frame_ready(&self, _: DocumentId, _scrolled: bool, _composite_needed: bool) {
        self.wake_up();
    }
}

pub trait HandyDandyRectBuilder {
    fn to(&self, x2: i32, y2: i32) -> LayoutRect;
    fn by(&self, w: i32, h: i32) -> LayoutRect;
}
// Allows doing `(x, y).to(x2, y2)` or `(x, y).by(width, height)` with i32
// values to build a f32 LayoutRect
impl HandyDandyRectBuilder for (i32, i32) {
    fn to(&self, x2: i32, y2: i32) -> LayoutRect {
        LayoutRect::new(
            LayoutPoint::new(self.0 as f32, self.1 as f32),
            LayoutSize::new((x2 - self.0) as f32, (y2 - self.1) as f32),
        )
    }

    fn by(&self, w: i32, h: i32) -> LayoutRect {
        LayoutRect::new(
            LayoutPoint::new(self.0 as f32, self.1 as f32),
            LayoutSize::new(w as f32, h as f32),
        )
    }
}

pub trait Example {
    const TITLE: &'static str = "WebRender Sample App";
    const PRECACHE_SHADERS: bool = false;
    const WIDTH: u32 = 1920;
    const HEIGHT: u32 = 1080;

    fn render(
        &mut self,
        api: &RenderApi,
        builder: &mut DisplayListBuilder,
        resources: &mut ResourceUpdates,
        framebuffer_size: DeviceUintSize,
        pipeline_id: PipelineId,
        document_id: DocumentId,
    );
    fn on_event(&mut self, glutin::WindowEvent, &RenderApi, DocumentId) -> bool {
        false
    }
    fn get_image_handlers(
        &mut self,
        _gl: &gl::Gl,
    ) -> (Option<Box<webrender::ExternalImageHandler>>,
          Option<Box<webrender::OutputImageHandler>>) {
        (None, None)
    }
    fn draw_custom(&self, _gl: &gl::Gl) {
    }
}

pub fn main_wrapper<E: Example>(
    example: &mut E,
    options: Option<webrender::RendererOptions>,
) {
    env_logger::init();

    let args: Vec<String> = env::args().collect();
    let res_path = if args.len() > 1 {
        Some(PathBuf::from(&args[1]))
    } else {
        None
    };

    let mut events_loop = glutin::EventsLoop::new();
    let context_builder = glutin::ContextBuilder::new()
        .with_gl(glutin::GlRequest::GlThenGles {
            opengl_version: (3, 2),
            opengles_version: (3, 0),
        });
    let window_builder = glutin::WindowBuilder::new()
        .with_title(E::TITLE)
        .with_multitouch()
        .with_dimensions(E::WIDTH, E::HEIGHT);
    let window = glutin::GlWindow::new(window_builder, context_builder, &events_loop)
        .unwrap();

    unsafe {
        window.make_current().ok();
    }

    let gl = match window.get_api() {
        glutin::Api::OpenGl => unsafe {
            gl::GlFns::load_with(|symbol| window.get_proc_address(symbol) as *const _)
        },
        glutin::Api::OpenGlEs => unsafe {
            gl::GlesFns::load_with(|symbol| window.get_proc_address(symbol) as *const _)
        },
        glutin::Api::WebGl => unimplemented!(),
    };

    println!("OpenGL version {}", gl.get_string(gl::VERSION));
    println!("Shader resource path: {:?}", res_path);
    let device_pixel_ratio = window.hidpi_factor();
    println!("Device pixel ratio: {}", device_pixel_ratio);

    println!("Loading shaders...");
    let opts = webrender::RendererOptions {
        resource_override_path: res_path,
        precache_shaders: E::PRECACHE_SHADERS,
        device_pixel_ratio,
        clear_color: Some(ColorF::new(0.3, 0.0, 0.0, 1.0)),
        //scatter_gpu_cache_updates: false,
        debug_flags: webrender::DebugFlags::ECHO_DRIVER_MESSAGES,
        ..options.unwrap_or(webrender::RendererOptions::default())
    };

    let framebuffer_size = {
        let (width, height) = window.get_inner_size().unwrap();
        DeviceUintSize::new(width, height)
    };
    let notifier = Box::new(Notifier::new(events_loop.create_proxy()));
    let (mut renderer, sender) = webrender::Renderer::new(gl.clone(), notifier, opts).unwrap();
    let api = sender.create_api();
    let document_id = api.add_document(framebuffer_size, 0);

    let (external, output) = example.get_image_handlers(&*gl);

    if let Some(output_image_handler) = output {
        renderer.set_output_image_handler(output_image_handler);
    }

    if let Some(external_image_handler) = external {
        renderer.set_external_image_handler(external_image_handler);
    }

    let epoch = Epoch(0);
    let pipeline_id = PipelineId(0, 0);
    let layout_size = framebuffer_size.to_f32() / euclid::TypedScale::new(device_pixel_ratio);
    let mut builder = DisplayListBuilder::new(pipeline_id, layout_size);
    let mut resources = ResourceUpdates::new();

    example.render(
        &api,
        &mut builder,
        &mut resources,
        framebuffer_size,
        pipeline_id,
        document_id,
    );
    let mut txn = Transaction::new();
    txn.set_display_list(
        epoch,
        None,
        layout_size,
        builder.finalize(),
        true,
    );
    txn.update_resources(resources);
    txn.set_root_pipeline(pipeline_id);
    txn.generate_frame();
    api.send_transaction(document_id, txn);

    println!("Entering event loop");
    events_loop.run_forever(|global_event| {
        let mut txn = Transaction::new();
        let mut custom_event = true;

        match global_event {
            glutin::Event::WindowEvent { event: glutin::WindowEvent::Closed, .. } => return glutin::ControlFlow::Break,
            glutin::Event::WindowEvent {
                event: glutin::WindowEvent::KeyboardInput {
                    input: glutin::KeyboardInput {
                        state: glutin::ElementState::Pressed,
                        virtual_keycode: Some(key),
                        ..
                    },
                    ..
                },
                ..
            } => match key {
                glutin::VirtualKeyCode::Escape => return glutin::ControlFlow::Break,
                glutin::VirtualKeyCode::P => renderer.toggle_debug_flags(webrender::DebugFlags::PROFILER_DBG),
                glutin::VirtualKeyCode::O => renderer.toggle_debug_flags(webrender::DebugFlags::RENDER_TARGET_DBG),
                glutin::VirtualKeyCode::I => renderer.toggle_debug_flags(webrender::DebugFlags::TEXTURE_CACHE_DBG),
                glutin::VirtualKeyCode::S => renderer.toggle_debug_flags(webrender::DebugFlags::COMPACT_PROFILER),
                glutin::VirtualKeyCode::Q => renderer.toggle_debug_flags(
                    webrender::DebugFlags::GPU_TIME_QUERIES | webrender::DebugFlags::GPU_SAMPLE_QUERIES
                ),
                glutin::VirtualKeyCode::Key1 => txn.set_window_parameters(
                    framebuffer_size,
                    DeviceUintRect::new(DeviceUintPoint::zero(), framebuffer_size),
                    1.0
                ),
                glutin::VirtualKeyCode::Key2 => txn.set_window_parameters(
                    framebuffer_size,
                    DeviceUintRect::new(DeviceUintPoint::zero(), framebuffer_size),
                    2.0
                ),
                glutin::VirtualKeyCode::M => api.notify_memory_pressure(),
                #[cfg(feature = "capture")]
                glutin::VirtualKeyCode::C => {
                    let path: PathBuf = "../captures/example".into();
                    //TODO: switch between SCENE/FRAME capture types
                    // based on "shift" modifier, when `glutin` is updated.
                    let bits = CaptureBits::all();
                    api.save_capture(path, bits);
                },
                _ => {
                    let win_event = match global_event {
                        glutin::Event::WindowEvent { event, .. } => event,
                        _ => unreachable!()
                    };
                    custom_event = example.on_event(win_event, &api, document_id)
                },
            },
            glutin::Event::WindowEvent { event, .. } => custom_event = example.on_event(event, &api, document_id),
            _ => return glutin::ControlFlow::Continue,
        };

        if custom_event {
            let mut builder = DisplayListBuilder::new(pipeline_id, layout_size);
            let mut resources = ResourceUpdates::new();

            example.render(
                &api,
                &mut builder,
                &mut resources,
                framebuffer_size,
                pipeline_id,
                document_id,
            );
            txn.set_display_list(
                epoch,
                None,
                layout_size,
                builder.finalize(),
                true,
            );
            txn.update_resources(resources);
            txn.generate_frame();
        }
        api.send_transaction(document_id, txn);

        renderer.update();
        renderer.render(framebuffer_size).unwrap();
        let _ = renderer.flush_pipeline_info();
        example.draw_custom(&*gl);
        window.swap_buffers().ok();

        glutin::ControlFlow::Continue
    });

    renderer.deinit();
}
