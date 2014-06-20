/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_StyleSheetList_h
#define mozilla_dom_StyleSheetList_h

#include "nsIDOMStyleSheetList.h"
#include "nsWrapperCache.h"

class nsCSSStyleSheet;
class nsINode;

namespace mozilla {
namespace dom {

class StyleSheetList : public nsIDOMStyleSheetList
                     , public nsWrapperCache
{
public:
  StyleSheetList()
  {
    SetIsDOMBinding();
  }
  virtual ~StyleSheetList() {}

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(StyleSheetList)
  NS_DECL_NSIDOMSTYLESHEETLIST

  virtual JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE MOZ_FINAL;

  virtual nsINode* GetParentObject() const = 0;

  virtual uint32_t Length() = 0;
  virtual nsCSSStyleSheet* IndexedGetter(uint32_t aIndex, bool& aFound) = 0;
  nsCSSStyleSheet* Item(uint32_t aIndex)
  {
    bool dummy = false;
    return IndexedGetter(aIndex, dummy);
  }
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_StyleSheetList_h
