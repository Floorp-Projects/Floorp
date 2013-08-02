/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DeviceStorageRequestParent.h"
#include "nsDOMFile.h"
#include "nsIMIMEService.h"
#include "nsCExternalHandlerService.h"
#include "mozilla/unused.h"
#include "mozilla/dom/ipc/Blob.h"
#include "ContentParent.h"
#include "nsProxyRelease.h"
#include "AppProcessChecker.h"
#include "mozilla/Preferences.h"

namespace mozilla {
namespace dom {
namespace devicestorage {

DeviceStorageRequestParent::DeviceStorageRequestParent(const DeviceStorageParams& aParams)
  : mParams(aParams)
  , mMutex("DeviceStorageRequestParent::mMutex")
  , mActorDestoryed(false)
{
  MOZ_COUNT_CTOR(DeviceStorageRequestParent);

  DebugOnly<DeviceStorageUsedSpaceCache*> usedSpaceCache = DeviceStorageUsedSpaceCache::CreateOrGet();
  NS_ASSERTION(usedSpaceCache, "DeviceStorageUsedSpaceCache is null");
}

void
DeviceStorageRequestParent::Dispatch()
{
  switch (mParams.type()) {
    case DeviceStorageParams::TDeviceStorageAddParams:
    {
      DeviceStorageAddParams p = mParams;

      nsRefPtr<DeviceStorageFile> dsf =
        new DeviceStorageFile(p.type(), p.storageName(), p.relpath());

      BlobParent* bp = static_cast<BlobParent*>(p.blobParent());
      nsCOMPtr<nsIDOMBlob> blob = bp->GetBlob();

      nsCOMPtr<nsIInputStream> stream;
      blob->GetInternalStream(getter_AddRefs(stream));

      nsRefPtr<CancelableRunnable> r = new WriteFileEvent(this, dsf, stream);

      nsCOMPtr<nsIEventTarget> target = do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID);
      NS_ASSERTION(target, "Must have stream transport service");
      target->Dispatch(r, NS_DISPATCH_NORMAL);
      break;
    }

    case DeviceStorageParams::TDeviceStorageGetParams:
    {
      DeviceStorageGetParams p = mParams;
      nsRefPtr<DeviceStorageFile> dsf =
        new DeviceStorageFile(p.type(), p.storageName(), p.rootDir(), p.relpath());
      nsRefPtr<CancelableRunnable> r = new ReadFileEvent(this, dsf);

      nsCOMPtr<nsIEventTarget> target = do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID);
      NS_ASSERTION(target, "Must have stream transport service");
      target->Dispatch(r, NS_DISPATCH_NORMAL);
      break;
    }

    case DeviceStorageParams::TDeviceStorageDeleteParams:
    {
      DeviceStorageDeleteParams p = mParams;

      nsRefPtr<DeviceStorageFile> dsf =
        new DeviceStorageFile(p.type(), p.storageName(), p.relpath());
      nsRefPtr<CancelableRunnable> r = new DeleteFileEvent(this, dsf);

      nsCOMPtr<nsIEventTarget> target = do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID);
      NS_ASSERTION(target, "Must have stream transport service");
      target->Dispatch(r, NS_DISPATCH_NORMAL);
      break;
    }

    case DeviceStorageParams::TDeviceStorageFreeSpaceParams:
    {
      DeviceStorageFreeSpaceParams p = mParams;

      nsRefPtr<DeviceStorageFile> dsf =
        new DeviceStorageFile(p.type(), p.storageName());
      nsRefPtr<FreeSpaceFileEvent> r = new FreeSpaceFileEvent(this, dsf);

      nsCOMPtr<nsIEventTarget> target = do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID);
      NS_ASSERTION(target, "Must have stream transport service");
      target->Dispatch(r, NS_DISPATCH_NORMAL);
      break;
    }

    case DeviceStorageParams::TDeviceStorageUsedSpaceParams:
    {
      DeviceStorageUsedSpaceCache* usedSpaceCache = DeviceStorageUsedSpaceCache::CreateOrGet();
      NS_ASSERTION(usedSpaceCache, "DeviceStorageUsedSpaceCache is null");

      DeviceStorageUsedSpaceParams p = mParams;

      nsRefPtr<DeviceStorageFile> dsf =
        new DeviceStorageFile(p.type(), p.storageName());
      nsRefPtr<UsedSpaceFileEvent> r = new UsedSpaceFileEvent(this, dsf);

      usedSpaceCache->Dispatch(r);
      break;
    }

