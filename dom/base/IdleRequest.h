/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_idlerequest_h
#define mozilla_dom_idlerequest_h

#include "mozilla/LinkedList.h"
#include "mozilla/Maybe.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsDOMNavigationTiming.h"
#include "nsITimeoutHandler.h"

class nsPIDOMWindowInner;

namespace mozilla {
namespace dom {

class IdleRequestCallback;

class IdleRequest final : public nsITimeoutHandler
                        , public nsIRunnable
                        , public nsICancelableRunnable
                        , public nsIIncrementalRunnable
                        , public LinkedListElement<IdleRequest>
{
public:
  IdleRequest(JSContext* aCx, nsPIDOMWindowInner* aWindow,
              IdleRequestCallback& aCallback, uint32_t aHandle);

  virtual nsresult Call() override;
  virtual void GetLocation(const char** aFileName, uint32_t* aLineNo,
                           uint32_t* aColumn) override;
  virtual void MarkForCC() override;

  nsresult SetTimeout(uint32_t aTimout);
  nsresult RunIdleRequestCallback(bool aDidTimeout);
  void CancelTimeout();

  NS_DECL_NSIRUNNABLE;
  virtual nsresult Cancel() override;
  virtual void SetDeadline(mozilla::TimeStamp aDeadline) override;

  uint32_t Handle() const
  {
    return mHandle;
  }

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(IdleRequest, nsITimeoutHandler)

private:
  ~IdleRequest();

  // filename, line number and JS language version string of the
  // caller of setTimeout()
  nsCString mFileName;
  uint32_t mLineNo;
  uint32_t mColumn;

  nsCOMPtr<nsPIDOMWindowInner> mWindow;
  RefPtr<IdleRequestCallback> mCallback;
  uint32_t mHandle;
  mozilla::Maybe<int32_t> mTimeoutHandle;
  DOMHighResTimeStamp mDeadline;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_idlerequest_h
