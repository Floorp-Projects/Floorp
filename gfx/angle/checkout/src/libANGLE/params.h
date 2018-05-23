//
// Copyright 2017 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// params:
//   Parameter wrapper structs for OpenGL ES. These helpers cache re-used values
//   in entry point routines.

#ifndef LIBANGLE_PARAMS_H_
#define LIBANGLE_PARAMS_H_

#include "angle_gl.h"
#include "common/Optional.h"
#include "common/angleutils.h"
#include "common/mathutil.h"
#include "libANGLE/Error.h"
#include "libANGLE/entry_points_enum_autogen.h"

namespace gl
{
class Context;

template <EntryPoint EP>
struct EntryPointParam;

template <EntryPoint EP>
using EntryPointParamType = typename EntryPointParam<EP>::Type;

class ParamTypeInfo
{
  public:
    constexpr ParamTypeInfo(const char *selfClass, const ParamTypeInfo *parentType)
        : mSelfClass(selfClass), mParentTypeInfo(parentType)
    {
    }

    constexpr bool hasDynamicType(const ParamTypeInfo &typeInfo) const
    {
        return mSelfClass == typeInfo.mSelfClass ||
               (mParentTypeInfo && mParentTypeInfo->hasDynamicType(typeInfo));
    }

    constexpr bool isValid() const { return mSelfClass != nullptr; }

