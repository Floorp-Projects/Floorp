/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_FutureResolver_h
#define mozilla_dom_FutureResolver_h

#include "mozilla/dom/Future.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"

struct JSContext;

namespace mozilla {
namespace dom {

class FutureResolver MOZ_FINAL : public nsISupports,
                                 public nsWrapperCache
{
  friend class FutureResolverTask;

private:
  enum FutureTaskSync {
    SyncTask,
    AsyncTask
  };

public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(FutureResolver)

  FutureResolver(Future* aFuture);

  Future* GetParentObject() const
  {
    return mFuture;
  }

  virtual JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;

  void Resolve(JSContext* aCx, const Optional<JS::Handle<JS::Value> >& aValue,
               FutureTaskSync aSync = AsyncTask);

  void Reject(JSContext* aCx, const Optional<JS::Handle<JS::Value> >& aValue,
              FutureTaskSync aSync = AsyncTask);

private:
  void RunTask(JS::Handle<JS::Value> aValue,
               Future::FutureState aState, FutureTaskSync aSync);

  nsRefPtr<Future> mFuture;

  bool mResolvePending;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_FutureResolver_h
