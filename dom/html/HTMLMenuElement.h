/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_HTMLMenuElement_h
#define mozilla_dom_HTMLMenuElement_h

#include "mozilla/Attributes.h"
#include "nsGenericHTMLElement.h"

namespace mozilla::dom {

class HTMLMenuElement final : public nsGenericHTMLElement {
 public:
  explicit HTMLMenuElement(
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

  NS_IMPL_FROMNODE_HTML_WITH_TAG(HTMLMenuElement, menu)

  // nsISupports
  NS_INLINE_DECL_REFCOUNTING_INHERITED(HTMLMenuElement, nsGenericHTMLElement)

  nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override;

  // WebIDL
  bool Compact() const { return GetBoolAttr(nsGkAtoms::compact); }
  void SetCompact(bool aCompact, ErrorResult& aError) {
    SetHTMLBoolAttr(nsGkAtoms::compact, aCompact, aError);
  }

 protected:
  virtual ~HTMLMenuElement();

  JSObject* WrapNode(JSContext* aCx,
                     JS::Handle<JSObject*> aGivenProto) override;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_HTMLMenuElement_h
