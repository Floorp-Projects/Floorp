/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use channel::MsgSender;
use gleam::gl;

#[cfg(feature = "nightly")]
use core::nonzero::NonZero;

use offscreen_gl_context::{GLContext, NativeGLContextMethods, GLContextAttributes};
use std::fmt;

#[derive(Clone, Deserialize, Serialize)]
pub enum WebGLCommand {
    GetContextAttributes(MsgSender<GLContextAttributes>),
    ActiveTexture(u32),
    BlendColor(f32, f32, f32, f32),
    BlendEquation(u32),
    BlendEquationSeparate(u32, u32),
    BlendFunc(u32, u32),
    BlendFuncSeparate(u32, u32, u32, u32),
    AttachShader(WebGLProgramId, WebGLShaderId),
    DetachShader(WebGLProgramId, WebGLShaderId),
    BindAttribLocation(WebGLProgramId, u32, String),
    BufferData(u32, Vec<u8>, u32),
    BufferSubData(u32, isize, Vec<u8>),
    Clear(u32),
    ClearColor(f32, f32, f32, f32),
    ClearDepth(f64),
    ClearStencil(i32),
    ColorMask(bool, bool, bool, bool),
    CullFace(u32),
    FrontFace(u32),
    DepthFunc(u32),
    DepthMask(bool),
    DepthRange(f64, f64),
    Enable(u32),
    Disable(u32),
    CompileShader(WebGLShaderId, String),
    CopyTexImage2D(u32, i32, u32, i32, i32, i32, i32, i32),
    CopyTexSubImage2D(u32, i32, i32, i32, i32, i32, i32, i32),
    CreateBuffer(MsgSender<Option<WebGLBufferId>>),
    CreateFramebuffer(MsgSender<Option<WebGLFramebufferId>>),
    CreateRenderbuffer(MsgSender<Option<WebGLRenderbufferId>>),
    CreateTexture(MsgSender<Option<WebGLTextureId>>),
    CreateProgram(MsgSender<Option<WebGLProgramId>>),
    CreateShader(u32, MsgSender<Option<WebGLShaderId>>),
    DeleteBuffer(WebGLBufferId),
    DeleteFramebuffer(WebGLFramebufferId),
    DeleteRenderbuffer(WebGLRenderbufferId),
    DeleteTexture(WebGLTextureId),
    DeleteProgram(WebGLProgramId),
    DeleteShader(WebGLShaderId),
    BindBuffer(u32, Option<WebGLBufferId>),
    BindFramebuffer(u32, WebGLFramebufferBindingRequest),
    BindRenderbuffer(u32, Option<WebGLRenderbufferId>),
    BindTexture(u32, Option<WebGLTextureId>),
    DisableVertexAttribArray(u32),
    DrawArrays(u32, i32, i32),
    DrawElements(u32, i32, u32, i64),
    EnableVertexAttribArray(u32),
    FramebufferRenderbuffer(u32, u32, u32, Option<WebGLRenderbufferId>),
    FramebufferTexture2D(u32, u32, u32, Option<WebGLTextureId>, i32),
    GetBufferParameter(u32, u32, MsgSender<WebGLResult<WebGLParameter>>),
    GetExtensions(MsgSender<String>),
    GetParameter(u32, MsgSender<WebGLResult<WebGLParameter>>),
    GetProgramParameter(WebGLProgramId, u32, MsgSender<WebGLResult<WebGLParameter>>),
    GetShaderParameter(WebGLShaderId, u32, MsgSender<WebGLResult<WebGLParameter>>),
    GetShaderPrecisionFormat(u32, u32, MsgSender<WebGLResult<(i32, i32, i32)>>),
    GetActiveAttrib(WebGLProgramId, u32, MsgSender<WebGLResult<(i32, u32, String)>>),
    GetActiveUniform(WebGLProgramId, u32, MsgSender<WebGLResult<(i32, u32, String)>>),
    GetAttribLocation(WebGLProgramId, String, MsgSender<Option<i32>>),
    GetUniformLocation(WebGLProgramId, String, MsgSender<Option<i32>>),
    GetVertexAttrib(u32, u32, MsgSender<WebGLResult<WebGLParameter>>),
    GetVertexAttribOffset(u32, u32, MsgSender<WebGLResult<isize>>),
    GetShaderInfoLog(WebGLShaderId, MsgSender<String>),
    GetProgramInfoLog(WebGLProgramId, MsgSender<String>),
    PolygonOffset(f32, f32),
    RenderbufferStorage(u32, u32, i32, i32),
    ReadPixels(i32, i32, i32, i32, u32, u32, MsgSender<Vec<u8>>),
    SampleCoverage(f32, bool),
    Scissor(i32, i32, i32, i32),
    StencilFunc(u32, i32, u32),
    StencilFuncSeparate(u32, u32, i32, u32),
    StencilMask(u32),
    StencilMaskSeparate(u32, u32),
    StencilOp(u32, u32, u32),
    StencilOpSeparate(u32, u32, u32, u32),
    Hint(u32, u32),
    IsEnabled(u32, MsgSender<bool>),
    LineWidth(f32),
    PixelStorei(u32, i32),
    LinkProgram(WebGLProgramId),
    Uniform1f(i32, f32),
    Uniform1fv(i32, Vec<f32>),
    Uniform1i(i32, i32),
    Uniform1iv(i32, Vec<i32>),
    Uniform2f(i32, f32, f32),
    Uniform2fv(i32, Vec<f32>),
    Uniform2i(i32, i32, i32),
    Uniform2iv(i32, Vec<i32>),
    Uniform3f(i32, f32, f32, f32),
    Uniform3fv(i32, Vec<f32>),
    Uniform3i(i32, i32, i32, i32),
    Uniform3iv(i32, Vec<i32>),
    Uniform4f(i32, f32, f32, f32, f32),
    Uniform4fv(i32, Vec<f32>),
    Uniform4i(i32, i32, i32, i32, i32),
    Uniform4iv(i32, Vec<i32>),
    UniformMatrix2fv(i32, bool, Vec<f32>),
    UniformMatrix3fv(i32, bool, Vec<f32>),
    UniformMatrix4fv(i32, bool, Vec<f32>),
    UseProgram(WebGLProgramId),
    ValidateProgram(WebGLProgramId),
    VertexAttrib(u32, f32, f32, f32, f32),
    VertexAttribPointer(u32, i32, u32, bool, i32, u32),
    VertexAttribPointer2f(u32, i32, bool, i32, u32),
    Viewport(i32, i32, i32, i32),
    TexImage2D(u32, i32, i32, i32, i32, u32, u32, Vec<u8>),
    TexParameteri(u32, u32, i32),
    TexParameterf(u32, u32, f32),
    TexSubImage2D(u32, i32, i32, i32, i32, i32, u32, u32, Vec<u8>),
    DrawingBufferWidth(MsgSender<i32>),
    DrawingBufferHeight(MsgSender<i32>),
    Finish(MsgSender<()>),
    Flush,
    GenerateMipmap(u32),
    CreateVertexArray(MsgSender<Option<WebGLVertexArrayId>>),
    DeleteVertexArray(WebGLVertexArrayId),
    BindVertexArray(Option<WebGLVertexArrayId>),
}

