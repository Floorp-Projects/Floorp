/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_StyleSheetList_h
#define mozilla_dom_StyleSheetList_h

#include "mozilla/dom/StyleScope.h"
#include "nsIDOMStyleSheetList.h"
#include "nsWrapperCache.h"
#include "nsStubDocumentObserver.h"

class nsINode;

namespace mozilla {
class StyleSheet;

namespace dom {

class StyleSheetList final : public nsIDOMStyleSheetList
                           , public nsWrapperCache
                           , public nsStubDocumentObserver
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_AMBIGUOUS(StyleSheetList, nsIDOMStyleSheetList)

  NS_DECL_NSIDOMSTYLESHEETLIST

  NS_DECL_NSIMUTATIONOBSERVER_NODEWILLBEDESTROYED

  explicit StyleSheetList(StyleScope& aScope);

  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override final;

  nsINode* GetParentObject() const
  {
    return mStyleScope ? &mStyleScope->AsNode() : nullptr;
  }

  uint32_t Length() const
  {
    return mStyleScope ? mStyleScope->SheetCount() : 0;
  }

  StyleSheet* IndexedGetter(uint32_t aIndex, bool& aFound) const
  {
    if (!mStyleScope) {
      aFound = false;
      return nullptr;
    }

    StyleSheet* sheet = mStyleScope->SheetAt(aIndex);
    aFound = !!sheet;
    return sheet;
  }

  StyleSheet* Item(uint32_t aIndex) const
  {
    bool dummy = false;
    return IndexedGetter(aIndex, dummy);
  }

protected:
  virtual ~StyleSheetList();

  StyleScope* mStyleScope; // Weak, cleared on "NodeWillBeDestroyed".
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_StyleSheetList_h
