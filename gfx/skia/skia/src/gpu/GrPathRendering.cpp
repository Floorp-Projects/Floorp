/*
 * Copyright 2014 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "include/core/SkMatrix.h"
#include "include/core/SkTypeface.h"
#include "src/core/SkDescriptor.h"
#include "src/core/SkGlyph.h"
#include "src/core/SkScalerContext.h"
#include "src/gpu/GrGpu.h"
#include "src/gpu/GrPathRendering.h"
#include "src/gpu/GrProgramInfo.h"
#include "src/gpu/GrRenderTarget.h"

const GrUserStencilSettings& GrPathRendering::GetStencilPassSettings(FillType fill) {
    switch (fill) {
        default:
            SK_ABORT("Unexpected path fill.");
        case GrPathRendering::kWinding_FillType: {
            constexpr static GrUserStencilSettings kWindingStencilPass(
                GrUserStencilSettings::StaticInit<
                    0xffff,
                    GrUserStencilTest::kAlwaysIfInClip,
                    0xffff,
                    GrUserStencilOp::kIncWrap,
                    GrUserStencilOp::kIncWrap,
                    0xffff>()
            );
            return kWindingStencilPass;
        }
        case GrPathRendering::kEvenOdd_FillType: {
            constexpr static GrUserStencilSettings kEvenOddStencilPass(
                GrUserStencilSettings::StaticInit<
                    0xffff,
                    GrUserStencilTest::kAlwaysIfInClip,
                    0xffff,
                    GrUserStencilOp::kInvert,
                    GrUserStencilOp::kInvert,
                    0xffff>()
            );
            return kEvenOddStencilPass;
        }
    }
}

void GrPathRendering::stencilPath(const StencilPathArgs& args, const GrPath* path) {
    fGpu->handleDirtyContext();
    this->onStencilPath(args, path);
}

void GrPathRendering::drawPath(GrRenderTarget* renderTarget,
                               const GrProgramInfo& programInfo,
                               // Cover pass settings in pipeline.
                               const GrStencilSettings& stencilPassSettings,
                               const GrPath* path) {
    fGpu->handleDirtyContext();
    if (auto barrierType = programInfo.pipeline().xferBarrierType(renderTarget->asTexture(),
                                                                  *fGpu->caps())) {
        fGpu->xferBarrier(renderTarget, barrierType);
    }
    this->onDrawPath(renderTarget, programInfo, stencilPassSettings, path);
}
