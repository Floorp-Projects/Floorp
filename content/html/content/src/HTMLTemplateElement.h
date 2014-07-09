/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_HTMLTemplateElement_h
#define mozilla_dom_HTMLTemplateElement_h

#include "mozilla/Attributes.h"
#include "nsIDOMHTMLElement.h"
#include "nsGenericHTMLElement.h"
#include "mozilla/dom/DocumentFragment.h"

namespace mozilla {
namespace dom {

class HTMLTemplateElement MOZ_FINAL : public nsGenericHTMLElement
{
public:
  HTMLTemplateElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo);

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(HTMLTemplateElement,
                                           nsGenericHTMLElement)

  virtual nsresult Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult) const MOZ_OVERRIDE;

  DocumentFragment* Content()
  {
    return mContent;
  }

protected:
  virtual ~HTMLTemplateElement();

  virtual JSObject* WrapNode(JSContext *aCx) MOZ_OVERRIDE;

  nsRefPtr<DocumentFragment> mContent;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_HTMLTemplateElement_h

