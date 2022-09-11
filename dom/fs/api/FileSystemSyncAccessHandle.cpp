/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileSystemSyncAccessHandle.h"

#include "fs/FileSystemRequestHandler.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/FileSystemAccessHandleChild.h"
#include "mozilla/dom/FileSystemHandleBinding.h"
#include "mozilla/dom/FileSystemManager.h"
#include "mozilla/dom/FileSystemSyncAccessHandleBinding.h"
#include "mozilla/dom/Promise.h"
#include "private/pprio.h"

namespace mozilla {

LazyLogModule gOPFSLog("OPFS");

}
#define LOG(args) MOZ_LOG(mozilla::gOPFSLog, mozilla::LogLevel::Verbose, args)

#define LOG_DEBUG(args) \
  MOZ_LOG(mozilla::gOPFSLog, mozilla::LogLevel::Debug, args)

/**
 * TODO: Duplicated from netwerk/cache2/CacheFileIOManager.cpp
 * Please remove after bug 1286601 is fixed,
 * https://bugzilla.mozilla.org/show_bug.cgi?id=1286601
 */
static nsresult TruncFile(PRFileDesc* aFD, int64_t aEOF) {
#if defined(XP_UNIX)
  if (ftruncate(PR_FileDesc2NativeHandle(aFD), aEOF) != 0) {
    NS_ERROR("ftruncate failed");
    return NS_ERROR_FAILURE;
  }
#elif defined(XP_WIN)
  int64_t cnt = PR_Seek64(aFD, aEOF, PR_SEEK_SET);
  if (cnt == -1) {
    return NS_ERROR_FAILURE;
  }
  if (!SetEndOfFile((HANDLE)PR_FileDesc2NativeHandle(aFD))) {
    NS_ERROR("SetEndOfFile failed");
    return NS_ERROR_FAILURE;
  }
#else
  MOZ_ASSERT(false, "Not implemented!");
  return NS_ERROR_NOT_IMPLEMENTED;
#endif

  return NS_OK;
}

namespace mozilla::dom {

FileSystemSyncAccessHandle::FileSystemSyncAccessHandle(
    nsIGlobalObject* aGlobal, RefPtr<FileSystemManager>& aManager,
    RefPtr<FileSystemAccessHandleChild> aActor,
    const fs::FileSystemEntryMetadata& aMetadata,
    fs::FileSystemRequestHandler* aRequestHandler)
    : mGlobal(aGlobal),
      mManager(aManager),
      mActor(std::move(aActor)),
      mMetadata(aMetadata),
      mRequestHandler(aRequestHandler) {}

FileSystemSyncAccessHandle::FileSystemSyncAccessHandle(
    nsIGlobalObject* aGlobal, RefPtr<FileSystemManager>& aManager,
    RefPtr<FileSystemAccessHandleChild> aActor,
    const fs::FileSystemEntryMetadata& aMetadata)
    : FileSystemSyncAccessHandle(aGlobal, aManager, std::move(aActor),
                                 aMetadata,
                                 new fs::FileSystemRequestHandler()) {
  LOG(("Created SyncAccessHandle %p for fd %p", this,
       mActor->MutableFileDescPtr()));
}

FileSystemSyncAccessHandle::~FileSystemSyncAccessHandle() {
  if (mActor) {
    mActor->Close();
  }
}

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(FileSystemSyncAccessHandle)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(FileSystemSyncAccessHandle)
NS_IMPL_CYCLE_COLLECTING_RELEASE(FileSystemSyncAccessHandle)

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(FileSystemSyncAccessHandle)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(FileSystemSyncAccessHandle)
  // Don't unlink mManager!
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(FileSystemSyncAccessHandle)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mGlobal)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mManager)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

void FileSystemSyncAccessHandle::ClearActor() {
  MOZ_ASSERT(mActor);

  mActor = nullptr;
}

// WebIDL Boilerplate

nsIGlobalObject* FileSystemSyncAccessHandle::GetParentObject() const {
  return mGlobal;
}

JSObject* FileSystemSyncAccessHandle::WrapObject(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return FileSystemSyncAccessHandle_Binding::Wrap(aCx, this, aGivenProto);
}

