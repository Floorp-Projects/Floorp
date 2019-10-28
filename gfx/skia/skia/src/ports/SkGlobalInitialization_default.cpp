/*
 * Copyright 2011 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "include/core/SkFlattenable.h"

#if defined(SK_DISABLE_EFFECT_DESERIALIZATION)

    void SkFlattenable::PrivateInitializer::InitEffects() {}
    void SkFlattenable::PrivateInitializer::InitImageFilters() {}

#else

    #include "include/core/SkColorFilter.h"
    #include "src/effects/SkDashImpl.h"
    #include "include/effects/SkGradientShader.h"
    #include "include/core/SkMaskFilter.h"

    #include "include/effects/SkBlurImageFilter.h"
    #include "include/effects/SkComposeImageFilter.h"

    /*
     *  Register most effects for deserialization.
     *
     *  None of these are strictly required for Skia to operate, so if you're
     *  not using deserialization yourself, you can define
     *  SK_DISABLE_EFFECT_SERIALIZATION, or modify/replace this file as needed.
     */
    void SkFlattenable::PrivateInitializer::InitEffects() {
        // Shaders.
        SkGradientShader::RegisterFlattenables();

        // Color filters.
        SkColorFilter::RegisterFlattenables();

        // Mask filters.
        SkMaskFilter::RegisterFlattenables();

        // Path effects.
        SK_REGISTER_FLATTENABLE(SkDashImpl);
    }

    /*
     *  Register SkImageFilters for deserialization.
     *
     *  None of these are strictly required for Skia to operate, so if you're
     *  not using deserialization yourself, you can define
     *  SK_DISABLE_EFFECT_SERIALIZATION, or modify/replace this file as needed.
     */
    void SkFlattenable::PrivateInitializer::InitImageFilters() {
        SkBlurImageFilter::RegisterFlattenables();
        SkComposeImageFilter::RegisterFlattenables();
    }

#endif
