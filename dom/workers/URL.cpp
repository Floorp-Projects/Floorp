/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "URL.h"
#include "File.h"

#include "nsTraceRefcnt.h"

#include "WorkerPrivate.h"
#include "nsThreadUtils.h"

#include "nsPIDOMWindow.h"
#include "nsGlobalWindow.h"
#include "nsHostObjectProtocolHandler.h"

#include "nsIDocument.h"
#include "nsIDOMFile.h"

USING_WORKERS_NAMESPACE
using mozilla::dom::WorkerGlobalObject;

// Base class for the Revoke and Create runnable objects.
class URLRunnable : public nsRunnable
{
protected:
  WorkerPrivate* mWorkerPrivate;
  uint32_t mSyncQueueKey;

private:
  class ResponseRunnable : public WorkerSyncRunnable
  {
    uint32_t mSyncQueueKey;

  public:
    ResponseRunnable(WorkerPrivate* aWorkerPrivate,
                     uint32_t aSyncQueueKey)
    : WorkerSyncRunnable(aWorkerPrivate, aSyncQueueKey, false),
      mSyncQueueKey(aSyncQueueKey)
    {
      NS_ASSERTION(aWorkerPrivate, "Don't hand me a null WorkerPrivate!");
    }

    bool
    WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate)
    {
      aWorkerPrivate->StopSyncLoop(mSyncQueueKey, true);
      return true;
    }

    bool
    PreDispatch(JSContext* aCx, WorkerPrivate* aWorkerPrivate)
    {
      AssertIsOnMainThread();
      return true;
    }

    void
    PostDispatch(JSContext* aCx, WorkerPrivate* aWorkerPrivate,
                 bool aDispatchResult)
    {
      AssertIsOnMainThread();
    }
  };

protected:
  URLRunnable(WorkerPrivate* aWorkerPrivate)
  : mWorkerPrivate(aWorkerPrivate)
  {
    mWorkerPrivate->AssertIsOnWorkerThread();
  }

public:
  bool
  Dispatch(JSContext* aCx)
  {
    mWorkerPrivate->AssertIsOnWorkerThread();
    mSyncQueueKey = mWorkerPrivate->CreateNewSyncLoop();

    if (NS_FAILED(NS_DispatchToMainThread(this, NS_DISPATCH_NORMAL))) {
      JS_ReportError(aCx, "Failed to dispatch to main thread!");
      mWorkerPrivate->StopSyncLoop(mSyncQueueKey, false);
      return false;
    }

    return mWorkerPrivate->RunSyncLoop(aCx, mSyncQueueKey);
  }

private:
  NS_IMETHOD Run()
  {
    AssertIsOnMainThread();

    MainThreadRun();

    nsRefPtr<ResponseRunnable> response =
      new ResponseRunnable(mWorkerPrivate, mSyncQueueKey);
    if (!response->Dispatch(nullptr)) {
      NS_WARNING("Failed to dispatch response!");
    }

    return NS_OK;
  }

protected:
  virtual void
  MainThreadRun() = 0;
};

// This class creates an URL from a DOM Blob on the main thread.
class CreateURLRunnable : public URLRunnable
{
private:
  nsIDOMBlob* mBlob;
  nsString& mURL;

public:
  CreateURLRunnable(WorkerPrivate* aWorkerPrivate, nsIDOMBlob* aBlob,
                    const mozilla::dom::objectURLOptionsWorkers& aOptions,
                    nsString& aURL)
  : URLRunnable(aWorkerPrivate),
    mBlob(aBlob),
    mURL(aURL)
  {
    MOZ_ASSERT(aBlob);
  }

  void
  MainThreadRun()
  {
    AssertIsOnMainThread();

    nsCOMPtr<nsIPrincipal> principal;
    nsIDocument* doc = nullptr;

    nsCOMPtr<nsPIDOMWindow> window = mWorkerPrivate->GetWindow();
    if (window) {
      doc = window->GetExtantDoc();
      if (!doc) {
        SetDOMStringToNull(mURL);
        return;
      }

      principal = doc->NodePrincipal();
    } else {
      MOZ_ASSERT_IF(!mWorkerPrivate->GetParent(), mWorkerPrivate->IsChromeWorker());
      principal = mWorkerPrivate->GetPrincipal();
    }

    nsCString url;
    nsresult rv = nsHostObjectProtocolHandler::AddDataEntry(
        NS_LITERAL_CSTRING(BLOBURI_SCHEME),
        mBlob, principal, url);

    if (NS_FAILED(rv)) {
      NS_WARNING("Failed to add data entry for the blob!");
      SetDOMStringToNull(mURL);
      return;
    }

    if (doc) {
      doc->RegisterHostObjectUri(url);
    } else {
      mWorkerPrivate->RegisterHostObjectURI(url);
    }

    mURL = NS_ConvertUTF8toUTF16(url);
  }
};