#[cfg(feature = "nightly")]
macro_rules! define_resource_id_struct {
    ($name:ident) => {
        #[derive(Clone, Copy, Eq, Hash, PartialEq)]
        pub struct $name(NonZero<u32>);

        impl $name {
            #[inline]
            unsafe fn new(id: u32) -> Self {
                $name(NonZero::new(id))
            }

            #[inline]
            fn get(self) -> u32 {
                *self.0
            }
        }

    };
}

#[cfg(not(feature = "nightly"))]
macro_rules! define_resource_id_struct {
    ($name:ident) => {
        #[derive(Clone, Copy, Eq, Hash, PartialEq)]
        pub struct $name(u32);

        impl $name {
            #[inline]
            unsafe fn new(id: u32) -> Self {
                $name(id)
            }

            #[inline]
            fn get(self) -> u32 {
                self.0
            }
        }
    };
}

macro_rules! define_resource_id {
    ($name:ident) => {
        define_resource_id_struct!($name);

        impl<'de> ::serde::Deserialize<'de> for $name {
            fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
                where D: ::serde::Deserializer<'de>
            {
                let id = try!(u32::deserialize(deserializer));
                if id == 0 {
                    Err(::serde::de::Error::custom("expected a non-zero value"))
                } else {
                    Ok(unsafe { $name::new(id) })
                }
            }
        }

        impl ::serde::Serialize for $name {
            fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
                where S: ::serde::Serializer
            {
                self.get().serialize(serializer)
            }
        }

        impl ::std::fmt::Debug for $name {
            fn fmt(&self, fmt: &mut ::std::fmt::Formatter)
                  -> Result<(), ::std::fmt::Error> {
                fmt.debug_tuple(stringify!($name))
                   .field(&self.get())
                   .finish()
            }
        }

        impl ::std::fmt::Display for $name {
            fn fmt(&self, fmt: &mut ::std::fmt::Formatter)
                  -> Result<(), ::std::fmt::Error> {
                write!(fmt, "{}", self.get())
            }
        }

        impl ::heapsize::HeapSizeOf for $name {
            fn heap_size_of_children(&self) -> usize { 0 }
        }
    }
}

define_resource_id!(WebGLBufferId);
define_resource_id!(WebGLFramebufferId);
define_resource_id!(WebGLRenderbufferId);
define_resource_id!(WebGLTextureId);
define_resource_id!(WebGLProgramId);
define_resource_id!(WebGLShaderId);
define_resource_id!(WebGLVertexArrayId);

#[derive(Clone, Copy, Debug, Deserialize, Eq, Hash, Ord, PartialEq, PartialOrd, Serialize)]
pub struct WebGLContextId(pub usize);

impl ::heapsize::HeapSizeOf for WebGLContextId {
    fn heap_size_of_children(&self) -> usize { 0 }
}

#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub enum WebGLError {
    InvalidEnum,
    InvalidFramebufferOperation,
    InvalidOperation,
    InvalidValue,
    OutOfMemory,
    ContextLost,
}

#[derive(Clone, Debug, Deserialize, Serialize)]
pub enum WebGLFramebufferBindingRequest {
    Explicit(WebGLFramebufferId),
    Default,
}

#[derive(Clone, Debug, Deserialize, Serialize)]
pub enum WebGLParameter {
    Int(i32),
    Bool(bool),
    String(String),
    Float(f32),
    FloatArray(Vec<f32>),
    Invalid,
}

pub type WebGLResult<T> = Result<T, WebGLError>;

#[derive(Clone, Debug, Deserialize, Serialize)]
pub enum WebGLShaderParameter {
    Int(i32),
    Bool(bool),
    Invalid,
}

