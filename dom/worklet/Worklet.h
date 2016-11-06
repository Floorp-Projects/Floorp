/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_Worklet_h
#define mozilla_dom_Worklet_h

#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "nsWrapperCache.h"
#include "nsCOMPtr.h"

class nsIGlobalObject;

namespace mozilla {
namespace dom {

class Promise;

class Worklet final : public nsISupports
                    , public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(Worklet)

  explicit Worklet(nsIGlobalObject* aGlobal)
    : mGlobal(aGlobal)
  {}

  nsIGlobalObject* GetParentObject() const
  {
    return mGlobal;
  }

  virtual JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  already_AddRefed<Promise>
  Import(const nsAString& aModuleURL, ErrorResult& aRv);

private:
  ~Worklet()
  {}

  nsCOMPtr<nsIGlobalObject> mGlobal;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_Worklet_h
