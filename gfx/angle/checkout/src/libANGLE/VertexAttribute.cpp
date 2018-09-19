//
// Copyright 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Implementation of the state classes for mananging GLES 3.1 Vertex Array Objects.
//

#include "libANGLE/VertexAttribute.h"

namespace gl
{

// [OpenGL ES 3.1] (November 3, 2016) Section 20 Page 361
// Table 20.2: Vertex Array Object State
VertexBinding::VertexBinding() : VertexBinding(0)
{
}

VertexBinding::VertexBinding(GLuint boundAttribute) : mStride(16u), mDivisor(0), mOffset(0)
{
    mBoundAttributesMask.set(boundAttribute);
}

VertexBinding::VertexBinding(VertexBinding &&binding)
{
    *this = std::move(binding);
}

VertexBinding::~VertexBinding()
{
}

VertexBinding &VertexBinding::operator=(VertexBinding &&binding)
{
    if (this != &binding)
    {
        mStride  = binding.mStride;
        mDivisor = binding.mDivisor;
        mOffset  = binding.mOffset;
        mBoundAttributesMask = binding.mBoundAttributesMask;
        std::swap(binding.mBuffer, mBuffer);
    }
    return *this;
}

void VertexBinding::setBuffer(const gl::Context *context, Buffer *bufferIn, bool containerIsBound)
{
    if (containerIsBound)
    {
        if (mBuffer.get())
            mBuffer->onNonTFBindingChanged(context, -1);
        if (bufferIn)
            bufferIn->onNonTFBindingChanged(context, 1);
    }
    mBuffer.set(context, bufferIn);
}

void VertexBinding::onContainerBindingChanged(const Context *context, int incr) const
{
    if (mBuffer.get())
        mBuffer->onNonTFBindingChanged(context, incr);
}

VertexAttribute::VertexAttribute(GLuint bindingIndex)
    : enabled(false),
      type(GL_FLOAT),
      size(4u),
      normalized(false),
      pureInteger(false),
      pointer(nullptr),
      relativeOffset(0),
      vertexAttribArrayStride(0),
      bindingIndex(bindingIndex),
      mCachedElementLimit(0)
{
}

VertexAttribute::VertexAttribute(VertexAttribute &&attrib)
    : enabled(attrib.enabled),
      type(attrib.type),
      size(attrib.size),
      normalized(attrib.normalized),
      pureInteger(attrib.pureInteger),
      pointer(attrib.pointer),
      relativeOffset(attrib.relativeOffset),
      vertexAttribArrayStride(attrib.vertexAttribArrayStride),
      bindingIndex(attrib.bindingIndex),
      mCachedElementLimit(attrib.mCachedElementLimit)
{
}

VertexAttribute &VertexAttribute::operator=(VertexAttribute &&attrib)
{
    if (this != &attrib)
    {
        enabled                 = attrib.enabled;
        type                    = attrib.type;
        size                    = attrib.size;
        normalized              = attrib.normalized;
        pureInteger             = attrib.pureInteger;
        pointer                 = attrib.pointer;
        relativeOffset          = attrib.relativeOffset;
        vertexAttribArrayStride = attrib.vertexAttribArrayStride;
        bindingIndex            = attrib.bindingIndex;
        mCachedElementLimit     = attrib.mCachedElementLimit;
    }
    return *this;
}

void VertexAttribute::updateCachedElementLimit(const VertexBinding &binding)
{
    Buffer *buffer = binding.getBuffer().get();
    if (!buffer)
    {
        mCachedElementLimit = 0;
        return;
    }

    angle::CheckedNumeric<GLint64> bufferSize(buffer->getSize());
    angle::CheckedNumeric<GLint64> bufferOffset(binding.getOffset());
    angle::CheckedNumeric<GLint64> attribOffset(relativeOffset);
    angle::CheckedNumeric<GLint64> attribSize(ComputeVertexAttributeTypeSize(*this));

    // (buffer.size - buffer.offset - attrib.relativeOffset - attrib.size) / binding.stride
    angle::CheckedNumeric<GLint64> elementLimit =
        (bufferSize - bufferOffset - attribOffset - attribSize);

    // Use the special integer overflow value if there was a math error.
    if (!elementLimit.IsValid())
    {
        static_assert(kIntegerOverflow < 0, "Unexpected value");
        mCachedElementLimit = kIntegerOverflow;
        return;
    }

    mCachedElementLimit = elementLimit.ValueOrDie();
    if (mCachedElementLimit < 0)
    {
        return;
    }

    if (binding.getStride() == 0)
    {
        // Special case for a zero stride. If we can fit one vertex we can fit infinite vertices.
        mCachedElementLimit = std::numeric_limits<GLint64>::max();
        return;
    }

    angle::CheckedNumeric<GLint64> bindingStride(binding.getStride());
    elementLimit /= bindingStride;

    if (binding.getDivisor() > 0)
    {
        // For instanced draws, the element count is floor(instanceCount - 1) / binding.divisor.
        angle::CheckedNumeric<GLint64> bindingDivisor(binding.getDivisor());
        elementLimit *= bindingDivisor;

        // We account for the floor() part rounding by adding a rounding constant.
        elementLimit += bindingDivisor - 1;
    }

    mCachedElementLimit = elementLimit.ValueOrDefault(kIntegerOverflow);
}

size_t ComputeVertexAttributeTypeSize(const VertexAttribute& attrib)
{
    GLuint size = attrib.size;
    switch (attrib.type)
    {
      case GL_BYTE:                        return size * sizeof(GLbyte);
      case GL_UNSIGNED_BYTE:               return size * sizeof(GLubyte);
      case GL_SHORT:                       return size * sizeof(GLshort);
      case GL_UNSIGNED_SHORT:              return size * sizeof(GLushort);
      case GL_INT:                         return size * sizeof(GLint);
      case GL_UNSIGNED_INT:                return size * sizeof(GLuint);
      case GL_INT_2_10_10_10_REV:          return 4;
      case GL_UNSIGNED_INT_2_10_10_10_REV: return 4;
      case GL_FIXED:                       return size * sizeof(GLfixed);
      case GL_HALF_FLOAT:                  return size * sizeof(GLhalf);
      case GL_FLOAT:                       return size * sizeof(GLfloat);
      default: UNREACHABLE();              return size * sizeof(GLfloat);
    }
}

size_t ComputeVertexAttributeStride(const VertexAttribute &attrib, const VertexBinding &binding)
{
    // In ES 3.1, VertexAttribPointer will store the type size in the binding stride.
    // Hence, rendering always uses the binding's stride.
    return attrib.enabled ? binding.getStride() : 16u;
}

// Warning: you should ensure binding really matches attrib.bindingIndex before using this function.
GLintptr ComputeVertexAttributeOffset(const VertexAttribute &attrib, const VertexBinding &binding)
{
    return attrib.relativeOffset + binding.getOffset();
}

size_t ComputeVertexBindingElementCount(GLuint divisor, size_t drawCount, size_t instanceCount)
{
    // For instanced rendering, we draw "instanceDrawCount" sets of "vertexDrawCount" vertices.
    //
    // A vertex attribute with a positive divisor loads one instanced vertex for every set of
    // non-instanced vertices, and the instanced vertex index advances once every "mDivisor"
    // instances.
    if (instanceCount > 0 && divisor > 0)
    {
        // When instanceDrawCount is not a multiple attrib.divisor, the division must round up.
        // For instance, with 5 non-instanced vertices and a divisor equal to 3, we need 2 instanced
        // vertices.
        return (instanceCount + divisor - 1u) / divisor;
    }

    return drawCount;
}

GLenum GetVertexAttributeBaseType(const VertexAttribute &attrib)
{
    if (attrib.pureInteger)
    {
        switch (attrib.type)
        {
            case GL_BYTE:
            case GL_SHORT:
            case GL_INT:
                return GL_INT;

            case GL_UNSIGNED_BYTE:
            case GL_UNSIGNED_SHORT:
            case GL_UNSIGNED_INT:
                return GL_UNSIGNED_INT;

            default:
                UNREACHABLE();
                return GL_NONE;
        }
    }
    else
    {
        return GL_FLOAT;
    }
}

}  // namespace gl
