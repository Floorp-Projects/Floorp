/*
 * Copyright 2019 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "include/core/SkScalar.h"
#include "include/private/GrGLTypesPriv.h"
#include "src/gpu/GrSwizzle.h"
#include "src/gpu/gl/GrGLDefines.h"

GrGLTextureParameters::SamplerOverriddenState::SamplerOverriddenState()
        // These are the OpenGL defaults.
        : fMinFilter(GR_GL_NEAREST_MIPMAP_LINEAR)
        , fMagFilter(GR_GL_LINEAR)
        , fWrapS(GR_GL_REPEAT)
        , fWrapT(GR_GL_REPEAT)
        , fMinLOD(-1000.f)
        , fMaxLOD(1000.f)
        , fBorderColorInvalid(false) {}

void GrGLTextureParameters::SamplerOverriddenState::invalidate() {
    fMinFilter = ~0U;
    fMagFilter = ~0U;
    fWrapS = ~0U;
    fWrapT = ~0U;
    fMinLOD = SK_ScalarNaN;
    fMaxLOD = SK_ScalarNaN;
    fBorderColorInvalid = true;
}

GrGLTextureParameters::NonsamplerState::NonsamplerState()
        // These are the OpenGL defaults.
        : fSwizzleKey(GrSwizzle::RGBA().asKey()), fBaseMipMapLevel(0), fMaxMipMapLevel(1000) {}

void GrGLTextureParameters::NonsamplerState::invalidate() {
    fSwizzleKey = ~0U;
    fBaseMipMapLevel = ~0;
    fMaxMipMapLevel = ~0;
}

void GrGLTextureParameters::invalidate() {
    fSamplerOverriddenState.invalidate();
    fNonsamplerState.invalidate();
}

void GrGLTextureParameters::set(const SamplerOverriddenState* samplerState,
                                const NonsamplerState& nonsamplerState,
                                ResetTimestamp currTimestamp) {
    if (samplerState) {
        fSamplerOverriddenState = *samplerState;
    }
    fNonsamplerState = nonsamplerState;
    fResetTimestamp = currTimestamp;
}

void GrGLBackendTextureInfo::assign(const GrGLBackendTextureInfo& that, bool thisIsValid) {
    fInfo = that.fInfo;
    SkSafeRef(that.fParams);
    if (thisIsValid) {
        SkSafeUnref(fParams);
    }
    fParams = that.fParams;
}

void GrGLBackendTextureInfo::cleanup() { SkSafeUnref(fParams); }
