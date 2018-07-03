//
// Copyright (c) 2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// validationES2.h: Validation functions for OpenGL ES 2.0 entry point parameters

#ifndef LIBANGLE_VALIDATION_ES2_H_
#define LIBANGLE_VALIDATION_ES2_H_

#include "libANGLE/PackedEnums.h"

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

namespace gl
{
class Context;

bool ValidateES2TexStorageParameters(Context *context,
                                     GLenum target,
                                     GLsizei levels,
                                     GLenum internalformat,
                                     GLsizei width,
                                     GLsizei height);

bool ValidateDiscardFramebufferEXT(Context *context,
                                   GLenum target,
                                   GLsizei numAttachments,
                                   const GLenum *attachments);

bool ValidateDrawBuffersEXT(Context *context, GLsizei n, const GLenum *bufs);

bool ValidateBindVertexArrayOES(Context *context, GLuint array);
bool ValidateDeleteVertexArraysOES(Context *context, GLsizei n, const GLuint *arrays);
bool ValidateGenVertexArraysOES(Context *context, GLsizei n, GLuint *arrays);
bool ValidateIsVertexArrayOES(Context *context, GLuint array);

bool ValidateProgramBinaryOES(Context *context,
                              GLuint program,
                              GLenum binaryFormat,
                              const void *binary,
                              GLint length);
bool ValidateGetProgramBinaryOES(Context *context,
                                 GLuint program,
                                 GLsizei bufSize,
                                 GLsizei *length,
                                 GLenum *binaryFormat,
                                 void *binary);

// GL_KHR_debug
bool ValidateDebugMessageControlKHR(Context *context,
                                    GLenum source,
                                    GLenum type,
                                    GLenum severity,
                                    GLsizei count,
                                    const GLuint *ids,
                                    GLboolean enabled);
bool ValidateDebugMessageInsertKHR(Context *context,
                                   GLenum source,
                                   GLenum type,
                                   GLuint id,
                                   GLenum severity,
                                   GLsizei length,
                                   const GLchar *buf);
bool ValidateDebugMessageCallbackKHR(Context *context,
                                     GLDEBUGPROCKHR callback,
                                     const void *userParam);
bool ValidateGetDebugMessageLogKHR(Context *context,
                                   GLuint count,
                                   GLsizei bufSize,
                                   GLenum *sources,
                                   GLenum *types,
                                   GLuint *ids,
                                   GLenum *severities,
                                   GLsizei *lengths,
                                   GLchar *messageLog);
bool ValidatePushDebugGroupKHR(Context *context,
                               GLenum source,
                               GLuint id,
                               GLsizei length,
                               const GLchar *message);
bool ValidatePopDebugGroupKHR(Context *context);
bool ValidateObjectLabelKHR(Context *context,
                            GLenum identifier,
                            GLuint name,
                            GLsizei length,
                            const GLchar *label);
bool ValidateGetObjectLabelKHR(Context *context,
                               GLenum identifier,
                               GLuint name,
                               GLsizei bufSize,
                               GLsizei *length,
                               GLchar *label);
bool ValidateObjectPtrLabelKHR(Context *context,
                               const void *ptr,
                               GLsizei length,
                               const GLchar *label);
bool ValidateGetObjectPtrLabelKHR(Context *context,
                                  const void *ptr,
                                  GLsizei bufSize,
                                  GLsizei *length,
                                  GLchar *label);
bool ValidateGetPointervKHR(Context *context, GLenum pname, void **params);
bool ValidateGetPointervRobustANGLERobustANGLE(Context *context,
                                               GLenum pname,
                                               GLsizei bufSize,
                                               GLsizei *length,
                                               void **params);
bool ValidateBlitFramebufferANGLE(Context *context,
                                  GLint srcX0,
                                  GLint srcY0,
                                  GLint srcX1,
                                  GLint srcY1,
                                  GLint dstX0,
                                  GLint dstY0,
                                  GLint dstX1,
                                  GLint dstY1,
                                  GLbitfield mask,
                                  GLenum filter);

bool ValidateClear(Context *context, GLbitfield mask);
bool ValidateTexImage2D(Context *context,
                        TextureTarget target,
                        GLint level,
                        GLint internalformat,
                        GLsizei width,
                        GLsizei height,
                        GLint border,
                        GLenum format,
                        GLenum type,
                        const void *pixels);
bool ValidateTexImage2DRobustANGLE(Context *context,
                                   TextureTarget target,
                                   GLint level,
                                   GLint internalformat,
                                   GLsizei width,
                                   GLsizei height,
                                   GLint border,
                                   GLenum format,
                                   GLenum type,
                                   GLsizei bufSize,
                                   const void *pixels);
bool ValidateTexSubImage2D(Context *context,
                           TextureTarget target,
                           GLint level,
                           GLint xoffset,
                           GLint yoffset,
                           GLsizei width,
                           GLsizei height,
                           GLenum format,
                           GLenum type,
                           const void *pixels);
bool ValidateTexSubImage2DRobustANGLE(Context *context,
                                      TextureTarget target,
                                      GLint level,
                                      GLint xoffset,
                                      GLint yoffset,
                                      GLsizei width,
                                      GLsizei height,
                                      GLenum format,
                                      GLenum type,
                                      GLsizei bufSize,
                                      const void *pixels);
bool ValidateCompressedTexImage2D(Context *context,
                                  TextureTarget target,
                                  GLint level,
                                  GLenum internalformat,
                                  GLsizei width,
                                  GLsizei height,
                                  GLint border,
                                  GLsizei imageSize,
                                  const void *data);
bool ValidateCompressedTexSubImage2D(Context *context,
                                     TextureTarget target,
                                     GLint level,
                                     GLint xoffset,
                                     GLint yoffset,
                                     GLsizei width,
                                     GLsizei height,
                                     GLenum format,
                                     GLsizei imageSize,
                                     const void *data);
bool ValidateCompressedTexImage2DRobustANGLE(Context *context,
                                             TextureTarget target,
                                             GLint level,
                                             GLenum internalformat,
                                             GLsizei width,
                                             GLsizei height,
                                             GLint border,
                                             GLsizei imageSize,
                                             GLsizei dataSize,
                                             const void *data);
bool ValidateCompressedTexSubImage2DRobustANGLE(Context *context,
                                                TextureTarget target,
                                                GLint level,
                                                GLint xoffset,
                                                GLint yoffset,
                                                GLsizei width,
                                                GLsizei height,
                                                GLenum format,
                                                GLsizei imageSize,
                                                GLsizei dataSize,
                                                const void *data);

bool ValidateBindTexture(Context *context, TextureType target, GLuint texture);

bool ValidateGetBufferPointervOES(Context *context,
                                  BufferBinding target,
                                  GLenum pname,
                                  void **params);
bool ValidateMapBufferOES(Context *context, BufferBinding target, GLenum access);
bool ValidateUnmapBufferOES(Context *context, BufferBinding target);
bool ValidateMapBufferRangeEXT(Context *context,
                               BufferBinding target,
                               GLintptr offset,
                               GLsizeiptr length,
                               GLbitfield access);
bool ValidateMapBufferBase(Context *context, BufferBinding target);
bool ValidateFlushMappedBufferRangeEXT(Context *context,
                                       BufferBinding target,
                                       GLintptr offset,
                                       GLsizeiptr length);

bool ValidateBindUniformLocationCHROMIUM(Context *context,
                                         GLuint program,
                                         GLint location,
                                         const GLchar *name);

bool ValidateCoverageModulationCHROMIUM(Context *context, GLenum components);

// CHROMIUM_path_rendering
bool ValidateMatrixLoadfCHROMIUM(Context *context, GLenum matrixMode, const GLfloat *matrix);
bool ValidateMatrixLoadIdentityCHROMIUM(Context *context, GLenum matrixMode);
bool ValidateGenPathsCHROMIUM(Context *context, GLsizei range);
bool ValidateDeletePathsCHROMIUM(Context *context, GLuint first, GLsizei range);
bool ValidatePathCommandsCHROMIUM(Context *context,
                                  GLuint path,
                                  GLsizei numCommands,
                                  const GLubyte *commands,
                                  GLsizei numCoords,
                                  GLenum coordType,
                                  const void *coords);
bool ValidatePathParameterfCHROMIUM(Context *context, GLuint path, GLenum pname, GLfloat value);
bool ValidatePathParameteriCHROMIUM(Context *context, GLuint path, GLenum pname, GLint value);
bool ValidateGetPathParameterfvCHROMIUM(Context *context,
                                        GLuint path,
                                        GLenum pname,
                                        GLfloat *value);
bool ValidateGetPathParameterivCHROMIUM(Context *context, GLuint path, GLenum pname, GLint *value);
bool ValidatePathStencilFuncCHROMIUM(Context *context, GLenum func, GLint ref, GLuint mask);
bool ValidateStencilFillPathCHROMIUM(Context *context, GLuint path, GLenum fillMode, GLuint mask);
bool ValidateStencilStrokePathCHROMIUM(Context *context, GLuint path, GLint reference, GLuint mask);
bool ValidateCoverFillPathCHROMIUM(Context *context, GLuint path, GLenum coverMode);
bool ValidateCoverStrokePathCHROMIUM(Context *context, GLuint path, GLenum coverMode);
bool ValidateCoverPathCHROMIUM(Context *context, GLuint path, GLenum coverMode);
bool ValidateStencilThenCoverFillPathCHROMIUM(Context *context,
                                              GLuint path,
                                              GLenum fillMode,
                                              GLuint mask,
                                              GLenum coverMode);
bool ValidateStencilThenCoverStrokePathCHROMIUM(Context *context,
                                                GLuint path,
                                                GLint reference,
                                                GLuint mask,
                                                GLenum coverMode);
bool ValidateIsPathCHROMIUM(Context *context, GLuint path);
bool ValidateCoverFillPathInstancedCHROMIUM(Context *context,
                                            GLsizei numPaths,
                                            GLenum pathNameType,
                                            const void *paths,
                                            GLuint pathBase,
                                            GLenum coverMode,
                                            GLenum transformType,
                                            const GLfloat *transformValues);
bool ValidateCoverStrokePathInstancedCHROMIUM(Context *context,
                                              GLsizei numPaths,
                                              GLenum pathNameType,
                                              const void *paths,
                                              GLuint pathBase,
                                              GLenum coverMode,
                                              GLenum transformType,
                                              const GLfloat *transformValues);
bool ValidateStencilFillPathInstancedCHROMIUM(Context *context,
                                              GLsizei numPaths,
                                              GLenum pathNameType,
                                              const void *paths,
                                              GLuint pathBAse,
                                              GLenum fillMode,
                                              GLuint mask,
                                              GLenum transformType,
                                              const GLfloat *transformValues);
bool ValidateStencilStrokePathInstancedCHROMIUM(Context *context,
                                                GLsizei numPaths,
                                                GLenum pathNameType,
                                                const void *paths,
                                                GLuint pathBase,
                                                GLint reference,
                                                GLuint mask,
                                                GLenum transformType,
                                                const GLfloat *transformValues);
bool ValidateStencilThenCoverFillPathInstancedCHROMIUM(Context *context,
                                                       GLsizei numPaths,
                                                       GLenum pathNameType,
                                                       const void *paths,
                                                       GLuint pathBase,
                                                       GLenum fillMode,
                                                       GLuint mask,
                                                       GLenum coverMode,
                                                       GLenum transformType,
                                                       const GLfloat *transformValues);
bool ValidateStencilThenCoverStrokePathInstancedCHROMIUM(Context *context,
                                                         GLsizei numPaths,
                                                         GLenum pathNameType,
                                                         const void *paths,
                                                         GLuint pathBase,
                                                         GLint reference,
                                                         GLuint mask,
                                                         GLenum coverMode,
                                                         GLenum transformType,
                                                         const GLfloat *transformValues);
bool ValidateBindFragmentInputLocationCHROMIUM(Context *context,
                                               GLuint program,
                                               GLint location,
                                               const GLchar *name);
bool ValidateProgramPathFragmentInputGenCHROMIUM(Context *context,
                                                 GLuint program,
                                                 GLint location,
                                                 GLenum genMode,
                                                 GLint components,
                                                 const GLfloat *coeffs);

bool ValidateCopyTextureCHROMIUM(Context *context,
                                 GLuint sourceId,
                                 GLint sourceLevel,
                                 TextureTarget destTarget,
                                 GLuint destId,
                                 GLint destLevel,
                                 GLint internalFormat,
                                 GLenum destType,
                                 GLboolean unpackFlipY,
                                 GLboolean unpackPremultiplyAlpha,
                                 GLboolean unpackUnmultiplyAlpha);
bool ValidateCopySubTextureCHROMIUM(Context *context,
                                    GLuint sourceId,
                                    GLint sourceLevel,
                                    TextureTarget destTarget,
                                    GLuint destId,
                                    GLint destLevel,
                                    GLint xoffset,
                                    GLint yoffset,
                                    GLint x,
                                    GLint y,
                                    GLsizei width,
                                    GLsizei height,
                                    GLboolean unpackFlipY,
                                    GLboolean unpackPremultiplyAlpha,
                                    GLboolean unpackUnmultiplyAlpha);
bool ValidateCompressedCopyTextureCHROMIUM(Context *context, GLuint sourceId, GLuint destId);

bool ValidateCreateShader(Context *context, ShaderType type);
bool ValidateBufferData(Context *context,
                        BufferBinding target,
                        GLsizeiptr size,
                        const void *data,
                        BufferUsage usage);
bool ValidateBufferSubData(Context *context,
                           BufferBinding target,
                           GLintptr offset,
                           GLsizeiptr size,
                           const void *data);

bool ValidateRequestExtensionANGLE(Context *context, const GLchar *name);

bool ValidateActiveTexture(Context *context, GLenum texture);
bool ValidateAttachShader(Context *context, GLuint program, GLuint shader);
bool ValidateBindAttribLocation(Context *context, GLuint program, GLuint index, const GLchar *name);
bool ValidateBindBuffer(Context *context, BufferBinding target, GLuint buffer);
bool ValidateBindFramebuffer(Context *context, GLenum target, GLuint framebuffer);
bool ValidateBindRenderbuffer(Context *context, GLenum target, GLuint renderbuffer);
bool ValidateBlendColor(Context *context,
                        GLclampf red,
                        GLclampf green,
                        GLclampf blue,
                        GLclampf alpha);
bool ValidateBlendEquation(Context *context, GLenum mode);
bool ValidateBlendEquationSeparate(Context *context, GLenum modeRGB, GLenum modeAlpha);
bool ValidateBlendFunc(Context *context, GLenum sfactor, GLenum dfactor);
bool ValidateBlendFuncSeparate(Context *context,
                               GLenum srcRGB,
                               GLenum dstRGB,
                               GLenum srcAlpha,
                               GLenum dstAlpha);

bool ValidateGetString(Context *context, GLenum name);
bool ValidateLineWidth(Context *context, GLfloat width);
bool ValidateVertexAttribPointer(Context *context,
                                 GLuint index,
                                 GLint size,
                                 GLenum type,
                                 GLboolean normalized,
                                 GLsizei stride,
                                 const void *ptr);

bool ValidateDepthRangef(Context *context, GLclampf zNear, GLclampf zFar);
bool ValidateRenderbufferStorage(Context *context,
                                 GLenum target,
                                 GLenum internalformat,
                                 GLsizei width,
                                 GLsizei height);
bool ValidateRenderbufferStorageMultisampleANGLE(Context *context,
                                                 GLenum target,
                                                 GLsizei samples,
                                                 GLenum internalformat,
                                                 GLsizei width,
                                                 GLsizei height);

bool ValidateCheckFramebufferStatus(Context *context, GLenum target);
bool ValidateClearColor(Context *context,
                        GLclampf red,
                        GLclampf green,
                        GLclampf blue,
                        GLclampf alpha);
bool ValidateClearDepthf(Context *context, GLclampf depth);
bool ValidateClearStencil(Context *context, GLint s);
bool ValidateColorMask(Context *context,
                       GLboolean red,
                       GLboolean green,
                       GLboolean blue,
                       GLboolean alpha);
bool ValidateCompileShader(Context *context, GLuint shader);
bool ValidateCreateProgram(Context *context);
bool ValidateCullFace(Context *context, CullFaceMode mode);
bool ValidateDeleteProgram(Context *context, GLuint program);
bool ValidateDeleteShader(Context *context, GLuint shader);
bool ValidateDepthFunc(Context *context, GLenum func);
bool ValidateDepthMask(Context *context, GLboolean flag);
bool ValidateDetachShader(Context *context, GLuint program, GLuint shader);
bool ValidateDisableVertexAttribArray(Context *context, GLuint index);
bool ValidateEnableVertexAttribArray(Context *context, GLuint index);
bool ValidateFinish(Context *context);
bool ValidateFlush(Context *context);
bool ValidateFrontFace(Context *context, GLenum mode);
bool ValidateGetActiveAttrib(Context *context,
                             GLuint program,
                             GLuint index,
                             GLsizei bufsize,
                             GLsizei *length,
                             GLint *size,
                             GLenum *type,
                             GLchar *name);
bool ValidateGetActiveUniform(Context *context,
                              GLuint program,
                              GLuint index,
                              GLsizei bufsize,
                              GLsizei *length,
                              GLint *size,
                              GLenum *type,
                              GLchar *name);
bool ValidateGetAttachedShaders(Context *context,
                                GLuint program,
                                GLsizei maxcount,
                                GLsizei *count,
                                GLuint *shaders);
bool ValidateGetAttribLocation(Context *context, GLuint program, const GLchar *name);
bool ValidateGetBooleanv(Context *context, GLenum pname, GLboolean *params);
bool ValidateGetError(Context *context);
bool ValidateGetFloatv(Context *context, GLenum pname, GLfloat *params);
bool ValidateGetIntegerv(Context *context, GLenum pname, GLint *params);
bool ValidateGetProgramInfoLog(Context *context,
                               GLuint program,
                               GLsizei bufsize,
                               GLsizei *length,
                               GLchar *infolog);
bool ValidateGetShaderInfoLog(Context *context,
                              GLuint shader,
                              GLsizei bufsize,
                              GLsizei *length,
                              GLchar *infolog);
bool ValidateGetShaderPrecisionFormat(Context *context,
                                      GLenum shadertype,
                                      GLenum precisiontype,
                                      GLint *range,
                                      GLint *precision);
bool ValidateGetShaderSource(Context *context,
                             GLuint shader,
                             GLsizei bufsize,
                             GLsizei *length,
                             GLchar *source);
bool ValidateGetUniformLocation(Context *context, GLuint program, const GLchar *name);
bool ValidateHint(Context *context, GLenum target, GLenum mode);
bool ValidateIsBuffer(Context *context, GLuint buffer);
bool ValidateIsFramebuffer(Context *context, GLuint framebuffer);
bool ValidateIsProgram(Context *context, GLuint program);
bool ValidateIsRenderbuffer(Context *context, GLuint renderbuffer);
bool ValidateIsShader(Context *context, GLuint shader);
bool ValidateIsTexture(Context *context, GLuint texture);
bool ValidatePixelStorei(Context *context, GLenum pname, GLint param);
bool ValidatePolygonOffset(Context *context, GLfloat factor, GLfloat units);
bool ValidateReleaseShaderCompiler(Context *context);
bool ValidateSampleCoverage(Context *context, GLclampf value, GLboolean invert);
bool ValidateScissor(Context *context, GLint x, GLint y, GLsizei width, GLsizei height);
bool ValidateShaderBinary(Context *context,
                          GLsizei n,
                          const GLuint *shaders,
                          GLenum binaryformat,
                          const void *binary,
                          GLsizei length);
bool ValidateShaderSource(Context *context,
                          GLuint shader,
                          GLsizei count,
                          const GLchar *const *string,
                          const GLint *length);
bool ValidateStencilFunc(Context *context, GLenum func, GLint ref, GLuint mask);
bool ValidateStencilFuncSeparate(Context *context,
                                 GLenum face,
                                 GLenum func,
                                 GLint ref,
                                 GLuint mask);
bool ValidateStencilMask(Context *context, GLuint mask);
bool ValidateStencilMaskSeparate(Context *context, GLenum face, GLuint mask);
bool ValidateStencilOp(Context *context, GLenum fail, GLenum zfail, GLenum zpass);
bool ValidateStencilOpSeparate(Context *context,
                               GLenum face,
                               GLenum fail,
                               GLenum zfail,
                               GLenum zpass);
bool ValidateUniform1f(Context *context, GLint location, GLfloat x);
bool ValidateUniform1fv(Context *context, GLint location, GLsizei count, const GLfloat *v);
bool ValidateUniform1i(Context *context, GLint location, GLint x);
bool ValidateUniform2f(Context *context, GLint location, GLfloat x, GLfloat y);
bool ValidateUniform2fv(Context *context, GLint location, GLsizei count, const GLfloat *v);
bool ValidateUniform2i(Context *context, GLint location, GLint x, GLint y);
bool ValidateUniform2iv(Context *context, GLint location, GLsizei count, const GLint *v);
bool ValidateUniform3f(Context *context, GLint location, GLfloat x, GLfloat y, GLfloat z);
bool ValidateUniform3fv(Context *context, GLint location, GLsizei count, const GLfloat *v);
bool ValidateUniform3i(Context *context, GLint location, GLint x, GLint y, GLint z);
bool ValidateUniform3iv(Context *context, GLint location, GLsizei count, const GLint *v);
bool ValidateUniform4f(Context *context,
                       GLint location,
                       GLfloat x,
                       GLfloat y,
                       GLfloat z,
                       GLfloat w);
bool ValidateUniform4fv(Context *context, GLint location, GLsizei count, const GLfloat *v);
bool ValidateUniform4i(Context *context, GLint location, GLint x, GLint y, GLint z, GLint w);
bool ValidateUniform4iv(Context *context, GLint location, GLsizei count, const GLint *v);
bool ValidateUniformMatrix2fv(Context *context,
                              GLint location,
                              GLsizei count,
                              GLboolean transpose,
                              const GLfloat *value);
bool ValidateUniformMatrix3fv(Context *context,
                              GLint location,
                              GLsizei count,
                              GLboolean transpose,
                              const GLfloat *value);
bool ValidateUniformMatrix4fv(Context *context,
                              GLint location,
                              GLsizei count,
                              GLboolean transpose,
                              const GLfloat *value);
bool ValidateValidateProgram(Context *context, GLuint program);
bool ValidateVertexAttrib1f(Context *context, GLuint index, GLfloat x);
bool ValidateVertexAttrib1fv(Context *context, GLuint index, const GLfloat *values);
bool ValidateVertexAttrib2f(Context *context, GLuint index, GLfloat x, GLfloat y);
bool ValidateVertexAttrib2fv(Context *context, GLuint index, const GLfloat *values);
bool ValidateVertexAttrib3f(Context *context, GLuint index, GLfloat x, GLfloat y, GLfloat z);
bool ValidateVertexAttrib3fv(Context *context, GLuint index, const GLfloat *values);
bool ValidateVertexAttrib4f(Context *context,
                            GLuint index,
                            GLfloat x,
                            GLfloat y,
                            GLfloat z,
                            GLfloat w);
bool ValidateVertexAttrib4fv(Context *context, GLuint index, const GLfloat *values);
bool ValidateViewport(Context *context, GLint x, GLint y, GLsizei width, GLsizei height);
bool ValidateDrawElements(Context *context,
                          GLenum mode,
                          GLsizei count,
                          GLenum type,
                          const void *indices);

bool ValidateDrawArrays(Context *context, GLenum mode, GLint first, GLsizei count);

bool ValidateGetFramebufferAttachmentParameteriv(Context *context,
                                                 GLenum target,
                                                 GLenum attachment,
                                                 GLenum pname,
                                                 GLint *params);
bool ValidateGetProgramiv(Context *context, GLuint program, GLenum pname, GLint *params);

bool ValidateCopyTexImage2D(Context *context,
                            TextureTarget target,
                            GLint level,
                            GLenum internalformat,
                            GLint x,
                            GLint y,
                            GLsizei width,
                            GLsizei height,
                            GLint border);

bool ValidateCopyTexSubImage2D(Context *context,
                               TextureTarget target,
                               GLint level,
                               GLint xoffset,
                               GLint yoffset,
                               GLint x,
                               GLint y,
                               GLsizei width,
                               GLsizei height);

bool ValidateDeleteBuffers(Context *context, GLint n, const GLuint *buffers);
bool ValidateDeleteFramebuffers(Context *context, GLint n, const GLuint *framebuffers);
bool ValidateDeleteRenderbuffers(Context *context, GLint n, const GLuint *renderbuffers);
bool ValidateDeleteTextures(Context *context, GLint n, const GLuint *textures);
bool ValidateDisable(Context *context, GLenum cap);
bool ValidateEnable(Context *context, GLenum cap);
bool ValidateFramebufferRenderbuffer(Context *context,
                                     GLenum target,
                                     GLenum attachment,
                                     GLenum renderbuffertarget,
                                     GLuint renderbuffer);
bool ValidateFramebufferTexture2D(Context *context,
                                  GLenum target,
                                  GLenum attachment,
                                  TextureTarget textarget,
                                  GLuint texture,
                                  GLint level);
bool ValidateGenBuffers(Context *context, GLint n, GLuint *buffers);
bool ValidateGenerateMipmap(Context *context, TextureType target);
bool ValidateGenFramebuffers(Context *context, GLint n, GLuint *framebuffers);
bool ValidateGenRenderbuffers(Context *context, GLint n, GLuint *renderbuffers);
bool ValidateGenTextures(Context *context, GLint n, GLuint *textures);
bool ValidateGetBufferParameteriv(Context *context,
                                  BufferBinding target,
                                  GLenum pname,
                                  GLint *params);
bool ValidateGetRenderbufferParameteriv(Context *context,
                                        GLenum target,
                                        GLenum pname,
                                        GLint *params);
bool ValidateGetShaderiv(Context *context, GLuint shader, GLenum pname, GLint *params);
bool ValidateGetTexParameterfv(Context *context, TextureType target, GLenum pname, GLfloat *params);
bool ValidateGetTexParameteriv(Context *context, TextureType target, GLenum pname, GLint *params);
bool ValidateGetUniformfv(Context *context, GLuint program, GLint location, GLfloat *params);
bool ValidateGetUniformiv(Context *context, GLuint program, GLint location, GLint *params);
bool ValidateGetVertexAttribfv(Context *context, GLuint index, GLenum pname, GLfloat *params);
bool ValidateGetVertexAttribiv(Context *context, GLuint index, GLenum pname, GLint *params);
bool ValidateGetVertexAttribPointerv(Context *context, GLuint index, GLenum pname, void **pointer);
bool ValidateIsEnabled(Context *context, GLenum cap);
bool ValidateLinkProgram(Context *context, GLuint program);
bool ValidateReadPixels(Context *context,
                        GLint x,
                        GLint y,
                        GLsizei width,
                        GLsizei height,
                        GLenum format,
                        GLenum type,
                        void *pixels);
bool ValidateTexParameterf(Context *context, TextureType target, GLenum pname, GLfloat param);
bool ValidateTexParameterfv(Context *context,
                            TextureType target,
                            GLenum pname,
                            const GLfloat *params);
bool ValidateTexParameteri(Context *context, TextureType target, GLenum pname, GLint param);
bool ValidateTexParameteriv(Context *context,
                            TextureType target,
                            GLenum pname,
                            const GLint *params);
bool ValidateUniform1iv(Context *context, GLint location, GLsizei count, const GLint *value);
bool ValidateUseProgram(Context *context, GLuint program);

// Extension validation
bool ValidateDeleteFencesNV(Context *context, GLsizei n, const GLuint *fences);
bool ValidateFinishFenceNV(Context *context, GLuint fence);
bool ValidateGenFencesNV(Context *context, GLsizei n, GLuint *fences);
bool ValidateGetFenceivNV(Context *context, GLuint fence, GLenum pname, GLint *params);
bool ValidateGetGraphicsResetStatusEXT(Context *context);
bool ValidateGetTranslatedShaderSourceANGLE(Context *context,
                                            GLuint shader,
                                            GLsizei bufsize,
                                            GLsizei *length,
                                            GLchar *source);
bool ValidateIsFenceNV(Context *context, GLuint fence);
bool ValidateSetFenceNV(Context *context, GLuint fence, GLenum condition);
bool ValidateTestFenceNV(Context *context, GLuint fence);
bool ValidateTexStorage2DEXT(Context *context,
                             TextureType type,
                             GLsizei levels,
                             GLenum internalformat,
                             GLsizei width,
                             GLsizei height);
bool ValidateVertexAttribDivisorANGLE(Context *context, GLuint index, GLuint divisor);
bool ValidateTexImage3DOES(Context *context,
                           GLenum target,
                           GLint level,
                           GLenum internalformat,
                           GLsizei width,
                           GLsizei height,
                           GLsizei depth,
                           GLint border,
                           GLenum format,
                           GLenum type,
                           const void *pixels);
bool ValidatePopGroupMarkerEXT(Context *context);
bool ValidateTexStorage1DEXT(Context *context,
                             GLenum target,
                             GLsizei levels,
                             GLenum internalformat,
                             GLsizei width);
bool ValidateTexStorage3DEXT(Context *context,
                             TextureType target,
                             GLsizei levels,
                             GLenum internalformat,
                             GLsizei width,
                             GLsizei height,
                             GLsizei depth);

}  // namespace gl

#endif  // LIBANGLE_VALIDATION_ES2_H_
