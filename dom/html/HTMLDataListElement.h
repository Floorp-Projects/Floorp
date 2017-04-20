/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef HTMLDataListElement_h___
#define HTMLDataListElement_h___

#include "mozilla/Attributes.h"
#include "nsGenericHTMLElement.h"
#include "nsContentList.h"

namespace mozilla {
namespace dom {

class HTMLDataListElement final : public nsGenericHTMLElement
{
public:
  explicit HTMLDataListElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo)
    : nsGenericHTMLElement(aNodeInfo)
  {
  }

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  nsContentList* Options()
  {
    if (!mOptions) {
      mOptions = new nsContentList(this, MatchOptions, nullptr, nullptr, true);
    }

    return mOptions;
  }


  virtual nsresult Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult,
                         bool aPreallocateChildren) const override;

  // This function is used to generate the nsContentList (option elements).
  static bool MatchOptions(Element* aElement, int32_t aNamespaceID,
                           nsIAtom* aAtom, void* aData);

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(HTMLDataListElement,
                                           nsGenericHTMLElement)
protected:
  virtual ~HTMLDataListElement();

  virtual JSObject* WrapNode(JSContext *aCx, JS::Handle<JSObject*> aGivenProto) override;

  // <option>'s list inside the datalist element.
  RefPtr<nsContentList> mOptions;
};

} // namespace dom
} // namespace mozilla

#endif /* HTMLDataListElement_h___ */
