/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ArchiveRequest.h"

#include "nsContentUtils.h"
#include "nsLayoutStatics.h"
#include "nsEventDispatcher.h"
#include "nsDOMClassInfoID.h"

USING_FILE_NAMESPACE

/**
 * Class used to make asynchronous the ArchiveRequest.
 */
class ArchiveRequestEvent : public nsRunnable
{
public:
  NS_DECL_NSIRUNNABLE

  ArchiveRequestEvent(ArchiveRequest* request)
  : mRequest(request)
  {
    MOZ_COUNT_CTOR(ArchiveRequestEvent);
  }

  ~ArchiveRequestEvent()
  {
    MOZ_COUNT_DTOR(ArchiveRequestEvent);
  }

private: //data
  nsRefPtr<ArchiveRequest> mRequest;
};

NS_IMETHODIMP
ArchiveRequestEvent::Run()
{
  NS_ABORT_IF_FALSE(mRequest, "the request is not longer valid");
  mRequest->Run();
  return NS_OK;
}

/* ArchiveRequest */

ArchiveRequest::ArchiveRequest(nsIDOMWindow* aWindow,
                               ArchiveReader* aReader)
: DOMRequest(aWindow),
  mArchiveReader(aReader)
{
  nsLayoutStatics::AddRef();

  /* An event to make this request asynchronous: */
  nsRefPtr<ArchiveRequestEvent> event = new ArchiveRequestEvent(this);
  NS_DispatchToCurrentThread(event);
}

ArchiveRequest::~ArchiveRequest()
{
  nsLayoutStatics::Release();
}

nsresult
ArchiveRequest::PreHandleEvent(nsEventChainPreVisitor& aVisitor)
{
  aVisitor.mCanHandle = true;
  aVisitor.mParentTarget = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
ArchiveRequest::GetReader(nsIDOMArchiveReader** aArchiveReader)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  nsCOMPtr<nsIDOMArchiveReader> archiveReader(mArchiveReader);
  archiveReader.forget(aArchiveReader);
  return NS_OK;
}

// Here the request is processed:
void
ArchiveRequest::Run()
{
  // Register this request to the reader.
  // When the reader is ready to return data, a 'Ready()' will be called
  nsresult rv = mArchiveReader->RegisterRequest(this);
  if (NS_FAILED(rv))
    FireError(rv);
}

void
ArchiveRequest::OpGetFilenames()
{
  mOperation = GetFilenames;
}

void
ArchiveRequest::OpGetFile(const nsAString& aFilename)
{
  mOperation = GetFile;
  mFilename = aFilename;
}

nsresult
ArchiveRequest::ReaderReady(nsTArray<nsCOMPtr<nsIDOMFile> >& aFileList,
                            nsresult aStatus)
{
  if (aStatus != NS_OK) {
    FireError(aStatus);
    return NS_OK;
  }

  jsval result;
  nsresult rv;

  nsIScriptContext* sc = GetContextForEventHandlers(&rv);
  NS_ENSURE_STATE(sc);

  JSContext* cx = sc->GetNativeContext();
  NS_ASSERTION(cx, "Failed to get a context!");

  JSObject* global = sc->GetNativeGlobal();
  NS_ASSERTION(global, "Failed to get global object!");

  JSAutoRequest ar(cx);
  JSAutoEnterCompartment ac;
  if (ac.enter(cx, global)) {
    switch (mOperation) {
      case GetFilenames:
        rv = GetFilenamesResult(cx, &result, aFileList);
        break;

      case GetFile:
        rv = GetFileResult(cx, &result, aFileList);
        break;
    }

    if (NS_FAILED(rv)) {
      NS_WARNING("Get*Result failed!");
    }
  } else {
    NS_WARNING("Failed to enter correct compartment!");
    rv = NS_ERROR_FAILURE;
  }

  if (NS_SUCCEEDED(rv)) {
    FireSuccess(result);
  }
  else {
    FireError(rv);
  }

  return NS_OK;
}

nsresult
ArchiveRequest::GetFilenamesResult(JSContext* aCx,
                                   jsval* aValue,
                                   nsTArray<nsCOMPtr<nsIDOMFile> >& aFileList)
{
  JSObject* array = JS_NewArrayObject(aCx, aFileList.Length(), nullptr);
  nsresult rv;

  if (!array) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  for (PRUint32 i = 0; i < aFileList.Length(); ++i) {
    nsCOMPtr<nsIDOMFile> file = aFileList[i];

    nsString filename;
    rv = file->GetName(filename);
    NS_ENSURE_SUCCESS(rv, rv);

    JSString* str = JS_NewUCStringCopyZ(aCx, filename.get());
    NS_ENSURE_TRUE(str, NS_ERROR_OUT_OF_MEMORY);

    jsval item = STRING_TO_JSVAL(str);

    if (rv != NS_OK || !JS_SetElement(aCx, array, i, &item)) {
      return NS_ERROR_FAILURE;
    }
  }

  if (!JS_FreezeObject(aCx, array)) {
    return NS_ERROR_FAILURE;
  }
  
  *aValue = OBJECT_TO_JSVAL(array);
  return NS_OK;
}

nsresult
ArchiveRequest::GetFileResult(JSContext* aCx,
                              jsval* aValue,
                              nsTArray<nsCOMPtr<nsIDOMFile> >& aFileList)
{
  for (PRUint32 i = 0; i < aFileList.Length(); ++i) {
    nsCOMPtr<nsIDOMFile> file = aFileList[i];

    nsString filename;
    nsresult rv = file->GetName(filename);
    NS_ENSURE_SUCCESS(rv, rv);

    if (filename == mFilename) {
      JSObject* scope = JS_GetGlobalForScopeChain(aCx);
      nsresult rv = nsContentUtils::WrapNative(aCx, scope, file, aValue, nullptr, true);
      return rv;
    }
  }

  return NS_ERROR_FAILURE;
}

// static
already_AddRefed<ArchiveRequest>
ArchiveRequest::Create(nsIDOMWindow* aOwner,
                       ArchiveReader* aReader)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  nsRefPtr<ArchiveRequest> request = new ArchiveRequest(aOwner, aReader);

  return request.forget();
}

NS_IMPL_CYCLE_COLLECTION_CLASS(ArchiveRequest)

// C++ traverse
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(ArchiveRequest,
                                                  DOMRequest)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR_AMBIGUOUS(mArchiveReader, nsIDOMArchiveReader)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

// Unlink
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(ArchiveRequest,
                                                DOMRequest)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mArchiveReader)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(ArchiveRequest)
  NS_INTERFACE_MAP_ENTRY(nsIDOMArchiveRequest)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(ArchiveRequest)
NS_INTERFACE_MAP_END_INHERITING(DOMRequest)

NS_IMPL_ADDREF_INHERITED(ArchiveRequest, DOMRequest)
NS_IMPL_RELEASE_INHERITED(ArchiveRequest, DOMRequest)

DOMCI_DATA(ArchiveRequest, ArchiveRequest)
