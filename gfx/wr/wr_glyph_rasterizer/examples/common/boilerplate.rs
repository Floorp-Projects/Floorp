/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::ImageFormat;
use euclid::{Transform3D, UnknownUnit};
use glutin::{self, PossiblyCurrent};
use gleam::gl;
use wr_glyph_rasterizer::{RasterizedGlyph, GlyphFormat};

use std::{ffi::CStr, rc::Rc};

#[allow(unused)]
pub struct Gl {
    pub gl: Rc<dyn gl::Gl>,
    program: gl::GLuint,
    vb: gl::GLuint,
    vao: gl::GLuint,
    vs: gl::GLuint,
    fs: gl::GLuint,
    textures: Vec<gl::GLuint>,
    u_transform: i32,
    u_text_color: i32,
    u_sampler_color: i32,
    glyphs: Vec<RasterizedGlyph>,
}

pub fn load(gl_context: &glutin::Context<PossiblyCurrent>, glyphs: Vec<RasterizedGlyph>) -> Gl {
    env_logger::init();

    #[cfg(target_os = "macos")]
    {
        use core_foundation::{self as cf, base::TCFType};
        let i = cf::bundle::CFBundle::main_bundle().info_dictionary();
        let mut i = unsafe { i.to_mutable() };
        i.set(
            cf::string::CFString::new("NSSupportsAutomaticGraphicsSwitching"),
            cf::boolean::CFBoolean::true_value().into_CFType(),
        );
    }

    let gl = match gl_context.get_api() {
        glutin::Api::OpenGl => unsafe {
            gl::GlFns::load_with(|symbol| gl_context.get_proc_address(symbol) as *const _)
        },
        glutin::Api::OpenGlEs => unsafe {
            gl::GlesFns::load_with(|symbol| gl_context.get_proc_address(symbol) as *const _)
        },
        glutin::Api::WebGl => unimplemented!(),
    };

    let version = unsafe {
        let data = CStr::from_ptr(gl.get_string(gl::VERSION).as_ptr() as *const _)
            .to_bytes()
            .to_vec();
        String::from_utf8(data).unwrap()
    };

    println!("OpenGL version {}", version);

    let vs = gl.create_shader(gl::VERTEX_SHADER);
    gl.shader_source(vs, &[VS_SRC]);
    gl.compile_shader(vs);

    let fs = gl.create_shader(gl::FRAGMENT_SHADER);
    gl.shader_source(fs, &[FS_SRC]);
    gl.compile_shader(fs);

    let program = gl.create_program();
    gl.attach_shader(program, vs);
    gl.attach_shader(program, fs);
    gl.link_program(program);
    gl.use_program(program);

    let vb = gl.gen_buffers(1)[0];
    gl.bind_buffer(gl::ARRAY_BUFFER, vb);
    gl.buffer_data_untyped(
        gl::ARRAY_BUFFER,
        (6 * 4 * std::mem::size_of::<f32>()) as gl::types::GLsizeiptr,
        std::ptr::null(),
        gl::DYNAMIC_DRAW,
    );

    let vao = gl.gen_vertex_arrays(1)[0];
    gl.bind_vertex_array(vao);

    let u_transform = gl.get_uniform_location(program, "uTransform");
    let u_text_color = gl.get_uniform_location(program, "uTextColor");
    let u_sampler_color = gl.get_uniform_location(program, "uSamplerColor");
    let i_position = gl.get_attrib_location(program, "iPosition");
    let i_tex_coords = gl.get_attrib_location(program, "iTexCoords");
    gl.vertex_attrib_pointer(
        i_position as gl::types::GLuint,
        2,
        gl::FLOAT,
        false,
        4 * std::mem::size_of::<f32>() as gl::types::GLsizei,
        0,
    );
    gl.vertex_attrib_pointer(
        i_tex_coords as gl::types::GLuint,
        2,
        gl::FLOAT,
        false,
        4 * std::mem::size_of::<f32>() as gl::types::GLsizei,
        (2 * std::mem::size_of::<f32>()) as gl::types::GLuint,
    );
    gl.enable_vertex_attrib_array(i_position as gl::types::GLuint);
    gl.enable_vertex_attrib_array(i_tex_coords as gl::types::GLuint);

    let textures = create_texture(&gl, &glyphs);

    gl.bind_buffer(gl::ARRAY_BUFFER, 0);
    gl.bind_vertex_array(0);

    unsafe { log_shader(&gl, vs) };
    unsafe { log_shader(&gl, fs) };

    Gl {
        gl,
        program,
        vb,
        vao,
        u_transform,
        u_text_color,
        u_sampler_color,
        glyphs,
        textures,
        vs,
        fs,
    }
}

