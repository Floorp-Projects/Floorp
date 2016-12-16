/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLTransformFeedback.h"

#include "GLContext.h"
#include "mozilla/dom/WebGL2RenderingContextBinding.h"
#include "WebGL2Context.h"

namespace mozilla {

WebGLTransformFeedback::WebGLTransformFeedback(WebGLContext* webgl, GLuint tf)
    : WebGLContextBoundObject(webgl)
    , mGLName(tf)
    , mIndexedBindings(webgl->mGLMaxTransformFeedbackSeparateAttribs)
    , mIsPaused(false)
    , mIsActive(false)
    , mBuffersForTF_Dirty(true)
{
    mContext->mTransformFeedbacks.insertBack(this);
}

WebGLTransformFeedback::~WebGLTransformFeedback()
{
    DeleteOnce();
}

void
WebGLTransformFeedback::Delete()
{
    if (mGLName) {
        mContext->MakeContextCurrent();
        mContext->gl->fDeleteTransformFeedbacks(1, &mGLName);
    }
    removeFrom(mContext->mTransformFeedbacks);
}

////

const decltype(WebGLTransformFeedback::mBuffersForTF)&
WebGLTransformFeedback::BuffersForTF() const
{
    // The generic bind point cannot incur undefined read/writes because otherwise it
    // would be impossible to read back from this. The spec implies that readback from
    // the TRANSFORM_FEEDBACK target is possible, just not simultaneously with being
    // "bound or in use for transform feedback".
    // Therefore, only the indexed bindings of the TFO count.
    if (mBuffersForTF_Dirty) {
        mBuffersForTF.clear();
        for (const auto& cur : mIndexedBindings) {
            if (cur.mBufferBinding) {
                mBuffersForTF.insert(cur.mBufferBinding.get());
            }
        }
        mBuffersForTF_Dirty = false;
    }
    return mBuffersForTF;
}

////////////////////////////////////////

void
WebGLTransformFeedback::BeginTransformFeedback(GLenum primMode)
{
    const char funcName[] = "beginTransformFeedback";

    if (mIsActive)
        return mContext->ErrorInvalidOperation("%s: Already active.", funcName);

    switch (primMode) {
    case LOCAL_GL_POINTS:
    case LOCAL_GL_LINES:
    case LOCAL_GL_TRIANGLES:
        break;
    default:
        mContext->ErrorInvalidEnum("%s: `primitiveMode` must be one of POINTS, LINES, or"
                                   " TRIANGLES.",
                                   funcName);
        return;
    }

    const auto& prog = mContext->mCurrentProgram;
    if (!prog ||
        !prog->IsLinked() ||
        !prog->LinkInfo()->componentsPerTFVert.size())
    {
        mContext->ErrorInvalidOperation("%s: Current program not valid for transform"
                                        " feedback.",
                                        funcName);
        return;
    }

    const auto& linkInfo = prog->LinkInfo();
    const auto& componentsPerTFVert = linkInfo->componentsPerTFVert;

    size_t minVertCapacity = SIZE_MAX;
    for (size_t i = 0; i < componentsPerTFVert.size(); i++) {
        const auto& indexedBinding = mIndexedBindings[i];
        const auto& componentsPerVert = componentsPerTFVert[i];

        const auto& buffer = indexedBinding.mBufferBinding;
        if (!buffer) {
            mContext->ErrorInvalidOperation("%s: No buffer attached to required transform"
                                            " feedback index %u.",
                                            funcName, (uint32_t)i);
            return;
        }

        const size_t vertCapacity = buffer->ByteLength() / 4 / componentsPerVert;
        minVertCapacity = std::min(minVertCapacity, vertCapacity);
    }

    ////

    const auto& gl = mContext->gl;
    gl->MakeCurrent();
    gl->fBeginTransformFeedback(primMode);

    ////

    mIsActive = true;
    MOZ_ASSERT(!mIsPaused);

    mActive_Program = prog;
    mActive_PrimMode = primMode;
    mActive_VertPosition = 0;
    mActive_VertCapacity = minVertCapacity;

    ////

    mActive_Program->mNumActiveTFOs++;
}


void
WebGLTransformFeedback::EndTransformFeedback()
{
    const char funcName[] = "endTransformFeedback";

    if (!mIsActive)
        return mContext->ErrorInvalidOperation("%s: Not active.", funcName);

    ////

    const auto& gl = mContext->gl;
    gl->MakeCurrent();
    gl->fEndTransformFeedback();

    ////

    mIsActive = false;
    mIsPaused = false;

    ////

    mActive_Program->mNumActiveTFOs--;
}

void
WebGLTransformFeedback::PauseTransformFeedback()
{
    const char funcName[] = "pauseTransformFeedback";

    if (!mIsActive ||
        mIsPaused)
    {
        mContext->ErrorInvalidOperation("%s: Not active or is paused.", funcName);
        return;
    }

    ////

    const auto& gl = mContext->gl;
    gl->MakeCurrent();
    gl->fPauseTransformFeedback();

    ////

    mIsPaused = true;
}

void
WebGLTransformFeedback::ResumeTransformFeedback()
{
    const char funcName[] = "resumeTransformFeedback";

    if (!mIsPaused)
        return mContext->ErrorInvalidOperation("%s: Not paused.", funcName);

    if (mContext->mCurrentProgram != mActive_Program) {
        mContext->ErrorInvalidOperation("%s: Active program differs from original.",
                                        funcName);
        return;
    }

    ////

    const auto& gl = mContext->gl;
    gl->MakeCurrent();
    gl->fResumeTransformFeedback();

    ////

    MOZ_ASSERT(mIsActive);
    mIsPaused = false;
}

////////////////////////////////////////

JSObject*
WebGLTransformFeedback::WrapObject(JSContext* cx, JS::Handle<JSObject*> givenProto)
{
    return dom::WebGLTransformFeedbackBinding::Wrap(cx, this, givenProto);
}

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(WebGLTransformFeedback, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(WebGLTransformFeedback, Release)
NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(WebGLTransformFeedback,
                                      mGenericBufferBinding,
                                      mIndexedBindings,
                                      mActive_Program)

} // namespace mozilla
