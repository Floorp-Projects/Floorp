//
// Copyright (c) 2018 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// validationES1.cpp: Validation functions for OpenGL ES 1.0 entry point parameters

#include "libANGLE/validationES1.h"

#include "common/debug.h"
#include "libANGLE/Context.h"
#include "libANGLE/ErrorStrings.h"
#include "libANGLE/validationES.h"

#define ANGLE_VALIDATE_IS_GLES1(context)                              \
    if (context->getClientMajorVersion() > 1)                         \
    {                                                                 \
        ANGLE_VALIDATION_ERR(context, InvalidOperation(), GLES1Only); \
        return false;                                                 \
    }

namespace gl
{

bool ValidateAlphaFuncCommon(Context *context, AlphaTestFunc func)
{
    switch (func)
    {
        case AlphaTestFunc::AlwaysPass:
        case AlphaTestFunc::Equal:
        case AlphaTestFunc::Gequal:
        case AlphaTestFunc::Greater:
        case AlphaTestFunc::Lequal:
        case AlphaTestFunc::Less:
        case AlphaTestFunc::Never:
        case AlphaTestFunc::NotEqual:
            return true;
        default:
            ANGLE_VALIDATION_ERR(context, InvalidEnum(), EnumNotSupported);
            return false;
    }
}

bool ValidateClientStateCommon(Context *context, ClientVertexArrayType arrayType)
{
    ANGLE_VALIDATE_IS_GLES1(context);
    switch (arrayType)
    {
        case ClientVertexArrayType::Vertex:
        case ClientVertexArrayType::Normal:
        case ClientVertexArrayType::Color:
        case ClientVertexArrayType::TextureCoord:
            return true;
        case ClientVertexArrayType::PointSize:
            if (!context->getExtensions().pointSizeArray)
            {
                ANGLE_VALIDATION_ERR(context, InvalidEnum(), PointSizeArrayExtensionNotEnabled);
                return false;
            }
            return true;
        default:
            ANGLE_VALIDATION_ERR(context, InvalidEnum(), InvalidClientState);
            return false;
    }
}

bool ValidateBuiltinVertexAttributeCommon(Context *context,
                                          ClientVertexArrayType arrayType,
                                          GLint size,
                                          GLenum type,
                                          GLsizei stride,
                                          const void *pointer)
{
    ANGLE_VALIDATE_IS_GLES1(context);

    if (stride < 0)
    {
        ANGLE_VALIDATION_ERR(context, InvalidValue(), InvalidVertexPointerStride);
        return false;
    }

    int minSize = 1;
    int maxSize = 4;

    switch (arrayType)
    {
        case ClientVertexArrayType::Vertex:
        case ClientVertexArrayType::TextureCoord:
            minSize = 2;
            maxSize = 4;
            break;
        case ClientVertexArrayType::Normal:
            minSize = 3;
            maxSize = 3;
            break;
        case ClientVertexArrayType::Color:
            minSize = 4;
            maxSize = 4;
            break;
        case ClientVertexArrayType::PointSize:
            if (!context->getExtensions().pointSizeArray)
            {
                ANGLE_VALIDATION_ERR(context, InvalidEnum(), PointSizeArrayExtensionNotEnabled);
                return false;
            }

            minSize = 1;
            maxSize = 1;
            break;
        default:
            UNREACHABLE();
            return false;
    }

    if (size < minSize || size > maxSize)
    {
        ANGLE_VALIDATION_ERR(context, InvalidValue(), InvalidVertexPointerSize);
        return false;
    }

    switch (type)
    {
        case GL_BYTE:
            if (arrayType == ClientVertexArrayType::PointSize)
            {
                ANGLE_VALIDATION_ERR(context, InvalidEnum(), InvalidVertexPointerType);
                return false;
            }
            break;
        case GL_SHORT:
            if (arrayType == ClientVertexArrayType::PointSize ||
                arrayType == ClientVertexArrayType::Color)
            {
                ANGLE_VALIDATION_ERR(context, InvalidEnum(), InvalidVertexPointerType);
                return false;
            }
            break;
        case GL_FIXED:
        case GL_FLOAT:
            break;
        default:
            ANGLE_VALIDATION_ERR(context, InvalidEnum(), InvalidVertexPointerType);
            return false;
    }

    return true;
}

}  // namespace gl