impl fmt::Debug for WebGLCommand {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        use WebGLCommand::*;
        let name = match *self {
            GetContextAttributes(..) => "GetContextAttributes",
            ActiveTexture(..) => "ActiveTexture",
            BlendColor(..) => "BlendColor",
            BlendEquation(..) => "BlendEquation",
            BlendEquationSeparate(..) => "BlendEquationSeparate",
            BlendFunc(..) => "BlendFunc",
            BlendFuncSeparate(..) => "BlendFuncSeparate",
            AttachShader(..) => "AttachShader",
            DetachShader(..) => "DetachShader",
            BindAttribLocation(..) => "BindAttribLocation",
            BufferData(..) => "BufferData",
            BufferSubData(..) => "BufferSubData",
            Clear(..) => "Clear",
            ClearColor(..) => "ClearColor",
            ClearDepth(..) => "ClearDepth",
            ClearStencil(..) => "ClearStencil",
            ColorMask(..) => "ColorMask",
            CopyTexImage2D(..) => "CopyTexImage2D",
            CopyTexSubImage2D(..) => "CopyTexSubImage2D",
            CullFace(..) => "CullFace",
            FrontFace(..) => "FrontFace",
            DepthFunc(..) => "DepthFunc",
            DepthMask(..) => "DepthMask",
            DepthRange(..) => "DepthRange",
            Enable(..) => "Enable",
            Disable(..) => "Disable",
            CompileShader(..) => "CompileShader",
            CreateBuffer(..) => "CreateBuffer",
            CreateFramebuffer(..) => "CreateFramebuffer",
            CreateRenderbuffer(..) => "CreateRenderbuffer",
            CreateTexture(..) => "CreateTexture",
            CreateProgram(..) => "CreateProgram",
            CreateShader(..) => "CreateShader",
            DeleteBuffer(..) => "DeleteBuffer",
            DeleteFramebuffer(..) => "DeleteFramebuffer",
            DeleteRenderbuffer(..) => "DeleteRenderBuffer",
            DeleteTexture(..) => "DeleteTexture",
            DeleteProgram(..) => "DeleteProgram",
            DeleteShader(..) => "DeleteShader",
            BindBuffer(..) => "BindBuffer",
            BindFramebuffer(..) => "BindFramebuffer",
            BindRenderbuffer(..) => "BindRenderbuffer",
            BindTexture(..) => "BindTexture",
            DisableVertexAttribArray(..) => "DisableVertexAttribArray",
            DrawArrays(..) => "DrawArrays",
            DrawElements(..) => "DrawElements",
            EnableVertexAttribArray(..) => "EnableVertexAttribArray",
            FramebufferRenderbuffer(..) => "FramebufferRenderbuffer",
            FramebufferTexture2D(..) => "FramebufferTexture2D",
            GetBufferParameter(..) => "GetBufferParameter",
            GetExtensions(..) => "GetExtensions",
            GetParameter(..) => "GetParameter",
            GetProgramParameter(..) => "GetProgramParameter",
            GetShaderParameter(..) => "GetShaderParameter",
            GetShaderPrecisionFormat(..) => "GetShaderPrecisionFormat",
            GetActiveAttrib(..) => "GetActiveAttrib",
            GetActiveUniform(..) => "GetActiveUniform",
            GetAttribLocation(..) => "GetAttribLocation",
            GetUniformLocation(..) => "GetUniformLocation",
            GetShaderInfoLog(..) => "GetShaderInfoLog",
            GetProgramInfoLog(..) => "GetProgramInfoLog",
            GetVertexAttrib(..) => "GetVertexAttrib",
            GetVertexAttribOffset(..) => "GetVertexAttribOffset",
            PolygonOffset(..) => "PolygonOffset",
            ReadPixels(..) => "ReadPixels",
            RenderbufferStorage(..) => "RenderbufferStorage",
            SampleCoverage(..) => "SampleCoverage",
            Scissor(..) => "Scissor",
            StencilFunc(..) => "StencilFunc",
            StencilFuncSeparate(..) => "StencilFuncSeparate",
            StencilMask(..) => "StencilMask",
            StencilMaskSeparate(..) => "StencilMaskSeparate",
            StencilOp(..) => "StencilOp",
            StencilOpSeparate(..) => "StencilOpSeparate",
            Hint(..) => "Hint",
            IsEnabled(..) => "IsEnabled",
            LineWidth(..) => "LineWidth",
            PixelStorei(..) => "PixelStorei",
            LinkProgram(..) => "LinkProgram",
            Uniform1f(..) => "Uniform1f",
            Uniform1fv(..) => "Uniform1fv",
            Uniform1i(..) => "Uniform1i",
            Uniform1iv(..) => "Uniform1iv",
            Uniform2f(..) => "Uniform2f",
            Uniform2fv(..) => "Uniform2fv",
            Uniform2i(..) => "Uniform2i",
            Uniform2iv(..) => "Uniform2iv",
            Uniform3f(..) => "Uniform3f",
            Uniform3fv(..) => "Uniform3fv",
            Uniform3i(..) => "Uniform3i",
            Uniform3iv(..) => "Uniform3iv",
            Uniform4f(..) => "Uniform4f",
            Uniform4fv(..) => "Uniform4fv",
            Uniform4i(..) => "Uniform4i",
            Uniform4iv(..) => "Uniform4iv",
            UniformMatrix2fv(..) => "UniformMatrix2fv",
            UniformMatrix3fv(..) => "UniformMatrix3fv",
            UniformMatrix4fv(..) => "UniformMatrix4fv",
            UseProgram(..) => "UseProgram",
            ValidateProgram(..) => "ValidateProgram",
            VertexAttrib(..) => "VertexAttrib",
            VertexAttribPointer2f(..) => "VertexAttribPointer2f",
            VertexAttribPointer(..) => "VertexAttribPointer",
            Viewport(..) => "Viewport",
            TexImage2D(..) => "TexImage2D",
            TexParameteri(..) => "TexParameteri",
            TexParameterf(..) => "TexParameterf",
            TexSubImage2D(..) => "TexSubImage2D",
            DrawingBufferWidth(..) => "DrawingBufferWidth",
            DrawingBufferHeight(..) => "DrawingBufferHeight",
            Finish(..) => "Finish",
            Flush => "Flush",
            GenerateMipmap(..) => "GenerateMipmap",
            CreateVertexArray(..) => "CreateVertexArray",
            DeleteVertexArray(..) => "DeleteVertexArray",
            BindVertexArray(..) => "BindVertexArray"
        };

        write!(f, "CanvasWebGLMsg::{}(..)", name)
    }
}

