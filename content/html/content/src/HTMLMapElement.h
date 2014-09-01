/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_HTMLMapElement_h
#define mozilla_dom_HTMLMapElement_h

#include "mozilla/Attributes.h"
#include "nsGenericHTMLElement.h"
#include "nsIDOMHTMLMapElement.h"
#include "nsAutoPtr.h"
#include "nsGkAtoms.h"

class nsContentList;

namespace mozilla {
namespace dom {

class HTMLMapElement MOZ_FINAL : public nsGenericHTMLElement,
                                 public nsIDOMHTMLMapElement
{
public:
  HTMLMapElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo);

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMHTMLMapElement
  NS_DECL_NSIDOMHTMLMAPELEMENT

  virtual nsresult Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult) const MOZ_OVERRIDE;

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED_NO_UNLINK(HTMLMapElement,
                                                     nsGenericHTMLElement)

  // XPCOM GetName is fine.
  void SetName(const nsAString& aName, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::name, aName, aError);
  }
  nsIHTMLCollection* Areas();

  virtual JSObject* WrapNode(JSContext* aCx) MOZ_OVERRIDE;

protected:
  ~HTMLMapElement() {}

  nsRefPtr<nsContentList> mAreas;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_HTMLMapElement_h
