//
// Copyright (c) 2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// validationES3.h: Validation functions for OpenGL ES 3.0 entry point parameters

#ifndef LIBANGLE_VALIDATION_ES3_H_
#define LIBANGLE_VALIDATION_ES3_H_

#include <GLES3/gl3.h>

namespace gl
{
class Context;
class ValidationContext;

bool ValidateES3TexImageParametersBase(ValidationContext *context,
                                       GLenum target,
                                       GLint level,
                                       GLenum internalformat,
                                       bool isCompressed,
                                       bool isSubImage,
                                       GLint xoffset,
                                       GLint yoffset,
                                       GLint zoffset,
                                       GLsizei width,
                                       GLsizei height,
                                       GLsizei depth,
                                       GLint border,
                                       GLenum format,
                                       GLenum type,
                                       const GLvoid *pixels);

bool ValidateES3TexStorageParameters(Context *context,
                                     GLenum target,
                                     GLsizei levels,
                                     GLenum internalformat,
                                     GLsizei width,
                                     GLsizei height,
                                     GLsizei depth);

bool ValidateES3TexImage2DParameters(Context *context,
                                     GLenum target,
                                     GLint level,
                                     GLenum internalformat,
                                     bool isCompressed,
                                     bool isSubImage,
                                     GLint xoffset,
                                     GLint yoffset,
                                     GLint zoffset,
                                     GLsizei width,
                                     GLsizei height,
                                     GLsizei depth,
                                     GLint border,
                                     GLenum format,
                                     GLenum type,
                                     const GLvoid *pixels);

bool ValidateES3TexImage3DParameters(Context *context,
                                     GLenum target,
                                     GLint level,
                                     GLenum internalformat,
                                     bool isCompressed,
                                     bool isSubImage,
                                     GLint xoffset,
                                     GLint yoffset,
                                     GLint zoffset,
                                     GLsizei width,
                                     GLsizei height,
                                     GLsizei depth,
                                     GLint border,
                                     GLenum format,
                                     GLenum type,
                                     const GLvoid *pixels);

bool ValidateES3CopyTexImageParametersBase(ValidationContext *context,
                                           GLenum target,
                                           GLint level,
                                           GLenum internalformat,
                                           bool isSubImage,
                                           GLint xoffset,
                                           GLint yoffset,
                                           GLint zoffset,
                                           GLint x,
                                           GLint y,
                                           GLsizei width,
                                           GLsizei height,
                                           GLint border);

bool ValidateES3CopyTexImage2DParameters(ValidationContext *context,
                                         GLenum target,
                                         GLint level,
                                         GLenum internalformat,
                                         bool isSubImage,
                                         GLint xoffset,
                                         GLint yoffset,
                                         GLint zoffset,
                                         GLint x,
                                         GLint y,
                                         GLsizei width,
                                         GLsizei height,
                                         GLint border);

bool ValidateES3CopyTexImage3DParameters(ValidationContext *context,
                                         GLenum target,
                                         GLint level,
                                         GLenum internalformat,
                                         bool isSubImage,
                                         GLint xoffset,
                                         GLint yoffset,
                                         GLint zoffset,
                                         GLint x,
                                         GLint y,
                                         GLsizei width,
                                         GLsizei height,
                                         GLint border);

bool ValidateES3TexStorageParametersBase(Context *context,
                                         GLenum target,
                                         GLsizei levels,
                                         GLenum internalformat,
                                         GLsizei width,
                                         GLsizei height,
                                         GLsizei depth);

bool ValidateES3TexStorage2DParameters(Context *context,
                                       GLenum target,
                                       GLsizei levels,
                                       GLenum internalformat,
                                       GLsizei width,
                                       GLsizei height,
                                       GLsizei depth);

bool ValidateES3TexStorage3DParameters(Context *context,
                                       GLenum target,
                                       GLsizei levels,
                                       GLenum internalformat,
                                       GLsizei width,
                                       GLsizei height,
                                       GLsizei depth);

bool ValidateGenQueries(Context *context, GLsizei n, const GLuint *ids);

bool ValidateDeleteQueries(Context *context, GLsizei n, const GLuint *ids);

bool ValidateBeginQuery(Context *context, GLenum target, GLuint id);

bool ValidateEndQuery(Context *context, GLenum target);

bool ValidateGetQueryiv(Context *context, GLenum target, GLenum pname, GLint *params);

bool ValidateGetQueryObjectuiv(Context *context, GLuint id, GLenum pname, GLuint *params);

bool ValidateFramebufferTextureLayer(Context *context, GLenum target, GLenum attachment,
                                     GLuint texture, GLint level, GLint layer);

bool ValidES3ReadFormatType(Context *context, GLenum internalFormat, GLenum format, GLenum type);

bool ValidateES3RenderbufferStorageParameters(Context *context, GLenum target, GLsizei samples,
                                              GLenum internalformat, GLsizei width, GLsizei height);

bool ValidateInvalidateFramebuffer(Context *context, GLenum target, GLsizei numAttachments,
                                   const GLenum *attachments);

bool ValidateClearBuffer(ValidationContext *context);

bool ValidateGetUniformuiv(Context *context, GLuint program, GLint location, GLuint* params);

bool ValidateReadBuffer(Context *context, GLenum mode);

bool ValidateCompressedTexImage3D(Context *context,
                                  GLenum target,
                                  GLint level,
                                  GLenum internalformat,
                                  GLsizei width,
                                  GLsizei height,
                                  GLsizei depth,
                                  GLint border,
                                  GLsizei imageSize,
                                  const GLvoid *data);

bool ValidateBindVertexArray(Context *context, GLuint array);
bool ValidateDeleteVertexArrays(Context *context, GLsizei n);
bool ValidateGenVertexArrays(Context *context, GLsizei n);
bool ValidateIsVertexArray(Context *context);

bool ValidateProgramBinary(Context *context,
                           GLuint program,
                           GLenum binaryFormat,
                           const void *binary,
                           GLint length);
bool ValidateGetProgramBinary(Context *context,
                              GLuint program,
                              GLsizei bufSize,
                              GLsizei *length,
                              GLenum *binaryFormat,
                              void *binary);
bool ValidateProgramParameter(Context *context, GLuint program, GLenum pname, GLint value);
bool ValidateBlitFramebuffer(Context *context,
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
bool ValidateClearBufferiv(ValidationContext *context,
                           GLenum buffer,
                           GLint drawbuffer,
                           const GLint *value);
bool ValidateClearBufferuiv(ValidationContext *context,
                            GLenum buffer,
                            GLint drawbuffer,
                            const GLuint *value);
bool ValidateClearBufferfv(ValidationContext *context,
                           GLenum buffer,
                           GLint drawbuffer,
                           const GLfloat *value);
bool ValidateClearBufferfi(ValidationContext *context,
                           GLenum buffer,
                           GLint drawbuffer,
                           GLfloat depth,
                           GLint stencil);
bool ValidateDrawBuffers(ValidationContext *context, GLsizei n, const GLenum *bufs);
bool ValidateCopyTexSubImage3D(Context *context,
                               GLenum target,
                               GLint level,
                               GLint xoffset,
                               GLint yoffset,
                               GLint zoffset,
                               GLint x,
                               GLint y,
                               GLsizei width,
                               GLsizei height);

}  // namespace gl

#endif // LIBANGLE_VALIDATION_ES3_H_
