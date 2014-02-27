/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_telephony_TelephonyIPCProvider_h
#define mozilla_dom_telephony_TelephonyIPCProvider_h

#include "mozilla/dom/telephony/TelephonyCommon.h"
#include "mozilla/Attributes.h"
#include "nsIObserver.h"
#include "nsITelephonyProvider.h"

BEGIN_TELEPHONY_NAMESPACE

struct IPCTelephonyRequest;
class PTelephonyChild;

class TelephonyIPCProvider MOZ_FINAL : public nsITelephonyProvider
                                     , public nsITelephonyListener
                                     , public nsIObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSITELEPHONYPROVIDER
  NS_DECL_NSITELEPHONYLISTENER
  NS_DECL_NSIOBSERVER

  TelephonyIPCProvider();

protected:
  virtual ~TelephonyIPCProvider();

private:
  nsTArray<nsCOMPtr<nsITelephonyListener> > mListeners;
  PTelephonyChild* mPTelephonyChild;
  uint32_t mDefaultServiceId;

  nsresult SendRequest(nsITelephonyListener *aListener,
                       nsITelephonyCallback *aCallback,
                       const IPCTelephonyRequest& aRequest);
};

END_TELEPHONY_NAMESPACE

#endif // mozilla_dom_telephony_TelephonyIPCProvider_h
