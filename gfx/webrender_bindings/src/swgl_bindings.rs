/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use bindings::WrCompositor;
use gleam::{gl, gl::Gl};
use std::collections::hash_map::HashMap;
use std::os::raw::c_void;
use std::ptr;
use std::rc::Rc;
use webrender::{api::units::*, Compositor, CompositorCapabilities, NativeSurfaceId, NativeSurfaceInfo, NativeTileId};

#[no_mangle]
pub extern "C" fn wr_swgl_create_context() -> *mut c_void {
    swgl::Context::create().into()
}

#[no_mangle]
pub extern "C" fn wr_swgl_destroy_context(ctx: *mut c_void) {
    swgl::Context::from(ctx).destroy();
}

#[no_mangle]
pub extern "C" fn wr_swgl_make_current(ctx: *mut c_void) {
    swgl::Context::from(ctx).make_current();
}

#[no_mangle]
pub extern "C" fn wr_swgl_init_default_framebuffer(ctx: *mut c_void, width: i32, height: i32,
                                                   stride: i32, buf: *mut c_void) {
    swgl::Context::from(ctx).init_default_framebuffer(width, height, stride, buf);
}

#[derive(Debug)]
pub struct SwTile {
    x: i32,
    y: i32,
    fbo_id: u32,
    color_id: u32,
    tex_id: u32,
    pbo_id: u32,
    dirty_rect: DeviceIntRect,
    valid_rect: DeviceIntRect,
}

pub struct SwSurface {
    tile_size: DeviceIntSize,
    is_opaque: bool,
    tiles: Vec<SwTile>,
}

struct DrawTileHelper {
    gl: Rc<dyn gl::Gl>,
    prog: u32,
    quad_vbo: u32,
    quad_vao: u32,
    dest_matrix_loc: i32,
    tex_matrix_loc: i32,
}

impl DrawTileHelper {
    fn new(gl: Rc<dyn gl::Gl>) -> Self {
        let quad_vbo = gl.gen_buffers(1)[0];
        gl.bind_buffer(gl::ARRAY_BUFFER, quad_vbo);
        let quad_data: [f32; 8] = [0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 1.0, 1.0];
        gl::buffer_data(&*gl, gl::ARRAY_BUFFER, &quad_data, gl::STATIC_DRAW);

        let quad_vao = gl.gen_vertex_arrays(1)[0];
        gl.bind_vertex_array(quad_vao);
        gl.enable_vertex_attrib_array(0);
        gl.vertex_attrib_pointer(0, 2, gl::FLOAT, false, 0, 0);
        gl.bind_vertex_array(0);

        let version = match gl.get_type() {
            gl::GlType::Gl => "#version 150",
            gl::GlType::Gles => "#version 300 es",
        };
        let vert_source = "
            in vec2 aVert;
            uniform mat3 uDestMatrix;
            uniform mat3 uTexMatrix;
            out vec2 vTexCoord;
            void main(void) {
                gl_Position = vec4((uDestMatrix * vec3(aVert, 1.0)).xy, 0.0, 1.0);
                vTexCoord = (uTexMatrix * vec3(aVert, 1.0)).xy;
            }
        ";
        let vs = gl.create_shader(gl::VERTEX_SHADER);
        gl.shader_source(vs, &[version.as_bytes(), vert_source.as_bytes()]);
        gl.compile_shader(vs);
        let frag_source = "
            #ifdef GL_ES
                #ifdef GL_FRAGMENT_PRECISION_HIGH
                    precision highp float;
                #else
                    precision mediump float;
                #endif
            #endif
            in vec2 vTexCoord;
            out vec4 oFragColor;
            uniform sampler2D uTex;
            void main(void) {
                oFragColor = texture(uTex, vTexCoord);
            }
        ";
        let fs = gl.create_shader(gl::FRAGMENT_SHADER);
        gl.shader_source(fs, &[version.as_bytes(), frag_source.as_bytes()]);
        gl.compile_shader(fs);

        let prog = gl.create_program();
        gl.attach_shader(prog, vs);
        gl.attach_shader(prog, fs);
        gl.bind_attrib_location(prog, 0, "aVert");
        gl.link_program(prog);

        let mut status = [0];
        unsafe {
            gl.get_program_iv(prog, gl::LINK_STATUS, &mut status);
        }
        assert!(status[0] != 0);

        //println!("vert: {}", gl.get_shader_info_log(vs));
        //println!("frag: {}", gl.get_shader_info_log(fs));
        //println!("status: {}, {}", status[0], gl.get_program_info_log(prog));

        gl.use_program(prog);
        let dest_matrix_loc = gl.get_uniform_location(prog, "uDestMatrix");
        assert!(dest_matrix_loc != -1);
        let tex_matrix_loc = gl.get_uniform_location(prog, "uTexMatrix");
        assert!(tex_matrix_loc != -1);
        let tex_loc = gl.get_uniform_location(prog, "uTex");
        assert!(tex_loc != -1);
        gl.uniform_1i(tex_loc, 0);
        gl.use_program(0);

        gl.delete_shader(vs);
        gl.delete_shader(fs);

        DrawTileHelper {
            gl,
            prog,
            quad_vao,
            quad_vbo,
            dest_matrix_loc,
            tex_matrix_loc,
        }
    }

