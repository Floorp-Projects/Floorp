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
namespace dom {

class SVGForeignObjectElement;

class SVGDocument final : public XMLDocument
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
                                 bool aNotify) override;
  virtual nsresult Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult) const override;

  // WebIDL API
  void GetDomain(nsAString& aDomain, ErrorResult& aRv);
  nsSVGElement* GetRootElement(ErrorResult& aRv);

  virtual SVGDocument* AsSVGDocument() override {
    return this;
  }

private:
  void EnsureNonSVGUserAgentStyleSheetsLoaded();

  virtual JSObject* WrapNode(JSContext *aCx, JS::Handle<JSObject*> aGivenProto) override;

  bool mHasLoadedNonSVGUserAgentStyleSheets;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_SVGDocument_h
