//
// Copyright 2018 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Context_gles_1_0.cpp: Implements the GLES1-specific parts of Context.

#include "libANGLE/Context.h"

#include "common/mathutil.h"
#include "common/utilities.h"

#include "libANGLE/GLES1Renderer.h"
#include "libANGLE/queryconversions.h"
#include "libANGLE/queryutils.h"

namespace
{

angle::Mat4 FixedMatrixToMat4(const GLfixed *m)
{
    angle::Mat4 matrixAsFloat;
    GLfloat *floatData = matrixAsFloat.data();

    for (int i = 0; i < 16; i++)
    {
        floatData[i] = gl::FixedToFloat(m[i]);
    }

    return matrixAsFloat;
}

}  // namespace

namespace gl
{

void Context::alphaFunc(AlphaTestFunc func, GLfloat ref)
{
    mGLState.gles1().setAlphaFunc(func, ref);
}

void Context::alphaFuncx(AlphaTestFunc func, GLfixed ref)
{
    mGLState.gles1().setAlphaFunc(func, FixedToFloat(ref));
}

void Context::clearColorx(GLfixed red, GLfixed green, GLfixed blue, GLfixed alpha)
{
    UNIMPLEMENTED();
}

void Context::clearDepthx(GLfixed depth)
{
    UNIMPLEMENTED();
}

void Context::clientActiveTexture(GLenum texture)
{
    mGLState.gles1().setClientTextureUnit(texture - GL_TEXTURE0);
    mStateCache.onGLES1ClientStateChange(this);
}

void Context::clipPlanef(GLenum p, const GLfloat *eqn)
{
    mGLState.gles1().setClipPlane(p - GL_CLIP_PLANE0, eqn);
}

void Context::clipPlanex(GLenum plane, const GLfixed *equation)
{
    const GLfloat equationf[4] = {
        FixedToFloat(equation[0]), FixedToFloat(equation[1]), FixedToFloat(equation[2]),
        FixedToFloat(equation[3]),
    };

    mGLState.gles1().setClipPlane(plane - GL_CLIP_PLANE0, equationf);
}

void Context::color4f(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)
{
    mGLState.gles1().setCurrentColor({red, green, blue, alpha});
}

void Context::color4ub(GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha)
{
    mGLState.gles1().setCurrentColor(
        {normalizedToFloat<uint8_t>(red), normalizedToFloat<uint8_t>(green),
         normalizedToFloat<uint8_t>(blue), normalizedToFloat<uint8_t>(alpha)});
}

void Context::color4x(GLfixed red, GLfixed green, GLfixed blue, GLfixed alpha)
{
    mGLState.gles1().setCurrentColor(
        {FixedToFloat(red), FixedToFloat(green), FixedToFloat(blue), FixedToFloat(alpha)});
}

void Context::colorPointer(GLint size, GLenum type, GLsizei stride, const void *ptr)
{
    vertexAttribPointer(vertexArrayIndex(ClientVertexArrayType::Color), size, type, GL_FALSE,
                        stride, ptr);
}

void Context::depthRangex(GLfixed n, GLfixed f)
{
    UNIMPLEMENTED();
}

void Context::disableClientState(ClientVertexArrayType clientState)
{
    mGLState.gles1().setClientStateEnabled(clientState, false);
    disableVertexAttribArray(vertexArrayIndex(clientState));
    mStateCache.onGLES1ClientStateChange(this);
}

void Context::enableClientState(ClientVertexArrayType clientState)
{
    mGLState.gles1().setClientStateEnabled(clientState, true);
    enableVertexAttribArray(vertexArrayIndex(clientState));
    mStateCache.onGLES1ClientStateChange(this);
}

void Context::fogf(GLenum pname, GLfloat param)
{
    SetFogParameters(&mGLState.gles1(), pname, &param);
}

void Context::fogfv(GLenum pname, const GLfloat *params)
{
    SetFogParameters(&mGLState.gles1(), pname, params);
}

void Context::fogx(GLenum pname, GLfixed param)
{
    if (GetFogParameterCount(pname) == 1)
    {
        GLfloat paramf = pname == GL_FOG_MODE ? ConvertToGLenum(param) : FixedToFloat(param);
        fogf(pname, paramf);
    }
    else
    {
        UNREACHABLE();
    }
}

void Context::fogxv(GLenum pname, const GLfixed *params)
{
    int paramCount = GetFogParameterCount(pname);

    if (paramCount > 0)
    {
        GLfloat paramsf[4];
        for (int i = 0; i < paramCount; i++)
        {
            paramsf[i] =
                pname == GL_FOG_MODE ? ConvertToGLenum(params[i]) : FixedToFloat(params[i]);
        }
        fogfv(pname, paramsf);
    }
    else
    {
        UNREACHABLE();
    }
}

void Context::frustumf(GLfloat l, GLfloat r, GLfloat b, GLfloat t, GLfloat n, GLfloat f)
{
    mGLState.gles1().multMatrix(angle::Mat4::Frustum(l, r, b, t, n, f));
}

void Context::frustumx(GLfixed l, GLfixed r, GLfixed b, GLfixed t, GLfixed n, GLfixed f)
{
    mGLState.gles1().multMatrix(angle::Mat4::Frustum(FixedToFloat(l), FixedToFloat(r),
                                                     FixedToFloat(b), FixedToFloat(t),
                                                     FixedToFloat(n), FixedToFloat(f)));
}

void Context::getClipPlanef(GLenum plane, GLfloat *equation)
{
    mGLState.gles1().getClipPlane(plane - GL_CLIP_PLANE0, equation);
}

void Context::getClipPlanex(GLenum plane, GLfixed *equation)
{
    GLfloat equationf[4] = {};

    mGLState.gles1().getClipPlane(plane - GL_CLIP_PLANE0, equationf);

    for (int i = 0; i < 4; i++)
    {
        equation[i] = FloatToFixed(equationf[i]);
    }
}

void Context::getFixedv(GLenum pname, GLfixed *params)
{
    UNIMPLEMENTED();
}

void Context::getLightfv(GLenum light, LightParameter pname, GLfloat *params)
{
    GetLightParameters(&mGLState.gles1(), light, pname, params);
}

void Context::getLightxv(GLenum light, LightParameter pname, GLfixed *params)
{
    GLfloat paramsf[4];
    getLightfv(light, pname, paramsf);

    for (unsigned int i = 0; i < GetLightParameterCount(pname); i++)
    {
        params[i] = FloatToFixed(paramsf[i]);
    }
}

void Context::getMaterialfv(GLenum face, MaterialParameter pname, GLfloat *params)
{
    GetMaterialParameters(&mGLState.gles1(), face, pname, params);
}

void Context::getMaterialxv(GLenum face, MaterialParameter pname, GLfixed *params)
{
    GLfloat paramsf[4];
    getMaterialfv(face, pname, paramsf);

    for (unsigned int i = 0; i < GetMaterialParameterCount(pname); i++)
    {
        params[i] = FloatToFixed(paramsf[i]);
    }
}

void Context::getTexEnvfv(TextureEnvTarget target, TextureEnvParameter pname, GLfloat *params)
{
    GetTextureEnv(mGLState.getActiveSampler(), &mGLState.gles1(), target, pname, params);
}

void Context::getTexEnviv(TextureEnvTarget target, TextureEnvParameter pname, GLint *params)
{
    GLfloat paramsf[4];
    GetTextureEnv(mGLState.getActiveSampler(), &mGLState.gles1(), target, pname, paramsf);
    ConvertTextureEnvToInt(pname, paramsf, params);
}

void Context::getTexEnvxv(TextureEnvTarget target, TextureEnvParameter pname, GLfixed *params)
{
    GLfloat paramsf[4];
    GetTextureEnv(mGLState.getActiveSampler(), &mGLState.gles1(), target, pname, paramsf);
    ConvertTextureEnvToFixed(pname, paramsf, params);
}

void Context::getTexParameterxv(TextureType target, GLenum pname, GLfixed *params)
{
    UNIMPLEMENTED();
}

void Context::lightModelf(GLenum pname, GLfloat param)
{
    SetLightModelParameters(&mGLState.gles1(), pname, &param);
}

void Context::lightModelfv(GLenum pname, const GLfloat *params)
{
    SetLightModelParameters(&mGLState.gles1(), pname, params);
}

void Context::lightModelx(GLenum pname, GLfixed param)
{
    lightModelf(pname, FixedToFloat(param));
}

void Context::lightModelxv(GLenum pname, const GLfixed *param)
{
    GLfloat paramsf[4];

    for (unsigned int i = 0; i < GetLightModelParameterCount(pname); i++)
    {
        paramsf[i] = FixedToFloat(param[i]);
    }

    lightModelfv(pname, paramsf);
}

void Context::lightf(GLenum light, LightParameter pname, GLfloat param)
{
    SetLightParameters(&mGLState.gles1(), light, pname, &param);
}

void Context::lightfv(GLenum light, LightParameter pname, const GLfloat *params)
{
    SetLightParameters(&mGLState.gles1(), light, pname, params);
}

void Context::lightx(GLenum light, LightParameter pname, GLfixed param)
{
    lightf(light, pname, FixedToFloat(param));
}

void Context::lightxv(GLenum light, LightParameter pname, const GLfixed *params)
{
    GLfloat paramsf[4];

    for (unsigned int i = 0; i < GetLightParameterCount(pname); i++)
    {
        paramsf[i] = FixedToFloat(params[i]);
    }

    lightfv(light, pname, paramsf);
}

void Context::lineWidthx(GLfixed width)
{
    UNIMPLEMENTED();
}

void Context::loadIdentity()
{
    mGLState.gles1().loadMatrix(angle::Mat4());
}

void Context::loadMatrixf(const GLfloat *m)
{
    mGLState.gles1().loadMatrix(angle::Mat4(m));
}

void Context::loadMatrixx(const GLfixed *m)
{
    mGLState.gles1().loadMatrix(FixedMatrixToMat4(m));
}

void Context::logicOp(LogicalOperation opcodePacked)
{
    mGLState.gles1().setLogicOp(opcodePacked);
}

void Context::materialf(GLenum face, MaterialParameter pname, GLfloat param)
{
    SetMaterialParameters(&mGLState.gles1(), face, pname, &param);
}

void Context::materialfv(GLenum face, MaterialParameter pname, const GLfloat *params)
{
    SetMaterialParameters(&mGLState.gles1(), face, pname, params);
}

void Context::materialx(GLenum face, MaterialParameter pname, GLfixed param)
{
    materialf(face, pname, FixedToFloat(param));
}

void Context::materialxv(GLenum face, MaterialParameter pname, const GLfixed *param)
{
    GLfloat paramsf[4];

    for (unsigned int i = 0; i < GetMaterialParameterCount(pname); i++)
    {
        paramsf[i] = FixedToFloat(param[i]);
    }

    materialfv(face, pname, paramsf);
}

void Context::matrixMode(MatrixType mode)
{
    mGLState.gles1().setMatrixMode(mode);
}

void Context::multMatrixf(const GLfloat *m)
{
    mGLState.gles1().multMatrix(angle::Mat4(m));
}

void Context::multMatrixx(const GLfixed *m)
{
    mGLState.gles1().multMatrix(FixedMatrixToMat4(m));
}

void Context::multiTexCoord4f(GLenum target, GLfloat s, GLfloat t, GLfloat r, GLfloat q)
{
    unsigned int unit = target - GL_TEXTURE0;
    ASSERT(target >= GL_TEXTURE0 && unit < getCaps().maxMultitextureUnits);
    mGLState.gles1().setCurrentTextureCoords(unit, {s, t, r, q});
}

void Context::multiTexCoord4x(GLenum target, GLfixed s, GLfixed t, GLfixed r, GLfixed q)
{
    unsigned int unit = target - GL_TEXTURE0;
    ASSERT(target >= GL_TEXTURE0 && unit < getCaps().maxMultitextureUnits);
    mGLState.gles1().setCurrentTextureCoords(
        unit, {FixedToFloat(s), FixedToFloat(t), FixedToFloat(r), FixedToFloat(q)});
}

void Context::normal3f(GLfloat nx, GLfloat ny, GLfloat nz)
{
    mGLState.gles1().setCurrentNormal({nx, ny, nz});
}

void Context::normal3x(GLfixed nx, GLfixed ny, GLfixed nz)
{
    mGLState.gles1().setCurrentNormal({FixedToFloat(nx), FixedToFloat(ny), FixedToFloat(nz)});
}

void Context::normalPointer(GLenum type, GLsizei stride, const void *ptr)
{
    vertexAttribPointer(vertexArrayIndex(ClientVertexArrayType::Normal), 3, type, GL_FALSE, stride,
                        ptr);
}

void Context::orthof(GLfloat left,
                     GLfloat right,
                     GLfloat bottom,
                     GLfloat top,
                     GLfloat zNear,
                     GLfloat zFar)
{
    mGLState.gles1().multMatrix(angle::Mat4::Ortho(left, right, bottom, top, zNear, zFar));
}

void Context::orthox(GLfixed l, GLfixed r, GLfixed b, GLfixed t, GLfixed n, GLfixed f)
{
    mGLState.gles1().multMatrix(angle::Mat4::Ortho(FixedToFloat(l), FixedToFloat(r),
                                                   FixedToFloat(b), FixedToFloat(t),
                                                   FixedToFloat(n), FixedToFloat(f)));
}

void Context::pointParameterf(PointParameter pname, GLfloat param)
{
    SetPointParameter(&mGLState.gles1(), pname, &param);
}

void Context::pointParameterfv(PointParameter pname, const GLfloat *params)
{
    SetPointParameter(&mGLState.gles1(), pname, params);
}

void Context::pointParameterx(PointParameter pname, GLfixed param)
{
    GLfloat paramf = FixedToFloat(param);
    SetPointParameter(&mGLState.gles1(), pname, &paramf);
}

void Context::pointParameterxv(PointParameter pname, const GLfixed *params)
{
    GLfloat paramsf[4] = {};
    for (unsigned int i = 0; i < GetPointParameterCount(pname); i++)
    {
        paramsf[i] = FixedToFloat(params[i]);
    }
    SetPointParameter(&mGLState.gles1(), pname, paramsf);
}

void Context::pointSize(GLfloat size)
{
    SetPointSize(&mGLState.gles1(), size);
}

void Context::pointSizex(GLfixed size)
{
    SetPointSize(&mGLState.gles1(), FixedToFloat(size));
}

void Context::polygonOffsetx(GLfixed factor, GLfixed units)
{
    UNIMPLEMENTED();
}

void Context::popMatrix()
{
    mGLState.gles1().popMatrix();
}

void Context::pushMatrix()
{
    mGLState.gles1().pushMatrix();
}

void Context::rotatef(float angle, float x, float y, float z)
{
    mGLState.gles1().multMatrix(angle::Mat4::Rotate(angle, angle::Vector3(x, y, z)));
}

void Context::rotatex(GLfixed angle, GLfixed x, GLfixed y, GLfixed z)
{
    mGLState.gles1().multMatrix(angle::Mat4::Rotate(
        FixedToFloat(angle), angle::Vector3(FixedToFloat(x), FixedToFloat(y), FixedToFloat(z))));
}

void Context::sampleCoveragex(GLclampx value, GLboolean invert)
{
    UNIMPLEMENTED();
}

void Context::scalef(float x, float y, float z)
{
    mGLState.gles1().multMatrix(angle::Mat4::Scale(angle::Vector3(x, y, z)));
}

void Context::scalex(GLfixed x, GLfixed y, GLfixed z)
{
    mGLState.gles1().multMatrix(
        angle::Mat4::Scale(angle::Vector3(FixedToFloat(x), FixedToFloat(y), FixedToFloat(z))));
}

void Context::shadeModel(ShadingModel model)
{
    mGLState.gles1().setShadeModel(model);
}

void Context::texCoordPointer(GLint size, GLenum type, GLsizei stride, const void *ptr)
{
    vertexAttribPointer(vertexArrayIndex(ClientVertexArrayType::TextureCoord), size, type, GL_FALSE,
                        stride, ptr);
}

void Context::texEnvf(TextureEnvTarget target, TextureEnvParameter pname, GLfloat param)
{
    SetTextureEnv(mGLState.getActiveSampler(), &mGLState.gles1(), target, pname, &param);
}

void Context::texEnvfv(TextureEnvTarget target, TextureEnvParameter pname, const GLfloat *params)
{
    SetTextureEnv(mGLState.getActiveSampler(), &mGLState.gles1(), target, pname, params);
}

void Context::texEnvi(TextureEnvTarget target, TextureEnvParameter pname, GLint param)
{
    GLfloat paramsf[4] = {};
    ConvertTextureEnvFromInt(pname, &param, paramsf);
    SetTextureEnv(mGLState.getActiveSampler(), &mGLState.gles1(), target, pname, paramsf);
}

void Context::texEnviv(TextureEnvTarget target, TextureEnvParameter pname, const GLint *params)
{
    GLfloat paramsf[4] = {};
    ConvertTextureEnvFromInt(pname, params, paramsf);
    SetTextureEnv(mGLState.getActiveSampler(), &mGLState.gles1(), target, pname, paramsf);
}

void Context::texEnvx(TextureEnvTarget target, TextureEnvParameter pname, GLfixed param)
{
    GLfloat paramsf[4] = {};
    ConvertTextureEnvFromFixed(pname, &param, paramsf);
    SetTextureEnv(mGLState.getActiveSampler(), &mGLState.gles1(), target, pname, paramsf);
}

void Context::texEnvxv(TextureEnvTarget target, TextureEnvParameter pname, const GLfixed *params)
{
    GLfloat paramsf[4] = {};
    ConvertTextureEnvFromFixed(pname, params, paramsf);
    SetTextureEnv(mGLState.getActiveSampler(), &mGLState.gles1(), target, pname, paramsf);
}

void Context::texParameterx(TextureType target, GLenum pname, GLfixed param)
{
    UNIMPLEMENTED();
}

void Context::texParameterxv(TextureType target, GLenum pname, const GLfixed *params)
{
    UNIMPLEMENTED();
}

void Context::translatef(float x, float y, float z)
{
    mGLState.gles1().multMatrix(angle::Mat4::Translate(angle::Vector3(x, y, z)));
}

void Context::translatex(GLfixed x, GLfixed y, GLfixed z)
{
    mGLState.gles1().multMatrix(
        angle::Mat4::Translate(angle::Vector3(FixedToFloat(x), FixedToFloat(y), FixedToFloat(z))));
}

void Context::vertexPointer(GLint size, GLenum type, GLsizei stride, const void *ptr)
{
    vertexAttribPointer(vertexArrayIndex(ClientVertexArrayType::Vertex), size, type, GL_FALSE,
                        stride, ptr);
}

// GL_OES_draw_texture
void Context::drawTexf(float x, float y, float z, float width, float height)
{
    mGLES1Renderer->drawTexture(this, &mGLState, x, y, z, width, height);
}

void Context::drawTexfv(const GLfloat *coords)
{
    mGLES1Renderer->drawTexture(this, &mGLState, coords[0], coords[1], coords[2], coords[3],
                                coords[4]);
}

void Context::drawTexi(GLint x, GLint y, GLint z, GLint width, GLint height)
{
    mGLES1Renderer->drawTexture(this, &mGLState, static_cast<GLfloat>(x), static_cast<GLfloat>(y),
                                static_cast<GLfloat>(z), static_cast<GLfloat>(width),
                                static_cast<GLfloat>(height));
}

void Context::drawTexiv(const GLint *coords)
{
    mGLES1Renderer->drawTexture(this, &mGLState, static_cast<GLfloat>(coords[0]),
                                static_cast<GLfloat>(coords[1]), static_cast<GLfloat>(coords[2]),
                                static_cast<GLfloat>(coords[3]), static_cast<GLfloat>(coords[4]));
}

void Context::drawTexs(GLshort x, GLshort y, GLshort z, GLshort width, GLshort height)
{
    mGLES1Renderer->drawTexture(this, &mGLState, static_cast<GLfloat>(x), static_cast<GLfloat>(y),
                                static_cast<GLfloat>(z), static_cast<GLfloat>(width),
                                static_cast<GLfloat>(height));
}

void Context::drawTexsv(const GLshort *coords)
{
    mGLES1Renderer->drawTexture(this, &mGLState, static_cast<GLfloat>(coords[0]),
                                static_cast<GLfloat>(coords[1]), static_cast<GLfloat>(coords[2]),
                                static_cast<GLfloat>(coords[3]), static_cast<GLfloat>(coords[4]));
}

void Context::drawTexx(GLfixed x, GLfixed y, GLfixed z, GLfixed width, GLfixed height)
{
    mGLES1Renderer->drawTexture(this, &mGLState, FixedToFloat(x), FixedToFloat(y), FixedToFloat(z),
                                FixedToFloat(width), FixedToFloat(height));
}

void Context::drawTexxv(const GLfixed *coords)
{
    mGLES1Renderer->drawTexture(this, &mGLState, FixedToFloat(coords[0]), FixedToFloat(coords[1]),
                                FixedToFloat(coords[2]), FixedToFloat(coords[3]),
                                FixedToFloat(coords[4]));
}

// GL_OES_matrix_palette
void Context::currentPaletteMatrix(GLuint matrixpaletteindex)
{
    UNIMPLEMENTED();
}

void Context::loadPaletteFromModelViewMatrix()
{
    UNIMPLEMENTED();
}

void Context::matrixIndexPointer(GLint size, GLenum type, GLsizei stride, const void *pointer)
{
    UNIMPLEMENTED();
}

void Context::weightPointer(GLint size, GLenum type, GLsizei stride, const void *pointer)
{
    UNIMPLEMENTED();
}

// GL_OES_point_size_array
void Context::pointSizePointer(GLenum type, GLsizei stride, const void *ptr)
{
    vertexAttribPointer(vertexArrayIndex(ClientVertexArrayType::PointSize), 1, type, GL_FALSE,
                        stride, ptr);
}

// GL_OES_query_matrix
GLbitfield Context::queryMatrixx(GLfixed *mantissa, GLint *exponent)
{
    UNIMPLEMENTED();
    return 0;
}

// GL_OES_texture_cube_map
void Context::getTexGenfv(GLenum coord, GLenum pname, GLfloat *params)
{
    UNIMPLEMENTED();
}

void Context::getTexGeniv(GLenum coord, GLenum pname, GLint *params)
{
    UNIMPLEMENTED();
}

void Context::getTexGenxv(GLenum coord, GLenum pname, GLfixed *params)
{
    UNIMPLEMENTED();
}

void Context::texGenf(GLenum coord, GLenum pname, GLfloat param)
{
    UNIMPLEMENTED();
}

void Context::texGenfv(GLenum coord, GLenum pname, const GLfloat *params)
{
    UNIMPLEMENTED();
}

void Context::texGeni(GLenum coord, GLenum pname, GLint param)
{
    UNIMPLEMENTED();
}

void Context::texGeniv(GLenum coord, GLenum pname, const GLint *params)
{
    UNIMPLEMENTED();
}

void Context::texGenx(GLenum coord, GLenum pname, GLfixed param)
{
    UNIMPLEMENTED();
}

void Context::texGenxv(GLenum coord, GLenum pname, const GLint *params)
{
    UNIMPLEMENTED();
}

int Context::vertexArrayIndex(ClientVertexArrayType type) const
{
    return GLES1Renderer::VertexArrayIndex(type, mGLState.gles1());
}

// static
int Context::TexCoordArrayIndex(unsigned int unit)
{
    return GLES1Renderer::TexCoordArrayIndex(unit);
}
}  // namespace gl
