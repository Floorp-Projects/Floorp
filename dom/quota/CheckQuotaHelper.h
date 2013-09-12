/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_quota_checkquotahelper_h__
#define mozilla_dom_quota_checkquotahelper_h__

#include "QuotaCommon.h"

#include "nsIInterfaceRequestor.h"
#include "nsIObserver.h"
#include "nsIRunnable.h"

#include "mozilla/CondVar.h"
#include "mozilla/Mutex.h"

class nsIPrincipal;
class nsPIDOMWindow;

BEGIN_QUOTA_NAMESPACE

class CheckQuotaHelper MOZ_FINAL : public nsIRunnable,
                                   public nsIInterfaceRequestor,
                                   public nsIObserver
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIRUNNABLE
  NS_DECL_NSIINTERFACEREQUESTOR
  NS_DECL_NSIOBSERVER

  CheckQuotaHelper(nsPIDOMWindow* aWindow,
                   mozilla::Mutex& aMutex);

  bool
  PromptAndReturnQuotaIsDisabled();

  void
  Cancel();

  static uint32_t
  GetQuotaPermission(nsIPrincipal* aPrincipal);

private:
  nsPIDOMWindow* mWindow;

  mozilla::Mutex& mMutex;
  mozilla::CondVar mCondVar;
  uint32_t mPromptResult;
  bool mWaiting;
  bool mHasPrompted;
};

END_QUOTA_NAMESPACE

#endif // mozilla_dom_quota_checkquotahelper_h__
