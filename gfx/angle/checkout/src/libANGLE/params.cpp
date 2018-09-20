//
// Copyright 2017 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// params:
//   Parameter wrapper structs for OpenGL ES. These helpers cache re-used values
//   in entry point routines.

#include "libANGLE/params.h"

#include "common/utilities.h"
#include "libANGLE/Context.h"
#include "libANGLE/VertexArray.h"

namespace gl
{

// static
constexpr ParamTypeInfo ParamsBase::TypeInfo;
constexpr ParamTypeInfo DrawCallParams::TypeInfo;

// DrawCallParams implementation.
// Called by DrawArrays.
DrawCallParams::DrawCallParams(PrimitiveMode mode,
                               GLint firstVertex,
                               GLsizei vertexCount,
                               GLsizei instances)
    : mMode(mode),
      mFirstVertex(firstVertex),
      mVertexCount(vertexCount),
      mIndexCount(0),
      mBaseVertex(0),
      mType(GL_NONE),
      mIndices(nullptr),
      mInstances(instances),
      mIndirect(nullptr)
{
}

// Called by DrawElements.
DrawCallParams::DrawCallParams(PrimitiveMode mode,
                               GLint indexCount,
                               GLenum type,
                               const void *indices,
                               GLint baseVertex,
                               GLsizei instances)
    : mMode(mode),
      mFirstVertex(0),
      mVertexCount(0),
      mIndexCount(indexCount),
      mBaseVertex(baseVertex),
      mType(type),
      mIndices(indices),
      mInstances(instances),
      mIndirect(nullptr)
{
}

// Called by DrawArraysIndirect.
DrawCallParams::DrawCallParams(PrimitiveMode mode, const void *indirect)
    : mMode(mode),
      mFirstVertex(0),
      mVertexCount(0),
      mIndexCount(0),
      mBaseVertex(0),
      mType(GL_NONE),
      mIndices(nullptr),
      mInstances(0),
      mIndirect(indirect)
{
}

// Called by DrawElementsIndirect.
DrawCallParams::DrawCallParams(PrimitiveMode mode, GLenum type, const void *indirect)
    : mMode(mode),
      mFirstVertex(0),
      mVertexCount(0),
      mIndexCount(0),
      mBaseVertex(0),
      mType(type),
      mIndices(nullptr),
      mInstances(0),
      mIndirect(indirect)
{
}

GLint DrawCallParams::firstVertex() const
{
    // In some cases we can know the first vertex will be fixed at zero, if we're on the "fast
    // path". In these cases the index range is not resolved. If the first vertex is not zero,
    // however, then it must be because the index range is resolved. This only applies to the
    // D3D11 back-end currently.
    ASSERT(mFirstVertex == 0 || (!isDrawElements() || mIndexRange.valid()));
    return mFirstVertex;
}

GLsizei DrawCallParams::indexCount() const
{
    ASSERT(isDrawElements());
    return mIndexCount;
}

GLint DrawCallParams::baseVertex() const
{
    return mBaseVertex;
}

GLenum DrawCallParams::type() const
{
    ASSERT(isDrawElements());
    return mType;
}

const void *DrawCallParams::indices() const
{
    return mIndices;
}

GLsizei DrawCallParams::instances() const
{
    return mInstances;
}

const void *DrawCallParams::indirect() const
{
    return mIndirect;
}

bool DrawCallParams::isDrawIndirect() const
{
    // This is a bit of a hack - it's quite possible for a direct call to have a zero count, but we
    // assume these calls are filtered out before they make it to this code.
    return (mIndexCount == 0 && mVertexCount == 0);
}

Error DrawCallParams::ensureIndexRangeResolved(const Context *context) const
{
    if (mIndexRange.valid() || !isDrawElements())
    {
        return NoError();
    }

    const State &state = context->getGLState();

    const gl::VertexArray *vao     = state.getVertexArray();
    gl::Buffer *elementArrayBuffer = vao->getElementArrayBuffer().get();

    if (elementArrayBuffer)
    {
        uintptr_t offset = reinterpret_cast<uintptr_t>(mIndices);
        IndexRange indexRange;
        ANGLE_TRY(elementArrayBuffer->getIndexRange(context, mType, static_cast<size_t>(offset),
                                                    mIndexCount, state.isPrimitiveRestartEnabled(),
                                                    &indexRange));
        mIndexRange = indexRange;
    }
    else
    {
        mIndexRange =
            ComputeIndexRange(mType, mIndices, mIndexCount, state.isPrimitiveRestartEnabled());
    }

    const IndexRange &indexRange = mIndexRange.value();

    // The entire index range should be within the limits of a 32-bit uint because the largest GL
    // index type is GL_UNSIGNED_INT.
    ASSERT(indexRange.start <= std::numeric_limits<uint32_t>::max() &&
           indexRange.end <= std::numeric_limits<uint32_t>::max());

    // Given the assertion above and the type of mBaseVertex (GLint), adding them both as 64-bit
    // ints is safe.
    int64_t startVertexInt64 =
        static_cast<int64_t>(mBaseVertex) + static_cast<int64_t>(indexRange.start);

    // OpenGL ES 3.2 spec section 10.5: "Behavior of DrawElementsOneInstance is undefined if the
    // vertex ID is negative for any element"
    if (startVertexInt64 < 0)
    {
        return gl::InternalError() << "Negative index value.";
    }

    // OpenGL ES 3.2 spec section 10.5: "If the vertex ID is larger than the maximum value
    // representable by type, it should behave as if the calculation were upconverted to 32-bit
    // unsigned integers(with wrapping on overflow conditions)." ANGLE does not fully handle these
    // rules, an overflow error is returned if the start vertex cannot be stored in a 32-bit signed
    // integer.
    if (startVertexInt64 > std::numeric_limits<GLint>::max())
    {
        return gl::InternalError() << "Negative value overflow.";
    }

    mFirstVertex = static_cast<GLint>(startVertexInt64);
    mVertexCount = indexRange.vertexCount();

    return NoError();
}

const IndexRange &DrawCallParams::getIndexRange() const
{
    ASSERT(isDrawElements() && mIndexRange.valid());
    return mIndexRange.value();
}

}  // namespace gl
