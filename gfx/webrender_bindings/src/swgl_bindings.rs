/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use gleam::gl::Gl;

use std::os::raw::c_void;

#[no_mangle]
pub extern "C" fn wr_swgl_create_context() -> *mut c_void {
    swgl::Context::create().into()
}

#[no_mangle]
pub extern "C" fn wr_swgl_reference_context(ctx: *mut c_void) {
    swgl::Context::from(ctx).reference();
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
pub extern "C" fn wr_swgl_init_default_framebuffer(
    ctx: *mut c_void,
    x: i32,
    y: i32,
    width: i32,
    height: i32,
    stride: i32,
    buf: *mut c_void,
) {
    swgl::Context::from(ctx).init_default_framebuffer(x, y, width, height, stride, buf);
}

#[no_mangle]
pub extern "C" fn wr_swgl_gen_texture(ctx: *mut c_void) -> u32 {
    swgl::Context::from(ctx).gen_textures(1)[0]
}

#[no_mangle]
pub extern "C" fn wr_swgl_delete_texture(ctx: *mut c_void, tex: u32) {
    swgl::Context::from(ctx).delete_textures(&[tex]);
}

#[no_mangle]
pub extern "C" fn wr_swgl_set_texture_parameter(ctx: *mut c_void, tex: u32, pname: u32, param: i32) {
    swgl::Context::from(ctx).set_texture_parameter(tex, pname, param);
}

#[no_mangle]
pub extern "C" fn wr_swgl_set_texture_buffer(
    ctx: *mut c_void,
    tex: u32,
    internal_format: u32,
    width: i32,
    height: i32,
    stride: i32,
    buf: *mut c_void,
    min_width: i32,
    min_height: i32,
) {
    swgl::Context::from(ctx).set_texture_buffer(
        tex,
        internal_format,
        width,
        height,
        stride,
        buf,
        min_width,
        min_height,
    );
}

#[no_mangle]
pub extern "C" fn wr_swgl_clear_color_rect(
    ctx: *mut c_void,
    fbo: u32,
    x: i32,
    y: i32,
    width: i32,
    height: i32,
    r: f32,
    g: f32,
    b: f32,
    a: f32,
) {
    swgl::Context::from(ctx).clear_color_rect(fbo, x, y, width, height, r, g, b, a);
}
