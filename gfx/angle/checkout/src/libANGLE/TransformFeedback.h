//
// Copyright (c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef LIBANGLE_TRANSFORM_FEEDBACK_H_
#define LIBANGLE_TRANSFORM_FEEDBACK_H_

#include "libANGLE/RefCountObject.h"

#include "common/angleutils.h"
#include "libANGLE/Debug.h"

#include "angle_gl.h"

namespace rx
{
class GLImplFactory;
class TransformFeedbackImpl;
}

namespace gl
{
class Buffer;
struct Caps;
class Context;
class Program;

class TransformFeedbackState final : angle::NonCopyable
{
  public:
    TransformFeedbackState(size_t maxIndexedBuffers);
    ~TransformFeedbackState();

    const OffsetBindingPointer<Buffer> &getIndexedBuffer(size_t idx) const;
    const std::vector<OffsetBindingPointer<Buffer>> &getIndexedBuffers() const;

  private:
    friend class TransformFeedback;

    std::string mLabel;

    bool mActive;
    GLenum mPrimitiveMode;
    bool mPaused;
    GLsizeiptr mVerticesDrawn;
    GLsizeiptr mVertexCapacity;

    Program *mProgram;

    std::vector<OffsetBindingPointer<Buffer>> mIndexedBuffers;
};

class TransformFeedback final : public RefCountObject, public LabeledObject
{
  public:
    TransformFeedback(rx::GLImplFactory *implFactory, GLuint id, const Caps &caps);
    ~TransformFeedback() override;
    Error onDestroy(const Context *context) override;

    void setLabel(const std::string &label) override;
    const std::string &getLabel() const override;

    void begin(const Context *context, GLenum primitiveMode, Program *program);
    void end(const Context *context);
    void pause();
    void resume();

    bool isActive() const;
    bool isPaused() const;
    GLenum getPrimitiveMode() const;
    // Validates that the vertices produced by a draw call will fit in the bound transform feedback
    // buffers.
    bool checkBufferSpaceForDraw(GLsizei count, GLsizei primcount) const;
    // This must be called after each draw call when transform feedback is enabled to keep track of
    // how many vertices have been written to the buffers. This information is needed by
    // checkBufferSpaceForDraw because each draw call appends vertices to the buffers starting just
    // after the last vertex of the previous draw call.
    void onVerticesDrawn(const Context *context, GLsizei count, GLsizei primcount);

    bool hasBoundProgram(GLuint program) const;

    void bindIndexedBuffer(const Context *context,
                           size_t index,
                           Buffer *buffer,
                           size_t offset,
                           size_t size);
    const OffsetBindingPointer<Buffer> &getIndexedBuffer(size_t index) const;
    size_t getIndexedBufferCount() const;

    // Returns true if any buffer bound to this object is also bound to another target.
    bool buffersBoundForOtherUse() const;

    void detachBuffer(const Context *context, GLuint bufferName);

    rx::TransformFeedbackImpl *getImplementation();
    const rx::TransformFeedbackImpl *getImplementation() const;

    void onBindingChanged(const Context *context, bool bound);

  private:
    void bindProgram(const Context *context, Program *program);

    TransformFeedbackState mState;
    rx::TransformFeedbackImpl *mImplementation;
};

}  // namespace gl

#endif // LIBANGLE_TRANSFORM_FEEDBACK_H_
