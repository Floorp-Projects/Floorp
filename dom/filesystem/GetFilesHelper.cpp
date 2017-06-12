/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GetFilesHelper.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/FileBlobImpl.h"
#include "mozilla/dom/IPCBlobUtils.h"
#include "mozilla/ipc/IPCStreamUtils.h"
#include "FileSystemUtils.h"
#include "nsProxyRelease.h"

namespace mozilla {
namespace dom {

namespace {

// This class is used in the DTOR of GetFilesHelper to release resources in the
// correct thread.
class ReleaseRunnable final : public Runnable
{
public:
  static void
  MaybeReleaseOnMainThread(nsTArray<RefPtr<Promise>>& aPromises,
                           nsTArray<RefPtr<GetFilesCallback>>& aCallbacks,
                           Sequence<RefPtr<File>>& aFiles,
                           already_AddRefed<nsIGlobalObject> aGlobal)
  {
    nsCOMPtr<nsIGlobalObject> global(aGlobal);
    if (NS_IsMainThread()) {
      return;
    }

    RefPtr<ReleaseRunnable> runnable =
      new ReleaseRunnable(aPromises, aCallbacks, aFiles, global.forget());
    FileSystemUtils::DispatchRunnable(nullptr, runnable.forget());
  }

  NS_IMETHOD
  Run() override
  {
    MOZ_ASSERT(NS_IsMainThread());

    mPromises.Clear();
    mCallbacks.Clear();
    mFiles.Clear();
    mGlobal = nullptr;

    return NS_OK;
  }

private:
  ReleaseRunnable(nsTArray<RefPtr<Promise>>& aPromises,
                  nsTArray<RefPtr<GetFilesCallback>>& aCallbacks,
                  Sequence<RefPtr<File>>& aFiles,
                  already_AddRefed<nsIGlobalObject> aGlobal)
    : Runnable("dom::ReleaseRunnable")
  {
    mPromises.SwapElements(aPromises);
    mCallbacks.SwapElements(aCallbacks);
    mFiles.SwapElements(aFiles);
    mGlobal = aGlobal;
  }

  nsTArray<RefPtr<Promise>> mPromises;
  nsTArray<RefPtr<GetFilesCallback>> mCallbacks;
  Sequence<RefPtr<File>> mFiles;
  nsCOMPtr<nsIGlobalObject> mGlobal;
};

} // anonymous

///////////////////////////////////////////////////////////////////////////////
// GetFilesHelper Base class

already_AddRefed<GetFilesHelper>
GetFilesHelper::Create(nsIGlobalObject* aGlobal,
                       const nsTArray<OwningFileOrDirectory>& aFilesOrDirectory,
                       bool aRecursiveFlag, ErrorResult& aRv)
{
  RefPtr<GetFilesHelper> helper;

  if (XRE_IsParentProcess()) {
    helper = new GetFilesHelper(aGlobal, aRecursiveFlag);
  } else {
    helper = new GetFilesHelperChild(aGlobal, aRecursiveFlag);
  }

  nsAutoString directoryPath;

  for (uint32_t i = 0; i < aFilesOrDirectory.Length(); ++i) {
    const OwningFileOrDirectory& data = aFilesOrDirectory[i];
    if (data.IsFile()) {
      if (!helper->mFiles.AppendElement(data.GetAsFile(), fallible)) {
        aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
        return nullptr;
      }
    } else {
      MOZ_ASSERT(data.IsDirectory());

      // We support the upload of only 1 top-level directory from our
      // directory picker. This means that we cannot have more than 1
      // Directory object in aFilesOrDirectory array.
      MOZ_ASSERT(directoryPath.IsEmpty());

      RefPtr<Directory> directory = data.GetAsDirectory();
      MOZ_ASSERT(directory);

      aRv = directory->GetFullRealPath(directoryPath);
      if (NS_WARN_IF(aRv.Failed())) {
        return nullptr;
      }
    }
  }

  // No directories to explore.
  if (directoryPath.IsEmpty()) {
    helper->mListingCompleted = true;
    return helper.forget();
  }

  MOZ_ASSERT(helper->mFiles.IsEmpty());
  helper->SetDirectoryPath(directoryPath);

  helper->Work(aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  return helper.forget();
}

GetFilesHelper::GetFilesHelper(nsIGlobalObject* aGlobal, bool aRecursiveFlag)
  : Runnable("GetFilesHelper")
  , GetFilesHelperBase(aRecursiveFlag)
  , mGlobal(aGlobal)
  , mListingCompleted(false)
  , mErrorResult(NS_OK)
  , mMutex("GetFilesHelper::mMutex")
  , mCanceled(false)
{
}

GetFilesHelper::~GetFilesHelper()
{
  ReleaseRunnable::MaybeReleaseOnMainThread(mPromises, mCallbacks, mFiles,
                                            mGlobal.forget());
}

void
GetFilesHelper::AddPromise(Promise* aPromise)
{
  MOZ_ASSERT(aPromise);

  // Still working.
  if (!mListingCompleted) {
    mPromises.AppendElement(aPromise);
    return;
  }

  MOZ_ASSERT(mPromises.IsEmpty());
  ResolveOrRejectPromise(aPromise);
}

void
GetFilesHelper::AddCallback(GetFilesCallback* aCallback)
{
  MOZ_ASSERT(aCallback);

  // Still working.
  if (!mListingCompleted) {
    mCallbacks.AppendElement(aCallback);
    return;
  }

  MOZ_ASSERT(mCallbacks.IsEmpty());
  RunCallback(aCallback);
}

void
GetFilesHelper::Unlink()
{
  mGlobal = nullptr;
  mFiles.Clear();
  mPromises.Clear();
  mCallbacks.Clear();

  {
    MutexAutoLock lock(mMutex);
    mCanceled = true;
  }

  Cancel();
}

void
GetFilesHelper::Traverse(nsCycleCollectionTraversalCallback &cb)
{
  GetFilesHelper* tmp = this;
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mGlobal);
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mFiles);
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPromises);
}

