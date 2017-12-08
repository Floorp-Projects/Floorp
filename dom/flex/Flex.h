/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_Flex_h
#define mozilla_dom_Flex_h

#include "mozilla/dom/Element.h"
#include "nsISupports.h"
#include "nsWrapperCache.h"

class nsFlexContainerFrame;

namespace mozilla {
namespace dom {

class FlexLine;

class Flex : public nsISupports
           , public nsWrapperCache
{
public:
  explicit Flex(Element* aParent, nsFlexContainerFrame* aFrame);

protected:
  virtual ~Flex() = default;

public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(Flex)

  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;
  Element* GetParentObject()
  {
    return mParent;
  }

  void GetLines(nsTArray<RefPtr<FlexLine>>& aResult);

protected:
  nsCOMPtr<Element> mParent;
  nsTArray<RefPtr<FlexLine>> mLines;
};

} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_Flex_h */
