/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_HTMLModElement_h
#define mozilla_dom_HTMLModElement_h

#include "mozilla/Attributes.h"
#include "nsGenericHTMLElement.h"
#include "nsGkAtoms.h"

namespace mozilla {
namespace dom {

class HTMLModElement MOZ_FINAL : public nsGenericHTMLElement
{
public:
  HTMLModElement(already_AddRefed<nsINodeInfo>& aNodeInfo);
  virtual ~HTMLModElement();

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const MOZ_OVERRIDE;

  void GetCite(nsString& aCite)
  {
    GetHTMLURIAttr(nsGkAtoms::cite, aCite);
  }
  void SetCite(const nsAString& aCite, ErrorResult& aRv)
  {
    SetHTMLAttr(nsGkAtoms::cite, aCite, aRv);
  }
  void GetDateTime(nsAString& aDateTime)
  {
    GetHTMLAttr(nsGkAtoms::datetime, aDateTime);
  }
  void SetDateTime(const nsAString& aDateTime, ErrorResult& aRv)
  {
    SetHTMLAttr(nsGkAtoms::datetime, aDateTime, aRv);
  }

  virtual JSObject* WrapNode(JSContext* aCx,
                             JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_HTMLModElement_h
