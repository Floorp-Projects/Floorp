//
// Copyright(c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// entry_points_gles_2_0_ext.h : Defines the GLES 2.0 extension entry points.

#ifndef LIBGLESV2_ENTRYPOINTGLES20EXT_H_
#define LIBGLESV2_ENTRYPOINTGLES20EXT_H_

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <export.h>

namespace gl
{

// GL_CHROMIUM_bind_uniform_location
ANGLE_EXPORT void GL_APIENTRY BindUniformLocationCHROMIUM(GLuint program,
                                                          GLint location,
                                                          const GLchar *name);

// GL_CHROMIUM_framebuffer_mixed_samples
ANGLE_EXPORT void GL_APIENTRY MatrixLoadfCHROMIUM(GLenum matrixMode, const GLfloat *matrix);
ANGLE_EXPORT void GL_APIENTRY MatrixLoadIdentityCHROMIUM(GLenum matrixMode);

ANGLE_EXPORT void GL_APIENTRY CoverageModulationCHROMIUM(GLenum components);

// GL_CHROMIUM_path_rendering
ANGLE_EXPORT GLuint GL_APIENTRY GenPathsCHROMIUM(GLsizei chromium);
ANGLE_EXPORT void GL_APIENTRY DeletePathsCHROMIUM(GLuint first, GLsizei range);
ANGLE_EXPORT GLboolean GL_APIENTRY IsPathCHROMIUM(GLuint path);
ANGLE_EXPORT void GL_APIENTRY PathCommandsCHROMIUM(GLuint path,
                                                   GLsizei numCommands,
                                                   const GLubyte *commands,
                                                   GLsizei numCoords,
                                                   GLenum coordType,
                                                   const void *coords);
ANGLE_EXPORT void GL_APIENTRY PathParameterfCHROMIUM(GLuint path, GLenum pname, GLfloat value);
ANGLE_EXPORT void GL_APIENTRY PathParameteriCHROMIUM(GLuint path, GLenum pname, GLint value);
ANGLE_EXPORT void GL_APIENTRY GetPathParameterfvCHROMIUM(GLuint path, GLenum pname, GLfloat *value);
ANGLE_EXPORT void GL_APIENTRY GetPathParameterivCHROMIUM(GLuint path, GLenum pname, GLint *value);
ANGLE_EXPORT void GL_APIENTRY PathStencilFuncCHROMIUM(GLenum func, GLint ref, GLuint mask);
ANGLE_EXPORT void GL_APIENTRY StencilFillPathCHROMIUM(GLuint path, GLenum fillMode, GLuint mask);
ANGLE_EXPORT void GL_APIENTRY StencilStrokePathCHROMIUM(GLuint path, GLint reference, GLuint mask);
ANGLE_EXPORT void GL_APIENTRY CoverFillPathCHROMIUM(GLuint path, GLenum coverMode);
ANGLE_EXPORT void GL_APIENTRY CoverStrokePathCHROMIUM(GLuint path, GLenum coverMode);
ANGLE_EXPORT void GL_APIENTRY StencilThenCoverFillPathCHROMIUM(GLuint path,
                                                               GLenum fillMode,
                                                               GLuint mask,
                                                               GLenum coverMode);
ANGLE_EXPORT void GL_APIENTRY StencilThenCoverStrokePathCHROMIUM(GLuint path,
                                                                 GLint reference,
                                                                 GLuint mask,
                                                                 GLenum coverMode);
ANGLE_EXPORT void GL_APIENTRY CoverFillPathInstancedCHROMIUM(GLsizei numPaths,
                                                             GLenum pathNameType,
                                                             const void *paths,
                                                             GLuint pathBase,
                                                             GLenum coverMode,
                                                             GLenum transformType,
                                                             const GLfloat *transformValues);
ANGLE_EXPORT void GL_APIENTRY CoverStrokePathInstancedCHROMIUM(GLsizei numPaths,
                                                               GLenum pathNameType,
                                                               const void *paths,
                                                               GLuint pathBase,
                                                               GLenum coverMode,
                                                               GLenum transformType,
                                                               const GLfloat *transformValues);
ANGLE_EXPORT void GL_APIENTRY StencilFillPathInstancedCHROMIUM(GLsizei numPaths,
                                                               GLenum pathNameType,
                                                               const void *paths,
                                                               GLuint pathBAse,
                                                               GLenum fillMode,
                                                               GLuint mask,
                                                               GLenum transformType,
                                                               const GLfloat *transformValues);
ANGLE_EXPORT void GL_APIENTRY StencilStrokePathInstancedCHROMIUM(GLsizei numPaths,
                                                                 GLenum pathNameType,
                                                                 const void *paths,
                                                                 GLuint pathBase,
                                                                 GLint reference,
                                                                 GLuint mask,
                                                                 GLenum transformType,
                                                                 const GLfloat *transformValues);
ANGLE_EXPORT void GL_APIENTRY
StencilThenCoverFillPathInstancedCHROMIUM(GLsizei numPaths,
                                          GLenum pathNameType,
                                          const void *paths,
                                          GLuint pathBase,
                                          GLenum fillMode,
                                          GLuint mask,
                                          GLenum coverMode,
                                          GLenum transformType,
                                          const GLfloat *transformValues);
ANGLE_EXPORT void GL_APIENTRY
StencilThenCoverStrokePathInstancedCHROMIUM(GLsizei numPaths,
                                            GLenum pathNameType,
                                            const void *paths,
                                            GLuint pathBase,
                                            GLint reference,
                                            GLuint mask,
                                            GLenum coverMode,
                                            GLenum transformType,
                                            const GLfloat *transformValues);
ANGLE_EXPORT void GL_APIENTRY BindFragmentInputLocationCHROMIUM(GLuint program,
                                                                GLint location,
                                                                const GLchar *name);
ANGLE_EXPORT void GL_APIENTRY ProgramPathFragmentInputGenCHROMIUM(GLuint program,
                                                                  GLint location,
                                                                  GLenum genMode,
                                                                  GLint components,
                                                                  const GLfloat *coeffs);

// GL_CHROMIUM_copy_texture
ANGLE_EXPORT void GL_APIENTRY CopyTextureCHROMIUM(GLuint sourceId,
                                                  GLint sourceLevel,
                                                  GLenum destTarget,
                                                  GLuint destId,
                                                  GLint destLevel,
                                                  GLint internalFormat,
                                                  GLenum destType,
                                                  GLboolean unpackFlipY,
                                                  GLboolean unpackPremultiplyAlpha,
                                                  GLboolean unpackUnmultiplyAlpha);

ANGLE_EXPORT void GL_APIENTRY CopySubTextureCHROMIUM(GLuint sourceId,
                                                     GLint sourceLevel,
                                                     GLenum destTarget,
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

// GL_CHROMIUM_copy_compressed_texture
ANGLE_EXPORT void GL_APIENTRY CompressedCopyTextureCHROMIUM(GLuint sourceId, GLuint destId);

// GL_ANGLE_request_extension
ANGLE_EXPORT void GL_APIENTRY RequestExtensionANGLE(const GLchar *name);

// GL_ANGLE_robust_client_memory
ANGLE_EXPORT void GL_APIENTRY GetBooleanvRobustANGLE(GLenum pname,
                                                     GLsizei bufSize,
                                                     GLsizei *length,
                                                     GLboolean *data);
ANGLE_EXPORT void GL_APIENTRY GetBufferParameterivRobustANGLE(GLenum target,
                                                              GLenum pname,
                                                              GLsizei bufSize,
                                                              GLsizei *length,
                                                              GLint *params);
ANGLE_EXPORT void GL_APIENTRY GetFloatvRobustANGLE(GLenum pname,
                                                   GLsizei bufSize,
                                                   GLsizei *length,
                                                   GLfloat *data);
ANGLE_EXPORT void GL_APIENTRY GetFramebufferAttachmentParameterivRobustANGLE(GLenum target,
                                                                             GLenum attachment,
                                                                             GLenum pname,
                                                                             GLsizei bufSize,
                                                                             GLsizei *length,
                                                                             GLint *params);
ANGLE_EXPORT void GL_APIENTRY GetIntegervRobustANGLE(GLenum pname,
                                                     GLsizei bufSize,
                                                     GLsizei *length,
                                                     GLint *data);
ANGLE_EXPORT void GL_APIENTRY GetProgramivRobustANGLE(GLuint program,
                                                      GLenum pname,
                                                      GLsizei bufSize,
                                                      GLsizei *length,
                                                      GLint *params);
ANGLE_EXPORT void GL_APIENTRY GetRenderbufferParameterivRobustANGLE(GLenum target,
                                                                    GLenum pname,
                                                                    GLsizei bufSize,
                                                                    GLsizei *length,
                                                                    GLint *params);
ANGLE_EXPORT void GL_APIENTRY GetShaderivRobustANGLE(GLuint shader,
                                                     GLenum pname,
                                                     GLsizei bufSize,
                                                     GLsizei *length,
                                                     GLint *params);
ANGLE_EXPORT void GL_APIENTRY GetTexParameterfvRobustANGLE(GLenum target,
                                                           GLenum pname,
                                                           GLsizei bufSize,
                                                           GLsizei *length,
                                                           GLfloat *params);
ANGLE_EXPORT void GL_APIENTRY GetTexParameterivRobustANGLE(GLenum target,
                                                           GLenum pname,
                                                           GLsizei bufSize,
                                                           GLsizei *length,
                                                           GLint *params);
ANGLE_EXPORT void GL_APIENTRY GetUniformfvRobustANGLE(GLuint program,
                                                      GLint location,
                                                      GLsizei bufSize,
                                                      GLsizei *length,
                                                      GLfloat *params);
ANGLE_EXPORT void GL_APIENTRY GetUniformivRobustANGLE(GLuint program,
                                                      GLint location,
                                                      GLsizei bufSize,
                                                      GLsizei *length,
                                                      GLint *params);
ANGLE_EXPORT void GL_APIENTRY GetVertexAttribfvRobustANGLE(GLuint index,
                                                           GLenum pname,
                                                           GLsizei bufSize,
                                                           GLsizei *length,
                                                           GLfloat *params);
ANGLE_EXPORT void GL_APIENTRY GetVertexAttribivRobustANGLE(GLuint index,
                                                           GLenum pname,
                                                           GLsizei bufSize,
                                                           GLsizei *length,
                                                           GLint *params);
ANGLE_EXPORT void GL_APIENTRY GetVertexAttribPointervRobustANGLE(GLuint index,
                                                                 GLenum pname,
                                                                 GLsizei bufSize,
                                                                 GLsizei *length,
                                                                 void **pointer);
ANGLE_EXPORT void GL_APIENTRY ReadPixelsRobustANGLE(GLint x,
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
ANGLE_EXPORT void GL_APIENTRY TexImage2DRobustANGLE(GLenum target,
                                                    GLint level,
                                                    GLint internalformat,
                                                    GLsizei width,
                                                    GLsizei height,
                                                    GLint border,
                                                    GLenum format,
                                                    GLenum type,
                                                    GLsizei bufSize,
                                                    const void *pixels);
ANGLE_EXPORT void GL_APIENTRY TexParameterfvRobustANGLE(GLenum target,
                                                        GLenum pname,
                                                        GLsizei bufSize,
                                                        const GLfloat *params);
ANGLE_EXPORT void GL_APIENTRY TexParameterivRobustANGLE(GLenum target,
                                                        GLenum pname,
                                                        GLsizei bufSize,
                                                        const GLint *params);
ANGLE_EXPORT void GL_APIENTRY TexSubImage2DRobustANGLE(GLenum target,
                                                       GLint level,
                                                       GLint xoffset,
                                                       GLint yoffset,
                                                       GLsizei width,
                                                       GLsizei height,
                                                       GLenum format,
                                                       GLenum type,
                                                       GLsizei bufSize,
                                                       const void *pixels);

ANGLE_EXPORT void GL_APIENTRY TexImage3DRobustANGLE(GLenum target,
                                                    GLint level,
                                                    GLint internalformat,
                                                    GLsizei width,
                                                    GLsizei height,
                                                    GLsizei depth,
                                                    GLint border,
                                                    GLenum format,
                                                    GLenum type,
                                                    GLsizei bufSize,
                                                    const void *pixels);
ANGLE_EXPORT void GL_APIENTRY TexSubImage3DRobustANGLE(GLenum target,
                                                       GLint level,
                                                       GLint xoffset,
                                                       GLint yoffset,
                                                       GLint zoffset,
                                                       GLsizei width,
                                                       GLsizei height,
                                                       GLsizei depth,
                                                       GLenum format,
                                                       GLenum type,
                                                       GLsizei bufSize,
                                                       const void *pixels);

ANGLE_EXPORT void GL_APIENTRY CompressedTexImage2DRobustANGLE(GLenum target,
                                                              GLint level,
                                                              GLenum internalformat,
                                                              GLsizei width,
                                                              GLsizei height,
                                                              GLint border,
                                                              GLsizei imageSize,
                                                              GLsizei dataSize,
                                                              const GLvoid *data);
ANGLE_EXPORT void GL_APIENTRY CompressedTexSubImage2DRobustANGLE(GLenum target,
                                                                 GLint level,
                                                                 GLint xoffset,
                                                                 GLint yoffset,
                                                                 GLsizei width,
                                                                 GLsizei height,
                                                                 GLenum format,
                                                                 GLsizei imageSize,
                                                                 GLsizei dataSize,
                                                                 const GLvoid *data);
ANGLE_EXPORT void GL_APIENTRY CompressedTexImage3DRobustANGLE(GLenum target,
                                                              GLint level,
                                                              GLenum internalformat,
                                                              GLsizei width,
                                                              GLsizei height,
                                                              GLsizei depth,
                                                              GLint border,
                                                              GLsizei imageSize,
                                                              GLsizei dataSize,
                                                              const GLvoid *data);
ANGLE_EXPORT void GL_APIENTRY CompressedTexSubImage3DRobustANGLE(GLenum target,
                                                                 GLint level,
                                                                 GLint xoffset,
                                                                 GLint yoffset,
                                                                 GLint zoffset,
                                                                 GLsizei width,
                                                                 GLsizei height,
                                                                 GLsizei depth,
                                                                 GLenum format,
                                                                 GLsizei imageSize,
                                                                 GLsizei dataSize,
                                                                 const GLvoid *data);

ANGLE_EXPORT void GL_APIENTRY
GetQueryivRobustANGLE(GLenum target, GLenum pname, GLsizei bufSize, GLsizei *length, GLint *params);
ANGLE_EXPORT void GL_APIENTRY GetQueryObjectuivRobustANGLE(GLuint id,
                                                           GLenum pname,
                                                           GLsizei bufSize,
                                                           GLsizei *length,
                                                           GLuint *params);
ANGLE_EXPORT void GL_APIENTRY GetBufferPointervRobustANGLE(GLenum target,
                                                           GLenum pname,
                                                           GLsizei bufSize,
                                                           GLsizei *length,
                                                           void **params);
ANGLE_EXPORT void GL_APIENTRY GetIntegeri_vRobustANGLE(GLenum target,
                                                       GLuint index,
                                                       GLsizei bufSize,
                                                       GLsizei *length,
                                                       GLint *data);
ANGLE_EXPORT void GL_APIENTRY GetInternalformativRobustANGLE(GLenum target,
                                                             GLenum internalformat,
                                                             GLenum pname,
                                                             GLsizei bufSize,
                                                             GLsizei *length,
                                                             GLint *params);
ANGLE_EXPORT void GL_APIENTRY GetVertexAttribIivRobustANGLE(GLuint index,
                                                            GLenum pname,
                                                            GLsizei bufSize,
                                                            GLsizei *length,
                                                            GLint *params);
ANGLE_EXPORT void GL_APIENTRY GetVertexAttribIuivRobustANGLE(GLuint index,
                                                             GLenum pname,
                                                             GLsizei bufSize,
                                                             GLsizei *length,
                                                             GLuint *params);
ANGLE_EXPORT void GL_APIENTRY GetUniformuivRobustANGLE(GLuint program,
                                                       GLint location,
                                                       GLsizei bufSize,
                                                       GLsizei *length,
                                                       GLuint *params);
ANGLE_EXPORT void GL_APIENTRY GetActiveUniformBlockivRobustANGLE(GLuint program,
                                                                 GLuint uniformBlockIndex,
                                                                 GLenum pname,
                                                                 GLsizei bufSize,
                                                                 GLsizei *length,
                                                                 GLint *params);
ANGLE_EXPORT void GL_APIENTRY GetInteger64vRobustANGLE(GLenum pname,
                                                       GLsizei bufSize,
                                                       GLsizei *length,
                                                       GLint64 *data);
ANGLE_EXPORT void GL_APIENTRY GetInteger64i_vRobustANGLE(GLenum target,
                                                         GLuint index,
                                                         GLsizei bufSize,
                                                         GLsizei *length,
                                                         GLint64 *data);
ANGLE_EXPORT void GL_APIENTRY GetBufferParameteri64vRobustANGLE(GLenum target,
                                                                GLenum pname,
                                                                GLsizei bufSize,
                                                                GLsizei *length,
                                                                GLint64 *params);
ANGLE_EXPORT void GL_APIENTRY SamplerParameterivRobustANGLE(GLuint sampler,
                                                            GLenum pname,
                                                            GLsizei bufSize,
                                                            const GLint *param);
ANGLE_EXPORT void GL_APIENTRY SamplerParameterfvRobustANGLE(GLuint sampler,
                                                            GLenum pname,
                                                            GLsizei bufSize,
                                                            const GLfloat *param);
ANGLE_EXPORT void GL_APIENTRY GetSamplerParameterivRobustANGLE(GLuint sampler,
                                                               GLenum pname,
                                                               GLsizei bufSize,
                                                               GLsizei *length,
                                                               GLint *params);
ANGLE_EXPORT void GL_APIENTRY GetSamplerParameterfvRobustANGLE(GLuint sampler,
                                                               GLenum pname,
                                                               GLsizei bufSize,
                                                               GLsizei *length,
                                                               GLfloat *params);

ANGLE_EXPORT void GL_APIENTRY GetFramebufferParameterivRobustANGLE(GLenum target,
                                                                   GLenum pname,
                                                                   GLsizei bufSize,
                                                                   GLsizei *length,
                                                                   GLint *params);
ANGLE_EXPORT void GL_APIENTRY GetProgramInterfaceivRobustANGLE(GLuint program,
                                                               GLenum programInterface,
                                                               GLenum pname,
                                                               GLsizei bufSize,
                                                               GLsizei *length,
                                                               GLint *params);
ANGLE_EXPORT void GL_APIENTRY GetBooleani_vRobustANGLE(GLenum target,
                                                       GLuint index,
                                                       GLsizei bufSize,
                                                       GLsizei *length,
                                                       GLboolean *data);
ANGLE_EXPORT void GL_APIENTRY GetMultisamplefvRobustANGLE(GLenum pname,
                                                          GLuint index,
                                                          GLsizei bufSize,
                                                          GLsizei *length,
                                                          GLfloat *val);
ANGLE_EXPORT void GL_APIENTRY GetTexLevelParameterivRobustANGLE(GLenum target,
                                                                GLint level,
                                                                GLenum pname,
                                                                GLsizei bufSize,
                                                                GLsizei *length,
                                                                GLint *params);
ANGLE_EXPORT void GL_APIENTRY GetTexLevelParameterfvRobustANGLE(GLenum target,
                                                                GLint level,
                                                                GLenum pname,
                                                                GLsizei bufSize,
                                                                GLsizei *length,
                                                                GLfloat *params);

ANGLE_EXPORT void GL_APIENTRY GetPointervRobustANGLERobustANGLE(GLenum pname,
                                                                GLsizei bufSize,
                                                                GLsizei *length,
                                                                void **params);
ANGLE_EXPORT void GL_APIENTRY ReadnPixelsRobustANGLE(GLint x,
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
ANGLE_EXPORT void GL_APIENTRY GetnUniformfvRobustANGLE(GLuint program,
                                                       GLint location,
                                                       GLsizei bufSize,
                                                       GLsizei *length,
                                                       GLfloat *params);
ANGLE_EXPORT void GL_APIENTRY GetnUniformivRobustANGLE(GLuint program,
                                                       GLint location,
                                                       GLsizei bufSize,
                                                       GLsizei *length,
                                                       GLint *params);
ANGLE_EXPORT void GL_APIENTRY GetnUniformuivRobustANGLE(GLuint program,
                                                        GLint location,
                                                        GLsizei bufSize,
                                                        GLsizei *length,
                                                        GLuint *params);
ANGLE_EXPORT void GL_APIENTRY TexParameterIivRobustANGLE(GLenum target,
                                                         GLenum pname,
                                                         GLsizei bufSize,
                                                         const GLint *params);
ANGLE_EXPORT void GL_APIENTRY TexParameterIuivRobustANGLE(GLenum target,
                                                          GLenum pname,
                                                          GLsizei bufSize,
                                                          const GLuint *params);
ANGLE_EXPORT void GL_APIENTRY GetTexParameterIivRobustANGLE(GLenum target,
                                                            GLenum pname,
                                                            GLsizei bufSize,
                                                            GLsizei *length,
                                                            GLint *params);
ANGLE_EXPORT void GL_APIENTRY GetTexParameterIuivRobustANGLE(GLenum target,
                                                             GLenum pname,
                                                             GLsizei bufSize,
                                                             GLsizei *length,
                                                             GLuint *params);
ANGLE_EXPORT void GL_APIENTRY SamplerParameterIivRobustANGLE(GLuint sampler,
                                                             GLenum pname,
                                                             GLsizei bufSize,
                                                             const GLint *param);
ANGLE_EXPORT void GL_APIENTRY SamplerParameterIuivRobustANGLE(GLuint sampler,
                                                              GLenum pname,
                                                              GLsizei bufSize,
                                                              const GLuint *param);
ANGLE_EXPORT void GL_APIENTRY GetSamplerParameterIivRobustANGLE(GLuint sampler,
                                                                GLenum pname,
                                                                GLsizei bufSize,
                                                                GLsizei *length,
                                                                GLint *params);
ANGLE_EXPORT void GL_APIENTRY GetSamplerParameterIuivRobustANGLE(GLuint sampler,
                                                                 GLenum pname,
                                                                 GLsizei bufSize,
                                                                 GLsizei *length,
                                                                 GLuint *params);
ANGLE_EXPORT void GL_APIENTRY GetQueryObjectivRobustANGLE(GLuint id,
                                                          GLenum pname,
                                                          GLsizei bufSize,
                                                          GLsizei *length,
                                                          GLint *params);
ANGLE_EXPORT void GL_APIENTRY GetQueryObjecti64vRobustANGLE(GLuint id,
                                                            GLenum pname,
                                                            GLsizei bufSize,
                                                            GLsizei *length,
                                                            GLint64 *params);
ANGLE_EXPORT void GL_APIENTRY GetQueryObjectui64vRobustANGLE(GLuint id,
                                                             GLenum pname,
                                                             GLsizei bufSize,
                                                             GLsizei *length,
                                                             GLuint64 *params);

// GL_ANGLE_multiview
ANGLE_EXPORT void GL_APIENTRY FramebufferTextureMultiviewLayeredANGLE(GLenum target,
                                                                      GLenum attachment,
                                                                      GLuint texture,
                                                                      GLint level,
                                                                      GLint baseViewIndex,
                                                                      GLsizei numViews);
ANGLE_EXPORT void GL_APIENTRY
FramebufferTextureMultiviewSideBySideANGLE(GLenum target,
                                           GLenum attachment,
                                           GLuint texture,
                                           GLint level,
                                           GLsizei numViews,
                                           const GLint *viewportOffsets);
}  // namespace gl

#endif  // LIBGLESV2_ENTRYPOINTGLES20EXT_H_