    case DeviceStorageParams::TDeviceStorageAvailableParams:
    {
      DeviceStorageAvailableParams p = mParams;

      nsRefPtr<DeviceStorageFile> dsf =
        new DeviceStorageFile(p.type(), p.storageName());
      nsRefPtr<PostAvailableResultEvent> r = new PostAvailableResultEvent(this, dsf);
      NS_DispatchToMainThread(r);
      break;
    }

    case DeviceStorageParams::TDeviceStorageEnumerationParams:
    {
      DeviceStorageEnumerationParams p = mParams;
      nsRefPtr<DeviceStorageFile> dsf
        = new DeviceStorageFile(p.type(), p.storageName(), p.rootdir(), NS_LITERAL_STRING(""));
      nsRefPtr<CancelableRunnable> r = new EnumerateFileEvent(this, dsf, p.since());

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

bool
DeviceStorageRequestParent::EnsureRequiredPermissions(mozilla::dom::ContentParent* aParent)
{
  if (mozilla::Preferences::GetBool("device.storage.testing", false)) {
    return true;
  }

  nsString type;
  DeviceStorageRequestType requestType;

  switch (mParams.type())
  {
    case DeviceStorageParams::TDeviceStorageAddParams:
    {
      DeviceStorageAddParams p = mParams;
      type = p.type();
      requestType = DEVICE_STORAGE_REQUEST_CREATE;
      break;
    }

    case DeviceStorageParams::TDeviceStorageGetParams:
    {
      DeviceStorageGetParams p = mParams;
      type = p.type();
      requestType = DEVICE_STORAGE_REQUEST_READ;
      break;
    }

    case DeviceStorageParams::TDeviceStorageDeleteParams:
    {
      DeviceStorageDeleteParams p = mParams;
      type = p.type();
      requestType = DEVICE_STORAGE_REQUEST_DELETE;
      break;
    }

    case DeviceStorageParams::TDeviceStorageFreeSpaceParams:
    {
      DeviceStorageFreeSpaceParams p = mParams;
      type = p.type();
      requestType = DEVICE_STORAGE_REQUEST_FREE_SPACE;
      break;
    }

    case DeviceStorageParams::TDeviceStorageUsedSpaceParams:
    {
      DeviceStorageUsedSpaceParams p = mParams;
      type = p.type();
      requestType = DEVICE_STORAGE_REQUEST_FREE_SPACE;
      break;
    }

    case DeviceStorageParams::TDeviceStorageAvailableParams:
    {
      DeviceStorageAvailableParams p = mParams;
      type = p.type();
      requestType = DEVICE_STORAGE_REQUEST_AVAILABLE;
      break;
    }

    case DeviceStorageParams::TDeviceStorageEnumerationParams:
    {
      DeviceStorageEnumerationParams p = mParams;
      type = p.type();
      requestType = DEVICE_STORAGE_REQUEST_READ;
      break;
    }

    default:
    {
      return false;
    }
  }

  // The 'apps' type is special.  We only want this exposed
  // if the caller has the "webapps-manage" permission.
  if (type.EqualsLiteral("apps")) {
    if (!AssertAppProcessPermission(aParent, "webapps-manage")) {
      return false;
    }
  }

  nsAutoCString permissionName;
  nsresult rv = DeviceStorageTypeChecker::GetPermissionForType(type, permissionName);
  if (NS_FAILED(rv)) {
    return false;
  }

  nsCString access;
  rv = DeviceStorageTypeChecker::GetAccessForRequest(requestType, access);
  if (NS_FAILED(rv)) {
    return false;
  }

  permissionName.AppendLiteral("-");
  permissionName.Append(access);

  if (!AssertAppProcessPermission(aParent, permissionName.get())) {
    return false;
  }

  return true;
}

DeviceStorageRequestParent::~DeviceStorageRequestParent()
{
  MOZ_COUNT_DTOR(DeviceStorageRequestParent);
}

NS_IMPL_ADDREF(DeviceStorageRequestParent)
NS_IMPL_RELEASE(DeviceStorageRequestParent)

void
DeviceStorageRequestParent::ActorDestroy(ActorDestroyReason)
{
  MutexAutoLock lock(mMutex);
  mActorDestoryed = true;
  int32_t count = mRunnables.Length();
  for (int32_t index = 0; index < count; index++) {
    mRunnables[index]->Cancel();
  }
}

DeviceStorageRequestParent::PostFreeSpaceResultEvent::PostFreeSpaceResultEvent(DeviceStorageRequestParent* aParent,
                                                                               uint64_t aFreeSpace)
  : CancelableRunnable(aParent)
  , mFreeSpace(aFreeSpace)
{
}

DeviceStorageRequestParent::PostFreeSpaceResultEvent::~PostFreeSpaceResultEvent() {}

nsresult
DeviceStorageRequestParent::PostFreeSpaceResultEvent::CancelableRun() {
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  FreeSpaceStorageResponse response(mFreeSpace);
  unused << mParent->Send__delete__(mParent, response);
  return NS_OK;
}

DeviceStorageRequestParent::PostUsedSpaceResultEvent::PostUsedSpaceResultEvent(DeviceStorageRequestParent* aParent,
                                                                               const nsAString& aType,
                                                                               uint64_t aUsedSpace)
  : CancelableRunnable(aParent)
  , mType(aType)
  , mUsedSpace(aUsedSpace)
{
}

DeviceStorageRequestParent::PostUsedSpaceResultEvent::~PostUsedSpaceResultEvent() {}

nsresult
DeviceStorageRequestParent::PostUsedSpaceResultEvent::CancelableRun() {
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  UsedSpaceStorageResponse response(mUsedSpace);
  unused << mParent->Send__delete__(mParent, response);
  return NS_OK;
}

DeviceStorageRequestParent::PostErrorEvent::PostErrorEvent(DeviceStorageRequestParent* aParent,
                                                           const char* aError)
  : CancelableRunnable(aParent)
{
  CopyASCIItoUTF16(aError, mError);
}

DeviceStorageRequestParent::PostErrorEvent::~PostErrorEvent() {}

nsresult
DeviceStorageRequestParent::PostErrorEvent::CancelableRun() {
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  ErrorResponse response(mError);
  unused << mParent->Send__delete__(mParent, response);
  return NS_OK;
}

DeviceStorageRequestParent::PostSuccessEvent::PostSuccessEvent(DeviceStorageRequestParent* aParent)
  : CancelableRunnable(aParent)
{
}

DeviceStorageRequestParent::PostSuccessEvent::~PostSuccessEvent() {}

nsresult
DeviceStorageRequestParent::PostSuccessEvent::CancelableRun() {
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  SuccessResponse response;
  unused << mParent->Send__delete__(mParent, response);
  return NS_OK;
}

DeviceStorageRequestParent::PostBlobSuccessEvent::PostBlobSuccessEvent(DeviceStorageRequestParent* aParent,
                                                                       DeviceStorageFile* aFile,
                                                                       uint32_t aLength,
                                                                       nsACString& aMimeType,
                                                                       uint64_t aLastModifiedDate)
  : CancelableRunnable(aParent)
  , mLength(aLength)
  , mLastModificationDate(aLastModifiedDate)
  , mFile(aFile)
  , mMimeType(aMimeType)
{
}

DeviceStorageRequestParent::PostBlobSuccessEvent::~PostBlobSuccessEvent() {}

nsresult
DeviceStorageRequestParent::PostBlobSuccessEvent::CancelableRun() {
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  nsString mime;
  CopyASCIItoUTF16(mMimeType, mime);

  nsString fullPath;
  mFile->GetFullPath(fullPath);
  nsCOMPtr<nsIDOMBlob> blob = new nsDOMFileFile(fullPath, mime, mLength, mFile->mFile, mLastModificationDate);

  ContentParent* cp = static_cast<ContentParent*>(mParent->Manager());
  BlobParent* actor = cp->GetOrCreateActorForBlob(blob);
  if (!actor) {
    ErrorResponse response(NS_LITERAL_STRING(POST_ERROR_EVENT_UNKNOWN));
    unused << mParent->Send__delete__(mParent, response);
    return NS_OK;
  }

  BlobResponse response;
  response.blobParent() = actor;

  unused << mParent->Send__delete__(mParent, response);
  return NS_OK;
}

DeviceStorageRequestParent::PostEnumerationSuccessEvent::PostEnumerationSuccessEvent(DeviceStorageRequestParent* aParent,
                                                                                     const nsAString& aStorageType,
                                                                                     const nsAString& aRelPath,
                                                                                     InfallibleTArray<DeviceStorageFileValue>& aPaths)
  : CancelableRunnable(aParent)
  , mStorageType(aStorageType)
  , mRelPath(aRelPath)
  , mPaths(aPaths)
{
}

DeviceStorageRequestParent::PostEnumerationSuccessEvent::~PostEnumerationSuccessEvent() {}

nsresult
DeviceStorageRequestParent::PostEnumerationSuccessEvent::CancelableRun() {
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  EnumerationResponse response(mStorageType, mRelPath, mPaths);
  unused << mParent->Send__delete__(mParent, response);
  return NS_OK;
}

DeviceStorageRequestParent::WriteFileEvent::WriteFileEvent(DeviceStorageRequestParent* aParent,
                                                           DeviceStorageFile* aFile,
                                                           nsIInputStream* aInputStream)
  : CancelableRunnable(aParent)
  , mFile(aFile)
  , mInputStream(aInputStream)
{
}

DeviceStorageRequestParent::WriteFileEvent::~WriteFileEvent()
{
}

nsresult
DeviceStorageRequestParent::WriteFileEvent::CancelableRun()
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

  nsRefPtr<nsRunnable> r;

  if (!mInputStream) {
    r = new PostErrorEvent(mParent, POST_ERROR_EVENT_UNKNOWN);
    NS_DispatchToMainThread(r);
    return NS_OK;
  }

  bool check = false;
  mFile->mFile->Exists(&check);
  if (check) {
    nsCOMPtr<PostErrorEvent> event = new PostErrorEvent(mParent, POST_ERROR_EVENT_FILE_EXISTS);
    NS_DispatchToMainThread(event);
    return NS_OK;
  }

  nsresult rv = mFile->Write(mInputStream);

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
  : CancelableRunnable(aParent)
  , mFile(aFile)
{
}

DeviceStorageRequestParent::DeleteFileEvent::~DeleteFileEvent()
{
}

nsresult
DeviceStorageRequestParent::DeleteFileEvent::CancelableRun()
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

  mFile->Remove();

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

DeviceStorageRequestParent::FreeSpaceFileEvent::FreeSpaceFileEvent(DeviceStorageRequestParent* aParent,
                                                                   DeviceStorageFile* aFile)
  : CancelableRunnable(aParent)
  , mFile(aFile)
{
}

DeviceStorageRequestParent::FreeSpaceFileEvent::~FreeSpaceFileEvent()
{
}

nsresult
DeviceStorageRequestParent::FreeSpaceFileEvent::CancelableRun()
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

  int64_t freeSpace = 0;
  if (mFile) {
    mFile->GetDiskFreeSpace(&freeSpace);
  }

  nsCOMPtr<nsIRunnable> r;
  r = new PostFreeSpaceResultEvent(mParent, static_cast<uint64_t>(freeSpace));
  NS_DispatchToMainThread(r);
  return NS_OK;
}

DeviceStorageRequestParent::UsedSpaceFileEvent::UsedSpaceFileEvent(DeviceStorageRequestParent* aParent,
                                                                   DeviceStorageFile* aFile)
  : CancelableRunnable(aParent)
  , mFile(aFile)
{
}

DeviceStorageRequestParent::UsedSpaceFileEvent::~UsedSpaceFileEvent()
{
}

nsresult
DeviceStorageRequestParent::UsedSpaceFileEvent::CancelableRun()
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

