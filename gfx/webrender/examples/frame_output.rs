/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

extern crate gleam;
extern crate glutin;
extern crate webrender;

#[path = "common/boilerplate.rs"]
mod boilerplate;

use boilerplate::{Example, HandyDandyRectBuilder};
use gleam::gl;
use webrender::api::*;

// This example demonstrates using the frame output feature to copy
// the output of a WR framebuffer to a custom texture.

const VS: &str = "#version 130
    in vec2 aPos;out vec2 vUv;
    void main() { vUv = aPos; gl_Position = vec4(aPos, 0.0, 1.0); }
";
const FS: &str = "#version 130
    out vec4 oFragColor;
    in vec2 vUv;
    uniform sampler2D s;
    void main() { oFragColor = texture(s, vUv); }
";

struct App {
    iframe_pipeline_id: Option<PipelineId>,
    texture_id: gl::GLuint,
}

struct OutputHandler {
    texture_id: gl::GLuint,
}

impl OutputHandler {
    fn new(texture_id: gl::GLuint) -> OutputHandler {
        OutputHandler { texture_id }
    }
}

impl webrender::OutputImageHandler for OutputHandler {
    fn lock(&mut self, _id: PipelineId) -> Option<(u32, DeviceIntSize)> {
        Some((self.texture_id, DeviceIntSize::new(100, 100)))
    }

    fn unlock(&mut self, _id: PipelineId) {}
}

impl Example for App {
    fn render(
        &mut self,
        api: &RenderApi,
        builder: &mut DisplayListBuilder,
        _resources: &mut ResourceUpdates,
        _layout_size: LayoutSize,
        _pipeline_id: PipelineId,
        document_id: DocumentId,
    ) {
        // Build the iframe display list on first render.
        if self.iframe_pipeline_id.is_none() {
            let epoch = Epoch(0);
            let root_background_color = ColorF::new(0.0, 1.0, 0.0, 1.0);

            let iframe_pipeline_id = PipelineId(0, 1);
            let layout_size = LayoutSize::new(100.0, 100.0);
            let mut builder = DisplayListBuilder::new(iframe_pipeline_id, layout_size);
            let resources = ResourceUpdates::new();

            let bounds = (0, 0).to(50, 50);
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

            builder.push_rect(&info, ColorF::new(1.0, 1.0, 0.0, 1.0));
            builder.pop_stacking_context();

            api.set_display_list(
                document_id,
                epoch,
                Some(root_background_color),
                layout_size,
                builder.finalize(),
                true,
                resources,
            );

            self.iframe_pipeline_id = Some(iframe_pipeline_id);
            api.enable_frame_output(document_id, iframe_pipeline_id, true);
        }

        let bounds = (100, 100).to(200, 200);
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

        builder.push_iframe(&info, self.iframe_pipeline_id.unwrap());

        builder.pop_stacking_context();
    }

    fn draw_custom(&self, gl: &gl::Gl) {
        let vbo = gl.gen_buffers(1)[0];
        let vao = gl.gen_vertex_arrays(1)[0];

        let pid = create_program(gl);

        let vertices: [f32; 12] = [0.0, 1.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0, 1.0, 0.0];

        gl.active_texture(gl::TEXTURE0);
        gl.bind_texture(gl::TEXTURE_2D, self.texture_id);

        gl.use_program(pid);
        let sampler = gl.get_uniform_location(pid, "s");
        debug_assert!(sampler != -1);
        gl.uniform_1i(sampler, 0);

        gl.bind_buffer(gl::ARRAY_BUFFER, vbo);
        gl::buffer_data(gl, gl::ARRAY_BUFFER, &vertices, gl::STATIC_DRAW);

        gl.bind_vertex_array(vao);
        gl.enable_vertex_attrib_array(0);
        gl.vertex_attrib_pointer(0, 2, gl::FLOAT, false, 8, 0);

        gl.draw_arrays(gl::TRIANGLES, 0, 6);

        gl.delete_vertex_arrays(&[vao]);
        gl.delete_buffers(&[vbo]);
        gl.delete_program(pid);
    }

    fn on_event(
        &mut self,
        _event: glutin::Event,
        _api: &RenderApi,
        _document_id: DocumentId,
    ) -> bool {
        false
    }

    fn get_output_image_handler(
        &mut self,
        gl: &gl::Gl,
    ) -> Option<Box<webrender::OutputImageHandler>> {
        let texture_id = gl.gen_textures(1)[0];

        gl.bind_texture(gl::TEXTURE_2D, texture_id);
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
        gl.tex_image_2d(
            gl::TEXTURE_2D,
            0,
            gl::RGBA as gl::GLint,
            100,
            100,
            0,
            gl::BGRA,
            gl::UNSIGNED_BYTE,
            None,
        );
        gl.bind_texture(gl::TEXTURE_2D, 0);

        self.texture_id = texture_id;
        Some(Box::new(OutputHandler::new(texture_id)))
    }
}

fn main() {
    let mut app = App {
        iframe_pipeline_id: None,
        texture_id: 0,
    };
    boilerplate::main_wrapper(&mut app, None);
}

pub fn compile_shader(gl: &gl::Gl, shader_type: gl::GLenum, source: &str) -> gl::GLuint {
    let id = gl.create_shader(shader_type);
    gl.shader_source(id, &[source.as_bytes()]);
    gl.compile_shader(id);
    let log = gl.get_shader_info_log(id);
    if gl.get_shader_iv(id, gl::COMPILE_STATUS) == (0 as gl::GLint) {
        panic!("{:?} {}", source, log);
    }
    id
}

pub fn create_program(gl: &gl::Gl) -> gl::GLuint {
    let vs_id = compile_shader(gl, gl::VERTEX_SHADER, VS);
    let fs_id = compile_shader(gl, gl::FRAGMENT_SHADER, FS);

    let pid = gl.create_program();
    gl.attach_shader(pid, vs_id);
    gl.attach_shader(pid, fs_id);

    gl.bind_attrib_location(pid, 0, "aPos");
    gl.link_program(pid);

    gl.detach_shader(pid, vs_id);
    gl.detach_shader(pid, fs_id);
    gl.delete_shader(vs_id);
    gl.delete_shader(fs_id);

    if gl.get_program_iv(pid, gl::LINK_STATUS) == (0 as gl::GLint) {
        let error_log = gl.get_program_info_log(pid);
        panic!("{}", error_log);
    }

    pid
}
