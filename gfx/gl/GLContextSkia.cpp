/* -*- Mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 40; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "skia/GrGLInterface.h"
#include "mozilla/gfx/2D.h"

/* SkPostConfig.h includes windows.h, which includes windef.h
 * which redefines min/max. We don't want that. */
#ifdef _WIN32
#undef min
#undef max
#endif

#include "GLContext.h"

using mozilla::gl::GLContext;
using mozilla::gfx::DrawTarget;

static GLContext* sGLContext;

extern "C" {

void EnsureGLContext(const GrGLInterface* i)
{
    const DrawTarget* drawTarget = reinterpret_cast<const DrawTarget*>(i->fCallbackData);
    MOZ_ASSERT(drawTarget);
    sGLContext = static_cast<GLContext*>(drawTarget->GetGLContext());
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

GrGLInterface* CreateGrGLInterfaceFromGLContext(GLContext* context)
{
    sGLContext = context;

    GrGLInterface* i = new GrGLInterface();
    i->fCallback = EnsureGLContext;
    i->fCallbackData = 0; // must be later initialized to be a valid DrawTargetSkia* pointer

    // Core GL functions required by Ganesh
    i->fActiveTexture = glActiveTexture_mozilla;
    i->fAttachShader = glAttachShader_mozilla;
    i->fBindAttribLocation = glBindAttribLocation_mozilla;
    i->fBindBuffer = glBindBuffer_mozilla;
    i->fBindFramebuffer = glBindFramebuffer_mozilla;
    i->fBindRenderbuffer = glBindRenderbuffer_mozilla;
    i->fBindTexture = glBindTexture_mozilla;
    i->fBlendFunc = glBlendFunc_mozilla;
    i->fBlendColor = glBlendColor_mozilla;
    i->fBufferData = glBufferData_mozilla;
    i->fBufferSubData = glBufferSubData_mozilla;
    i->fCheckFramebufferStatus = glCheckFramebufferStatus_mozilla;
    i->fClear = glClear_mozilla;
    i->fClearColor = glClearColor_mozilla;
    i->fClearStencil = glClearStencil_mozilla;
    i->fColorMask = glColorMask_mozilla;
    i->fCompileShader = glCompileShader_mozilla;
    i->fCreateProgram = glCreateProgram_mozilla;
    i->fCreateShader = glCreateShader_mozilla;
    i->fCullFace = glCullFace_mozilla;
    i->fDeleteBuffers = glDeleteBuffers_mozilla;
    i->fDeleteFramebuffers = glDeleteFramebuffers_mozilla;
    i->fDeleteProgram = glDeleteProgram_mozilla;
    i->fDeleteRenderbuffers = glDeleteRenderbuffers_mozilla;
    i->fDeleteShader = glDeleteShader_mozilla;
    i->fDeleteTextures = glDeleteTextures_mozilla;
    i->fDepthMask = glDepthMask_mozilla;
    i->fDisable = glDisable_mozilla;
    i->fDisableVertexAttribArray = glDisableVertexAttribArray_mozilla;
    i->fDrawArrays = glDrawArrays_mozilla;
    i->fDrawElements = glDrawElements_mozilla;
    i->fEnable = glEnable_mozilla;
    i->fEnableVertexAttribArray = glEnableVertexAttribArray_mozilla;
    i->fFinish = glFinish_mozilla;
    i->fFlush = glFlush_mozilla;
    i->fFramebufferRenderbuffer = glFramebufferRenderbuffer_mozilla;
    i->fFramebufferTexture2D = glFramebufferTexture2D_mozilla;
    i->fFrontFace = glFrontFace_mozilla;
    i->fGenBuffers = glGenBuffers_mozilla;
    i->fGenFramebuffers = glGenFramebuffers_mozilla;
    i->fGenRenderbuffers = glGenRenderbuffers_mozilla;
    i->fGetFramebufferAttachmentParameteriv = glGetFramebufferAttachmentParameteriv_mozilla;
    i->fGenTextures = glGenTextures_mozilla;
    i->fGetBufferParameteriv = glGetBufferParameteriv_mozilla;
    i->fGetError = glGetError_mozilla;
    i->fGetIntegerv = glGetIntegerv_mozilla;
    i->fGetProgramInfoLog = glGetProgramInfoLog_mozilla;
    i->fGetProgramiv = glGetProgramiv_mozilla;
    i->fGetRenderbufferParameteriv = glGetRenderbufferParameteriv_mozilla;
    i->fGetShaderInfoLog = glGetShaderInfoLog_mozilla;
    i->fGetShaderiv = glGetShaderiv_mozilla;
    i->fGetString = glGetString_mozilla;
    i->fGetUniformLocation = glGetUniformLocation_mozilla;
    i->fLineWidth = glLineWidth_mozilla;
    i->fLinkProgram = glLinkProgram_mozilla;
    i->fPixelStorei = glPixelStorei_mozilla;
    i->fReadPixels = glReadPixels_mozilla;
    i->fRenderbufferStorage = glRenderbufferStorage_mozilla;
    i->fScissor = glScissor_mozilla;
    i->fShaderSource = glShaderSource_mozilla;
    i->fStencilFunc = glStencilFunc_mozilla;
    i->fStencilMask = glStencilMask_mozilla;
    i->fStencilOp = glStencilOp_mozilla;
    i->fTexImage2D = glTexImage2D_mozilla;
    i->fTexParameteri = glTexParameteri_mozilla;
    i->fTexParameteriv = glTexParameteriv_mozilla;
    i->fTexSubImage2D = glTexSubImage2D_mozilla;
    i->fUniform1f = glUniform1f_mozilla;
    i->fUniform1i = glUniform1i_mozilla;
    i->fUniform1fv = glUniform1fv_mozilla;
    i->fUniform1iv = glUniform1iv_mozilla;
    i->fUniform2f = glUniform2f_mozilla;
    i->fUniform2i = glUniform2i_mozilla;
    i->fUniform2fv = glUniform2fv_mozilla;
    i->fUniform2iv = glUniform2iv_mozilla;
    i->fUniform3f = glUniform3f_mozilla;
    i->fUniform3i = glUniform3i_mozilla;
    i->fUniform3fv = glUniform3fv_mozilla;
    i->fUniform3iv = glUniform3iv_mozilla;
    i->fUniform4f = glUniform4f_mozilla;
    i->fUniform4i = glUniform4i_mozilla;
    i->fUniform4fv = glUniform4fv_mozilla;
    i->fUniform4iv = glUniform4iv_mozilla;
    i->fUniformMatrix2fv = glUniformMatrix2fv_mozilla;
    i->fUniformMatrix3fv = glUniformMatrix3fv_mozilla;
    i->fUniformMatrix4fv = glUniformMatrix4fv_mozilla;
    i->fUseProgram = glUseProgram_mozilla;
    i->fVertexAttrib4fv = glVertexAttrib4fv_mozilla;
    i->fVertexAttribPointer = glVertexAttribPointer_mozilla;
    i->fViewport = glViewport_mozilla;

    // Required for either desktop OpenGL 2.0 or OpenGL ES 2.0
    i->fStencilFuncSeparate = glStencilFuncSeparate_mozilla;
    i->fStencilMaskSeparate = glStencilMaskSeparate_mozilla;
    i->fStencilOpSeparate = glStencilOpSeparate_mozilla;

    // GLContext supports glMapBuffer
    i->fMapBuffer = glMapBuffer_mozilla;
    i->fUnmapBuffer = glUnmapBuffer_mozilla;

    // GLContext supports glRenderbufferStorageMultisample/glBlitFramebuffer
    i->fRenderbufferStorageMultisample = glRenderbufferStorageMultisample_mozilla;
    i->fBlitFramebuffer = glBlitFramebuffer_mozilla;

    // GLContext supports glCompressedTexImage2D
    i->fCompressedTexImage2D = glCompressedTexImage2D_mozilla;

    // Desktop GL 
    i->fGetTexLevelParameteriv = glGetTexLevelParameteriv_mozilla;
    i->fDrawBuffer = glDrawBuffer_mozilla;
    i->fReadBuffer = glReadBuffer_mozilla;

    // Desktop OpenGL > 1.5
    i->fGenQueries = glGenQueries_mozilla;
    i->fDeleteQueries = glDeleteQueries_mozilla;
    i->fBeginQuery = glBeginQuery_mozilla;
    i->fEndQuery = glEndQuery_mozilla;
    i->fGetQueryiv = glGetQueryiv_mozilla;
    i->fGetQueryObjectiv = glGetQueryObjectiv_mozilla;
    i->fGetQueryObjectuiv = glGetQueryObjectuiv_mozilla;

    // Desktop OpenGL > 2.0
    i->fDrawBuffers = glDrawBuffers_mozilla;

    // We support both desktop GL and GLES2
    if (context->IsGLES2()) {
        i->fBindingsExported = kES2_GrGLBinding;
    } else {
        i->fBindingsExported = kDesktop_GrGLBinding;
    }

    return i;
}
