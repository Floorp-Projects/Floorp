/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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

class HTMLModElement final : public nsGenericHTMLElement
{
public:
  explicit HTMLModElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo);

  virtual nsresult Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult) const override;

  void GetCite(nsString& aCite)
  {
    GetHTMLURIAttr(nsGkAtoms::cite, aCite);
  }
  void SetCite(const nsAString& aCite, ErrorResult& aRv)
  {
    SetHTMLAttr(nsGkAtoms::cite, aCite, aRv);
  }
  void GetDateTime(DOMString& aDateTime)
  {
    GetHTMLAttr(nsGkAtoms::datetime, aDateTime);
  }
  void SetDateTime(const nsAString& aDateTime, ErrorResult& aRv)
  {
    SetHTMLAttr(nsGkAtoms::datetime, aDateTime, aRv);
  }

protected:
  virtual ~HTMLModElement();

  virtual JSObject* WrapNode(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_HTMLModElement_h