fn create_texture(gl: &Rc<dyn gl::Gl>, glyphs: &[RasterizedGlyph]) -> Vec<gl::GLuint> {
    let textures = gl.gen_textures(glyphs.len() as gl::types::GLsizei);
    for (i, glyph) in glyphs.iter().enumerate() {
        let (internal_format, external_format) = get_texture_format(&glyph.format);
        let texture = textures[i];
        gl.bind_texture(gl::TEXTURE_2D, texture);
        gl.tex_parameter_i(
            gl::TEXTURE_2D,
            gl::TEXTURE_MAG_FILTER,
            gl::LINEAR as gl::GLint,
        );
        gl.tex_parameter_i(
            gl::TEXTURE_2D,
            gl::TEXTURE_MIN_FILTER,
            gl::LINEAR as gl::GLint,
        );
        gl.tex_parameter_i(
            gl::TEXTURE_2D,
            gl::TEXTURE_WRAP_S,
            gl::CLAMP_TO_EDGE as gl::GLint,
        );
        gl.tex_parameter_i(
            gl::TEXTURE_2D,
            gl::TEXTURE_WRAP_T,
            gl::CLAMP_TO_EDGE as gl::GLint,
        );
        // TODO: use tex_storage_2d
        gl.tex_image_2d(
            gl::TEXTURE_2D,
            0,
            internal_format as gl::GLint,
            glyph.width,
            glyph.height,
            0,
            external_format,
            gl::UNSIGNED_BYTE,
            Some(&glyph.bytes),
        );

        gl.pixel_store_i(gl::UNPACK_ALIGNMENT, 1);
        gl.enable(gl::BLEND);
        gl.blend_func(gl::SRC_ALPHA, gl::ONE_MINUS_SRC_ALPHA);
    }

    textures
}

fn get_texture_format(format: &GlyphFormat) -> (gl::GLuint, gl::GLuint) {
    match format.image_format(false) {
        ImageFormat::BGRA8 => (gl::RGBA, gl::BGRA),
        _ => unimplemented!(),
    }
}

unsafe fn log_shader(gl: &Rc<dyn gl::Gl>, shader: gl::GLuint) {
    let log = gl.get_shader_info_log(shader);
    if log.len() != 0 {
        println!("[ERROR] {}", log);
    }
}

impl Gl {
    pub fn draw_frame(
        &self,
        width: f32,
        height: f32,
        text_color: [f32; 4],
        background_color: [f32; 4],
        scale_factor: f32,
    ) {
        let projection: Transform3D<f32, UnknownUnit, UnknownUnit> =
            Transform3D::ortho(0., width, height, 0., -1., 1.);
        self.gl
            .uniform_matrix_4fv(self.u_transform, false, &projection.to_array());
        self.gl.uniform_4fv(self.u_text_color, &text_color);

        self.gl.active_texture(gl::TEXTURE0);

        self.gl.bind_vertex_array(self.vao);

        self.gl.clear_color(
            background_color[0],
            background_color[1],
            background_color[2],
            background_color[3],
        );
        self.gl.clear(gl::COLOR_BUFFER_BIT);

        let mut ax = 0.;
        for (i, glyph) in self.glyphs.iter().enumerate() {
            let texture = self.textures[i];

            let x = ax + glyph.left;
            let y = glyph.top;

            let w = (glyph.width as f32) * scale_factor;
            let h = (glyph.height as f32) * scale_factor;

            #[rustfmt::skip]
            let vertices = [
                x,     y,      0.0, 0.0,
                x,     y + h,  0.0, 1.0,
                x + w, y + h,  1.0, 1.0,

                x,     y,      0.0, 0.0,
                x + w, y + h,  1.0, 1.0,
                x + w, y,      1.0, 0.0
            ];

            self.gl.uniform_1i(self.u_sampler_color, 0);
            self.gl.bind_texture(gl::TEXTURE_2D, texture);
            self.gl.bind_buffer(gl::ARRAY_BUFFER, self.vb);
            self.gl.buffer_data_untyped(
                gl::ARRAY_BUFFER,
                (vertices.len() * std::mem::size_of::<f32>()) as gl::GLsizeiptr,
                vertices.as_ptr() as *const _,
                gl::DYNAMIC_DRAW,
            );
            self.gl.bind_buffer(gl::ARRAY_BUFFER, 0);

            self.gl.draw_arrays(gl::TRIANGLES, 0, 6);

            ax += (glyph.left * scale_factor) + (glyph.width as f32 * scale_factor);
        }
        self.gl.bind_vertex_array(0);
        self.gl.bind_texture(gl::TEXTURE_2D, 0);

        unsafe {
            log_shader(&self.gl, self.vs);
            log_shader(&self.gl, self.fs);
        };
    }
}

const VS_SRC: &[u8] = b"
#version 150

in vec2 iPosition;
in vec2 iTexCoords;

uniform mat4 uTransform;

out vec2 vColorTexCoord;

void main() {
    gl_Position = uTransform * vec4(iPosition, 0.0, 1.0);
    vColorTexCoord = iTexCoords;
}
\0";

const FS_SRC: &[u8] = b"
#version 150

in vec2 vColorTexCoord;

uniform sampler2D uSamplerColor;
uniform vec4 uTextColor;

out vec4 oFragmentColor;

void main() {
    vec4 alpha = vec4(1.0, 1.0, 1.0, texture(uSamplerColor, vColorTexCoord).r);
    oFragmentColor = uTextColor * alpha;
}
\0";