impl WebGLCommand {
    /// NOTE: This method consumes the command
    pub fn apply<Native: NativeGLContextMethods>(self, ctx: &GLContext<Native>) {
        match self {
            WebGLCommand::GetContextAttributes(sender) =>
                sender.send(*ctx.borrow_attributes()).unwrap(),
            WebGLCommand::ActiveTexture(target) =>
                ctx.gl().active_texture(target),
            WebGLCommand::AttachShader(program_id, shader_id) =>
                ctx.gl().attach_shader(program_id.get(), shader_id.get()),
            WebGLCommand::DetachShader(program_id, shader_id) =>
                ctx.gl().detach_shader(program_id.get(), shader_id.get()),
            WebGLCommand::BindAttribLocation(program_id, index, name) =>
                ctx.gl().bind_attrib_location(program_id.get(), index, &name),
            WebGLCommand::BlendColor(r, g, b, a) =>
                ctx.gl().blend_color(r, g, b, a),
            WebGLCommand::BlendEquation(mode) =>
                ctx.gl().blend_equation(mode),
            WebGLCommand::BlendEquationSeparate(mode_rgb, mode_alpha) =>
                ctx.gl().blend_equation_separate(mode_rgb, mode_alpha),
            WebGLCommand::BlendFunc(src, dest) =>
                ctx.gl().blend_func(src, dest),
            WebGLCommand::BlendFuncSeparate(src_rgb, dest_rgb, src_alpha, dest_alpha) =>
                ctx.gl().blend_func_separate(src_rgb, dest_rgb, src_alpha, dest_alpha),
            WebGLCommand::BufferData(buffer_type, data, usage) =>
                gl::buffer_data(ctx.gl(), buffer_type, &data, usage),
            WebGLCommand::BufferSubData(buffer_type, offset, data) =>
                gl::buffer_sub_data(ctx.gl(), buffer_type, offset, &data),
            WebGLCommand::Clear(mask) =>
                ctx.gl().clear(mask),
            WebGLCommand::ClearColor(r, g, b, a) =>
                ctx.gl().clear_color(r, g, b, a),
            WebGLCommand::ClearDepth(depth) =>
                ctx.gl().clear_depth(depth),
            WebGLCommand::ClearStencil(stencil) =>
                ctx.gl().clear_stencil(stencil),
            WebGLCommand::ColorMask(r, g, b, a) =>
                ctx.gl().color_mask(r, g, b, a),
            WebGLCommand::CopyTexImage2D(target, level, internal_format, x, y, width, height, border) =>
                ctx.gl().copy_tex_image_2d(target, level, internal_format, x, y, width, height, border),
            WebGLCommand::CopyTexSubImage2D(target, level, xoffset, yoffset, x, y, width, height) =>
                ctx.gl().copy_tex_sub_image_2d(target, level, xoffset, yoffset, x, y, width, height),
            WebGLCommand::CullFace(mode) =>
                ctx.gl().cull_face(mode),
            WebGLCommand::DepthFunc(func) =>
                ctx.gl().depth_func(func),
            WebGLCommand::DepthMask(flag) =>
                ctx.gl().depth_mask(flag),
            WebGLCommand::DepthRange(near, far) =>
                ctx.gl().depth_range(near, far),
            WebGLCommand::Disable(cap) =>
                ctx.gl().disable(cap),
            WebGLCommand::Enable(cap) =>
                ctx.gl().enable(cap),
            WebGLCommand::FramebufferRenderbuffer(target, attachment, renderbuffertarget, rb) =>
                ctx.gl().framebuffer_renderbuffer(target, attachment, renderbuffertarget, rb.map_or(0, WebGLRenderbufferId::get)),
            WebGLCommand::FramebufferTexture2D(target, attachment, textarget, texture, level) =>
                ctx.gl().framebuffer_texture_2d(target, attachment, textarget, texture.map_or(0, WebGLTextureId::get), level),
            WebGLCommand::FrontFace(mode) =>
                ctx.gl().front_face(mode),
            WebGLCommand::DisableVertexAttribArray(attrib_id) =>
                ctx.gl().disable_vertex_attrib_array(attrib_id),
            WebGLCommand::DrawArrays(mode, first, count) =>
                ctx.gl().draw_arrays(mode, first, count),
            WebGLCommand::DrawElements(mode, count, type_, offset) =>
                ctx.gl().draw_elements(mode, count, type_, offset as u32),
            WebGLCommand::EnableVertexAttribArray(attrib_id) =>
                ctx.gl().enable_vertex_attrib_array(attrib_id),
            WebGLCommand::Hint(name, val) =>
                ctx.gl().hint(name, val),
            WebGLCommand::IsEnabled(cap, chan) =>
                chan.send(ctx.gl().is_enabled(cap) != 0).unwrap(),
            WebGLCommand::LineWidth(width) =>
                ctx.gl().line_width(width),
            WebGLCommand::PixelStorei(name, val) =>
                ctx.gl().pixel_store_i(name, val),
            WebGLCommand::PolygonOffset(factor, units) =>
                ctx.gl().polygon_offset(factor, units),
            WebGLCommand::ReadPixels(x, y, width, height, format, pixel_type, chan) =>
                Self::read_pixels(ctx.gl(), x, y, width, height, format, pixel_type, chan),
            WebGLCommand::RenderbufferStorage(target, format, width, height) =>
                ctx.gl().renderbuffer_storage(target, format, width, height),
            WebGLCommand::SampleCoverage(value, invert) =>
                ctx.gl().sample_coverage(value, invert),
            WebGLCommand::Scissor(x, y, width, height) =>
                ctx.gl().scissor(x, y, width, height),
            WebGLCommand::StencilFunc(func, ref_, mask) =>
                ctx.gl().stencil_func(func, ref_, mask),
            WebGLCommand::StencilFuncSeparate(face, func, ref_, mask) =>
                ctx.gl().stencil_func_separate(face, func, ref_, mask),
            WebGLCommand::StencilMask(mask) =>
                ctx.gl().stencil_mask(mask),
            WebGLCommand::StencilMaskSeparate(face, mask) =>
                ctx.gl().stencil_mask_separate(face, mask),
            WebGLCommand::StencilOp(fail, zfail, zpass) =>
                ctx.gl().stencil_op(fail, zfail, zpass),
            WebGLCommand::StencilOpSeparate(face, fail, zfail, zpass) =>
                ctx.gl().stencil_op_separate(face, fail, zfail, zpass),
            WebGLCommand::GetActiveAttrib(program_id, index, chan) =>
                Self::active_attrib(ctx.gl(), program_id, index, chan),
            WebGLCommand::GetActiveUniform(program_id, index, chan) =>
                Self::active_uniform(ctx.gl(), program_id, index, chan),
            WebGLCommand::GetAttribLocation(program_id, name, chan) =>
                Self::attrib_location(ctx.gl(), program_id, name, chan),
            WebGLCommand::GetVertexAttrib(index, pname, chan) =>
                Self::vertex_attrib(ctx.gl(), index, pname, chan),
            WebGLCommand::GetVertexAttribOffset(index, pname, chan) =>
                Self::vertex_attrib_offset(ctx.gl(), index, pname, chan),
            WebGLCommand::GetBufferParameter(target, param_id, chan) =>
                Self::buffer_parameter(ctx.gl(), target, param_id, chan),
            WebGLCommand::GetParameter(param_id, chan) =>
                Self::parameter(ctx.gl(), param_id, chan),
            WebGLCommand::GetProgramParameter(program_id, param_id, chan) =>
                Self::program_parameter(ctx.gl(), program_id, param_id, chan),
            WebGLCommand::GetShaderParameter(shader_id, param_id, chan) =>
                Self::shader_parameter(ctx.gl(), shader_id, param_id, chan),
            WebGLCommand::GetShaderPrecisionFormat(shader_type, precision_type, chan) =>
                Self::shader_precision_format(ctx.gl(), shader_type, precision_type, chan),
            WebGLCommand::GetExtensions(chan) =>
                Self::get_extensions(ctx.gl(), chan),
            WebGLCommand::GetUniformLocation(program_id, name, chan) =>
                Self::uniform_location(ctx.gl(), program_id, name, chan),
            WebGLCommand::GetShaderInfoLog(shader_id, chan) =>
                Self::shader_info_log(ctx.gl(), shader_id, chan),
            WebGLCommand::GetProgramInfoLog(program_id, chan) =>
                Self::program_info_log(ctx.gl(), program_id, chan),
            WebGLCommand::CompileShader(shader_id, source) =>
                Self::compile_shader(ctx.gl(), shader_id, source),
            WebGLCommand::CreateBuffer(chan) =>
                Self::create_buffer(ctx.gl(), chan),
            WebGLCommand::CreateFramebuffer(chan) =>
                Self::create_framebuffer(ctx.gl(), chan),
            WebGLCommand::CreateRenderbuffer(chan) =>
                Self::create_renderbuffer(ctx.gl(), chan),
            WebGLCommand::CreateTexture(chan) =>
                Self::create_texture(ctx.gl(), chan),
            WebGLCommand::CreateProgram(chan) =>
                Self::create_program(ctx.gl(), chan),
            WebGLCommand::CreateShader(shader_type, chan) =>
                Self::create_shader(ctx.gl(), shader_type, chan),
            WebGLCommand::DeleteBuffer(id) =>
                ctx.gl().delete_buffers(&[id.get()]),
            WebGLCommand::DeleteFramebuffer(id) =>
                ctx.gl().delete_framebuffers(&[id.get()]),
            WebGLCommand::DeleteRenderbuffer(id) =>
                ctx.gl().delete_renderbuffers(&[id.get()]),
            WebGLCommand::DeleteTexture(id) =>
                ctx.gl().delete_textures(&[id.get()]),
            WebGLCommand::DeleteProgram(id) =>
                ctx.gl().delete_program(id.get()),
            WebGLCommand::DeleteShader(id) =>
                ctx.gl().delete_shader(id.get()),
            WebGLCommand::BindBuffer(target, id) =>
                ctx.gl().bind_buffer(target, id.map_or(0, WebGLBufferId::get)),
            WebGLCommand::BindFramebuffer(target, request) =>
                Self::bind_framebuffer(ctx.gl(), target, request, ctx),
            WebGLCommand::BindRenderbuffer(target, id) =>
                ctx.gl().bind_renderbuffer(target, id.map_or(0, WebGLRenderbufferId::get)),
            WebGLCommand::BindTexture(target, id) =>
                ctx.gl().bind_texture(target, id.map_or(0, WebGLTextureId::get)),
            WebGLCommand::LinkProgram(program_id) =>
                ctx.gl().link_program(program_id.get()),
            WebGLCommand::Uniform1f(uniform_id, v) =>
                ctx.gl().uniform_1f(uniform_id, v),
            WebGLCommand::Uniform1fv(uniform_id, v) =>
                ctx.gl().uniform_1fv(uniform_id, &v),
            WebGLCommand::Uniform1i(uniform_id, v) =>
                ctx.gl().uniform_1i(uniform_id, v),
            WebGLCommand::Uniform1iv(uniform_id, v) =>
                ctx.gl().uniform_1iv(uniform_id, &v),
            WebGLCommand::Uniform2f(uniform_id, x, y) =>
                ctx.gl().uniform_2f(uniform_id, x, y),
            WebGLCommand::Uniform2fv(uniform_id, v) =>
                ctx.gl().uniform_2fv(uniform_id, &v),
            WebGLCommand::Uniform2i(uniform_id, x, y) =>
                ctx.gl().uniform_2i(uniform_id, x, y),
            WebGLCommand::Uniform2iv(uniform_id, v) =>
                ctx.gl().uniform_2iv(uniform_id, &v),
            WebGLCommand::Uniform3f(uniform_id, x, y, z) =>
                ctx.gl().uniform_3f(uniform_id, x, y, z),
            WebGLCommand::Uniform3fv(uniform_id, v) =>
                ctx.gl().uniform_3fv(uniform_id, &v),
            WebGLCommand::Uniform3i(uniform_id, x, y, z) =>
                ctx.gl().uniform_3i(uniform_id, x, y, z),
            WebGLCommand::Uniform3iv(uniform_id, v) =>
                ctx.gl().uniform_3iv(uniform_id, &v),
            WebGLCommand::Uniform4f(uniform_id, x, y, z, w) =>
                ctx.gl().uniform_4f(uniform_id, x, y, z, w),
            WebGLCommand::Uniform4fv(uniform_id, v) =>
                ctx.gl().uniform_4fv(uniform_id, &v),
            WebGLCommand::Uniform4i(uniform_id, x, y, z, w) =>
                ctx.gl().uniform_4i(uniform_id, x, y, z, w),
            WebGLCommand::Uniform4iv(uniform_id, v) =>
                ctx.gl().uniform_4iv(uniform_id, &v),
            WebGLCommand::UniformMatrix2fv(uniform_id, transpose,  v) =>
                ctx.gl().uniform_matrix_2fv(uniform_id, transpose, &v),
            WebGLCommand::UniformMatrix3fv(uniform_id, transpose,  v) =>
                ctx.gl().uniform_matrix_3fv(uniform_id, transpose, &v),
            WebGLCommand::UniformMatrix4fv(uniform_id, transpose,  v) =>
                ctx.gl().uniform_matrix_4fv(uniform_id, transpose, &v),
            WebGLCommand::UseProgram(program_id) =>
                ctx.gl().use_program(program_id.get()),
            WebGLCommand::ValidateProgram(program_id) =>
                ctx.gl().validate_program(program_id.get()),
            WebGLCommand::VertexAttrib(attrib_id, x, y, z, w) =>
                ctx.gl().vertex_attrib_4f(attrib_id, x, y, z, w),
            WebGLCommand::VertexAttribPointer2f(attrib_id, size, normalized, stride, offset) =>
                ctx.gl().vertex_attrib_pointer_f32(attrib_id, size, normalized, stride, offset),
            WebGLCommand::VertexAttribPointer(attrib_id, size, data_type, normalized, stride, offset) =>
                ctx.gl().vertex_attrib_pointer(attrib_id, size, data_type, normalized, stride, offset),
            WebGLCommand::Viewport(x, y, width, height) =>
                ctx.gl().viewport(x, y, width, height),
            WebGLCommand::TexImage2D(target, level, internal, width, height, format, data_type, data) =>
                ctx.gl().tex_image_2d(target, level, internal, width, height, /*border*/0, format, data_type, Some(&data)),
            WebGLCommand::TexParameteri(target, name, value) =>
                ctx.gl().tex_parameter_i(target, name, value),
            WebGLCommand::TexParameterf(target, name, value) =>
                ctx.gl().tex_parameter_f(target, name, value),
            WebGLCommand::TexSubImage2D(target, level, xoffset, yoffset, x, y, width, height, data) =>
                ctx.gl().tex_sub_image_2d(target, level, xoffset, yoffset, x, y, width, height, &data),
            WebGLCommand::DrawingBufferWidth(sender) =>
                sender.send(ctx.borrow_draw_buffer().unwrap().size().width).unwrap(),
            WebGLCommand::DrawingBufferHeight(sender) =>
                sender.send(ctx.borrow_draw_buffer().unwrap().size().height).unwrap(),
            WebGLCommand::Finish(sender) =>
                Self::finish(ctx.gl(), sender),
            WebGLCommand::Flush =>
                ctx.gl().flush(),
            WebGLCommand::GenerateMipmap(target) =>
                ctx.gl().generate_mipmap(target),
            WebGLCommand::CreateVertexArray(chan) =>
                Self::create_vertex_array(ctx.gl(), chan),
            WebGLCommand::DeleteVertexArray(id) =>
                ctx.gl().delete_vertex_arrays(&[id.get()]),
            WebGLCommand::BindVertexArray(id) =>
                ctx.gl().bind_vertex_array(id.map_or(0, WebGLVertexArrayId::get)),
        }

        // FIXME: Use debug_assertions once tests are run with them
        let error = ctx.gl().get_error();
        assert!(error == gl::NO_ERROR, "Unexpected WebGL error: 0x{:x} ({})", error, error);
    }

