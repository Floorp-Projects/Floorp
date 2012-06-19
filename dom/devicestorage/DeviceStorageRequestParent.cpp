/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DeviceStorageRequestParent.h"
#include "nsDOMFile.h"
#include "nsIMIMEService.h"
#include "nsCExternalHandlerService.h"
#include "mozilla/unused.h"

namespace mozilla {
namespace dom {
namespace devicestorage {

DeviceStorageRequestParent::DeviceStorageRequestParent(const DeviceStorageParams& aParams)
{
  MOZ_COUNT_CTOR(DeviceStorageRequestParent);

  switch (aParams.type()) {
    case DeviceStorageParams::TDeviceStorageAddParams:
    {
      DeviceStorageAddParams p = aParams;

      nsCOMPtr<nsIFile> f;
      NS_NewLocalFile(p.fullpath(), false, getter_AddRefs(f));

      nsRefPtr<DeviceStorageFile> dsf = new DeviceStorageFile(f);
      nsRefPtr<WriteFileEvent> r = new WriteFileEvent(this, dsf, p.bits());

      nsCOMPtr<nsIEventTarget> target = do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID);
      NS_ASSERTION(target, "Must have stream transport service");
      target->Dispatch(r, NS_DISPATCH_NORMAL);
      break;
    }

    case DeviceStorageParams::TDeviceStorageGetParams:
    {
      DeviceStorageGetParams p = aParams;

      nsCOMPtr<nsIFile> f;
      NS_NewLocalFile(p.fullpath(), false, getter_AddRefs(f));

      nsRefPtr<DeviceStorageFile> dsf = new DeviceStorageFile(f);
      nsRefPtr<ReadFileEvent> r = new ReadFileEvent(this, dsf);

      nsCOMPtr<nsIEventTarget> target = do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID);
      NS_ASSERTION(target, "Must have stream transport service");
      target->Dispatch(r, NS_DISPATCH_NORMAL);
      break;
    }

    case DeviceStorageParams::TDeviceStorageDeleteParams:
    {
      DeviceStorageDeleteParams p = aParams;

      nsCOMPtr<nsIFile> f;
      NS_NewLocalFile(p.fullpath(), false, getter_AddRefs(f));

      nsRefPtr<DeviceStorageFile> dsf = new DeviceStorageFile(f);
      nsRefPtr<DeleteFileEvent> r = new DeleteFileEvent(this, dsf);

      nsCOMPtr<nsIEventTarget> target = do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID);
      NS_ASSERTION(target, "Must have stream transport service");
      target->Dispatch(r, NS_DISPATCH_NORMAL);
      break;
    }

    case DeviceStorageParams::TDeviceStorageEnumerationParams:
    {
      DeviceStorageEnumerationParams p = aParams;

      nsCOMPtr<nsIFile> f;
      NS_NewLocalFile(p.fullpath(), false, getter_AddRefs(f));

      nsRefPtr<DeviceStorageFile> dsf = new DeviceStorageFile(f);
      nsRefPtr<EnumerateFileEvent> r = new EnumerateFileEvent(this, dsf, p.since());

      nsCOMPtr<nsIEventTarget> target = do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID);
      NS_ASSERTION(target, "Must have stream transport service");
      target->Dispatch(r, NS_DISPATCH_NORMAL);
      break;
    }
    default:
    {
      NS_RUNTIMEABORT("not reached");
      break;
    }
  }
}

DeviceStorageRequestParent::~DeviceStorageRequestParent()
{
  MOZ_COUNT_DTOR(DeviceStorageRequestParent);
}

DeviceStorageRequestParent::PostErrorEvent::PostErrorEvent(DeviceStorageRequestParent* aParent,
                                                           const char* aError)
  : mParent(aParent)
{
  mError.AssignWithConversion(aError);
}

DeviceStorageRequestParent::PostErrorEvent::~PostErrorEvent() {}

NS_IMETHODIMP
DeviceStorageRequestParent::PostErrorEvent::Run() {
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  ErrorResponse response(mError);
  unused << mParent->Send__delete__(mParent, response);
  return NS_OK;
}