void
GetFilesHelper::Work(ErrorResult& aRv)
{
  nsCOMPtr<nsIEventTarget> target =
    do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID);
  MOZ_ASSERT(target);

  aRv = target->Dispatch(this, NS_DISPATCH_NORMAL);
}

NS_IMETHODIMP
GetFilesHelper::Run()
{
  MOZ_ASSERT(!mDirectoryPath.IsEmpty());
  MOZ_ASSERT(!mListingCompleted);

  // First step is to retrieve the list of file paths.
  // This happens in the I/O thread.
  if (!NS_IsMainThread()) {
    RunIO();

    // If this operation has been canceled, we don't have to go back to
    // main-thread.
    if (IsCanceled()) {
      return NS_OK;
    }

    RefPtr<Runnable> runnable = this;
    return FileSystemUtils::DispatchRunnable(nullptr, runnable.forget());
  }

  // We are here, but we should not do anything on this thread because, in the
  // meantime, the operation has been canceled.
  if (IsCanceled()) {
    return NS_OK;
  }

  RunMainThread();

  OperationCompleted();
  return NS_OK;
}

void
GetFilesHelper::OperationCompleted()
{
  // We mark the operation as completed here.
  mListingCompleted = true;

  // Let's process the pending promises.
  nsTArray<RefPtr<Promise>> promises;
  promises.SwapElements(mPromises);

  for (uint32_t i = 0; i < promises.Length(); ++i) {
    ResolveOrRejectPromise(promises[i]);
  }

  // Let's process the pending callbacks.
  nsTArray<RefPtr<GetFilesCallback>> callbacks;
  callbacks.SwapElements(mCallbacks);

  for (uint32_t i = 0; i < callbacks.Length(); ++i) {
    RunCallback(callbacks[i]);
  }
}

void
GetFilesHelper::RunIO()
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(!mDirectoryPath.IsEmpty());
  MOZ_ASSERT(!mListingCompleted);

  nsCOMPtr<nsIFile> file;
  mErrorResult = NS_NewLocalFile(mDirectoryPath, true, getter_AddRefs(file));
  if (NS_WARN_IF(NS_FAILED(mErrorResult))) {
    return;
  }

  nsAutoString leafName;
  mErrorResult = file->GetLeafName(leafName);
  if (NS_WARN_IF(NS_FAILED(mErrorResult))) {
    return;
  }

  nsAutoString domPath;
  domPath.AssignLiteral(FILESYSTEM_DOM_PATH_SEPARATOR_LITERAL);
  domPath.Append(leafName);

  mErrorResult = ExploreDirectory(domPath, file);
}

void
GetFilesHelper::RunMainThread()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mDirectoryPath.IsEmpty());
  MOZ_ASSERT(!mListingCompleted);

  // If there is an error, do nothing.
  if (NS_FAILED(mErrorResult)) {
    return;
  }

  // Create the sequence of Files.
  for (uint32_t i = 0; i < mTargetBlobImplArray.Length(); ++i) {
    RefPtr<File> domFile = File::Create(mGlobal, mTargetBlobImplArray[i]);
    MOZ_ASSERT(domFile);

    if (!mFiles.AppendElement(domFile, fallible)) {
      mErrorResult = NS_ERROR_OUT_OF_MEMORY;
      mFiles.Clear();
      return;
    }
  }
}

