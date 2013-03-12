/* -*- Mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 40; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "skia/GrGLInterface.h"

/* SkPostConfig.h includes windows.h, which includes windef.h
 * which redefines min/max. We don't want that. */
#ifdef _WIN32
#undef min
#undef max
#endif

#include "GLContext.h"

using mozilla::gl::GLContext;

static GLContext* sGLContext;

extern "C" {

void EnsureGLContext(const GrGLInterface* interface)
{
    sGLContext = (GLContext*)(interface->fCallbackData);
    sGLContext->MakeCurrent();
}

// Core GL functions required by Ganesh

GrGLvoid glActiveTexture_mozilla(GrGLenum texture)
{
    return sGLContext->fActiveTexture(texture);
}

GrGLvoid glAttachShader_mozilla(GrGLuint program, GrGLuint shader)
{
    return sGLContext->fAttachShader(program, shader);
}

GrGLvoid glBindAttribLocation_mozilla(GrGLuint program, GrGLuint index, const GLchar* name)
{
    return sGLContext->fBindAttribLocation(program, index, name);
}

GrGLvoid glBindBuffer_mozilla(GrGLenum target, GrGLuint buffer)
{
    return sGLContext->fBindBuffer(target, buffer);
}

GrGLvoid glBindFramebuffer_mozilla(GrGLenum target, GrGLuint framebuffer)
{
    return sGLContext->fBindFramebuffer(target, framebuffer);
}

GrGLvoid glBindRenderbuffer_mozilla(GrGLenum target, GrGLuint renderbuffer)
{
    return sGLContext->fBindRenderbuffer(target, renderbuffer);
}

GrGLvoid glBindTexture_mozilla(GrGLenum target, GrGLuint texture)
{
    return sGLContext->fBindTexture(target, texture);
}

GrGLvoid glBlendColor_mozilla(GrGLclampf red, GrGLclampf green, GrGLclampf blue, GrGLclampf alpha)
{
    return sGLContext->fBlendColor(red, green, blue, alpha);
}

GrGLvoid glBlendFunc_mozilla(GrGLenum sfactor, GrGLenum dfactor)
{
    return sGLContext->fBlendFunc(sfactor, dfactor);
}

GrGLvoid glBufferData_mozilla(GrGLenum target, GrGLsizeiptr size, const void* data, GrGLenum usage)
{
    return sGLContext->fBufferData(target, size, data, usage);
}

GrGLvoid glBufferSubData_mozilla(GrGLenum target, GrGLintptr offset, GrGLsizeiptr size, const void* data)
{
    return sGLContext->fBufferSubData(target, offset, size, data);
}

GrGLenum glCheckFramebufferStatus_mozilla(GrGLenum target)
{
    return sGLContext->fCheckFramebufferStatus(target);
}

GrGLvoid glClear_mozilla(GrGLbitfield mask)
{
    return sGLContext->fClear(mask);
}

GrGLvoid glClearColor_mozilla(GrGLclampf red, GrGLclampf green, GrGLclampf blue, GrGLclampf alpha)
{
    return sGLContext->fClearColor(red, green, blue, alpha);
}

GrGLvoid glClearStencil_mozilla(GrGLint s)
{
    return sGLContext->fClearStencil(s);
}

GrGLvoid glColorMask_mozilla(GrGLboolean red, GrGLboolean green, GrGLboolean blue, GrGLboolean alpha)
{
    return sGLContext->fColorMask(red, green, blue, alpha);
}

GrGLvoid glCompileShader_mozilla(GrGLuint shader)
{
    return sGLContext->fCompileShader(shader);
}

GrGLuint glCreateProgram_mozilla(void)
{
    return sGLContext->fCreateProgram();
}

GrGLuint glCreateShader_mozilla(GrGLenum type)
{
    return sGLContext->fCreateShader(type);
}

GrGLvoid glCullFace_mozilla(GrGLenum mode)
{
    return sGLContext->fCullFace(mode);
}

GrGLvoid glDeleteBuffers_mozilla(GrGLsizei n, const GrGLuint* buffers)
{
    return sGLContext->fDeleteBuffers(n, const_cast<GrGLuint*>(buffers));
}

GrGLvoid glDeleteFramebuffers_mozilla(GrGLsizei n, const GrGLuint* framebuffers)
{
    return sGLContext->fDeleteFramebuffers(n, const_cast<GrGLuint*>(framebuffers));
}

GrGLvoid glDeleteProgram_mozilla(GrGLuint program)
{
    return sGLContext->fDeleteProgram(program);
}

GrGLvoid glDeleteRenderbuffers_mozilla(GrGLsizei n, const GrGLuint* renderbuffers)
{
    return sGLContext->fDeleteRenderbuffers(n, const_cast<GrGLuint*>(renderbuffers));
}

GrGLvoid glDeleteShader_mozilla(GrGLuint shader)
{
    return sGLContext->fDeleteShader(shader);
}

GrGLvoid glDeleteTextures_mozilla(GrGLsizei n, const GrGLuint* textures)
{
    return sGLContext->fDeleteTextures(n, const_cast<GrGLuint*>(textures));
}

GrGLvoid glDepthMask_mozilla(GrGLboolean flag)
{
    return sGLContext->fDepthMask(flag);
}

GrGLvoid glDisable_mozilla(GrGLenum cap)
{
    return sGLContext->fDisable(cap);
}

GrGLvoid glDisableVertexAttribArray_mozilla(GrGLuint index)
{
    return sGLContext->fDisableVertexAttribArray(index);
}

GrGLvoid glDrawArrays_mozilla(GrGLenum mode, GrGLint first, GrGLsizei count)
{
    return sGLContext->fDrawArrays(mode, first, count);
}

GrGLvoid glDrawElements_mozilla(GrGLenum mode, GrGLsizei count, GrGLenum type, const void* indices)
{
    return sGLContext->fDrawElements(mode, count, type, indices);
}

GrGLvoid glEnable_mozilla(GrGLenum cap)
{
    return sGLContext->fEnable(cap);
}

GrGLvoid glEnableVertexAttribArray_mozilla(GrGLuint index)
{
    return sGLContext->fEnableVertexAttribArray(index);
}

GrGLvoid glFinish_mozilla()
{
    return sGLContext->fFinish();
}

GrGLvoid glFlush_mozilla()
{
    return sGLContext->fFlush();
}

GrGLvoid glFramebufferRenderbuffer_mozilla(GrGLenum target, GrGLenum attachment, GrGLenum renderbuffertarget, GrGLuint renderbuffer)
{
    return sGLContext->fFramebufferRenderbuffer(target, attachment, renderbuffertarget, renderbuffer);
}

GrGLvoid glFramebufferTexture2D_mozilla(GrGLenum target, GrGLenum attachment, GrGLenum textarget, GrGLuint texture, GrGLint level)
{
    return sGLContext->fFramebufferTexture2D(target, attachment, textarget, texture, level);
}

GrGLvoid glFrontFace_mozilla(GrGLenum mode)
{
    return sGLContext->fFrontFace(mode);
}

GrGLvoid glGenBuffers_mozilla(GrGLsizei n, GrGLuint* buffers)
{
    return sGLContext->fGenBuffers(n, buffers);
}

GrGLvoid glGenFramebuffers_mozilla(GrGLsizei n, GrGLuint* framebuffers)
{
    return sGLContext->fGenFramebuffers(n, framebuffers);
}

GrGLvoid glGenRenderbuffers_mozilla(GrGLsizei n, GrGLuint* renderbuffers)
{
    return sGLContext->fGenRenderbuffers(n, renderbuffers);
}

GrGLvoid glGenTextures_mozilla(GrGLsizei n, GrGLuint* textures)
{
    return sGLContext->fGenTextures(n, textures);
}

GrGLvoid glGetBufferParameteriv_mozilla(GrGLenum target, GrGLenum pname, GrGLint* params)
{
    return sGLContext->fGetBufferParameteriv(target, pname, params);
}

GrGLvoid glGetFramebufferAttachmentParameteriv_mozilla(GrGLenum target, GrGLenum attachment, GrGLenum pname, GrGLint* params)
{
    return sGLContext->fGetFramebufferAttachmentParameteriv(target, attachment, pname, params);
}

GrGLenum glGetError_mozilla()
{
    return sGLContext->fGetError();
}

GrGLvoid glGetIntegerv_mozilla(GrGLenum pname, GrGLint* params)
{
    return sGLContext->fGetIntegerv(pname, params);
}

GrGLvoid glGetProgramInfoLog_mozilla(GrGLuint program, GrGLsizei bufsize, GrGLsizei* length, char* infolog)
{
    return sGLContext->fGetProgramInfoLog(program, bufsize, length, infolog);
}

GrGLvoid glGetProgramiv_mozilla(GrGLuint program, GrGLenum pname, GrGLint* params)
{
    return sGLContext->fGetProgramiv(program, pname, params);
}

GrGLvoid glGetRenderbufferParameteriv_mozilla(GrGLenum target, GrGLenum pname, GrGLint* params)
{
    return sGLContext->fGetRenderbufferParameteriv(target, pname, params);
}

GrGLvoid glGetShaderInfoLog_mozilla(GrGLuint shader, GrGLsizei bufsize, GrGLsizei* length, char* infolog)
{
    return sGLContext->fGetShaderInfoLog(shader, bufsize, length, infolog);
}

GrGLvoid glGetShaderiv_mozilla(GrGLuint shader, GrGLenum pname, GrGLint* params)
{
    return sGLContext->fGetShaderiv(shader, pname, params);
}

const GLubyte* glGetString_mozilla(GrGLenum name)
{
    // GLContext only exposes a OpenGL 2.0 style API, so we have to intercept a bunch
    // of checks that Ganesh makes to determine which capabilities are present
    // on the GL implementation and change them to match what GLContext actually exposes.

    if (name == LOCAL_GL_VERSION) {
        if (sGLContext->IsGLES2()) {
            return reinterpret_cast<const GLubyte*>("OpenGL ES 2.0");
        } else {
            return reinterpret_cast<const GLubyte*>("2.0");
        }
    } else if (name == LOCAL_GL_EXTENSIONS) {
        // Only expose the bare minimum extensions we want to support to ensure a functional Ganesh
        // as GLContext only exposes certain extensions
        static bool extensionsStringBuilt = false;
        static char extensionsString[120];

        if (!extensionsStringBuilt) {
            if (sGLContext->IsExtensionSupported(GLContext::EXT_texture_format_BGRA8888)) {
                strcpy(extensionsString, "GL_EXT_texture_format_BGRA8888 ");
            }

            if (sGLContext->IsExtensionSupported(GLContext::OES_packed_depth_stencil)) {
                strcat(extensionsString, "GL_OES_packed_depth_stencil ");
            }

            if (sGLContext->IsExtensionSupported(GLContext::EXT_packed_depth_stencil)) {
                strcat(extensionsString, "GL_EXT_packed_depth_stencil ");
            }

            extensionsStringBuilt = true;
        }

        return reinterpret_cast<const GLubyte*>(extensionsString);

    } else if (name == LOCAL_GL_SHADING_LANGUAGE_VERSION) {
        if (sGLContext->IsGLES2()) {
            return reinterpret_cast<const GLubyte*>("OpenGL ES GLSL ES 1.0");
        } else {
            return reinterpret_cast<const GLubyte*>("1.10");
        }
    }

    return sGLContext->fGetString(name);
}

GrGLint glGetUniformLocation_mozilla(GrGLuint program, const char* name)
{
    return sGLContext->fGetUniformLocation(program, name);
}

GrGLvoid glLineWidth_mozilla(GrGLfloat width)
{
    return sGLContext->fLineWidth(width);
}

GrGLvoid glLinkProgram_mozilla(GrGLuint program)
{
    return sGLContext->fLinkProgram(program);
}

GrGLvoid glPixelStorei_mozilla(GrGLenum pname, GrGLint param)
{
    return sGLContext->fPixelStorei(pname, param);
}

GrGLvoid glReadPixels_mozilla(GrGLint x, GrGLint y, GrGLsizei width, GrGLsizei height,
                              GrGLenum format, GrGLenum type, void* pixels)
{
    return sGLContext->fReadPixels(x, y, width, height,
                                   format, type, pixels);
}

GrGLvoid glRenderbufferStorage_mozilla(GrGLenum target, GrGLenum internalformat, GrGLsizei width, GrGLsizei height)
{
    return sGLContext->fRenderbufferStorage(target, internalformat, width, height);
}

GrGLvoid glScissor_mozilla(GrGLint x, GrGLint y, GrGLsizei width, GrGLsizei height)
{
    return sGLContext->fScissor(x, y, width, height);
}

GrGLvoid glShaderSource_mozilla(GrGLuint shader, GrGLsizei count, const char** str, const GrGLint* length)
{
    return sGLContext->fShaderSource(shader, count, str, length);
}

GrGLvoid glStencilFunc_mozilla(GrGLenum func, GrGLint ref, GrGLuint mask)
{
    return sGLContext->fStencilFunc(func, ref, mask);
}

GrGLvoid glStencilMask_mozilla(GrGLuint mask)
{
    return sGLContext->fStencilMask(mask);
}

GrGLvoid glStencilOp_mozilla(GrGLenum fail, GrGLenum zfail, GrGLenum zpass)
{
    return sGLContext->fStencilOp(fail, zfail, zpass);
}

GrGLvoid glTexImage2D_mozilla(GrGLenum target, GrGLint level, GrGLint internalformat,
                              GrGLsizei width, GrGLsizei height, GrGLint border,
                              GrGLenum format, GrGLenum type, const void* pixels)
{
    return sGLContext->fTexImage2D(target, level, internalformat,
                                   width, height, border,
                                   format, type, pixels);
}

GrGLvoid glTexParameteri_mozilla(GrGLenum target, GrGLenum pname, GrGLint param)
{
    return sGLContext->fTexParameteri(target, pname, param);
}

GrGLvoid glTexParameteriv_mozilla(GrGLenum target, GrGLenum pname, const GrGLint* params)
{
    return sGLContext->fTexParameteriv(target, pname, const_cast<GrGLint*>(params));
}

GrGLvoid glTexSubImage2D_mozilla(GrGLenum target, GrGLint level,
                                 GrGLint xoffset, GrGLint yoffset,
                                 GrGLsizei width, GrGLsizei height,
                                 GrGLenum format, GrGLenum type, const void* pixels)
{
    return sGLContext->fTexSubImage2D(target, level,
                                      xoffset, yoffset,
                                      width, height,
                                      format, type, pixels);
}

GrGLvoid glUniform1f_mozilla(GrGLint location, GrGLfloat v)
{
    return sGLContext->fUniform1f(location, v);
}

GrGLvoid glUniform1i_mozilla(GrGLint location, GrGLint v)
{
    return sGLContext->fUniform1i(location, v);
}

GrGLvoid glUniform1fv_mozilla(GrGLint location, GrGLsizei count, const GrGLfloat* v)
{
    return sGLContext->fUniform1fv(location, count, v);
}

GrGLvoid glUniform1iv_mozilla(GrGLint location, GrGLsizei count, const GrGLint* v)
{
    return sGLContext->fUniform1iv(location, count, v);
}

GrGLvoid glUniform2f_mozilla(GrGLint location, GrGLfloat v0, GrGLfloat v1)
{
    return sGLContext->fUniform2f(location, v0, v1);
}

GrGLvoid glUniform2i_mozilla(GrGLint location, GrGLint v0, GrGLint v1)
{
    return sGLContext->fUniform2i(location, v0, v1);
}

GrGLvoid glUniform2fv_mozilla(GrGLint location, GrGLsizei count, const GrGLfloat* v)
{
    return sGLContext->fUniform2fv(location, count, v);
}

GrGLvoid glUniform2iv_mozilla(GrGLint location, GrGLsizei count, const GrGLint* v)
{
    return sGLContext->fUniform2iv(location, count, v);
}

GrGLvoid glUniform3f_mozilla(GrGLint location, GrGLfloat v0, GrGLfloat v1, GrGLfloat v2)
{
    return sGLContext->fUniform3f(location, v0, v1, v2);
}

GrGLvoid glUniform3i_mozilla(GrGLint location, GrGLint v0, GrGLint v1, GrGLint v2)
{
    return sGLContext->fUniform3i(location, v0, v1, v2);
}

GrGLvoid glUniform3fv_mozilla(GrGLint location, GrGLsizei count, const GrGLfloat* v)
{
    return sGLContext->fUniform3fv(location, count, v);
}

GrGLvoid glUniform3iv_mozilla(GrGLint location, GrGLsizei count, const GrGLint* v)
{
    return sGLContext->fUniform3iv(location, count, v);
}

GrGLvoid glUniform4f_mozilla(GrGLint location, GrGLfloat v0, GrGLfloat v1, GrGLfloat v2, GrGLfloat v3)
{
    return sGLContext->fUniform4f(location, v0, v1, v2, v3);
}

GrGLvoid glUniform4i_mozilla(GrGLint location, GrGLint v0, GrGLint v1, GrGLint v2, GrGLint v3)
{
    return sGLContext->fUniform4i(location, v0, v1, v2, v3);
}

GrGLvoid glUniform4fv_mozilla(GrGLint location, GrGLsizei count, const GrGLfloat* v)
{
    return sGLContext->fUniform4fv(location, count, v);
}

GrGLvoid glUniform4iv_mozilla(GrGLint location, GrGLsizei count, const GrGLint* v)
{
    return sGLContext->fUniform4iv(location, count, v);
}

GrGLvoid glUniformMatrix2fv_mozilla(GrGLint location, GrGLsizei count, GrGLboolean transpose, const GrGLfloat* value)
{
    return sGLContext->fUniformMatrix2fv(location, count, transpose, value);
}

GrGLvoid glUniformMatrix3fv_mozilla(GrGLint location, GrGLsizei count, GrGLboolean transpose, const GrGLfloat* value)
{
    return sGLContext->fUniformMatrix3fv(location, count, transpose, value);
}

GrGLvoid glUniformMatrix4fv_mozilla(GrGLint location, GrGLsizei count, GrGLboolean transpose, const GrGLfloat* value)
{
    return sGLContext->fUniformMatrix4fv(location, count, transpose, value);
}

GrGLvoid glUseProgram_mozilla(GrGLuint program)
{
    return sGLContext->fUseProgram(program);
}

GrGLvoid glVertexAttrib4fv_mozilla(GrGLuint index, const GrGLfloat* values)
{
    return sGLContext->fVertexAttrib4fv(index, values);
}

GrGLvoid glVertexAttribPointer_mozilla(GrGLuint index, GrGLint size, GrGLenum type, GrGLboolean normalized, GrGLsizei stride, const void* ptr)
{
    return sGLContext->fVertexAttribPointer(index, size, type, normalized, stride, ptr);
}

GrGLvoid glViewport_mozilla(GrGLint x, GrGLint y, GrGLsizei width, GrGLsizei height)
{
    return sGLContext->fViewport(x, y, width, height);
}

// Required if the bindings are GLES2 or desktop OpenGL 2.0

GrGLvoid glStencilFuncSeparate_mozilla(GrGLenum frontfunc, GrGLenum backfunc, GrGLint ref, GrGLuint mask)
{
    return sGLContext->fStencilFuncSeparate(frontfunc, backfunc, ref, mask);
}

GrGLvoid glStencilMaskSeparate_mozilla(GrGLenum face, GrGLuint mask)
{
    return sGLContext->fStencilMaskSeparate(face, mask);
}

GrGLvoid glStencilOpSeparate_mozilla(GrGLenum face, GrGLenum sfail, GrGLenum dpfail, GrGLenum dppass)
{
    return sGLContext->fStencilOpSeparate(face, sfail, dpfail, dppass);
}

// Not in GLES2

GrGLvoid glGetTexLevelParameteriv_mozilla(GrGLenum target, GrGLint level, GrGLenum pname, GrGLint *params)
{
    return sGLContext->fGetTexLevelParameteriv(target, level, pname, params);
}

GrGLvoid glDrawBuffer_mozilla(GrGLenum mode)
{
    return sGLContext->fDrawBuffer(mode);
}

GrGLvoid glReadBuffer_mozilla(GrGLenum mode)
{
    return sGLContext->fReadBuffer(mode);
}

// Desktop OpenGL version >= 1.5

GrGLvoid glGenQueries_mozilla(GrGLsizei n, GrGLuint* ids)
{
    return sGLContext->fGenQueries(n, ids);
}

GrGLvoid glDeleteQueries_mozilla(GrGLsizei n, const GrGLuint* ids)
{
    return sGLContext->fDeleteQueries(n, const_cast<GrGLuint*>(ids));
}

GrGLvoid glBeginQuery_mozilla(GrGLenum target, GrGLuint id)
{
    return sGLContext->fBeginQuery(target, id);
}

GrGLvoid glEndQuery_mozilla(GrGLenum target)
{
    return sGLContext->fEndQuery(target);
}

GrGLvoid glGetQueryiv_mozilla(GrGLenum target, GrGLenum pname, GrGLint* params)
{
    return sGLContext->fGetQueryiv(target, pname, params);
}

GrGLvoid glGetQueryObjectiv_mozilla(GrGLuint id, GrGLenum pname, GrGLint* params)
{
    return sGLContext->fGetQueryObjectiv(id, pname, params);
}

GrGLvoid glGetQueryObjectuiv_mozilla(GrGLuint id, GrGLenum pname, GrGLuint* params)
{
    return sGLContext->fGetQueryObjectuiv(id, pname, params);
}

// Desktop OpenGL version >= 2.0

GrGLvoid glDrawBuffers_mozilla(GrGLsizei n, const GrGLenum* bufs)
{
    return sGLContext->fDrawBuffers(n, const_cast<GrGLenum*>(bufs));
}

// GLContext supports glMapBuffer on everything (GL_OES_mapbuffer)

GrGLvoid* glMapBuffer_mozilla(GrGLenum target, GrGLenum access)
{
    return sGLContext->fMapBuffer(target, access);
}

GrGLboolean glUnmapBuffer_mozilla(GrGLenum target)
{
    return sGLContext->fUnmapBuffer(target);
}

// GLContext supports glCompressedTexImage2D (GL_ARB_texture_compression)

GrGLvoid glCompressedTexImage2D_mozilla(GrGLenum target, GrGLint level, GrGLenum internalformat,
                                        GrGLsizei width, GrGLsizei height, GrGLint border,
                                        GrGLsizei imageSize, const GrGLvoid* pixels)
{
    return sGLContext->fCompressedTexImage2D(target, level, internalformat,
                                             width, height, border,
                                             imageSize, pixels);
}

// GLContext supports glBlitFramebuffer/glRenderbufferStorageMultisample (GL_ARB_framebuffer_object)

GrGLvoid glRenderbufferStorageMultisample_mozilla(GrGLenum target, GrGLsizei samples, GrGLenum internalformat,
                                                  GrGLsizei width, GrGLsizei height)
{
    return sGLContext->fRenderbufferStorageMultisample(target, samples, internalformat,
                                                       width, height);
}

GrGLvoid glBlitFramebuffer_mozilla(GrGLint srcX0, GrGLint srcY0,
                                   GrGLint srcX1, GrGLint srcY1,
                                   GrGLint dstX0, GrGLint dstY0,
                                   GrGLint dstX1, GrGLint dstY1,
                                   GrGLbitfield mask, GrGLenum filter) {
    return sGLContext->fBlitFramebuffer(srcX0, srcY0,
                                        srcX1, srcY1,
                                        dstX0, dstY0,
                                        dstX1, dstY1,
                                        mask, filter);
}

} // extern "C"

