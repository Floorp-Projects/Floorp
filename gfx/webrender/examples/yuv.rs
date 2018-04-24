/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

extern crate gleam;
extern crate glutin;
extern crate webrender;

#[path = "common/boilerplate.rs"]
mod boilerplate;

use boilerplate::Example;
use gleam::gl;
use webrender::api::*;

fn init_gl_texture(
    id: gl::GLuint,
    internal: gl::GLenum,
    external: gl::GLenum,
    bytes: &[u8],
    gl: &gl::Gl,
) {
    gl.bind_texture(gl::TEXTURE_2D, id);
    gl.tex_parameter_i(gl::TEXTURE_2D, gl::TEXTURE_MAG_FILTER, gl::LINEAR as gl::GLint);
    gl.tex_parameter_i(gl::TEXTURE_2D, gl::TEXTURE_MIN_FILTER, gl::LINEAR as gl::GLint);
    gl.tex_parameter_i(gl::TEXTURE_2D, gl::TEXTURE_WRAP_S, gl::CLAMP_TO_EDGE as gl::GLint);
    gl.tex_parameter_i(gl::TEXTURE_2D, gl::TEXTURE_WRAP_T, gl::CLAMP_TO_EDGE as gl::GLint);
    gl.tex_image_2d(
        gl::TEXTURE_2D,
        0,
        internal as gl::GLint,
        100,
        100,
        0,
        external,
        gl::UNSIGNED_BYTE,
        Some(bytes),
    );
    gl.bind_texture(gl::TEXTURE_2D, 0);
}

struct YuvImageProvider {
    texture_ids: Vec<gl::GLuint>,
}

impl YuvImageProvider {
    fn new(gl: &gl::Gl) -> Self {
        let texture_ids = gl.gen_textures(4);

        init_gl_texture(texture_ids[0], gl::RED, gl::RED, &[127; 100 * 100], gl);
        init_gl_texture(texture_ids[1], gl::RG8, gl::RG, &[0; 100 * 100 * 2], gl);
        init_gl_texture(texture_ids[2], gl::RED, gl::RED, &[127; 100 * 100], gl);
        init_gl_texture(texture_ids[3], gl::RED, gl::RED, &[127; 100 * 100], gl);

        YuvImageProvider {
            texture_ids
        }
    }
}

impl webrender::ExternalImageHandler for YuvImageProvider {
    fn lock(&mut self, key: ExternalImageId, _channel_index: u8) -> webrender::ExternalImage {
        let id = self.texture_ids[key.0 as usize];
        webrender::ExternalImage {
            uv: TexelRect::new(0.0, 0.0, 1.0, 1.0),
            source: webrender::ExternalImageSource::NativeTexture(id),
        }
    }
    fn unlock(&mut self, _key: ExternalImageId, _channel_index: u8) {
    }
}

struct App {
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
            None,
            ScrollPolicy::Scrollable,
            None,
            TransformStyle::Flat,
            None,
            MixBlendMode::Normal,
            Vec::new(),
            GlyphRasterSpace::Screen,
        );

        let yuv_chanel1 = api.generate_image_key();
        let yuv_chanel2 = api.generate_image_key();
        let yuv_chanel2_1 = api.generate_image_key();
        let yuv_chanel3 = api.generate_image_key();
        resources.add_image(
            yuv_chanel1,
            ImageDescriptor::new(100, 100, ImageFormat::R8, true, false),
            ImageData::External(ExternalImageData {
                id: ExternalImageId(0),
                channel_index: 0,
                image_type: ExternalImageType::TextureHandle(
                    TextureTarget::Default,
                ),
            }),
            None,
        );
        resources.add_image(
            yuv_chanel2,
            ImageDescriptor::new(100, 100, ImageFormat::RG8, true, false),
            ImageData::External(ExternalImageData {
                id: ExternalImageId(1),
                channel_index: 0,
                image_type: ExternalImageType::TextureHandle(
                    TextureTarget::Default,
                ),
            }),
            None,
        );
        resources.add_image(
            yuv_chanel2_1,
            ImageDescriptor::new(100, 100, ImageFormat::R8, true, false),
            ImageData::External(ExternalImageData {
                id: ExternalImageId(2),
                channel_index: 0,
                image_type: ExternalImageType::TextureHandle(
                    TextureTarget::Default,
                ),
            }),
            None,
        );
        resources.add_image(
            yuv_chanel3,
            ImageDescriptor::new(100, 100, ImageFormat::R8, true, false),
            ImageData::External(ExternalImageData {
                id: ExternalImageId(3),
                channel_index: 0,
                image_type: ExternalImageType::TextureHandle(
                    TextureTarget::Default,
                ),
            }),
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

    fn on_event(
        &mut self,
        _event: glutin::WindowEvent,
        _api: &RenderApi,
        _document_id: DocumentId,
    ) -> bool {
        false
    }

    fn get_image_handlers(
        &mut self,
        gl: &gl::Gl,
    ) -> (Option<Box<webrender::ExternalImageHandler>>,
          Option<Box<webrender::OutputImageHandler>>) {
        (Some(Box::new(YuvImageProvider::new(gl))), None)
    }
}

fn main() {
    let mut app = App {
    };
    boilerplate::main_wrapper(&mut app, None);
}