nsresult
GetFilesHelperBase::ExploreDirectory(const nsAString& aDOMPath, nsIFile* aFile)
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(aFile);

  // We check if this operation has to be terminated at each recursion.
  if (IsCanceled()) {
    return NS_OK;
  }

  nsresult rv = AddExploredDirectory(aFile);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsISimpleEnumerator> entries;
  rv = aFile->GetDirectoryEntries(getter_AddRefs(entries));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  for (;;) {
    bool hasMore = false;
    if (NS_WARN_IF(NS_FAILED(entries->HasMoreElements(&hasMore))) || !hasMore) {
      break;
    }

    nsCOMPtr<nsISupports> supp;
    if (NS_WARN_IF(NS_FAILED(entries->GetNext(getter_AddRefs(supp))))) {
      break;
    }

    nsCOMPtr<nsIFile> currFile = do_QueryInterface(supp);
    MOZ_ASSERT(currFile);

    bool isLink, isSpecial, isFile, isDir;
    if (NS_WARN_IF(NS_FAILED(currFile->IsSymlink(&isLink)) ||
                   NS_FAILED(currFile->IsSpecial(&isSpecial))) ||
        isSpecial) {
      continue;
    }

    if (NS_WARN_IF(NS_FAILED(currFile->IsFile(&isFile)) ||
                   NS_FAILED(currFile->IsDirectory(&isDir))) ||
        !(isFile || isDir)) {
      continue;
    }

    // We don't want to explore loops of links.
    if (isDir && isLink && !ShouldFollowSymLink(currFile)) {
      continue;
    }

    // The new domPath
    nsAutoString domPath;
    domPath.Assign(aDOMPath);
    if (!aDOMPath.EqualsLiteral(FILESYSTEM_DOM_PATH_SEPARATOR_LITERAL)) {
      domPath.AppendLiteral(FILESYSTEM_DOM_PATH_SEPARATOR_LITERAL);
    }

    nsAutoString leafName;
    if (NS_WARN_IF(NS_FAILED(currFile->GetLeafName(leafName)))) {
      continue;
    }
    domPath.Append(leafName);

    if (isFile) {
      RefPtr<BlobImpl> blobImpl = new FileBlobImpl(currFile);
      blobImpl->SetDOMPath(domPath);

      if (!mTargetBlobImplArray.AppendElement(blobImpl, fallible)) {
        return NS_ERROR_OUT_OF_MEMORY;
      }

      continue;
    }

    MOZ_ASSERT(isDir);
    if (!mRecursiveFlag) {
      continue;
    }

    // Recursive.
    rv = ExploreDirectory(domPath, currFile);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  return NS_OK;
}

nsresult
GetFilesHelperBase::AddExploredDirectory(nsIFile* aDir)
{
  nsresult rv;

#ifdef DEBUG
  bool isDir;
  rv = aDir->IsDirectory(&isDir);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_ASSERT(isDir, "Why are we here?");
#endif

  bool isLink;
  rv = aDir->IsSymlink(&isLink);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsAutoString path;
  if (!isLink) {
    rv = aDir->GetPath(path);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  } else {
    rv = aDir->GetTarget(path);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  mExploredDirectories.PutEntry(path);
  return NS_OK;
}

bool
GetFilesHelperBase::ShouldFollowSymLink(nsIFile* aDir)
{
#ifdef DEBUG
  bool isLink, isDir;
  if (NS_WARN_IF(NS_FAILED(aDir->IsSymlink(&isLink)) ||
                 NS_FAILED(aDir->IsDirectory(&isDir)))) {
    return false;
  }

  MOZ_ASSERT(isLink && isDir, "Why are we here?");
#endif

  nsAutoString targetPath;
  if (NS_WARN_IF(NS_FAILED(aDir->GetTarget(targetPath)))) {
    return false;
  }

  return !mExploredDirectories.Contains(targetPath);
}

void
GetFilesHelper::ResolveOrRejectPromise(Promise* aPromise)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mListingCompleted);
  MOZ_ASSERT(aPromise);

  // Error propagation.
  if (NS_FAILED(mErrorResult)) {
    aPromise->MaybeReject(mErrorResult);
    return;
  }

  aPromise->MaybeResolve(mFiles);
}

