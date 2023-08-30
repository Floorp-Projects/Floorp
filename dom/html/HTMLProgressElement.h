/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_HTMLProgressElement_h
#define mozilla_dom_HTMLProgressElement_h

#include "mozilla/Attributes.h"
#include "nsGenericHTMLElement.h"
#include "nsAttrValue.h"
#include "nsAttrValueInlines.h"
#include <algorithm>

namespace mozilla::dom {

class HTMLProgressElement final : public nsGenericHTMLElement {
 public:
  explicit HTMLProgressElement(
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

  nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override;

  bool ParseAttribute(int32_t aNamespaceID, nsAtom* aAttribute,
                      const nsAString& aValue,
                      nsIPrincipal* aMaybeScriptedPrincipal,
                      nsAttrValue& aResult) override;
  void AfterSetAttr(int32_t aNameSpaceID, nsAtom* aName,
                    const nsAttrValue* aValue, const nsAttrValue* aOldValue,
                    nsIPrincipal* aSubjectPrincipal, bool aNotify) override;

  // WebIDL
  double Value() const;
  void SetValue(double aValue, ErrorResult& aRv) {
    SetDoubleAttr(nsGkAtoms::value, aValue, aRv);
  }
  double Max() const;
  void SetMax(double aValue, ErrorResult& aRv) {
    SetDoubleAttr(nsGkAtoms::max, aValue, aRv);
  }
  double Position() const;

  NS_IMPL_FROMNODE_HTML_WITH_TAG(HTMLProgressElement, progress);

 protected:
  virtual ~HTMLProgressElement();

  JSObject* WrapNode(JSContext*, JS::Handle<JSObject*> aGivenProto) override;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_HTMLProgressElement_h