// WebIDL Interface

uint64_t FileSystemSyncAccessHandle::Read(
    const MaybeSharedArrayBufferViewOrMaybeSharedArrayBuffer& aBuffer,
    const FileSystemReadWriteOptions& aOptions, ErrorResult& aRv) {
  if (!mActor) {
    aRv.ThrowInvalidStateError("SyncAccessHandle is closed");
    return 0;
  }

  PRFileDesc* fileDesc = mActor->MutableFileDescPtr();

  // read directly from filehandle, blocking

  // Handle seek before read ('at')
  if (aOptions.mAt.WasPassed()) {
    uint64_t at = aOptions.mAt.Value();
    LOG(("%p: Seeking to %" PRIu64, fileDesc, at));
    int64_t where = PR_Seek64(fileDesc, (PROffset64)at, PR_SEEK_SET);
    if (where == -1) {
      LOG(("Read at %" PRIu64 " failed to seek (errno %d)", at, errno));
      return 0;
    }
    if (where != (int64_t)at) {
      LOG(("Read at %" PRIu64 " failed to seek (%" PRId64 " instead)", at,
           where));
      return 0;
    }
  }

  uint8_t* data;
  size_t length;
  if (aBuffer.IsArrayBuffer()) {
    const ArrayBuffer& buffer = aBuffer.GetAsArrayBuffer();
    buffer.ComputeState();
    data = buffer.Data();
    length = buffer.Length();
  } else if (aBuffer.IsArrayBufferView()) {
    const ArrayBufferView& buffer = aBuffer.GetAsArrayBufferView();
    buffer.ComputeState();
    data = buffer.Data();
    length = buffer.Length();
  } else {
    // really impossible
    LOG(("Impossible read source"));
    return 0;
  }
  // for read starting past the end of the file, return 0, which should happen
  // automatically
  LOG(("%p: Reading %zu bytes", fileDesc, length));
  // Unfortunately, PR_Read() is limited to int32
  uint64_t result = 0;
  while (length > 0) {
    PRInt32 iter_len = (length > PR_INT32_MAX) ? PR_INT32_MAX : length;
    PRInt32 temp = PR_Read(fileDesc, data, iter_len);
    if (temp == -1 || temp == 0 /* EOF*/) {
      return result;  // per spec, 2.6.1 #11
    }
    result += temp;
    length -= temp;
  }
  return result;
}

uint64_t FileSystemSyncAccessHandle::Write(
    const MaybeSharedArrayBufferViewOrMaybeSharedArrayBuffer& aBuffer,
    const FileSystemReadWriteOptions& aOptions, ErrorResult& aRv) {
  if (!mActor) {
    aRv.ThrowInvalidStateError("SyncAccessHandle is closed");
    return 0;
  }

  PRFileDesc* fileDesc = mActor->MutableFileDescPtr();

  // Write directly from filehandle, blocking

  // Handle seek before write ('at')
  if (aOptions.mAt.WasPassed()) {
    uint64_t at = aOptions.mAt.Value();
    LOG(("%p: Seeking to %" PRIu64, fileDesc, at));
    int64_t where = PR_Seek64(fileDesc, (PROffset64)at, PR_SEEK_SET);
    if (where == -1) {
      LOG(("Write at %" PRIu64 " failed to seek (errno %d)", at, errno));
      return 0;
    }
    if (where != (int64_t)at) {
      LOG(("Write at %" PRIu64 " failed to seek (%" PRId64 " instead)", at,
           where));
      return 0;
    }
  }

  // if we seek past the end of the file and write, it implicitly extends it
  // with 0's
  const uint8_t* data;
  size_t length;
  if (aBuffer.IsArrayBuffer()) {
    const ArrayBuffer& buffer = aBuffer.GetAsArrayBuffer();
    buffer.ComputeState();
    data = buffer.Data();
    length = buffer.Length();
  } else if (aBuffer.IsArrayBufferView()) {
    const ArrayBufferView& buffer = aBuffer.GetAsArrayBufferView();
    buffer.ComputeState();
    data = buffer.Data();
    length = buffer.Length();
  } else {
    LOG(("Impossible write source"));
    return 0;
  }
  LOG(("%p: Writing %zu bytes", fileDesc, length));
  // Unfortunately, PR_Write() is limited to int32
  uint64_t result = 0;
  while (length > 0) {
    PRInt32 iter_len = (length > PR_INT32_MAX) ? PR_INT32_MAX : length;
    PRInt32 temp = PR_Write(fileDesc, data, iter_len);
    if (temp == -1) {
      return result;  // per spec, 2.6.2 #13
    }
    result += temp;
    length -= temp;
  }
  return result;
}