DeviceStorageRequestParent::PostSuccessEvent::PostSuccessEvent(DeviceStorageRequestParent* aParent)
  : mParent(aParent)
{
}

DeviceStorageRequestParent::PostSuccessEvent::~PostSuccessEvent() {}

NS_IMETHODIMP
DeviceStorageRequestParent::PostSuccessEvent::Run() {
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  SuccessResponse response;
  unused <<  mParent->Send__delete__(mParent, response);
  return NS_OK;
}

DeviceStorageRequestParent::PostBlobSuccessEvent::PostBlobSuccessEvent(DeviceStorageRequestParent* aParent,
                                                                       void* aBuffer,
                                                                       PRUint32 aLength,
                                                                       nsACString& aMimeType)
  : mParent(aParent)
  , mMimeType(aMimeType)
{
  mBits.SetCapacity(aLength);
  void* bits = mBits.Elements();
  memcpy(bits, aBuffer, aLength);
}

DeviceStorageRequestParent::PostBlobSuccessEvent::~PostBlobSuccessEvent() {}

NS_IMETHODIMP
DeviceStorageRequestParent::PostBlobSuccessEvent::Run() {
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  BlobResponse response(mBits, mMimeType);
  unused <<  mParent->Send__delete__(mParent, response);
  return NS_OK;
}

DeviceStorageRequestParent::PostEnumerationSuccessEvent::PostEnumerationSuccessEvent(DeviceStorageRequestParent* aParent,
                                                                                     InfallibleTArray<DeviceStorageFileValue>& aPaths)
  : mParent(aParent)
  , mPaths(aPaths)
{
}

DeviceStorageRequestParent::PostEnumerationSuccessEvent::~PostEnumerationSuccessEvent() {}

NS_IMETHODIMP
DeviceStorageRequestParent::PostEnumerationSuccessEvent::Run() {
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  EnumerationResponse response(mPaths);
  unused <<  mParent->Send__delete__(mParent, response);
  return NS_OK;
}

DeviceStorageRequestParent::WriteFileEvent::WriteFileEvent(DeviceStorageRequestParent* aParent,
                                                           DeviceStorageFile* aFile,
                                                           InfallibleTArray<PRUint8>& aBits)
  : mParent(aParent)
  , mFile(aFile)
  , mBits(aBits)
{
}

DeviceStorageRequestParent::WriteFileEvent::~WriteFileEvent()
{
}

NS_IMETHODIMP
DeviceStorageRequestParent::WriteFileEvent::Run()
{
  nsRefPtr<nsRunnable> r;
  nsresult rv = mFile->Write(mBits);

  if (NS_FAILED(rv)) {
    r = new PostErrorEvent(mParent, POST_ERROR_EVENT_UNKNOWN);
  }
  else {
    r = new PostPathResultEvent(mParent, mFile->mPath);
  }

  NS_DispatchToMainThread(r);
  return NS_OK;
}


DeviceStorageRequestParent::DeleteFileEvent::DeleteFileEvent(DeviceStorageRequestParent* aParent,
                                                             DeviceStorageFile* aFile)
  : mParent(aParent)
  , mFile(aFile)
{
}

DeviceStorageRequestParent::DeleteFileEvent::~DeleteFileEvent()
{
}

NS_IMETHODIMP
DeviceStorageRequestParent::DeleteFileEvent::Run()
{
  mFile->mFile->Remove(true);

  nsRefPtr<nsRunnable> r;

  bool check = false;
  mFile->mFile->Exists(&check);
  if (check) {
    r = new PostErrorEvent(mParent, POST_ERROR_EVENT_UNKNOWN);
  }
  else {
    r = new PostPathResultEvent(mParent, mFile->mPath);
  }
  NS_DispatchToMainThread(r);
  return NS_OK;
}

