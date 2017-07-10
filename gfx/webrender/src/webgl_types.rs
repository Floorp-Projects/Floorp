/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! A set of WebGL-related types, in their own module so it's easy to
//! compile it off.

use gleam::gl;
use offscreen_gl_context::{NativeGLContext, NativeGLContextHandle};
use offscreen_gl_context::{GLContext, NativeGLContextMethods, GLContextDispatcher};
use offscreen_gl_context::{OSMesaContext, OSMesaContextHandle};
use offscreen_gl_context::{ColorAttachmentType, GLContextAttributes, GLLimits};
use api::{WebGLCommand, DeviceIntSize};

pub enum GLContextHandleWrapper {
    Native(NativeGLContextHandle),
    OSMesa(OSMesaContextHandle),
}

impl GLContextHandleWrapper {
    pub fn current_native_handle() -> Option<GLContextHandleWrapper> {
        NativeGLContext::current_handle().map(GLContextHandleWrapper::Native)
    }

    pub fn current_osmesa_handle() -> Option<GLContextHandleWrapper> {
        OSMesaContext::current_handle().map(GLContextHandleWrapper::OSMesa)
    }

    pub fn new_context(&self,
                       size: DeviceIntSize,
                       attributes: GLContextAttributes,
                       dispatcher: Option<Box<GLContextDispatcher>>) -> Result<GLContextWrapper, &'static str> {
        match *self {
            GLContextHandleWrapper::Native(ref handle) => {
                let ctx = GLContext::<NativeGLContext>::new_shared_with_dispatcher(size.to_untyped(),
                                                                                   attributes,
                                                                                   ColorAttachmentType::Texture,
                                                                                   gl::GlType::default(),
                                                                                   Some(handle),
                                                                                   dispatcher);
                ctx.map(GLContextWrapper::Native)
            }
            GLContextHandleWrapper::OSMesa(ref handle) => {
                let ctx = GLContext::<OSMesaContext>::new_shared_with_dispatcher(size.to_untyped(),
                                                                                 attributes,
                                                                                 ColorAttachmentType::Texture,
                                                                                 gl::GlType::default(),
                                                                                 Some(handle),
                                                                                 dispatcher);
                ctx.map(GLContextWrapper::OSMesa)
            }
        }
    }
}

pub enum GLContextWrapper {
    Native(GLContext<NativeGLContext>),
    OSMesa(GLContext<OSMesaContext>),
}

impl GLContextWrapper {
    pub fn make_current(&self) {
        match *self {
            GLContextWrapper::Native(ref ctx) => {
                ctx.make_current().unwrap();
            }
            GLContextWrapper::OSMesa(ref ctx) => {
                ctx.make_current().unwrap();
            }
        }
    }

    pub fn unbind(&self) {
        match *self {
            GLContextWrapper::Native(ref ctx) => {
                ctx.unbind().unwrap();
            }
            GLContextWrapper::OSMesa(ref ctx) => {
                ctx.unbind().unwrap();
            }
        }
    }

    pub fn apply_command(&self, cmd: WebGLCommand) {
        match *self {
            GLContextWrapper::Native(ref ctx) => {
                cmd.apply(ctx);
            }
            GLContextWrapper::OSMesa(ref ctx) => {
                cmd.apply(ctx);
            }
        }
    }

    pub fn get_info(&self) -> (DeviceIntSize, u32, GLLimits) {
        match *self {
            GLContextWrapper::Native(ref ctx) => {
                let (real_size, texture_id) = {
                    let draw_buffer = ctx.borrow_draw_buffer().unwrap();
                    (draw_buffer.size(), draw_buffer.get_bound_texture_id().unwrap())
                };

                let limits = ctx.borrow_limits().clone();

                (DeviceIntSize::from_untyped(&real_size), texture_id, limits)
            }
            GLContextWrapper::OSMesa(ref ctx) => {
                let (real_size, texture_id) = {
                    let draw_buffer = ctx.borrow_draw_buffer().unwrap();
                    (draw_buffer.size(), draw_buffer.get_bound_texture_id().unwrap())
                };

                let limits = ctx.borrow_limits().clone();

                (DeviceIntSize::from_untyped(&real_size), texture_id, limits)
            }
        }
    }

    pub fn resize(&mut self, size: &DeviceIntSize) -> Result<(), &'static str> {
        match *self {
            GLContextWrapper::Native(ref mut ctx) => {
                ctx.resize(size.to_untyped())
            }
            GLContextWrapper::OSMesa(ref mut ctx) => {
                ctx.resize(size.to_untyped())
            }
        }
    }
}