    fn read_pixels(gl: &gl::Gl, x: i32, y: i32, width: i32, height: i32, format: u32, pixel_type: u32,
                   chan: MsgSender<Vec<u8>>) {
      let result = gl.read_pixels(x, y, width, height, format, pixel_type);
      chan.send(result).unwrap()
    }

    fn active_attrib(gl: &gl::Gl,
                     program_id: WebGLProgramId,
                     index: u32,
                     chan: MsgSender<WebGLResult<(i32, u32, String)>>) {
        let result = if index >= gl.get_program_iv(program_id.get(), gl::ACTIVE_ATTRIBUTES) as u32 {
            Err(WebGLError::InvalidValue)
        } else {
            Ok(gl.get_active_attrib(program_id.get(), index))
        };
        chan.send(result).unwrap();
    }

    fn active_uniform(gl: &gl::Gl,
                      program_id: WebGLProgramId,
                      index: u32,
                      chan: MsgSender<WebGLResult<(i32, u32, String)>>) {
        let result = if index >= gl.get_program_iv(program_id.get(), gl::ACTIVE_UNIFORMS) as u32 {
            Err(WebGLError::InvalidValue)
        } else {
            Ok(gl.get_active_uniform(program_id.get(), index))
        };
        chan.send(result).unwrap();
    }