    fn deinit(&self) {
        self.gl.delete_program(self.prog);
        self.gl.delete_vertex_arrays(&[self.quad_vao]);
        self.gl.delete_buffers(&[self.quad_vbo]);
    }

    fn enable(&self, viewport: &DeviceIntRect) {
        self.gl.viewport(
            viewport.origin.x,
            viewport.origin.y,
            viewport.size.width,
            viewport.size.height,
        );
        self.gl.bind_vertex_array(self.quad_vao);
        self.gl.use_program(self.prog);
        self.gl.active_texture(gl::TEXTURE0);
    }

    fn draw(
        &self,
        viewport: &DeviceIntRect,
        dest: &DeviceIntRect,
        src: &DeviceIntRect,
        surface: &SwSurface,
        tile: &SwTile,
    ) {
        let dx = dest.origin.x as f32 / viewport.size.width as f32;
        let dy = dest.origin.y as f32 / viewport.size.height as f32;
        let dw = dest.size.width as f32 / viewport.size.width as f32;
        let dh = dest.size.height as f32 / viewport.size.height as f32;
        self.gl.uniform_matrix_3fv(
            self.dest_matrix_loc,
            false,
            &[
                2.0 * dw,
                0.0,
                0.0,
                0.0,
                -2.0 * dh,
                0.0,
                -1.0 + 2.0 * dx,
                1.0 - 2.0 * dy,
                1.0,
            ],
        );
        let sx = src.origin.x as f32 / surface.tile_size.width as f32;
        let sy = src.origin.y as f32 / surface.tile_size.height as f32;
        let sw = src.size.width as f32 / surface.tile_size.width as f32;
        let sh = src.size.height as f32 / surface.tile_size.height as f32;
        self.gl
            .uniform_matrix_3fv(self.tex_matrix_loc, false, &[sw, 0.0, 0.0, 0.0, sh, 0.0, sx, sy, 1.0]);
        self.gl.bind_texture(gl::TEXTURE_2D, tile.tex_id);
        self.gl.draw_arrays(gl::TRIANGLE_STRIP, 0, 4);
    }

    fn disable(&self) {
        self.gl.use_program(0);
        self.gl.bind_vertex_array(0);
    }
}

pub struct SwCompositor {
    gl: swgl::Context,
    native_gl: Option<Rc<dyn gl::Gl>>,
    compositor: Option<WrCompositor>,
    surfaces: HashMap<NativeSurfaceId, SwSurface>,
    frame_surfaces: Vec<(NativeSurfaceId, DeviceIntPoint, DeviceIntRect)>,
    cur_tile: NativeTileId,
    draw_tile: Option<DrawTileHelper>,
    /// The maximum tile size required for any of the allocated surfaces.
    max_tile_size: DeviceIntSize,
    /// Reuse the same depth texture amongst all tiles in all surfaces.
    /// This depth texture must be big enough to accommodate the largest used
    /// tile size for any surface. The maximum requested tile size is tracked
    /// to ensure that this depth texture is at least that big.
    depth_id: u32,
}

