//
// Copyright(c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// entry_points_gles_2_0_ext.cpp : Implements the GLES 2.0 extension entry points.

#include "libGLESv2/entry_points_gles_2_0_ext.h"
#include "libGLESv2/global_state.h"

#include "libANGLE/Buffer.h"
#include "libANGLE/Context.h"
#include "libANGLE/Error.h"
#include "libANGLE/ErrorStrings.h"
#include "libANGLE/Fence.h"
#include "libANGLE/Framebuffer.h"
#include "libANGLE/Query.h"
#include "libANGLE/Shader.h"
#include "libANGLE/Thread.h"
#include "libANGLE/VertexArray.h"
#include "libANGLE/queryconversions.h"
#include "libANGLE/queryutils.h"

#include "libANGLE/validationES.h"
#include "libANGLE/validationES2.h"
#include "libANGLE/validationES3.h"
#include "libANGLE/validationES31.h"

#include "common/debug.h"
#include "common/utilities.h"

namespace gl
{

namespace
{

void SetRobustLengthParam(GLsizei *length, GLsizei value)
{
    if (length)
    {
        *length = value;
    }
}

}  // anonymous namespace

ANGLE_EXPORT void GL_APIENTRY BindUniformLocationCHROMIUM(GLuint program,
                                                          GLint location,
                                                          const GLchar *name)
{
    EVENT("(GLuint program = %u, GLint location = %d, const GLchar *name = 0x%0.8p)", program,
          location, name);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() &&
            !ValidateBindUniformLocationCHROMIUM(context, program, location, name))
        {
            return;
        }

        context->bindUniformLocation(program, location, name);
    }
}

ANGLE_EXPORT void GL_APIENTRY CoverageModulationCHROMIUM(GLenum components)
{
    EVENT("(GLenum components = %u)", components);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateCoverageModulationCHROMIUM(context, components))
        {
            return;
        }
        context->setCoverageModulation(components);
    }
}

// CHROMIUM_path_rendering
ANGLE_EXPORT void GL_APIENTRY MatrixLoadfCHROMIUM(GLenum matrixMode, const GLfloat *matrix)
{
    EVENT("(GLenum matrixMode = %u)", matrixMode);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateMatrixLoadfCHROMIUM(context, matrixMode, matrix))
        {
            return;
        }
        context->loadPathRenderingMatrix(matrixMode, matrix);
    }
}

ANGLE_EXPORT void GL_APIENTRY MatrixLoadIdentityCHROMIUM(GLenum matrixMode)
{
    EVENT("(GLenum matrixMode = %u)", matrixMode);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateMatrixLoadIdentityCHROMIUM(context, matrixMode))
        {
            return;
        }
        context->loadPathRenderingIdentityMatrix(matrixMode);
    }
}

ANGLE_EXPORT GLuint GL_APIENTRY GenPathsCHROMIUM(GLsizei range)
{
    EVENT("(GLsizei range = %d)", range);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateGenPathsCHROMIUM(context, range))
        {
            return 0;
        }
        return context->createPaths(range);
    }
    return 0;
}

ANGLE_EXPORT void GL_APIENTRY DeletePathsCHROMIUM(GLuint first, GLsizei range)
{
    EVENT("(GLuint first = %u, GLsizei range = %d)", first, range);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateDeletePathsCHROMIUM(context, first, range))
        {
            return;
        }
        context->deletePaths(first, range);
    }
}

ANGLE_EXPORT GLboolean GL_APIENTRY IsPathCHROMIUM(GLuint path)
{
    EVENT("(GLuint path = %u)", path);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateIsPathCHROMIUM(context))
        {
            return GL_FALSE;
        }
        return context->hasPathData(path);
    }
    return GL_FALSE;
}

ANGLE_EXPORT void GL_APIENTRY PathCommandsCHROMIUM(GLuint path,
                                                   GLsizei numCommands,
                                                   const GLubyte *commands,
                                                   GLsizei numCoords,
                                                   GLenum coordType,
                                                   const void *coords)
{
    EVENT(
        "(GLuint path = %u, GLsizei numCommands = %d, commands = %p, "
        "GLsizei numCoords = %d, GLenum coordType = %u, void* coords = %p)",
        path, numCommands, commands, numCoords, coordType, coords);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() &&
            !ValidatePathCommandsCHROMIUM(context, path, numCommands, commands, numCoords,
                                          coordType, coords))
        {
            return;
        }
        context->setPathCommands(path, numCommands, commands, numCoords, coordType, coords);
    }
}

ANGLE_EXPORT void GL_APIENTRY PathParameterfCHROMIUM(GLuint path, GLenum pname, GLfloat value)
{
    EVENT("(GLuint path = %u, GLenum pname = %u, GLfloat value = %f)", path, pname, value);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() &&
            !ValidatePathParameterfCHROMIUM(context, path, pname, value))
        {
            return;
        }

        context->pathParameterf(path, pname, value);
    }
}

ANGLE_EXPORT void GL_APIENTRY PathParameteriCHROMIUM(GLuint path, GLenum pname, GLint value)
{
    EVENT("(GLuint path = %u, GLenum pname = %u, GLint value = %d)", path, pname, value);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() &&
            !ValidatePathParameteriCHROMIUM(context, path, pname, value))
        {
            return;
        }

        context->pathParameteri(path, pname, value);
    }
}

ANGLE_EXPORT void GL_APIENTRY GetPathParameterfvCHROMIUM(GLuint path, GLenum pname, GLfloat *value)
{
    EVENT("(GLuint path = %u, GLenum pname = %u, GLfloat *value = %p)", path, pname, value);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() &&
            !ValidateGetPathParameterfvCHROMIUM(context, path, pname, value))
        {
            return;
        }
        context->getPathParameterfv(path, pname, value);
    }
}

ANGLE_EXPORT void GL_APIENTRY GetPathParameterivCHROMIUM(GLuint path, GLenum pname, GLint *value)
{
    EVENT("(GLuint path = %u, GLenum pname = %u, GLint *value = %p)", path, pname, value);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() &&
            !ValidateGetPathParameterivCHROMIUM(context, path, pname, value))
        {
            return;
        }
        context->getPathParameteriv(path, pname, value);
    }
}

ANGLE_EXPORT void GL_APIENTRY PathStencilFuncCHROMIUM(GLenum func, GLint ref, GLuint mask)
{
    EVENT("(GLenum func = %u, GLint ref = %d, GLuint mask = %u)", func, ref, mask);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() &&
            !ValidatePathStencilFuncCHROMIUM(context, func, ref, mask))
        {
            return;
        }
        context->setPathStencilFunc(func, ref, mask);
    }
}

ANGLE_EXPORT void GL_APIENTRY StencilFillPathCHROMIUM(GLuint path, GLenum fillMode, GLuint mask)
{
    EVENT("(GLuint path = %u, GLenum fillMode = %u, GLuint mask = %u)", path, fillMode, mask);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() &&
            !ValidateStencilFillPathCHROMIUM(context, path, fillMode, mask))
        {
            return;
        }
        context->stencilFillPath(path, fillMode, mask);
    }
}

ANGLE_EXPORT void GL_APIENTRY StencilStrokePathCHROMIUM(GLuint path, GLint reference, GLuint mask)
{
    EVENT("(GLuint path = %u, GLint ference = %d, GLuint mask = %u)", path, reference, mask);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() &&
            !ValidateStencilStrokePathCHROMIUM(context, path, reference, mask))
        {
            return;
        }
        context->stencilStrokePath(path, reference, mask);
    }
}

ANGLE_EXPORT void GL_APIENTRY CoverFillPathCHROMIUM(GLuint path, GLenum coverMode)
{
    EVENT("(GLuint path = %u, GLenum coverMode = %u)", path, coverMode);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateCoverPathCHROMIUM(context, path, coverMode))
        {
            return;
        }
        context->coverFillPath(path, coverMode);
    }
}

ANGLE_EXPORT void GL_APIENTRY CoverStrokePathCHROMIUM(GLuint path, GLenum coverMode)
{
    EVENT("(GLuint path = %u, GLenum coverMode = %u)", path, coverMode);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateCoverPathCHROMIUM(context, path, coverMode))
        {
            return;
        }
        context->coverStrokePath(path, coverMode);
    }
}

ANGLE_EXPORT void GL_APIENTRY StencilThenCoverFillPathCHROMIUM(GLuint path,
                                                               GLenum fillMode,
                                                               GLuint mask,
                                                               GLenum coverMode)
{
    EVENT("(GLuint path = %u, GLenum fillMode = %u, GLuint mask = %u, GLenum coverMode = %u)", path,
          fillMode, mask, coverMode);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() &&
            !ValidateStencilThenCoverFillPathCHROMIUM(context, path, fillMode, mask, coverMode))
        {
            return;
        }
        context->stencilThenCoverFillPath(path, fillMode, mask, coverMode);
    }
}

