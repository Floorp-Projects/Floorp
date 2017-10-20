/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLContext.h"

#include "GLContext.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/UniquePtrExtensions.h"
#include "nsPrintfCString.h"
#include "WebGLBuffer.h"
#include "WebGLContextUtils.h"
#include "WebGLFramebuffer.h"
#include "WebGLProgram.h"
#include "WebGLRenderbuffer.h"
#include "WebGLShader.h"
#include "WebGLTexture.h"
#include "WebGLTransformFeedback.h"
#include "WebGLVertexArray.h"
#include "WebGLVertexAttribData.h"

#include <algorithm>

namespace mozilla {

// For a Tegra workaround.
static const int MAX_DRAW_CALLS_SINCE_FLUSH = 100;

////////////////////////////////////////

class ScopedResolveTexturesForDraw
{
    struct TexRebindRequest
    {
        uint32_t texUnit;
        WebGLTexture* tex;
    };

    WebGLContext* const mWebGL;
    std::vector<TexRebindRequest> mRebindRequests;

public:
    ScopedResolveTexturesForDraw(WebGLContext* webgl, const char* funcName,
                                 bool* const out_error);
    ~ScopedResolveTexturesForDraw();
};

bool
WebGLTexture::IsFeedback(WebGLContext* webgl, const char* funcName, uint32_t texUnit,
                         const std::vector<const WebGLFBAttachPoint*>& fbAttachments) const
{
    auto itr = fbAttachments.cbegin();
    for (; itr != fbAttachments.cend(); ++itr) {
        const auto& attach = *itr;
        if (attach->Texture() == this)
            break;
    }

    if (itr == fbAttachments.cend())
        return false;

    ////

    const auto minLevel = mBaseMipmapLevel;
    uint32_t maxLevel;
    if (!MaxEffectiveMipmapLevel(texUnit, &maxLevel)) {
        // No valid mips. Will need fake-black.
        return false;
    }

    ////

    for (; itr != fbAttachments.cend(); ++itr) {
        const auto& attach = *itr;
        if (attach->Texture() != this)
            continue;

        const auto dstLevel = attach->MipLevel();

        if (minLevel <= dstLevel && dstLevel <= maxLevel) {
            webgl->ErrorInvalidOperation("%s: Feedback loop detected between tex target"
                                         " 0x%04x, tex unit %u, levels %u-%u; and"
                                         " framebuffer attachment 0x%04x, level %u.",
                                         funcName, mTarget.get(), texUnit, minLevel,
                                         maxLevel, attach->mAttachmentPoint, dstLevel);
            return true;
        }
    }

    return false;
}

ScopedResolveTexturesForDraw::ScopedResolveTexturesForDraw(WebGLContext* webgl,
                                                           const char* funcName,
                                                           bool* const out_error)
    : mWebGL(webgl)
{
    MOZ_ASSERT(mWebGL->gl->IsCurrent());

    if (!mWebGL->mActiveProgramLinkInfo) {
        mWebGL->ErrorInvalidOperation("%s: The current program is not linked.", funcName);
        *out_error = true;
        return;
    }

    const std::vector<const WebGLFBAttachPoint*>* attachList = nullptr;
    const auto& fb = mWebGL->mBoundDrawFramebuffer;
    if (fb) {
        if (!fb->ValidateAndInitAttachments(funcName)) {
            *out_error = true;
            return;
        }

        attachList = &(fb->ResolvedCompleteData()->texDrawBuffers);
    } else {
        webgl->ClearBackbufferIfNeeded();
    }

    MOZ_ASSERT(mWebGL->mActiveProgramLinkInfo);
    const auto& uniformSamplers = mWebGL->mActiveProgramLinkInfo->uniformSamplers;
    for (const auto& uniform : uniformSamplers) {
        const auto& texList = *(uniform->mSamplerTexList);

        for (const auto& texUnit : uniform->mSamplerValues) {
            if (texUnit >= texList.Length())
                continue;

            const auto& tex = texList[texUnit];
            if (!tex)
                continue;

            if (attachList &&
                tex->IsFeedback(mWebGL, funcName, texUnit, *attachList))
            {
                *out_error = true;
                return;
            }

            FakeBlackType fakeBlack;
            if (!tex->ResolveForDraw(funcName, texUnit, &fakeBlack)) {
                mWebGL->ErrorOutOfMemory("%s: Failed to resolve textures for draw.",
                                         funcName);
                *out_error = true;
                return;
            }

            if (fakeBlack == FakeBlackType::None)
                continue;

            if (!mWebGL->BindFakeBlack(texUnit, tex->Target(), fakeBlack)) {
                mWebGL->ErrorOutOfMemory("%s: Failed to create fake black texture.",
                                         funcName);
                *out_error = true;
                return;
            }

            mRebindRequests.push_back({texUnit, tex});
        }
    }

    *out_error = false;
}

ScopedResolveTexturesForDraw::~ScopedResolveTexturesForDraw()
{
    if (mRebindRequests.empty())
        return;

    gl::GLContext* gl = mWebGL->gl;

    for (const auto& itr : mRebindRequests) {
        gl->fActiveTexture(LOCAL_GL_TEXTURE0 + itr.texUnit);
        gl->fBindTexture(itr.tex->Target().get(), itr.tex->mGLName);
    }

    gl->fActiveTexture(LOCAL_GL_TEXTURE0 + mWebGL->mActiveTexture);
}

bool
WebGLContext::BindFakeBlack(uint32_t texUnit, TexTarget target, FakeBlackType fakeBlack)
{
    MOZ_ASSERT(fakeBlack == FakeBlackType::RGBA0000 ||
               fakeBlack == FakeBlackType::RGBA0001);

    const auto fnGetSlot = [this, target, fakeBlack]() -> UniquePtr<FakeBlackTexture>*
    {
        switch (fakeBlack) {
        case FakeBlackType::RGBA0000:
            switch (target.get()) {
            case LOCAL_GL_TEXTURE_2D      : return &mFakeBlack_2D_0000;
            case LOCAL_GL_TEXTURE_CUBE_MAP: return &mFakeBlack_CubeMap_0000;
            case LOCAL_GL_TEXTURE_3D      : return &mFakeBlack_3D_0000;
            case LOCAL_GL_TEXTURE_2D_ARRAY: return &mFakeBlack_2D_Array_0000;
            default: return nullptr;
            }

        case FakeBlackType::RGBA0001:
            switch (target.get()) {
            case LOCAL_GL_TEXTURE_2D      : return &mFakeBlack_2D_0001;
            case LOCAL_GL_TEXTURE_CUBE_MAP: return &mFakeBlack_CubeMap_0001;
            case LOCAL_GL_TEXTURE_3D      : return &mFakeBlack_3D_0001;
            case LOCAL_GL_TEXTURE_2D_ARRAY: return &mFakeBlack_2D_Array_0001;
            default: return nullptr;
            }

        default:
            return nullptr;
        }
    };

    UniquePtr<FakeBlackTexture>* slot = fnGetSlot();
    if (!slot) {
        MOZ_CRASH("GFX: fnGetSlot failed.");
    }
    UniquePtr<FakeBlackTexture>& fakeBlackTex = *slot;

    if (!fakeBlackTex) {
        fakeBlackTex = FakeBlackTexture::Create(gl, target, fakeBlack);
        if (!fakeBlackTex) {
            return false;
        }
    }

    gl->fActiveTexture(LOCAL_GL_TEXTURE0 + texUnit);
    gl->fBindTexture(target.get(), fakeBlackTex->mGLName);
    gl->fActiveTexture(LOCAL_GL_TEXTURE0 + mActiveTexture);
    return true;
}

////////////////////////////////////////

bool
WebGLContext::DrawInstanced_check(const char* info)
{
    MOZ_ASSERT(IsWebGL2() ||
               IsExtensionEnabled(WebGLExtensionID::ANGLE_instanced_arrays));
    if (!mBufferFetchingHasPerVertex) {
        /* http://www.khronos.org/registry/gles/extensions/ANGLE/ANGLE_instanced_arrays.txt
         *  If all of the enabled vertex attribute arrays that are bound to active
         *  generic attributes in the program have a non-zero divisor, the draw
         *  call should return INVALID_OPERATION.
         *
         * NB: This also appears to apply to NV_instanced_arrays, though the
         * INVALID_OPERATION emission is not explicitly stated.
         * ARB_instanced_arrays does not have this restriction.
         */
        ErrorInvalidOperation("%s: at least one vertex attribute divisor should be 0", info);
        return false;
    }

    return true;
}

bool
WebGLContext::DrawArrays_check(const char* funcName, GLenum mode, GLint first,
                               GLsizei vertCount, GLsizei instanceCount)
{
    if (!ValidateDrawModeEnum(mode, funcName))
        return false;

    if (!ValidateNonNegative(funcName, "first", first) ||
        !ValidateNonNegative(funcName, "vertCount", vertCount) ||
        !ValidateNonNegative(funcName, "instanceCount", instanceCount))
    {
        return false;
    }

    if (!ValidateStencilParamsForDrawCall())
        return false;

    if (IsWebGL2() && !gl->IsSupported(gl::GLFeature::prim_restart_fixed)) {
        MOZ_ASSERT(gl->IsSupported(gl::GLFeature::prim_restart));
        if (mPrimRestartTypeBytes != 0) {
            mPrimRestartTypeBytes = 0;

            // OSX appears to have severe perf issues with leaving this enabled.
            gl->fDisable(LOCAL_GL_PRIMITIVE_RESTART);
        }
    }

    if (!vertCount || !instanceCount)
        return false; // No error, just early out.

    if (!ValidateBufferFetching(funcName))
        return false;

    const auto checked_firstPlusCount = CheckedInt<GLsizei>(first) + vertCount;
    if (!checked_firstPlusCount.isValid()) {
        ErrorInvalidOperation("%s: overflow in first+vertCount", funcName);
        return false;
    }

    if (uint32_t(checked_firstPlusCount.value()) > mMaxFetchedVertices) {
        ErrorInvalidOperation("%s: Bound vertex attribute buffers do not have sufficient"
                              " size for given first and count.",
                              funcName);
        return false;
    }

    return true;
}

////////////////////////////////////////

template<typename T>
static bool
DoSetsIntersect(const std::set<T>& a, const std::set<T>& b)
{
    std::vector<T> intersection;
    std::set_intersection(a.begin(), a.end(), b.begin(), b.end(),
                          std::back_inserter(intersection));
    return bool(intersection.size());
}

class ScopedDrawHelper final
{
    WebGLContext* const mWebGL;
    bool mDidFake;

public:
    ScopedDrawHelper(WebGLContext* webgl, const char* funcName, uint32_t firstVertex,
                     uint32_t vertCount, uint32_t instanceCount, bool* const out_error)
        : mWebGL(webgl)
        , mDidFake(false)
    {
        if (instanceCount > mWebGL->mMaxFetchedInstances) {
            mWebGL->ErrorInvalidOperation("%s: Bound instance attribute buffers do not"
                                          " have sufficient size for given"
                                          " `instanceCount`.",
                                          funcName);
            *out_error = true;
            return;
        }

        MOZ_ASSERT(mWebGL->gl->IsCurrent());

        if (mWebGL->mBoundDrawFramebuffer) {
            if (!mWebGL->mBoundDrawFramebuffer->ValidateAndInitAttachments(funcName)) {
                *out_error = true;
                return;
            }
        } else {
            mWebGL->ClearBackbufferIfNeeded();
        }

        ////

        const size_t requiredVerts = firstVertex + vertCount;
        if (!mWebGL->DoFakeVertexAttrib0(funcName, requiredVerts)) {
            *out_error = true;
            return;
        }
        mDidFake = true;

        ////
        // Check UBO sizes.

        const auto& linkInfo = mWebGL->mActiveProgramLinkInfo;

        for (const auto& cur : linkInfo->uniformBlocks) {
            const auto& dataSize = cur->mDataSize;
            const auto& binding = cur->mBinding;
            if (!binding) {
                mWebGL->ErrorInvalidOperation("%s: Buffer for uniform block is null.",
                                              funcName);
                *out_error = true;
                return;
            }

            const auto availByteCount = binding->ByteCount();
            if (dataSize > availByteCount) {
                mWebGL->ErrorInvalidOperation("%s: Buffer for uniform block is smaller"
                                              " than UNIFORM_BLOCK_DATA_SIZE.",
                                              funcName);
                *out_error = true;
                return;
            }

            if (binding->mBufferBinding->IsBoundForTF()) {
                mWebGL->ErrorInvalidOperation("%s: Buffer for uniform block is bound or"
                                              " in use for transform feedback.",
                                              funcName);
                *out_error = true;
                return;
            }
        }

        ////

        const auto& tfo = mWebGL->mBoundTransformFeedback;
        if (tfo && tfo->IsActiveAndNotPaused()) {
            uint32_t numUsed;
            switch (linkInfo->transformFeedbackBufferMode) {
            case LOCAL_GL_INTERLEAVED_ATTRIBS:
                numUsed = 1;
                break;

            case LOCAL_GL_SEPARATE_ATTRIBS:
                numUsed = linkInfo->transformFeedbackVaryings.size();
                break;

            default:
                MOZ_CRASH();
            }

            for (uint32_t i = 0; i < numUsed; ++i) {
                const auto& buffer = tfo->mIndexedBindings[i].mBufferBinding;
                if (buffer->IsBoundForNonTF()) {
                    mWebGL->ErrorInvalidOperation("%s: Transform feedback varying %u's"
                                                  " buffer is bound for"
                                                  " non-transform-feedback.",
                                                  funcName, i);
                    *out_error = true;
                    return;
                }
            }
        }

        ////

        for (const auto& progAttrib : linkInfo->attribs) {
            const auto& loc = progAttrib.mLoc;
            if (loc == -1)
                continue;

            const auto& attribData = mWebGL->mBoundVertexArray->mAttribs[loc];

            GLenum attribDataBaseType;
            if (attribData.mEnabled) {
                attribDataBaseType = attribData.BaseType();

                if (attribData.mBuf->IsBoundForTF()) {
                    mWebGL->ErrorInvalidOperation("%s: Vertex attrib %u's buffer is bound"
                                                  " or in use for transform feedback.",
                                                  funcName, loc);
                    *out_error = true;
                    return;
                }
            } else {
                attribDataBaseType = mWebGL->mGenericVertexAttribTypes[loc];
            }

            if (attribDataBaseType != progAttrib.mBaseType) {
                nsCString progType, dataType;
                WebGLContext::EnumName(progAttrib.mBaseType, &progType);
                WebGLContext::EnumName(attribDataBaseType, &dataType);
                mWebGL->ErrorInvalidOperation("%s: Vertex attrib %u requires data of type"
                                              " %s, but is being supplied with type %s.",
                                              funcName, loc, progType.BeginReading(),
                                              dataType.BeginReading());
                *out_error = true;
                return;
            }
        }

        ////

        mWebGL->RunContextLossTimer();
    }

