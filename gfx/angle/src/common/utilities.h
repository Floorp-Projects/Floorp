//
// Copyright (c) 2002-2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// utilities.h: Conversion functions and other utility routines.

#ifndef COMMON_UTILITIES_H_
#define COMMON_UTILITIES_H_

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include "angle_gl.h"
#include <string>
#include <math.h>

#include "common/mathutil.h"

namespace gl
{

int VariableComponentCount(GLenum type);
GLenum VariableComponentType(GLenum type);
size_t VariableComponentSize(GLenum type);
size_t VariableInternalSize(GLenum type);
size_t VariableExternalSize(GLenum type);
int VariableRowCount(GLenum type);
int VariableColumnCount(GLenum type);
bool IsSamplerType(GLenum type);
bool IsImageType(GLenum type);
bool IsAtomicCounterType(GLenum type);
bool IsOpaqueType(GLenum type);
GLenum SamplerTypeToTextureType(GLenum samplerType);
bool IsMatrixType(GLenum type);
GLenum TransposeMatrixType(GLenum type);
int VariableRegisterCount(GLenum type);
int MatrixRegisterCount(GLenum type, bool isRowMajorMatrix);
int MatrixComponentCount(GLenum type, bool isRowMajorMatrix);
int VariableSortOrder(GLenum type);
GLenum VariableBoolVectorType(GLenum type);

int AllocateFirstFreeBits(unsigned int *bits, unsigned int allocationSize, unsigned int bitsSize);

static const GLenum FirstCubeMapTextureTarget = GL_TEXTURE_CUBE_MAP_POSITIVE_X;
static const GLenum LastCubeMapTextureTarget = GL_TEXTURE_CUBE_MAP_NEGATIVE_Z;
bool IsCubeMapTextureTarget(GLenum target);
size_t CubeMapTextureTargetToLayerIndex(GLenum target);
GLenum LayerIndexToCubeMapTextureTarget(size_t index);

// Parse the base resource name and array index.  Returns the base name of the resource.
// outSubscript is set to GL_INVALID_INDEX if the provided name is not an array or the array index
// is invalid.
std::string ParseResourceName(const std::string &name, size_t *outSubscript);

// Find the range of index values in the provided indices pointer.  Primitive restart indices are
// only counted in the range if primitive restart is disabled.
IndexRange ComputeIndexRange(GLenum indexType,
                             const GLvoid *indices,
                             size_t count,
                             bool primitiveRestartEnabled);

// Get the primitive restart index value for the given index type.
GLuint GetPrimitiveRestartIndex(GLenum indexType);

bool IsTriangleMode(GLenum drawMode);
bool IsIntegerFormat(GLenum unsizedFormat);

// [OpenGL ES 3.0.2] Section 2.3.1 page 14
// Data Conversion For State-Setting Commands
// Floating-point values are rounded to the nearest integer, instead of truncated, as done by static_cast.
template <typename outT> outT iround(GLfloat value) { return static_cast<outT>(value > 0.0f ? floor(value + 0.5f) : ceil(value - 0.5f)); }
template <typename outT> outT uiround(GLfloat value) { return static_cast<outT>(value + 0.5f); }

// Helper for converting arbitrary GL types to other GL types used in queries and state setting

// TODO(jie.a.chen@intel.com): Add the conversion rule for all helpers as the spec requires:
// "If a value is so large in magnitude that it cannot be represented with the requested type,"
// "then the nearest value representable using the requested type is returned."

template <typename ParamType>
GLuint ConvertToGLuint(ParamType param)
{
    return static_cast<GLuint>(param);
}
template <>
GLuint ConvertToGLuint(GLfloat param);

template <typename ParamType>
GLint ConvertToGLint(ParamType param)
{
    return static_cast<GLint>(param);
}

template <>
GLint ConvertToGLint(uint32_t param);

template <>
GLint ConvertToGLint(uint64_t param);

template <>
GLint ConvertToGLint(GLfloat param);

// Same conversion as uint
template <typename ParamType>
GLenum ConvertToGLenum(ParamType param)
{
    return static_cast<GLenum>(ConvertToGLuint(param));
}

template <typename ParamType>
GLfloat ConvertToGLfloat(ParamType param)
{
    return static_cast<GLfloat>(param);
}

template <typename ParamType>
ParamType ConvertFromGLfloat(GLfloat param)
{
    return static_cast<ParamType>(param);
}
template <>
GLint ConvertFromGLfloat(GLfloat param);
template <>
GLuint ConvertFromGLfloat(GLfloat param);

template <typename ParamType>
ParamType ConvertFromGLenum(GLenum param)
{
    return static_cast<ParamType>(param);
}

template <typename ParamType>
ParamType ConvertFromGLuint(GLuint param)
{
    return static_cast<ParamType>(param);
}

template <typename ParamType>
ParamType ConvertFromGLint(GLint param)
{
    return static_cast<ParamType>(param);
}

template <typename ParamType>
ParamType ConvertFromGLboolean(GLboolean param)
{
    return static_cast<ParamType>(param ? GL_TRUE : GL_FALSE);
}

template <typename ParamType>
ParamType ConvertFromGLint64(GLint64 param)
{
    return clampCast<ParamType>(param);
}

unsigned int ParseAndStripArrayIndex(std::string *name);

struct UniformTypeInfo final : angle::NonCopyable
{
    constexpr UniformTypeInfo(GLenum type,
                              GLenum componentType,
                              GLenum samplerTextureType,
                              GLenum transposedMatrixType,
                              GLenum boolVectorType,
                              int rowCount,
                              int columnCount,
                              int componentCount,
                              size_t componentSize,
                              size_t internalSize,
                              size_t externalSize,
                              bool isSampler,
                              bool isMatrixType,
                              bool isImageType)
        : type(type),
          componentType(componentType),
          samplerTextureType(samplerTextureType),
          transposedMatrixType(transposedMatrixType),
          boolVectorType(boolVectorType),
          rowCount(rowCount),
          columnCount(columnCount),
          componentCount(componentCount),
          componentSize(componentSize),
          internalSize(internalSize),
          externalSize(externalSize),
          isSampler(isSampler),
          isMatrixType(isMatrixType),
          isImageType(isImageType)
    {
    }

