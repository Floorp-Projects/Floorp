/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use debug_font_data;
use device::{Device, GpuMarker, ProgramId, VAOId, TextureId, VertexFormat};
use device::{TextureFilter, VertexUsageHint, TextureTarget};
use euclid::{Transform3D, Point2D, Size2D, Rect};
use internal_types::{ORTHO_NEAR_PLANE, ORTHO_FAR_PLANE, TextureSampler};
use internal_types::{DebugFontVertex, DebugColorVertex, RenderTargetMode, PackedColor};
use std::f32;
use api::{ColorF, ImageFormat, DeviceUintSize};

pub struct DebugRenderer {
    font_vertices: Vec<DebugFontVertex>,
    font_indices: Vec<u32>,
    font_program_id: ProgramId,
    font_vao: VAOId,
    font_texture_id: TextureId,

    tri_vertices: Vec<DebugColorVertex>,
    tri_indices: Vec<u32>,
    tri_vao: VAOId,
    line_vertices: Vec<DebugColorVertex>,
    line_vao: VAOId,
    color_program_id: ProgramId,
}

impl DebugRenderer {
    pub fn new(device: &mut Device) -> DebugRenderer {
        let font_program_id = device.create_program("debug_font", "shared_other", VertexFormat::DebugFont).unwrap();
        let color_program_id = device.create_program("debug_color", "shared_other", VertexFormat::DebugColor).unwrap();

        let font_vao = device.create_vao(VertexFormat::DebugFont, 32);
        let line_vao = device.create_vao(VertexFormat::DebugColor, 32);
        let tri_vao = device.create_vao(VertexFormat::DebugColor, 32);

        let font_texture_id = device.create_texture_ids(1, TextureTarget::Default)[0];
        device.init_texture(font_texture_id,
                            debug_font_data::BMP_WIDTH,
                            debug_font_data::BMP_HEIGHT,
                            ImageFormat::A8,
                            TextureFilter::Linear,
                            RenderTargetMode::None,
                            Some(&debug_font_data::FONT_BITMAP));

        DebugRenderer {
            font_vertices: Vec::new(),
            font_indices: Vec::new(),
            line_vertices: Vec::new(),
            tri_vao,
            tri_vertices: Vec::new(),
            tri_indices: Vec::new(),
            font_program_id,
            color_program_id,
            font_vao,
            line_vao,
            font_texture_id,
        }
    }

    pub fn line_height(&self) -> f32 {
        debug_font_data::FONT_SIZE as f32 * 1.1
    }

    pub fn add_text(&mut self,
                    x: f32,
                    y: f32,
                    text: &str,
                    color: &ColorF) -> Rect<f32> {
        let mut x_start = x;
        let ipw = 1.0 / debug_font_data::BMP_WIDTH as f32;
        let iph = 1.0 / debug_font_data::BMP_HEIGHT as f32;
        let color = PackedColor::from_color(color);

        let mut min_x = f32::MAX;
        let mut max_x = -f32::MAX;
        let mut min_y = f32::MAX;
        let mut max_y = -f32::MAX;

        for c in text.chars() {
            let c = c as usize - debug_font_data::FIRST_GLYPH_INDEX as usize;
            if c < debug_font_data::GLYPHS.len() {
                let glyph = &debug_font_data::GLYPHS[c];

                let x0 = (x_start + glyph.xo + 0.5).floor();
                let y0 = (y + glyph.yo + 0.5).floor();

                let x1 = x0 + glyph.x1 as f32 - glyph.x0 as f32;
                let y1 = y0 + glyph.y1 as f32 - glyph.y0 as f32;

                let s0 = glyph.x0 as f32 * ipw;
                let t0 = glyph.y0 as f32 * iph;
                let s1 = glyph.x1 as f32 * ipw;
                let t1 = glyph.y1 as f32 * iph;

                x_start += glyph.xa;

                let vertex_count = self.font_vertices.len() as u32;

                self.font_vertices.push(DebugFontVertex::new(x0, y0, s0, t0, color));
                self.font_vertices.push(DebugFontVertex::new(x1, y0, s1, t0, color));
                self.font_vertices.push(DebugFontVertex::new(x0, y1, s0, t1, color));
                self.font_vertices.push(DebugFontVertex::new(x1, y1, s1, t1, color));

                self.font_indices.push(vertex_count + 0);
                self.font_indices.push(vertex_count + 1);
                self.font_indices.push(vertex_count + 2);
                self.font_indices.push(vertex_count + 2);
                self.font_indices.push(vertex_count + 1);
                self.font_indices.push(vertex_count + 3);

                min_x = min_x.min(x0);
                max_x = max_x.max(x1);
                min_y = min_y.min(y0);
                max_y = max_y.max(y1);
            }
        }

        Rect::new(Point2D::new(min_x, min_y), Size2D::new(max_x-min_x, max_y-min_y))
    }