    ~ScopedDrawHelper() {
        if (mDidFake) {
            mWebGL->UndoFakeVertexAttrib0();
        }
    }
};

////////////////////////////////////////

static uint32_t
UsedVertsForTFDraw(GLenum mode, uint32_t vertCount)
{
    uint8_t vertsPerPrim;

    switch (mode) {
    case LOCAL_GL_POINTS:
        vertsPerPrim = 1;
        break;
    case LOCAL_GL_LINES:
        vertsPerPrim = 2;
        break;
    case LOCAL_GL_TRIANGLES:
        vertsPerPrim = 3;
        break;
    default:
        MOZ_CRASH("`mode`");
    }

    return vertCount / vertsPerPrim * vertsPerPrim;
}

class ScopedDrawWithTransformFeedback final
{
    WebGLContext* const mWebGL;
    WebGLTransformFeedback* const mTFO;
    const bool mWithTF;
    uint32_t mUsedVerts;

public:
    ScopedDrawWithTransformFeedback(WebGLContext* webgl, const char* funcName,
                                    GLenum mode, uint32_t vertCount,
                                    uint32_t instanceCount, bool* const out_error)
        : mWebGL(webgl)
        , mTFO(mWebGL->mBoundTransformFeedback)
        , mWithTF(mTFO &&
                  mTFO->mIsActive &&
                  !mTFO->mIsPaused)
        , mUsedVerts(0)
    {
        *out_error = false;
        if (!mWithTF)
            return;

        if (mode != mTFO->mActive_PrimMode) {
            mWebGL->ErrorInvalidOperation("%s: Drawing with transform feedback requires"
                                          " `mode` to match BeginTransformFeedback's"
                                          " `primitiveMode`.",
                                          funcName);
            *out_error = true;
            return;
        }

        const auto usedVertsPerInstance = UsedVertsForTFDraw(mode, vertCount);
        const auto usedVerts = CheckedInt<uint32_t>(usedVertsPerInstance) * instanceCount;

        const auto remainingCapacity = mTFO->mActive_VertCapacity - mTFO->mActive_VertPosition;
        if (!usedVerts.isValid() ||
            usedVerts.value() > remainingCapacity)
        {
            mWebGL->ErrorInvalidOperation("%s: Insufficient buffer capacity remaining for"
                                          " transform feedback.",
                                          funcName);
            *out_error = true;
            return;
        }

        mUsedVerts = usedVerts.value();
    }