    fn attrib_location(gl: &gl::Gl,
                       program_id: WebGLProgramId,
                       name: String,
                       chan: MsgSender<Option<i32>> ) {
        let attrib_location = gl.get_attrib_location(program_id.get(), &name);

        let attrib_location = if attrib_location == -1 {
            None
        } else {
            Some(attrib_location)
        };

        chan.send(attrib_location).unwrap();
    }

    fn parameter(gl: &gl::Gl,
                 param_id: u32,
                 chan: MsgSender<WebGLResult<WebGLParameter>>) {
        let result = match param_id {
            gl::ACTIVE_TEXTURE |
            gl::ALPHA_BITS |
            gl::BLEND_DST_ALPHA |
            gl::BLEND_DST_RGB |
            gl::BLEND_EQUATION_ALPHA |
            gl::BLEND_EQUATION_RGB |
            gl::BLEND_SRC_ALPHA |
            gl::BLEND_SRC_RGB |
            gl::BLUE_BITS |
            gl::CULL_FACE_MODE |
            gl::DEPTH_BITS |
            gl::DEPTH_FUNC |
            gl::FRONT_FACE |
            //gl::GENERATE_MIPMAP_HINT |
            gl::GREEN_BITS |
            //gl::IMPLEMENTATION_COLOR_READ_FORMAT |
            //gl::IMPLEMENTATION_COLOR_READ_TYPE |
            gl::MAX_COMBINED_TEXTURE_IMAGE_UNITS |
            gl::MAX_CUBE_MAP_TEXTURE_SIZE |
            //gl::MAX_FRAGMENT_UNIFORM_VECTORS |
            gl::MAX_RENDERBUFFER_SIZE |
            gl::MAX_TEXTURE_IMAGE_UNITS |
            gl::MAX_TEXTURE_SIZE |
            //gl::MAX_VARYING_VECTORS |
            gl::MAX_VERTEX_ATTRIBS |
            gl::MAX_VERTEX_TEXTURE_IMAGE_UNITS |
            //gl::MAX_VERTEX_UNIFORM_VECTORS |
            gl::PACK_ALIGNMENT |
            gl::RED_BITS |
            gl::SAMPLE_BUFFERS |
            gl::SAMPLES |
            gl::STENCIL_BACK_FAIL |
            gl::STENCIL_BACK_FUNC |
            gl::STENCIL_BACK_PASS_DEPTH_FAIL |
            gl::STENCIL_BACK_PASS_DEPTH_PASS |
            gl::STENCIL_BACK_REF |
            gl::STENCIL_BACK_VALUE_MASK |
            gl::STENCIL_BACK_WRITEMASK |
            gl::STENCIL_BITS |
            gl::STENCIL_CLEAR_VALUE |
            gl::STENCIL_FAIL |
            gl::STENCIL_FUNC |
            gl::STENCIL_PASS_DEPTH_FAIL |
            gl::STENCIL_PASS_DEPTH_PASS |
            gl::STENCIL_REF |
            gl::STENCIL_VALUE_MASK |
            gl::STENCIL_WRITEMASK |
            gl::SUBPIXEL_BITS |
            gl::UNPACK_ALIGNMENT =>
            //gl::UNPACK_COLORSPACE_CONVERSION_WEBGL =>
                Ok(WebGLParameter::Int(gl.get_integer_v(param_id))),

            gl::BLEND |
            gl::CULL_FACE |
            gl::DEPTH_TEST |
            gl::DEPTH_WRITEMASK |
            gl::DITHER |
            gl::POLYGON_OFFSET_FILL |
            gl::SAMPLE_COVERAGE_INVERT |
            gl::STENCIL_TEST =>
            //gl::UNPACK_FLIP_Y_WEBGL |
            //gl::UNPACK_PREMULTIPLY_ALPHA_WEBGL =>
                Ok(WebGLParameter::Bool(gl.get_boolean_v(param_id) != 0)),

            gl::DEPTH_CLEAR_VALUE |
            gl::LINE_WIDTH |
            gl::POLYGON_OFFSET_FACTOR |
            gl::POLYGON_OFFSET_UNITS |
            gl::SAMPLE_COVERAGE_VALUE =>
                Ok(WebGLParameter::Float(gl.get_float_v(param_id))),

            gl::VERSION => Ok(WebGLParameter::String("WebGL 1.0".to_owned())),
            gl::RENDERER |
            gl::VENDOR => Ok(WebGLParameter::String("Mozilla/Servo".to_owned())),
            gl::SHADING_LANGUAGE_VERSION => Ok(WebGLParameter::String("WebGL GLSL ES 1.0".to_owned())),

            // TODO(zbarsky, emilio): Implement support for the following valid parameters
            // Float32Array
            gl::ALIASED_LINE_WIDTH_RANGE |
            //gl::ALIASED_POINT_SIZE_RANGE |
            //gl::BLEND_COLOR |
            gl::COLOR_CLEAR_VALUE |
            gl::DEPTH_RANGE |

            // WebGLBuffer
            gl::ARRAY_BUFFER_BINDING |
            gl::ELEMENT_ARRAY_BUFFER_BINDING |

            // WebGLFrameBuffer
            gl::FRAMEBUFFER_BINDING |

            // WebGLRenderBuffer
            gl::RENDERBUFFER_BINDING |

            // WebGLProgram
            gl::CURRENT_PROGRAM |

            // WebGLTexture
            gl::TEXTURE_BINDING_2D |
            gl::TEXTURE_BINDING_CUBE_MAP |

            // sequence<GlBoolean>
            gl::COLOR_WRITEMASK |

            // Uint32Array
            gl::COMPRESSED_TEXTURE_FORMATS |

            // Int32Array
            gl::MAX_VIEWPORT_DIMS |
            gl::SCISSOR_BOX |
            gl::VIEWPORT => Err(WebGLError::InvalidEnum),

            // Invalid parameters
            _ => Err(WebGLError::InvalidEnum)
        };

        chan.send(result).unwrap();
    }

