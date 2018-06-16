/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_Worklet_h
#define mozilla_dom_Worklet_h

#include "mozilla/Attributes.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/ErrorResult.h"
#include "nsRefPtrHashtable.h"
#include "nsWrapperCache.h"
#include "nsCOMPtr.h"

class nsPIDOMWindowInner;
class nsIPrincipal;

namespace mozilla {
namespace dom {

class Promise;
class Worklet;
class WorkletFetchHandler;
class WorkletGlobalScope;
class WorkletThread;
enum class CallerType : uint32_t;

class WorkletLoadInfo
{
public:
  WorkletLoadInfo(nsPIDOMWindowInner* aWindow, nsIPrincipal* aPrincipal);
  ~WorkletLoadInfo();

  uint64_t OuterWindowID() const { return mOuterWindowID; }
  uint64_t InnerWindowID() const { return mInnerWindowID; }
  bool DumpEnabled() const { return mDumpEnabled; }

  const OriginAttributes& OriginAttributesRef() const
  {
    return mOriginAttributes;
  }

  nsIPrincipal* Principal() const
  {
    MOZ_ASSERT(NS_IsMainThread());
    return mPrincipal;
  }

private:
  uint64_t mOuterWindowID;
  uint64_t mInnerWindowID;
  bool mDumpEnabled;
  OriginAttributes mOriginAttributes;
  nsCOMPtr<nsIPrincipal> mPrincipal;

  friend class Worklet;
  friend class WorkletThread;
};

class Worklet final : public nsISupports
                    , public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(Worklet)

  enum WorkletType {
    eAudioWorklet,
    ePaintWorklet,
  };

  Worklet(nsPIDOMWindowInner* aWindow, nsIPrincipal* aPrincipal,
          WorkletType aWorkletType);

  nsPIDOMWindowInner* GetParentObject() const
  {
    return mWindow;
  }

  virtual JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  already_AddRefed<Promise>
  Import(const nsAString& aModuleURL, CallerType aCallerType,
         ErrorResult& aRv);

  WorkletType Type() const
  {
    return mWorkletType;
  }

  static already_AddRefed<WorkletGlobalScope>
  CreateGlobalScope(JSContext* aCx, WorkletType aWorkletType);

  WorkletThread*
  GetOrCreateThread();

  const WorkletLoadInfo&
  LoadInfo() const
  {
    return mWorkletLoadInfo;
  }

private:
  ~Worklet();

  WorkletFetchHandler*
  GetImportFetchHandler(const nsACString& aURI);

  void
  AddImportFetchHandler(const nsACString& aURI, WorkletFetchHandler* aHandler);

  void
  TerminateThread();

  nsCOMPtr<nsPIDOMWindowInner> mWindow;

  WorkletType mWorkletType;

  nsRefPtrHashtable<nsCStringHashKey, WorkletFetchHandler> mImportHandlers;

  RefPtr<WorkletThread> mWorkletThread;

  WorkletLoadInfo mWorkletLoadInfo;

  friend class WorkletFetchHandler;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_Worklet_h
