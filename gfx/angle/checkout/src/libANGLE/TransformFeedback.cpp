//
// Copyright (c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "libANGLE/TransformFeedback.h"

#include "common/mathutil.h"
#include "libANGLE/Buffer.h"
#include "libANGLE/Caps.h"
#include "libANGLE/Context.h"
#include "libANGLE/ContextState.h"
#include "libANGLE/Program.h"
#include "libANGLE/renderer/GLImplFactory.h"
#include "libANGLE/renderer/TransformFeedbackImpl.h"

#include <limits>

namespace gl
{

angle::CheckedNumeric<GLsizeiptr> GetVerticesNeededForDraw(GLenum primitiveMode,
                                                           GLsizei count,
                                                           GLsizei primcount)
{
    if (count < 0 || primcount < 0)
    {
        return 0;
    }
    // Transform feedback only outputs complete primitives, so we need to round down to the nearest
    // complete primitive before multiplying by the number of instances.
    angle::CheckedNumeric<GLsizeiptr> checkedCount     = count;
    angle::CheckedNumeric<GLsizeiptr> checkedPrimcount = primcount;
    switch (primitiveMode)
    {
        case GL_TRIANGLES:
            return checkedPrimcount * (checkedCount - checkedCount % 3);
        case GL_LINES:
            return checkedPrimcount * (checkedCount - checkedCount % 2);
        case GL_POINTS:
            return checkedPrimcount * checkedCount;
        default:
            UNREACHABLE();
            return checkedPrimcount * checkedCount;
    }
}

TransformFeedbackState::TransformFeedbackState(size_t maxIndexedBuffers)
    : mLabel(),
      mActive(false),
      mPrimitiveMode(GL_NONE),
      mPaused(false),
      mVerticesDrawn(0),
      mVertexCapacity(0),
      mProgram(nullptr),
      mIndexedBuffers(maxIndexedBuffers)
{
}

TransformFeedbackState::~TransformFeedbackState()
{
}

const OffsetBindingPointer<Buffer> &TransformFeedbackState::getIndexedBuffer(size_t idx) const
{
    return mIndexedBuffers[idx];
}

const std::vector<OffsetBindingPointer<Buffer>> &TransformFeedbackState::getIndexedBuffers() const
{
    return mIndexedBuffers;
}

TransformFeedback::TransformFeedback(rx::GLImplFactory *implFactory, GLuint id, const Caps &caps)
    : RefCountObject(id),
      mState(caps.maxTransformFeedbackSeparateAttributes),
      mImplementation(implFactory->createTransformFeedback(mState))
{
    ASSERT(mImplementation != nullptr);
}

Error TransformFeedback::onDestroy(const Context *context)
{
    ASSERT(!context || !context->isCurrentTransformFeedback(this));
    if (mState.mProgram)
    {
        mState.mProgram->release(context);
        mState.mProgram = nullptr;
    }

    ASSERT(!mState.mProgram);
    for (size_t i = 0; i < mState.mIndexedBuffers.size(); i++)
    {
        mState.mIndexedBuffers[i].set(context, nullptr);
    }

    return NoError();
}

TransformFeedback::~TransformFeedback()
{
    SafeDelete(mImplementation);
}

void TransformFeedback::setLabel(const std::string &label)
{
    mState.mLabel = label;
}

const std::string &TransformFeedback::getLabel() const
{
    return mState.mLabel;
}

void TransformFeedback::begin(const Context *context, GLenum primitiveMode, Program *program)
{
    mState.mActive        = true;
    mState.mPrimitiveMode = primitiveMode;
    mState.mPaused        = false;
    mState.mVerticesDrawn = 0;
    mImplementation->begin(primitiveMode);
    bindProgram(context, program);

    if (program)
    {
        // Compute the number of vertices we can draw before overflowing the bound buffers.
        auto strides = program->getTransformFeedbackStrides();
        ASSERT(strides.size() <= mState.mIndexedBuffers.size() && !strides.empty());
        GLsizeiptr minCapacity = std::numeric_limits<GLsizeiptr>::max();
        for (size_t index = 0; index < strides.size(); index++)
        {
            GLsizeiptr capacity =
                GetBoundBufferAvailableSize(mState.mIndexedBuffers[index]) / strides[index];
            minCapacity = std::min(minCapacity, capacity);
        }
        mState.mVertexCapacity = minCapacity;
    }
    else
    {
        mState.mVertexCapacity = 0;
    }
}

void TransformFeedback::end(const Context *context)
{
    mState.mActive         = false;
    mState.mPrimitiveMode  = GL_NONE;
    mState.mPaused         = false;
    mState.mVerticesDrawn  = 0;
    mState.mVertexCapacity = 0;
    mImplementation->end();
    if (mState.mProgram)
    {
        mState.mProgram->release(context);
        mState.mProgram = nullptr;
    }
}

void TransformFeedback::pause()
{
    mState.mPaused = true;
    mImplementation->pause();
}

void TransformFeedback::resume()
{
    mState.mPaused = false;
    mImplementation->resume();
}

bool TransformFeedback::isActive() const
{
    return mState.mActive;
}

bool TransformFeedback::isPaused() const
{
    return mState.mPaused;
}

GLenum TransformFeedback::getPrimitiveMode() const
{
    return mState.mPrimitiveMode;
}

bool TransformFeedback::checkBufferSpaceForDraw(GLsizei count, GLsizei primcount) const
{
    auto vertices =
        mState.mVerticesDrawn + GetVerticesNeededForDraw(mState.mPrimitiveMode, count, primcount);
    return vertices.IsValid() && vertices.ValueOrDie() <= mState.mVertexCapacity;
}

void TransformFeedback::onVerticesDrawn(const Context *context, GLsizei count, GLsizei primcount)
{
    ASSERT(mState.mActive && !mState.mPaused);
    // All draws should be validated with checkBufferSpaceForDraw so ValueOrDie should never fail.
    mState.mVerticesDrawn =
        (mState.mVerticesDrawn + GetVerticesNeededForDraw(mState.mPrimitiveMode, count, primcount))
            .ValueOrDie();

    for (auto &buffer : mState.mIndexedBuffers)
    {
        if (buffer.get() != nullptr)
        {
            buffer->onTransformFeedback(context);
        }
    }
}

void TransformFeedback::bindProgram(const Context *context, Program *program)
{
    if (mState.mProgram != program)
    {
        if (mState.mProgram != nullptr)
        {
            mState.mProgram->release(context);
        }
        mState.mProgram = program;
        if (mState.mProgram != nullptr)
        {
            mState.mProgram->addRef();
        }
    }
}

bool TransformFeedback::hasBoundProgram(GLuint program) const
{
    return mState.mProgram != nullptr && mState.mProgram->id() == program;
}

void TransformFeedback::detachBuffer(const Context *context, GLuint bufferName)
{
    bool isBound = context->isCurrentTransformFeedback(this);
    for (size_t index = 0; index < mState.mIndexedBuffers.size(); index++)
    {
        if (mState.mIndexedBuffers[index].id() == bufferName)
        {
            if (isBound)
            {
                mState.mIndexedBuffers[index]->onBindingChanged(context, false,
                                                                BufferBinding::TransformFeedback);
            }
            mState.mIndexedBuffers[index].set(context, nullptr);
            mImplementation->bindIndexedBuffer(index, mState.mIndexedBuffers[index]);
        }
    }
}

void TransformFeedback::bindIndexedBuffer(const Context *context,
                                          size_t index,
                                          Buffer *buffer,
                                          size_t offset,
                                          size_t size)
{
    ASSERT(index < mState.mIndexedBuffers.size());
    bool isBound = context && context->isCurrentTransformFeedback(this);
    if (isBound && mState.mIndexedBuffers[index].get())
    {
        mState.mIndexedBuffers[index]->onBindingChanged(context, false,
                                                        BufferBinding::TransformFeedback);
    }
    mState.mIndexedBuffers[index].set(context, buffer, offset, size);
    if (isBound && buffer)
    {
        buffer->onBindingChanged(context, true, BufferBinding::TransformFeedback);
    }

    mImplementation->bindIndexedBuffer(index, mState.mIndexedBuffers[index]);
}

const OffsetBindingPointer<Buffer> &TransformFeedback::getIndexedBuffer(size_t index) const
{
    ASSERT(index < mState.mIndexedBuffers.size());
    return mState.mIndexedBuffers[index];
}

size_t TransformFeedback::getIndexedBufferCount() const
{
    return mState.mIndexedBuffers.size();
}

bool TransformFeedback::buffersBoundForOtherUse() const
{
    for (auto &buffer : mState.mIndexedBuffers)
    {
        if (buffer.get() && buffer->isBoundForTransformFeedbackAndOtherUse())
        {
            return true;
        }
    }
    return false;
}

rx::TransformFeedbackImpl *TransformFeedback::getImplementation()
{
    return mImplementation;
}

const rx::TransformFeedbackImpl *TransformFeedback::getImplementation() const
{
    return mImplementation;
}

void TransformFeedback::onBindingChanged(const Context *context, bool bound)
{
    for (auto &buffer : mState.mIndexedBuffers)
    {
        if (buffer.get())
        {
            buffer->onBindingChanged(context, bound, BufferBinding::TransformFeedback);
        }
    }
}
}  // namespace gl