impl SwCompositor {
    pub fn new(gl: swgl::Context, native_gl: Option<Rc<dyn gl::Gl>>, compositor: Option<WrCompositor>) -> Self {
        let depth_id = gl.gen_textures(1)[0];
        SwCompositor {
            gl,
            compositor,
            surfaces: HashMap::new(),
            frame_surfaces: Vec::new(),
            cur_tile: NativeTileId {
                surface_id: NativeSurfaceId(0),
                x: 0,
                y: 0,
            },
            draw_tile: native_gl.as_ref().map(|gl| DrawTileHelper::new(gl.clone())),
            native_gl,
            max_tile_size: DeviceIntSize::zero(),
            depth_id,
        }
    }

    fn deinit_shader(&mut self) {
        if let Some(draw_tile) = &self.draw_tile {
            draw_tile.deinit();
        }
        self.draw_tile = None;
    }

    fn deinit_tile(&self, tile: &SwTile) {
        self.gl.delete_framebuffers(&[tile.fbo_id]);
        self.gl.delete_textures(&[tile.color_id]);
        if let Some(native_gl) = &self.native_gl {
            native_gl.delete_textures(&[tile.tex_id]);
            native_gl.delete_buffers(&[tile.pbo_id]);
        }
    }

    fn deinit_surface(&self, surface: &SwSurface) {
        for tile in &surface.tiles {
            self.deinit_tile(tile);
        }
    }
}

impl Compositor for SwCompositor {
    fn create_surface(
        &mut self,
        id: NativeSurfaceId,
        virtual_offset: DeviceIntPoint,
        tile_size: DeviceIntSize,
        is_opaque: bool,
    ) {
        if let Some(compositor) = &mut self.compositor {
            compositor.create_surface(id, virtual_offset, tile_size, is_opaque);
        }
        self.max_tile_size = DeviceIntSize::new(
            self.max_tile_size.width.max(tile_size.width),
            self.max_tile_size.height.max(tile_size.height),
        );
        self.surfaces.insert(
            id,
            SwSurface {
                tile_size,
                is_opaque,
                tiles: Vec::new(),
            },
        );
    }

    fn destroy_surface(&mut self, id: NativeSurfaceId) {
        if let Some(surface) = self.surfaces.remove(&id) {
            self.deinit_surface(&surface);
        }
        if let Some(compositor) = &mut self.compositor {
            compositor.destroy_surface(id);
        }
    }

    fn deinit(&mut self) {
        for surface in self.surfaces.values() {
            self.deinit_surface(surface);
        }

        self.gl.delete_textures(&[self.depth_id]);

        self.deinit_shader();

        if let Some(compositor) = &mut self.compositor {
            compositor.deinit();
        }
    }