ANGLE_EXPORT void GL_APIENTRY StencilThenCoverStrokePathCHROMIUM(GLuint path,
                                                                 GLint reference,
                                                                 GLuint mask,
                                                                 GLenum coverMode)
{
    EVENT("(GLuint path = %u, GLint reference = %d, GLuint mask = %u, GLenum coverMode = %u)", path,
          reference, mask, coverMode);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() &&
            !ValidateStencilThenCoverStrokePathCHROMIUM(context, path, reference, mask, coverMode))
        {
            return;
        }
        context->stencilThenCoverStrokePath(path, reference, mask, coverMode);
    }
}

ANGLE_EXPORT void GL_APIENTRY CoverFillPathInstancedCHROMIUM(GLsizei numPaths,
                                                             GLenum pathNameType,
                                                             const void *paths,
                                                             GLuint pathBase,
                                                             GLenum coverMode,
                                                             GLenum transformType,
                                                             const GLfloat *transformValues)
{
    EVENT(
        "(GLsizei numPaths = %d, GLenum pathNameType = %u, const void *paths = %p "
        "GLuint pathBase = %u, GLenum coverMode = %u, GLenum transformType = %u "
        "const GLfloat *transformValues = %p)",
        numPaths, pathNameType, paths, pathBase, coverMode, transformType, transformValues);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateCoverFillPathInstancedCHROMIUM(
                                              context, numPaths, pathNameType, paths, pathBase,
                                              coverMode, transformType, transformValues))
        {
            return;
        }
        context->coverFillPathInstanced(numPaths, pathNameType, paths, pathBase, coverMode,
                                        transformType, transformValues);
    }
}

ANGLE_EXPORT void GL_APIENTRY CoverStrokePathInstancedCHROMIUM(GLsizei numPaths,
                                                               GLenum pathNameType,
                                                               const void *paths,
                                                               GLuint pathBase,
                                                               GLenum coverMode,
                                                               GLenum transformType,
                                                               const GLfloat *transformValues)
{
    EVENT(
        "(GLsizei numPaths = %d, GLenum pathNameType = %u, const void *paths = %p "
        "GLuint pathBase = %u, GLenum coverMode = %u, GLenum transformType = %u "
        "const GLfloat *transformValues = %p)",
        numPaths, pathNameType, paths, pathBase, coverMode, transformType, transformValues);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateCoverStrokePathInstancedCHROMIUM(
                                              context, numPaths, pathNameType, paths, pathBase,
                                              coverMode, transformType, transformValues))
        {
            return;
        }
        context->coverStrokePathInstanced(numPaths, pathNameType, paths, pathBase, coverMode,
                                          transformType, transformValues);
    }
}

ANGLE_EXPORT void GL_APIENTRY StencilStrokePathInstancedCHROMIUM(GLsizei numPaths,
                                                                 GLenum pathNameType,
                                                                 const void *paths,
                                                                 GLuint pathBase,
                                                                 GLint reference,
                                                                 GLuint mask,
                                                                 GLenum transformType,
                                                                 const GLfloat *transformValues)
{
    EVENT(
        "(GLsizei numPaths = %u, GLenum pathNameType = %u, const void *paths = %p "
        "GLuint pathBase = %u, GLint reference = %d GLuint mask = %u GLenum transformType = %u "
        "const GLfloat *transformValues = %p)",
        numPaths, pathNameType, paths, pathBase, reference, mask, transformType, transformValues);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateStencilStrokePathInstancedCHROMIUM(
                                              context, numPaths, pathNameType, paths, pathBase,
                                              reference, mask, transformType, transformValues))
        {
            return;
        }
        context->stencilStrokePathInstanced(numPaths, pathNameType, paths, pathBase, reference,
                                            mask, transformType, transformValues);
    }
}

ANGLE_EXPORT void GL_APIENTRY StencilFillPathInstancedCHROMIUM(GLsizei numPaths,
                                                               GLenum pathNameType,
                                                               const void *paths,
                                                               GLuint pathBase,
                                                               GLenum fillMode,
                                                               GLuint mask,
                                                               GLenum transformType,
                                                               const GLfloat *transformValues)
{
    EVENT(
        "(GLsizei numPaths = %u, GLenum pathNameType = %u const void *paths = %p "
        "GLuint pathBase = %u, GLenum fillMode = %u, GLuint mask = %u, GLenum transformType = %u "
        "const GLfloat *transformValues = %p)",
        numPaths, pathNameType, paths, pathBase, fillMode, mask, transformType, transformValues);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateStencilFillPathInstancedCHROMIUM(
                                              context, numPaths, pathNameType, paths, pathBase,
                                              fillMode, mask, transformType, transformValues))
        {
            return;
        }
        context->stencilFillPathInstanced(numPaths, pathNameType, paths, pathBase, fillMode, mask,
                                          transformType, transformValues);
    }
}

ANGLE_EXPORT void GL_APIENTRY
StencilThenCoverFillPathInstancedCHROMIUM(GLsizei numPaths,
                                          GLenum pathNameType,
                                          const void *paths,
                                          GLuint pathBase,
                                          GLenum fillMode,
                                          GLuint mask,
                                          GLenum coverMode,
                                          GLenum transformType,
                                          const GLfloat *transformValues)
{
    EVENT(
        "(GLsizei numPaths = %u, GLenum pathNameType = %u const void *paths = %p "
        "GLuint pathBase = %u, GLenum coverMode = %u, GLuint mask = %u, GLenum transformType = %u "
        "const GLfloat *transformValues = %p)",
        numPaths, pathNameType, paths, pathBase, coverMode, mask, transformType, transformValues);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() &&
            !ValidateStencilThenCoverFillPathInstancedCHROMIUM(
                context, numPaths, pathNameType, paths, pathBase, fillMode, mask, coverMode,
                transformType, transformValues))
        {
            return;
        }
        context->stencilThenCoverFillPathInstanced(numPaths, pathNameType, paths, pathBase,
                                                   fillMode, mask, coverMode, transformType,
                                                   transformValues);
    }
}

ANGLE_EXPORT void GL_APIENTRY
StencilThenCoverStrokePathInstancedCHROMIUM(GLsizei numPaths,
                                            GLenum pathNameType,
                                            const void *paths,
                                            GLuint pathBase,
                                            GLint reference,
                                            GLuint mask,
                                            GLenum coverMode,
                                            GLenum transformType,
                                            const GLfloat *transformValues)
{
    EVENT(
        "(GLsizei numPaths = %u, GLenum pathNameType = %u, const void *paths = %p "
        "GLuint pathBase = %u GLenum coverMode = %u GLint reference = %d GLuint mask = %u GLenum "
        "transformType = %u "
        "const GLfloat *transformValues = %p)",
        numPaths, pathNameType, paths, pathBase, coverMode, reference, mask, transformType,
        transformValues);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() &&
            !ValidateStencilThenCoverStrokePathInstancedCHROMIUM(
                context, numPaths, pathNameType, paths, pathBase, reference, mask, coverMode,
                transformType, transformValues))
        {
            return;
        }
        context->stencilThenCoverStrokePathInstanced(numPaths, pathNameType, paths, pathBase,
                                                     reference, mask, coverMode, transformType,
                                                     transformValues);
    }
}

ANGLE_EXPORT void GL_APIENTRY BindFragmentInputLocationCHROMIUM(GLuint program,
                                                                GLint location,
                                                                const GLchar *name)
{
    EVENT("(GLuint program = %u, GLint location = %d, const GLchar *name = %p)", program, location,
          name);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() &&
            !ValidateBindFragmentInputLocationCHROMIUM(context, program, location, name))
        {
            return;
        }
        context->bindFragmentInputLocation(program, location, name);
    }
}

ANGLE_EXPORT void GL_APIENTRY ProgramPathFragmentInputGenCHROMIUM(GLuint program,
                                                                  GLint location,
                                                                  GLenum genMode,
                                                                  GLint components,
                                                                  const GLfloat *coeffs)
{
    EVENT(
        "(GLuint program = %u, GLint location %d, GLenum genMode = %u, GLint components = %d, "
        "const GLfloat * coeffs = %p)",
        program, location, genMode, components, coeffs);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() &&
            !ValidateProgramPathFragmentInputGenCHROMIUM(context, program, location, genMode,
                                                         components, coeffs))
        {
            return;
        }
        context->programPathFragmentInputGen(program, location, genMode, components, coeffs);
    }
}

