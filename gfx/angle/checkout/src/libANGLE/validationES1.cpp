//
// Copyright (c) 2018 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// validationES1.cpp: Validation functions for OpenGL ES 1.0 entry point parameters

#include "libANGLE/validationES1.h"

#include "common/debug.h"

namespace gl
{

bool ValidateAlphaFunc(Context *context, GLenum func, GLfloat ref)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateAlphaFuncx(Context *context, GLenum func, GLfixed ref)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateClearColorx(Context *context, GLfixed red, GLfixed green, GLfixed blue, GLfixed alpha)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateClearDepthx(Context *context, GLfixed depth)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateClientActiveTexture(Context *context, GLenum texture)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateClipPlanef(Context *context, GLenum p, const GLfloat *eqn)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateClipPlanex(Context *context, GLenum plane, const GLfixed *equation)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateColor4f(Context *context, GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateColor4ub(Context *context, GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateColor4x(Context *context, GLfixed red, GLfixed green, GLfixed blue, GLfixed alpha)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateColorPointer(Context *context,
                          GLint size,
                          GLenum type,
                          GLsizei stride,
                          const void *pointer)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateCullFace(Context *context, GLenum mode)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateDepthRangex(Context *context, GLfixed n, GLfixed f)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateDisableClientState(Context *context, GLenum array)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateEnableClientState(Context *context, GLenum array)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateFogf(Context *context, GLenum pname, GLfloat param)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateFogfv(Context *context, GLenum pname, const GLfloat *params)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateFogx(Context *context, GLenum pname, GLfixed param)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateFogxv(Context *context, GLenum pname, const GLfixed *param)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateFrustumf(Context *context,
                      GLfloat l,
                      GLfloat r,
                      GLfloat b,
                      GLfloat t,
                      GLfloat n,
                      GLfloat f)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateFrustumx(Context *context,
                      GLfixed l,
                      GLfixed r,
                      GLfixed b,
                      GLfixed t,
                      GLfixed n,
                      GLfixed f)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateGetBufferParameteriv(Context *context, GLenum target, GLenum pname, GLint *params)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateGetClipPlanef(Context *context, GLenum plane, GLfloat *equation)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateGetClipPlanex(Context *context, GLenum plane, GLfixed *equation)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateGetFixedv(Context *context, GLenum pname, GLfixed *params)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateGetLightfv(Context *context, GLenum light, GLenum pname, GLfloat *params)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateGetLightxv(Context *context, GLenum light, GLenum pname, GLfixed *params)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateGetMaterialfv(Context *context, GLenum face, GLenum pname, GLfloat *params)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateGetMaterialxv(Context *context, GLenum face, GLenum pname, GLfixed *params)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateGetPointerv(Context *context, GLenum pname, void **params)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateGetTexEnvfv(Context *context, GLenum target, GLenum pname, GLfloat *params)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateGetTexEnviv(Context *context, GLenum target, GLenum pname, GLint *params)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateGetTexEnvxv(Context *context, GLenum target, GLenum pname, GLfixed *params)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateGetTexParameterxv(Context *context, GLenum target, GLenum pname, GLfixed *params)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateLightModelf(Context *context, GLenum pname, GLfloat param)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateLightModelfv(Context *context, GLenum pname, const GLfloat *params)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateLightModelx(Context *context, GLenum pname, GLfixed param)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateLightModelxv(Context *context, GLenum pname, const GLfixed *param)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateLightf(Context *context, GLenum light, GLenum pname, GLfloat param)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateLightfv(Context *context, GLenum light, GLenum pname, const GLfloat *params)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateLightx(Context *context, GLenum light, GLenum pname, GLfixed param)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateLightxv(Context *context, GLenum light, GLenum pname, const GLfixed *params)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateLineWidthx(Context *context, GLfixed width)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateLoadIdentity(Context *context)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateLoadMatrixf(Context *context, const GLfloat *m)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateLoadMatrixx(Context *context, const GLfixed *m)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateLogicOp(Context *context, GLenum opcode)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateMaterialf(Context *context, GLenum face, GLenum pname, GLfloat param)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateMaterialfv(Context *context, GLenum face, GLenum pname, const GLfloat *params)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateMaterialx(Context *context, GLenum face, GLenum pname, GLfixed param)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateMaterialxv(Context *context, GLenum face, GLenum pname, const GLfixed *param)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateMatrixMode(Context *context, GLenum mode)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateMultMatrixf(Context *context, const GLfloat *m)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateMultMatrixx(Context *context, const GLfixed *m)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateMultiTexCoord4f(Context *context,
                             GLenum target,
                             GLfloat s,
                             GLfloat t,
                             GLfloat r,
                             GLfloat q)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateMultiTexCoord4x(Context *context,
                             GLenum texture,
                             GLfixed s,
                             GLfixed t,
                             GLfixed r,
                             GLfixed q)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateNormal3f(Context *context, GLfloat nx, GLfloat ny, GLfloat nz)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateNormal3x(Context *context, GLfixed nx, GLfixed ny, GLfixed nz)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateNormalPointer(Context *context, GLenum type, GLsizei stride, const void *pointer)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateOrthof(Context *context,
                    GLfloat l,
                    GLfloat r,
                    GLfloat b,
                    GLfloat t,
                    GLfloat n,
                    GLfloat f)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateOrthox(Context *context,
                    GLfixed l,
                    GLfixed r,
                    GLfixed b,
                    GLfixed t,
                    GLfixed n,
                    GLfixed f)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidatePointParameterf(Context *context, GLenum pname, GLfloat param)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidatePointParameterfv(Context *context, GLenum pname, const GLfloat *params)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidatePointParameterx(Context *context, GLenum pname, GLfixed param)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidatePointParameterxv(Context *context, GLenum pname, const GLfixed *params)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidatePointSize(Context *context, GLfloat size)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidatePointSizex(Context *context, GLfixed size)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidatePolygonOffsetx(Context *context, GLfixed factor, GLfixed units)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidatePopMatrix(Context *context)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidatePushMatrix(Context *context)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateRotatef(Context *context, GLfloat angle, GLfloat x, GLfloat y, GLfloat z)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateRotatex(Context *context, GLfixed angle, GLfixed x, GLfixed y, GLfixed z)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateSampleCoveragex(Context *context, GLclampx value, GLboolean invert)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateScalef(Context *context, GLfloat x, GLfloat y, GLfloat z)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateScalex(Context *context, GLfixed x, GLfixed y, GLfixed z)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateShadeModel(Context *context, GLenum mode)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateTexCoordPointer(Context *context,
                             GLint size,
                             GLenum type,
                             GLsizei stride,
                             const void *pointer)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateTexEnvf(Context *context, GLenum target, GLenum pname, GLfloat param)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateTexEnvfv(Context *context, GLenum target, GLenum pname, const GLfloat *params)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateTexEnvi(Context *context, GLenum target, GLenum pname, GLint param)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateTexEnviv(Context *context, GLenum target, GLenum pname, const GLint *params)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateTexEnvx(Context *context, GLenum target, GLenum pname, GLfixed param)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateTexEnvxv(Context *context, GLenum target, GLenum pname, const GLfixed *params)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateTexParameterx(Context *context, GLenum target, GLenum pname, GLfixed param)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateTexParameterxv(Context *context, GLenum target, GLenum pname, const GLfixed *params)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateTranslatef(Context *context, GLfloat x, GLfloat y, GLfloat z)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateTranslatex(Context *context, GLfixed x, GLfixed y, GLfixed z)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateVertexPointer(Context *context,
                           GLint size,
                           GLenum type,
                           GLsizei stride,
                           const void *pointer)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateDrawTexfOES(Context *context,
                         GLfloat x,
                         GLfloat y,
                         GLfloat z,
                         GLfloat width,
                         GLfloat height)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateDrawTexfvOES(Context *context, const GLfloat *coords)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateDrawTexiOES(Context *context, GLint x, GLint y, GLint z, GLint width, GLint height)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateDrawTexivOES(Context *context, const GLint *coords)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateDrawTexsOES(Context *context,
                         GLshort x,
                         GLshort y,
                         GLshort z,
                         GLshort width,
                         GLshort height)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateDrawTexsvOES(Context *context, const GLshort *coords)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateDrawTexxOES(Context *context,
                         GLfixed x,
                         GLfixed y,
                         GLfixed z,
                         GLfixed width,
                         GLfixed height)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateDrawTexxvOES(Context *context, const GLfixed *coords)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateCurrentPaletteMatrixOES(Context *context, GLuint matrixpaletteindex)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateLoadPaletteFromModelViewMatrixOES(Context *context)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateMatrixIndexPointerOES(Context *context,
                                   GLint size,
                                   GLenum type,
                                   GLsizei stride,
                                   const void *pointer)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateWeightPointerOES(Context *context,
                              GLint size,
                              GLenum type,
                              GLsizei stride,
                              const void *pointer)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidatePointSizePointerOES(Context *context, GLenum type, GLsizei stride, const void *pointer)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateQueryMatrixxOES(Context *context, GLfixed *mantissa, GLint *exponent)
{
    UNIMPLEMENTED();
    return true;
}
}