    fn create_tile(&mut self, id: NativeTileId) {
        if let Some(compositor) = &mut self.compositor {
            compositor.create_tile(id);
        }
        if let Some(surface) = self.surfaces.get_mut(&id.surface_id) {
            let color_id = self.gl.gen_textures(1)[0];
            let fbo_id = self.gl.gen_framebuffers(1)[0];
            self.gl.bind_framebuffer(gl::DRAW_FRAMEBUFFER, fbo_id);
            self.gl
                .framebuffer_texture_2d(gl::DRAW_FRAMEBUFFER, gl::COLOR_ATTACHMENT0, gl::TEXTURE_2D, color_id, 0);
            self.gl
                .framebuffer_texture_2d(gl::DRAW_FRAMEBUFFER, gl::DEPTH_ATTACHMENT, gl::TEXTURE_2D, self.depth_id, 0);
            self.gl.bind_framebuffer(gl::DRAW_FRAMEBUFFER, 0);

            let mut tex_id = 0;
            let mut pbo_id = 0;
            if let Some(native_gl) = &self.native_gl {
                tex_id = native_gl.gen_textures(1)[0];
                native_gl.bind_texture(gl::TEXTURE_2D, tex_id);
                native_gl.tex_image_2d(
                    gl::TEXTURE_2D,
                    0,
                    gl::RGBA8 as gl::GLint,
                    surface.tile_size.width,
                    surface.tile_size.height,
                    0,
                    gl::RGBA,
                    gl::UNSIGNED_BYTE,
                    None,
                );
                native_gl.tex_parameter_i(gl::TEXTURE_2D, gl::TEXTURE_MIN_FILTER, gl::LINEAR as gl::GLint);
                native_gl.tex_parameter_i(gl::TEXTURE_2D, gl::TEXTURE_MAG_FILTER, gl::LINEAR as gl::GLint);
                native_gl.tex_parameter_i(gl::TEXTURE_2D, gl::TEXTURE_WRAP_S, gl::CLAMP_TO_EDGE as gl::GLint);
                native_gl.tex_parameter_i(gl::TEXTURE_2D, gl::TEXTURE_WRAP_T, gl::CLAMP_TO_EDGE as gl::GLint);
                native_gl.bind_texture(gl::TEXTURE_2D, 0);

                pbo_id = native_gl.gen_buffers(1)[0];
                native_gl.bind_buffer(gl::PIXEL_UNPACK_BUFFER, pbo_id);
                native_gl.buffer_data_untyped(
                    gl::PIXEL_UNPACK_BUFFER,
                    surface.tile_size.area() as isize * 4 + 16,
                    ptr::null(),
                    gl::DYNAMIC_DRAW,
                );
                native_gl.bind_buffer(gl::PIXEL_UNPACK_BUFFER, 0);
            }

            surface.tiles.push(SwTile {
                x: id.x,
                y: id.y,
                fbo_id,
                color_id,
                tex_id,
                pbo_id,
                dirty_rect: DeviceIntRect::zero(),
                valid_rect: DeviceIntRect::zero(),
            });
        }
    }

    fn destroy_tile(&mut self, id: NativeTileId) {
        if let Some(surface) = self.surfaces.get_mut(&id.surface_id) {
            if let Some(idx) = surface.tiles.iter().position(|t| t.x == id.x && t.y == id.y) {
                let tile = surface.tiles.remove(idx);
                self.deinit_tile(&tile);
            }
        }
        if let Some(compositor) = &mut self.compositor {
            compositor.destroy_tile(id);
        }
    }

