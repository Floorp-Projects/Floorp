/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DeviceStorageRequestParent.h"
#include "nsIMIMEService.h"
#include "nsCExternalHandlerService.h"
#include "mozilla/unused.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/ipc/BlobParent.h"
#include "ContentParent.h"
#include "nsProxyRelease.h"
#include "AppProcessChecker.h"
#include "mozilla/Preferences.h"
#include "nsNetCID.h"

namespace mozilla {
namespace dom {
namespace devicestorage {

DeviceStorageRequestParent::DeviceStorageRequestParent(
  const DeviceStorageParams& aParams)
  : mParams(aParams)
  , mMutex("DeviceStorageRequestParent::mMutex")
  , mActorDestroyed(false)
{
  MOZ_COUNT_CTOR(DeviceStorageRequestParent);

  DebugOnly<DeviceStorageUsedSpaceCache*> usedSpaceCache
    = DeviceStorageUsedSpaceCache::CreateOrGet();
  MOZ_ASSERT(usedSpaceCache);
}

void
DeviceStorageRequestParent::Dispatch()
{
  switch (mParams.type()) {
    case DeviceStorageParams::TDeviceStorageAddParams:
    {
      DeviceStorageAddParams p = mParams;

      RefPtr<DeviceStorageFile> dsf =
        new DeviceStorageFile(p.type(), p.storageName(), p.relpath());

      BlobParent* bp = static_cast<BlobParent*>(p.blobParent());
      RefPtr<BlobImpl> blobImpl = bp->GetBlobImpl();

      ErrorResult rv;
      nsCOMPtr<nsIInputStream> stream;
      blobImpl->GetInternalStream(getter_AddRefs(stream), rv);
      MOZ_ASSERT(!rv.Failed());

      RefPtr<CancelableRunnable> r = new WriteFileEvent(this, dsf.forget(), stream,
                                                        DEVICE_STORAGE_REQUEST_CREATE);

      nsCOMPtr<nsIEventTarget> target
        = do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID);
      MOZ_ASSERT(target);
      target->Dispatch(r, NS_DISPATCH_NORMAL);
      break;
    }

    case DeviceStorageParams::TDeviceStorageAppendParams:
    {
      DeviceStorageAppendParams p = mParams;

      RefPtr<DeviceStorageFile> dsf =
        new DeviceStorageFile(p.type(), p.storageName(), p.relpath());

      BlobParent* bp = static_cast<BlobParent*>(p.blobParent());
      RefPtr<BlobImpl> blobImpl = bp->GetBlobImpl();

      ErrorResult rv;
      nsCOMPtr<nsIInputStream> stream;
      blobImpl->GetInternalStream(getter_AddRefs(stream), rv);
      MOZ_ASSERT(!rv.Failed());

      RefPtr<CancelableRunnable> r = new WriteFileEvent(this, dsf.forget(), stream,
                                                        DEVICE_STORAGE_REQUEST_APPEND);

      nsCOMPtr<nsIEventTarget> target
        = do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID);
      MOZ_ASSERT(target);
      target->Dispatch(r, NS_DISPATCH_NORMAL);
      break;
    }

    case DeviceStorageParams::TDeviceStorageCreateFdParams:
    {
      DeviceStorageCreateFdParams p = mParams;

      RefPtr<DeviceStorageFile> dsf =
        new DeviceStorageFile(p.type(), p.storageName(), p.relpath());

      RefPtr<CancelableRunnable> r = new CreateFdEvent(this, dsf.forget());

      nsCOMPtr<nsIEventTarget> target
        = do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID);
      MOZ_ASSERT(target);
      target->Dispatch(r, NS_DISPATCH_NORMAL);
      break;
    }

    case DeviceStorageParams::TDeviceStorageGetParams:
    {
      DeviceStorageGetParams p = mParams;
      RefPtr<DeviceStorageFile> dsf =
        new DeviceStorageFile(p.type(), p.storageName(),
                              p.rootDir(), p.relpath());
      RefPtr<CancelableRunnable> r = new ReadFileEvent(this, dsf.forget());

      nsCOMPtr<nsIEventTarget> target
        = do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID);
      MOZ_ASSERT(target);
      target->Dispatch(r, NS_DISPATCH_NORMAL);
      break;
    }

