//
// Copyright (c) 2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// validationES.h: Validation functions for generic OpenGL ES entry point parameters

#ifndef LIBANGLE_VALIDATION_ES_H_
#define LIBANGLE_VALIDATION_ES_H_

#include "common/mathutil.h"
#include "libANGLE/PackedGLEnums.h"

#include <GLES2/gl2.h>
#include <GLES3/gl3.h>
#include <GLES3/gl31.h>

namespace egl
{
class Display;
class Image;
}

namespace gl
{
class Context;
struct Format;
class Framebuffer;
struct LinkedUniform;
class Program;
class Shader;

void SetRobustLengthParam(GLsizei *length, GLsizei value);
bool IsETC2EACFormat(const GLenum format);
bool ValidTextureTarget(const Context *context, TextureType type);
bool ValidTexture2DTarget(const Context *context, TextureType type);
bool ValidTexture3DTarget(const Context *context, TextureType target);
bool ValidTextureExternalTarget(const Context *context, TextureType target);
bool ValidTexture2DDestinationTarget(const Context *context, TextureTarget target);
bool ValidTexture3DDestinationTarget(const Context *context, TextureType target);
bool ValidTexLevelDestinationTarget(const Context *context, TextureType type);
bool ValidFramebufferTarget(const Context *context, GLenum target);
bool ValidMipLevel(const Context *context, TextureType type, GLint level);
bool ValidImageSizeParameters(Context *context,
                              TextureType target,
                              GLint level,
                              GLsizei width,
                              GLsizei height,
                              GLsizei depth,
                              bool isSubImage);
bool ValidCompressedImageSize(const Context *context,
                              GLenum internalFormat,
                              GLint level,
                              GLsizei width,
                              GLsizei height);
bool ValidCompressedSubImageSize(const Context *context,
                                 GLenum internalFormat,
                                 GLint xoffset,
                                 GLint yoffset,
                                 GLsizei width,
                                 GLsizei height,
                                 size_t textureWidth,
                                 size_t textureHeight);
bool ValidImageDataSize(Context *context,
                        TextureType texType,
                        GLsizei width,
                        GLsizei height,
                        GLsizei depth,
                        GLenum format,
                        GLenum type,
                        const void *pixels,
                        GLsizei imageSize);

bool ValidQueryType(const Context *context, GLenum queryType);

bool ValidateWebGLVertexAttribPointer(Context *context,
                                      GLenum type,
                                      GLboolean normalized,
                                      GLsizei stride,
                                      const void *ptr,
                                      bool pureInteger);

// Returns valid program if id is a valid program name
// Errors INVALID_OPERATION if valid shader is given and returns NULL
// Errors INVALID_VALUE otherwise and returns NULL
Program *GetValidProgram(Context *context, GLuint id);

// Returns valid shader if id is a valid shader name
// Errors INVALID_OPERATION if valid program is given and returns NULL
// Errors INVALID_VALUE otherwise and returns NULL
Shader *GetValidShader(Context *context, GLuint id);

bool ValidateAttachmentTarget(Context *context, GLenum attachment);
bool ValidateRenderbufferStorageParametersBase(Context *context,
                                               GLenum target,
                                               GLsizei samples,
                                               GLenum internalformat,
                                               GLsizei width,
                                               GLsizei height);
bool ValidateFramebufferRenderbufferParameters(Context *context,
                                               GLenum target,
                                               GLenum attachment,
                                               GLenum renderbuffertarget,
                                               GLuint renderbuffer);

bool ValidateBlitFramebufferParameters(Context *context,
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

bool ValidateReadPixelsBase(Context *context,
                            GLint x,
                            GLint y,
                            GLsizei width,
                            GLsizei height,
                            GLenum format,
                            GLenum type,
                            GLsizei bufSize,
                            GLsizei *length,
                            GLsizei *columns,
                            GLsizei *rows,
                            void *pixels);
bool ValidateReadPixelsRobustANGLE(Context *context,
                                   GLint x,
                                   GLint y,
                                   GLsizei width,
                                   GLsizei height,
                                   GLenum format,
                                   GLenum type,
                                   GLsizei bufSize,
                                   GLsizei *length,
                                   GLsizei *columns,
                                   GLsizei *rows,
                                   void *pixels);
bool ValidateReadnPixelsEXT(Context *context,
                            GLint x,
                            GLint y,
                            GLsizei width,
                            GLsizei height,
                            GLenum format,
                            GLenum type,
                            GLsizei bufSize,
                            void *pixels);
bool ValidateReadnPixelsRobustANGLE(Context *context,
                                    GLint x,
                                    GLint y,
                                    GLsizei width,
                                    GLsizei height,
                                    GLenum format,
                                    GLenum type,
                                    GLsizei bufSize,
                                    GLsizei *length,
                                    GLsizei *columns,
                                    GLsizei *rows,
                                    void *data);

bool ValidateGenQueriesEXT(gl::Context *context, GLsizei n, GLuint *ids);
bool ValidateDeleteQueriesEXT(gl::Context *context, GLsizei n, const GLuint *ids);
bool ValidateIsQueryEXT(gl::Context *context, GLuint id);
bool ValidateBeginQueryBase(Context *context, GLenum target, GLuint id);
bool ValidateBeginQueryEXT(Context *context, GLenum target, GLuint id);
bool ValidateEndQueryBase(Context *context, GLenum target);
bool ValidateEndQueryEXT(Context *context, GLenum target);
bool ValidateQueryCounterEXT(Context *context, GLuint id, GLenum target);
bool ValidateGetQueryivBase(Context *context, GLenum target, GLenum pname, GLsizei *numParams);
bool ValidateGetQueryivEXT(Context *context, GLenum target, GLenum pname, GLint *params);
bool ValidateGetQueryivRobustANGLE(Context *context,
                                   GLenum target,
                                   GLenum pname,
                                   GLsizei bufSize,
                                   GLsizei *length,
                                   GLint *params);
bool ValidateGetQueryObjectValueBase(Context *context,
                                     GLenum target,
                                     GLenum pname,
                                     GLsizei *numParams);
bool ValidateGetQueryObjectivEXT(Context *context, GLuint id, GLenum pname, GLint *params);
bool ValidateGetQueryObjectivRobustANGLE(Context *context,
                                         GLuint id,
                                         GLenum pname,
                                         GLsizei bufSize,
                                         GLsizei *length,
                                         GLint *params);
bool ValidateGetQueryObjectuivEXT(Context *context, GLuint id, GLenum pname, GLuint *params);
bool ValidateGetQueryObjectuivRobustANGLE(Context *context,
                                          GLuint id,
                                          GLenum pname,
                                          GLsizei bufSize,
                                          GLsizei *length,
                                          GLuint *params);
bool ValidateGetQueryObjecti64vEXT(Context *context, GLuint id, GLenum pname, GLint64 *params);
bool ValidateGetQueryObjecti64vRobustANGLE(Context *context,
                                           GLuint id,
                                           GLenum pname,
                                           GLsizei bufSize,
                                           GLsizei *length,
                                           GLint64 *params);
bool ValidateGetQueryObjectui64vEXT(Context *context, GLuint id, GLenum pname, GLuint64 *params);
bool ValidateGetQueryObjectui64vRobustANGLE(Context *context,
                                            GLuint id,
                                            GLenum pname,
                                            GLsizei bufSize,
                                            GLsizei *length,
                                            GLuint64 *params);

bool ValidateUniformCommonBase(Context *context,
                               gl::Program *program,
                               GLint location,
                               GLsizei count,
                               const LinkedUniform **uniformOut);
bool ValidateUniform1ivValue(Context *context,
                             GLenum uniformType,
                             GLsizei count,
                             const GLint *value);
bool ValidateUniformValue(Context *context, GLenum valueType, GLenum uniformType);
bool ValidateUniformMatrixValue(Context *context, GLenum valueType, GLenum uniformType);
bool ValidateUniform(Context *context, GLenum uniformType, GLint location, GLsizei count);
bool ValidateUniformMatrix(Context *context,
                           GLenum matrixType,
                           GLint location,
                           GLsizei count,
                           GLboolean transpose);
bool ValidateGetBooleanvRobustANGLE(Context *context,
                                    GLenum pname,
                                    GLsizei bufSize,
                                    GLsizei *length,
                                    GLboolean *params);
bool ValidateGetFloatvRobustANGLE(Context *context,
                                  GLenum pname,
                                  GLsizei bufSize,
                                  GLsizei *length,
                                  GLfloat *params);
bool ValidateStateQuery(Context *context,
                        GLenum pname,
                        GLenum *nativeType,
                        unsigned int *numParams);
bool ValidateGetIntegervRobustANGLE(Context *context,
                                    GLenum pname,
                                    GLsizei bufSize,
                                    GLsizei *length,
                                    GLint *data);
bool ValidateGetInteger64vRobustANGLE(Context *context,
                                      GLenum pname,
                                      GLsizei bufSize,
                                      GLsizei *length,
                                      GLint64 *data);
bool ValidateRobustStateQuery(Context *context,
                              GLenum pname,
                              GLsizei bufSize,
                              GLenum *nativeType,
                              unsigned int *numParams);

bool ValidateCopyTexImageParametersBase(Context *context,
                                        TextureTarget target,
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
                                        GLint border,
                                        Format *textureFormatOut);

bool ValidateDrawBase(Context *context, GLenum mode, GLsizei count);
bool ValidateDrawArraysCommon(Context *context,
                              GLenum mode,
                              GLint first,
                              GLsizei count,
                              GLsizei primcount);
bool ValidateDrawArraysInstancedBase(Context *context,
                                     GLenum mode,
                                     GLint first,
                                     GLsizei count,
                                     GLsizei primcount);
bool ValidateDrawArraysInstancedANGLE(Context *context,
                                      GLenum mode,
                                      GLint first,
                                      GLsizei count,
                                      GLsizei primcount);

bool ValidateDrawElementsBase(Context *context, GLenum type);
bool ValidateDrawElementsCommon(Context *context,
                                GLenum mode,
                                GLsizei count,
                                GLenum type,
                                const void *indices,
                                GLsizei primcount);

bool ValidateDrawElementsInstancedCommon(Context *context,
                                         GLenum mode,
                                         GLsizei count,
                                         GLenum type,
                                         const void *indices,
                                         GLsizei primcount);
bool ValidateDrawElementsInstancedANGLE(Context *context,
                                        GLenum mode,
                                        GLsizei count,
                                        GLenum type,
                                        const void *indices,
                                        GLsizei primcount);

bool ValidateFramebufferTextureBase(Context *context,
                                    GLenum target,
                                    GLenum attachment,
                                    GLuint texture,
                                    GLint level);

bool ValidateGetUniformBase(Context *context, GLuint program, GLint location);
bool ValidateGetnUniformfvEXT(Context *context,
                              GLuint program,
                              GLint location,
                              GLsizei bufSize,
                              GLfloat *params);
bool ValidateGetnUniformfvRobustANGLE(Context *context,
                                      GLuint program,
                                      GLint location,
                                      GLsizei bufSize,
                                      GLsizei *length,
                                      GLfloat *params);
bool ValidateGetnUniformivEXT(Context *context,
                              GLuint program,
                              GLint location,
                              GLsizei bufSize,
                              GLint *params);
bool ValidateGetnUniformivRobustANGLE(Context *context,
                                      GLuint program,
                                      GLint location,
                                      GLsizei bufSize,
                                      GLsizei *length,
                                      GLint *params);
bool ValidateGetnUniformuivRobustANGLE(Context *context,
                                       GLuint program,
                                       GLint location,
                                       GLsizei bufSize,
                                       GLsizei *length,
                                       GLuint *params);
bool ValidateGetUniformfvRobustANGLE(Context *context,
                                     GLuint program,
                                     GLint location,
                                     GLsizei bufSize,
                                     GLsizei *length,
                                     GLfloat *params);
bool ValidateGetUniformivRobustANGLE(Context *context,
                                     GLuint program,
                                     GLint location,
                                     GLsizei bufSize,
                                     GLsizei *length,
                                     GLint *params);
bool ValidateGetUniformuivRobustANGLE(Context *context,
                                      GLuint program,
                                      GLint location,
                                      GLsizei bufSize,
                                      GLsizei *length,
                                      GLuint *params);

bool ValidateDiscardFramebufferBase(Context *context,
                                    GLenum target,
                                    GLsizei numAttachments,
                                    const GLenum *attachments,
                                    bool defaultFramebuffer);

bool ValidateInsertEventMarkerEXT(Context *context, GLsizei length, const char *marker);
bool ValidatePushGroupMarkerEXT(Context *context, GLsizei length, const char *marker);

bool ValidateEGLImageTargetTexture2DOES(Context *context,
                                        gl::TextureType type,
                                        GLeglImageOES image);
bool ValidateEGLImageTargetRenderbufferStorageOES(Context *context,
                                                  GLenum target,
                                                  GLeglImageOES image);

bool ValidateBindVertexArrayBase(Context *context, GLuint array);

bool ValidateProgramBinaryBase(Context *context,
                               GLuint program,
                               GLenum binaryFormat,
                               const void *binary,
                               GLint length);
bool ValidateGetProgramBinaryBase(Context *context,
                                  GLuint program,
                                  GLsizei bufSize,
                                  GLsizei *length,
                                  GLenum *binaryFormat,
                                  void *binary);

bool ValidateDrawBuffersBase(Context *context, GLsizei n, const GLenum *bufs);

bool ValidateGetBufferPointervBase(Context *context,
                                   BufferBinding target,
                                   GLenum pname,
                                   GLsizei *length,
                                   void **params);
bool ValidateUnmapBufferBase(Context *context, BufferBinding target);
bool ValidateMapBufferRangeBase(Context *context,
                                BufferBinding target,
                                GLintptr offset,
                                GLsizeiptr length,
                                GLbitfield access);
bool ValidateFlushMappedBufferRangeBase(Context *context,
                                        BufferBinding target,
                                        GLintptr offset,
                                        GLsizeiptr length);

bool ValidateGenOrDelete(Context *context, GLint n);

bool ValidateRobustEntryPoint(Context *context, GLsizei bufSize);
bool ValidateRobustBufferSize(Context *context, GLsizei bufSize, GLsizei numParams);

bool ValidateGetFramebufferAttachmentParameterivBase(Context *context,
                                                     GLenum target,
                                                     GLenum attachment,
                                                     GLenum pname,
                                                     GLsizei *numParams);
bool ValidateGetFramebufferAttachmentParameterivRobustANGLE(Context *context,
                                                            GLenum target,
                                                            GLenum attachment,
                                                            GLenum pname,
                                                            GLsizei bufSize,
                                                            GLsizei *length,
                                                            GLint *params);

bool ValidateGetBufferParameterBase(Context *context,
                                    BufferBinding target,
                                    GLenum pname,
                                    bool pointerVersion,
                                    GLsizei *numParams);
bool ValidateGetBufferParameterivRobustANGLE(Context *context,
                                             BufferBinding target,
                                             GLenum pname,
                                             GLsizei bufSize,
                                             GLsizei *length,
                                             GLint *params);

bool ValidateGetBufferParameteri64vRobustANGLE(Context *context,
                                               BufferBinding target,
                                               GLenum pname,
                                               GLsizei bufSize,
                                               GLsizei *length,
                                               GLint64 *params);

bool ValidateGetProgramivBase(Context *context, GLuint program, GLenum pname, GLsizei *numParams);
bool ValidateGetProgramivRobustANGLE(Context *context,
                                     GLuint program,
                                     GLenum pname,
                                     GLsizei bufSize,
                                     GLsizei *length,
                                     GLint *params);

bool ValidateGetRenderbufferParameterivBase(Context *context,
                                            GLenum target,
                                            GLenum pname,
                                            GLsizei *length);
bool ValidateGetRenderbufferParameterivRobustANGLE(Context *context,
                                                   GLenum target,
                                                   GLenum pname,
                                                   GLsizei bufSize,
                                                   GLsizei *length,
                                                   GLint *params);

bool ValidateGetShaderivBase(Context *context, GLuint shader, GLenum pname, GLsizei *length);
bool ValidateGetShaderivRobustANGLE(Context *context,
                                    GLuint shader,
                                    GLenum pname,
                                    GLsizei bufSize,
                                    GLsizei *length,
                                    GLint *params);

bool ValidateGetTexParameterBase(Context *context,
                                 TextureType target,
                                 GLenum pname,
                                 GLsizei *length);
bool ValidateGetTexParameterfvRobustANGLE(Context *context,
                                          TextureType target,
                                          GLenum pname,
                                          GLsizei bufSize,
                                          GLsizei *length,
                                          GLfloat *params);
bool ValidateGetTexParameterivRobustANGLE(Context *context,
                                          TextureType target,
                                          GLenum pname,
                                          GLsizei bufSize,
                                          GLsizei *length,
                                          GLint *params);
bool ValidateGetTexParameterIivRobustANGLE(Context *context,
                                           TextureType target,
                                           GLenum pname,
                                           GLsizei bufSize,
                                           GLsizei *length,
                                           GLint *params);
bool ValidateGetTexParameterIuivRobustANGLE(Context *context,
                                            TextureType target,
                                            GLenum pname,
                                            GLsizei bufSize,
                                            GLsizei *length,
                                            GLuint *params);

template <typename ParamType>
bool ValidateTexParameterBase(Context *context,
                              TextureType target,
                              GLenum pname,
                              GLsizei bufSize,
                              const ParamType *params);
bool ValidateTexParameterfvRobustANGLE(Context *context,
                                       TextureType target,
                                       GLenum pname,
                                       GLsizei bufSize,
                                       const GLfloat *params);
bool ValidateTexParameterivRobustANGLE(Context *context,
                                       TextureType target,
                                       GLenum pname,
                                       GLsizei bufSize,
                                       const GLint *params);
bool ValidateTexParameterIivRobustANGLE(Context *context,
                                        TextureType target,
                                        GLenum pname,
                                        GLsizei bufSize,
                                        const GLint *params);
bool ValidateTexParameterIuivRobustANGLE(Context *context,
                                         TextureType target,
                                         GLenum pname,
                                         GLsizei bufSize,
                                         const GLuint *params);

bool ValidateGetSamplerParameterfvRobustANGLE(Context *context,
                                              GLuint sampler,
                                              GLenum pname,
                                              GLuint bufSize,
                                              GLsizei *length,
                                              GLfloat *params);
bool ValidateGetSamplerParameterivRobustANGLE(Context *context,
                                              GLuint sampler,
                                              GLenum pname,
                                              GLsizei bufSize,
                                              GLsizei *length,
                                              GLint *params);
bool ValidateGetSamplerParameterIivRobustANGLE(Context *context,
                                               GLuint sampler,
                                               GLenum pname,
                                               GLsizei bufSize,
                                               GLsizei *length,
                                               GLint *params);
bool ValidateGetSamplerParameterIuivRobustANGLE(Context *context,
                                                GLuint sampler,
                                                GLenum pname,
                                                GLsizei bufSize,
                                                GLsizei *length,
                                                GLuint *params);

bool ValidateSamplerParameterfvRobustANGLE(Context *context,
                                           GLuint sampler,
                                           GLenum pname,
                                           GLsizei bufSize,
                                           const GLfloat *params);
bool ValidateSamplerParameterivRobustANGLE(Context *context,
                                           GLuint sampler,
                                           GLenum pname,
                                           GLsizei bufSize,
                                           const GLint *params);
bool ValidateSamplerParameterIivRobustANGLE(Context *context,
                                            GLuint sampler,
                                            GLenum pname,
                                            GLsizei bufSize,
                                            const GLint *param);
bool ValidateSamplerParameterIuivRobustANGLE(Context *context,
                                             GLuint sampler,
                                             GLenum pname,
                                             GLsizei bufSize,
                                             const GLuint *param);

bool ValidateGetVertexAttribBase(Context *context,
                                 GLuint index,
                                 GLenum pname,
                                 GLsizei *length,
                                 bool pointer,
                                 bool pureIntegerEntryPoint);
bool ValidateGetVertexAttribfvRobustANGLE(Context *context,
                                          GLuint index,
                                          GLenum pname,
                                          GLsizei bufSize,
                                          GLsizei *length,
                                          GLfloat *params);

bool ValidateGetVertexAttribivRobustANGLE(Context *context,
                                          GLuint index,
                                          GLenum pname,
                                          GLsizei bufSize,
                                          GLsizei *length,
                                          GLint *params);

bool ValidateGetVertexAttribPointervRobustANGLE(Context *context,
                                                GLuint index,
                                                GLenum pname,
                                                GLsizei bufSize,
                                                GLsizei *length,
                                                void **pointer);

bool ValidateGetVertexAttribIivRobustANGLE(Context *context,
                                           GLuint index,
                                           GLenum pname,
                                           GLsizei bufSize,
                                           GLsizei *length,
                                           GLint *params);

bool ValidateGetVertexAttribIuivRobustANGLE(Context *context,
                                            GLuint index,
                                            GLenum pname,
                                            GLsizei bufSize,
                                            GLsizei *length,
                                            GLuint *params);

bool ValidateGetActiveUniformBlockivRobustANGLE(Context *context,
                                                GLuint program,
                                                GLuint uniformBlockIndex,
                                                GLenum pname,
                                                GLsizei bufSize,
                                                GLsizei *length,
                                                GLint *params);

bool ValidateGetInternalFormativRobustANGLE(Context *context,
                                            GLenum target,
                                            GLenum internalformat,
                                            GLenum pname,
                                            GLsizei bufSize,
                                            GLsizei *length,
                                            GLint *params);

bool ValidateVertexFormatBase(Context *context,
                              GLuint attribIndex,
                              GLint size,
                              GLenum type,
                              GLboolean pureInteger);

bool ValidateWebGLFramebufferAttachmentClearType(Context *context,
                                                 GLint drawbuffer,
                                                 const GLenum *validComponentTypes,
                                                 size_t validComponentTypeCount);

bool ValidateRobustCompressedTexImageBase(Context *context, GLsizei imageSize, GLsizei dataSize);

bool ValidateVertexAttribIndex(Context *context, GLuint index);

bool ValidateGetActiveUniformBlockivBase(Context *context,
                                         GLuint program,
                                         GLuint uniformBlockIndex,
                                         GLenum pname,
                                         GLsizei *length);

bool ValidateGetSamplerParameterBase(Context *context,
                                     GLuint sampler,
                                     GLenum pname,
                                     GLsizei *length);

template <typename ParamType>
bool ValidateSamplerParameterBase(Context *context,
                                  GLuint sampler,
                                  GLenum pname,
                                  GLsizei bufSize,
                                  ParamType *params);

bool ValidateGetInternalFormativBase(Context *context,
                                     GLenum target,
                                     GLenum internalformat,
                                     GLenum pname,
                                     GLsizei bufSize,
                                     GLsizei *numParams);

bool ValidateFramebufferComplete(Context *context, Framebuffer *framebuffer, bool isFramebufferOp);
bool ValidateFramebufferNotMultisampled(Context *context, Framebuffer *framebuffer);

bool ValidateMultitextureUnit(Context *context, GLenum texture);

// Utility macro for handling implementation methods inside Validation.
#define ANGLE_HANDLE_VALIDATION_ERR(X) \
    context->handleError(X);           \
    return false;
#define ANGLE_VALIDATION_TRY(EXPR) ANGLE_TRY_TEMPLATE(EXPR, ANGLE_HANDLE_VALIDATION_ERR);

}  // namespace gl

#endif  // LIBANGLE_VALIDATION_ES_H_
