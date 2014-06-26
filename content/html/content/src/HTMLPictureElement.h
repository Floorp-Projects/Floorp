/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_HTMLPictureElement_h
#define mozilla_dom_HTMLPictureElement_h

#include "mozilla/Attributes.h"
#include "nsIDOMHTMLPictureElement.h"
#include "nsGenericHTMLElement.h"

#include "mozilla/dom/HTMLUnknownElement.h"

namespace mozilla {
namespace dom {

class HTMLPictureElement MOZ_FINAL : public nsGenericHTMLElement,
                                    public nsIDOMHTMLPictureElement
{
public:
  HTMLPictureElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo);
  virtual ~HTMLPictureElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMHTMLPictureElement
  NS_DECL_NSIDOMHTMLPICTUREELEMENT

  virtual nsresult Clone(mozilla::dom::NodeInfo* aNodeInfo, nsINode** aResult) const MOZ_OVERRIDE;

  static bool IsPictureEnabled();

protected:
  virtual JSObject* WrapNode(JSContext* aCx) MOZ_OVERRIDE;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_HTMLPictureElement_h