    void Advance() const {
        if (!mWithTF)
            return;

        mTFO->mActive_VertPosition += mUsedVerts;
    }
};

////////////////////////////////////////

void
WebGLContext::DrawArrays(GLenum mode, GLint first, GLsizei vertCount)
{
    AUTO_PROFILER_LABEL("WebGLContext::DrawArrays", GRAPHICS);
    const char funcName[] = "drawArrays";
    if (IsContextLost())
        return;

    MakeContextCurrent();

    bool error = false;
    ScopedResolveTexturesForDraw scopedResolve(this, funcName, &error);
    if (error)
        return;

    const GLsizei instanceCount = 1;
    if (!DrawArrays_check(funcName, mode, first, vertCount, instanceCount))
        return;

    const ScopedDrawHelper scopedHelper(this, funcName, first, vertCount, instanceCount, &error);
    if (error)
        return;

    const ScopedDrawWithTransformFeedback scopedTF(this, funcName, mode, vertCount,
                                                   instanceCount, &error);
    if (error)
        return;

    {
        ScopedDrawCallWrapper wrapper(*this);
        AUTO_PROFILER_LABEL("glDrawArrays", GRAPHICS);
        gl->fDrawArrays(mode, first, vertCount);
    }

    Draw_cleanup(funcName);
    scopedTF.Advance();
}

void
WebGLContext::DrawArraysInstanced(GLenum mode, GLint first, GLsizei vertCount,
                                  GLsizei instanceCount)
{
    AUTO_PROFILER_LABEL("WebGLContext::DrawArraysInstanced", GRAPHICS);
    const char funcName[] = "drawArraysInstanced";
    if (IsContextLost())
        return;

    MakeContextCurrent();

    bool error = false;
    ScopedResolveTexturesForDraw scopedResolve(this, funcName, &error);
    if (error)
        return;

    if (!DrawArrays_check(funcName, mode, first, vertCount, instanceCount))
        return;

    if (!DrawInstanced_check(funcName))
        return;

    const ScopedDrawHelper scopedHelper(this, funcName, first, vertCount, instanceCount, &error);
    if (error)
        return;

    const ScopedDrawWithTransformFeedback scopedTF(this, funcName, mode, vertCount,
                                                   instanceCount, &error);
    if (error)
        return;

    {
        ScopedDrawCallWrapper wrapper(*this);
        AUTO_PROFILER_LABEL("glDrawArraysInstanced", GRAPHICS);
        gl->fDrawArraysInstanced(mode, first, vertCount, instanceCount);
    }

    Draw_cleanup(funcName);
    scopedTF.Advance();
}

////////////////////////////////////////

bool
WebGLContext::DrawElements_check(const char* funcName, GLenum mode, GLsizei vertCount,
                                 GLenum type, WebGLintptr byteOffset,
                                 GLsizei instanceCount)
{
    if (!ValidateDrawModeEnum(mode, funcName))
        return false;

    if (mBoundTransformFeedback &&
        mBoundTransformFeedback->mIsActive &&
        !mBoundTransformFeedback->mIsPaused)
    {
        ErrorInvalidOperation("%s: DrawElements* functions are incompatible with"
                              " transform feedback.",
                              funcName);
        return false;
    }

    if (!ValidateNonNegative(funcName, "vertCount", vertCount) ||
        !ValidateNonNegative(funcName, "byteOffset", byteOffset) ||
        !ValidateNonNegative(funcName, "instanceCount", instanceCount))
    {
        return false;
    }

    if (!ValidateStencilParamsForDrawCall())
        return false;

    if (!vertCount || !instanceCount)
        return false; // No error, just early out.

    uint8_t bytesPerElem = 0;
    switch (type) {
    case LOCAL_GL_UNSIGNED_BYTE:
        bytesPerElem = 1;
        break;

    case LOCAL_GL_UNSIGNED_SHORT:
        bytesPerElem = 2;
        break;

    case LOCAL_GL_UNSIGNED_INT:
        if (IsWebGL2() || IsExtensionEnabled(WebGLExtensionID::OES_element_index_uint)) {
            bytesPerElem = 4;
        }
        break;
    }

    if (!bytesPerElem) {
        ErrorInvalidEnum("%s: Invalid `type`: 0x%04x", funcName, type);
        return false;
    }

    if (byteOffset % bytesPerElem != 0) {
        ErrorInvalidOperation("%s: `byteOffset` must be a multiple of the size of `type`",
                              funcName);
        return false;
    }

    ////

    if (IsWebGL2() && !gl->IsSupported(gl::GLFeature::prim_restart_fixed)) {
        MOZ_ASSERT(gl->IsSupported(gl::GLFeature::prim_restart));
        if (mPrimRestartTypeBytes != bytesPerElem) {
            mPrimRestartTypeBytes = bytesPerElem;

            const uint32_t ones = UINT32_MAX >> (32 - 8*mPrimRestartTypeBytes);
            gl->fEnable(LOCAL_GL_PRIMITIVE_RESTART);
            gl->fPrimitiveRestartIndex(ones);
        }
    }

    ////

    const GLsizei first = byteOffset / bytesPerElem;
    const CheckedUint32 checked_byteCount = bytesPerElem * CheckedUint32(vertCount);

    if (!checked_byteCount.isValid()) {
        ErrorInvalidValue("%s: Overflow in byteCount.", funcName);
        return false;
    }

    if (!mBoundVertexArray->mElementArrayBuffer) {
        ErrorInvalidOperation("%s: Must have element array buffer binding.", funcName);
        return false;
    }

    WebGLBuffer& elemArrayBuffer = *mBoundVertexArray->mElementArrayBuffer;

    if (!elemArrayBuffer.ByteLength()) {
        ErrorInvalidOperation("%s: Bound element array buffer doesn't have any data.",
                              funcName);
        return false;
    }

    CheckedInt<GLsizei> checked_neededByteCount = checked_byteCount.toChecked<GLsizei>() + byteOffset;

    if (!checked_neededByteCount.isValid()) {
        ErrorInvalidOperation("%s: Overflow in byteOffset+byteCount.", funcName);
        return false;
    }

    if (uint32_t(checked_neededByteCount.value()) > elemArrayBuffer.ByteLength()) {
        ErrorInvalidOperation("%s: Bound element array buffer is too small for given"
                              " count and offset.",
                              funcName);
        return false;
    }

    if (!ValidateBufferFetching(funcName))
        return false;

    if (!mMaxFetchedVertices ||
        !elemArrayBuffer.ValidateIndexedFetch(type, mMaxFetchedVertices, first, vertCount))
    {
        ErrorInvalidOperation("%s: bound vertex attribute buffers do not have sufficient "
                              "size for given indices from the bound element array",
                              funcName);
        return false;
    }

    return true;
}

static void
HandleDrawElementsErrors(WebGLContext* webgl, const char* funcName,
                         gl::GLContext::LocalErrorScope& errorScope)
{
    const auto err = errorScope.GetError();
    if (err == LOCAL_GL_INVALID_OPERATION) {
        webgl->ErrorInvalidOperation("%s: Driver rejected indexed draw call, possibly"
                                     " due to out-of-bounds indices.", funcName);
        return;
    }

    MOZ_ASSERT(!err);
    if (err) {
        webgl->ErrorImplementationBug("%s: Unexpected driver error during indexed draw"
                                      " call. Please file a bug.",
                                      funcName);
        return;
    }
}

void
WebGLContext::DrawElements(GLenum mode, GLsizei vertCount, GLenum type,
                           WebGLintptr byteOffset, const char* funcName)
{
    AUTO_PROFILER_LABEL("WebGLContext::DrawElements", GRAPHICS);
    if (!funcName) {
        funcName = "drawElements";
    }

    if (IsContextLost())
        return;

    MakeContextCurrent();

    bool error = false;
    ScopedResolveTexturesForDraw scopedResolve(this, funcName, &error);
    if (error)
        return;

    const GLsizei instanceCount = 1;
    if (!DrawElements_check(funcName, mode, vertCount, type, byteOffset, instanceCount))
        return;

    const ScopedDrawHelper scopedHelper(this, funcName, 0, mMaxFetchedVertices, instanceCount,
                                        &error);
    if (error)
        return;

    {
        ScopedDrawCallWrapper wrapper(*this);
        {
            UniquePtr<gl::GLContext::LocalErrorScope> errorScope;

            if (gl->IsANGLE()) {
                errorScope.reset(new gl::GLContext::LocalErrorScope(*gl));
            }

            AUTO_PROFILER_LABEL("glDrawElements", GRAPHICS);
            gl->fDrawElements(mode, vertCount, type,
                              reinterpret_cast<GLvoid*>(byteOffset));

            if (errorScope) {
                HandleDrawElementsErrors(this, funcName, *errorScope);
            }
        }
    }

    Draw_cleanup(funcName);
}

void
WebGLContext::DrawElementsInstanced(GLenum mode, GLsizei vertCount, GLenum type,
                                    WebGLintptr byteOffset, GLsizei instanceCount)
{
    AUTO_PROFILER_LABEL("WebGLContext::DrawElementsInstanced", GRAPHICS);
    const char funcName[] = "drawElementsInstanced";
    if (IsContextLost())
        return;

    MakeContextCurrent();

    bool error = false;
    ScopedResolveTexturesForDraw scopedResolve(this, funcName, &error);
    if (error)
        return;

    if (!DrawElements_check(funcName, mode, vertCount, type, byteOffset, instanceCount))
        return;

    if (!DrawInstanced_check(funcName))
        return;

    const ScopedDrawHelper scopedHelper(this, funcName, 0, mMaxFetchedVertices, instanceCount,
                                        &error);
    if (error)
        return;

    {
        ScopedDrawCallWrapper wrapper(*this);
        {
            UniquePtr<gl::GLContext::LocalErrorScope> errorScope;

            if (gl->IsANGLE()) {
                errorScope.reset(new gl::GLContext::LocalErrorScope(*gl));
            }

            AUTO_PROFILER_LABEL("glDrawElementsInstanced", GRAPHICS);
            gl->fDrawElementsInstanced(mode, vertCount, type,
                                       reinterpret_cast<GLvoid*>(byteOffset),
                                       instanceCount);

            if (errorScope) {
                HandleDrawElementsErrors(this, funcName, *errorScope);
            }
        }
    }

    Draw_cleanup(funcName);
}

////////////////////////////////////////

void
WebGLContext::Draw_cleanup(const char* funcName)
{
    if (gl->WorkAroundDriverBugs()) {
        if (gl->Renderer() == gl::GLRenderer::Tegra) {
            mDrawCallsSinceLastFlush++;

            if (mDrawCallsSinceLastFlush >= MAX_DRAW_CALLS_SINCE_FLUSH) {
                gl->fFlush();
                mDrawCallsSinceLastFlush = 0;
            }
        }
    }

    // Let's check for a really common error: Viewport is larger than the actual
    // destination framebuffer.
    uint32_t destWidth = mViewportWidth;
    uint32_t destHeight = mViewportHeight;

    if (mBoundDrawFramebuffer) {
        const auto& drawBuffers = mBoundDrawFramebuffer->ColorDrawBuffers();
        for (const auto& cur : drawBuffers) {
            if (!cur->IsDefined())
                continue;
            cur->Size(&destWidth, &destHeight);
            break;
        }
    } else {
        destWidth = mWidth;
        destHeight = mHeight;
    }

    if (mViewportWidth > int32_t(destWidth) ||
        mViewportHeight > int32_t(destHeight))
    {
        if (!mAlreadyWarnedAboutViewportLargerThanDest) {
            GenerateWarning("%s: Drawing to a destination rect smaller than the viewport"
                            " rect. (This warning will only be given once)",
                            funcName);
            mAlreadyWarnedAboutViewportLargerThanDest = true;
        }
    }
}

/*
 * Verify that state is consistent for drawing, and compute max number of elements (maxAllowedCount)
 * that will be legal to be read from bound VBOs.
 */

bool
WebGLContext::ValidateBufferFetching(const char* info)
{
    MOZ_ASSERT(mCurrentProgram);
    // Note that mCurrentProgram->IsLinked() is NOT GUARANTEED.
    MOZ_ASSERT(mActiveProgramLinkInfo);

#ifdef DEBUG
    GLint currentProgram = 0;
    MakeContextCurrent();
    gl->fGetIntegerv(LOCAL_GL_CURRENT_PROGRAM, &currentProgram);
    MOZ_ASSERT(GLuint(currentProgram) == mCurrentProgram->mGLName,
               "WebGL: current program doesn't agree with GL state");
#endif

    if (mBufferFetchingIsVerified)
        return true;

    bool hasPerVertex = false;
    uint32_t maxVertices = UINT32_MAX;
    uint32_t maxInstances = UINT32_MAX;
    const uint32_t attribCount = mBoundVertexArray->mAttribs.Length();

    uint32_t i = 0;
    for (const auto& vd : mBoundVertexArray->mAttribs) {
        // If the attrib array isn't enabled, there's nothing to check;
        // it's a static value.
        if (!vd.mEnabled)
            continue;

        if (!vd.mBuf) {
            ErrorInvalidOperation("%s: no VBO bound to enabled vertex attrib index %du!",
                                  info, i);
            return false;
        }

        ++i;
    }

    mBufferFetch_IsAttrib0Active = false;

    for (const auto& attrib : mActiveProgramLinkInfo->attribs) {
        if (attrib.mLoc == -1)
            continue;

        const uint32_t attribLoc(attrib.mLoc);
        if (attribLoc >= attribCount)
            continue;

        if (attribLoc == 0) {
            mBufferFetch_IsAttrib0Active = true;
        }

        const auto& vd = mBoundVertexArray->mAttribs[attribLoc];
        if (!vd.mEnabled)
            continue;

        const auto& bufByteLen = vd.mBuf->ByteLength();
        if (vd.ByteOffset() > bufByteLen) {
            maxVertices = 0;
            maxInstances = 0;
            break;
        }

        size_t availBytes = bufByteLen - vd.ByteOffset();
        if (vd.BytesPerVertex() > availBytes) {
            maxVertices = 0;
            maxInstances = 0;
            break;
        }
        availBytes -= vd.BytesPerVertex(); // Snip off the tail.
        const size_t vertCapacity = availBytes / vd.ExplicitStride() + 1; // Add +1 for the snipped tail.

        if (vd.mDivisor == 0) {
            if (vertCapacity < maxVertices) {
                maxVertices = vertCapacity;
            }
            hasPerVertex = true;
        } else {
            const auto curMaxInstances = CheckedInt<size_t>(vertCapacity) * vd.mDivisor;
            // If this isn't valid, it's because we overflowed, which means we can support
            // *too much*. Don't update maxInstances in this case.
            if (curMaxInstances.isValid() &&
                curMaxInstances.value() < maxInstances)
            {
                maxInstances = curMaxInstances.value();
            }
        }
    }

    mBufferFetchingIsVerified = true;
    mBufferFetchingHasPerVertex = hasPerVertex;
    mMaxFetchedVertices = maxVertices;
    mMaxFetchedInstances = maxInstances;

    return true;
}

WebGLVertexAttrib0Status
WebGLContext::WhatDoesVertexAttrib0Need() const
{
    MOZ_ASSERT(mCurrentProgram);
    MOZ_ASSERT(mActiveProgramLinkInfo);

    const auto& isAttribArray0Enabled = mBoundVertexArray->mAttribs[0].mEnabled;

    bool legacyAttrib0 = gl->IsCompatibilityProfile();
#ifdef XP_MACOSX
    if (gl->WorkAroundDriverBugs()) {
        // Failures in conformance/attribs/gl-disabled-vertex-attrib.
        // Even in Core profiles on NV. Sigh.
        legacyAttrib0 |= (gl->Vendor() == gl::GLVendor::NVIDIA);
    }
#endif

    if (!legacyAttrib0)
        return WebGLVertexAttrib0Status::Default;

    if (isAttribArray0Enabled && mBufferFetch_IsAttrib0Active)
        return WebGLVertexAttrib0Status::Default;

    if (mBufferFetch_IsAttrib0Active)
        return WebGLVertexAttrib0Status::EmulatedInitializedArray;

    // Ensure that the legacy code has enough buffer.
    return WebGLVertexAttrib0Status::EmulatedUninitializedArray;
}

bool
WebGLContext::DoFakeVertexAttrib0(const char* funcName, GLuint vertexCount)
{
    if (!vertexCount) {
        vertexCount = 1;
    }

    const auto whatDoesAttrib0Need = WhatDoesVertexAttrib0Need();
    if (MOZ_LIKELY(whatDoesAttrib0Need == WebGLVertexAttrib0Status::Default))
        return true;

    if (!mAlreadyWarnedAboutFakeVertexAttrib0) {
        GenerateWarning("Drawing without vertex attrib 0 array enabled forces the browser "
                        "to do expensive emulation work when running on desktop OpenGL "
                        "platforms, for example on Mac. It is preferable to always draw "
                        "with vertex attrib 0 array enabled, by using bindAttribLocation "
                        "to bind some always-used attribute to location 0.");
        mAlreadyWarnedAboutFakeVertexAttrib0 = true;
    }

    gl->fEnableVertexAttribArray(0);

    if (!mFakeVertexAttrib0BufferObject) {
        gl->fGenBuffers(1, &mFakeVertexAttrib0BufferObject);
        mFakeVertexAttrib0BufferObjectSize = 0;
    }
    gl->fBindBuffer(LOCAL_GL_ARRAY_BUFFER, mFakeVertexAttrib0BufferObject);

    ////

    switch (mGenericVertexAttribTypes[0]) {
    case LOCAL_GL_FLOAT:
        gl->fVertexAttribPointer(0, 4, LOCAL_GL_FLOAT, false, 0, 0);
        break;

    case LOCAL_GL_INT:
        gl->fVertexAttribIPointer(0, 4, LOCAL_GL_INT, 0, 0);
        break;

    case LOCAL_GL_UNSIGNED_INT:
        gl->fVertexAttribIPointer(0, 4, LOCAL_GL_UNSIGNED_INT, 0, 0);
        break;

    default:
        MOZ_CRASH();
    }

    ////

    const auto bytesPerVert = sizeof(mFakeVertexAttrib0Data);
    const auto checked_dataSize = CheckedUint32(vertexCount) * bytesPerVert;
    if (!checked_dataSize.isValid()) {
        ErrorOutOfMemory("Integer overflow trying to construct a fake vertex attrib 0 array for a draw-operation "
                         "with %d vertices. Try reducing the number of vertices.", vertexCount);
        return false;
    }
    const auto dataSize = checked_dataSize.value();

    if (mFakeVertexAttrib0BufferObjectSize < dataSize) {
        gl->fBufferData(LOCAL_GL_ARRAY_BUFFER, dataSize, nullptr, LOCAL_GL_DYNAMIC_DRAW);
        mFakeVertexAttrib0BufferObjectSize = dataSize;
        mFakeVertexAttrib0DataDefined = false;
    }

    if (whatDoesAttrib0Need == WebGLVertexAttrib0Status::EmulatedUninitializedArray)
        return true;

    ////

    if (mFakeVertexAttrib0DataDefined &&
        memcmp(mFakeVertexAttrib0Data, mGenericVertexAttrib0Data, bytesPerVert) == 0)
    {
        return true;
    }

    ////

    const UniqueBuffer data(malloc(dataSize));
    if (!data) {
        ErrorOutOfMemory("%s: Failed to allocate fake vertex attrib 0 array.",
                         funcName);
        return false;
    }
    auto itr = (uint8_t*)data.get();
    const auto itrEnd = itr + dataSize;
    while (itr != itrEnd) {
        memcpy(itr, mGenericVertexAttrib0Data, bytesPerVert);
        itr += bytesPerVert;
    }

    {
        gl::GLContext::LocalErrorScope errorScope(*gl);

        gl->fBufferSubData(LOCAL_GL_ARRAY_BUFFER, 0, dataSize, data.get());

        const auto err = errorScope.GetError();
        if (err) {
            ErrorOutOfMemory("%s: Failed to upload fake vertex attrib 0 data.", funcName);
            return false;
        }
    }

    ////

    memcpy(mFakeVertexAttrib0Data, mGenericVertexAttrib0Data, bytesPerVert);
    mFakeVertexAttrib0DataDefined = true;
    return true;
}

void
WebGLContext::UndoFakeVertexAttrib0()
{
    const auto whatDoesAttrib0Need = WhatDoesVertexAttrib0Need();
    if (MOZ_LIKELY(whatDoesAttrib0Need == WebGLVertexAttrib0Status::Default))
        return;

    if (mBoundVertexArray->mAttribs[0].mBuf) {
        const WebGLVertexAttribData& attrib0 = mBoundVertexArray->mAttribs[0];
        gl->fBindBuffer(LOCAL_GL_ARRAY_BUFFER, attrib0.mBuf->mGLName);
        attrib0.DoVertexAttribPointer(gl, 0);
    } else {
        gl->fBindBuffer(LOCAL_GL_ARRAY_BUFFER, 0);
    }

    gl->fBindBuffer(LOCAL_GL_ARRAY_BUFFER, mBoundArrayBuffer ? mBoundArrayBuffer->mGLName : 0);
}

static GLuint
CreateGLTexture(gl::GLContext* gl)
{
    MOZ_ASSERT(gl->IsCurrent());
    GLuint ret = 0;
    gl->fGenTextures(1, &ret);
    return ret;
}

UniquePtr<WebGLContext::FakeBlackTexture>
WebGLContext::FakeBlackTexture::Create(gl::GLContext* gl, TexTarget target,
                                       FakeBlackType type)
{
    GLenum texFormat;
    switch (type) {
    case FakeBlackType::RGBA0000:
        texFormat = LOCAL_GL_RGBA;
        break;

    case FakeBlackType::RGBA0001:
        texFormat = LOCAL_GL_RGB;
        break;

    default:
        MOZ_CRASH("GFX: bad type");
    }

    UniquePtr<FakeBlackTexture> result(new FakeBlackTexture(gl));
    gl::ScopedBindTexture scopedBind(gl, result->mGLName, target.get());

    gl->fTexParameteri(target.get(), LOCAL_GL_TEXTURE_MIN_FILTER, LOCAL_GL_NEAREST);
    gl->fTexParameteri(target.get(), LOCAL_GL_TEXTURE_MAG_FILTER, LOCAL_GL_NEAREST);

    // We allocate our zeros on the heap, and we overallocate (16 bytes instead of 4) to
    // minimize the risk of running into a driver bug in texImage2D, as it is a bit
    // unusual maybe to create 1x1 textures, and the stack may not have the alignment that
    // TexImage2D expects.

    const webgl::DriverUnpackInfo dui = {texFormat, texFormat, LOCAL_GL_UNSIGNED_BYTE};
    UniqueBuffer zeros = moz_xcalloc(1, 16); // Infallible allocation.

    MOZ_ASSERT(gl->IsCurrent());

    if (target == LOCAL_GL_TEXTURE_CUBE_MAP) {
        for (int i = 0; i < 6; ++i) {
            const TexImageTarget curTarget = LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_X + i;
            const GLenum error = DoTexImage(gl, curTarget.get(), 0, &dui, 1, 1, 1,
                                            zeros.get());
            if (error) {
                return nullptr;
            }
        }
    } else {
        const GLenum error = DoTexImage(gl, target.get(), 0, &dui, 1, 1, 1,
                                        zeros.get());
        if (error) {
            return nullptr;
        }
    }

    return result;
}

WebGLContext::FakeBlackTexture::FakeBlackTexture(gl::GLContext* gl)
    : mGL(gl)
    , mGLName(CreateGLTexture(gl))
{
}

WebGLContext::FakeBlackTexture::~FakeBlackTexture()
{
    mGL->MakeCurrent();
    mGL->fDeleteTextures(1, &mGLName);
}

} // namespace mozilla
