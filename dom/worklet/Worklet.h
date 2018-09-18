/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_Worklet_h
#define mozilla_dom_Worklet_h

#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "nsRefPtrHashtable.h"
#include "nsWrapperCache.h"
#include "nsCOMPtr.h"

class nsPIDOMWindowInner;

namespace mozilla {

class WorkletImpl;

namespace dom {

class Promise;
class WorkletFetchHandler;
struct WorkletOptions;
enum class CallerType : uint32_t;

class Worklet final : public nsISupports
                    , public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(Worklet)

  Worklet(nsPIDOMWindowInner* aWindow, RefPtr<WorkletImpl> aImpl);

  nsPIDOMWindowInner* GetParentObject() const
  {
    return mWindow;
  }

  virtual JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  already_AddRefed<Promise>
  AddModule(const nsAString& aModuleURL,
            const WorkletOptions& aOptions,
            CallerType aCallerType, ErrorResult& aRv);

private:
  ~Worklet();

  WorkletFetchHandler*
  GetImportFetchHandler(const nsACString& aURI);

  void
  AddImportFetchHandler(const nsACString& aURI, WorkletFetchHandler* aHandler);

  nsCOMPtr<nsPIDOMWindowInner> mWindow;

  nsRefPtrHashtable<nsCStringHashKey, WorkletFetchHandler> mImportHandlers;

  const RefPtr<WorkletImpl> mImpl;

  friend class WorkletFetchHandler;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_Worklet_h
