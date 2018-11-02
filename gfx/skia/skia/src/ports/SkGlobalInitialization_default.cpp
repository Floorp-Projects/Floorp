/*
 * Copyright 2011 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "SkFlattenablePriv.h"
#include "../../src/effects/SkDashImpl.h"
#include "SkGradientShader.h"

/*
 *  None of these are strictly "required" for Skia to operate.
 *
 *  These are the bulk of our "effects" -- subclasses of various effects on SkPaint.
 *
 *  Clients should feel free to dup this file and modify it as needed. This function "InitEffects"
 *  will automatically be called before any of skia's effects are asked to be deserialized.
 */
void SkFlattenable::PrivateInitializer::InitEffects() {
    // Shader
    SkGradientShader::InitializeFlattenables();

    // PathEffect
    SK_DEFINE_FLATTENABLE_REGISTRAR_ENTRY(SkDashImpl)
}