ANGLE_EXPORT void GL_APIENTRY CopyTextureCHROMIUM(GLuint sourceId,
                                                  GLint sourceLevel,
                                                  GLenum destTarget,
                                                  GLuint destId,
                                                  GLint destLevel,
                                                  GLint internalFormat,
                                                  GLenum destType,
                                                  GLboolean unpackFlipY,
                                                  GLboolean unpackPremultiplyAlpha,
                                                  GLboolean unpackUnmultiplyAlpha)
{
    EVENT(
        "(GLuint sourceId = %u, GLint sourceLevel = %d, GLenum destTarget = 0x%X, GLuint destId = "
        "%u, GLint destLevel = %d, GLint internalFormat = 0x%X, GLenum destType = "
        "0x%X, GLboolean unpackFlipY = %u, GLboolean unpackPremultiplyAlpha = %u, GLboolean "
        "unpackUnmultiplyAlpha = %u)",
        sourceId, sourceLevel, destTarget, destId, destLevel, internalFormat, destType, unpackFlipY,
        unpackPremultiplyAlpha, unpackUnmultiplyAlpha);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() &&
            !ValidateCopyTextureCHROMIUM(context, sourceId, sourceLevel, destTarget, destId,
                                         destLevel, internalFormat, destType, unpackFlipY,
                                         unpackPremultiplyAlpha, unpackUnmultiplyAlpha))
        {
            return;
        }

        context->copyTexture(sourceId, sourceLevel, destTarget, destId, destLevel, internalFormat,
                             destType, unpackFlipY, unpackPremultiplyAlpha, unpackUnmultiplyAlpha);
    }
}

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
                                                     GLboolean unpackUnmultiplyAlpha)
{
    EVENT(
        "(GLuint sourceId = %u, GLint sourceLevel = %d, GLenum destTarget = 0x%X, GLuint destId = "
        "%u, GLint destLevel = %d, GLint xoffset = "
        "%d, GLint yoffset = %d, GLint x = %d, GLint y = %d, GLsizei width = %d, GLsizei height = "
        "%d, GLboolean unpackPremultiplyAlpha = %u, GLboolean unpackUnmultiplyAlpha = %u)",
        sourceId, sourceLevel, destTarget, destId, destLevel, xoffset, yoffset, x, y, width, height,
        unpackFlipY, unpackPremultiplyAlpha, unpackUnmultiplyAlpha);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() &&
            !ValidateCopySubTextureCHROMIUM(
                context, sourceId, sourceLevel, destTarget, destId, destLevel, xoffset, yoffset, x,
                y, width, height, unpackFlipY, unpackPremultiplyAlpha, unpackUnmultiplyAlpha))
        {
            return;
        }

        context->copySubTexture(sourceId, sourceLevel, destTarget, destId, destLevel, xoffset,
                                yoffset, x, y, width, height, unpackFlipY, unpackPremultiplyAlpha,
                                unpackUnmultiplyAlpha);
    }
}

ANGLE_EXPORT void GL_APIENTRY CompressedCopyTextureCHROMIUM(GLuint sourceId, GLuint destId)
{
    EVENT("(GLuint sourceId = %u, GLuint destId = %u)", sourceId, destId);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() &&
            !ValidateCompressedCopyTextureCHROMIUM(context, sourceId, destId))
        {
            return;
        }

        context->compressedCopyTexture(sourceId, destId);
    }
}

ANGLE_EXPORT void GL_APIENTRY RequestExtensionANGLE(const GLchar *name)
{
    EVENT("(const GLchar *name = %p)", name);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateRequestExtensionANGLE(context, name))
        {
            return;
        }

        context->requestExtension(name);
    }
}

ANGLE_EXPORT void GL_APIENTRY GetBooleanvRobustANGLE(GLenum pname,
                                                     GLsizei bufSize,
                                                     GLsizei *length,
                                                     GLboolean *params)
{
    EVENT(
        "(GLenum pname = 0x%X, GLsizei bufsize = %d, GLsizei* length = 0x%0.8p, GLboolean* params "
        "= 0x%0.8p)",
        pname, bufSize, length, params);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        GLenum nativeType;
        unsigned int numParams = 0;
        if (!ValidateRobustStateQuery(context, pname, bufSize, &nativeType, &numParams))
        {
            return;
        }

        context->getBooleanv(pname, params);
        SetRobustLengthParam(length, numParams);
    }
}

ANGLE_EXPORT void GL_APIENTRY GetBufferParameterivRobustANGLE(GLenum target,
                                                              GLenum pname,
                                                              GLsizei bufSize,
                                                              GLsizei *length,
                                                              GLint *params)
{
    EVENT("(GLenum target = 0x%X, GLenum pname = 0x%X, GLint* params = 0x%0.8p)", target, pname,
          params);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        BufferBinding targetPacked = FromGLenum<BufferBinding>(target);

        GLsizei numParams = 0;
        if (!ValidateGetBufferParameterivRobustANGLE(context, targetPacked, pname, bufSize,
                                                     &numParams, params))
        {
            return;
        }

        Buffer *buffer = context->getGLState().getTargetBuffer(targetPacked);
        QueryBufferParameteriv(buffer, pname, params);
        SetRobustLengthParam(length, numParams);
    }
}

ANGLE_EXPORT void GL_APIENTRY GetFloatvRobustANGLE(GLenum pname,
                                                   GLsizei bufSize,
                                                   GLsizei *length,
                                                   GLfloat *params)
{
    EVENT(
        "(GLenum pname = 0x%X, GLsizei bufsize = %d, GLsizei* length = 0x%0.8p, GLfloat* params = "
        "0x%0.8p)",
        pname, bufSize, length, params);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        GLenum nativeType;
        unsigned int numParams = 0;
        if (!ValidateRobustStateQuery(context, pname, bufSize, &nativeType, &numParams))
        {
            return;
        }

        context->getFloatv(pname, params);
        SetRobustLengthParam(length, numParams);
    }
}

ANGLE_EXPORT void GL_APIENTRY GetFramebufferAttachmentParameterivRobustANGLE(GLenum target,
                                                                             GLenum attachment,
                                                                             GLenum pname,
                                                                             GLsizei bufSize,
                                                                             GLsizei *length,
                                                                             GLint *params)
{
    EVENT(
        "(GLenum target = 0x%X, GLenum attachment = 0x%X, GLenum pname = 0x%X,  GLsizei bufsize = "
        "%d, GLsizei* length = 0x%0.8p, GLint* params = 0x%0.8p)",
        target, attachment, pname, bufSize, length, params);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        GLsizei numParams = 0;
        if (!ValidateGetFramebufferAttachmentParameterivRobustANGLE(context, target, attachment,
                                                                    pname, bufSize, &numParams))
        {
            return;
        }

        const Framebuffer *framebuffer = context->getGLState().getTargetFramebuffer(target);
        QueryFramebufferAttachmentParameteriv(context, framebuffer, attachment, pname, params);
        SetRobustLengthParam(length, numParams);
    }
}

ANGLE_EXPORT void GL_APIENTRY GetIntegervRobustANGLE(GLenum pname,
                                                     GLsizei bufSize,
                                                     GLsizei *length,
                                                     GLint *data)
{
    EVENT(
        "(GLenum pname = 0x%X, GLsizei bufsize = %d, GLsizei* length = 0x%0.8p, GLint* params = "
        "0x%0.8p)",
        pname, bufSize, length, data);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        GLenum nativeType;
        unsigned int numParams = 0;
        if (!ValidateRobustStateQuery(context, pname, bufSize, &nativeType, &numParams))
        {
            return;
        }

        context->getIntegerv(pname, data);
        SetRobustLengthParam(length, numParams);
    }
}

ANGLE_EXPORT void GL_APIENTRY GetProgramivRobustANGLE(GLuint program,
                                                      GLenum pname,
                                                      GLsizei bufSize,
                                                      GLsizei *length,
                                                      GLint *params)
{
    EVENT(
        "(GLuint program = %d, GLenum pname = %d, GLsizei bufsize = %d, GLsizei* length = 0x%0.8p, "
        "GLint* params = 0x%0.8p)",
        program, pname, bufSize, length, params);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        GLsizei numParams = 0;
        if (!ValidateGetProgramivRobustANGLE(context, program, pname, bufSize, &numParams))
        {
            return;
        }

        Program *programObject = context->getProgram(program);
        QueryProgramiv(context, programObject, pname, params);
        SetRobustLengthParam(length, numParams);
    }
}

ANGLE_EXPORT void GL_APIENTRY GetRenderbufferParameterivRobustANGLE(GLenum target,
                                                                    GLenum pname,
                                                                    GLsizei bufSize,
                                                                    GLsizei *length,
                                                                    GLint *params)
{
    EVENT(
        "(GLenum target = 0x%X, GLenum pname = 0x%X, GLsizei bufsize = %d, GLsizei* length = "
        "0x%0.8p, GLint* params = 0x%0.8p)",
        target, pname, bufSize, length, params);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        GLsizei numParams = 0;
        if (!ValidateGetRenderbufferParameterivRobustANGLE(context, target, pname, bufSize,
                                                           &numParams, params))
        {
            return;
        }

        Renderbuffer *renderbuffer = context->getGLState().getCurrentRenderbuffer();
        QueryRenderbufferiv(context, renderbuffer, pname, params);
        SetRobustLengthParam(length, numParams);
    }
}