DeviceStorageRequestParent::ReadFileEvent::ReadFileEvent(DeviceStorageRequestParent* aParent,
                                                         DeviceStorageFile* aFile)
  : mParent(aParent)
  , mFile(aFile)
{
  nsCOMPtr<nsIMIMEService> mimeService = do_GetService(NS_MIMESERVICE_CONTRACTID);
  if (mimeService) {
    nsresult rv = mimeService->GetTypeFromFile(mFile->mFile, mMimeType);
    if (NS_FAILED(rv)) {
      mMimeType.Truncate();
    }
  }
}

DeviceStorageRequestParent::ReadFileEvent::~ReadFileEvent()
{
}

NS_IMETHODIMP
DeviceStorageRequestParent::ReadFileEvent::Run()
{
  nsCOMPtr<nsIRunnable> r;
  bool check = false;
  mFile->mFile->Exists(&check);
  if (!check) {
    r = new PostErrorEvent(mParent, POST_ERROR_EVENT_FILE_DOES_NOT_EXIST);
    NS_DispatchToMainThread(r);
    return NS_OK;
  }

  PRInt64 fileSize;
  nsresult rv = mFile->mFile->GetFileSize(&fileSize);
  if (NS_FAILED(rv)) {
    r = new PostErrorEvent(mParent, POST_ERROR_EVENT_UNKNOWN);
    NS_DispatchToMainThread(r);
    return NS_OK;
  }
  
  PRFileDesc *fileHandle;
  rv = mFile->mFile->OpenNSPRFileDesc(PR_RDONLY, 0, &fileHandle);

  // i am going to hell.  this is temp until bent provides seralizaiton of blobs.
  void* buf = (void*) malloc(fileSize);
  PRInt32 read = PR_Read(fileHandle, buf, fileSize); 
  if (read != fileSize) {
    r = new PostErrorEvent(mParent, POST_ERROR_EVENT_UNKNOWN);
    NS_DispatchToMainThread(r);
    return NS_OK;
  }

  r = new PostBlobSuccessEvent(mParent, buf, fileSize, mMimeType);

  PR_Free(buf);
  PR_Close(fileHandle);

  NS_DispatchToMainThread(r);
  return NS_OK;
}

DeviceStorageRequestParent::EnumerateFileEvent::EnumerateFileEvent(DeviceStorageRequestParent* aParent,
                                                                   DeviceStorageFile* aFile,
                                                                   PRUint32 aSince)
  : mParent(aParent)
  , mFile(aFile)
  , mSince(aSince)
{
}

DeviceStorageRequestParent::EnumerateFileEvent::~EnumerateFileEvent()
{
}

NS_IMETHODIMP
DeviceStorageRequestParent::EnumerateFileEvent::Run()
{
  nsCOMPtr<nsIRunnable> r;
  bool check = false;
  mFile->mFile->Exists(&check);
  if (!check) {
    r = new PostErrorEvent(mParent, POST_ERROR_EVENT_FILE_DOES_NOT_EXIST);
    NS_DispatchToMainThread(r);
    return NS_OK;
  }

  nsTArray<nsRefPtr<DeviceStorageFile> > files;
  mFile->CollectFiles(files, mSince);

  InfallibleTArray<DeviceStorageFileValue> values;

  PRUint32 count = files.Length();
  for (PRUint32 i = 0; i < count; i++) {
    nsString fullpath;
    files[i]->mFile->GetPath(fullpath);
    DeviceStorageFileValue dsvf(fullpath, files[i]->mPath);
    values.AppendElement(dsvf);
  }

  r = new PostEnumerationSuccessEvent(mParent, values);
  NS_DispatchToMainThread(r);
  return NS_OK;
}


DeviceStorageRequestParent::PostPathResultEvent::PostPathResultEvent(DeviceStorageRequestParent* aParent,
                                                             const nsAString& aPath)
  : mParent(aParent)
  , mPath(aPath)
{
}

DeviceStorageRequestParent::PostPathResultEvent::~PostPathResultEvent()
{
}

NS_IMETHODIMP
DeviceStorageRequestParent::PostPathResultEvent::Run()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  SuccessResponse response;
  unused <<  mParent->Send__delete__(mParent, response);
  return NS_OK;
}


} // namespace devicestorage
} // namespace dom
} // namespace mozilla