  uint64_t picturesUsage = 0, videosUsage = 0, musicUsage = 0, totalUsage = 0;
  mFile->AccumDiskUsage(&picturesUsage, &videosUsage,
                        &musicUsage, &totalUsage);
  nsCOMPtr<nsIRunnable> r;
  if (mFile->mStorageType.EqualsLiteral(DEVICESTORAGE_PICTURES)) {
    r = new PostUsedSpaceResultEvent(mParent, mFile->mStorageType, picturesUsage);
  }
  else if (mFile->mStorageType.EqualsLiteral(DEVICESTORAGE_VIDEOS)) {
    r = new PostUsedSpaceResultEvent(mParent, mFile->mStorageType, videosUsage);
  }
  else if (mFile->mStorageType.EqualsLiteral(DEVICESTORAGE_MUSIC)) {
    r = new PostUsedSpaceResultEvent(mParent, mFile->mStorageType, musicUsage);
  } else {
    r = new PostUsedSpaceResultEvent(mParent, mFile->mStorageType, totalUsage);
  }
  NS_DispatchToMainThread(r);
  return NS_OK;
}

DeviceStorageRequestParent::ReadFileEvent::ReadFileEvent(DeviceStorageRequestParent* aParent,
                                                         DeviceStorageFile* aFile)
  : CancelableRunnable(aParent)
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