    case DeviceStorageParams::TDeviceStorageDeleteParams:
    {
      DeviceStorageDeleteParams p = mParams;

      RefPtr<DeviceStorageFile> dsf =
        new DeviceStorageFile(p.type(), p.storageName(), p.relpath());
      RefPtr<CancelableRunnable> r = new DeleteFileEvent(this, dsf.forget());

      nsCOMPtr<nsIEventTarget> target
        = do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID);
      MOZ_ASSERT(target);
      target->Dispatch(r, NS_DISPATCH_NORMAL);
      break;
    }

    case DeviceStorageParams::TDeviceStorageFreeSpaceParams:
    {
      DeviceStorageFreeSpaceParams p = mParams;

      RefPtr<DeviceStorageFile> dsf =
        new DeviceStorageFile(p.type(), p.storageName());
      RefPtr<FreeSpaceFileEvent> r = new FreeSpaceFileEvent(this, dsf.forget());

      nsCOMPtr<nsIEventTarget> target
        = do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID);
      MOZ_ASSERT(target);
      target->Dispatch(r, NS_DISPATCH_NORMAL);
      break;
    }

    case DeviceStorageParams::TDeviceStorageUsedSpaceParams:
    {
      DeviceStorageUsedSpaceCache* usedSpaceCache
        = DeviceStorageUsedSpaceCache::CreateOrGet();
      MOZ_ASSERT(usedSpaceCache);

      DeviceStorageUsedSpaceParams p = mParams;

      RefPtr<DeviceStorageFile> dsf =
        new DeviceStorageFile(p.type(), p.storageName());
      RefPtr<UsedSpaceFileEvent> r = new UsedSpaceFileEvent(this, dsf.forget());

      usedSpaceCache->Dispatch(r);
      break;
    }

    case DeviceStorageParams::TDeviceStorageFormatParams:
    {
      DeviceStorageFormatParams p = mParams;

      RefPtr<DeviceStorageFile> dsf =
        new DeviceStorageFile(p.type(), p.storageName());
      RefPtr<PostFormatResultEvent> r
        = new PostFormatResultEvent(this, dsf.forget());
      DebugOnly<nsresult> rv = NS_DispatchToMainThread(r);
      MOZ_ASSERT(NS_SUCCEEDED(rv));
      break;
    }

    case DeviceStorageParams::TDeviceStorageMountParams:
    {
      DeviceStorageMountParams p = mParams;

      RefPtr<DeviceStorageFile> dsf =
        new DeviceStorageFile(p.type(), p.storageName());
      RefPtr<PostMountResultEvent> r
        = new PostMountResultEvent(this, dsf.forget());
      DebugOnly<nsresult> rv = NS_DispatchToMainThread(r);
      MOZ_ASSERT(NS_SUCCEEDED(rv));
      break;
    }

    case DeviceStorageParams::TDeviceStorageUnmountParams:
    {
      DeviceStorageUnmountParams p = mParams;

      RefPtr<DeviceStorageFile> dsf =
        new DeviceStorageFile(p.type(), p.storageName());
      RefPtr<PostUnmountResultEvent> r
        = new PostUnmountResultEvent(this, dsf.forget());
      DebugOnly<nsresult> rv = NS_DispatchToMainThread(r);
      MOZ_ASSERT(NS_SUCCEEDED(rv));
      break;
    }

    case DeviceStorageParams::TDeviceStorageEnumerationParams:
    {
      DeviceStorageEnumerationParams p = mParams;
      RefPtr<DeviceStorageFile> dsf
        = new DeviceStorageFile(p.type(), p.storageName(),
                                p.rootdir(), NS_LITERAL_STRING(""));
      RefPtr<CancelableRunnable> r
        = new EnumerateFileEvent(this, dsf.forget(), p.since());

      nsCOMPtr<nsIEventTarget> target
        = do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID);
      MOZ_ASSERT(target);
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
DeviceStorageRequestParent::EnsureRequiredPermissions(
  mozilla::dom::ContentParent* aParent)
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

    case DeviceStorageParams::TDeviceStorageAppendParams:
    {
      DeviceStorageAppendParams p = mParams;
      type = p.type();
      requestType = DEVICE_STORAGE_REQUEST_APPEND;
      break;
    }

    case DeviceStorageParams::TDeviceStorageCreateFdParams:
    {
      DeviceStorageCreateFdParams p = mParams;
      type = p.type();
      requestType = DEVICE_STORAGE_REQUEST_CREATEFD;
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

    case DeviceStorageParams::TDeviceStorageStatusParams:
    {
      DeviceStorageStatusParams p = mParams;
      type = p.type();
      requestType = DEVICE_STORAGE_REQUEST_STATUS;
      break;
    }

    case DeviceStorageParams::TDeviceStorageFormatParams:
    {
      DeviceStorageFormatParams p = mParams;
      type = p.type();
      requestType = DEVICE_STORAGE_REQUEST_FORMAT;
      break;
    }

    case DeviceStorageParams::TDeviceStorageMountParams:
    {
      DeviceStorageMountParams p = mParams;
      type = p.type();
      requestType = DEVICE_STORAGE_REQUEST_MOUNT;
      break;
    }

    case DeviceStorageParams::TDeviceStorageUnmountParams:
    {
      DeviceStorageUnmountParams p = mParams;
      type = p.type();
      requestType = DEVICE_STORAGE_REQUEST_UNMOUNT;
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
  nsresult rv = DeviceStorageTypeChecker::GetPermissionForType(type,
                                                               permissionName);
  if (NS_FAILED(rv)) {
    return false;
  }

  nsCString access;
  rv = DeviceStorageTypeChecker::GetAccessForRequest(requestType, access);
  if (NS_FAILED(rv)) {
    return false;
  }

  permissionName.Append('-');
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
  mActorDestroyed = true;
  for (auto& runnable : mRunnables) {
    runnable->Cancel();
  }
  // Ensure we clear all references to the runnables so that there won't
  // be leak due to cyclic reference. Note that it is safe to release
  // the references here, since if a runnable is not cancelled yet, the
  // corresponding thread should still hold a reference to it, and thus
  // the runnable will end up being released in that thread, not here.
  mRunnables.Clear();
}

DeviceStorageRequestParent::PostFreeSpaceResultEvent::PostFreeSpaceResultEvent(
  DeviceStorageRequestParent* aParent,
  uint64_t aFreeSpace)
  : CancelableRunnable(aParent)
  , mFreeSpace(aFreeSpace)
{
}

DeviceStorageRequestParent::PostFreeSpaceResultEvent::
  ~PostFreeSpaceResultEvent() {}

nsresult
DeviceStorageRequestParent::PostFreeSpaceResultEvent::CancelableRun() {
  MOZ_ASSERT(NS_IsMainThread());

  FreeSpaceStorageResponse response(mFreeSpace);
  Unused << mParent->Send__delete__(mParent, response);
  return NS_OK;
}

nsresult
DeviceStorageRequestParent::PostUsedSpaceResultEvent::CancelableRun() {
  MOZ_ASSERT(NS_IsMainThread());

  UsedSpaceStorageResponse response(mUsedSpace);
  Unused << mParent->Send__delete__(mParent, response);
  return NS_OK;
}

DeviceStorageRequestParent::PostErrorEvent::
  PostErrorEvent(DeviceStorageRequestParent* aParent, const char* aError)
  : CancelableRunnable(aParent)
{
  CopyASCIItoUTF16(aError, mError);
}

nsresult
DeviceStorageRequestParent::PostErrorEvent::CancelableRun() {
  MOZ_ASSERT(NS_IsMainThread());

  ErrorResponse response(mError);
  Unused << mParent->Send__delete__(mParent, response);
  return NS_OK;
}

nsresult
DeviceStorageRequestParent::PostSuccessEvent::CancelableRun() {
  MOZ_ASSERT(NS_IsMainThread());

  SuccessResponse response;
  Unused << mParent->Send__delete__(mParent, response);
  return NS_OK;
}

nsresult
DeviceStorageRequestParent::PostBlobSuccessEvent::CancelableRun() {
  MOZ_ASSERT(NS_IsMainThread());

  nsString mime;
  CopyASCIItoUTF16(mMimeType, mime);

  nsString fullPath;
  mFile->GetFullPath(fullPath);
  RefPtr<BlobImpl> blob =
    new BlobImplFile(fullPath, mime, mLength, mFile->mFile,
                     mLastModificationDate);

  ContentParent* cp = static_cast<ContentParent*>(mParent->Manager());
  BlobParent* actor = cp->GetOrCreateActorForBlobImpl(blob);
  if (!actor) {
    ErrorResponse response(NS_LITERAL_STRING(POST_ERROR_EVENT_UNKNOWN));
    Unused << mParent->Send__delete__(mParent, response);
    return NS_OK;
  }

  BlobResponse response;
  response.blobParent() = actor;

  Unused << mParent->Send__delete__(mParent, response);
  return NS_OK;
}

nsresult
DeviceStorageRequestParent::PostEnumerationSuccessEvent::CancelableRun() {
  MOZ_ASSERT(NS_IsMainThread());

  EnumerationResponse response(mStorageType, mRelPath, mPaths);
  Unused << mParent->Send__delete__(mParent, response);
  return NS_OK;
}

nsresult
DeviceStorageRequestParent::CreateFdEvent::CancelableRun()
{
  MOZ_ASSERT(!NS_IsMainThread());

  nsCOMPtr<nsIRunnable> r;

  if (!mFile->mFile) {
    r = new PostErrorEvent(mParent, POST_ERROR_EVENT_UNKNOWN);
    return NS_DispatchToMainThread(r);
  }
  bool check = false;
  mFile->mFile->Exists(&check);
  if (check) {
    r = new PostErrorEvent(mParent, POST_ERROR_EVENT_FILE_EXISTS);
    return NS_DispatchToMainThread(r);
  }

  FileDescriptor fileDescriptor;
  nsresult rv = mFile->CreateFileDescriptor(fileDescriptor);
  if (NS_FAILED(rv)) {
    NS_WARNING("CreateFileDescriptor failed");
    mFile->Dump("CreateFileDescriptor failed");
    r = new PostErrorEvent(mParent, POST_ERROR_EVENT_UNKNOWN);
  }
  else {
    r = new PostFileDescriptorResultEvent(mParent, fileDescriptor);
  }

  return NS_DispatchToMainThread(r);
}

nsresult
DeviceStorageRequestParent::WriteFileEvent::CancelableRun()
{
  MOZ_ASSERT(!NS_IsMainThread());

  nsCOMPtr<nsIRunnable> r;

  if (!mInputStream || !mFile->mFile) {
    r = new PostErrorEvent(mParent, POST_ERROR_EVENT_UNKNOWN);
    return NS_DispatchToMainThread(r);
  }

  bool check = false;
  nsresult rv;
  mFile->mFile->Exists(&check);

  if (mRequestType == DEVICE_STORAGE_REQUEST_CREATE) {
    if (check) {
      r = new PostErrorEvent(mParent, POST_ERROR_EVENT_FILE_EXISTS);
      return NS_DispatchToMainThread(r);
    }
    rv = mFile->Write(mInputStream);
  } else if (mRequestType == DEVICE_STORAGE_REQUEST_APPEND) {
    if (!check) {
      r = new PostErrorEvent(mParent, POST_ERROR_EVENT_FILE_DOES_NOT_EXIST);
      return NS_DispatchToMainThread(r);
    }
    rv = mFile->Append(mInputStream);
  } else {
    r = new PostErrorEvent(mParent, POST_ERROR_EVENT_UNKNOWN);
    return NS_DispatchToMainThread(r);
  }

  if (NS_FAILED(rv)) {
    r = new PostErrorEvent(mParent, POST_ERROR_EVENT_UNKNOWN);
  }
  else {
    r = new PostPathResultEvent(mParent, mFile->mPath);
  }

  return NS_DispatchToMainThread(r);
}

nsresult
DeviceStorageRequestParent::DeleteFileEvent::CancelableRun()
{
  MOZ_ASSERT(!NS_IsMainThread());

  mFile->Remove();

  nsCOMPtr<nsIRunnable> r;

  if (!mFile->mFile) {
    r = new PostErrorEvent(mParent, POST_ERROR_EVENT_UNKNOWN);
    return NS_DispatchToMainThread(r);
  }
  bool check = false;
  mFile->mFile->Exists(&check);
  if (check) {
    r = new PostErrorEvent(mParent, POST_ERROR_EVENT_UNKNOWN);
  }
  else {
    r = new PostPathResultEvent(mParent, mFile->mPath);
  }

  return NS_DispatchToMainThread(r);
}

nsresult
DeviceStorageRequestParent::FreeSpaceFileEvent::CancelableRun()
{
  MOZ_ASSERT(!NS_IsMainThread());

  int64_t freeSpace = 0;
  if (mFile) {
    mFile->GetStorageFreeSpace(&freeSpace);
  }

  nsCOMPtr<nsIRunnable> r;
  r = new PostFreeSpaceResultEvent(mParent, static_cast<uint64_t>(freeSpace));
  return NS_DispatchToMainThread(r);
}

nsresult
DeviceStorageRequestParent::UsedSpaceFileEvent::CancelableRun()
{
  MOZ_ASSERT(!NS_IsMainThread());

  uint64_t picturesUsage = 0, videosUsage = 0, musicUsage = 0, totalUsage = 0;
  mFile->AccumDiskUsage(&picturesUsage, &videosUsage,
                        &musicUsage, &totalUsage);
  nsCOMPtr<nsIRunnable> r;
  if (mFile->mStorageType.EqualsLiteral(DEVICESTORAGE_PICTURES)) {
    r = new PostUsedSpaceResultEvent(mParent, mFile->mStorageType,
                                     picturesUsage);
  }
  else if (mFile->mStorageType.EqualsLiteral(DEVICESTORAGE_VIDEOS)) {
    r = new PostUsedSpaceResultEvent(mParent, mFile->mStorageType, videosUsage);
  }
  else if (mFile->mStorageType.EqualsLiteral(DEVICESTORAGE_MUSIC)) {
    r = new PostUsedSpaceResultEvent(mParent, mFile->mStorageType, musicUsage);
  } else {
    r = new PostUsedSpaceResultEvent(mParent, mFile->mStorageType, totalUsage);
  }
  return NS_DispatchToMainThread(r);
}

DeviceStorageRequestParent::ReadFileEvent::
  ReadFileEvent(DeviceStorageRequestParent* aParent,
                already_AddRefed<DeviceStorageFile>&& aFile)
  : CancelableFileEvent(aParent, Move(aFile))
{
  nsCOMPtr<nsIMIMEService> mimeService
    = do_GetService(NS_MIMESERVICE_CONTRACTID);
  if (mimeService) {
    nsresult rv = mimeService->GetTypeFromFile(mFile->mFile, mMimeType);
    if (NS_FAILED(rv)) {
      mMimeType.Truncate();
    }
  }
}

nsresult
DeviceStorageRequestParent::ReadFileEvent::CancelableRun()
{
  MOZ_ASSERT(!NS_IsMainThread());

  nsCOMPtr<nsIRunnable> r;

  if (!mFile->mFile) {
    r = new PostErrorEvent(mParent, POST_ERROR_EVENT_UNKNOWN);
    return NS_DispatchToMainThread(r);
  }
  bool check = false;
  mFile->mFile->Exists(&check);

  if (!check) {
    r = new PostErrorEvent(mParent, POST_ERROR_EVENT_FILE_DOES_NOT_EXIST);
    return NS_DispatchToMainThread(r);
  }

  int64_t fileSize;
  nsresult rv = mFile->mFile->GetFileSize(&fileSize);
  if (NS_FAILED(rv)) {
    r = new PostErrorEvent(mParent, POST_ERROR_EVENT_UNKNOWN);
    return NS_DispatchToMainThread(r);
  }

  PRTime modDate;
  rv = mFile->mFile->GetLastModifiedTime(&modDate);
  if (NS_FAILED(rv)) {
    r = new PostErrorEvent(mParent, POST_ERROR_EVENT_UNKNOWN);
    return NS_DispatchToMainThread(r);
  }

  r = new PostBlobSuccessEvent(mParent, mFile.forget(),
                               static_cast<uint64_t>(fileSize),
                               mMimeType, modDate);
  return NS_DispatchToMainThread(r);
}

nsresult
DeviceStorageRequestParent::EnumerateFileEvent::CancelableRun()
{
  MOZ_ASSERT(!NS_IsMainThread());

  nsCOMPtr<nsIRunnable> r;
  if (mFile->mFile) {
    bool check = false;
    mFile->mFile->Exists(&check);
    if (!check) {
      r = new PostErrorEvent(mParent, POST_ERROR_EVENT_FILE_DOES_NOT_EXIST);
      return NS_DispatchToMainThread(r);
    }
  }

  nsTArray<RefPtr<DeviceStorageFile> > files;
  mFile->CollectFiles(files, mSince);

  InfallibleTArray<DeviceStorageFileValue> values;

  uint32_t count = files.Length();
  for (uint32_t i = 0; i < count; i++) {
    DeviceStorageFileValue dsvf(files[i]->mStorageName, files[i]->mPath);
    values.AppendElement(dsvf);
  }

  r = new PostEnumerationSuccessEvent(mParent, mFile->mStorageType,
                                      mFile->mRootDir, values);
  return NS_DispatchToMainThread(r);
}

nsresult
DeviceStorageRequestParent::PostPathResultEvent::CancelableRun()
{
  MOZ_ASSERT(NS_IsMainThread());

  SuccessResponse response;
  Unused << mParent->Send__delete__(mParent, response);
  return NS_OK;
}

nsresult
DeviceStorageRequestParent::PostFileDescriptorResultEvent::CancelableRun()
{
  MOZ_ASSERT(NS_IsMainThread());

  FileDescriptorResponse response(mFileDescriptor);
  Unused << mParent->Send__delete__(mParent, response);
  return NS_OK;
}

nsresult
DeviceStorageRequestParent::PostFormatResultEvent::CancelableRun()
{
  MOZ_ASSERT(NS_IsMainThread());

  nsString state = NS_LITERAL_STRING("unavailable");
  if (mFile) {
    mFile->DoFormat(state);
  }

  FormatStorageResponse response(state);
  Unused << mParent->Send__delete__(mParent, response);
  return NS_OK;
}

nsresult
DeviceStorageRequestParent::PostMountResultEvent::CancelableRun()
{
  MOZ_ASSERT(NS_IsMainThread());

  nsString state = NS_LITERAL_STRING("unavailable");
  if (mFile) {
    mFile->DoMount(state);
  }

  MountStorageResponse response(state);
  Unused << mParent->Send__delete__(mParent, response);
  return NS_OK;
}

nsresult
DeviceStorageRequestParent::PostUnmountResultEvent::CancelableRun()
{
  MOZ_ASSERT(NS_IsMainThread());

  nsString state = NS_LITERAL_STRING("unavailable");
  if (mFile) {
    mFile->DoUnmount(state);
  }

  UnmountStorageResponse response(state);
  Unused << mParent->Send__delete__(mParent, response);
  return NS_OK;
}

} // namespace devicestorage
} // namespace dom
} // namespace mozilla