namespace gl
{

bool ValidateAlphaFunc(Context *context, AlphaTestFunc func, GLfloat ref)
{
    ANGLE_VALIDATE_IS_GLES1(context);
    return ValidateAlphaFuncCommon(context, func);
}

bool ValidateAlphaFuncx(Context *context, AlphaTestFunc func, GLfixed ref)
{
    ANGLE_VALIDATE_IS_GLES1(context);
    return ValidateAlphaFuncCommon(context, func);
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
    ANGLE_VALIDATE_IS_GLES1(context);
    return ValidateMultitextureUnit(context, texture);
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
    ANGLE_VALIDATE_IS_GLES1(context);
    return true;
}

bool ValidateColor4ub(Context *context, GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha)
{
    ANGLE_VALIDATE_IS_GLES1(context);
    return true;
}

bool ValidateColor4x(Context *context, GLfixed red, GLfixed green, GLfixed blue, GLfixed alpha)
{
    ANGLE_VALIDATE_IS_GLES1(context);
    return true;
}

bool ValidateColorPointer(Context *context,
                          GLint size,
                          GLenum type,
                          GLsizei stride,
                          const void *pointer)
{
    return ValidateBuiltinVertexAttributeCommon(context, ClientVertexArrayType::Color, size, type,
                                                stride, pointer);
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

bool ValidateDisableClientState(Context *context, ClientVertexArrayType arrayType)
{
    return ValidateClientStateCommon(context, arrayType);
}

bool ValidateEnableClientState(Context *context, ClientVertexArrayType arrayType)
{
    return ValidateClientStateCommon(context, arrayType);
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
    ANGLE_VALIDATE_IS_GLES1(context);
    if (l == r || b == t || n == f || n <= 0.0f || f <= 0.0f)
    {
        ANGLE_VALIDATION_ERR(context, InvalidValue(), InvalidProjectionMatrix);
    }
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
    ANGLE_VALIDATE_IS_GLES1(context);
    if (l == r || b == t || n == f || n <= 0 || f <= 0)
    {
        ANGLE_VALIDATION_ERR(context, InvalidValue(), InvalidProjectionMatrix);
    }
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
    ANGLE_VALIDATE_IS_GLES1(context);
    switch (pname)
    {
        case GL_VERTEX_ARRAY_POINTER:
        case GL_NORMAL_ARRAY_POINTER:
        case GL_COLOR_ARRAY_POINTER:
        case GL_TEXTURE_COORD_ARRAY_POINTER:
        case GL_POINT_SIZE_ARRAY_POINTER_OES:
            return true;
        default:
            ANGLE_VALIDATION_ERR(context, InvalidEnum(), InvalidPointerQuery);
            return false;
    }
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

bool ValidateGetTexParameterxv(Context *context, TextureType target, GLenum pname, GLfixed *params)
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
    ANGLE_VALIDATE_IS_GLES1(context);
    return true;
}

bool ValidateLoadMatrixf(Context *context, const GLfloat *m)
{
    ANGLE_VALIDATE_IS_GLES1(context);
    return true;
}

bool ValidateLoadMatrixx(Context *context, const GLfixed *m)
{
    ANGLE_VALIDATE_IS_GLES1(context);
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

bool ValidateMatrixMode(Context *context, MatrixType mode)
{
    ANGLE_VALIDATE_IS_GLES1(context);
    switch (mode)
    {
        case MatrixType::Projection:
        case MatrixType::Modelview:
        case MatrixType::Texture:
            return true;
        default:
            ANGLE_VALIDATION_ERR(context, InvalidEnum(), InvalidMatrixMode);
            return false;
    }
}

bool ValidateMultMatrixf(Context *context, const GLfloat *m)
{
    ANGLE_VALIDATE_IS_GLES1(context);
    return true;
}

bool ValidateMultMatrixx(Context *context, const GLfixed *m)
{
    ANGLE_VALIDATE_IS_GLES1(context);
    return true;
}

bool ValidateMultiTexCoord4f(Context *context,
                             GLenum target,
                             GLfloat s,
                             GLfloat t,
                             GLfloat r,
                             GLfloat q)
{
    ANGLE_VALIDATE_IS_GLES1(context);
    return ValidateMultitextureUnit(context, target);
}

bool ValidateMultiTexCoord4x(Context *context,
                             GLenum target,
                             GLfixed s,
                             GLfixed t,
                             GLfixed r,
                             GLfixed q)
{
    ANGLE_VALIDATE_IS_GLES1(context);
    return ValidateMultitextureUnit(context, target);
}

bool ValidateNormal3f(Context *context, GLfloat nx, GLfloat ny, GLfloat nz)
{
    ANGLE_VALIDATE_IS_GLES1(context);
    return true;
}

bool ValidateNormal3x(Context *context, GLfixed nx, GLfixed ny, GLfixed nz)
{
    ANGLE_VALIDATE_IS_GLES1(context);
    return true;
}

bool ValidateNormalPointer(Context *context, GLenum type, GLsizei stride, const void *pointer)
{
    return ValidateBuiltinVertexAttributeCommon(context, ClientVertexArrayType::Normal, 3, type,
                                                stride, pointer);
}

bool ValidateOrthof(Context *context,
                    GLfloat l,
                    GLfloat r,
                    GLfloat b,
                    GLfloat t,
                    GLfloat n,
                    GLfloat f)
{
    ANGLE_VALIDATE_IS_GLES1(context);
    if (l == r || b == t || n == f || n <= 0.0f || f <= 0.0f)
    {
        ANGLE_VALIDATION_ERR(context, InvalidValue(), InvalidProjectionMatrix);
    }
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
    ANGLE_VALIDATE_IS_GLES1(context);
    if (l == r || b == t || n == f || n <= 0 || f <= 0)
    {
        ANGLE_VALIDATION_ERR(context, InvalidValue(), InvalidProjectionMatrix);
    }
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
    ANGLE_VALIDATE_IS_GLES1(context);
    const auto &stack = context->getGLState().gles1().currentMatrixStack();
    if (stack.size() == 1)
    {
        ANGLE_VALIDATION_ERR(context, StackUnderflow(), MatrixStackUnderflow);
        return false;
    }
    return true;
}

bool ValidatePushMatrix(Context *context)
{
    ANGLE_VALIDATE_IS_GLES1(context);
    const auto &stack = context->getGLState().gles1().currentMatrixStack();
    if (stack.size() == stack.max_size())
    {
        ANGLE_VALIDATION_ERR(context, StackOverflow(), MatrixStackOverflow);
        return false;
    }
    return true;
}

bool ValidateRotatef(Context *context, GLfloat angle, GLfloat x, GLfloat y, GLfloat z)
{
    ANGLE_VALIDATE_IS_GLES1(context);
    return true;
}

bool ValidateRotatex(Context *context, GLfixed angle, GLfixed x, GLfixed y, GLfixed z)
{
    ANGLE_VALIDATE_IS_GLES1(context);
    return true;
}

bool ValidateSampleCoveragex(Context *context, GLclampx value, GLboolean invert)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateScalef(Context *context, GLfloat x, GLfloat y, GLfloat z)
{
    ANGLE_VALIDATE_IS_GLES1(context);
    return true;
}

bool ValidateScalex(Context *context, GLfixed x, GLfixed y, GLfixed z)
{
    ANGLE_VALIDATE_IS_GLES1(context);
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
    return ValidateBuiltinVertexAttributeCommon(context, ClientVertexArrayType::TextureCoord, size,
                                                type, stride, pointer);
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

bool ValidateTexParameterx(Context *context, TextureType target, GLenum pname, GLfixed param)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateTexParameterxv(Context *context,
                            TextureType target,
                            GLenum pname,
                            const GLfixed *params)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateTranslatef(Context *context, GLfloat x, GLfloat y, GLfloat z)
{
    ANGLE_VALIDATE_IS_GLES1(context);
    return true;
}

bool ValidateTranslatex(Context *context, GLfixed x, GLfixed y, GLfixed z)
{
    ANGLE_VALIDATE_IS_GLES1(context);
    return true;
}

bool ValidateVertexPointer(Context *context,
                           GLint size,
                           GLenum type,
                           GLsizei stride,
                           const void *pointer)
{
    return ValidateBuiltinVertexAttributeCommon(context, ClientVertexArrayType::Vertex, size, type,
                                                stride, pointer);
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
    return ValidateBuiltinVertexAttributeCommon(context, ClientVertexArrayType::PointSize, 1, type,
                                                stride, pointer);
}

bool ValidateQueryMatrixxOES(Context *context, GLfixed *mantissa, GLint *exponent)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateGenFramebuffersOES(Context *context, GLsizei n, GLuint *framebuffers)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateDeleteFramebuffersOES(Context *context, GLsizei n, const GLuint *framebuffers)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateGenRenderbuffersOES(Context *context, GLsizei n, GLuint *renderbuffers)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateDeleteRenderbuffersOES(Context *context, GLsizei n, const GLuint *renderbuffers)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateBindFramebufferOES(Context *context, GLenum target, GLuint framebuffer)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateBindRenderbufferOES(Context *context, GLenum target, GLuint renderbuffer)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateCheckFramebufferStatusOES(Context *context, GLenum target)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateFramebufferRenderbufferOES(Context *context,
                                        GLenum target,
                                        GLenum attachment,
                                        GLenum rbtarget,
                                        GLuint renderbuffer)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateFramebufferTexture2DOES(Context *context,
                                     GLenum target,
                                     GLenum attachment,
                                     TextureTarget textarget,
                                     GLuint texture,
                                     GLint level)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateGenerateMipmapOES(Context *context, TextureType target)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateGetFramebufferAttachmentParameterivOES(Context *context,
                                                    GLenum target,
                                                    GLenum attachment,
                                                    GLenum pname,
                                                    GLint *params)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateGetRenderbufferParameterivOES(Context *context,
                                           GLenum target,
                                           GLenum pname,
                                           GLint *params)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateIsFramebufferOES(Context *context, GLuint framebuffer)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateIsRenderbufferOES(Context *context, GLuint renderbuffer)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateRenderbufferStorageOES(Context *context,
                                    GLenum target,
                                    GLint internalformat,
                                    GLsizei width,
                                    GLsizei height)
{
    UNIMPLEMENTED();
    return true;
}

// GL_OES_texture_cube_map

bool ValidateGetTexGenfvOES(Context *context, GLenum coord, GLenum pname, GLfloat *params)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateGetTexGenivOES(Context *context, GLenum coord, GLenum pname, int *params)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateGetTexGenxvOES(Context *context, GLenum coord, GLenum pname, GLfixed *params)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateTexGenfvOES(Context *context, GLenum coord, GLenum pname, const GLfloat *params)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateTexGenivOES(Context *context, GLenum coord, GLenum pname, const GLint *param)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateTexGenxvOES(Context *context, GLenum coord, GLenum pname, const GLint *param)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateTexGenfOES(Context *context, GLenum coord, GLenum pname, GLfloat param)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateTexGeniOES(Context *context, GLenum coord, GLenum pname, GLint param)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateTexGenxOES(Context *context, GLenum coord, GLenum pname, GLfixed param)
{
    UNIMPLEMENTED();
    return true;
}

}  // namespace gl
