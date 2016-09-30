/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLContext.h"

#include <algorithm>
#include "angle/ShaderLang.h"
#include "CanvasUtils.h"
#include "gfxPrefs.h"
#include "GLContext.h"
#include "jsfriendapi.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/Preferences.h"
#include "mozilla/Services.h"
#include "nsIObserverService.h"
#include "nsPrintfCString.h"
#include "WebGLActiveInfo.h"
#include "WebGLBuffer.h"
#include "WebGLContextUtils.h"
#include "WebGLFramebuffer.h"
#include "WebGLProgram.h"
#include "WebGLRenderbuffer.h"
#include "WebGLSampler.h"
#include "WebGLShader.h"
#include "WebGLTexture.h"
#include "WebGLUniformLocation.h"
#include "WebGLValidateStrings.h"
#include "WebGLVertexArray.h"
#include "WebGLVertexAttribData.h"

#if defined(MOZ_WIDGET_COCOA)
#include "nsCocoaFeatures.h"
#endif

namespace mozilla {

bool
WebGLContext::ValidateBlendEquationEnum(GLenum mode, const char* info)
{
    switch (mode) {
    case LOCAL_GL_FUNC_ADD:
    case LOCAL_GL_FUNC_SUBTRACT:
    case LOCAL_GL_FUNC_REVERSE_SUBTRACT:
        return true;

    case LOCAL_GL_MIN:
    case LOCAL_GL_MAX:
        if (IsWebGL2() ||
            IsExtensionEnabled(WebGLExtensionID::EXT_blend_minmax))
        {
            return true;
        }

        break;

    default:
        break;
    }

    ErrorInvalidEnumInfo(info, mode);
    return false;
}

bool
WebGLContext::ValidateBlendFuncDstEnum(GLenum factor, const char* info)
{
    switch (factor) {
    case LOCAL_GL_ZERO:
    case LOCAL_GL_ONE:
    case LOCAL_GL_SRC_COLOR:
    case LOCAL_GL_ONE_MINUS_SRC_COLOR:
    case LOCAL_GL_DST_COLOR:
    case LOCAL_GL_ONE_MINUS_DST_COLOR:
    case LOCAL_GL_SRC_ALPHA:
    case LOCAL_GL_ONE_MINUS_SRC_ALPHA:
    case LOCAL_GL_DST_ALPHA:
    case LOCAL_GL_ONE_MINUS_DST_ALPHA:
    case LOCAL_GL_CONSTANT_COLOR:
    case LOCAL_GL_ONE_MINUS_CONSTANT_COLOR:
    case LOCAL_GL_CONSTANT_ALPHA:
    case LOCAL_GL_ONE_MINUS_CONSTANT_ALPHA:
        return true;

    default:
        ErrorInvalidEnumInfo(info, factor);
        return false;
    }
}

bool
WebGLContext::ValidateBlendFuncSrcEnum(GLenum factor, const char* info)
{
    if (factor == LOCAL_GL_SRC_ALPHA_SATURATE)
        return true;

    return ValidateBlendFuncDstEnum(factor, info);
}

bool
WebGLContext::ValidateBlendFuncEnumsCompatibility(GLenum sfactor,
                                                  GLenum dfactor,
                                                  const char* info)
{
    bool sfactorIsConstantColor = sfactor == LOCAL_GL_CONSTANT_COLOR ||
                                  sfactor == LOCAL_GL_ONE_MINUS_CONSTANT_COLOR;
    bool sfactorIsConstantAlpha = sfactor == LOCAL_GL_CONSTANT_ALPHA ||
                                  sfactor == LOCAL_GL_ONE_MINUS_CONSTANT_ALPHA;
    bool dfactorIsConstantColor = dfactor == LOCAL_GL_CONSTANT_COLOR ||
                                  dfactor == LOCAL_GL_ONE_MINUS_CONSTANT_COLOR;
    bool dfactorIsConstantAlpha = dfactor == LOCAL_GL_CONSTANT_ALPHA ||
                                  dfactor == LOCAL_GL_ONE_MINUS_CONSTANT_ALPHA;
    if ( (sfactorIsConstantColor && dfactorIsConstantAlpha) ||
         (dfactorIsConstantColor && sfactorIsConstantAlpha) )
    {
        ErrorInvalidOperation("%s are mutually incompatible, see section 6.8 in"
                              " the WebGL 1.0 spec", info);
        return false;
    }

    return true;
}

/**
 * Check data ranges [readOffset, readOffset + size] and [writeOffset,
 * writeOffset + size] for overlap.
 *
 * It is assumed that offset and size have already been validated.
 */
bool
WebGLContext::ValidateDataRanges(WebGLintptr readOffset, WebGLintptr writeOffset, WebGLsizeiptr size, const char* info)
{
    MOZ_ASSERT((CheckedInt<WebGLsizeiptr>(readOffset) + size).isValid());
    MOZ_ASSERT((CheckedInt<WebGLsizeiptr>(writeOffset) + size).isValid());

    bool separate = (readOffset + size < writeOffset || writeOffset + size < readOffset);
    if (!separate) {
        ErrorInvalidValue("%s: ranges [readOffset, readOffset + size) and [writeOffset, "
                          "writeOffset + size) overlap", info);
    }

    return separate;
}

bool
WebGLContext::ValidateTextureTargetEnum(GLenum target, const char* info)
{
    switch (target) {
    case LOCAL_GL_TEXTURE_2D:
    case LOCAL_GL_TEXTURE_CUBE_MAP:
        return true;

    case LOCAL_GL_TEXTURE_3D:
        if (IsWebGL2())
            return true;

        break;

    default:
        break;
    }

    ErrorInvalidEnumInfo(info, target);
    return false;
}

bool
WebGLContext::ValidateComparisonEnum(GLenum target, const char* info)
{
    switch (target) {
    case LOCAL_GL_NEVER:
    case LOCAL_GL_LESS:
    case LOCAL_GL_LEQUAL:
    case LOCAL_GL_GREATER:
    case LOCAL_GL_GEQUAL:
    case LOCAL_GL_EQUAL:
    case LOCAL_GL_NOTEQUAL:
    case LOCAL_GL_ALWAYS:
        return true;

    default:
        ErrorInvalidEnumInfo(info, target);
        return false;
    }
}

bool
WebGLContext::ValidateStencilOpEnum(GLenum action, const char* info)
{
    switch (action) {
    case LOCAL_GL_KEEP:
    case LOCAL_GL_ZERO:
    case LOCAL_GL_REPLACE:
    case LOCAL_GL_INCR:
    case LOCAL_GL_INCR_WRAP:
    case LOCAL_GL_DECR:
    case LOCAL_GL_DECR_WRAP:
    case LOCAL_GL_INVERT:
        return true;

    default:
        ErrorInvalidEnumInfo(info, action);
        return false;
    }
}

bool
WebGLContext::ValidateFaceEnum(GLenum face, const char* info)
{
    switch (face) {
    case LOCAL_GL_FRONT:
    case LOCAL_GL_BACK:
    case LOCAL_GL_FRONT_AND_BACK:
        return true;

    default:
        ErrorInvalidEnumInfo(info, face);
        return false;
    }
}

bool
WebGLContext::ValidateDrawModeEnum(GLenum mode, const char* info)
{
    switch (mode) {
    case LOCAL_GL_TRIANGLES:
    case LOCAL_GL_TRIANGLE_STRIP:
    case LOCAL_GL_TRIANGLE_FAN:
    case LOCAL_GL_POINTS:
    case LOCAL_GL_LINE_STRIP:
    case LOCAL_GL_LINE_LOOP:
    case LOCAL_GL_LINES:
        return true;

    default:
        ErrorInvalidEnumInfo(info, mode);
        return false;
    }
}

bool
WebGLContext::ValidateFramebufferAttachment(const WebGLFramebuffer* fb, GLenum attachment,
                                            const char* funcName,
                                            bool badColorAttachmentIsInvalidOp)
{
    if (!fb) {
        switch (attachment) {
        case LOCAL_GL_COLOR:
        case LOCAL_GL_DEPTH:
        case LOCAL_GL_STENCIL:
            return true;

        default:
            ErrorInvalidEnum("%s: attachment: invalid enum value 0x%x.",
                             funcName, attachment);
            return false;
        }
    }

    if (attachment == LOCAL_GL_DEPTH_ATTACHMENT ||
        attachment == LOCAL_GL_STENCIL_ATTACHMENT ||
        attachment == LOCAL_GL_DEPTH_STENCIL_ATTACHMENT)
    {
        return true;
    }

    if (attachment >= LOCAL_GL_COLOR_ATTACHMENT0 &&
        attachment <= LastColorAttachmentEnum())
    {
        return true;
    }

    if (badColorAttachmentIsInvalidOp &&
        attachment >= LOCAL_GL_COLOR_ATTACHMENT0)
    {
        const uint32_t offset = attachment - LOCAL_GL_COLOR_ATTACHMENT0;
        ErrorInvalidOperation("%s: Bad color attachment: COLOR_ATTACHMENT%u. (0x%04x)",
                              funcName, offset, attachment);
    } else {
        ErrorInvalidEnum("%s: attachment: Bad attachment 0x%x.", funcName, attachment);
    }
    return false;
}

/**
 * Return true if pname is valid for GetSamplerParameter calls.
 */
bool
WebGLContext::ValidateSamplerParameterName(GLenum pname, const char* info)
{
    switch (pname) {
    case LOCAL_GL_TEXTURE_MIN_FILTER:
    case LOCAL_GL_TEXTURE_MAG_FILTER:
    case LOCAL_GL_TEXTURE_WRAP_S:
    case LOCAL_GL_TEXTURE_WRAP_T:
    case LOCAL_GL_TEXTURE_WRAP_R:
    case LOCAL_GL_TEXTURE_MIN_LOD:
    case LOCAL_GL_TEXTURE_MAX_LOD:
    case LOCAL_GL_TEXTURE_COMPARE_MODE:
    case LOCAL_GL_TEXTURE_COMPARE_FUNC:
        return true;

    default:
        ErrorInvalidEnum("%s: invalid pname: %s", info, EnumName(pname));
        return false;
    }
}

/**
 * Return true if pname and param are valid combination for SamplerParameter calls.
 */
bool
WebGLContext::ValidateSamplerParameterParams(GLenum pname, const WebGLIntOrFloat& param, const char* info)
{
    const GLenum p = param.AsInt();

    switch (pname) {
    case LOCAL_GL_TEXTURE_MIN_FILTER:
        switch (p) {
        case LOCAL_GL_NEAREST:
        case LOCAL_GL_LINEAR:
        case LOCAL_GL_NEAREST_MIPMAP_NEAREST:
        case LOCAL_GL_NEAREST_MIPMAP_LINEAR:
        case LOCAL_GL_LINEAR_MIPMAP_NEAREST:
        case LOCAL_GL_LINEAR_MIPMAP_LINEAR:
            return true;

        default:
            ErrorInvalidEnum("%s: invalid param: %s", info, EnumName(p));
            return false;
        }

    case LOCAL_GL_TEXTURE_MAG_FILTER:
        switch (p) {
        case LOCAL_GL_NEAREST:
        case LOCAL_GL_LINEAR:
            return true;

        default:
            ErrorInvalidEnum("%s: invalid param: %s", info, EnumName(p));
            return false;
        }

    case LOCAL_GL_TEXTURE_WRAP_S:
    case LOCAL_GL_TEXTURE_WRAP_T:
    case LOCAL_GL_TEXTURE_WRAP_R:
        switch (p) {
        case LOCAL_GL_CLAMP_TO_EDGE:
        case LOCAL_GL_REPEAT:
        case LOCAL_GL_MIRRORED_REPEAT:
            return true;

        default:
            ErrorInvalidEnum("%s: invalid param: %s", info, EnumName(p));
            return false;
        }

    case LOCAL_GL_TEXTURE_MIN_LOD:
    case LOCAL_GL_TEXTURE_MAX_LOD:
        return true;

    case LOCAL_GL_TEXTURE_COMPARE_MODE:
        switch (param.AsInt()) {
        case LOCAL_GL_NONE:
        case LOCAL_GL_COMPARE_REF_TO_TEXTURE:
            return true;

        default:
            ErrorInvalidEnum("%s: invalid param: %s", info, EnumName(p));
            return false;
        }

    case LOCAL_GL_TEXTURE_COMPARE_FUNC:
        switch (p) {
        case LOCAL_GL_LEQUAL:
        case LOCAL_GL_GEQUAL:
        case LOCAL_GL_LESS:
        case LOCAL_GL_GREATER:
        case LOCAL_GL_EQUAL:
        case LOCAL_GL_NOTEQUAL:
        case LOCAL_GL_ALWAYS:
        case LOCAL_GL_NEVER:
            return true;

        default:
            ErrorInvalidEnum("%s: invalid param: %s", info, EnumName(p));
            return false;
        }

    default:
        ErrorInvalidEnum("%s: invalid pname: %s", info, EnumName(pname));
        return false;
    }
}

bool
WebGLContext::ValidateUniformLocation(WebGLUniformLocation* loc, const char* funcName)
{
    /* GLES 2.0.25, p38:
     *   If the value of location is -1, the Uniform* commands will silently
     *   ignore the data passed in, and the current uniform values will not be
     *   changed.
     */
    if (!loc)
        return false;

    if (!ValidateObject(funcName, loc))
        return false;

    if (!mCurrentProgram) {
        ErrorInvalidOperation("%s: No program is currently bound.", funcName);
        return false;
    }

    return loc->ValidateForProgram(mCurrentProgram, funcName);
}

bool
WebGLContext::ValidateAttribArraySetter(const char* name, uint32_t setterElemSize,
                                        uint32_t arrayLength)
{
    if (IsContextLost())
        return false;

    if (arrayLength < setterElemSize) {
        ErrorInvalidValue("%s: Array must have >= %d elements.", name,
                          setterElemSize);
        return false;
    }

    return true;
}

bool
WebGLContext::ValidateUniformSetter(WebGLUniformLocation* loc,
                                    uint8_t setterElemSize, GLenum setterType,
                                    const char* funcName)
{
    if (IsContextLost())
        return false;

    if (!ValidateUniformLocation(loc, funcName))
        return false;

    if (!loc->ValidateSizeAndType(setterElemSize, setterType, funcName))
        return false;

    return true;
}

bool
WebGLContext::ValidateUniformArraySetter(WebGLUniformLocation* loc,
                                         uint8_t setterElemSize,
                                         GLenum setterType,
                                         uint32_t setterArraySize,
                                         const char* funcName,
                                         uint32_t* const out_numElementsToUpload)
{
    if (IsContextLost())
        return false;

    if (!ValidateUniformLocation(loc, funcName))
        return false;

    if (!loc->ValidateSizeAndType(setterElemSize, setterType, funcName))
        return false;

    if (!loc->ValidateArrayLength(setterElemSize, setterArraySize, funcName))
        return false;

    const auto& elemCount = loc->mInfo->mActiveInfo->mElemCount;
    MOZ_ASSERT(elemCount > loc->mArrayIndex);
    const uint32_t uniformElemCount = elemCount - loc->mArrayIndex;

    *out_numElementsToUpload = std::min(uniformElemCount,
                                        setterArraySize / setterElemSize);
    return true;
}

bool
WebGLContext::ValidateUniformMatrixArraySetter(WebGLUniformLocation* loc,
                                               uint8_t setterCols,
                                               uint8_t setterRows,
                                               GLenum setterType,
                                               uint32_t setterArraySize,
                                               bool setterTranspose,
                                               const char* funcName,
                                               uint32_t* const out_numElementsToUpload)
{
    const uint8_t setterElemSize = setterCols * setterRows;

    if (IsContextLost())
        return false;

    if (!ValidateUniformLocation(loc, funcName))
        return false;

    if (!loc->ValidateSizeAndType(setterElemSize, setterType, funcName))
        return false;

    if (!loc->ValidateArrayLength(setterElemSize, setterArraySize, funcName))
        return false;

    if (!ValidateUniformMatrixTranspose(setterTranspose, funcName))
        return false;

    const auto& elemCount = loc->mInfo->mActiveInfo->mElemCount;
    MOZ_ASSERT(elemCount > loc->mArrayIndex);
    const uint32_t uniformElemCount = elemCount - loc->mArrayIndex;

    *out_numElementsToUpload = std::min(uniformElemCount,
                                        setterArraySize / setterElemSize);
    return true;
}

bool
WebGLContext::ValidateAttribIndex(GLuint index, const char* info)
{
    bool valid = (index < MaxVertexAttribs());

    if (!valid) {
        if (index == GLuint(-1)) {
            ErrorInvalidValue("%s: -1 is not a valid `index`. This value"
                              " probably comes from a getAttribLocation()"
                              " call, where this return value -1 means"
                              " that the passed name didn't correspond to"
                              " an active attribute in the specified"
                              " program.", info);
        } else {
            ErrorInvalidValue("%s: `index` must be less than"
                              " MAX_VERTEX_ATTRIBS.", info);
        }
    }

    return valid;
}

bool
WebGLContext::ValidateAttribPointer(bool integerMode, GLuint index, GLint size, GLenum type,
                                    WebGLboolean normalized, GLsizei stride,
                                    WebGLintptr byteOffset, const char* info)
{
    WebGLBuffer* buffer = mBoundArrayBuffer;
    if (!buffer) {
        ErrorInvalidOperation("%s: must have valid GL_ARRAY_BUFFER binding", info);
        return false;
    }

    uint32_t requiredAlignment = 0;
    if (!ValidateAttribPointerType(integerMode, type, &requiredAlignment, info))
        return false;

    // requiredAlignment should always be a power of two
    MOZ_ASSERT(IsPowerOfTwo(requiredAlignment));
    GLsizei requiredAlignmentMask = requiredAlignment - 1;

    if (size < 1 || size > 4) {
        ErrorInvalidValue("%s: invalid element size", info);
        return false;
    }

    // see WebGL spec section 6.6 "Vertex Attribute Data Stride"
    if (stride < 0 || stride > 255) {
        ErrorInvalidValue("%s: negative or too large stride", info);
        return false;
    }

    if (byteOffset < 0) {
        ErrorInvalidValue("%s: negative offset", info);
        return false;
    }

    if (stride & requiredAlignmentMask) {
        ErrorInvalidOperation("%s: stride doesn't satisfy the alignment "
                              "requirement of given type", info);
        return false;
    }

    if (byteOffset & requiredAlignmentMask) {
        ErrorInvalidOperation("%s: byteOffset doesn't satisfy the alignment "
                              "requirement of given type", info);
        return false;
    }

    return true;
}

bool
WebGLContext::ValidateStencilParamsForDrawCall()
{
    const char msg[] = "%s set different front and back stencil %s. Drawing in"
                       " this configuration is not allowed.";

    if (mStencilRefFront != mStencilRefBack) {
        ErrorInvalidOperation(msg, "stencilFuncSeparate", "reference values");
        return false;
    }

    if (mStencilValueMaskFront != mStencilValueMaskBack) {
        ErrorInvalidOperation(msg, "stencilFuncSeparate", "value masks");
        return false;
    }

    if (mStencilWriteMaskFront != mStencilWriteMaskBack) {
        ErrorInvalidOperation(msg, "stencilMaskSeparate", "write masks");
        return false;
    }

    return true;
}

static inline int32_t
FloorPOT(int32_t x)
{
    MOZ_ASSERT(x > 0);
    int32_t pot = 1;
    while (pot < 0x40000000) {
        if (x < pot*2)
            break;
        pot *= 2;
    }
    return pot;
}

bool
WebGLContext::InitAndValidateGL(FailureReason* const out_failReason)
{
    MOZ_RELEASE_ASSERT(gl, "GFX: GL not initialized");

    // Unconditionally create a new format usage authority. This is
    // important when restoring contexts and extensions need to add
    // formats back into the authority.
    mFormatUsage = CreateFormatUsage(gl);
    if (!mFormatUsage) {
        *out_failReason = { "FEATURE_FAILURE_WEBGL_FORMAT",
                            "Failed to create mFormatUsage." };
        return false;
    }

    GLenum error = gl->fGetError();
    if (error != LOCAL_GL_NO_ERROR) {
        const nsPrintfCString reason("GL error 0x%x occurred during OpenGL context"
                                     " initialization, before WebGL initialization!",
                                     error);
        *out_failReason = { "FEATURE_FAILURE_WEBGL_GLERR_1", reason };
        return false;
    }

    mMinCapability = gfxPrefs::WebGLMinCapabilityMode();
    mDisableExtensions = gfxPrefs::WebGLDisableExtensions();
    mLoseContextOnMemoryPressure = gfxPrefs::WebGLLoseContextOnMemoryPressure();
    mCanLoseContextInForeground = gfxPrefs::WebGLCanLoseContextInForeground();
    mRestoreWhenVisible = gfxPrefs::WebGLRestoreWhenVisible();

    if (MinCapabilityMode())
        mDisableFragHighP = true;

    // These are the default values, see 6.2 State tables in the
    // OpenGL ES 2.0.25 spec.
    mColorWriteMask[0] = 1;
    mColorWriteMask[1] = 1;
    mColorWriteMask[2] = 1;
    mColorWriteMask[3] = 1;
    mDepthWriteMask = 1;
    mColorClearValue[0] = 0.f;
    mColorClearValue[1] = 0.f;
    mColorClearValue[2] = 0.f;
    mColorClearValue[3] = 0.f;
    mDepthClearValue = 1.f;
    mStencilClearValue = 0;
    mStencilRefFront = 0;
    mStencilRefBack = 0;

    mLineWidth = 1.0;

    /*
    // Technically, we should be setting mStencil[...] values to
    // `allOnes`, but either ANGLE breaks or the SGX540s on Try break.
    GLuint stencilBits = 0;
    gl->GetUIntegerv(LOCAL_GL_STENCIL_BITS, &stencilBits);
    GLuint allOnes = ~(UINT32_MAX << stencilBits);
    mStencilValueMaskFront = allOnes;
    mStencilValueMaskBack  = allOnes;
    mStencilWriteMaskFront = allOnes;
    mStencilWriteMaskBack  = allOnes;
    */

    gl->GetUIntegerv(LOCAL_GL_STENCIL_VALUE_MASK,      &mStencilValueMaskFront);
    gl->GetUIntegerv(LOCAL_GL_STENCIL_BACK_VALUE_MASK, &mStencilValueMaskBack);
    gl->GetUIntegerv(LOCAL_GL_STENCIL_WRITEMASK,       &mStencilWriteMaskFront);
    gl->GetUIntegerv(LOCAL_GL_STENCIL_BACK_WRITEMASK,  &mStencilWriteMaskBack);

    AssertUintParamCorrect(gl, LOCAL_GL_STENCIL_VALUE_MASK,      mStencilValueMaskFront);
    AssertUintParamCorrect(gl, LOCAL_GL_STENCIL_BACK_VALUE_MASK, mStencilValueMaskBack);
    AssertUintParamCorrect(gl, LOCAL_GL_STENCIL_WRITEMASK,       mStencilWriteMaskFront);
    AssertUintParamCorrect(gl, LOCAL_GL_STENCIL_BACK_WRITEMASK,  mStencilWriteMaskBack);

    mDitherEnabled = true;
    mRasterizerDiscardEnabled = false;
    mScissorTestEnabled = false;

    // Bindings, etc.
    mActiveTexture = 0;
    mDefaultFB_DrawBuffer0 = LOCAL_GL_BACK;

    mEmitContextLostErrorOnce = true;
    mWebGLError = LOCAL_GL_NO_ERROR;
    mUnderlyingGLError = LOCAL_GL_NO_ERROR;

    mBound2DTextures.Clear();
    mBoundCubeMapTextures.Clear();
    mBound3DTextures.Clear();
    mBound2DArrayTextures.Clear();
    mBoundSamplers.Clear();

    mBoundArrayBuffer = nullptr;
    mCurrentProgram = nullptr;

    mBoundDrawFramebuffer = nullptr;
    mBoundReadFramebuffer = nullptr;
    mBoundRenderbuffer = nullptr;

    MakeContextCurrent();

    // For OpenGL compat. profiles, we always keep vertex attrib 0 array enabled.
    if (gl->IsCompatibilityProfile())
        gl->fEnableVertexAttribArray(0);

    if (MinCapabilityMode())
        mGLMaxVertexAttribs = MINVALUE_GL_MAX_VERTEX_ATTRIBS;
    else
        gl->fGetIntegerv(LOCAL_GL_MAX_VERTEX_ATTRIBS, &mGLMaxVertexAttribs);

    if (mGLMaxVertexAttribs < 8) {
        const nsPrintfCString reason("GL_MAX_VERTEX_ATTRIBS: %d is < 8!",
                                     mGLMaxVertexAttribs);
        *out_failReason = { "FEATURE_FAILURE_WEBGL_V_ATRB", reason };
        return false;
    }

    // Note: GL_MAX_TEXTURE_UNITS is fixed at 4 for most desktop hardware,
    // even though the hardware supports much more.  The
    // GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS value is the accurate value.
    if (MinCapabilityMode())
        mGLMaxTextureUnits = MINVALUE_GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS;
    else
        gl->fGetIntegerv(LOCAL_GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &mGLMaxTextureUnits);

    if (mGLMaxTextureUnits < 8) {
        const nsPrintfCString reason("GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS: %d is < 8!",
                                     mGLMaxTextureUnits);
        *out_failReason = { "FEATURE_FAILURE_WEBGL_T_UNIT", reason };
        return false;
    }

    mBound2DTextures.SetLength(mGLMaxTextureUnits);
    mBoundCubeMapTextures.SetLength(mGLMaxTextureUnits);
    mBound3DTextures.SetLength(mGLMaxTextureUnits);
    mBound2DArrayTextures.SetLength(mGLMaxTextureUnits);
    mBoundSamplers.SetLength(mGLMaxTextureUnits);

    ////////////////

    if (MinCapabilityMode()) {
        mImplMaxTextureSize = MINVALUE_GL_MAX_TEXTURE_SIZE;
        mImplMaxCubeMapTextureSize = MINVALUE_GL_MAX_CUBE_MAP_TEXTURE_SIZE;
        mImplMaxRenderbufferSize = MINVALUE_GL_MAX_RENDERBUFFER_SIZE;

        mImplMax3DTextureSize = MINVALUE_GL_MAX_3D_TEXTURE_SIZE;
        mImplMaxArrayTextureLayers = MINVALUE_GL_MAX_ARRAY_TEXTURE_LAYERS;

        mGLMaxTextureImageUnits = MINVALUE_GL_MAX_TEXTURE_IMAGE_UNITS;
        mGLMaxVertexTextureImageUnits = MINVALUE_GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS;
    } else {
        gl->fGetIntegerv(LOCAL_GL_MAX_TEXTURE_SIZE, (GLint*)&mImplMaxTextureSize);
        gl->fGetIntegerv(LOCAL_GL_MAX_CUBE_MAP_TEXTURE_SIZE, (GLint*)&mImplMaxCubeMapTextureSize);
        gl->fGetIntegerv(LOCAL_GL_MAX_RENDERBUFFER_SIZE, (GLint*)&mImplMaxRenderbufferSize);

        if (!gl->GetPotentialInteger(LOCAL_GL_MAX_3D_TEXTURE_SIZE, (GLint*)&mImplMax3DTextureSize))
            mImplMax3DTextureSize = 0;
        if (!gl->GetPotentialInteger(LOCAL_GL_MAX_ARRAY_TEXTURE_LAYERS, (GLint*)&mImplMaxArrayTextureLayers))
            mImplMaxArrayTextureLayers = 0;

        gl->fGetIntegerv(LOCAL_GL_MAX_TEXTURE_IMAGE_UNITS, &mGLMaxTextureImageUnits);
        gl->fGetIntegerv(LOCAL_GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS, &mGLMaxVertexTextureImageUnits);
    }

    // If we don't support a target, its max size is 0. We should only floor-to-POT if the
    // value if it's non-zero. (NB log2(0) is -Inf, so zero isn't an integer power-of-two)
    const auto fnFloorPOTIfSupported = [](uint32_t& val) {
        if (val) {
            val = FloorPOT(val);
        }
    };

    fnFloorPOTIfSupported(mImplMaxTextureSize);
    fnFloorPOTIfSupported(mImplMaxCubeMapTextureSize);
    fnFloorPOTIfSupported(mImplMaxRenderbufferSize);

    fnFloorPOTIfSupported(mImplMax3DTextureSize);
    fnFloorPOTIfSupported(mImplMaxArrayTextureLayers);

    ////////////////

    mGLMaxColorAttachments = 1;
    mGLMaxDrawBuffers = 1;
    gl->GetPotentialInteger(LOCAL_GL_MAX_COLOR_ATTACHMENTS,
                            (GLint*)&mGLMaxColorAttachments);
    gl->GetPotentialInteger(LOCAL_GL_MAX_DRAW_BUFFERS, (GLint*)&mGLMaxDrawBuffers);

    if (MinCapabilityMode()) {
        mGLMaxColorAttachments = std::min(mGLMaxColorAttachments,
                                          kMinMaxColorAttachments);
        mGLMaxDrawBuffers = std::min(mGLMaxDrawBuffers, kMinMaxDrawBuffers);
    }

    if (IsWebGL2()) {
        mImplMaxColorAttachments = mGLMaxColorAttachments;
        mImplMaxDrawBuffers = std::min(mGLMaxDrawBuffers, mImplMaxColorAttachments);
    } else {
        mImplMaxColorAttachments = 1;
        mImplMaxDrawBuffers = 1;
    }

    ////////////////

    if (MinCapabilityMode()) {
        mGLMaxFragmentUniformVectors = MINVALUE_GL_MAX_FRAGMENT_UNIFORM_VECTORS;
        mGLMaxVertexUniformVectors = MINVALUE_GL_MAX_VERTEX_UNIFORM_VECTORS;
        mGLMaxVaryingVectors = MINVALUE_GL_MAX_VARYING_VECTORS;
    } else {
        if (gl->IsSupported(gl::GLFeature::ES2_compatibility)) {
            gl->fGetIntegerv(LOCAL_GL_MAX_FRAGMENT_UNIFORM_VECTORS, &mGLMaxFragmentUniformVectors);
            gl->fGetIntegerv(LOCAL_GL_MAX_VERTEX_UNIFORM_VECTORS, &mGLMaxVertexUniformVectors);
            gl->fGetIntegerv(LOCAL_GL_MAX_VARYING_VECTORS, &mGLMaxVaryingVectors);
        } else {
            gl->fGetIntegerv(LOCAL_GL_MAX_FRAGMENT_UNIFORM_COMPONENTS, &mGLMaxFragmentUniformVectors);
            mGLMaxFragmentUniformVectors /= 4;
            gl->fGetIntegerv(LOCAL_GL_MAX_VERTEX_UNIFORM_COMPONENTS, &mGLMaxVertexUniformVectors);
            mGLMaxVertexUniformVectors /= 4;

            /* We are now going to try to read GL_MAX_VERTEX_OUTPUT_COMPONENTS
             * and GL_MAX_FRAGMENT_INPUT_COMPONENTS, however these constants
             * only entered the OpenGL standard at OpenGL 3.2. So we will try
             * reading, and check OpenGL error for INVALID_ENUM.
             *
             * On the public_webgl list, "problematic GetParameter pnames"
             * thread, the following formula was given:
             *   maxVaryingVectors = min(GL_MAX_VERTEX_OUTPUT_COMPONENTS,
             *                           GL_MAX_FRAGMENT_INPUT_COMPONENTS) / 4
             */
            GLint maxVertexOutputComponents = 0;
            GLint maxFragmentInputComponents = 0;

            const bool ok = (gl->GetPotentialInteger(LOCAL_GL_MAX_VERTEX_OUTPUT_COMPONENTS,
                                                     &maxVertexOutputComponents) &&
                             gl->GetPotentialInteger(LOCAL_GL_MAX_FRAGMENT_INPUT_COMPONENTS,
                                                     &maxFragmentInputComponents));

            if (ok) {
                mGLMaxVaryingVectors = std::min(maxVertexOutputComponents,
                                                maxFragmentInputComponents) / 4;
            } else {
                mGLMaxVaryingVectors = 16;
                // 16 = 64/4, and 64 is the min value for
                // maxVertexOutputComponents in the OpenGL 3.2 spec.
            }
        }
    }

    if (gl->IsCompatibilityProfile()) {
        // gl_PointSize is always available in ES2 GLSL, but has to be
        // specifically enabled on desktop GLSL.
        gl->fEnable(LOCAL_GL_VERTEX_PROGRAM_POINT_SIZE);

        /* gl_PointCoord is always available in ES2 GLSL and in newer desktop
         * GLSL versions, but apparently not in OpenGL 2 and apparently not (due
         * to a driver bug) on certain NVIDIA setups. See:
         *   http://www.opengl.org/discussion_boards/ubbthreads.php?ubb=showflat&Number=261472
         *
         * Note that this used to cause crashes on old ATI drivers... Hopefully
         * not a significant anymore. See bug 602183.
         */
        gl->fEnable(LOCAL_GL_POINT_SPRITE);
    }

#ifdef XP_MACOSX
    if (gl->WorkAroundDriverBugs() &&
        gl->Vendor() == gl::GLVendor::ATI &&
        !nsCocoaFeatures::IsAtLeastVersion(10,9))
    {
        // The Mac ATI driver, in all known OSX version up to and including
        // 10.8, renders points sprites upside-down. (Apple bug 11778921)
        gl->fPointParameterf(LOCAL_GL_POINT_SPRITE_COORD_ORIGIN,
                             LOCAL_GL_LOWER_LEFT);
    }
#endif

    if (gl->IsSupported(gl::GLFeature::seamless_cube_map_opt_in)) {
        gl->fEnable(LOCAL_GL_TEXTURE_CUBE_MAP_SEAMLESS);
    }

    // Check the shader validator pref
    mBypassShaderValidation = gfxPrefs::WebGLBypassShaderValidator();

    // initialize shader translator
    if (!ShInitialize()) {
        *out_failReason = { "FEATURE_FAILURE_WEBGL_GLSL",
                            "GLSL translator initialization failed!" };
        return false;
    }

    // Mesa can only be detected with the GL_VERSION string, of the form
    // "2.1 Mesa 7.11.0"
    const char* versionStr = (const char*)(gl->fGetString(LOCAL_GL_VERSION));
    mIsMesa = strstr(versionStr, "Mesa");

    // Notice that the point of calling fGetError here is not only to check for
    // errors, but also to reset the error flags so that a subsequent WebGL
    // getError call will give the correct result.
    error = gl->fGetError();
    if (error != LOCAL_GL_NO_ERROR) {
        const nsPrintfCString reason("GL error 0x%x occurred during WebGL context"
                                     " initialization!",
                                     error);
        *out_failReason = { "FEATURE_FAILURE_WEBGL_GLERR_2", reason };
        return false;
    }

    if (IsWebGL2() &&
        !InitWebGL2(out_failReason))
    {
        // Todo: Bug 898404: Only allow WebGL2 on GL>=3.0 on desktop GL.
        return false;
    }

    // Default value for all disabled vertex attributes is [0, 0, 0, 1]
    mVertexAttribType = MakeUnique<GLenum[]>(mGLMaxVertexAttribs);
    for (int32_t index = 0; index < mGLMaxVertexAttribs; ++index) {
        mVertexAttribType[index] = LOCAL_GL_FLOAT;
        VertexAttrib4f(index, 0, 0, 0, 1);
    }

    mDefaultVertexArray = WebGLVertexArray::Create(this);
    mDefaultVertexArray->mAttribs.SetLength(mGLMaxVertexAttribs);
    mBoundVertexArray = mDefaultVertexArray;

    // OpenGL core profiles remove the default VAO object from version
    // 4.0.0. We create a default VAO for all core profiles,
    // regardless of version.
    //
    // GL Spec 4.0.0:
    // (https://www.opengl.org/registry/doc/glspec40.core.20100311.pdf)
    // in Section E.2.2 "Removed Features", pg 397: "[...] The default
    // vertex array object (the name zero) is also deprecated. [...]"

    if (gl->IsCoreProfile()) {
        mDefaultVertexArray->GenVertexArray();
        mDefaultVertexArray->BindVertexArray();
    }

    mPixelStore_FlipY = false;
    mPixelStore_PremultiplyAlpha = false;
    mPixelStore_ColorspaceConversion = BROWSER_DEFAULT_WEBGL;

    // GLES 3.0.4, p259:
    mPixelStore_UnpackImageHeight = 0;
    mPixelStore_UnpackSkipImages = 0;
    mPixelStore_UnpackRowLength = 0;
    mPixelStore_UnpackSkipRows = 0;
    mPixelStore_UnpackSkipPixels = 0;
    mPixelStore_UnpackAlignment = 4;
    mPixelStore_PackRowLength = 0;
    mPixelStore_PackSkipRows = 0;
    mPixelStore_PackSkipPixels = 0;
    mPixelStore_PackAlignment = 4;

    mPrimRestartTypeBytes = 0;

    return true;
}

bool
WebGLContext::ValidateFramebufferTarget(GLenum target,
                                        const char* const info)
{
    bool isValid = true;
    switch (target) {
    case LOCAL_GL_FRAMEBUFFER:
        break;

    case LOCAL_GL_DRAW_FRAMEBUFFER:
    case LOCAL_GL_READ_FRAMEBUFFER:
        isValid = IsWebGL2();
        break;

    default:
        isValid = false;
        break;
    }

    if (MOZ_LIKELY(isValid)) {
        return true;
    }

    ErrorInvalidEnum("%s: Invalid target: %s (0x%04x).", info, EnumName(target),
                     target);
    return false;
}

} // namespace mozilla