ANGLE_EXPORT void GL_APIENTRY
GetShaderivRobustANGLE(GLuint shader, GLenum pname, GLsizei bufSize, GLsizei *length, GLint *params)
{
    EVENT(
        "(GLuint shader = %d, GLenum pname = %d, GLsizei bufsize = %d, GLsizei* length = 0x%0.8p, "
        "GLint* params = 0x%0.8p)",
        shader, pname, bufSize, length, params);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        GLsizei numParams = 0;
        if (!ValidateGetShaderivRobustANGLE(context, shader, pname, bufSize, &numParams, params))
        {
            return;
        }

        Shader *shaderObject = context->getShader(shader);
        QueryShaderiv(context, shaderObject, pname, params);
        SetRobustLengthParam(length, numParams);
    }
}

ANGLE_EXPORT void GL_APIENTRY GetTexParameterfvRobustANGLE(GLenum target,
                                                           GLenum pname,
                                                           GLsizei bufSize,
                                                           GLsizei *length,
                                                           GLfloat *params)
{
    EVENT(
        "(GLenum target = 0x%X, GLenum pname = 0x%X, GLsizei bufsize = %d, GLsizei* length = "
        "0x%0.8p, GLfloat* params = 0x%0.8p)",
        target, pname, bufSize, length, params);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        GLsizei numParams = 0;
        if (!ValidateGetTexParameterfvRobustANGLE(context, target, pname, bufSize, &numParams,
                                                  params))
        {
            return;
        }

        Texture *texture = context->getTargetTexture(target);
        QueryTexParameterfv(texture, pname, params);
        SetRobustLengthParam(length, numParams);
    }
}

ANGLE_EXPORT void GL_APIENTRY GetTexParameterivRobustANGLE(GLenum target,
                                                           GLenum pname,
                                                           GLsizei bufSize,
                                                           GLsizei *length,
                                                           GLint *params)
{
    EVENT(
        "(GLenum target = 0x%X, GLenum pname = 0x%X, GLsizei bufsize = %d, GLsizei* length = "
        "0x%0.8p, GLfloat* params = 0x%0.8p)",
        target, pname, bufSize, length, params);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        GLsizei numParams = 0;
        if (!ValidateGetTexParameterivRobustANGLE(context, target, pname, bufSize, &numParams,
                                                  params))
        {
            return;
        }

        Texture *texture = context->getTargetTexture(target);
        QueryTexParameteriv(texture, pname, params);
        SetRobustLengthParam(length, numParams);
    }
}

ANGLE_EXPORT void GL_APIENTRY GetUniformfvRobustANGLE(GLuint program,
                                                      GLint location,
                                                      GLsizei bufSize,
                                                      GLsizei *length,
                                                      GLfloat *params)
{
    EVENT(
        "(GLuint program = %d, GLint location = %d, GLsizei bufsize = %d, GLsizei* length = "
        "0x%0.8p, GLfloat* params = 0x%0.8p)",
        program, location, bufSize, length, params);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        GLsizei writeLength = 0;
        if (!ValidateGetUniformfvRobustANGLE(context, program, location, bufSize, &writeLength,
                                             params))
        {
            return;
        }

        Program *programObject = context->getProgram(program);
        ASSERT(programObject);

        programObject->getUniformfv(context, location, params);
        SetRobustLengthParam(length, writeLength);
    }
}

ANGLE_EXPORT void GL_APIENTRY GetUniformivRobustANGLE(GLuint program,
                                                      GLint location,
                                                      GLsizei bufSize,
                                                      GLsizei *length,
                                                      GLint *params)
{
    EVENT(
        "(GLuint program = %d, GLint location = %d, GLsizei bufsize = %d, GLsizei* length = "
        "0x%0.8p, GLint* params = 0x%0.8p)",
        program, location, bufSize, length, params);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        GLsizei writeLength = 0;
        if (!ValidateGetUniformivRobustANGLE(context, program, location, bufSize, &writeLength,
                                             params))
        {
            return;
        }

        Program *programObject = context->getProgram(program);
        ASSERT(programObject);

        programObject->getUniformiv(context, location, params);
        SetRobustLengthParam(length, writeLength);
    }
}

ANGLE_EXPORT void GL_APIENTRY GetVertexAttribfvRobustANGLE(GLuint index,
                                                           GLenum pname,
                                                           GLsizei bufSize,
                                                           GLsizei *length,
                                                           GLfloat *params)
{
    EVENT(
        "(GLuint index = %d, GLenum pname = 0x%X, GLsizei bufsize = %d, GLsizei* length = 0x%0.8p, "
        "GLfloat* params = 0x%0.8p)",
        index, pname, bufSize, length, params);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        GLsizei writeLength = 0;
        if (!ValidateGetVertexAttribfvRobustANGLE(context, index, pname, bufSize, &writeLength,
                                                  params))
        {
            return;
        }

        context->getVertexAttribfv(index, pname, params);
        SetRobustLengthParam(length, writeLength);
    }
}

ANGLE_EXPORT void GL_APIENTRY GetVertexAttribivRobustANGLE(GLuint index,
                                                           GLenum pname,
                                                           GLsizei bufSize,
                                                           GLsizei *length,
                                                           GLint *params)
{
    EVENT(
        "(GLuint index = %d, GLenum pname = 0x%X, GLsizei bufsize = %d, GLsizei* length = 0x%0.8p, "
        "GLint* params = 0x%0.8p)",
        index, pname, bufSize, length, params);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        GLsizei writeLength = 0;
        if (!ValidateGetVertexAttribivRobustANGLE(context, index, pname, bufSize, &writeLength,
                                                  params))
        {
            return;
        }

        context->getVertexAttribiv(index, pname, params);
        SetRobustLengthParam(length, writeLength);
    }
}

ANGLE_EXPORT void GL_APIENTRY GetVertexAttribPointervRobustANGLE(GLuint index,
                                                                 GLenum pname,
                                                                 GLsizei bufSize,
                                                                 GLsizei *length,
                                                                 void **pointer)
{
    EVENT(
        "(GLuint index = %d, GLenum pname = 0x%X, GLsizei bufsize = %d, GLsizei* length = 0x%0.8p, "
        "void** pointer = 0x%0.8p)",
        index, pname, bufSize, length, pointer);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        GLsizei writeLength = 0;
        if (!ValidateGetVertexAttribPointervRobustANGLE(context, index, pname, bufSize,
                                                        &writeLength, pointer))
        {
            return;
        }

        context->getVertexAttribPointerv(index, pname, pointer);
        SetRobustLengthParam(length, writeLength);
    }
}

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
                                                    void *pixels)
{
    EVENT(
        "(GLint x = %d, GLint y = %d, GLsizei width = %d, GLsizei height = %d, "
        "GLenum format = 0x%X, GLenum type = 0x%X, GLsizei bufsize = %d, GLsizei* length = "
        "0x%0.8p, GLsizei* columns = 0x%0.8p, GLsizei* rows = 0x%0.8p, void* pixels = 0x%0.8p)",
        x, y, width, height, format, type, bufSize, length, columns, rows, pixels);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        GLsizei writeLength  = 0;
        GLsizei writeColumns = 0;
        GLsizei writeRows    = 0;
        if (!ValidateReadPixelsRobustANGLE(context, x, y, width, height, format, type, bufSize,
                                           &writeLength, &writeColumns, &writeRows, pixels))
        {
            return;
        }

        context->readPixels(x, y, width, height, format, type, pixels);

        SetRobustLengthParam(length, writeLength);
        SetRobustLengthParam(columns, writeColumns);
        SetRobustLengthParam(rows, writeRows);
    }
}

ANGLE_EXPORT void GL_APIENTRY TexImage2DRobustANGLE(GLenum target,
                                                    GLint level,
                                                    GLint internalformat,
                                                    GLsizei width,
                                                    GLsizei height,
                                                    GLint border,
                                                    GLenum format,
                                                    GLenum type,
                                                    GLsizei bufSize,
                                                    const void *pixels)
{
    EVENT(
        "(GLenum target = 0x%X, GLint level = %d, GLint internalformat = %d, GLsizei width = %d, "
        "GLsizei height = %d, GLint border = %d, GLenum format = 0x%X, GLenum type = 0x%X, GLsizei "
        "bufSize = %d, const void* pixels = 0x%0.8p)",
        target, level, internalformat, width, height, border, format, type, bufSize, pixels);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!ValidateTexImage2DRobust(context, target, level, internalformat, width, height, border,
                                      format, type, bufSize, pixels))
        {
            return;
        }

        context->texImage2D(target, level, internalformat, width, height, border, format, type,
                            pixels);
    }
}

ANGLE_EXPORT void GL_APIENTRY TexParameterfvRobustANGLE(GLenum target,
                                                        GLenum pname,
                                                        GLsizei bufSize,
                                                        const GLfloat *params)
{
    EVENT(
        "(GLenum target = 0x%X, GLenum pname = 0x%X, GLsizei bufsize = %d, GLfloat* params = "
        "0x%0.8p)",
        target, pname, bufSize, params);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!ValidateTexParameterfvRobustANGLE(context, target, pname, bufSize, params))
        {
            return;
        }

        Texture *texture = context->getTargetTexture(target);
        SetTexParameterfv(context, texture, pname, params);
    }
}

