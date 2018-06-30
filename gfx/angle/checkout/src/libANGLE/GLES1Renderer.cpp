//
// Copyright 2018 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// GLES1Renderer.cpp: Implements the GLES1Renderer renderer.

#include "libANGLE/GLES1Renderer.h"

#include <string.h>
#include <iterator>
#include <sstream>
#include <vector>

#include "libANGLE/Context.h"
#include "libANGLE/Program.h"
#include "libANGLE/ResourceManager.h"
#include "libANGLE/Shader.h"
#include "libANGLE/State.h"
#include "libANGLE/renderer/ContextImpl.h"

namespace
{
#include "libANGLE/GLES1Shaders.inc"
}  // anonymous namespace

namespace gl
{

GLES1Renderer::GLES1Renderer() : mRendererProgramInitialized(false)
{
}

void GLES1Renderer::onDestroy(Context *context, State *state)
{
    if (mRendererProgramInitialized)
    {
        state->setProgram(context, 0);

        mShaderPrograms->deleteProgram(context, mProgramState.program);
        mShaderPrograms->release(context);
        mShaderPrograms             = nullptr;
        mRendererProgramInitialized = false;
    }
}

GLES1Renderer::~GLES1Renderer() = default;

Error GLES1Renderer::prepareForDraw(Context *context, State *glState)
{
    ANGLE_TRY(initializeRendererProgram(context, glState));

    const GLES1State &gles1State = glState->gles1();

    Program *programObject = getProgram(mProgramState.program);

    GLES1UniformBuffers &uniformBuffers = mUniformBuffers;

    if (!gles1State.isClientStateEnabled(ClientVertexArrayType::Normal))
    {
        const angle::Vector3 normal = gles1State.getCurrentNormal();
        context->vertexAttrib3f(kNormalAttribIndex, normal.x(), normal.y(), normal.z());
    }

    if (!gles1State.isClientStateEnabled(ClientVertexArrayType::Color))
    {
        const ColorF color = gles1State.getCurrentColor();
        context->vertexAttrib4f(kColorAttribIndex, color.red, color.green, color.blue, color.alpha);
    }

    if (!gles1State.isClientStateEnabled(ClientVertexArrayType::PointSize))
    {
        GLfloat pointSize = gles1State.mPointParameters.pointSize;
        context->vertexAttrib1f(kPointSizeAttribIndex, pointSize);
    }

    for (int i = 0; i < kTexUnitCount; i++)
    {
        if (!gles1State.mTexCoordArrayEnabled[i])
        {
            const TextureCoordF texcoord = gles1State.getCurrentTextureCoords(i);
            context->vertexAttrib4f(kTextureCoordAttribIndexBase + i, texcoord.s, texcoord.t,
                                    texcoord.r, texcoord.q);
        }
    }

    {
        angle::Mat4 proj = gles1State.mProjectionMatrices.back();
        if (mProgramState.projMatrixLoc != -1)
        {
            programObject->setUniformMatrix4fv(mProgramState.projMatrixLoc, 1, GL_FALSE,
                                               proj.data());
        }

        angle::Mat4 modelview = gles1State.mModelviewMatrices.back();
        if (mProgramState.modelviewMatrixLoc != -1)
        {
            programObject->setUniformMatrix4fv(mProgramState.modelviewMatrixLoc, 1, GL_FALSE,
                                               modelview.data());
        }

        angle::Mat4 modelviewInvTr = modelview.transpose().inverse();
        if (mProgramState.modelviewInvTrLoc != -1)
        {
            programObject->setUniformMatrix4fv(mProgramState.modelviewInvTrLoc, 1, GL_FALSE,
                                               modelviewInvTr.data());
        }

        Mat4Uniform *textureMatrixBuffer = uniformBuffers.textureMatrices.data();

        for (int i = 0; i < kTexUnitCount; i++)
        {
            angle::Mat4 textureMatrix = gles1State.mTextureMatrices[i].back();
            memcpy(textureMatrixBuffer + i, textureMatrix.data(), sizeof(Mat4Uniform));
        }

        if (mProgramState.textureMatrixLoc != -1)
        {
            programObject->setUniformMatrix4fv(mProgramState.textureMatrixLoc, 4, GL_FALSE,
                                               (float *)uniformBuffers.textureMatrices.data());
        }
    }

    {
        std::array<GLint, kTexUnitCount> &tex2DEnables   = uniformBuffers.tex2DEnables;
        std::array<GLint, kTexUnitCount> &texCubeEnables = uniformBuffers.texCubeEnables;

        for (int i = 0; i < kTexUnitCount; i++)
        {
            // GL_OES_cube_map allows only one of TEXTURE_2D / TEXTURE_CUBE_MAP
            // to be enabled per unit, thankfully. From the extension text:
            //
            //  --  Section 3.8.10 "Texture Application"
            //
            //      Replace the beginning sentences of the first paragraph (page 138)
            //      with:
            //
            //      "Texturing is enabled or disabled using the generic Enable
            //      and Disable commands, respectively, with the symbolic constants
            //      TEXTURE_2D or TEXTURE_CUBE_MAP_OES to enable the two-dimensional or cube
            //      map texturing respectively.  If the cube map texture and the two-
            //      dimensional texture are enabled, then cube map texturing is used.  If
            //      texturing is disabled, a rasterized fragment is passed on unaltered to the
            //      next stage of the GL (although its texture coordinates may be discarded).
            //      Otherwise, a texture value is found according to the parameter values of
            //      the currently bound texture image of the appropriate dimensionality.

            texCubeEnables[i] = gles1State.isTextureTargetEnabled(i, TextureType::CubeMap);
            tex2DEnables[i] =
                !texCubeEnables[i] && (gles1State.isTextureTargetEnabled(i, TextureType::_2D));
        }

        if (mProgramState.enableTexture2DLoc != -1)
        {
            programObject->setUniform1iv(mProgramState.enableTexture2DLoc, kTexUnitCount,
                                         tex2DEnables.data());
        }
        if (mProgramState.enableTextureCubeMapLoc != -1)
        {
            programObject->setUniform1iv(mProgramState.enableTextureCubeMapLoc, kTexUnitCount,
                                         texCubeEnables.data());
        }

        GLint flatShading = gles1State.mShadeModel == ShadingModel::Flat;

        if (mProgramState.shadeModelFlatLoc != -1)
        {
            programObject->setUniform1iv(mProgramState.shadeModelFlatLoc, 1, &flatShading);
        }
    }

    // None of those are changes in sampler, so there is no need to set the GL_PROGRAM dirty.
    // Otherwise, put the dirtying here.

    return NoError();
}

int GLES1Renderer::vertexArrayIndex(ClientVertexArrayType type, const State *glState) const
{
    switch (type)
    {
        case ClientVertexArrayType::Vertex:
            return kVertexAttribIndex;
        case ClientVertexArrayType::Normal:
            return kNormalAttribIndex;
        case ClientVertexArrayType::Color:
            return kColorAttribIndex;
        case ClientVertexArrayType::PointSize:
            return kPointSizeAttribIndex;
        case ClientVertexArrayType::TextureCoord:
            return kTextureCoordAttribIndexBase + glState->gles1().getClientTextureUnit();
        default:
            UNREACHABLE();
            return 0;
    }
}

// static
int GLES1Renderer::TexCoordArrayIndex(unsigned int unit)
{
    return kTextureCoordAttribIndexBase + unit;
}

AttributesMask GLES1Renderer::getVertexArraysAttributeMask(const State *glState) const
{
    AttributesMask res;
    const GLES1State &gles1 = glState->gles1();

    ClientVertexArrayType nonTexcoordArrays[] = {
        ClientVertexArrayType::Vertex, ClientVertexArrayType::Normal, ClientVertexArrayType::Color,
        ClientVertexArrayType::PointSize,
    };

    for (const ClientVertexArrayType attrib : nonTexcoordArrays)
    {
        res.set(vertexArrayIndex(attrib, glState), gles1.isClientStateEnabled(attrib));
    }

    for (unsigned int i = 0; i < kTexUnitCount; i++)
    {
        res.set(TexCoordArrayIndex(i), gles1.isTexCoordArrayEnabled(i));
    }

    return res;
}

Shader *GLES1Renderer::getShader(GLuint handle) const
{
    return mShaderPrograms->getShader(handle);
}

Program *GLES1Renderer::getProgram(GLuint handle) const
{
    return mShaderPrograms->getProgram(handle);
}

Error GLES1Renderer::compileShader(Context *context,
                                   ShaderType shaderType,
                                   const char *src,
                                   GLuint *shaderOut)
{
    rx::ContextImpl *implementation = context->getImplementation();
    const Limitations &limitations  = implementation->getNativeLimitations();

    GLuint shader = mShaderPrograms->createShader(implementation, limitations, shaderType);

    Shader *shaderObject = getShader(shader);

    if (!shaderObject)
        return InternalError();

    shaderObject->setSource(1, &src, nullptr);
    shaderObject->compile(context);

    *shaderOut = shader;

    if (!shaderObject->isCompiled(context))
    {
        GLint infoLogLength = shaderObject->getInfoLogLength(context);
        std::vector<char> infoLog(infoLogLength, 0);
        shaderObject->getInfoLog(context, infoLogLength - 1, nullptr, infoLog.data());
        fprintf(stderr, "GLES1Renderer::%s: Info log: %s\n", __func__, infoLog.data());
        return InternalError() << "GLES1Renderer shader compile failed. Source: " << src
                               << " Info log: " << infoLog.data();
    }

    return NoError();
}

Error GLES1Renderer::linkProgram(Context *context,
                                 State *glState,
                                 GLuint vertexShader,
                                 GLuint fragmentShader,
                                 const std::unordered_map<GLint, std::string> &attribLocs,
                                 GLuint *programOut)
{
    GLuint program = mShaderPrograms->createProgram(context->getImplementation());

    Program *programObject = getProgram(program);

    if (!programObject)
    {
        return InternalError();
    }

    *programOut = program;

    programObject->attachShader(getShader(vertexShader));
    programObject->attachShader(getShader(fragmentShader));

    for (auto it : attribLocs)
    {
        GLint index             = it.first;
        const std::string &name = it.second;
        programObject->bindAttributeLocation(index, name.c_str());
    }

    ANGLE_TRY(programObject->link(context));

    glState->onProgramExecutableChange(programObject);

    if (!programObject->isLinked())
    {
        GLint infoLogLength = programObject->getInfoLogLength();
        std::vector<char> infoLog(infoLogLength, 0);
        programObject->getInfoLog(infoLogLength - 1, nullptr, infoLog.data());
        return InternalError() << "GLES1Renderer program link failed. Info log: " << infoLog.data();
    }

    programObject->detachShader(context, getShader(vertexShader));
    programObject->detachShader(context, getShader(fragmentShader));

    return NoError();
}

Error GLES1Renderer::initializeRendererProgram(Context *context, State *glState)
{
    if (mRendererProgramInitialized)
    {
        return NoError();
    }

    mShaderPrograms = new ShaderProgramManager();

    GLuint vertexShader;
    GLuint fragmentShader;

    ANGLE_TRY(compileShader(context, ShaderType::Vertex, kGLES1DrawVShader, &vertexShader));

    std::stringstream fragmentStream;
    fragmentStream << kGLES1DrawFShaderHeader;
    fragmentStream << kGLES1DrawFShaderUniformDefs;
    fragmentStream << kGLES1DrawFShaderFunctions;
    fragmentStream << kGLES1DrawFShaderMain;

    ANGLE_TRY(compileShader(context, ShaderType::Fragment, fragmentStream.str().c_str(),
                            &fragmentShader));

    std::unordered_map<GLint, std::string> attribLocs;

    attribLocs[(GLint)kVertexAttribIndex]    = "pos";
    attribLocs[(GLint)kNormalAttribIndex]    = "normal";
    attribLocs[(GLint)kColorAttribIndex]     = "color";
    attribLocs[(GLint)kPointSizeAttribIndex] = "pointsize";

    for (int i = 0; i < kTexUnitCount; i++)
    {
        std::stringstream ss;
        ss << "texcoord" << i;
        attribLocs[kTextureCoordAttribIndexBase + i] = ss.str();
    }

    ANGLE_TRY(linkProgram(context, glState, vertexShader, fragmentShader, attribLocs,
                          &mProgramState.program));

    mShaderPrograms->deleteShader(context, vertexShader);
    mShaderPrograms->deleteShader(context, fragmentShader);

    Program *programObject = getProgram(mProgramState.program);

    mProgramState.projMatrixLoc      = programObject->getUniformLocation("projection");
    mProgramState.modelviewMatrixLoc = programObject->getUniformLocation("modelview");
    mProgramState.textureMatrixLoc   = programObject->getUniformLocation("texture_matrix");
    mProgramState.modelviewInvTrLoc  = programObject->getUniformLocation("modelview_invtr");

    for (int i = 0; i < kTexUnitCount; i++)
    {
        std::stringstream ss2d;
        std::stringstream sscube;

        ss2d << "tex_sampler" << i;
        sscube << "tex_cube_sampler" << i;

        mProgramState.tex2DSamplerLocs[i] = programObject->getUniformLocation(ss2d.str().c_str());
        mProgramState.texCubeSamplerLocs[i] =
            programObject->getUniformLocation(sscube.str().c_str());
    }

    mProgramState.shadeModelFlatLoc = programObject->getUniformLocation("shade_model_flat");

    mProgramState.enableTexture2DLoc = programObject->getUniformLocation("enable_texture_2d");
    mProgramState.enableTextureCubeMapLoc =
        programObject->getUniformLocation("enable_texture_cube_map");

    glState->setProgram(context, programObject);

    for (int i = 0; i < kTexUnitCount; i++)
    {

        if (mProgramState.tex2DSamplerLocs[i] != -1)
        {
            GLint val = i;
            programObject->setUniform1iv(mProgramState.tex2DSamplerLocs[i], 1, &val);
        }

        if (mProgramState.texCubeSamplerLocs[i] != -1)
        {
            GLint val = i + kTexUnitCount;
            programObject->setUniform1iv(mProgramState.texCubeSamplerLocs[i], 1, &val);
        }
    }

    glState->setObjectDirty(GL_PROGRAM);

    mRendererProgramInitialized = true;
    return NoError();
}

}  // namespace gl
