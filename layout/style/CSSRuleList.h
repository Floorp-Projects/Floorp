/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_CSSRuleList_h
#define mozilla_dom_CSSRuleList_h

#include "mozilla/StyleSheetInlines.h"
#include "mozilla/css/Rule.h"
#include "nsWrapperCache.h"

namespace mozilla {
namespace dom {

class CSSRuleList : public nsISupports
                  , public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(CSSRuleList)

  virtual StyleSheet* GetParentObject() = 0;
  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override final;

  // WebIDL API
  css::Rule* Item(uint32_t aIndex)
  {
    bool unused;
    return IndexedGetter(aIndex, unused);
  }

  virtual css::Rule* IndexedGetter(uint32_t aIndex, bool& aFound) = 0;
  virtual uint32_t Length() = 0;

protected:
  virtual ~CSSRuleList() {}
};

} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_CSSRuleList_h */
