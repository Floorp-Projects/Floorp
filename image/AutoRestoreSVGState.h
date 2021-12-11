/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_image_AutoRestoreSVGState_h
#define mozilla_image_AutoRestoreSVGState_h

#include "mozilla/Attributes.h"
#include "mozilla/AutoRestore.h"
#include "mozilla/SVGContextPaint.h"
#include "mozilla/dom/SVGSVGElement.h"
#include "SVGDrawingParameters.h"
#include "SVGDocumentWrapper.h"

namespace mozilla {
namespace image {

class MOZ_STACK_CLASS AutoRestoreSVGState final {
 public:
  AutoRestoreSVGState(const SVGDrawingParameters& aParams,
                      SVGDocumentWrapper* aSVGDocumentWrapper,
                      bool aContextPaint)
      : AutoRestoreSVGState(aParams.svgContext, aParams.animationTime,
                            aSVGDocumentWrapper, aContextPaint) {}

  AutoRestoreSVGState(const Maybe<SVGImageContext>& aSVGContext,
                      float aAnimationTime,
                      SVGDocumentWrapper* aSVGDocumentWrapper,
                      bool aContextPaint)
      : mIsDrawing(aSVGDocumentWrapper->mIsDrawing)
        // Apply any 'preserveAspectRatio' override (if specified) to the root
        // element:
        ,
        mPAR(aSVGContext, aSVGDocumentWrapper->GetRootSVGElem())
        // Set the animation time:
        ,
        mTime(aSVGDocumentWrapper->GetRootSVGElem(), aAnimationTime) {
    MOZ_ASSERT(!mIsDrawing.SavedValue());
    MOZ_ASSERT(aSVGDocumentWrapper->GetDocument());

    aSVGDocumentWrapper->mIsDrawing = true;

    // Set context paint (if specified) on the document:
    if (aContextPaint) {
      MOZ_ASSERT(aSVGContext->GetContextPaint());
      mContextPaint.emplace(*aSVGContext->GetContextPaint(),
                            *aSVGDocumentWrapper->GetDocument());
    }
  }

 private:
  AutoRestore<bool> mIsDrawing;
  AutoPreserveAspectRatioOverride mPAR;
  AutoSVGTimeSetRestore mTime;
  Maybe<AutoSetRestoreSVGContextPaint> mContextPaint;
};

}  // namespace image
}  // namespace mozilla

#endif  // mozilla_image_AutoRestoreSVGState_h