    fn finish(gl: &gl::Gl, chan: MsgSender<()>) {
        gl.finish();
        chan.send(()).unwrap();
    }

    fn vertex_attrib(gl: &gl::Gl,
                     index: u32,
                     pname: u32,
                     chan: MsgSender<WebGLResult<WebGLParameter>>) {
        let result = if index >= gl.get_integer_v(gl::MAX_VERTEX_ATTRIBS) as u32 {
            Err(WebGLError::InvalidValue)
        } else {
            match pname {
                gl::VERTEX_ATTRIB_ARRAY_ENABLED |
                gl::VERTEX_ATTRIB_ARRAY_NORMALIZED =>
                    Ok(WebGLParameter::Bool(gl.get_vertex_attrib_iv(index, pname) != 0)),
                gl::VERTEX_ATTRIB_ARRAY_SIZE |
                gl::VERTEX_ATTRIB_ARRAY_STRIDE |
                gl::VERTEX_ATTRIB_ARRAY_TYPE =>
                    Ok(WebGLParameter::Int(gl.get_vertex_attrib_iv(index, pname))),
                gl::CURRENT_VERTEX_ATTRIB =>
                    Ok(WebGLParameter::FloatArray(gl.get_vertex_attrib_fv(index, pname))),
                // gl::VERTEX_ATTRIB_ARRAY_BUFFER_BINDING should return WebGLBuffer
                _ => Err(WebGLError::InvalidEnum),
            }
        };

        chan.send(result).unwrap();
    }

    fn vertex_attrib_offset(gl: &gl::Gl,
                            index: u32,
                            pname: u32,
                            chan: MsgSender<WebGLResult<isize>>) {
        let result = match pname {
                gl::VERTEX_ATTRIB_ARRAY_POINTER => Ok(gl.get_vertex_attrib_pointer_v(index, pname)),
                _ => Err(WebGLError::InvalidEnum),
        };

        chan.send(result).unwrap();
    }

    fn buffer_parameter(gl: &gl::Gl,
                        target: u32,
                        param_id: u32,
                        chan: MsgSender<WebGLResult<WebGLParameter>>) {
        let result = match param_id {
            gl::BUFFER_SIZE |
            gl::BUFFER_USAGE =>
                Ok(WebGLParameter::Int(gl.get_buffer_parameter_iv(target, param_id))),
            _ => Err(WebGLError::InvalidEnum),
        };

        chan.send(result).unwrap();
    }