GrGLInterface* CreateGrInterfaceFromGLContext(GLContext* context)
{
    sGLContext = context;

    GrGLInterface* interface = new GrGLInterface();
    interface->fCallbackData = reinterpret_cast<GrGLInterfaceCallbackData>(context);
    interface->fCallback = EnsureGLContext;

    // Core GL functions required by Ganesh
    interface->fActiveTexture = glActiveTexture_mozilla;
    interface->fAttachShader = glAttachShader_mozilla;
    interface->fBindAttribLocation = glBindAttribLocation_mozilla;
    interface->fBindBuffer = glBindBuffer_mozilla;
    interface->fBindFramebuffer = glBindFramebuffer_mozilla;
    interface->fBindRenderbuffer = glBindRenderbuffer_mozilla;
    interface->fBindTexture = glBindTexture_mozilla;
    interface->fBlendFunc = glBlendFunc_mozilla;
    interface->fBlendColor = glBlendColor_mozilla;
    interface->fBufferData = glBufferData_mozilla;
    interface->fBufferSubData = glBufferSubData_mozilla;
    interface->fCheckFramebufferStatus = glCheckFramebufferStatus_mozilla;
    interface->fClear = glClear_mozilla;
    interface->fClearColor = glClearColor_mozilla;
    interface->fClearStencil = glClearStencil_mozilla;
    interface->fColorMask = glColorMask_mozilla;
    interface->fCompileShader = glCompileShader_mozilla;
    interface->fCreateProgram = glCreateProgram_mozilla;
    interface->fCreateShader = glCreateShader_mozilla;
    interface->fCullFace = glCullFace_mozilla;
    interface->fDeleteBuffers = glDeleteBuffers_mozilla;
    interface->fDeleteFramebuffers = glDeleteFramebuffers_mozilla;
    interface->fDeleteProgram = glDeleteProgram_mozilla;
    interface->fDeleteRenderbuffers = glDeleteRenderbuffers_mozilla;
    interface->fDeleteShader = glDeleteShader_mozilla;
    interface->fDeleteTextures = glDeleteTextures_mozilla;
    interface->fDepthMask = glDepthMask_mozilla;
    interface->fDisable = glDisable_mozilla;
    interface->fDisableVertexAttribArray = glDisableVertexAttribArray_mozilla;
    interface->fDrawArrays = glDrawArrays_mozilla;
    interface->fDrawElements = glDrawElements_mozilla;
    interface->fEnable = glEnable_mozilla;
    interface->fEnableVertexAttribArray = glEnableVertexAttribArray_mozilla;
    interface->fFinish = glFinish_mozilla;
    interface->fFlush = glFlush_mozilla;
    interface->fFramebufferRenderbuffer = glFramebufferRenderbuffer_mozilla;
    interface->fFramebufferTexture2D = glFramebufferTexture2D_mozilla;
    interface->fFrontFace = glFrontFace_mozilla;
    interface->fGenBuffers = glGenBuffers_mozilla;
    interface->fGenFramebuffers = glGenFramebuffers_mozilla;
    interface->fGenRenderbuffers = glGenRenderbuffers_mozilla;
    interface->fGetFramebufferAttachmentParameteriv = glGetFramebufferAttachmentParameteriv_mozilla;
    interface->fGenTextures = glGenTextures_mozilla;
    interface->fGetBufferParameteriv = glGetBufferParameteriv_mozilla;
    interface->fGetError = glGetError_mozilla;
    interface->fGetIntegerv = glGetIntegerv_mozilla;
    interface->fGetProgramInfoLog = glGetProgramInfoLog_mozilla;
    interface->fGetProgramiv = glGetProgramiv_mozilla;
    interface->fGetRenderbufferParameteriv = glGetRenderbufferParameteriv_mozilla;
    interface->fGetShaderInfoLog = glGetShaderInfoLog_mozilla;
    interface->fGetShaderiv = glGetShaderiv_mozilla;
    interface->fGetString = glGetString_mozilla;
    interface->fGetUniformLocation = glGetUniformLocation_mozilla;
    interface->fLineWidth = glLineWidth_mozilla;
    interface->fLinkProgram = glLinkProgram_mozilla;
    interface->fPixelStorei = glPixelStorei_mozilla;
    interface->fReadPixels = glReadPixels_mozilla;
    interface->fRenderbufferStorage = glRenderbufferStorage_mozilla;
    interface->fScissor = glScissor_mozilla;
    interface->fShaderSource = glShaderSource_mozilla;
    interface->fStencilFunc = glStencilFunc_mozilla;
    interface->fStencilMask = glStencilMask_mozilla;
    interface->fStencilOp = glStencilOp_mozilla;
    interface->fTexImage2D = glTexImage2D_mozilla;
    interface->fTexParameteri = glTexParameteri_mozilla;
    interface->fTexParameteriv = glTexParameteriv_mozilla;
    interface->fTexSubImage2D = glTexSubImage2D_mozilla;
    interface->fUniform1f = glUniform1f_mozilla;
    interface->fUniform1i = glUniform1i_mozilla;
    interface->fUniform1fv = glUniform1fv_mozilla;
    interface->fUniform1iv = glUniform1iv_mozilla;
    interface->fUniform2f = glUniform2f_mozilla;
    interface->fUniform2i = glUniform2i_mozilla;
    interface->fUniform2fv = glUniform2fv_mozilla;
    interface->fUniform2iv = glUniform2iv_mozilla;
    interface->fUniform3f = glUniform3f_mozilla;
    interface->fUniform3i = glUniform3i_mozilla;
    interface->fUniform3fv = glUniform3fv_mozilla;
    interface->fUniform3iv = glUniform3iv_mozilla;
    interface->fUniform4f = glUniform4f_mozilla;
    interface->fUniform4i = glUniform4i_mozilla;
    interface->fUniform4fv = glUniform4fv_mozilla;
    interface->fUniform4iv = glUniform4iv_mozilla;
    interface->fUniformMatrix2fv = glUniformMatrix2fv_mozilla;
    interface->fUniformMatrix3fv = glUniformMatrix3fv_mozilla;
    interface->fUniformMatrix4fv = glUniformMatrix4fv_mozilla;
    interface->fUseProgram = glUseProgram_mozilla;
    interface->fVertexAttrib4fv = glVertexAttrib4fv_mozilla;
    interface->fVertexAttribPointer = glVertexAttribPointer_mozilla;
    interface->fViewport = glViewport_mozilla;

    // Required for either desktop OpenGL 2.0 or OpenGL ES 2.0
    interface->fStencilFuncSeparate = glStencilFuncSeparate_mozilla;
    interface->fStencilMaskSeparate = glStencilMaskSeparate_mozilla;
    interface->fStencilOpSeparate = glStencilOpSeparate_mozilla;

    // GLContext supports glMapBuffer
    interface->fMapBuffer = glMapBuffer_mozilla;
    interface->fUnmapBuffer = glUnmapBuffer_mozilla;

    // GLContext supports glRenderbufferStorageMultisample/glBlitFramebuffer
    interface->fRenderbufferStorageMultisample = glRenderbufferStorageMultisample_mozilla;
    interface->fBlitFramebuffer = glBlitFramebuffer_mozilla;

    // GLContext supports glCompressedTexImage2D
    interface->fCompressedTexImage2D = glCompressedTexImage2D_mozilla;

    // Desktop GL 
    interface->fGetTexLevelParameteriv = glGetTexLevelParameteriv_mozilla;
    interface->fDrawBuffer = glDrawBuffer_mozilla;
    interface->fReadBuffer = glReadBuffer_mozilla;

    // Desktop OpenGL > 1.5
    interface->fGenQueries = glGenQueries_mozilla;
    interface->fDeleteQueries = glDeleteQueries_mozilla;
    interface->fBeginQuery = glBeginQuery_mozilla;
    interface->fEndQuery = glEndQuery_mozilla;
    interface->fGetQueryiv = glGetQueryiv_mozilla;
    interface->fGetQueryObjectiv = glGetQueryObjectiv_mozilla;
    interface->fGetQueryObjectuiv = glGetQueryObjectuiv_mozilla;

    // Desktop OpenGL > 2.0
    interface->fDrawBuffers = glDrawBuffers_mozilla;

    // We support both desktop GL and GLES2
    if (context->IsGLES2()) {
        interface->fBindingsExported = kES2_GrGLBinding;
    } else {
        interface->fBindingsExported = kDesktop_GrGLBinding;
    }

    return interface;
}