ANGLE_EXPORT void GL_APIENTRY TexParameterivRobustANGLE(GLenum target,
                                                        GLenum pname,
                                                        GLsizei bufSize,
                                                        const GLint *params)
{
    EVENT(
        "(GLenum target = 0x%X, GLenum pname = 0x%X, GLsizei bufsize = %d, GLfloat* params = "
        "0x%0.8p)",
        target, pname, bufSize, params);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!ValidateTexParameterivRobustANGLE(context, target, pname, bufSize, params))
        {
            return;
        }

        Texture *texture = context->getTargetTexture(target);
        SetTexParameteriv(context, texture, pname, params);
    }
}

ANGLE_EXPORT void GL_APIENTRY TexSubImage2DRobustANGLE(GLenum target,
                                                       GLint level,
                                                       GLint xoffset,
                                                       GLint yoffset,
                                                       GLsizei width,
                                                       GLsizei height,
                                                       GLenum format,
                                                       GLenum type,
                                                       GLsizei bufSize,
                                                       const void *pixels)
{
    EVENT(
        "(GLenum target = 0x%X, GLint level = %d, GLint xoffset = %d, GLint yoffset = %d, "
        "GLsizei width = %d, GLsizei height = %d, GLenum format = 0x%X, GLenum type = 0x%X, "
        "GLsizei bufsize = %d, const void* pixels = 0x%0.8p)",
        target, level, xoffset, yoffset, width, height, format, type, bufSize, pixels);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!ValidateTexSubImage2DRobustANGLE(context, target, level, xoffset, yoffset, width,
                                              height, format, type, bufSize, pixels))
        {
            return;
        }

        context->texSubImage2D(target, level, xoffset, yoffset, width, height, format, type,
                               pixels);
    }
}

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
                                                    const void *pixels)
{
    EVENT(
        "(GLenum target = 0x%X, GLint level = %d, GLint internalformat = %d, GLsizei width = %d, "
        "GLsizei height = %d, GLsizei depth = %d, GLint border = %d, GLenum format = 0x%X, "
        "GLenum type = 0x%X, GLsizei bufsize = %d, const void* pixels = 0x%0.8p)",
        target, level, internalformat, width, height, depth, border, format, type, bufSize, pixels);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!ValidateTexImage3DRobustANGLE(context, target, level, internalformat, width, height,
                                           depth, border, format, type, bufSize, pixels))
        {
            return;
        }

        context->texImage3D(target, level, internalformat, width, height, depth, border, format,
                            type, pixels);
    }
}

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
                                                       const void *pixels)
{
    EVENT(
        "(GLenum target = 0x%X, GLint level = %d, GLint xoffset = %d, GLint yoffset = %d, "
        "GLint zoffset = %d, GLsizei width = %d, GLsizei height = %d, GLsizei depth = %d, "
        "GLenum format = 0x%X, GLenum type = 0x%X, GLsizei bufsize = %d, const void* pixels = "
        "0x%0.8p)",
        target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, bufSize,
        pixels);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!ValidateTexSubImage3DRobustANGLE(context, target, level, xoffset, yoffset, zoffset,
                                              width, height, depth, format, type, bufSize, pixels))
        {
            return;
        }

        context->texSubImage3D(target, level, xoffset, yoffset, zoffset, width, height, depth,
                               format, type, pixels);
    }
}

void GL_APIENTRY CompressedTexImage2DRobustANGLE(GLenum target,
                                                 GLint level,
                                                 GLenum internalformat,
                                                 GLsizei width,
                                                 GLsizei height,
                                                 GLint border,
                                                 GLsizei imageSize,
                                                 GLsizei dataSize,
                                                 const GLvoid *data)
{
    EVENT(
        "(GLenum target = 0x%X, GLint level = %d, GLenum internalformat = 0x%X, GLsizei width = "
        "%d, "
        "GLsizei height = %d, GLint border = %d, GLsizei imageSize = %d, GLsizei dataSize = %d, "
        "const GLvoid* data = 0x%0.8p)",
        target, level, internalformat, width, height, border, imageSize, dataSize, data);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() &&
            !ValidateCompressedTexImage2DRobustANGLE(context, target, level, internalformat, width,
                                                     height, border, imageSize, dataSize, data))
        {
            return;
        }

        context->compressedTexImage2D(target, level, internalformat, width, height, border,
                                      imageSize, data);
    }
}

void GL_APIENTRY CompressedTexSubImage2DRobustANGLE(GLenum target,
                                                    GLint level,
                                                    GLint xoffset,
                                                    GLint yoffset,
                                                    GLsizei width,
                                                    GLsizei height,
                                                    GLenum format,
                                                    GLsizei imageSize,
                                                    GLsizei dataSize,
                                                    const GLvoid *data)
{
    EVENT(
        "(GLenum target = 0x%X, GLint level = %d, GLint xoffset = %d, GLint yoffset = %d, "
        "GLsizei width = %d, GLsizei height = %d, GLenum format = 0x%X, "
        "GLsizei imageSize = %d, GLsizei dataSize = %d, const GLvoid* data = 0x%0.8p)",
        target, level, xoffset, yoffset, width, height, format, imageSize, dataSize, data);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateCompressedTexSubImage2DRobustANGLE(
                                              context, target, level, xoffset, yoffset, width,
                                              height, format, imageSize, dataSize, data))
        {
            return;
        }

        context->compressedTexSubImage2D(target, level, xoffset, yoffset, width, height, format,
                                         imageSize, data);
    }
}

void GL_APIENTRY CompressedTexImage3DRobustANGLE(GLenum target,
                                                 GLint level,
                                                 GLenum internalformat,
                                                 GLsizei width,
                                                 GLsizei height,
                                                 GLsizei depth,
                                                 GLint border,
                                                 GLsizei imageSize,
                                                 GLsizei dataSize,
                                                 const GLvoid *data)
{
    EVENT(
        "(GLenum target = 0x%X, GLint level = %d, GLenum internalformat = 0x%X, GLsizei width = "
        "%d, "
        "GLsizei height = %d, GLsizei depth = %d, GLint border = %d, GLsizei imageSize = %d, "
        "GLsizei dataSize = %d, const GLvoid* data = 0x%0.8p)",
        target, level, internalformat, width, height, depth, border, imageSize, dataSize, data);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateCompressedTexImage3DRobustANGLE(
                                              context, target, level, internalformat, width, height,
                                              depth, border, imageSize, dataSize, data))
        {
            return;
        }

        context->compressedTexImage3D(target, level, internalformat, width, height, depth, border,
                                      imageSize, data);
    }
}

void GL_APIENTRY CompressedTexSubImage3DRobustANGLE(GLenum target,
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
                                                    const GLvoid *data)
{
    EVENT(
        "(GLenum target = 0x%X, GLint level = %d, GLint xoffset = %d, GLint yoffset = %d, "
        "GLint zoffset = %d, GLsizei width = %d, GLsizei height = %d, GLsizei depth = %d, "
        "GLenum format = 0x%X, GLsizei imageSize = %d, GLsizei dataSize = %d, const GLvoid* data = "
        "0x%0.8p)",
        target, level, xoffset, yoffset, zoffset, width, height, depth, format, imageSize, dataSize,
        data);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() &&
            !ValidateCompressedTexSubImage3DRobustANGLE(context, target, level, xoffset, yoffset,
                                                        zoffset, width, height, depth, format,
                                                        imageSize, dataSize, data))
        {
            return;
        }

        context->compressedTexSubImage3D(target, level, xoffset, yoffset, zoffset, width, height,
                                         depth, format, imageSize, data);
    }
}

ANGLE_EXPORT void GL_APIENTRY
GetQueryivRobustANGLE(GLenum target, GLenum pname, GLsizei bufSize, GLsizei *length, GLint *params)
{
    EVENT(
        "(GLenum target = 0x%X, GLenum pname = 0x%X, GLsizei bufsize = %d, GLsizei* length = "
        "0x%0.8p, GLint* params = 0x%0.8p)",
        target, pname, bufSize, length, params);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        GLsizei numParams = 0;
        if (!ValidateGetQueryivRobustANGLE(context, target, pname, bufSize, &numParams, params))
        {
            return;
        }

        context->getQueryiv(target, pname, params);
        SetRobustLengthParam(length, numParams);
    }
}

ANGLE_EXPORT void GL_APIENTRY GetQueryObjectuivRobustANGLE(GLuint id,
                                                           GLenum pname,
                                                           GLsizei bufSize,
                                                           GLsizei *length,
                                                           GLuint *params)
{
    EVENT(
        "(GLuint id = %u, GLenum pname = 0x%X, GLsizei bufsize = %d, GLsizei* length = 0x%0.8p, "
        "GLint* params = 0x%0.8p)",
        id, pname, bufSize, length, params);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        GLsizei numParams = 0;
        if (!ValidateGetQueryObjectuivRobustANGLE(context, id, pname, bufSize, &numParams, params))
        {
            return;
        }

        context->getQueryObjectuiv(id, pname, params);
        SetRobustLengthParam(length, numParams);
    }
}

