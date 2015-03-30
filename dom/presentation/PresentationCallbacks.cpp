/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/Promise.h"
#include "PresentationCallbacks.h"
#include "PresentationSession.h"

using namespace mozilla;
using namespace mozilla::dom;

NS_IMPL_ISUPPORTS(PresentationRequesterCallback, nsIPresentationServiceCallback)

PresentationRequesterCallback::PresentationRequesterCallback(nsPIDOMWindow* aWindow,
                                                             const nsAString& aUrl,
                                                             const nsAString& aSessionId,
                                                             Promise* aPromise)
  : mWindow(aWindow)
  , mSessionId(aSessionId)
  , mPromise(aPromise)
{
  MOZ_ASSERT(mWindow);
  MOZ_ASSERT(mPromise);
  MOZ_ASSERT(!mSessionId.IsEmpty());
}

PresentationRequesterCallback::~PresentationRequesterCallback()
{
}

NS_IMETHODIMP
PresentationRequesterCallback::NotifySuccess()
{
  MOZ_ASSERT(NS_IsMainThread());

  // At the sender side, this function must get called after the transport
  // channel is ready. So we simply set the session state as connected.
  nsRefPtr<PresentationSession> session =
    PresentationSession::Create(mWindow, mSessionId, PresentationSessionState::Connected);
  if (!session) {
    mPromise->MaybeReject(NS_ERROR_DOM_ABORT_ERR);
    return NS_OK;
  }

  mPromise->MaybeResolve(session);
  return NS_OK;
}

NS_IMETHODIMP
PresentationRequesterCallback::NotifyError(nsresult aError)
{
  MOZ_ASSERT(NS_IsMainThread());

  mPromise->MaybeReject(aError);
  return NS_OK;
}
