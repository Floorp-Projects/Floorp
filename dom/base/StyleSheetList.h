/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_StyleSheetList_h
#define mozilla_dom_StyleSheetList_h

#include "nsIDOMStyleSheetList.h"
#include "nsWrapperCache.h"

class nsINode;

namespace mozilla {
class CSSStyleSheet;

namespace dom {

class StyleSheetList : public nsIDOMStyleSheetList
                     , public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(StyleSheetList)
  NS_DECL_NSIDOMSTYLESHEETLIST

  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override final;

  virtual nsINode* GetParentObject() const = 0;

  virtual uint32_t Length() = 0;
  virtual CSSStyleSheet* IndexedGetter(uint32_t aIndex, bool& aFound) = 0;
  CSSStyleSheet* Item(uint32_t aIndex)
  {
    bool dummy = false;
    return IndexedGetter(aIndex, dummy);
  }

protected:
  virtual ~StyleSheetList() {}
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_StyleSheetList_h
