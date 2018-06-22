/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGDocument_h
#define mozilla_dom_SVGDocument_h

#include "mozilla/Attributes.h"
#include "mozilla/dom/XMLDocument.h"

class nsSVGElement;

namespace mozilla {

class SVGContextPaint;

namespace dom {

class SVGForeignObjectElement;

class SVGDocument final : public XMLDocument
{
public:
  SVGDocument()
    : XMLDocument("image/svg+xml")
  {
    mType = eSVG;
  }

  virtual nsresult Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult,
                         bool aPreallocateChildren) const override;

  void SetCurrentContextPaint(const SVGContextPaint* aContextPaint)
  {
    mCurrentContextPaint = aContextPaint;
  }

  const SVGContextPaint* GetCurrentContextPaint() const
  {
    return mCurrentContextPaint;
  }

private:

  // This is maintained by AutoSetRestoreSVGContextPaint.
  const SVGContextPaint* mCurrentContextPaint = nullptr;
};

} // namespace dom
} // namespace mozilla

inline mozilla::dom::SVGDocument*
nsIDocument::AsSVGDocument()
{
  MOZ_ASSERT(IsSVGDocument());
  return static_cast<mozilla::dom::SVGDocument*>(this);
}

#endif // mozilla_dom_SVGDocument_h