    GLenum type;
    GLenum componentType;
    GLenum samplerTextureType;
    GLenum transposedMatrixType;
    GLenum boolVectorType;
    int rowCount;
    int columnCount;
    int componentCount;
    size_t componentSize;
    size_t internalSize;
    size_t externalSize;
    bool isSampler;
    bool isMatrixType;
    bool isImageType;
};

const UniformTypeInfo &GetUniformTypeInfo(GLenum uniformType);

}  // namespace gl

namespace egl
{
static const EGLenum FirstCubeMapTextureTarget = EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_X_KHR;
static const EGLenum LastCubeMapTextureTarget = EGL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Z_KHR;
bool IsCubeMapTextureTarget(EGLenum target);
size_t CubeMapTextureTargetToLayerIndex(EGLenum target);
EGLenum LayerIndexToCubeMapTextureTarget(size_t index);
bool IsTextureTarget(EGLenum target);
bool IsRenderbufferTarget(EGLenum target);
}

namespace egl_gl
{
GLenum EGLCubeMapTargetToGLCubeMapTarget(EGLenum eglTarget);
GLenum EGLImageTargetToGLTextureTarget(EGLenum eglTarget);
GLuint EGLClientBufferToGLObjectHandle(EGLClientBuffer buffer);
}

namespace gl_egl
{
EGLenum GLComponentTypeToEGLColorComponentType(GLenum glComponentType);
}  // namespace gl_egl

#if !defined(ANGLE_ENABLE_WINDOWS_STORE)
std::string getTempPath();
void writeFile(const char* path, const void* data, size_t size);
#endif

#if defined (ANGLE_PLATFORM_WINDOWS)
void ScheduleYield();
#endif

#endif  // COMMON_UTILITIES_H_