ANGLE_EXPORT void GL_APIENTRY GetBufferPointervRobustANGLE(GLenum target,
                                                           GLenum pname,
                                                           GLsizei bufSize,
                                                           GLsizei *length,
                                                           void **params)
{
    EVENT(
        "(GLenum target = 0x%X, GLenum pname = 0x%X,  GLsizei bufsize = %d, GLsizei* length = "
        "0x%0.8p, void** params = 0x%0.8p)",
        target, pname, bufSize, length, params);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        BufferBinding targetPacked = FromGLenum<BufferBinding>(target);

        GLsizei numParams = 0;
        if (!ValidateGetBufferPointervRobustANGLE(context, targetPacked, pname, bufSize, &numParams,
                                                  params))
        {
            return;
        }

        context->getBufferPointerv(targetPacked, pname, params);
        SetRobustLengthParam(length, numParams);
    }
}

ANGLE_EXPORT void GL_APIENTRY
GetIntegeri_vRobustANGLE(GLenum target, GLuint index, GLsizei bufSize, GLsizei *length, GLint *data)
{
    EVENT(
        "(GLenum target = 0x%X, GLuint index = %u, GLsizei bufsize = %d, GLsizei* length = "
        "0x%0.8p, GLint* data = 0x%0.8p)",
        target, index, bufSize, length, data);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        GLsizei numParams = 0;
        if (!ValidateGetIntegeri_vRobustANGLE(context, target, index, bufSize, &numParams, data))
        {
            return;
        }

        context->getIntegeri_v(target, index, data);
        SetRobustLengthParam(length, numParams);
    }
}

ANGLE_EXPORT void GL_APIENTRY GetInternalformativRobustANGLE(GLenum target,
                                                             GLenum internalformat,
                                                             GLenum pname,
                                                             GLsizei bufSize,
                                                             GLsizei *length,
                                                             GLint *params)
{
    EVENT(
        "(GLenum target = 0x%X, GLenum internalformat = 0x%X, GLenum pname = 0x%X, GLsizei bufSize "
        "= %d, GLsizei* length = 0x%0.8p, GLint* params = 0x%0.8p)",
        target, internalformat, pname, bufSize, length, params);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        GLsizei numParams = 0;
        if (!ValidateGetInternalFormativRobustANGLE(context, target, internalformat, pname, bufSize,
                                                    &numParams, params))
        {
            return;
        }

        const TextureCaps &formatCaps = context->getTextureCaps().get(internalformat);
        QueryInternalFormativ(formatCaps, pname, bufSize, params);
        SetRobustLengthParam(length, numParams);
    }
}

ANGLE_EXPORT void GL_APIENTRY GetVertexAttribIivRobustANGLE(GLuint index,
                                                            GLenum pname,
                                                            GLsizei bufSize,
                                                            GLsizei *length,
                                                            GLint *params)
{
    EVENT(
        "(GLuint index = %u, GLenum pname = 0x%X, GLsizei bufsize = %d, GLsizei* length = 0x%0.8p, "
        "GLint* params = 0x%0.8p)",
        index, pname, bufSize, length, params);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        GLsizei writeLength = 0;
        if (!ValidateGetVertexAttribIivRobustANGLE(context, index, pname, bufSize, &writeLength,
                                                   params))
        {
            return;
        }

        context->getVertexAttribIiv(index, pname, params);
        SetRobustLengthParam(length, writeLength);
    }
}

ANGLE_EXPORT void GL_APIENTRY GetVertexAttribIuivRobustANGLE(GLuint index,
                                                             GLenum pname,
                                                             GLsizei bufSize,
                                                             GLsizei *length,
                                                             GLuint *params)
{
    EVENT(
        "(GLuint index = %u, GLenum pname = 0x%X, GLsizei bufsize = %d, GLsizei* length = 0x%0.8p, "
        "GLuint* params = 0x%0.8p)",
        index, pname, bufSize, length, params);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        GLsizei writeLength = 0;
        if (!ValidateGetVertexAttribIuivRobustANGLE(context, index, pname, bufSize, &writeLength,
                                                    params))
        {
            return;
        }

        context->getVertexAttribIuiv(index, pname, params);
        SetRobustLengthParam(length, writeLength);
    }
}

ANGLE_EXPORT void GL_APIENTRY GetUniformuivRobustANGLE(GLuint program,
                                                       GLint location,
                                                       GLsizei bufSize,
                                                       GLsizei *length,
                                                       GLuint *params)
{
    EVENT(
        "(GLuint program = %u, GLint location = %d, GLsizei bufsize = %d, GLsizei* length = "
        "0x%0.8p, GLuint* params = 0x%0.8p)",
        program, location, bufSize, length, params);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        GLsizei writeLength = 0;
        if (!ValidateGetUniformuivRobustANGLE(context, program, location, bufSize, &writeLength,
                                              params))
        {
            return;
        }

        Program *programObject = context->getProgram(program);
        ASSERT(programObject);

        programObject->getUniformuiv(context, location, params);
        SetRobustLengthParam(length, writeLength);
    }
}

ANGLE_EXPORT void GL_APIENTRY GetActiveUniformBlockivRobustANGLE(GLuint program,
                                                                 GLuint uniformBlockIndex,
                                                                 GLenum pname,
                                                                 GLsizei bufSize,
                                                                 GLsizei *length,
                                                                 GLint *params)
{
    EVENT(
        "(GLuint program = %u, GLuint uniformBlockIndex = %u, GLenum pname = 0x%X, GLsizei bufsize "
        "= %d, GLsizei* length = 0x%0.8p, GLint* params = 0x%0.8p)",
        program, uniformBlockIndex, pname, bufSize, length, params);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        GLsizei writeLength = 0;
        if (!ValidateGetActiveUniformBlockivRobustANGLE(context, program, uniformBlockIndex, pname,
                                                        bufSize, &writeLength, params))
        {
            return;
        }

        const Program *programObject = context->getProgram(program);
        QueryActiveUniformBlockiv(programObject, uniformBlockIndex, pname, params);
        SetRobustLengthParam(length, writeLength);
    }
}

ANGLE_EXPORT void GL_APIENTRY GetInteger64vRobustANGLE(GLenum pname,
                                                       GLsizei bufSize,
                                                       GLsizei *length,
                                                       GLint64 *data)
{
    EVENT(
        "(GLenum pname = 0x%X, GLsizei bufsize = %d, GLsizei* length = 0x%0.8p, GLint64* params = "
        "0x%0.8p)",
        pname, bufSize, length, data);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        GLenum nativeType;
        unsigned int numParams = 0;
        if (!ValidateRobustStateQuery(context, pname, bufSize, &nativeType, &numParams))
        {
            return;
        }

        if (nativeType == GL_INT_64_ANGLEX)
        {
            context->getInteger64v(pname, data);
        }
        else
        {
            CastStateValues(context, nativeType, pname, numParams, data);
        }
        SetRobustLengthParam(length, numParams);
    }
}

ANGLE_EXPORT void GL_APIENTRY GetInteger64i_vRobustANGLE(GLenum target,
                                                         GLuint index,
                                                         GLsizei bufSize,
                                                         GLsizei *length,
                                                         GLint64 *data)
{
    EVENT(
        "(GLenum target = 0x%X, GLuint index = %u, GLsizei bufsize = %d, GLsizei* length = "
        "0x%0.8p, GLint64* data = 0x%0.8p)",
        target, index, bufSize, length, data);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        GLsizei numParams = 0;
        if (!ValidateGetInteger64i_vRobustANGLE(context, target, index, bufSize, &numParams, data))
        {
            return;
        }

        context->getInteger64i_v(target, index, data);
        SetRobustLengthParam(length, numParams);
    }
}

ANGLE_EXPORT void GL_APIENTRY GetBufferParameteri64vRobustANGLE(GLenum target,
                                                                GLenum pname,
                                                                GLsizei bufSize,
                                                                GLsizei *length,
                                                                GLint64 *params)
{
    EVENT("(GLenum target = 0x%X, GLenum pname = 0x%X, GLint64* params = 0x%0.8p)", target, pname,
          bufSize, length, params);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        BufferBinding targetPacked = FromGLenum<BufferBinding>(target);

        GLsizei numParams = 0;
        if (!ValidateGetBufferParameteri64vRobustANGLE(context, targetPacked, pname, bufSize,
                                                       &numParams, params))
        {
            return;
        }

        Buffer *buffer = context->getGLState().getTargetBuffer(targetPacked);
        QueryBufferParameteri64v(buffer, pname, params);
        SetRobustLengthParam(length, numParams);
    }
}

