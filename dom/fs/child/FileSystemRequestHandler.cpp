/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "fs/FileSystemRequestHandler.h"
#include "fs/FileSystemConstants.h"

#include "nsIScriptObjectPrincipal.h"
#include "mozilla/dom/BackgroundFileSystemChild.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/FileSystemFileHandle.h"
#include "mozilla/dom/FileSystemDirectoryHandle.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/BackgroundUtils.h"
#include "mozilla/ipc/PBackgroundChild.h"

using namespace mozilla::ipc;

namespace mozilla::dom::fs {

namespace {

// Not static: BackgroundFileSystemChild must be owned by calling thread
RefPtr<mozilla::dom::BackgroundFileSystemChild> GetRootProvider(
    nsIGlobalObject* aGlobal) {
  // TODO: It would be nice to convert all error checks to QM_TRY some time
  //       later.

  // TODO: It would be cleaner if we were called with a PrincipalInfo, instead
  //       of an nsIGlobalObject, so we wouldn't have the dependency on the
  //       workers code.

  PBackgroundChild* backgroundActor =
      BackgroundChild::GetOrCreateForCurrentThread();

  if (NS_WARN_IF(!backgroundActor)) {
    MOZ_ASSERT(false);
    return nullptr;
  }

  RefPtr<BackgroundFileSystemChild> result;

  if (NS_IsMainThread()) {
    nsCOMPtr<nsIScriptObjectPrincipal> sop = do_QueryInterface(aGlobal);
    if (!sop) {
      return nullptr;
    }

    nsCOMPtr<nsIPrincipal> principal = sop->GetEffectiveStoragePrincipal();
    if (!principal) {
      return nullptr;
    }

    auto principalInfo = MakeUnique<PrincipalInfo>();
    nsresult rv = PrincipalToPrincipalInfo(principal, principalInfo.get());
    if (NS_FAILED(rv)) {
      return nullptr;
    }

    BackgroundFileSystemChild* actor = new BackgroundFileSystemChild();

    result = static_cast<BackgroundFileSystemChild*>(
        backgroundActor->SendPBackgroundFileSystemConstructor(actor,
                                                              *principalInfo));
  } else {
    WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
    if (!workerPrivate) {
      return nullptr;
    }

    const PrincipalInfo& principalInfo =
        workerPrivate->GetEffectiveStoragePrincipalInfo();

    BackgroundFileSystemChild* actor = new BackgroundFileSystemChild();

    result = static_cast<BackgroundFileSystemChild*>(
        backgroundActor->SendPBackgroundFileSystemConstructor(actor,
                                                              principalInfo));
  }

  MOZ_ASSERT(result);

  return result;
}

// TODO: This is just a dummy implementation
RefPtr<File> MakeGetFileResult(const nsString& aName, const nsString& aType,
                               int64_t aLastModifiedMilliSeconds,
                               nsTArray<Name>&& aPath,
                               mozilla::ipc::FileDescriptor&& aFile,
                               nsIGlobalObject* aGlobal) {
  // TODO: Replace with a real implementation
  RefPtr<File> result = File::CreateMemoryFileWithCustomLastModified(
      aGlobal, static_cast<void*>(new uint8_t[1]), sizeof(uint8_t), aName,
      aType, aLastModifiedMilliSeconds);

  return result;
}

void GetDirectoryContentsResponseHandler(
    RefPtr<FileSystemActorHolder>& /* aActor */,
    FileSystemDirectoryListing&& aResponse, nsIGlobalObject* aGlobal,
    ArrayAppendable& /* aSink */) {
  // TODO: Add page size to FileSystemConstants, preallocate and handle overflow
  nsTArray<RefPtr<FileSystemHandle>> batch;

  for (const auto& it : aResponse.files()) {
    RefPtr<FileSystemHandle> handle = new FileSystemFileHandle(aGlobal, it);
    batch.AppendElement(handle);
  }

  for (const auto& it : aResponse.directories()) {
    RefPtr<FileSystemHandle> handle =
        new FileSystemDirectoryHandle(aGlobal, it);
    batch.AppendElement(handle);
  }
}

RefPtr<FileSystemDirectoryHandle> MakeResolution(
    nsIGlobalObject* aGlobal, FileSystemGetHandleResponse&& aResponse,
    const RefPtr<FileSystemDirectoryHandle>& /* aResolution */,
    const Name& aName, RefPtr<FileSystemActorHolder>& aActor) {
  RefPtr<FileSystemDirectoryHandle> result = new FileSystemDirectoryHandle(
      aGlobal, FileSystemEntryMetadata(aResponse.get_EntryId(), aName));
  return result;
}

RefPtr<FileSystemFileHandle> MakeResolution(
    nsIGlobalObject* aGlobal, FileSystemGetHandleResponse&& aResponse,
    const RefPtr<FileSystemFileHandle>& /* aResolution */, const Name& aName,
    RefPtr<FileSystemActorHolder>& aActor) {
  RefPtr<FileSystemFileHandle> result = new FileSystemFileHandle(
      aGlobal, FileSystemEntryMetadata(aResponse.get_EntryId(), aName));
  return result;
}

RefPtr<File> MakeResolution(nsIGlobalObject* aGlobal,
                            FileSystemGetFileResponse&& aResponse,
                            const RefPtr<File>& /* aResolution */,
                            const Name& aName,
                            RefPtr<FileSystemActorHolder>& aActor) {
  auto& fileProperties = aResponse.get_FileSystemFileProperties();
  return MakeGetFileResult(aName, fileProperties.type(),
                           fileProperties.last_modified_ms(),
                           std::move(fileProperties.path()),
                           std::move(fileProperties.file()), aGlobal);
}

template <class TResponse, class... Args>
void ResolveCallback(
    TResponse&& aResponse,
    RefPtr<Promise> aPromise,  // NOLINT(performance-unnecessary-value-param)
    Args&&... args) {
  MOZ_ASSERT(aPromise);
  QM_TRY(OkIf(Promise::PromiseState::Pending == aPromise->State()), QM_VOID);

  if (TResponse::Tnsresult == aResponse.type()) {
    aPromise->MaybeReject(aResponse.get_nsresult());
    return;
  }

  aPromise->MaybeResolve(MakeResolution(aPromise->GetParentObject(),
                                        std::forward<TResponse>(aResponse),
                                        std::forward<Args>(args)...));
}

template <>
void ResolveCallback(
    FileSystemRemoveEntryResponse&& aResponse,
    RefPtr<Promise> aPromise) {  // NOLINT(performance-unnecessary-value-param)
  MOZ_ASSERT(aPromise);
  QM_TRY(OkIf(Promise::PromiseState::Pending == aPromise->State()), QM_VOID);

  MOZ_ASSERT(FileSystemRemoveEntryResponse::Tnsresult == aResponse.type());
  const auto& status = aResponse.get_nsresult();
  if (NS_ERROR_FILE_ACCESS_DENIED == status) {
    aPromise->MaybeRejectWithNotAllowedError("Permission denied");
  } else if (NS_ERROR_DOM_FILESYSTEM_NO_MODIFICATION_ALLOWED_ERR == status) {
    aPromise->MaybeRejectWithInvalidModificationError("Disallowed by system");
  } else if (NS_FAILED(status)) {
    aPromise->MaybeRejectWithUnknownError("Unknown failure");
  } else {
    aPromise->MaybeResolveWithUndefined();
  }
}

// NOLINTBEGIN(readability-inconsistent-declaration-parameter-name)
template <>
void ResolveCallback(FileSystemGetEntriesResponse&& aResponse,
                     // NOLINTNEXTLINE(performance-unnecessary-value-param)
                     RefPtr<Promise> aPromise, ArrayAppendable& aSink,
                     RefPtr<FileSystemActorHolder>& aActor) {
  // NOLINTEND(readability-inconsistent-declaration-parameter-name)
  MOZ_ASSERT(aPromise);
  QM_TRY(OkIf(Promise::PromiseState::Pending == aPromise->State()), QM_VOID);

  if (FileSystemGetEntriesResponse::Tnsresult == aResponse.type()) {
    aPromise->MaybeReject(aResponse.get_nsresult());
    return;
  }

  GetDirectoryContentsResponseHandler(
      aActor,
      std::forward<FileSystemDirectoryListing>(
          aResponse.get_FileSystemDirectoryListing()),
      aPromise->GetParentObject(), aSink);

  // TODO: Remove this when sink is ready
  aPromise->MaybeReject(NS_ERROR_NOT_IMPLEMENTED);
}

template <class TResponse, class TReturns, class... Args,
          std::enable_if_t<std::is_same<TReturns, void>::value, bool> = true>
mozilla::ipc::ResolveCallback<TResponse> SelectResolveCallback(
    RefPtr<Promise> aPromise,  // NOLINT(performance-unnecessary-value-param)
    Args&&... args) {
  using TOverload = void (*)(TResponse&&, RefPtr<Promise>, Args...);
  return static_cast<std::function<void(TResponse &&)>>(
      // NOLINTNEXTLINE(modernize-avoid-bind)
      std::bind(static_cast<TOverload>(ResolveCallback), std::placeholders::_1,
                aPromise, std::forward<Args>(args)...));
}

template <class TResponse, class TReturns, class... Args,
          std::enable_if_t<!std::is_same<TReturns, void>::value, bool> = true>
mozilla::ipc::ResolveCallback<TResponse> SelectResolveCallback(
    RefPtr<Promise> aPromise,  // NOLINT(performance-unnecessary-value-param)
    Args&&... args) {
  using TOverload =
      void (*)(TResponse&&, RefPtr<Promise>, const TReturns&, Args...);
  return static_cast<std::function<void(TResponse &&)>>(
      // NOLINTNEXTLINE(modernize-avoid-bind)
      std::bind(static_cast<TOverload>(ResolveCallback), std::placeholders::_1,
                aPromise, TReturns(), std::forward<Args>(args)...));
}

// TODO: Find a better way to deal with these errors
void IPCRejectReporter(mozilla::ipc::ResponseRejectReason aReason) {
  switch (aReason) {
    case mozilla::ipc::ResponseRejectReason::ActorDestroyed:
      // This is ok
      break;
    case mozilla::ipc::ResponseRejectReason::HandlerRejected:
      QM_TRY(OkIf(false), QM_VOID);
      break;
    case mozilla::ipc::ResponseRejectReason::ChannelClosed:
      QM_TRY(OkIf(false), QM_VOID);
      break;
    case mozilla::ipc::ResponseRejectReason::ResolverDestroyed:
      QM_TRY(OkIf(false), QM_VOID);
      break;
    case mozilla::ipc::ResponseRejectReason::SendError:
      QM_TRY(OkIf(false), QM_VOID);
      break;
    default:
      QM_TRY(OkIf(false), QM_VOID);
      break;
  }
}

void RejectHandler(
    RefPtr<Promise> aPromise,  // NOLINT(performance-unnecessary-value-param)
    mozilla::ipc::ResponseRejectReason aReason) {
  IPCRejectReporter(aReason);
  QM_TRY(OkIf(Promise::PromiseState::Pending == aPromise->State()), QM_VOID);
  aPromise->MaybeRejectWithUndefined();
}

mozilla::ipc::RejectCallback GetRejectCallback(
    RefPtr<Promise> aPromise) {  // NOLINT(performance-unnecessary-value-param)
  return static_cast<mozilla::ipc::RejectCallback>(
      // NOLINTNEXTLINE(modernize-avoid-bind)
      std::bind(RejectHandler, aPromise, std::placeholders::_1));
}

}  // namespace

void FileSystemRequestHandler::GetRoot(
    const Origin& aOrigin,
    RefPtr<Promise> aPromise) {  // NOLINT(performance-unnecessary-value-param)
  MOZ_ASSERT(!aOrigin.IsEmpty());
  MOZ_ASSERT(aPromise);

  RefPtr<FileSystemActorHolder> dummyActor =
      MakeAndAddRef<FileSystemActorHolder>();

  Name name = kRootName;
  auto&& onResolve = SelectResolveCallback<FileSystemGetHandleResponse,
                                           RefPtr<FileSystemDirectoryHandle>>(
      aPromise, name, dummyActor);

  auto&& onReject = GetRejectCallback(aPromise);

  // XXX do something (register with global?) so that we can Close() the actor
  // before the event queue starts to shut down.  That will cancel all
  // outstanding async requests (returns lambdas with errors).
  RefPtr<mozilla::dom::BackgroundFileSystemChild> rootProvider =
      GetRootProvider(aPromise->GetGlobalObject());
  if (!rootProvider) {
    aPromise->MaybeRejectWithUnknownError("Could not access the file system");
    return;
  }
  rootProvider->SendGetRoot(aOrigin, std::move(onResolve), std::move(onReject));
}

void FileSystemRequestHandler::GetDirectoryHandle(
    RefPtr<FileSystemActorHolder>& aActor,
    const FileSystemChildMetadata& aDirectory, bool aCreate,
    RefPtr<Promise> aPromise) {  // NOLINT(performance-unnecessary-value-param)
  MOZ_ASSERT(!aDirectory.parentId().IsEmpty());
  MOZ_ASSERT(aPromise);

  FileSystemGetHandleRequest request(aDirectory, aCreate);

  auto&& onResolve = SelectResolveCallback<FileSystemGetHandleResponse,
                                           RefPtr<FileSystemDirectoryHandle>>(
      aPromise, aDirectory.childName(), aActor);

  auto&& onReject = GetRejectCallback(aPromise);

  auto actor = GetRootProvider(aPromise->GetGlobalObject());
  QM_TRY(OkIf(actor), QM_VOID);
  actor->SendGetDirectoryHandle(request, std::move(onResolve),
                                std::move(onReject));
}

void FileSystemRequestHandler::GetFileHandle(
    RefPtr<FileSystemActorHolder>& aActor, const FileSystemChildMetadata& aFile,
    bool aCreate,
    RefPtr<Promise> aPromise) {  // NOLINT(performance-unnecessary-value-param)
  MOZ_ASSERT(!aFile.parentId().IsEmpty());
  MOZ_ASSERT(aPromise);

  FileSystemGetHandleRequest request(aFile, aCreate);

  auto&& onResolve = SelectResolveCallback<FileSystemGetHandleResponse,
                                           RefPtr<FileSystemFileHandle>>(
      aPromise, aFile.childName(), aActor);

  auto&& onReject = GetRejectCallback(aPromise);

  auto actor = GetRootProvider(aPromise->GetGlobalObject());
  QM_TRY(OkIf(actor), QM_VOID);
  actor->SendGetFileHandle(request, std::move(onResolve), std::move(onReject));
}

void FileSystemRequestHandler::GetFile(
    RefPtr<FileSystemActorHolder>& aActor, const FileSystemEntryMetadata& aFile,
    RefPtr<Promise> aPromise) {  // NOLINT(performance-unnecessary-value-param)
  MOZ_ASSERT(!aFile.entryId().IsEmpty());
  MOZ_ASSERT(aPromise);

  FileSystemGetFileRequest request(aFile.entryId());

  auto&& onResolve =
      SelectResolveCallback<FileSystemGetFileResponse, RefPtr<File>>(
          aPromise, aFile.entryName(), aActor);

  auto&& onReject = GetRejectCallback(aPromise);

  auto actor = GetRootProvider(aPromise->GetGlobalObject());
  QM_TRY(OkIf(actor), QM_VOID);
  actor->SendGetFile(request, std::move(onResolve), std::move(onReject));
}

void FileSystemRequestHandler::GetEntries(
    RefPtr<FileSystemActorHolder>& aActor, const EntryId& aDirectory,
    PageNumber aPage,
    RefPtr<Promise> aPromise,  // NOLINT(performance-unnecessary-value-param)
    ArrayAppendable& aSink) {
  MOZ_ASSERT(!aDirectory.IsEmpty());
  MOZ_ASSERT(aPromise);

  FileSystemGetEntriesRequest request(aDirectory, aPage);

  using TOverload = void (*)(FileSystemGetEntriesResponse&&, RefPtr<Promise>,
                             ArrayAppendable&, RefPtr<FileSystemActorHolder>&);

  // We are not allowed to pass a promise to an external function in a lambda
  auto&& onResolve =
      static_cast<std::function<void(FileSystemGetEntriesResponse &&)>>(
          // NOLINTNEXTLINE(modernize-avoid-bind)
          std::bind(static_cast<TOverload>(ResolveCallback),
                    std::placeholders::_1, aPromise, std::ref(aSink), aActor));

  auto&& onReject = GetRejectCallback(aPromise);

  auto actor = GetRootProvider(aPromise->GetGlobalObject());
  QM_TRY(OkIf(actor), QM_VOID);
  actor->SendGetEntries(request, std::move(onResolve), std::move(onReject));
}

void FileSystemRequestHandler::RemoveEntry(
    RefPtr<FileSystemActorHolder>& /* aActor */,
    const FileSystemChildMetadata& aEntry, bool aRecursive,
    RefPtr<Promise> aPromise) {  // NOLINT(performance-unnecessary-value-param)
  MOZ_ASSERT(!aEntry.parentId().IsEmpty());
  MOZ_ASSERT(aPromise);

  FileSystemRemoveEntryRequest request(aEntry, aRecursive);

  auto&& onResolve =
      SelectResolveCallback<FileSystemRemoveEntryResponse, void>(aPromise);

  auto&& onReject = GetRejectCallback(aPromise);

  auto actor = GetRootProvider(aPromise->GetGlobalObject());
  QM_TRY(OkIf(actor), QM_VOID);
  actor->SendRemoveEntry(request, std::move(onResolve), std::move(onReject));
}

}  // namespace mozilla::dom::fs
