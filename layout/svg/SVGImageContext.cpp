/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


// Main header first:
#include "SVGImageContext.h"

// Keep others in (case-insensitive) order:
#include "gfxUtils.h"
#include "mozilla/Preferences.h"
#include "nsContentUtils.h"
#include "nsIFrame.h"
#include "nsPresContext.h"
#include "nsStyleStruct.h"

namespace mozilla {

/* static */ void
SVGImageContext::MaybeStoreContextPaint(Maybe<SVGImageContext>& aContext,
                                        nsIFrame* aFromFrame,
                                        imgIContainer* aImgContainer)
{
  static bool sEnabledForContent = false;
  static bool sEnabledForContentCached = false;

  if (!sEnabledForContentCached) {
    Preferences::AddBoolVarCache(&sEnabledForContent,
                                 "svg.context-properties.content.enabled", false);
    sEnabledForContentCached = true;
  }

  if (!aFromFrame->StyleSVG()->ExposesContextProperties()) {
    // Content must have '-moz-context-properties' set to the names of the
    // properties it wants to expose to images it links to.
    return;
  }

  if (!sEnabledForContent &&
      !nsContentUtils::IsChromeDoc(aFromFrame->PresContext()->Document())) {
    // Context paint is pref'ed off for content and this is a content doc.
    return;
  }

  if (aImgContainer->GetType() != imgIContainer::TYPE_VECTOR) {
    // Avoid this overhead for raster images.
    return;
  }

  bool haveContextPaint = false;

  RefPtr<SVGEmbeddingContextPaint> contextPaint = new SVGEmbeddingContextPaint();

  const nsStyleSVG* style = aFromFrame->StyleSVG();

  // XXX don't set values for properties not listed in 'context-properties'.

  if (style->mFill.Type() == eStyleSVGPaintType_Color) {
    haveContextPaint = true;
    contextPaint->SetFill(style->mFill.GetColor());
  }
  if (style->mStroke.Type() == eStyleSVGPaintType_Color) {
    haveContextPaint = true;
    contextPaint->SetStroke(style->mStroke.GetColor());
  }

  if (haveContextPaint) {
    if (!aContext) {
      aContext.emplace();
    }
    aContext->mContextPaint = contextPaint.forget();
  }
}

} // namespace mozilla
