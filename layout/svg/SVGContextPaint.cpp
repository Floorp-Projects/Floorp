/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SVGContextPaint.h"

#include "gfxContext.h"
#include "nsIDocument.h"

namespace mozilla {

void
SVGContextPaint::InitStrokeGeometry(gfxContext* aContext,
                                    float devUnitsPerSVGUnit)
{
  mStrokeWidth = aContext->CurrentLineWidth() / devUnitsPerSVGUnit;
  aContext->CurrentDash(mDashes, &mDashOffset);
  for (uint32_t i = 0; i < mDashes.Length(); i++) {
    mDashes[i] /= devUnitsPerSVGUnit;
  }
  mDashOffset /= devUnitsPerSVGUnit;
}

AutoSetRestoreSVGContextPaint::AutoSetRestoreSVGContextPaint(
                                 SVGContextPaint* aContextPaint,
                                 nsIDocument* aSVGDocument)
  : mSVGDocument(aSVGDocument)
  , mOuterContextPaint(aSVGDocument->GetProperty(nsGkAtoms::svgContextPaint))
{
  // The way that we supply context paint is to temporarily set the context
  // paint on the owner document of the SVG that we're painting while it's
  // being painted.

  MOZ_ASSERT(aContextPaint);
  MOZ_ASSERT(aSVGDocument->IsBeingUsedAsImage(),
             "nsSVGUtils::GetContextPaint assumes this");

  if (mOuterContextPaint) {
    mSVGDocument->UnsetProperty(nsGkAtoms::svgContextPaint);
  }

  DebugOnly<nsresult> res =
    mSVGDocument->SetProperty(nsGkAtoms::svgContextPaint, aContextPaint);

  NS_WARN_IF_FALSE(NS_SUCCEEDED(res), "Failed to set context paint");
}

AutoSetRestoreSVGContextPaint::~AutoSetRestoreSVGContextPaint()
{
  mSVGDocument->UnsetProperty(nsGkAtoms::svgContextPaint);
  if (mOuterContextPaint) {
    DebugOnly<nsresult> res =
      mSVGDocument->SetProperty(nsGkAtoms::svgContextPaint, mOuterContextPaint);

    NS_WARN_IF_FALSE(NS_SUCCEEDED(res), "Failed to restore context paint");
  }
}

} // namespace mozilla
