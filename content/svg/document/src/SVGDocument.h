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

class SVGDocument MOZ_FINAL : public XMLDocument
{
public:
  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const MOZ_OVERRIDE;

  // WebIDL API
  void GetDomain(nsAString& aDomain, ErrorResult& aRv);
  nsSVGElement* GetRootElement(ErrorResult& aRv);

protected:
  virtual JSObject* WrapNode(JSContext *aCx,
                             JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_SVGDocument_h
