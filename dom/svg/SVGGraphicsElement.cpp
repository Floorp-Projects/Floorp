/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGGraphicsElement.h"

#include "mozilla/dom/BindContext.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/SVGGraphicsElementBinding.h"
#include "mozilla/dom/SVGMatrix.h"
#include "mozilla/dom/SVGRect.h"
#include "mozilla/dom/SVGSVGElement.h"
#include "mozilla/ISVGDisplayableFrame.h"
#include "mozilla/SVGContentUtils.h"
#include "mozilla/SVGTextFrame.h"
#include "mozilla/SVGUtils.h"

#include "nsIContentInlines.h"
#include "nsLayoutUtils.h"

namespace mozilla::dom {

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(SVGGraphicsElement, SVGGraphicsElementBase)
NS_IMPL_RELEASE_INHERITED(SVGGraphicsElement, SVGGraphicsElementBase)

NS_INTERFACE_MAP_BEGIN(SVGGraphicsElement)
  NS_INTERFACE_MAP_ENTRY(mozilla::dom::SVGTests)
NS_INTERFACE_MAP_END_INHERITING(SVGGraphicsElementBase)

//----------------------------------------------------------------------
// Implementation

SVGGraphicsElement::SVGGraphicsElement(
    already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
    : SVGGraphicsElementBase(std::move(aNodeInfo)) {}

SVGElement* SVGGraphicsElement::GetNearestViewportElement() {
  return SVGContentUtils::GetNearestViewportElement(this);
}

SVGElement* SVGGraphicsElement::GetFarthestViewportElement() {
  return SVGContentUtils::GetOuterSVGElement(this);
}

static already_AddRefed<SVGRect> ZeroBBox(SVGGraphicsElement& aOwner) {
  return MakeAndAddRef<SVGRect>(&aOwner, gfx::Rect{0, 0, 0, 0});
}

already_AddRefed<SVGRect> SVGGraphicsElement::GetBBox(
    const SVGBoundingBoxOptions& aOptions) {
  nsIFrame* frame = GetPrimaryFrame(FlushType::Layout);

  if (!frame || frame->HasAnyStateBits(NS_FRAME_IS_NONDISPLAY)) {
    return ZeroBBox(*this);
  }
  ISVGDisplayableFrame* svgframe = do_QueryFrame(frame);

  if (!svgframe) {
    if (!frame->IsInSVGTextSubtree()) {
      return ZeroBBox(*this);
    }

    // For <tspan>, <textPath>, the frame is an nsInlineFrame or
    // nsBlockFrame, |svgframe| will be a nullptr.
    // We implement their getBBox directly here instead of in
    // SVGUtils::GetBBox, because SVGUtils::GetBBox is more
    // or less used for other purpose elsewhere. e.g. gradient
    // code assumes GetBBox of <tspan> returns the bbox of the
    // outer <text>.
    // TODO: cleanup this sort of usecase of SVGUtils::GetBBox,
    // then move this code SVGUtils::GetBBox.
    SVGTextFrame* text =
        static_cast<SVGTextFrame*>(nsLayoutUtils::GetClosestFrameOfType(
            frame->GetParent(), LayoutFrameType::SVGText));

    if (text->HasAnyStateBits(NS_FRAME_IS_NONDISPLAY)) {
      return ZeroBBox(*this);
    }

    gfxRect rec = text->TransformFrameRectFromTextChild(
        frame->GetRectRelativeToSelf(), frame);

    // Should also add the |x|, |y| of the SVGTextFrame itself, since
    // the result obtained by TransformFrameRectFromTextChild doesn't
    // include them.
    rec.x += float(text->GetPosition().x) / AppUnitsPerCSSPixel();
    rec.y += float(text->GetPosition().y) / AppUnitsPerCSSPixel();

    return do_AddRef(new SVGRect(this, ToRect(rec)));
  }

  if (!NS_SVGNewGetBBoxEnabled()) {
    return do_AddRef(new SVGRect(
        this, ToRect(SVGUtils::GetBBox(
                  frame, SVGUtils::eBBoxIncludeFillGeometry |
                             SVGUtils::eUseUserSpaceOfUseElement))));
  }
  uint32_t flags = 0;
  if (aOptions.mFill) {
    flags |= SVGUtils::eBBoxIncludeFill;
  }
  if (aOptions.mStroke) {
    flags |= SVGUtils::eBBoxIncludeStroke;
  }
  if (aOptions.mMarkers) {
    flags |= SVGUtils::eBBoxIncludeMarkers;
  }
  if (aOptions.mClipped) {
    flags |= SVGUtils::eBBoxIncludeClipped;
  }
  if (flags == 0) {
    return do_AddRef(new SVGRect(this, gfx::Rect()));
  }
  if (flags == SVGUtils::eBBoxIncludeMarkers ||
      flags == SVGUtils::eBBoxIncludeClipped) {
    flags |= SVGUtils::eBBoxIncludeFill;
  }
  flags |= SVGUtils::eUseUserSpaceOfUseElement;
  return do_AddRef(new SVGRect(this, ToRect(SVGUtils::GetBBox(frame, flags))));
}

already_AddRefed<SVGMatrix> SVGGraphicsElement::GetCTM() {
  Document* currentDoc = GetComposedDoc();
  if (currentDoc) {
    // Flush all pending notifications so that our frames are up to date
    currentDoc->FlushPendingNotifications(FlushType::Layout);
  }
  gfx::Matrix m = SVGContentUtils::GetCTM(this, false);
  RefPtr<SVGMatrix> mat =
      m.IsSingular() ? nullptr : new SVGMatrix(ThebesMatrix(m));
  return mat.forget();
}

already_AddRefed<SVGMatrix> SVGGraphicsElement::GetScreenCTM() {
  Document* currentDoc = GetComposedDoc();
  if (currentDoc) {
    // Flush all pending notifications so that our frames are up to date
    currentDoc->FlushPendingNotifications(FlushType::Layout);
  }
  gfx::Matrix m = SVGContentUtils::GetCTM(this, true);
  RefPtr<SVGMatrix> mat =
      m.IsSingular() ? nullptr : new SVGMatrix(ThebesMatrix(m));
  return mat.forget();
}

bool SVGGraphicsElement::IsSVGFocusable(bool* aIsFocusable,
                                        int32_t* aTabIndex) {
  // XXXedgar, maybe we could factor out the common code for SVG, HTML and
  // MathML elements, see bug 1586011.
  if (!IsInComposedDoc() || IsInDesignMode()) {
    // In designMode documents we only allow focusing the document.
    *aTabIndex = -1;
    *aIsFocusable = false;
    return true;
  }

  *aTabIndex = TabIndex();
  // If a tabindex is specified at all, or the default tabindex is 0, we're
  // focusable
  *aIsFocusable = *aTabIndex >= 0 || GetTabIndexAttrValue().isSome();
  return false;
}

Focusable SVGGraphicsElement::IsFocusableWithoutStyle(bool aWithMouse) {
  Focusable result;
  IsSVGFocusable(&result.mFocusable, &result.mTabIndex);
  return result;
}

}  // namespace mozilla::dom