    fn bind(&mut self, id: NativeTileId, dirty_rect: DeviceIntRect, valid_rect: DeviceIntRect) -> NativeSurfaceInfo {
        let mut surface_info = NativeSurfaceInfo {
            origin: DeviceIntPoint::zero(),
            fbo_id: 0,
        };

        self.cur_tile = id;

        if let Some(surface) = self.surfaces.get_mut(&id.surface_id) {
            if let Some(tile) = surface.tiles.iter_mut().find(|t| t.x == id.x && t.y == id.y) {
                tile.dirty_rect = dirty_rect;
                tile.valid_rect = valid_rect;
                if valid_rect.is_empty() {
                    return surface_info;
                }

                let mut stride = 0;
                let mut buf = ptr::null_mut();
                if let Some(compositor) = &mut self.compositor {
                    if let Some(tile_info) = compositor.map_tile(id, dirty_rect, valid_rect) {
                        stride = tile_info.stride;
                        buf = tile_info.data;
                    }
                } else if let Some(native_gl) = &self.native_gl {
                    if tile.pbo_id != 0 {
                        native_gl.bind_buffer(gl::PIXEL_UNPACK_BUFFER, tile.pbo_id);
                        buf = native_gl.map_buffer_range(
                            gl::PIXEL_UNPACK_BUFFER,
                            0,
                            valid_rect.size.area() as isize * 4 + 16,
                            gl::MAP_WRITE_BIT | gl::MAP_INVALIDATE_BUFFER_BIT,
                        ); // | gl::MAP_UNSYNCHRONIZED_BIT);
                        if buf != ptr::null_mut() {
                            stride = valid_rect.size.width * 4;
                        } else {
                            native_gl.bind_buffer(gl::PIXEL_UNPACK_BUFFER, 0);
                            native_gl.delete_buffers(&[tile.pbo_id]);
                            tile.pbo_id = 0;
                        }
                    }
                }
                self.gl.set_texture_buffer(
                    tile.color_id,
                    gl::RGBA8,
                    valid_rect.size.width,
                    valid_rect.size.height,
                    stride,
                    buf,
                    surface.tile_size.width,
                    surface.tile_size.height,
                );
                // Reallocate the shared depth buffer to fit the valid rect, but within
                // a buffer sized to actually fit at least the maximum possible tile size.
                // The maximum tile size is supplied to avoid reallocation by ensuring the
                // allocated buffer is actually big enough to accommodate the largest tile
                // size requested by any used surface, even though supplied valid rect may
                // actually be much smaller than this. This will only force a texture
                // reallocation inside SWGL if the maximum tile size has grown since the
                // last time it was supplied, instead simply reusing the buffer if the max
                // tile size is not bigger than what was previously allocated.
                self.gl.set_texture_buffer(
                    self.depth_id,
                    gl::DEPTH_COMPONENT16,
                    valid_rect.size.width,
                    valid_rect.size.height,
                    0,
                    ptr::null_mut(),
                    self.max_tile_size.width,
                    self.max_tile_size.height,
                );
                surface_info.fbo_id = tile.fbo_id;
                surface_info.origin -= valid_rect.origin.to_vector();
            }
        }

        surface_info
    }

    fn unbind(&mut self) {
        let id = self.cur_tile;
        if let Some(surface) = self.surfaces.get_mut(&id.surface_id) {
            if let Some(tile) = surface.tiles.iter().find(|t| t.x == id.x && t.y == id.y) {
                if tile.valid_rect.is_empty() {
                    return;
                }

                if let Some(compositor) = &mut self.compositor {
                    compositor.unmap_tile();
                    return;
                }

                let native_gl = match &self.native_gl {
                    Some(native_gl) => native_gl,
                    None => return,
                };

                let (swbuf, _, _, stride) = self.gl.get_color_buffer(tile.fbo_id, true);
                assert!(stride % 4 == 0);
                let buf = if tile.pbo_id != 0 {
                    native_gl.unmap_buffer(gl::PIXEL_UNPACK_BUFFER);
                    0 as *mut c_void
                } else {
                    swbuf
                };
                let dirty = tile.dirty_rect;
                let src = unsafe {
                    (buf as *mut u32).offset(
                        (dirty.origin.y - tile.valid_rect.origin.y) as isize * (stride / 4) as isize
                            + (dirty.origin.x - tile.valid_rect.origin.x) as isize,
                    )
                };
                native_gl.active_texture(gl::TEXTURE0);
                native_gl.bind_texture(gl::TEXTURE_2D, tile.tex_id);
                native_gl.pixel_store_i(gl::UNPACK_ROW_LENGTH, stride / 4);
                native_gl.tex_sub_image_2d_pbo(
                    gl::TEXTURE_2D,
                    0,
                    dirty.origin.x,
                    dirty.origin.y,
                    dirty.size.width,
                    dirty.size.height,
                    gl::BGRA,
                    gl::UNSIGNED_BYTE,
                    src as _,
                );
                native_gl.pixel_store_i(gl::UNPACK_ROW_LENGTH, 0);
                if tile.pbo_id != 0 {
                    native_gl.bind_buffer(gl::PIXEL_UNPACK_BUFFER, 0);
                }

                if let Some(compositor) = &mut self.compositor {
                    let info = compositor.bind(id, tile.dirty_rect, tile.valid_rect);
                    native_gl.bind_framebuffer(gl::DRAW_FRAMEBUFFER, info.fbo_id);

                    let viewport = dirty.translate(info.origin.to_vector());
                    let draw_tile = self.draw_tile.as_ref().unwrap();
                    draw_tile.enable(&viewport);
                    draw_tile.draw(&viewport, &viewport, &dirty, surface, tile);
                    draw_tile.disable();

                    native_gl.bind_framebuffer(gl::DRAW_FRAMEBUFFER, 0);

                    compositor.unbind();
                }

                native_gl.bind_texture(gl::TEXTURE_2D, 0);
            }
        }
    }

