/*
 * Copyright 2011 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "SkBitmapSourceDeserializer.h"
#include "SkBlurImageFilter.h"
#include "SkDashPathEffect.h"
#include "SkGradientShader.h"
#include "SkImageSource.h"
#include "SkLayerRasterizer.h"

// Security note:
//
// As new subclasses are added here, they should be reviewed by chrome security before they
// support deserializing cross-process: chrome-security@google.com. SampleFilterFuzz.cpp should
// also be amended to exercise the new subclass.
//
// See SkReadBuffer::isCrossProcess() and SkPicture::PictureIOSecurityPrecautionsEnabled()
//

/*
 *  None of these are strictly "required" for Skia to operate.
 *
 *  These are the bulk of our "effects" -- subclasses of various effects on SkPaint.
 *
 *  Clients should feel free to dup this file and modify it as needed. This function "InitEffects"
 *  will automatically be called before any of skia's effects are asked to be deserialized.
 */
void SkFlattenable::PrivateInitializer::InitEffects() {
    SK_DEFINE_FLATTENABLE_REGISTRAR_ENTRY(SkBitmapSourceDeserializer)

    // Rasterizer
    SK_DEFINE_FLATTENABLE_REGISTRAR_ENTRY(SkLayerRasterizer)

    // Shader
    SkGradientShader::InitializeFlattenables();

    // PathEffect
    SK_DEFINE_FLATTENABLE_REGISTRAR_ENTRY(SkDashPathEffect)

    // ImageFilter
    SK_DEFINE_FLATTENABLE_REGISTRAR_ENTRY(SkBlurImageFilter)
    SK_DEFINE_FLATTENABLE_REGISTRAR_ENTRY(SkImageSource)
}