nsresult
DeviceStorageRequestParent::ReadFileEvent::CancelableRun()
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

  nsCOMPtr<nsIRunnable> r;
  bool check = false;
  mFile->mFile->Exists(&check);

  if (!check) {
    r = new PostErrorEvent(mParent, POST_ERROR_EVENT_FILE_DOES_NOT_EXIST);
    NS_DispatchToMainThread(r);
    return NS_OK;
  }

  int64_t fileSize;
  nsresult rv = mFile->mFile->GetFileSize(&fileSize);
  if (NS_FAILED(rv)) {
    r = new PostErrorEvent(mParent, POST_ERROR_EVENT_UNKNOWN);
    NS_DispatchToMainThread(r);
    return NS_OK;
  }

  PRTime modDate;
  rv = mFile->mFile->GetLastModifiedTime(&modDate);
  if (NS_FAILED(rv)) {
    r = new PostErrorEvent(mParent, POST_ERROR_EVENT_UNKNOWN);
    NS_DispatchToMainThread(r);
    return NS_OK;
  }

  r = new PostBlobSuccessEvent(mParent, mFile, static_cast<uint64_t>(fileSize), mMimeType, modDate);
  NS_DispatchToMainThread(r);
  return NS_OK;
}

DeviceStorageRequestParent::EnumerateFileEvent::EnumerateFileEvent(DeviceStorageRequestParent* aParent,
                                                                   DeviceStorageFile* aFile,
                                                                   uint64_t aSince)
  : CancelableRunnable(aParent)
  , mFile(aFile)
  , mSince(aSince)
{
}