// This class revokes an URL on the main thread.
class RevokeURLRunnable : public URLRunnable
{
private:
  const nsString mURL;

public:
  RevokeURLRunnable(WorkerPrivate* aWorkerPrivate,
                    const nsAString& aURL)
  : URLRunnable(aWorkerPrivate),
    mURL(aURL)
  {}

  void
  MainThreadRun()
  {
    AssertIsOnMainThread();

    nsCOMPtr<nsIPrincipal> principal;
    nsIDocument* doc = nullptr;

    nsCOMPtr<nsPIDOMWindow> window = mWorkerPrivate->GetWindow();
    if (window) {
      doc = window->GetExtantDoc();
      if (!doc) {
        return;
      }

      principal = doc->NodePrincipal();
    } else {
      MOZ_ASSERT_IF(!mWorkerPrivate->GetParent(), mWorkerPrivate->IsChromeWorker());
      principal = mWorkerPrivate->GetPrincipal();
    }

    NS_ConvertUTF16toUTF8 url(mURL);

    nsIPrincipal* urlPrincipal =
      nsHostObjectProtocolHandler::GetDataEntryPrincipal(url);

    bool subsumes;
    if (urlPrincipal &&
        NS_SUCCEEDED(principal->Subsumes(urlPrincipal, &subsumes)) &&
        subsumes) {
      if (doc) {
        doc->UnregisterHostObjectUri(url);
      }

      nsHostObjectProtocolHandler::RemoveDataEntry(url);
    }

    if (!window) {
      mWorkerPrivate->UnregisterHostObjectURI(url);
    }
  }
};

// static
void
URL::CreateObjectURL(const WorkerGlobalObject& aGlobal, JSObject* aBlob,
                     const mozilla::dom::objectURLOptionsWorkers& aOptions,
                     nsString& aResult, mozilla::ErrorResult& aRv)
{
  JSContext* cx = aGlobal.GetContext();
  WorkerPrivate* workerPrivate = GetWorkerPrivateFromContext(cx);

  nsCOMPtr<nsIDOMBlob> blob = file::GetDOMBlobFromJSObject(aBlob);
  if (!blob) {
    SetDOMStringToNull(aResult);

    NS_NAMED_LITERAL_STRING(argStr, "Argument 1 of URL.createObjectURL");
    NS_NAMED_LITERAL_STRING(blobStr, "Blob");
    aRv.ThrowTypeError(MSG_DOES_NOT_IMPLEMENT_INTERFACE, &argStr, &blobStr);
    return;
  }

  nsRefPtr<CreateURLRunnable> runnable =
    new CreateURLRunnable(workerPrivate, blob, aOptions, aResult);

  if (!runnable->Dispatch(cx)) {
    JS_ReportPendingException(cx);
  }
}

// static
void
URL::CreateObjectURL(const WorkerGlobalObject& aGlobal, JSObject& aBlob,
                     const mozilla::dom::objectURLOptionsWorkers& aOptions,
                     nsString& aResult, mozilla::ErrorResult& aRv)
{
  return CreateObjectURL(aGlobal, &aBlob, aOptions, aResult, aRv);
}

// static
void
URL::RevokeObjectURL(const WorkerGlobalObject& aGlobal, const nsAString& aUrl)
{
  JSContext* cx = aGlobal.GetContext();
  WorkerPrivate* workerPrivate = GetWorkerPrivateFromContext(cx);

  nsRefPtr<RevokeURLRunnable> runnable =
    new RevokeURLRunnable(workerPrivate, aUrl);

  if (!runnable->Dispatch(cx)) {
    JS_ReportPendingException(cx);
  }
}