ANGLE_EXPORT void GL_APIENTRY SamplerParameterivRobustANGLE(GLuint sampler,
                                                            GLenum pname,
                                                            GLsizei bufSize,
                                                            const GLint *param)
{
    EVENT(
        "(GLuint sampler = %u, GLenum pname = 0x%X, GLsizei bufsize = %d, const GLint* params = "
        "0x%0.8p)",
        sampler, pname, bufSize, param);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!ValidateSamplerParameterivRobustANGLE(context, sampler, pname, bufSize, param))
        {
            return;
        }

        context->samplerParameteriv(sampler, pname, param);
    }
}

ANGLE_EXPORT void GL_APIENTRY SamplerParameterfvRobustANGLE(GLuint sampler,
                                                            GLenum pname,
                                                            GLsizei bufSize,
                                                            const GLfloat *param)
{
    EVENT(
        "(GLuint sampler = %u, GLenum pname = 0x%X, GLsizei bufsize = %d, const GLfloat* params = "
        "0x%0.8p)",
        sampler, pname, bufSize, param);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!ValidateSamplerParameterfvRobustANGLE(context, sampler, pname, bufSize, param))
        {
            return;
        }

        context->samplerParameterfv(sampler, pname, param);
    }
}

ANGLE_EXPORT void GL_APIENTRY GetSamplerParameterivRobustANGLE(GLuint sampler,
                                                               GLenum pname,
                                                               GLsizei bufSize,
                                                               GLsizei *length,
                                                               GLint *params)
{
    EVENT(
        "(GLuint sampler = %u, GLenum pname = 0x%X, GLsizei bufsize = %d, GLsizei* length = "
        "0x%0.8p, GLint* params = 0x%0.8p)",
        sampler, pname, bufSize, length, params);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        GLsizei numParams = 0;
        if (!ValidateGetSamplerParameterivRobustANGLE(context, sampler, pname, bufSize, &numParams,
                                                      params))
        {
            return;
        }

        context->getSamplerParameteriv(sampler, pname, params);
        SetRobustLengthParam(length, numParams);
    }
}

ANGLE_EXPORT void GL_APIENTRY GetSamplerParameterfvRobustANGLE(GLuint sampler,
                                                               GLenum pname,
                                                               GLsizei bufSize,
                                                               GLsizei *length,
                                                               GLfloat *params)
{
    EVENT(
        "(GLuint sample = %ur, GLenum pname = 0x%X, GLsizei bufsize = %d, GLsizei* length = "
        "0x%0.8p, GLfloat* params = 0x%0.8p)",
        sampler, pname, bufSize, length, params);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        GLsizei numParams = 0;
        if (!ValidateGetSamplerParameterfvRobustANGLE(context, sampler, pname, bufSize, &numParams,
                                                      params))
        {
            return;
        }

        context->getSamplerParameterfv(sampler, pname, params);
        SetRobustLengthParam(length, numParams);
    }
}

ANGLE_EXPORT void GL_APIENTRY GetFramebufferParameterivRobustANGLE(GLenum target,
                                                                   GLenum pname,
                                                                   GLsizei bufSize,
                                                                   GLsizei *length,
                                                                   GLint *params)
{
    EVENT(
        "(GLenum target = 0x%X, GLenum pname = 0x%X, GLsizei bufsize = %d, GLsizei* length = "
        "0x%0.8p, GLint* params = 0x%0.8p)",
        target, pname, bufSize, length, params);
    UNIMPLEMENTED();
}

ANGLE_EXPORT void GL_APIENTRY GetProgramInterfaceivRobustANGLE(GLuint program,
                                                               GLenum programInterface,
                                                               GLenum pname,
                                                               GLsizei bufSize,
                                                               GLsizei *length,
                                                               GLint *params)
{
    EVENT(
        "(GLuint program = %u, GLenum programInterface = 0x%X, GLenum pname = 0x%X, GLsizei "
        "bufsize = %d, GLsizei* length = 0x%0.8p, GLint* params = 0x%0.8p)",
        program, programInterface, pname, bufSize, length, params);
    UNIMPLEMENTED();
}

ANGLE_EXPORT void GL_APIENTRY GetBooleani_vRobustANGLE(GLenum target,
                                                       GLuint index,
                                                       GLsizei bufSize,
                                                       GLsizei *length,
                                                       GLboolean *data)
{
    EVENT(
        "(GLenum target = 0x%X, GLuint index = %u, GLsizei bufsize = %d, GLsizei* length = "
        "0x%0.8p, GLboolean* data = 0x%0.8p)",
        target, index, bufSize, length, data);
    Context *context = GetValidGlobalContext();
    if (context)
    {
        GLsizei numParams = 0;
        if (!ValidateGetBooleani_vRobustANGLE(context, target, index, bufSize, &numParams, data))
        {
            return;
        }

        context->getBooleani_v(target, index, data);
        SetRobustLengthParam(length, numParams);
    }
}

ANGLE_EXPORT void GL_APIENTRY GetMultisamplefvRobustANGLE(GLenum pname,
                                                          GLuint index,
                                                          GLsizei bufSize,
                                                          GLsizei *length,
                                                          GLfloat *val)
{
    EVENT(
        "(GLenum pname = 0x%X, GLuint index = %u, GLsizei bufsize = %d, GLsizei* length = 0x%0.8p, "
        "GLfloat* val = 0x%0.8p)",
        pname, index, bufSize, length, val);
    UNIMPLEMENTED();
}

ANGLE_EXPORT void GL_APIENTRY GetTexLevelParameterivRobustANGLE(GLenum target,
                                                                GLint level,
                                                                GLenum pname,
                                                                GLsizei bufSize,
                                                                GLsizei *length,
                                                                GLint *params)
{
    EVENT(
        "(GLenum target = 0x%X, GLint level = %d, GLenum pname = 0x%X, GLsizei bufsize = %d, "
        "GLsizei* length = 0x%0.8p, GLint* params = 0x%0.8p)",
        target, level, pname, bufSize, length, params);
    UNIMPLEMENTED();
}

ANGLE_EXPORT void GL_APIENTRY GetTexLevelParameterfvRobustANGLE(GLenum target,
                                                                GLint level,
                                                                GLenum pname,
                                                                GLsizei bufSize,
                                                                GLsizei *length,
                                                                GLfloat *params)
{
    EVENT(
        "(GLenum target = 0x%X, GLint level = %d, GLenum pname = 0x%X, GLsizei bufsize = %d, "
        "GLsizei* length = 0x%0.8p, GLfloat* params = 0x%0.8p)",
        target, level, pname, bufSize, length, params);
    UNIMPLEMENTED();
}

ANGLE_EXPORT void GL_APIENTRY GetPointervRobustANGLERobustANGLE(GLenum pname,
                                                                GLsizei bufSize,
                                                                GLsizei *length,
                                                                void **params)
{
    EVENT(
        "(GLenum pname = 0x%X, GLsizei bufsize = %d, GLsizei* length = 0x%0.8p, void **params = "
        "0x%0.8p)",
        pname, bufSize, length, params);
    UNIMPLEMENTED();
}

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
                                                     void *data)
{
    EVENT(
        "(GLint x = %d, GLint y = %d, GLsizei width = %d, GLsizei height = %d, "
        "GLenum format = 0x%X, GLenum type = 0x%X, GLsizei bufsize = %d, GLsizei* length = "
        "0x%0.8p, GLsizei* columns = 0x%0.8p, GLsizei* rows = 0x%0.8p, void *data = 0x%0.8p)",
        x, y, width, height, format, type, bufSize, length, columns, rows, data);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        GLsizei writeLength  = 0;
        GLsizei writeColumns = 0;
        GLsizei writeRows    = 0;
        if (!ValidateReadnPixelsRobustANGLE(context, x, y, width, height, format, type, bufSize,
                                            &writeLength, &writeColumns, &writeRows, data))
        {
            return;
        }

        context->readPixels(x, y, width, height, format, type, data);

        SetRobustLengthParam(length, writeLength);
        SetRobustLengthParam(columns, writeColumns);
        SetRobustLengthParam(rows, writeRows);
    }
}

ANGLE_EXPORT void GL_APIENTRY GetnUniformfvRobustANGLE(GLuint program,
                                                       GLint location,
                                                       GLsizei bufSize,
                                                       GLsizei *length,
                                                       GLfloat *params)
{
    EVENT(
        "(GLuint program = %d, GLint location = %d, GLsizei bufsize = %d, GLsizei* length = "
        "0x%0.8p, GLfloat* params = 0x%0.8p)",
        program, location, bufSize, length, params);
    UNIMPLEMENTED();
}

ANGLE_EXPORT void GL_APIENTRY GetnUniformivRobustANGLE(GLuint program,
                                                       GLint location,
                                                       GLsizei bufSize,
                                                       GLsizei *length,
                                                       GLint *params)
{
    EVENT(
        "(GLuint program = %d, GLint location = %d, GLsizei bufsize = %d, GLsizei* length = "
        "0x%0.8p, GLint* params = 0x%0.8p)",
        program, location, bufSize, length, params);
    UNIMPLEMENTED();
}