DeviceStorageRequestParent::EnumerateFileEvent::~EnumerateFileEvent()
{
}

nsresult
DeviceStorageRequestParent::EnumerateFileEvent::CancelableRun()
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

  nsCOMPtr<nsIRunnable> r;
  if (mFile->mFile) {
    bool check = false;
    mFile->mFile->Exists(&check);
    if (!check) {
      r = new PostErrorEvent(mParent, POST_ERROR_EVENT_FILE_DOES_NOT_EXIST);
      NS_DispatchToMainThread(r);
      return NS_OK;
    }
  }

  nsTArray<nsRefPtr<DeviceStorageFile> > files;
  mFile->CollectFiles(files, mSince);

  InfallibleTArray<DeviceStorageFileValue> values;

  uint32_t count = files.Length();
  for (uint32_t i = 0; i < count; i++) {
    DeviceStorageFileValue dsvf(files[i]->mStorageName, files[i]->mPath);
    values.AppendElement(dsvf);
  }

  r = new PostEnumerationSuccessEvent(mParent, mFile->mStorageType, mFile->mRootDir, values);
  NS_DispatchToMainThread(r);
  return NS_OK;
}


DeviceStorageRequestParent::PostPathResultEvent::PostPathResultEvent(DeviceStorageRequestParent* aParent,
                                                                     const nsAString& aPath)
  : CancelableRunnable(aParent)
  , mPath(aPath)
{
}

DeviceStorageRequestParent::PostPathResultEvent::~PostPathResultEvent()
{
}

nsresult
DeviceStorageRequestParent::PostPathResultEvent::CancelableRun()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  SuccessResponse response;
  unused << mParent->Send__delete__(mParent, response);
  return NS_OK;
}

DeviceStorageRequestParent::PostAvailableResultEvent::PostAvailableResultEvent(DeviceStorageRequestParent* aParent,
                                                                               DeviceStorageFile* aFile)
  : CancelableRunnable(aParent)
  , mFile(aFile)
{
}

DeviceStorageRequestParent::PostAvailableResultEvent::~PostAvailableResultEvent()
{
}

nsresult
DeviceStorageRequestParent::PostAvailableResultEvent::CancelableRun()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  nsString state = NS_LITERAL_STRING("unavailable");
  if (mFile) {
    mFile->GetStatus(state);
  }

  AvailableStorageResponse response(state);
  unused << mParent->Send__delete__(mParent, response);
  return NS_OK;
}

} // namespace devicestorage
} // namespace dom
} // namespace mozilla
