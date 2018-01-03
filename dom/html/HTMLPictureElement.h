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
namespace dom {

class HTMLPictureElement final : public nsGenericHTMLElement
{
public:
  explicit HTMLPictureElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo);

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  virtual nsresult Clone(mozilla::dom::NodeInfo* aNodeInfo, nsINode** aResult,
                         bool aPreallocateChildren) const override;
  virtual void RemoveChildAt_Deprecated(uint32_t aIndex, bool aNotify) override;
  virtual nsresult InsertChildAt(nsIContent* aKid, uint32_t aIndex, bool aNotify) override;

protected:
  virtual ~HTMLPictureElement();

  virtual JSObject* WrapNode(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_HTMLPictureElement_h
