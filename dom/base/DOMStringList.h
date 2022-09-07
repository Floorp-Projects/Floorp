/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_DOMStringList_h
#define mozilla_dom_DOMStringList_h

#include "nsISupports.h"
#include "nsTArray.h"
#include "nsWrapperCache.h"
#include "nsString.h"

namespace mozilla::dom {

class DOMStringList : public nsISupports, public nsWrapperCache {
 protected:
  virtual ~DOMStringList();

 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(DOMStringList)

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;
  nsISupports* GetParentObject() { return nullptr; }

  void IndexedGetter(uint32_t aIndex, bool& aFound, nsAString& aResult) {
    EnsureFresh();
    if (aIndex < mNames.Length()) {
      aFound = true;
      aResult = mNames[aIndex];
    } else {
      aFound = false;
    }
  }

  void Item(uint32_t aIndex, nsAString& aResult) {
    EnsureFresh();
    if (aIndex < mNames.Length()) {
      aResult = mNames[aIndex];
    } else {
      aResult.SetIsVoid(true);
    }
  }

  uint32_t Length() {
    EnsureFresh();
    return mNames.Length();
  }

  bool Contains(const nsAString& aString) {
    EnsureFresh();
    return mNames.Contains(aString);
  }

  bool Add(const nsAString& aName) {
    // XXXbz(Bug 1631374) mNames should really be a fallible array; otherwise
    // this return value is meaningless. return mNames.AppendElement(aName) !=
    // nullptr;
    mNames.AppendElement(aName);
    return true;
  }

  void Clear() { mNames.Clear(); }

  nsTArray<nsString>& StringArray() { return mNames; }

  void CopyList(nsTArray<nsString>& aNames) { aNames = mNames.Clone(); }

 protected:
  // A method that subclasses can override to modify mNames as needed
  // before we index into it or return its length or whatnot.
  virtual void EnsureFresh() {}

  // XXXbz we really want this to be a fallible array, but we end up passing it
  // to consumers who declare themselves as taking and nsTArray.  :(
  nsTArray<nsString> mNames;
};

}  // namespace mozilla::dom

#endif /* mozilla_dom_DOMStringList_h */
