/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_Worklet_h
#define mozilla_dom_Worklet_h

#include "mozilla/Attributes.h"
#include "nsRefPtrHashtable.h"
#include "nsWrapperCache.h"
#include "nsCOMPtr.h"

class nsPIDOMWindowInner;

namespace mozilla {

class ErrorResult;
class WorkletImpl;

namespace dom {

class Promise;
class WorkletFetchHandler;
struct WorkletOptions;
enum class CallerType : uint32_t;

class Worklet final : public nsISupports, public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(Worklet)

  // |aOwnedObject| may be provided by the WorkletImpl as a parent thread
  // object to keep alive and traverse for CC as long as the Worklet has
  // references remaining.
  Worklet(nsPIDOMWindowInner* aWindow, RefPtr<WorkletImpl> aImpl,
          nsISupports* aOwnedObject = nullptr);

  nsPIDOMWindowInner* GetParentObject() const { return mWindow; }

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  already_AddRefed<Promise> AddModule(JSContext* aCx,
                                      const nsAString& aModuleURL,
                                      const WorkletOptions& aOptions,
                                      CallerType aCallerType, ErrorResult& aRv);

  WorkletImpl* Impl() const { return mImpl; }

 private:
  ~Worklet();

  WorkletFetchHandler* GetImportFetchHandler(const nsACString& aURI);

  void AddImportFetchHandler(const nsACString& aURI,
                             WorkletFetchHandler* aHandler);

  nsCOMPtr<nsPIDOMWindowInner> mWindow;
  nsCOMPtr<nsISupports> mOwnedObject;

  nsRefPtrHashtable<nsCStringHashKey, WorkletFetchHandler> mImportHandlers;

  const RefPtr<WorkletImpl> mImpl;

  friend class WorkletFetchHandler;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_Worklet_h
