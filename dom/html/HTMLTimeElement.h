/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_HTMLTimeElement_h
#define mozilla_dom_HTMLTimeElement_h

#include "mozilla/Attributes.h"
#include "nsIDOMHTMLElement.h"
#include "nsGenericHTMLElement.h"
#include "nsGkAtoms.h"

namespace mozilla {
namespace dom {

class HTMLTimeElement MOZ_FINAL : public nsGenericHTMLElement
{
public:
  explicit HTMLTimeElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo);
  virtual ~HTMLTimeElement();

  // HTMLTimeElement WebIDL
  void GetDateTime(DOMString& aDateTime)
  {
    GetHTMLAttr(nsGkAtoms::datetime, aDateTime);
  }

  void SetDateTime(const nsAString& aDateTime, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::datetime, aDateTime, aError);
  }

  virtual void GetItemValueText(DOMString& text) MOZ_OVERRIDE;
  virtual void SetItemValueText(const nsAString& text) MOZ_OVERRIDE;
  virtual nsresult Clone(mozilla::dom::NodeInfo* aNodeInfo, nsINode** aResult) const MOZ_OVERRIDE;

protected:
  virtual JSObject* WrapNode(JSContext* aCx) MOZ_OVERRIDE;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_HTMLTimeElement_h
