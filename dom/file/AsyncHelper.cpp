/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AsyncHelper.h"

#include "nsIRequestObserver.h"

#include "nsNetUtil.h"

#include "FileService.h"

USING_FILE_NAMESPACE

NS_IMPL_THREADSAFE_ISUPPORTS2(AsyncHelper, nsIRunnable, nsIRequest)

nsresult
AsyncHelper::AsyncWork(nsIRequestObserver* aObserver, nsISupports* aCtxt)
{
  nsresult rv;

  if (aObserver) {
    // build proxy for observer events
    rv = NS_NewRequestObserverProxy(getter_AddRefs(mObserver), aObserver, aCtxt);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  FileService* service = FileService::GetOrCreate();
  NS_ENSURE_TRUE(service, NS_ERROR_FAILURE);

  nsIEventTarget* target = service->StreamTransportTarget();

  rv = target->Dispatch(this, NS_DISPATCH_NORMAL);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
AsyncHelper::Run()
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

  if (mObserver) {
    mObserver->OnStartRequest(this, nullptr);
  }

  mStatus = DoStreamWork(mStream);

  if (mObserver) {
    mObserver->OnStopRequest(this, nullptr, mStatus);
  }

  return NS_OK;
}

NS_IMETHODIMP
AsyncHelper::GetName(nsACString& aName)
{
  NS_WARNING("Shouldn't be called!");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
AsyncHelper::IsPending(bool* _retval)
{
  NS_WARNING("Shouldn't be called!");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
AsyncHelper::GetStatus(nsresult* aStatus)
{
  *aStatus = mStatus;
  return NS_OK;
}

NS_IMETHODIMP
AsyncHelper::Cancel(nsresult aStatus)
{
  return NS_OK;
}

NS_IMETHODIMP
AsyncHelper::Suspend()
{
  NS_WARNING("Shouldn't be called!");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
AsyncHelper::Resume()
{
  NS_WARNING("Shouldn't be called!");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
AsyncHelper::GetLoadGroup(nsILoadGroup** aLoadGroup)
{
  NS_WARNING("Shouldn't be called!");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
AsyncHelper::SetLoadGroup(nsILoadGroup* aLoadGroup)
{
  NS_WARNING("Shouldn't be called!");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
AsyncHelper::GetLoadFlags(nsLoadFlags* aLoadFlags)
{
  NS_WARNING("Shouldn't be called!");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
AsyncHelper::SetLoadFlags(nsLoadFlags aLoadFlags)
{
  NS_WARNING("Shouldn't be called!");
  return NS_ERROR_NOT_IMPLEMENTED;
}
