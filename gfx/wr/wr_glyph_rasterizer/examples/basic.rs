/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::sync::Arc;
use std::mem;

use api::{
    IdNamespace, FontTemplate, FontKey, FontInstanceKey, FontInstanceOptions,
    FontInstancePlatformOptions, ColorF, FontInstanceFlags, units::DevicePoint,
};
use glutin::ContextBuilder;
use glutin::dpi::PhysicalSize;
use glutin::event::{Event, WindowEvent};
use glutin::event_loop::{ControlFlow, EventLoop};
use glutin::window::WindowBuilder;
use rayon::ThreadPoolBuilder;
use wr_glyph_rasterizer::RasterizedGlyph;
use wr_glyph_rasterizer::{
    SharedFontResources, BaseFontInstance, GlyphRasterizer, FontInstance, GlyphKey,
    SubpixelDirection, profiler::GlyphRasterizeProfiler,
};

#[path = "common/boilerplate.rs"]
mod boilerplate;

struct Profiler;

impl GlyphRasterizeProfiler for Profiler {
    fn start_time(&mut self) {}
    fn end_time(&mut self) -> f64 {
        0.
    }
    fn set(&mut self, _value: f64) {}
}

fn load_glyphs() -> Vec<RasterizedGlyph> {
    let namespace = IdNamespace(0);
    let mut fonts = SharedFontResources::new(namespace);

    let font_key = FontKey::new(namespace, 0);
    let raw_font_data = include_bytes!("../../wrench/reftests/text/FreeSans.ttf");
    let font_template = FontTemplate::Raw(Arc::new(raw_font_data.to_vec()), 0);
    let shared_font_key = fonts
        .font_keys
        .add_key(&font_key, &font_template)
        .expect("Failed to add font key");

    let font_instance_key = FontInstanceKey::new(namespace, 1);
    fonts.templates.add_font(shared_font_key, font_template);
    assert!(fonts.templates.has_font(&shared_font_key));

    // AddFontInstance will only be processed here, not in the resource cache, so it
    // is safe to take the options rather than clone them.
    let base = BaseFontInstance::new(
        font_instance_key,
        shared_font_key,
        32.,
        mem::take(&mut Some(FontInstanceOptions::default())),
        mem::take(&mut Some(FontInstancePlatformOptions::default())),
        mem::take(&mut Vec::new()),
    );
    let shared_instance = fonts
        .instance_keys
        .add_key(base)
        .expect("Failed to add font instance key");
    fonts.instances.add_font_instance(shared_instance);

    let workers = {
        let worker = ThreadPoolBuilder::new()
            .thread_name(|idx| format!("WRWorker#{}", idx))
            .build();
        Arc::new(worker.unwrap())
    };
    let mut glyph_rasterizer = GlyphRasterizer::new(workers, false);

    glyph_rasterizer.add_font(
        shared_font_key,
        fonts
            .templates
            .get_font(&shared_font_key)
            .expect("Could not find FontTemplate"),
    );

    let mut font = FontInstance::new(
        fonts
            .instances
            .get_font_instance(font_instance_key)
            .expect("Could not found BaseFontInstance"),
        ColorF::BLACK.into(),
        api::FontRenderMode::Alpha,
        FontInstanceFlags::SUBPIXEL_POSITION | FontInstanceFlags::FONT_SMOOTHING,
    );

    glyph_rasterizer.prepare_font(&mut font);

    let glyph_keys = [
        glyph_rasterizer
            .get_glyph_index(shared_font_key, 'A')
            .expect("Failed to get glyph index"),
        glyph_rasterizer
            .get_glyph_index(shared_font_key, 'B')
            .expect("Failed to get glyph index"),
        glyph_rasterizer
            .get_glyph_index(shared_font_key, 'C')
            .expect("Failed to get glyph index"),
    ];

    let glyph_keys = glyph_keys.map(|g| {
        GlyphKey::new(
            g,
            DevicePoint::new(100., 100.),
            SubpixelDirection::Horizontal,
        )
    });

    glyph_rasterizer.request_glyphs(font, &glyph_keys, |_| true);

    let mut glyphs = vec![];
    glyph_rasterizer.resolve_glyphs(
        |job, _| {
            if let Ok(glyph) = job.result {
                glyphs.push(glyph);
            }
        },
        &mut Profiler,
    );

    glyphs
}

fn main() {
    let glyphs = load_glyphs();

    let el = EventLoop::new();
    let wb = WindowBuilder::new()
        .with_title("A fantastic window!")
        .with_inner_size(PhysicalSize::new(1900. as f64, 1300. as f64));

    let windowed_context = ContextBuilder::new()
        .with_gl(glutin::GlRequest::GlThenGles {
            opengl_version: (3, 2),
            opengles_version: (3, 0),
        })
        .build_windowed(wb, &el)
        .unwrap();

    let windowed_context = unsafe { windowed_context.make_current().unwrap() };

    let gl = boilerplate::load(windowed_context.context(), glyphs);

    el.run(move |event, _, control_flow| {
        *control_flow = ControlFlow::Wait;

        match event {
            Event::LoopDestroyed => (),
            Event::WindowEvent { event, .. } => match event {
                WindowEvent::Resized(physical_size) => windowed_context.resize(physical_size),
                WindowEvent::CloseRequested => *control_flow = ControlFlow::Exit,
                _ => (),
            },
            Event::RedrawRequested(_) => {
                let size = windowed_context.window().inner_size();
                let scale_factor = windowed_context.window().scale_factor();
                gl.draw_frame(
                    size.width as f32,
                    size.height as f32,
                    [0., 0., 0., 1.0],
                    [1., 1., 1., 1.0],
                    scale_factor as f32,
                );
                windowed_context.swap_buffers().unwrap();
            }
            _ => (),
        }
    });
}
