/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_HTMLPictureElement_h
#define mozilla_dom_HTMLPictureElement_h

#include "mozilla/Attributes.h"
#include "nsGenericHTMLElement.h"

namespace mozilla {
class ErrorResult;
namespace dom {

class HTMLPictureElement final : public nsGenericHTMLElement {
 public:
  explicit HTMLPictureElement(
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

  // nsISupports
  NS_INLINE_DECL_REFCOUNTING_INHERITED(HTMLPictureElement, nsGenericHTMLElement)

  virtual nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override;
  virtual void RemoveChildNode(nsIContent* aKid, bool aNotify) override;
  virtual void InsertChildBefore(nsIContent* aKid, nsIContent* aBeforeThis,
                                 bool aNotify, ErrorResult& aRv) override;

 protected:
  virtual ~HTMLPictureElement();

  virtual JSObject* WrapNode(JSContext* aCx,
                             JS::Handle<JSObject*> aGivenProto) override;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_HTMLPictureElement_h
