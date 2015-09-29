//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// CompilerGL.cpp: Implements the class methods for CompilerGL.

#include "libANGLE/renderer/gl/CompilerGL.h"

#include "common/debug.h"
#include "libANGLE/Caps.h"
#include "libANGLE/Data.h"
#include "libANGLE/renderer/gl/FunctionsGL.h"

namespace rx
{

// Global count of active shader compiler handles. Needed to know when to call ShInitialize and ShFinalize.
static size_t activeCompilerHandles = 0;

static ShShaderOutput GetShaderOutputType(const FunctionsGL *functions)
{
    ASSERT(functions);

    if (functions->standard == STANDARD_GL_DESKTOP)
    {
        // GLSL outputs
        if (functions->isAtLeastGL(gl::Version(4, 5)))
        {
            return SH_GLSL_450_CORE_OUTPUT;
        }
        else if (functions->isAtLeastGL(gl::Version(4, 4)))
        {
            return SH_GLSL_440_CORE_OUTPUT;
        }
        else if (functions->isAtLeastGL(gl::Version(4, 3)))
        {
            return SH_GLSL_430_CORE_OUTPUT;
        }
        else if (functions->isAtLeastGL(gl::Version(4, 2)))
        {
            return SH_GLSL_420_CORE_OUTPUT;
        }
        else if (functions->isAtLeastGL(gl::Version(4, 1)))
        {
            return SH_GLSL_410_CORE_OUTPUT;
        }
        else if (functions->isAtLeastGL(gl::Version(4, 0)))
        {
            return SH_GLSL_400_CORE_OUTPUT;
        }
        else if (functions->isAtLeastGL(gl::Version(3, 3)))
        {
            return SH_GLSL_330_CORE_OUTPUT;
        }
        else if (functions->isAtLeastGL(gl::Version(3, 2)))
        {
            return SH_GLSL_150_CORE_OUTPUT;
        }
        else if (functions->isAtLeastGL(gl::Version(3, 1)))
        {
            return SH_GLSL_140_OUTPUT;
        }
        else if (functions->isAtLeastGL(gl::Version(3, 0)))
        {
            return SH_GLSL_130_OUTPUT;
        }
        else
        {
            return SH_GLSL_COMPATIBILITY_OUTPUT;
        }
    }
    else if (functions->standard == STANDARD_GL_ES)
    {
        // ESSL outputs
        return SH_ESSL_OUTPUT;
    }
    else
    {
        UNREACHABLE();
        return ShShaderOutput(0);
    }
}

CompilerGL::CompilerGL(const gl::Data &data, const FunctionsGL *functions)
    : CompilerImpl(),
      mSpec(data.clientVersion > 2 ? SH_GLES3_SPEC : SH_GLES2_SPEC),
      mOutputType(GetShaderOutputType(functions)),
      mResources(),
      mFragmentCompiler(nullptr),
      mVertexCompiler(nullptr)
{
    ASSERT(data.clientVersion == 2 || data.clientVersion == 3);

    const gl::Caps &caps = *data.caps;
    const gl::Extensions &extensions = *data.extensions;

    ShInitBuiltInResources(&mResources);
    mResources.MaxVertexAttribs = caps.maxVertexAttributes;
    mResources.MaxVertexUniformVectors = caps.maxVertexUniformVectors;
    mResources.MaxVaryingVectors = caps.maxVaryingVectors;
    mResources.MaxVertexTextureImageUnits = caps.maxVertexTextureImageUnits;
    mResources.MaxCombinedTextureImageUnits = caps.maxCombinedTextureImageUnits;
    mResources.MaxTextureImageUnits = caps.maxTextureImageUnits;
    mResources.MaxFragmentUniformVectors = caps.maxFragmentUniformVectors;
    mResources.MaxDrawBuffers = caps.maxDrawBuffers;
    mResources.OES_standard_derivatives = extensions.standardDerivatives;
    mResources.EXT_draw_buffers = extensions.drawBuffers;
    mResources.EXT_shader_texture_lod = extensions.shaderTextureLOD;
    mResources.OES_EGL_image_external = 0; // TODO: disabled until the extension is actually supported.
    mResources.FragmentPrecisionHigh = 1; // TODO: use shader precision caps to determine if high precision is supported?
    mResources.EXT_frag_depth = extensions.fragDepth;

    // GLSL ES 3.0 constants
    mResources.MaxVertexOutputVectors = caps.maxVertexOutputComponents / 4;
    mResources.MaxFragmentInputVectors = caps.maxFragmentInputComponents / 4;
    mResources.MinProgramTexelOffset = caps.minProgramTexelOffset;
    mResources.MaxProgramTexelOffset = caps.maxProgramTexelOffset;
}

CompilerGL::~CompilerGL()
{
    release();
}

gl::Error CompilerGL::release()
{
    if (mFragmentCompiler)
    {
        ShDestruct(mFragmentCompiler);
        mFragmentCompiler = NULL;

        ASSERT(activeCompilerHandles > 0);
        activeCompilerHandles--;
    }

    if (mVertexCompiler)
    {
        ShDestruct(mVertexCompiler);
        mVertexCompiler = NULL;

        ASSERT(activeCompilerHandles > 0);
        activeCompilerHandles--;
    }

    if (activeCompilerHandles == 0)
    {
        ShFinalize();
    }

    return gl::Error(GL_NO_ERROR);
}

ShHandle CompilerGL::getCompilerHandle(GLenum type)
{
    ShHandle *compiler = NULL;
    switch (type)
    {
      case GL_VERTEX_SHADER:
        compiler = &mVertexCompiler;
        break;

      case GL_FRAGMENT_SHADER:
        compiler = &mFragmentCompiler;
        break;

      default:
        UNREACHABLE();
        return NULL;
    }

    if ((*compiler) == nullptr)
    {
        if (activeCompilerHandles == 0)
        {
            ShInitialize();
        }

        *compiler = ShConstructCompiler(type, mSpec, mOutputType, &mResources);
        activeCompilerHandles++;
    }

    return *compiler;
}

}