already_AddRefed<Promise> FileSystemSyncAccessHandle::Truncate(
    uint64_t aSize, ErrorResult& aError) {
  RefPtr<Promise> promise = Promise::Create(GetParentObject(), aError);
  if (aError.Failed()) {
    return nullptr;
  }

  if (!mActor) {
    promise->MaybeReject(NS_ERROR_DOM_INVALID_STATE_ERR);
    return promise.forget();
  }

  PRFileDesc* fileDesc = mActor->MutableFileDescPtr();

  // truncate filehandle (can extend with 0's)
  LOG_DEBUG(("%p: Truncate to %" PRIu64, fileDesc, aSize));
  if (NS_WARN_IF(NS_FAILED(TruncFile(fileDesc, aSize)))) {
    promise->MaybeReject(NS_ErrorAccordingToNSPR());
  } else {
    promise->MaybeResolveWithUndefined();
  }

  return promise.forget();
}

already_AddRefed<Promise> FileSystemSyncAccessHandle::GetSize(
    ErrorResult& aError) {
  RefPtr<Promise> promise = Promise::Create(GetParentObject(), aError);
  if (aError.Failed()) {
    return nullptr;
  }

  if (!mActor) {
    promise->MaybeReject(NS_ERROR_DOM_INVALID_STATE_ERR);
    return promise.forget();
  }

  PRFileDesc* fileDesc = mActor->MutableFileDescPtr();

  // get current size of filehandle
  PRFileInfo64 info;
  if (PR_GetOpenFileInfo64(fileDesc, &info) == PR_FAILURE) {
    promise->MaybeReject(NS_ERROR_FAILURE);
  } else {
    LOG_DEBUG(("%p: GetSize %" PRIu64, fileDesc, info.size));
    promise->MaybeResolve(int64_t(info.size));
  }

  return promise.forget();
}

already_AddRefed<Promise> FileSystemSyncAccessHandle::Flush(
    ErrorResult& aError) {
  RefPtr<Promise> promise = Promise::Create(GetParentObject(), aError);
  if (aError.Failed()) {
    return nullptr;
  }

  if (!mActor) {
    promise->MaybeReject(NS_ERROR_DOM_INVALID_STATE_ERR);
    return promise.forget();
  }

  PRFileDesc* fileDesc = mActor->MutableFileDescPtr();

  // flush filehandle
  LOG_DEBUG(("%p: Flush", fileDesc));
  int32_t cnt = PR_Sync(fileDesc);
  if (cnt == -1) {
    promise->MaybeReject(NS_ErrorAccordingToNSPR());
  } else {
    promise->MaybeResolve(NS_OK);
  }

  return promise.forget();
}

already_AddRefed<Promise> FileSystemSyncAccessHandle::Close(
    ErrorResult& aError) {
  RefPtr<Promise> promise = Promise::Create(GetParentObject(), aError);
  if (aError.Failed()) {
    return nullptr;
  }

  if (!mActor) {
    promise->MaybeReject(NS_ERROR_DOM_INVALID_STATE_ERR);
    return promise.forget();
  }

  PRFileDesc* fileDesc = mActor->MutableFileDescPtr();

  MOZ_LOG(mozilla::gOPFSLog, mozilla::LogLevel::Debug,
          ("%p: Closing", fileDesc));

  mActor->Close();
  MOZ_ASSERT(!mActor);

  promise->MaybeResolveWithUndefined();

  return promise.forget();
}
}  // namespace mozilla::dom
