/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ReportingObserver_h
#define mozilla_dom_ReportingObserver_h

#include "mozilla/Attributes.h"
#include "mozilla/dom/BindingUtils.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"

class nsPIDOMWindowInner;

namespace mozilla {
namespace dom {

class ReportingObserver final : public nsISupports
                              , public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(ReportingObserver)

  static already_AddRefed<ReportingObserver>
  Constructor(const GlobalObject& aGlobal,
              ReportingObserverCallback& aCallback,
              const ReportingObserverOptions& aOptions,
              ErrorResult& aRv);

  ReportingObserver(nsPIDOMWindowInner* aWindow,
                    ReportingObserverCallback& aCallback,
                    const nsTArray<nsString>& aTypes,
                    bool aBuffered);

  JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  nsPIDOMWindowInner*
  GetParentObject() const
  {
    return mWindow;
  }

  void
  Observe();

  void
  Disconnect();

  void
  TakeRecords(nsTArray<RefPtr<Report>>& aRecords);

private:
  ~ReportingObserver();

  nsCOMPtr<nsPIDOMWindowInner> mWindow;
  RefPtr<ReportingObserverCallback> mCallback;
  nsTArray<nsString> mTypes;
  bool mBuffered;
};

} // dom namespace
} // mozilla namespace

#endif // mozilla_dom_ReportingObserver_h
