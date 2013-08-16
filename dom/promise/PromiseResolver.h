/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PromiseResolver_h
#define mozilla_dom_PromiseResolver_h

#include "mozilla/dom/Promise.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"

struct JSContext;

namespace mozilla {
namespace dom {

class PromiseResolver MOZ_FINAL : public nsWrapperCache
{
  friend class PromiseResolverTask;
  friend class WrapperPromiseCallback;
  friend class ResolvePromiseCallback;
  friend class RejectPromiseCallback;

private:
  enum PromiseTaskSync {
    SyncTask,
    AsyncTask
  };

public:
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(PromiseResolver)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(PromiseResolver)

  PromiseResolver(Promise* aPromise);
  virtual ~PromiseResolver();

  Promise* GetParentObject() const
  {
    return mPromise;
  }

  virtual JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;

  void Resolve(JSContext* aCx, const Optional<JS::Handle<JS::Value> >& aValue,
               PromiseTaskSync aSync = AsyncTask);

  void Reject(JSContext* aCx, const Optional<JS::Handle<JS::Value> >& aValue,
              PromiseTaskSync aSync = AsyncTask);

private:
  void ResolveInternal(JSContext* aCx,
                       const Optional<JS::Handle<JS::Value> >& aValue,
                       PromiseTaskSync aSync = AsyncTask);

  void RejectInternal(JSContext* aCx,
                      const Optional<JS::Handle<JS::Value> >& aValue,
                      PromiseTaskSync aSync = AsyncTask);

  void RunTask(JS::Handle<JS::Value> aValue,
               Promise::PromiseState aState, PromiseTaskSync aSync);

  nsRefPtr<Promise> mPromise;

  bool mResolvePending;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_PromiseResolver_h
