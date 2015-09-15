/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PresentationCallbacks_h
#define mozilla_dom_PresentationCallbacks_h

#include "mozilla/nsRefPtr.h"
#include "nsCOMPtr.h"
#include "nsIPresentationService.h"
#include "nsIWebProgressListener.h"
#include "nsString.h"
#include "nsWeakReference.h"

class nsIDocShell;
class nsIWebProgress;

namespace mozilla {
namespace dom {

class PresentationRequest;
class Promise;

class PresentationRequesterCallback final : public nsIPresentationServiceCallback
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPRESENTATIONSERVICECALLBACK

  PresentationRequesterCallback(PresentationRequest* aRequest,
                                const nsAString& aUrl,
                                const nsAString& aSessionId,
                                Promise* aPromise);

private:
  ~PresentationRequesterCallback();

  nsRefPtr<PresentationRequest> mRequest;
  nsString mSessionId;
  nsRefPtr<Promise> mPromise;
};

class PresentationResponderLoadingCallback final : public nsIWebProgressListener
                                                 , public nsSupportsWeakReference
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIWEBPROGRESSLISTENER

  explicit PresentationResponderLoadingCallback(const nsAString& aSessionId);

  nsresult Init(nsIDocShell* aDocShell);

private:
  ~PresentationResponderLoadingCallback();

  nsresult NotifyReceiverReady();

  nsString mSessionId;
  nsCOMPtr<nsIWebProgress> mProgress;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_PresentationCallbacks_h
