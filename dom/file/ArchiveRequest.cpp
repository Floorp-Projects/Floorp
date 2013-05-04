/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ArchiveRequest.h"

#include "mozilla/dom/ArchiveRequestBinding.h"
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

// ArchiveRequest

ArchiveRequest::ArchiveRequest(nsIDOMWindow* aWindow,
                               ArchiveReader* aReader)
: DOMRequest(aWindow),
  mArchiveReader(aReader)
{
  MOZ_ASSERT(aReader);

  MOZ_COUNT_CTOR(ArchiveRequest);
  nsLayoutStatics::AddRef();

  /* An event to make this request asynchronous: */
  nsRefPtr<ArchiveRequestEvent> event = new ArchiveRequestEvent(this);
  NS_DispatchToCurrentThread(event);
}

ArchiveRequest::~ArchiveRequest()
{
  MOZ_COUNT_DTOR(ArchiveRequest);
  nsLayoutStatics::Release();
}

nsresult
ArchiveRequest::PreHandleEvent(nsEventChainPreVisitor& aVisitor)
{
  aVisitor.mCanHandle = true;
  aVisitor.mParentTarget = nullptr;
  return NS_OK;
}

/* virtual */ JSObject*
ArchiveRequest::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return ArchiveRequestBinding::Wrap(aCx, aScope, this);
}

ArchiveReader*
ArchiveRequest::Reader() const
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  return mArchiveReader;
}

// Here the request is processed:
void
ArchiveRequest::Run()
{
  // Register this request to the reader.
  // When the reader is ready to return data, a 'Ready()' will be called
  nsresult rv = mArchiveReader->RegisterRequest(this);
  if (NS_FAILED(rv)) {
    FireError(rv);
  }
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

void
ArchiveRequest::OpGetFiles()
{
  mOperation = GetFiles;
}

nsresult
ArchiveRequest::ReaderReady(nsTArray<nsCOMPtr<nsIDOMFile> >& aFileList,
                            nsresult aStatus)
{
  if (NS_FAILED(aStatus)) {
    FireError(aStatus);
    return NS_OK;
  }

  JS::Value result;
  nsresult rv;

  nsIScriptContext* sc = GetContextForEventHandlers(&rv);
  NS_ENSURE_STATE(sc);

  AutoPushJSContext cx(sc->GetNativeContext());
  NS_ASSERTION(cx, "Failed to get a context!");

  JSObject* global = sc->GetNativeGlobal();
  NS_ASSERTION(global, "Failed to get global object!");

  JSAutoRequest ar(cx);
  JSAutoCompartment ac(cx, global);

  switch (mOperation) {
    case GetFilenames:
      rv = GetFilenamesResult(cx, &result, aFileList);
      break;

    case GetFile:
      rv = GetFileResult(cx, &result, aFileList);
      break;

      case GetFiles:
        rv = GetFilesResult(cx, &result, aFileList);
        break;
  }

  if (NS_FAILED(rv)) {
    NS_WARNING("Get*Result failed!");
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
                                   JS::Value* aValue,
                                   nsTArray<nsCOMPtr<nsIDOMFile> >& aFileList)
{
  JSObject* array = JS_NewArrayObject(aCx, aFileList.Length(), nullptr);
  nsresult rv;

  if (!array) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  for (uint32_t i = 0; i < aFileList.Length(); ++i) {
    nsCOMPtr<nsIDOMFile> file = aFileList[i];

    nsString filename;
    rv = file->GetName(filename);
    NS_ENSURE_SUCCESS(rv, rv);

    JSString* str = JS_NewUCStringCopyZ(aCx, filename.get());
    NS_ENSURE_TRUE(str, NS_ERROR_OUT_OF_MEMORY);

    JS::Value item = STRING_TO_JSVAL(str);

    if (NS_FAILED(rv) || !JS_SetElement(aCx, array, i, &item)) {
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
                              JS::Value* aValue,
                              nsTArray<nsCOMPtr<nsIDOMFile> >& aFileList)
{
  for (uint32_t i = 0; i < aFileList.Length(); ++i) {
    nsCOMPtr<nsIDOMFile> file = aFileList[i];

    nsString filename;
    nsresult rv = file->GetName(filename);
    NS_ENSURE_SUCCESS(rv, rv);

    if (filename == mFilename) {
      JS::Rooted<JSObject*> global(aCx, JS_GetGlobalForScopeChain(aCx));
      return nsContentUtils::WrapNative(aCx, global, file,
                                        &NS_GET_IID(nsIDOMFile), aValue);
    }
  }

  return NS_ERROR_FAILURE;
}

nsresult
ArchiveRequest::GetFilesResult(JSContext* aCx,
                               JS::Value* aValue,
                               nsTArray<nsCOMPtr<nsIDOMFile> >& aFileList)
{
  JSObject* array = JS_NewArrayObject(aCx, aFileList.Length(), nullptr);
  if (!array) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  for (uint32_t i = 0; i < aFileList.Length(); ++i) {
    nsCOMPtr<nsIDOMFile> file = aFileList[i];

    JS::Rooted<JS::Value> value(aCx);
    JS::Rooted<JSObject*> global(aCx, JS_GetGlobalForScopeChain(aCx));
    nsresult rv = nsContentUtils::WrapNative(aCx, global, file,
                                             &NS_GET_IID(nsIDOMFile),
                                             value.address());
    if (NS_FAILED(rv) || !JS_SetElement(aCx, array, i, value.address())) {
      return NS_ERROR_FAILURE;
    }
  }

  aValue->setObject(*array);
  return NS_OK;
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

NS_IMPL_CYCLE_COLLECTION_INHERITED_1(ArchiveRequest, DOMRequest,
                                     mArchiveReader)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(ArchiveRequest)
NS_INTERFACE_MAP_END_INHERITING(DOMRequest)

NS_IMPL_ADDREF_INHERITED(ArchiveRequest, DOMRequest)
NS_IMPL_RELEASE_INHERITED(ArchiveRequest, DOMRequest)

DOMCI_DATA(ArchiveRequest, ArchiveRequest)
