/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGDocument_h
#define mozilla_dom_SVGDocument_h

#include "mozilla/Attributes.h"
#include "mozilla/dom/XMLDocument.h"

class nsSVGElement;

namespace mozilla {
namespace dom {

class SVGForeignObjectElement;

class SVGDocument MOZ_FINAL : public XMLDocument
{
  friend class SVGForeignObjectElement; // To call EnsureNonSVGUserAgentStyleSheetsLoaded

public:
  SVGDocument()
    : XMLDocument("image/svg+xml")
    , mHasLoadedNonSVGUserAgentStyleSheets(false)
  {
    mType = eSVG;
  }

  virtual nsresult InsertChildAt(nsIContent* aKid, uint32_t aIndex,
                                 bool aNotify) MOZ_OVERRIDE;
  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const MOZ_OVERRIDE;

  // WebIDL API
  void GetDomain(nsAString& aDomain, ErrorResult& aRv);
  nsSVGElement* GetRootElement(ErrorResult& aRv);

  virtual SVGDocument* AsSVGDocument() MOZ_OVERRIDE {
    return this;
  }

private:
  void EnsureNonSVGUserAgentStyleSheetsLoaded();

  virtual JSObject* WrapNode(JSContext *aCx) MOZ_OVERRIDE;

  bool mHasLoadedNonSVGUserAgentStyleSheets;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_SVGDocument_h
