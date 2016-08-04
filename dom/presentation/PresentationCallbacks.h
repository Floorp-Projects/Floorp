/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PresentationCallbacks_h
#define mozilla_dom_PresentationCallbacks_h

#include "mozilla/RefPtr.h"
#include "nsCOMPtr.h"
#include "nsIPresentationService.h"
#include "nsIWebProgressListener.h"
#include "nsString.h"
#include "nsWeakReference.h"

class nsIDocShell;
class nsIWebProgress;

namespace mozilla {
namespace dom {

class PresentationConnection;
class PresentationRequest;
class Promise;

class PresentationRequesterCallback : public nsIPresentationServiceCallback
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPRESENTATIONSERVICECALLBACK

  PresentationRequesterCallback(PresentationRequest* aRequest,
                                const nsAString& aUrl,
                                const nsAString& aSessionId,
                                Promise* aPromise);

protected:
  virtual ~PresentationRequesterCallback();

  RefPtr<PresentationRequest> mRequest;
  nsString mUrl;
  nsString mSessionId;
  RefPtr<Promise> mPromise;
};

class PresentationReconnectCallback final : public PresentationRequesterCallback
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIPRESENTATIONSERVICECALLBACK

  PresentationReconnectCallback(PresentationRequest* aRequest,
                                const nsAString& aUrl,
                                const nsAString& aSessionId,
                                Promise* aPromise,
                                PresentationConnection* aConnection);

private:
  virtual ~PresentationReconnectCallback();

  RefPtr<PresentationConnection> mConnection;
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