    fn program_parameter(gl: &gl::Gl,
                         program_id: WebGLProgramId,
                         param_id: u32,
                         chan: MsgSender<WebGLResult<WebGLParameter>>) {
        let result = match param_id {
            gl::DELETE_STATUS |
            gl::LINK_STATUS |
            gl::VALIDATE_STATUS =>
                Ok(WebGLParameter::Bool(gl.get_program_iv(program_id.get(), param_id) != 0)),
            gl::ATTACHED_SHADERS |
            gl::ACTIVE_ATTRIBUTES |
            gl::ACTIVE_UNIFORMS =>
                Ok(WebGLParameter::Int(gl.get_program_iv(program_id.get(), param_id))),
            _ => Err(WebGLError::InvalidEnum),
        };

        chan.send(result).unwrap();
    }

    fn shader_parameter(gl: &gl::Gl,
                        shader_id: WebGLShaderId,
                        param_id: u32,
                        chan: MsgSender<WebGLResult<WebGLParameter>>) {
        let result = match param_id {
            gl::SHADER_TYPE =>
                Ok(WebGLParameter::Int(gl.get_shader_iv(shader_id.get(), param_id))),
            gl::DELETE_STATUS |
            gl::COMPILE_STATUS =>
                Ok(WebGLParameter::Bool(gl.get_shader_iv(shader_id.get(), param_id) != 0)),
            _ => Err(WebGLError::InvalidEnum),
        };

        chan.send(result).unwrap();
    }

    fn shader_precision_format(gl: &gl::Gl,
                               shader_type: u32,
                               precision_type: u32,
                               chan: MsgSender<WebGLResult<(i32, i32, i32)>>) {

        let result = match precision_type {
            gl::LOW_FLOAT |
            gl::MEDIUM_FLOAT |
            gl::HIGH_FLOAT |
            gl::LOW_INT |
            gl::MEDIUM_INT |
            gl::HIGH_INT => {
                Ok(gl.get_shader_precision_format(shader_type, precision_type))
            },
            _=> {
                Err(WebGLError::InvalidEnum)
            }
        };

        chan.send(result).unwrap();
    }

    fn get_extensions(gl: &gl::Gl, chan: MsgSender<String>) {
        chan.send(gl.get_string(gl::EXTENSIONS)).unwrap();
    }

    fn uniform_location(gl: &gl::Gl,
                        program_id: WebGLProgramId,
                        name: String,
                        chan: MsgSender<Option<i32>>) {
        let location = gl.get_uniform_location(program_id.get(), &name);
        let location = if location == -1 {
            None
        } else {
            Some(location)
        };

        chan.send(location).unwrap();
    }


    fn shader_info_log(gl: &gl::Gl, shader_id: WebGLShaderId, chan: MsgSender<String>) {
        let log = gl.get_shader_info_log(shader_id.get());
        chan.send(log).unwrap();
    }

    fn program_info_log(gl: &gl::Gl, program_id: WebGLProgramId, chan: MsgSender<String>) {
        let log = gl.get_program_info_log(program_id.get());
        chan.send(log).unwrap();
    }

    fn create_buffer(gl: &gl::Gl, chan: MsgSender<Option<WebGLBufferId>>) {
        let buffer = gl.gen_buffers(1)[0];
        let buffer = if buffer == 0 {
            None
        } else {
            Some(unsafe { WebGLBufferId::new(buffer) })
        };
        chan.send(buffer).unwrap();
    }

    fn create_framebuffer(gl: &gl::Gl, chan: MsgSender<Option<WebGLFramebufferId>>) {
        let framebuffer = gl.gen_framebuffers(1)[0];
        let framebuffer = if framebuffer == 0 {
            None
        } else {
            Some(unsafe { WebGLFramebufferId::new(framebuffer) })
        };
        chan.send(framebuffer).unwrap();
    }


    fn create_renderbuffer(gl: &gl::Gl, chan: MsgSender<Option<WebGLRenderbufferId>>) {
        let renderbuffer = gl.gen_renderbuffers(1)[0];
        let renderbuffer = if renderbuffer == 0 {
            None
        } else {
            Some(unsafe { WebGLRenderbufferId::new(renderbuffer) })
        };
        chan.send(renderbuffer).unwrap();
    }

    fn create_texture(gl: &gl::Gl, chan: MsgSender<Option<WebGLTextureId>>) {
        let texture = gl.gen_textures(1)[0];
        let texture = if texture == 0 {
            None
        } else {
            Some(unsafe { WebGLTextureId::new(texture) })
        };
        chan.send(texture).unwrap();
    }


    fn create_program(gl: &gl::Gl, chan: MsgSender<Option<WebGLProgramId>>) {
        let program = gl.create_program();
        let program = if program == 0 {
            None
        } else {
            Some(unsafe { WebGLProgramId::new(program) })
        };
        chan.send(program).unwrap();
    }

    fn create_shader(gl: &gl::Gl, shader_type: u32, chan: MsgSender<Option<WebGLShaderId>>) {
        let shader = gl.create_shader(shader_type);
        let shader = if shader == 0 {
            None
        } else {
            Some(unsafe { WebGLShaderId::new(shader) })
        };
        chan.send(shader).unwrap();
    }

    fn create_vertex_array(gl: &gl::Gl, chan: MsgSender<Option<WebGLVertexArrayId>>) {
        let vao = gl.gen_vertex_arrays(1)[0];
        let vao = if vao == 0 {
            None
        } else {
            Some(unsafe { WebGLVertexArrayId::new(vao) })
        };
        chan.send(vao).unwrap();
    }

    #[inline]
    fn bind_framebuffer<Native: NativeGLContextMethods>(gl: &gl::Gl,
                                                        target: u32,
                                                        request: WebGLFramebufferBindingRequest,
                                                        ctx: &GLContext<Native>) {
        let id = match request {
            WebGLFramebufferBindingRequest::Explicit(id) => id.get(),
            WebGLFramebufferBindingRequest::Default =>
                ctx.borrow_draw_buffer().unwrap().get_framebuffer(),
        };

        gl.bind_framebuffer(target, id);
    }


    #[inline]
    fn compile_shader(gl: &gl::Gl, shader_id: WebGLShaderId, source: String) {
        gl.shader_source(shader_id.get(), &[source.as_bytes()]);
        gl.compile_shader(shader_id.get());
    }
}