ANGLE_EXPORT void GL_APIENTRY GetnUniformuivRobustANGLE(GLuint program,
                                                        GLint location,
                                                        GLsizei bufSize,
                                                        GLsizei *length,
                                                        GLuint *params)
{
    EVENT(
        "(GLuint program = %u, GLint location = %d, GLsizei bufsize = %d, GLsizei* length = "
        "0x%0.8p, GLuint* params = 0x%0.8p)",
        program, location, bufSize, length, params);
    UNIMPLEMENTED();
}

ANGLE_EXPORT void GL_APIENTRY TexParameterIivRobustANGLE(GLenum target,
                                                         GLenum pname,
                                                         GLsizei bufSize,
                                                         const GLint *params)
{
    EVENT(
        "(GLenum target = 0x%X, GLenum pname = 0x%X, GLsizei bufsize = %d, const GLint *params = "
        "0x%0.8p)",
        target, pname, bufSize, params);
    UNIMPLEMENTED();
}

ANGLE_EXPORT void GL_APIENTRY TexParameterIuivRobustANGLE(GLenum target,
                                                          GLenum pname,
                                                          GLsizei bufSize,
                                                          const GLuint *params)
{
    EVENT(
        "(GLenum target = 0x%X, GLenum pname = 0x%X, GLsizei bufsize = %d, const GLuint *params = "
        "0x%0.8p)",
        target, pname, bufSize, params);
    UNIMPLEMENTED();
}

ANGLE_EXPORT void GL_APIENTRY GetTexParameterIivRobustANGLE(GLenum target,
                                                            GLenum pname,
                                                            GLsizei bufSize,
                                                            GLsizei *length,
                                                            GLint *params)
{
    EVENT(
        "(GLenum target = 0x%X, GLenum pname = 0x%X, GLsizei bufsize = %d, GLsizei* length = "
        "0x%0.8p, GLint *params = 0x%0.8p)",
        target, pname, bufSize, length, params);
    UNIMPLEMENTED();
}

ANGLE_EXPORT void GL_APIENTRY GetTexParameterIuivRobustANGLE(GLenum target,
                                                             GLenum pname,
                                                             GLsizei bufSize,
                                                             GLsizei *length,
                                                             GLuint *params)
{
    EVENT(
        "(GLenum target = 0x%X, GLenum pname = 0x%X, GLsizei bufsize = %d, GLsizei* length = "
        "0x%0.8p, GLuint *params = 0x%0.8p)",
        target, pname, bufSize, length, params);
    UNIMPLEMENTED();
}

ANGLE_EXPORT void GL_APIENTRY SamplerParameterIivRobustANGLE(GLuint sampler,
                                                             GLenum pname,
                                                             GLsizei bufSize,
                                                             const GLint *param)
{
    EVENT(
        "(GLuint sampler = %d, GLenum pname = 0x%X, GLsizei bufsize = %d, const GLint *param = "
        "0x%0.8p)",
        sampler, pname, bufSize, param);
    UNIMPLEMENTED();
}

ANGLE_EXPORT void GL_APIENTRY SamplerParameterIuivRobustANGLE(GLuint sampler,
                                                              GLenum pname,
                                                              GLsizei bufSize,
                                                              const GLuint *param)
{
    EVENT(
        "(GLuint sampler = %d, GLenum pname = 0x%X, GLsizei bufsize = %d, const GLuint *param = "
        "0x%0.8p)",
        sampler, pname, bufSize, param);
    UNIMPLEMENTED();
}

ANGLE_EXPORT void GL_APIENTRY GetSamplerParameterIivRobustANGLE(GLuint sampler,
                                                                GLenum pname,
                                                                GLsizei bufSize,
                                                                GLsizei *length,
                                                                GLint *params)
{
    EVENT(
        "(GLuint sampler = %d, GLenum pname = 0x%X, GLsizei bufsize = %d, GLsizei* length = "
        "0x%0.8p, GLint *params = 0x%0.8p)",
        sampler, pname, bufSize, length, params);
    UNIMPLEMENTED();
}

ANGLE_EXPORT void GL_APIENTRY GetSamplerParameterIuivRobustANGLE(GLuint sampler,
                                                                 GLenum pname,
                                                                 GLsizei bufSize,
                                                                 GLsizei *length,
                                                                 GLuint *params)
{
    EVENT(
        "(GLuint sampler = %d, GLenum pname = 0x%X, GLsizei bufsize = %d, GLsizei* length = "
        "0x%0.8p, GLuint *params = 0x%0.8p)",
        sampler, pname, bufSize, length, params);
    UNIMPLEMENTED();
}

ANGLE_EXPORT void GL_APIENTRY GetQueryObjectivRobustANGLE(GLuint id,
                                                          GLenum pname,
                                                          GLsizei bufSize,
                                                          GLsizei *length,
                                                          GLint *params)
{
    EVENT(
        "(GLuint id = %d, GLenum pname = 0x%X, GLsizei bufsize = %d, GLsizei* length = 0x%0.8p, "
        "GLuint *params = 0x%0.8p)",
        id, pname, bufSize, length, params);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        GLsizei numParams = 0;
        if (!ValidateGetQueryObjectivRobustANGLE(context, id, pname, bufSize, &numParams, params))
        {
            return;
        }

        context->getQueryObjectiv(id, pname, params);
        SetRobustLengthParam(length, numParams);
    }
}

ANGLE_EXPORT void GL_APIENTRY GetQueryObjecti64vRobustANGLE(GLuint id,
                                                            GLenum pname,
                                                            GLsizei bufSize,
                                                            GLsizei *length,
                                                            GLint64 *params)
{
    EVENT(
        "(GLuint id = %d, GLenum pname = 0x%X, GLsizei bufsize = %d, GLsizei* length = 0x%0.8p, "
        "GLint64 *params = 0x%0.8p)",
        id, pname, bufSize, length, params);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        GLsizei numParams = 0;
        if (!ValidateGetQueryObjecti64vRobustANGLE(context, id, pname, bufSize, &numParams, params))
        {
            return;
        }

        context->getQueryObjecti64v(id, pname, params);
        SetRobustLengthParam(length, numParams);
    }
}

ANGLE_EXPORT void GL_APIENTRY GetQueryObjectui64vRobustANGLE(GLuint id,
                                                             GLenum pname,
                                                             GLsizei bufSize,
                                                             GLsizei *length,
                                                             GLuint64 *params)
{
    EVENT(
        "(GLuint id = %d, GLenum pname = 0x%X, GLsizei bufsize = %d, GLsizei* length = 0x%0.8p, "
        "GLuint64 *params = 0x%0.8p)",
        id, pname, bufSize, length, params);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        GLsizei numParams = 0;
        if (!ValidateGetQueryObjectui64vRobustANGLE(context, id, pname, bufSize, &numParams,
                                                    params))
        {
            return;
        }

        context->getQueryObjectui64v(id, pname, params);
        SetRobustLengthParam(length, numParams);
    }
}

GL_APICALL void GL_APIENTRY FramebufferTextureMultiviewLayeredANGLE(GLenum target,
                                                                    GLenum attachment,
                                                                    GLuint texture,
                                                                    GLint level,
                                                                    GLint baseViewIndex,
                                                                    GLsizei numViews)
{
    EVENT(
        "(GLenum target = 0x%X, GLenum attachment = 0x%X, GLuint texture = %u, GLint level = %d, "
        "GLint baseViewIndex = %d, GLsizei numViews = %d)",
        target, attachment, texture, level, baseViewIndex, numViews);
    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() &&
            !ValidateFramebufferTextureMultiviewLayeredANGLE(context, target, attachment, texture,
                                                             level, baseViewIndex, numViews))
        {
            return;
        }
        context->framebufferTextureMultiviewLayeredANGLE(target, attachment, texture, level,
                                                         baseViewIndex, numViews);
    }
}

GL_APICALL void GL_APIENTRY FramebufferTextureMultiviewSideBySideANGLE(GLenum target,
                                                                       GLenum attachment,
                                                                       GLuint texture,
                                                                       GLint level,
                                                                       GLsizei numViews,
                                                                       const GLint *viewportOffsets)
{
    EVENT(
        "(GLenum target = 0x%X, GLenum attachment = 0x%X, GLuint texture = %u, GLint level = %d, "
        "GLsizei numViews = %d, GLsizei* viewportOffsets = 0x%0.8p)",
        target, attachment, texture, level, numViews, viewportOffsets);
    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() &&
            !ValidateFramebufferTextureMultiviewSideBySideANGLE(
                context, target, attachment, texture, level, numViews, viewportOffsets))
        {
            return;
        }
        context->framebufferTextureMultiviewSideBySideANGLE(target, attachment, texture, level,
                                                            numViews, viewportOffsets);
    }
}

}  // gl