    fn begin_frame(&mut self) {
        if let Some(compositor) = &mut self.compositor {
            compositor.begin_frame();
        }
        self.frame_surfaces.clear();
    }

    fn add_surface(&mut self, id: NativeSurfaceId, position: DeviceIntPoint, clip_rect: DeviceIntRect) {
        if let Some(compositor) = &mut self.compositor {
            compositor.add_surface(id, position, clip_rect);
        }
        self.frame_surfaces.push((id, position, clip_rect));
    }

    fn end_frame(&mut self) {
        if let Some(compositor) = &mut self.compositor {
            compositor.end_frame();
        } else if let Some(native_gl) = &self.native_gl {
            let (_, fw, fh, _) = self.gl.get_color_buffer(0, false);
            let viewport = DeviceIntRect::from_size(DeviceIntSize::new(fw, fh));
            let draw_tile = self.draw_tile.as_ref().unwrap();
            draw_tile.enable(&viewport);
            let mut blend = false;
            native_gl.blend_func(gl::ONE, gl::ONE_MINUS_SRC_ALPHA);
            for &(ref id, position, ref clip_rect) in &self.frame_surfaces {
                if let Some(surface) = self.surfaces.get(id) {
                    if surface.is_opaque {
                        if blend {
                            native_gl.disable(gl::BLEND);
                            blend = false;
                        }
                    } else if !blend {
                        native_gl.enable(gl::BLEND);
                        blend = true;
                    }
                    for tile in &surface.tiles {
                        let tile_pos =
                            DeviceIntPoint::new(tile.x * surface.tile_size.width, tile.y * surface.tile_size.height)
                                + position.to_vector();
                        if let Some(rect) = tile.valid_rect.translate(tile_pos.to_vector()).intersection(clip_rect) {
                            draw_tile.draw(&viewport, &rect, &rect.translate(-tile_pos.to_vector()), surface, tile);
                        }
                    }
                }
            }
            if blend {
                native_gl.disable(gl::BLEND);
            }
            draw_tile.disable();
        } else {
            for &(ref id, position, ref clip_rect) in &self.frame_surfaces {
                if let Some(surface) = self.surfaces.get(id) {
                    for tile in &surface.tiles {
                        let tile_pos =
                            DeviceIntPoint::new(tile.x * surface.tile_size.width, tile.y * surface.tile_size.height)
                                + position.to_vector();
                        let valid = tile.valid_rect.translate(tile_pos.to_vector());
                        if let Some(rect) = valid.intersection(clip_rect) {
                            self.gl.composite(
                                tile.color_id,
                                rect.min_x() - valid.origin.x,
                                rect.min_y() - valid.origin.y,
                                rect.size.width,
                                rect.size.height,
                                rect.min_x(),
                                rect.min_y(),
                                surface.is_opaque,
                                false,
                            );
                        }
                    }
                }
            }
        }
    }

    fn enable_native_compositor(&mut self, enable: bool) {
        if let Some(compositor) = &mut self.compositor {
            compositor.enable_native_compositor(enable);
        }
    }

    fn get_capabilities(&self) -> CompositorCapabilities {
        if let Some(compositor) = &self.compositor {
            compositor.get_capabilities()
        } else {
            CompositorCapabilities {
                virtual_surface_size: 0,
            }
        }
    }
}
