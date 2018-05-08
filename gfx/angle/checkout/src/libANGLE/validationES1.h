//
// Copyright (c) 2018 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// validationES1.h: Validation functions for OpenGL ES 1.0 entry point parameters

#ifndef LIBANGLE_VALIDATION_ES1_H_
#define LIBANGLE_VALIDATION_ES1_H_

#include <GLES/gl.h>
#include <GLES/glext.h>

#include "libANGLE/validationES2.h"

namespace gl
{
class Context;

bool ValidateAlphaFunc(Context *context, AlphaTestFunc func, GLfloat ref);
bool ValidateAlphaFuncx(Context *context, AlphaTestFunc func, GLfixed ref);
bool ValidateClearColorx(Context *context, GLfixed red, GLfixed green, GLfixed blue, GLfixed alpha);
bool ValidateClearDepthx(Context *context, GLfixed depth);
bool ValidateClientActiveTexture(Context *context, GLenum texture);
bool ValidateClipPlanef(Context *context, GLenum p, const GLfloat *eqn);
bool ValidateClipPlanex(Context *context, GLenum plane, const GLfixed *equation);
bool ValidateColor4f(Context *context, GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
bool ValidateColor4ub(Context *context, GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha);
bool ValidateColor4x(Context *context, GLfixed red, GLfixed green, GLfixed blue, GLfixed alpha);
bool ValidateColorPointer(Context *context,
                          GLint size,
                          GLenum type,
                          GLsizei stride,
                          const void *pointer);
bool ValidateCullFace(Context *context, GLenum mode);
bool ValidateDepthRangex(Context *context, GLfixed n, GLfixed f);
bool ValidateDisableClientState(Context *context, GLenum array);
bool ValidateEnableClientState(Context *context, GLenum array);
bool ValidateFogf(Context *context, GLenum pname, GLfloat param);
bool ValidateFogfv(Context *context, GLenum pname, const GLfloat *params);
bool ValidateFogx(Context *context, GLenum pname, GLfixed param);
bool ValidateFogxv(Context *context, GLenum pname, const GLfixed *param);
bool ValidateFrustumf(Context *context,
                      GLfloat l,
                      GLfloat r,
                      GLfloat b,
                      GLfloat t,
                      GLfloat n,
                      GLfloat f);
bool ValidateFrustumx(Context *context,
                      GLfixed l,
                      GLfixed r,
                      GLfixed b,
                      GLfixed t,
                      GLfixed n,
                      GLfixed f);
bool ValidateGetBufferParameteriv(Context *context, GLenum target, GLenum pname, GLint *params);
bool ValidateGetClipPlanef(Context *context, GLenum plane, GLfloat *equation);
bool ValidateGetClipPlanex(Context *context, GLenum plane, GLfixed *equation);
bool ValidateGetFixedv(Context *context, GLenum pname, GLfixed *params);
bool ValidateGetLightfv(Context *context, GLenum light, GLenum pname, GLfloat *params);
bool ValidateGetLightxv(Context *context, GLenum light, GLenum pname, GLfixed *params);
bool ValidateGetMaterialfv(Context *context, GLenum face, GLenum pname, GLfloat *params);
bool ValidateGetMaterialxv(Context *context, GLenum face, GLenum pname, GLfixed *params);
bool ValidateGetPointerv(Context *context, GLenum pname, void **params);
bool ValidateGetTexEnvfv(Context *context, GLenum target, GLenum pname, GLfloat *params);
bool ValidateGetTexEnviv(Context *context, GLenum target, GLenum pname, GLint *params);
bool ValidateGetTexEnvxv(Context *context, GLenum target, GLenum pname, GLfixed *params);
bool ValidateGetTexParameterxv(Context *context, TextureType target, GLenum pname, GLfixed *params);
bool ValidateLightModelf(Context *context, GLenum pname, GLfloat param);
bool ValidateLightModelfv(Context *context, GLenum pname, const GLfloat *params);
bool ValidateLightModelx(Context *context, GLenum pname, GLfixed param);
bool ValidateLightModelxv(Context *context, GLenum pname, const GLfixed *param);
bool ValidateLightf(Context *context, GLenum light, GLenum pname, GLfloat param);
bool ValidateLightfv(Context *context, GLenum light, GLenum pname, const GLfloat *params);
bool ValidateLightx(Context *context, GLenum light, GLenum pname, GLfixed param);
bool ValidateLightxv(Context *context, GLenum light, GLenum pname, const GLfixed *params);
bool ValidateLineWidthx(Context *context, GLfixed width);
bool ValidateLoadIdentity(Context *context);
bool ValidateLoadMatrixf(Context *context, const GLfloat *m);
bool ValidateLoadMatrixx(Context *context, const GLfixed *m);
bool ValidateLogicOp(Context *context, GLenum opcode);
bool ValidateMaterialf(Context *context, GLenum face, GLenum pname, GLfloat param);
bool ValidateMaterialfv(Context *context, GLenum face, GLenum pname, const GLfloat *params);
bool ValidateMaterialx(Context *context, GLenum face, GLenum pname, GLfixed param);
bool ValidateMaterialxv(Context *context, GLenum face, GLenum pname, const GLfixed *param);
bool ValidateMatrixMode(Context *context, MatrixType mode);
bool ValidateMultMatrixf(Context *context, const GLfloat *m);
bool ValidateMultMatrixx(Context *context, const GLfixed *m);
bool ValidateMultiTexCoord4f(Context *context,
                             GLenum target,
                             GLfloat s,
                             GLfloat t,
                             GLfloat r,
                             GLfloat q);
bool ValidateMultiTexCoord4x(Context *context,
                             GLenum texture,
                             GLfixed s,
                             GLfixed t,
                             GLfixed r,
                             GLfixed q);
bool ValidateNormal3f(Context *context, GLfloat nx, GLfloat ny, GLfloat nz);
bool ValidateNormal3x(Context *context, GLfixed nx, GLfixed ny, GLfixed nz);
bool ValidateNormalPointer(Context *context, GLenum type, GLsizei stride, const void *pointer);
bool ValidateOrthof(Context *context,
                    GLfloat l,
                    GLfloat r,
                    GLfloat b,
                    GLfloat t,
                    GLfloat n,
                    GLfloat f);
bool ValidateOrthox(Context *context,
                    GLfixed l,
                    GLfixed r,
                    GLfixed b,
                    GLfixed t,
                    GLfixed n,
                    GLfixed f);
bool ValidatePointParameterf(Context *context, GLenum pname, GLfloat param);
bool ValidatePointParameterfv(Context *context, GLenum pname, const GLfloat *params);
bool ValidatePointParameterx(Context *context, GLenum pname, GLfixed param);
bool ValidatePointParameterxv(Context *context, GLenum pname, const GLfixed *params);
bool ValidatePointSize(Context *context, GLfloat size);
bool ValidatePointSizex(Context *context, GLfixed size);
bool ValidatePolygonOffsetx(Context *context, GLfixed factor, GLfixed units);
bool ValidatePopMatrix(Context *context);
bool ValidatePushMatrix(Context *context);
bool ValidateRotatef(Context *context, GLfloat angle, GLfloat x, GLfloat y, GLfloat z);
bool ValidateRotatex(Context *context, GLfixed angle, GLfixed x, GLfixed y, GLfixed z);
bool ValidateSampleCoveragex(Context *context, GLclampx value, GLboolean invert);
bool ValidateScalef(Context *context, GLfloat x, GLfloat y, GLfloat z);
bool ValidateScalex(Context *context, GLfixed x, GLfixed y, GLfixed z);
bool ValidateShadeModel(Context *context, GLenum mode);
bool ValidateTexCoordPointer(Context *context,
                             GLint size,
                             GLenum type,
                             GLsizei stride,
                             const void *pointer);
bool ValidateTexEnvf(Context *context, GLenum target, GLenum pname, GLfloat param);
bool ValidateTexEnvfv(Context *context, GLenum target, GLenum pname, const GLfloat *params);
bool ValidateTexEnvi(Context *context, GLenum target, GLenum pname, GLint param);
bool ValidateTexEnviv(Context *context, GLenum target, GLenum pname, const GLint *params);
bool ValidateTexEnvx(Context *context, GLenum target, GLenum pname, GLfixed param);
bool ValidateTexEnvxv(Context *context, GLenum target, GLenum pname, const GLfixed *params);
bool ValidateTexParameterx(Context *context, TextureType target, GLenum pname, GLfixed param);
bool ValidateTexParameterxv(Context *context,
                            TextureType target,
                            GLenum pname,
                            const GLfixed *params);
bool ValidateTranslatef(Context *context, GLfloat x, GLfloat y, GLfloat z);
bool ValidateTranslatex(Context *context, GLfixed x, GLfixed y, GLfixed z);
bool ValidateVertexPointer(Context *context,
                           GLint size,
                           GLenum type,
                           GLsizei stride,
                           const void *pointer);

// GL_OES_draw_texture
bool ValidateDrawTexfOES(Context *context,
                         GLfloat x,
                         GLfloat y,
                         GLfloat z,
                         GLfloat width,
                         GLfloat height);
bool ValidateDrawTexfvOES(Context *context, const GLfloat *coords);
bool ValidateDrawTexiOES(Context *context, GLint x, GLint y, GLint z, GLint width, GLint height);
bool ValidateDrawTexivOES(Context *context, const GLint *coords);
bool ValidateDrawTexsOES(Context *context,
                         GLshort x,
                         GLshort y,
                         GLshort z,
                         GLshort width,
                         GLshort height);
bool ValidateDrawTexsvOES(Context *context, const GLshort *coords);
bool ValidateDrawTexxOES(Context *context,
                         GLfixed x,
                         GLfixed y,
                         GLfixed z,
                         GLfixed width,
                         GLfixed height);
bool ValidateDrawTexxvOES(Context *context, const GLfixed *coords);

// GL_OES_matrix_palette
bool ValidateCurrentPaletteMatrixOES(Context *context, GLuint matrixpaletteindex);
bool ValidateLoadPaletteFromModelViewMatrixOES(Context *context);
bool ValidateMatrixIndexPointerOES(Context *context,
                                   GLint size,
                                   GLenum type,
                                   GLsizei stride,
                                   const void *pointer);
bool ValidateWeightPointerOES(Context *context,
                              GLint size,
                              GLenum type,
                              GLsizei stride,
                              const void *pointer);

// GL_OES_point_size_array
bool ValidatePointSizePointerOES(Context *context,
                                 GLenum type,
                                 GLsizei stride,
                                 const void *pointer);

// GL_OES_query_matrix
bool ValidateQueryMatrixxOES(Context *context, GLfixed *mantissa, GLint *exponent);

// GL_OES_framebuffer_object
bool ValidateGenFramebuffersOES(Context *context, GLsizei n, GLuint *framebuffers);
bool ValidateDeleteFramebuffersOES(Context *context, GLsizei n, const GLuint *framebuffers);

bool ValidateGenRenderbuffersOES(Context *context, GLsizei n, GLuint *renderbuffers);
bool ValidateDeleteRenderbuffersOES(Context *context, GLsizei n, const GLuint *renderbuffers);

bool ValidateBindFramebufferOES(Context *context, GLenum target, GLuint framebuffer);
bool ValidateBindRenderbufferOES(Context *context, GLenum target, GLuint renderbuffer);
bool ValidateCheckFramebufferStatusOES(Context *context, GLenum target);
bool ValidateFramebufferRenderbufferOES(Context *context,
                                        GLenum target,
                                        GLenum attachment,
                                        GLenum rbtarget,
                                        GLuint renderbuffer);
bool ValidateFramebufferTexture2DOES(Context *context,
                                     GLenum target,
                                     GLenum attachment,
                                     TextureTarget textarget,
                                     GLuint texture,
                                     GLint level);

bool ValidateGenerateMipmapOES(Context *context, TextureType target);

bool ValidateGetFramebufferAttachmentParameterivOES(Context *context,
                                                    GLenum target,
                                                    GLenum attachment,
                                                    GLenum pname,
                                                    GLint *params);

bool ValidateGetRenderbufferParameterivOES(Context *context,
                                           GLenum target,
                                           GLenum pname,
                                           GLint *params);

bool ValidateIsFramebufferOES(Context *context, GLuint framebuffer);
bool ValidateIsRenderbufferOES(Context *context, GLuint renderbuffer);

bool ValidateRenderbufferStorageOES(Context *context,
                                    GLenum target,
                                    GLint internalformat,
                                    GLsizei width,
                                    GLsizei height);

// GL_OES_texture_cube_map
bool ValidateGetTexGenfvOES(Context *context, GLenum coord, GLenum pname, GLfloat *params);
bool ValidateGetTexGenivOES(Context *context, GLenum coord, GLenum pname, int *params);
bool ValidateGetTexGenxvOES(Context *context, GLenum coord, GLenum pname, GLfixed *params);

bool ValidateTexGenfvOES(Context *context, GLenum coord, GLenum pname, const GLfloat *params);
bool ValidateTexGenivOES(Context *context, GLenum coord, GLenum pname, const GLint *params);
bool ValidateTexGenxvOES(Context *context, GLenum coord, GLenum pname, const GLfixed *params);

bool ValidateTexGenfOES(Context *context, GLenum coord, GLenum pname, GLfloat param);
bool ValidateTexGeniOES(Context *context, GLenum coord, GLenum pname, GLint param);
bool ValidateTexGenxOES(Context *context, GLenum coord, GLenum pname, GLfixed param);

}  // namespace gl

#endif  // LIBANGLE_VALIDATION_ES1_H_
