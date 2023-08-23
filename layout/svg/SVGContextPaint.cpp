/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SVGContextPaint.h"

#include "gfxContext.h"
#include "gfxUtils.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/dom/Document.h"
#include "mozilla/extensions/WebExtensionPolicy.h"
#include "mozilla/StaticPrefs_svg.h"
#include "mozilla/SVGObserverUtils.h"
#include "mozilla/SVGUtils.h"
#include "SVGPaintServerFrame.h"

using namespace mozilla::gfx;
using namespace mozilla::image;

namespace mozilla {

using image::imgDrawingParams;

/* static */
bool SVGContextPaint::IsAllowedForImageFromURI(nsIURI* aURI) {
  if (StaticPrefs::svg_context_properties_content_enabled()) {
    return true;
  }

  // Context paint is pref'ed off for Web content.  Ideally we'd have some
  // easy means to determine whether the frame that has linked to the image
  // is a frame for a content node that originated from Web content.
  // Unfortunately different types of anonymous content, about: documents
  // such as about:reader, etc. that are "our" code that we ship are
  // sometimes hard to distinguish from real Web content.  As a result,
  // instead of trying to figure out what content is "ours" we instead let
  // any content provide image context paint, but only if the image is
  // chrome:// or resource:// do we return true.  This should be sufficient
  // to stop the image context paint feature being useful to (and therefore
  // used by and relied upon by) Web content.  (We don't want Web content to
  // use this feature because we're not sure that image context paint is a
  // good mechanism for wider use, or suitable for specification.)
  //
  // Because the default favicon used in the browser UI needs context paint, we
  // also allow it for:
  // - page-icon:<page-url> (used in history and bookmark items)
  // - moz-anno:favicon:<page-url> (used in the awesomebar)
  // This allowance does also inadvertently expose context-paint to 3rd-party
  // favicons, which is not great, but that hasn't caused trouble as far as we
  // know. Also: other places such as the tab bar don't use these protocols to
  // load favicons, so they help to ensure that 3rd-party favicons don't grow
  // to depend on this feature.
  //
  // One case that is not covered by chrome:// or resource:// are WebExtensions,
  // specifically ones that are "ours". WebExtensions are moz-extension://
  // regardless if the extension is in-tree or not. Since we don't want
  // extension developers coming to rely on image context paint either, we only
  // enable context-paint for extensions that are owned by Mozilla
  // (based on the extension permission "internal:svgContextPropertiesAllowed").
  //
  // We also allow this for browser UI icons that are served up from
  // Mozilla-controlled domains listed in the
  // svg.context-properties.content.allowed-domains pref.
  //
  nsAutoCString scheme;
  if (NS_SUCCEEDED(aURI->GetScheme(scheme)) &&
      (scheme.EqualsLiteral("chrome") || scheme.EqualsLiteral("resource") ||
       scheme.EqualsLiteral("page-icon") || scheme.EqualsLiteral("moz-anno"))) {
    return true;
  }
  RefPtr<BasePrincipal> principal =
      BasePrincipal::CreateContentPrincipal(aURI, OriginAttributes());

  RefPtr<extensions::WebExtensionPolicy> addonPolicy = principal->AddonPolicy();
  if (addonPolicy) {
    // Only allowed for extensions that have the
    // internal:svgContextPropertiesAllowed permission (added internally from
    // to Mozilla-owned extensions, see `isMozillaExtension` function
    // defined in Extension.jsm for the exact criteria).
    return addonPolicy->HasPermission(
        nsGkAtoms::svgContextPropertiesAllowedPermission);
  }

  bool isInAllowList = false;
  principal->IsURIInPrefList("svg.context-properties.content.allowed-domains",
                             &isInAllowList);
  return isInAllowList;
}

/**
 * Stores in |aTargetPaint| information on how to reconstruct the current
 * fill or stroke pattern. Will also set the paint opacity to transparent if
 * the paint is set to "none".
 * @param aOuterContextPaint pattern information from the outer text context
 * @param aTargetPaint where to store the current pattern information
 * @param aFillOrStroke member pointer to the paint we are setting up
 */
static void SetupInheritablePaint(const DrawTarget* aDrawTarget,
                                  const gfxMatrix& aContextMatrix,
                                  nsIFrame* aFrame, float& aOpacity,
                                  SVGContextPaint* aOuterContextPaint,
                                  SVGContextPaintImpl::Paint& aTargetPaint,
                                  StyleSVGPaint nsStyleSVG::*aFillOrStroke,
                                  nscolor aDefaultFallbackColor,
                                  imgDrawingParams& aImgParams) {
  const nsStyleSVG* style = aFrame->StyleSVG();
  SVGPaintServerFrame* ps =
      SVGObserverUtils::GetAndObservePaintServer(aFrame, aFillOrStroke);

  if (ps) {
    RefPtr<gfxPattern> pattern =
        ps->GetPaintServerPattern(aFrame, aDrawTarget, aContextMatrix,
                                  aFillOrStroke, aOpacity, aImgParams);

    if (pattern) {
      aTargetPaint.SetPaintServer(aFrame, aContextMatrix, ps);
      return;
    }
  }

  if (aOuterContextPaint) {
    RefPtr<gfxPattern> pattern;
    auto tag = SVGContextPaintImpl::Paint::Tag::None;
    switch ((style->*aFillOrStroke).kind.tag) {
      case StyleSVGPaintKind::Tag::ContextFill:
        tag = SVGContextPaintImpl::Paint::Tag::ContextFill;
        pattern = aOuterContextPaint->GetFillPattern(
            aDrawTarget, aOpacity, aContextMatrix, aImgParams);
        break;
      case StyleSVGPaintKind::Tag::ContextStroke:
        tag = SVGContextPaintImpl::Paint::Tag::ContextStroke;
        pattern = aOuterContextPaint->GetStrokePattern(
            aDrawTarget, aOpacity, aContextMatrix, aImgParams);
        break;
      default:;
    }
    if (pattern) {
      aTargetPaint.SetContextPaint(aOuterContextPaint, tag);
      return;
    }
  }

  nscolor color = SVGUtils::GetFallbackOrPaintColor(
      *aFrame->Style(), aFillOrStroke, aDefaultFallbackColor);
  aTargetPaint.SetColor(color);
}

DrawMode SVGContextPaintImpl::Init(const DrawTarget* aDrawTarget,
                                   const gfxMatrix& aContextMatrix,
                                   nsIFrame* aFrame,
                                   SVGContextPaint* aOuterContextPaint,
                                   imgDrawingParams& aImgParams) {
  DrawMode toDraw = DrawMode(0);

  const nsStyleSVG* style = aFrame->StyleSVG();

  // fill:
  if (style->mFill.kind.IsNone()) {
    SetFillOpacity(0.0f);
  } else {
    float opacity =
        SVGUtils::GetOpacity(style->mFillOpacity, aOuterContextPaint);

    SetupInheritablePaint(aDrawTarget, aContextMatrix, aFrame, opacity,
                          aOuterContextPaint, mFillPaint, &nsStyleSVG::mFill,
                          NS_RGB(0, 0, 0), aImgParams);

    SetFillOpacity(opacity);

    toDraw |= DrawMode::GLYPH_FILL;
  }

  // stroke:
  if (style->mStroke.kind.IsNone()) {
    SetStrokeOpacity(0.0f);
  } else {
    float opacity =
        SVGUtils::GetOpacity(style->mStrokeOpacity, aOuterContextPaint);

    SetupInheritablePaint(
        aDrawTarget, aContextMatrix, aFrame, opacity, aOuterContextPaint,
        mStrokePaint, &nsStyleSVG::mStroke, NS_RGBA(0, 0, 0, 0), aImgParams);

    SetStrokeOpacity(opacity);

    toDraw |= DrawMode::GLYPH_STROKE;
  }

  return toDraw;
}

void SVGContextPaint::InitStrokeGeometry(gfxContext* aContext,
                                         float devUnitsPerSVGUnit) {
  mStrokeWidth = aContext->CurrentLineWidth() / devUnitsPerSVGUnit;
  aContext->CurrentDash(mDashes, &mDashOffset);
  for (uint32_t i = 0; i < mDashes.Length(); i++) {
    mDashes[i] /= devUnitsPerSVGUnit;
  }
  mDashOffset /= devUnitsPerSVGUnit;
}

SVGContextPaint* SVGContextPaint::GetContextPaint(nsIContent* aContent) {
  dom::Document* ownerDoc = aContent->OwnerDoc();

  const auto* contextPaint = ownerDoc->GetCurrentContextPaint();

  // XXX The SVGContextPaint that Document keeps around is const. We could
  // and should keep that constness to the SVGContextPaint that we get here
  // (SVGImageContext is never changed after it is initialized).
  //
  // Unfortunately lazy initialization of SVGContextPaint (which is a member of
  // SVGImageContext, and also conceptually never changes after construction)
  // prevents some of SVGContextPaint's conceptually const methods from being
  // const.  Trying to fix SVGContextPaint (perhaps by using |mutable|) is a
  // bit of a headache so for now we punt on that, don't reapply the constness
  // to the SVGContextPaint here, and trust that no one will add code that
  // actually modifies the object.
  return const_cast<SVGContextPaint*>(contextPaint);
}

already_AddRefed<gfxPattern> SVGContextPaintImpl::GetFillPattern(
    const DrawTarget* aDrawTarget, float aOpacity, const gfxMatrix& aCTM,
    imgDrawingParams& aImgParams) {
  return mFillPaint.GetPattern(aDrawTarget, aOpacity, &nsStyleSVG::mFill, aCTM,
                               aImgParams);
}

already_AddRefed<gfxPattern> SVGContextPaintImpl::GetStrokePattern(
    const DrawTarget* aDrawTarget, float aOpacity, const gfxMatrix& aCTM,
    imgDrawingParams& aImgParams) {
  return mStrokePaint.GetPattern(aDrawTarget, aOpacity, &nsStyleSVG::mStroke,
                                 aCTM, aImgParams);
}

already_AddRefed<gfxPattern> SVGContextPaintImpl::Paint::GetPattern(
    const DrawTarget* aDrawTarget, float aOpacity,
    StyleSVGPaint nsStyleSVG::*aFillOrStroke, const gfxMatrix& aCTM,
    imgDrawingParams& aImgParams) {
  RefPtr<gfxPattern> pattern;
  if (mPatternCache.Get(aOpacity, getter_AddRefs(pattern))) {
    // Set the pattern matrix just in case it was messed with by a previous
    // caller. We should get the same matrix each time a pattern is constructed
    // so this should be fine.
    pattern->SetMatrix(aCTM * mPatternMatrix);
    return pattern.forget();
  }

  switch (mPaintType) {
    case Tag::None:
      pattern = new gfxPattern(DeviceColor());
      mPatternMatrix = gfxMatrix();
      break;
    case Tag::Color: {
      DeviceColor color = ToDeviceColor(mPaintDefinition.mColor);
      color.a *= aOpacity;
      pattern = new gfxPattern(color);
      mPatternMatrix = gfxMatrix();
      break;
    }
    case Tag::PaintServer:
      pattern = mPaintDefinition.mPaintServerFrame->GetPaintServerPattern(
          mFrame, aDrawTarget, mContextMatrix, aFillOrStroke, aOpacity,
          aImgParams);
      if (!pattern) {
        return nullptr;
      }
      {
        // m maps original-user-space to pattern space
        gfxMatrix m = pattern->GetMatrix();
        gfxMatrix deviceToOriginalUserSpace = mContextMatrix;
        if (!deviceToOriginalUserSpace.Invert()) {
          return nullptr;
        }
        // mPatternMatrix maps device space to pattern space via original user
        // space
        mPatternMatrix = deviceToOriginalUserSpace * m;
      }
      pattern->SetMatrix(aCTM * mPatternMatrix);
      break;
    case Tag::ContextFill:
      pattern = mPaintDefinition.mContextPaint->GetFillPattern(
          aDrawTarget, aOpacity, aCTM, aImgParams);
      // Don't cache this. mContextPaint will have cached it anyway. If we
      // cache it, we'll have to compute mPatternMatrix, which is annoying.
      return pattern.forget();
    case Tag::ContextStroke:
      pattern = mPaintDefinition.mContextPaint->GetStrokePattern(
          aDrawTarget, aOpacity, aCTM, aImgParams);
      // Don't cache this. mContextPaint will have cached it anyway. If we
      // cache it, we'll have to compute mPatternMatrix, which is annoying.
      return pattern.forget();
    default:
      MOZ_ASSERT(false, "invalid paint type");
      return nullptr;
  }

  mPatternCache.InsertOrUpdate(aOpacity, RefPtr{pattern});
  return pattern.forget();
}

AutoSetRestoreSVGContextPaint::AutoSetRestoreSVGContextPaint(
    const SVGContextPaint* aContextPaint, dom::Document* aDocument)
    : mDocument(aDocument),
      mOuterContextPaint(aDocument->GetCurrentContextPaint()) {
  mDocument->SetCurrentContextPaint(aContextPaint);
}

AutoSetRestoreSVGContextPaint::~AutoSetRestoreSVGContextPaint() {
  mDocument->SetCurrentContextPaint(mOuterContextPaint);
}

// SVGEmbeddingContextPaint

already_AddRefed<gfxPattern> SVGEmbeddingContextPaint::GetFillPattern(
    const DrawTarget* aDrawTarget, float aFillOpacity, const gfxMatrix& aCTM,
    imgDrawingParams& aImgParams) {
  if (!mFill) {
    return nullptr;
  }
  // The gfxPattern that we create below depends on aFillOpacity, and since
  // different elements in the SVG image may pass in different values for
  // fill opacities we don't try to cache the gfxPattern that we create.
  DeviceColor fill = *mFill;
  fill.a *= aFillOpacity;
  return do_AddRef(new gfxPattern(fill));
}

already_AddRefed<gfxPattern> SVGEmbeddingContextPaint::GetStrokePattern(
    const DrawTarget* aDrawTarget, float aStrokeOpacity, const gfxMatrix& aCTM,
    imgDrawingParams& aImgParams) {
  if (!mStroke) {
    return nullptr;
  }
  DeviceColor stroke = *mStroke;
  stroke.a *= aStrokeOpacity;
  return do_AddRef(new gfxPattern(stroke));
}

uint32_t SVGEmbeddingContextPaint::Hash() const {
  uint32_t hash = 0;

  if (mFill) {
    hash = HashGeneric(hash, mFill->ToABGR());
  } else {
    // Arbitrary number, just to avoid trivial hash collisions between pairs of
    // instances where one embedding context has fill set to the same value as
    // another context has stroke set to.
    hash = 1;
  }

  if (mStroke) {
    hash = HashGeneric(hash, mStroke->ToABGR());
  }

  if (mFillOpacity != 1.0f) {
    hash = HashGeneric(hash, mFillOpacity);
  }

  if (mStrokeOpacity != 1.0f) {
    hash = HashGeneric(hash, mStrokeOpacity);
  }

  return hash;
}

}  // namespace mozilla
