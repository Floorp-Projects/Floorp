/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_HTMLDataElement_h
#define mozilla_dom_HTMLDataElement_h

#include "mozilla/Attributes.h"
#include "nsIDOMHTMLElement.h"
#include "nsGenericHTMLElement.h"
#include "nsGkAtoms.h"

namespace mozilla {
namespace dom {

class HTMLDataElement MOZ_FINAL : public nsGenericHTMLElement
{
public:
  HTMLDataElement(already_AddRefed<nsINodeInfo> aNodeInfo);
  virtual ~HTMLDataElement();

  // HTMLDataElement WebIDL
  void GetValue(nsAString& aValue)
  {
    GetHTMLAttr(nsGkAtoms::value, aValue);
  }

  void SetValue(const nsAString& aValue, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::value, aValue, aError);
  }

  virtual void GetItemValueText(nsAString& text) MOZ_OVERRIDE;
  virtual void SetItemValueText(const nsAString& text) MOZ_OVERRIDE;
  virtual nsresult Clone(nsINodeInfo* aNodeInfo, nsINode** aResult) const MOZ_OVERRIDE;

protected:
  virtual JSObject* WrapNode(JSContext* aCx,
                             JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_HTMLDataElement_h