  private:
    const char *mSelfClass;
    const ParamTypeInfo *mParentTypeInfo;
};

#define ANGLE_PARAM_TYPE_INFO(NAME, BASENAME) \
    static constexpr ParamTypeInfo TypeInfo = {#NAME, &BASENAME::TypeInfo}

class ParamsBase : angle::NonCopyable
{
  public:
    ParamsBase(Context *context, ...){};

    template <EntryPoint EP, typename... ArgsT>
    static void Factory(EntryPointParamType<EP> *objBuffer, ArgsT... args);

    static constexpr ParamTypeInfo TypeInfo = {nullptr, nullptr};
};

// static
template <EntryPoint EP, typename... ArgsT>
ANGLE_INLINE void ParamsBase::Factory(EntryPointParamType<EP> *objBuffer, ArgsT... args)
{
    new (objBuffer) EntryPointParamType<EP>(args...);
}

// Helper class that encompasses draw call parameters. It uses the HasIndexRange
// helper class to only pull index range info lazily to prevent unnecessary readback.
// It is also used when syncing state for the VertexArray implementation, since the
// vertex and index buffer updates depend on draw call parameters.
class DrawCallParams final : angle::NonCopyable
{
  public:
    // Called by DrawArrays.
    DrawCallParams(GLenum mode, GLint firstVertex, GLsizei vertexCount, GLsizei instances);

    // Called by DrawElements.
    DrawCallParams(GLenum mode,
                   GLint indexCount,
                   GLenum type,
                   const void *indices,
                   GLint baseVertex,
                   GLsizei instances);

    // Called by DrawArraysIndirect.
    DrawCallParams(GLenum mode, const void *indirect);

    // Called by DrawElementsIndirect.
    DrawCallParams(GLenum mode, GLenum type, const void *indirect);

    GLenum mode() const;

    // This value is the sum of 'baseVertex' and the first indexed vertex for DrawElements calls.
    GLint firstVertex() const;

    size_t vertexCount() const;
    GLsizei indexCount() const;
    GLint baseVertex() const;
    GLenum type() const;
    const void *indices() const;
    GLsizei instances() const;
    const void *indirect() const;

    Error ensureIndexRangeResolved(const Context *context) const;
    bool isDrawElements() const;
    bool isDrawIndirect() const;

    // ensureIndexRangeResolved must be called first.
    const IndexRange &getIndexRange() const;

    template <typename T>
    T getClampedVertexCount() const;

    template <EntryPoint EP, typename... ArgsT>
    static void Factory(DrawCallParams *objBuffer, ArgsT... args);

    ANGLE_PARAM_TYPE_INFO(DrawCallParams, ParamsBase);

  private:
    GLenum mMode;
    mutable Optional<IndexRange> mIndexRange;
    mutable GLint mFirstVertex;
    mutable size_t mVertexCount;
    GLint mIndexCount;
    GLint mBaseVertex;
    GLenum mType;
    const void *mIndices;
    GLsizei mInstances;
    const void *mIndirect;
};

template <typename T>
T DrawCallParams::getClampedVertexCount() const
{
    constexpr size_t kMax = static_cast<size_t>(std::numeric_limits<T>::max());
    return static_cast<T>(mVertexCount > kMax ? kMax : mVertexCount);
}

// Entry point funcs essentially re-map different entry point parameter arrays into
// the format the parameter type class expects. For example, for HasIndexRange, for the
// various indexed draw calls, they drop parameters that aren't useful and re-arrange
// the rest.
#define ANGLE_ENTRY_POINT_FUNC(NAME, CLASS, ...)    \
    \
template<> struct EntryPointParam<EntryPoint::NAME> \
    {                                               \
        using Type = CLASS;                         \
    };                                              \
    \
template<> inline void CLASS::Factory<EntryPoint::NAME>(__VA_ARGS__)

ANGLE_ENTRY_POINT_FUNC(DrawArrays,
                       DrawCallParams,
                       DrawCallParams *objBuffer,
                       Context *context,
                       GLenum mode,
                       GLint first,
                       GLsizei count)
{
    return ParamsBase::Factory<EntryPoint::DrawArrays>(objBuffer, mode, first, count, 0);
}

ANGLE_ENTRY_POINT_FUNC(DrawArraysInstanced,
                       DrawCallParams,
                       DrawCallParams *objBuffer,
                       Context *context,
                       GLenum mode,
                       GLint first,
                       GLsizei count,
                       GLsizei instanceCount)
{
    return ParamsBase::Factory<EntryPoint::DrawArraysInstanced>(objBuffer, mode, first, count,
                                                                instanceCount);
}

ANGLE_ENTRY_POINT_FUNC(DrawArraysInstancedANGLE,
                       DrawCallParams,
                       DrawCallParams *objBuffer,
                       Context *context,
                       GLenum mode,
                       GLint first,
                       GLsizei count,
                       GLsizei instanceCount)
{
    return ParamsBase::Factory<EntryPoint::DrawArraysInstancedANGLE>(objBuffer, mode, first, count,
                                                                     instanceCount);
}

ANGLE_ENTRY_POINT_FUNC(DrawArraysIndirect,
                       DrawCallParams,
                       DrawCallParams *objBuffer,
                       Context *context,
                       GLenum mode,
                       const void *indirect)
{
    return ParamsBase::Factory<EntryPoint::DrawArraysIndirect>(objBuffer, mode, indirect);
}

ANGLE_ENTRY_POINT_FUNC(DrawElementsIndirect,
                       DrawCallParams,
                       DrawCallParams *objBuffer,
                       Context *context,
                       GLenum mode,
                       GLenum type,
                       const void *indirect)
{
    return ParamsBase::Factory<EntryPoint::DrawElementsIndirect>(objBuffer, mode, type, indirect);
}

ANGLE_ENTRY_POINT_FUNC(DrawElements,
                       DrawCallParams,
                       DrawCallParams *objBuffer,
                       Context *context,
                       GLenum mode,
                       GLsizei count,
                       GLenum type,
                       const void *indices)
{
    return ParamsBase::Factory<EntryPoint::DrawElements>(objBuffer, mode, count, type, indices, 0,
                                                         0);
}

ANGLE_ENTRY_POINT_FUNC(DrawElementsInstanced,
                       DrawCallParams,
                       DrawCallParams *objBuffer,
                       Context *context,
                       GLenum mode,
                       GLsizei count,
                       GLenum type,
                       const void *indices,
                       GLsizei instanceCount)
{
    return ParamsBase::Factory<EntryPoint::DrawElementsInstanced>(objBuffer, mode, count, type,
                                                                  indices, 0, instanceCount);
}

ANGLE_ENTRY_POINT_FUNC(DrawElementsInstancedANGLE,
                       DrawCallParams,
                       DrawCallParams *objBuffer,
                       Context *context,
                       GLenum mode,
                       GLsizei count,
                       GLenum type,
                       const void *indices,
                       GLsizei instanceCount)
{
    return ParamsBase::Factory<EntryPoint::DrawElementsInstancedANGLE>(objBuffer, mode, count, type,
                                                                       indices, 0, instanceCount);
}

ANGLE_ENTRY_POINT_FUNC(DrawRangeElements,
                       DrawCallParams,
                       DrawCallParams *objBuffer,
                       Context *context,
                       GLenum mode,
                       GLuint /*start*/,
                       GLuint /*end*/,
                       GLsizei count,
                       GLenum type,
                       const void *indices)
{
    return ParamsBase::Factory<EntryPoint::DrawRangeElements>(objBuffer, mode, count, type, indices,
                                                              0, 0);
}

#undef ANGLE_ENTRY_POINT_FUNC

template <EntryPoint EP>
struct EntryPointParam
{
    using Type = ParamsBase;
};

// A template struct for determining the default value to return for each entry point.
template <EntryPoint EP, typename ReturnType>
struct DefaultReturnValue;

// Default return values for each basic return type.
template <EntryPoint EP>
struct DefaultReturnValue<EP, GLint>
{
    static constexpr GLint kValue = -1;
};

// This doubles as the GLenum return value.
template <EntryPoint EP>
struct DefaultReturnValue<EP, GLuint>
{
    static constexpr GLuint kValue = 0;
};

template <EntryPoint EP>
struct DefaultReturnValue<EP, GLboolean>
{
    static constexpr GLboolean kValue = GL_FALSE;
};

// Catch-all rules for pointer types.
template <EntryPoint EP, typename PointerType>
struct DefaultReturnValue<EP, const PointerType *>
{
    static constexpr const PointerType *kValue = nullptr;
};

template <EntryPoint EP, typename PointerType>
struct DefaultReturnValue<EP, PointerType *>
{
    static constexpr PointerType *kValue = nullptr;
};

// Overloaded to return invalid index
template <>
struct DefaultReturnValue<EntryPoint::GetUniformBlockIndex, GLuint>
{
    static constexpr GLuint kValue = GL_INVALID_INDEX;
};

// Specialized enum error value.
template <>
struct DefaultReturnValue<EntryPoint::ClientWaitSync, GLenum>
{
    static constexpr GLenum kValue = GL_WAIT_FAILED;
};

// glTestFenceNV should still return TRUE for an invalid fence.
template <>
struct DefaultReturnValue<EntryPoint::TestFenceNV, GLboolean>
{
    static constexpr GLboolean kValue = GL_TRUE;
};

template <EntryPoint EP, typename ReturnType>
constexpr ANGLE_INLINE ReturnType GetDefaultReturnValue()
{
    return DefaultReturnValue<EP, ReturnType>::kValue;
}

}  // namespace gl

#endif  // LIBANGLE_PARAMS_H_
