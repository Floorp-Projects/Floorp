/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ArchiveReader.h"
#include "ArchiveRequest.h"
#include "ArchiveEvent.h"
#include "ArchiveZipEvent.h"

#include "nsIURI.h"
#include "nsNetUtil.h"

#include "mozilla/dom/ArchiveReaderBinding.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/EncodingUtils.h"
#include "mozilla/Preferences.h"

using namespace mozilla;
using namespace mozilla::dom;
USING_ARCHIVEREADER_NAMESPACE

/* static */ already_AddRefed<ArchiveReader>
ArchiveReader::Constructor(const GlobalObject& aGlobal,
                           File& aBlob,
                           const ArchiveReaderOptions& aOptions,
                           ErrorResult& aError)
{
  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(aGlobal.GetAsSupports());
  if (!window) {
    aError.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  nsAutoCString encoding;
  if (!EncodingUtils::FindEncodingForLabelNoReplacement(aOptions.mEncoding,
                                                        encoding)) {
    aError.ThrowRangeError(MSG_ENCODING_NOT_SUPPORTED, &aOptions.mEncoding);
    return nullptr;
  }

  nsRefPtr<ArchiveReader> reader =
    new ArchiveReader(aBlob, window, encoding);
  return reader.forget();
}

ArchiveReader::ArchiveReader(File& aBlob, nsPIDOMWindow* aWindow,
                             const nsACString& aEncoding)
  : mFileImpl(aBlob.Impl())
  , mWindow(aWindow)
  , mStatus(NOT_STARTED)
  , mEncoding(aEncoding)
{
  MOZ_ASSERT(aWindow);
}

ArchiveReader::~ArchiveReader()
{
}

/* virtual */ JSObject*
ArchiveReader::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return ArchiveReaderBinding::Wrap(aCx, this, aGivenProto);
}

nsresult
ArchiveReader::RegisterRequest(ArchiveRequest* aRequest)
{
  switch (mStatus) {
    // Append to the list and let's start to work:
    case NOT_STARTED:
      mRequests.AppendElement(aRequest);
      return OpenArchive();

    // Just append to the list:
    case WORKING:
      mRequests.AppendElement(aRequest);
      return NS_OK;

    // Return data!
    case READY:
      RequestReady(aRequest);
      return NS_OK;
  }

  NS_ASSERTION(false, "unexpected mStatus value");
  return NS_OK;
}

// This returns the input stream
nsresult
ArchiveReader::GetInputStream(nsIInputStream** aInputStream)
{
  // Getting the input stream
  mFileImpl->GetInternalStream(aInputStream);
  NS_ENSURE_TRUE(*aInputStream, NS_ERROR_UNEXPECTED);
  return NS_OK;
}

nsresult
ArchiveReader::GetSize(uint64_t* aSize)
{
  ErrorResult rv;
  *aSize = mFileImpl->GetSize(rv);
  return rv.StealNSResult();
}

// Here we open the archive:
nsresult
ArchiveReader::OpenArchive()
{
  mStatus = WORKING;
  nsresult rv;

  // Target:
  nsCOMPtr<nsIEventTarget> target = do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID);
  NS_ASSERTION(target, "Must have stream transport service");

  // Here a Event to make everything async:
  nsRefPtr<ArchiveReaderEvent> event;

  /* FIXME: If we want to support more than 1 format we should check the content type here: */
  event = new ArchiveReaderZipEvent(this, mEncoding);
  rv = target->Dispatch(event, NS_DISPATCH_NORMAL);
  NS_ENSURE_SUCCESS(rv, rv);

  // In order to be sure that this object exists when the event finishes its task,
  // we increase the refcount here:
  AddRef();

  return NS_OK;
}

// Data received from the dispatched event:
void
ArchiveReader::Ready(nsTArray<nsCOMPtr<nsIDOMFile> >& aFileList,
                     nsresult aStatus)
{
  mStatus = READY;
 
  // Let's store the values:
  mData.fileList = aFileList;
  mData.status = aStatus;

  // Propagate the results:
  for (uint32_t index = 0; index < mRequests.Length(); ++index) {
    nsRefPtr<ArchiveRequest> request = mRequests[index];
    RequestReady(request);
  }

  mRequests.Clear();

  // The async operation is concluded, we can decrease the reference:
  Release();
}

void
ArchiveReader::RequestReady(ArchiveRequest* aRequest)
{
  // The request will do the rest:
  aRequest->ReaderReady(mData.fileList, mData.status);
}

already_AddRefed<ArchiveRequest>
ArchiveReader::GetFilenames()
{
  nsRefPtr<ArchiveRequest> request = GenerateArchiveRequest();
  request->OpGetFilenames();

  return request.forget();
}

already_AddRefed<ArchiveRequest>
ArchiveReader::GetFile(const nsAString& filename)
{
  nsRefPtr<ArchiveRequest> request = GenerateArchiveRequest();
  request->OpGetFile(filename);

  return request.forget();
}

already_AddRefed<ArchiveRequest>
ArchiveReader::GetFiles()
{
  nsRefPtr<ArchiveRequest> request = GenerateArchiveRequest();
  request->OpGetFiles();

  return request.forget();
}

already_AddRefed<ArchiveRequest>
ArchiveReader::GenerateArchiveRequest()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  return ArchiveRequest::Create(mWindow, this);
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(ArchiveReader,
                                      mFileImpl,
                                      mWindow,
                                      mData.fileList,
                                      mRequests)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ArchiveReader)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(ArchiveReader)
NS_IMPL_CYCLE_COLLECTING_RELEASE(ArchiveReader)
