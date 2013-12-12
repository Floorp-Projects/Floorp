/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_URLSearchParams_h
#define mozilla_dom_URLSearchParams_h

#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/ErrorResult.h"
#include "nsClassHashtable.h"
#include "nsHashKeys.h"

namespace mozilla {
namespace dom {

class URLSearchParams MOZ_FINAL
{
public:
  NS_INLINE_DECL_REFCOUNTING(URL)

  URLSearchParams();
  ~URLSearchParams();

  // WebIDL methods
  nsISupports* GetParentObject() const
  {
    return nullptr;
  }

  JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope);

  static already_AddRefed<URLSearchParams>
  Constructor(const GlobalObject& aGlobal, const nsAString& aInit,
              ErrorResult& aRv);

  static already_AddRefed<URLSearchParams>
  Constructor(const GlobalObject& aGlobal, URLSearchParams& aInit,
              ErrorResult& aRv);

  void Get(const nsAString& aName, nsString& aRetval);

  void GetAll(const nsAString& aName, nsTArray<nsString >& aRetval);

  void Set(const nsAString& aName, const nsAString& aValue);

  void Append(const nsAString& aName, const nsAString& aValue);

  bool Has(const nsAString& aName);

  void Delete(const nsAString& aName);

  uint32_t Size() const
  {
    return mSearchParams.Count();
  }

private:
  void ParseInput(const nsAString& aInput);

  void DeleteAll();

  void DecodeString(const nsAString& aInput, nsAString& aOutput);

  static PLDHashOperator
  CopyEnumerator(const nsAString& aName, nsTArray<nsString>* aArray,
                 void *userData);

  nsClassHashtable<nsStringHashKey, nsTArray<nsString>> mSearchParams;
};

} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_URLSearchParams_h */
