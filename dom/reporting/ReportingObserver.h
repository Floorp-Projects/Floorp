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
#include "nsIObserver.h"
#include "nsWeakReference.h"
#include "nsWrapperCache.h"
#include "nsTArray.h"

class nsPIDOMWindowInner;

namespace mozilla {
namespace dom {

class Report;
class ReportingObserverCallback;
struct ReportingObserverOptions;

class ReportingObserver final : public nsIObserver,
                                public nsWrapperCache,
                                public nsSupportsWeakReference {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_AMBIGUOUS(ReportingObserver,
                                                         nsIObserver)
  NS_DECL_NSIOBSERVER

  static already_AddRefed<ReportingObserver> Constructor(
      const GlobalObject& aGlobal, ReportingObserverCallback& aCallback,
      const ReportingObserverOptions& aOptions, ErrorResult& aRv);

  ReportingObserver(nsPIDOMWindowInner* aWindow,
                    ReportingObserverCallback& aCallback,
                    const nsTArray<nsString>& aTypes, bool aBuffered);

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  nsPIDOMWindowInner* GetParentObject() const { return mWindow; }

  void Observe();

  void Disconnect();

  void TakeRecords(nsTArray<RefPtr<Report>>& aRecords);

  void MaybeReport(Report* aReport);

  MOZ_CAN_RUN_SCRIPT void MaybeNotify();

 private:
  ~ReportingObserver();

  void Shutdown();

  nsTArray<RefPtr<Report>> mReports;

  nsCOMPtr<nsPIDOMWindowInner> mWindow;
  RefPtr<ReportingObserverCallback> mCallback;
  nsTArray<nsString> mTypes;
  bool mBuffered;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_ReportingObserver_h