    pub fn add_quad(&mut self,
                    x0: f32,
                    y0: f32,
                    x1: f32,
                    y1: f32,
                    color_top: &ColorF,
                    color_bottom: &ColorF) {
        let color_top = PackedColor::from_color(color_top);
        let color_bottom = PackedColor::from_color(color_bottom);
        let vertex_count = self.tri_vertices.len() as u32;

        self.tri_vertices.push(DebugColorVertex::new(x0, y0, color_top));
        self.tri_vertices.push(DebugColorVertex::new(x1, y0, color_top));
        self.tri_vertices.push(DebugColorVertex::new(x0, y1, color_bottom));
        self.tri_vertices.push(DebugColorVertex::new(x1, y1, color_bottom));

        self.tri_indices.push(vertex_count + 0);
        self.tri_indices.push(vertex_count + 1);
        self.tri_indices.push(vertex_count + 2);
        self.tri_indices.push(vertex_count + 2);
        self.tri_indices.push(vertex_count + 1);
        self.tri_indices.push(vertex_count + 3);
    }

    #[allow(dead_code)]
    pub fn add_line(&mut self,
                    x0: i32,
                    y0: i32,
                    color0: &ColorF,
                    x1: i32,
                    y1: i32,
                    color1: &ColorF) {
        let color0 = PackedColor::from_color(color0);
        let color1 = PackedColor::from_color(color1);
        self.line_vertices.push(DebugColorVertex::new(x0 as f32, y0 as f32, color0));
        self.line_vertices.push(DebugColorVertex::new(x1 as f32, y1 as f32, color1));
    }

    pub fn render(&mut self,
                  device: &mut Device,
                  viewport_size: &DeviceUintSize) {
        let _gm = GpuMarker::new(device.rc_gl(), "debug");
        device.disable_depth();
        device.set_blend(true);
        device.set_blend_mode_alpha();

        let projection = Transform3D::ortho(0.0,
                                            viewport_size.width as f32,
                                            viewport_size.height as f32,
                                            0.0,
                                            ORTHO_NEAR_PLANE,
                                            ORTHO_FAR_PLANE);

        // Triangles
        if !self.tri_vertices.is_empty() {
            device.bind_program(self.color_program_id, &projection);
            device.bind_vao(self.tri_vao);
            device.update_vao_indices(self.tri_vao,
                                      &self.tri_indices,
                                      VertexUsageHint::Dynamic);
            device.update_vao_main_vertices(self.tri_vao,
                                            &self.tri_vertices,
                                            VertexUsageHint::Dynamic);
            device.draw_triangles_u32(0, self.tri_indices.len() as i32);
        }

        // Lines
        if !self.line_vertices.is_empty() {
            device.bind_program(self.color_program_id, &projection);
            device.bind_vao(self.line_vao);
            device.update_vao_main_vertices(self.line_vao,
                                            &self.line_vertices,
                                            VertexUsageHint::Dynamic);
            device.draw_nonindexed_lines(0, self.line_vertices.len() as i32);
        }

        // Glyph
        if !self.font_indices.is_empty() {
            device.bind_program(self.font_program_id, &projection);
            device.bind_texture(TextureSampler::Color0, self.font_texture_id);
            device.bind_vao(self.font_vao);
            device.update_vao_indices(self.font_vao,
                                      &self.font_indices,
                                      VertexUsageHint::Dynamic);
            device.update_vao_main_vertices(self.font_vao,
                                            &self.font_vertices,
                                            VertexUsageHint::Dynamic);
            device.draw_triangles_u32(0, self.font_indices.len() as i32);
        }

        self.font_indices.clear();
        self.font_vertices.clear();
        self.line_vertices.clear();
        self.tri_vertices.clear();
        self.tri_indices.clear();
    }
}