void
GetFilesHelper::RunCallback(GetFilesCallback* aCallback)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mListingCompleted);
  MOZ_ASSERT(aCallback);

  aCallback->Callback(mErrorResult, mFiles);
}

///////////////////////////////////////////////////////////////////////////////
// GetFilesHelperChild class

void
GetFilesHelperChild::Work(ErrorResult& aRv)
{
  ContentChild* cc = ContentChild::GetSingleton();
  if (NS_WARN_IF(!cc)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  aRv = nsContentUtils::GenerateUUIDInPlace(mUUID);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  mPendingOperation = true;
  cc->CreateGetFilesRequest(mDirectoryPath, mRecursiveFlag, mUUID, this);
}

void
GetFilesHelperChild::Cancel()
{
  if (!mPendingOperation) {
    return;
  }

  ContentChild* cc = ContentChild::GetSingleton();
  if (NS_WARN_IF(!cc)) {
    return;
  }

  mPendingOperation = false;
  cc->DeleteGetFilesRequest(mUUID, this);
}

bool
GetFilesHelperChild::AppendBlobImpl(BlobImpl* aBlobImpl)
{
  MOZ_ASSERT(mPendingOperation);
  MOZ_ASSERT(aBlobImpl);
  MOZ_ASSERT(aBlobImpl->IsFile());

  RefPtr<File> file = File::Create(mGlobal, aBlobImpl);
  MOZ_ASSERT(file);

  return mFiles.AppendElement(file, fallible);
}

void
GetFilesHelperChild::Finished(nsresult aError)
{
  MOZ_ASSERT(mPendingOperation);
  MOZ_ASSERT(NS_SUCCEEDED(mErrorResult));

  mPendingOperation = false;
  mErrorResult = aError;

  OperationCompleted();
}

///////////////////////////////////////////////////////////////////////////////
// GetFilesHelperParent class

class GetFilesHelperParentCallback final : public GetFilesCallback
{
public:
  explicit GetFilesHelperParentCallback(GetFilesHelperParent* aParent)
    : mParent(aParent)
  {
    MOZ_ASSERT(aParent);
  }

  void
  Callback(nsresult aStatus, const Sequence<RefPtr<File>>& aFiles) override
  {
    if (NS_FAILED(aStatus)) {
      mParent->mContentParent->SendGetFilesResponseAndForget(mParent->mUUID,
                                                             GetFilesResponseFailure(aStatus));
      return;
    }

    GetFilesResponseSuccess success;

    nsTArray<IPCBlob>& ipcBlobs = success.blobs();
    ipcBlobs.SetLength(aFiles.Length());

    for (uint32_t i = 0; i < aFiles.Length(); ++i) {
      nsresult rv = IPCBlobUtils::Serialize(aFiles[i]->Impl(),
                                            mParent->mContentParent,
                                            ipcBlobs[i]);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        mParent->mContentParent->SendGetFilesResponseAndForget(mParent->mUUID,
                                                               GetFilesResponseFailure(NS_ERROR_OUT_OF_MEMORY));
        return;
      }
    }

    mParent->mContentParent->SendGetFilesResponseAndForget(mParent->mUUID,
                                                           success);
  }

private:
  // Raw pointer because this callback is kept alive by this parent object.
  GetFilesHelperParent* mParent;
};

GetFilesHelperParent::GetFilesHelperParent(const nsID& aUUID,
                                           ContentParent* aContentParent,
                                           bool aRecursiveFlag)
  : GetFilesHelper(nullptr, aRecursiveFlag)
  , mContentParent(aContentParent)
  , mUUID(aUUID)
{}

GetFilesHelperParent::~GetFilesHelperParent()
{
  NS_ReleaseOnMainThread(
    "GetFilesHelperParent::mContentParent", mContentParent.forget());
}

/* static */ already_AddRefed<GetFilesHelperParent>
GetFilesHelperParent::Create(const nsID& aUUID, const nsAString& aDirectoryPath,
                             bool aRecursiveFlag, ContentParent* aContentParent,
                             ErrorResult& aRv)
{
  MOZ_ASSERT(aContentParent);

  RefPtr<GetFilesHelperParent> helper =
    new GetFilesHelperParent(aUUID, aContentParent, aRecursiveFlag);
  helper->SetDirectoryPath(aDirectoryPath);

  helper->Work(aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  RefPtr<GetFilesHelperParentCallback> callback =
    new GetFilesHelperParentCallback(helper);
  helper->AddCallback(callback);

  return helper.forget();
}

} // dom namespace
} // mozilla namespace
