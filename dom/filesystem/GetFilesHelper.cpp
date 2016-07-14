/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GetFilesHelper.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/ContentParent.h"

namespace mozilla {
namespace dom {

/* static */ already_AddRefed<GetFilesHelper>
GetFilesHelper::Create(nsIGlobalObject* aGlobal,
                       const nsTArray<OwningFileOrDirectory>& aFilesOrDirectory,
                       bool aRecursiveFlag, ErrorResult& aRv)
{
  MOZ_ASSERT(aGlobal);

  RefPtr<GetFilesHelper> helper = new GetFilesHelper(aGlobal, aRecursiveFlag);

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

  nsCOMPtr<nsIEventTarget> target =
    do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID);
  MOZ_ASSERT(target);

  aRv = target->Dispatch(helper, NS_DISPATCH_NORMAL);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  return helper.forget();
}

GetFilesHelper::GetFilesHelper(nsIGlobalObject* aGlobal, bool aRecursiveFlag)
  : mGlobal(aGlobal)
  , mRecursiveFlag(aRecursiveFlag)
  , mListingCompleted(false)
  , mErrorResult(NS_OK)
  , mMutex("GetFilesHelper::mMutex")
  , mCanceled(false)
{
  MOZ_ASSERT(aGlobal);
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

  MutexAutoLock lock(mMutex);
  mCanceled = true;
}

void
GetFilesHelper::Traverse(nsCycleCollectionTraversalCallback &cb)
{
  GetFilesHelper* tmp = this;
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mGlobal);
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mFiles);
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPromises);
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

    return NS_DispatchToMainThread(this);
  }

  // We are here, but we should not do anything on this thread because, in the
  // meantime, the operation has been canceled.
  if (IsCanceled()) {
    return NS_OK;
  }

  RunMainThread();

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

  return NS_OK;
}

void
GetFilesHelper::RunIO()
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(!mDirectoryPath.IsEmpty());
  MOZ_ASSERT(!mListingCompleted);

  nsCOMPtr<nsIFile> file;
  mErrorResult = NS_NewNativeLocalFile(NS_ConvertUTF16toUTF8(mDirectoryPath), true,
                                       getter_AddRefs(file));
  if (NS_WARN_IF(NS_FAILED(mErrorResult))) {
    return;
  }

  nsAutoString path;
  path.AssignLiteral(FILESYSTEM_DOM_PATH_SEPARATOR_LITERAL);

  mErrorResult = ExploreDirectory(path, file);
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
  for (uint32_t i = 0; i < mTargetPathArray.Length(); ++i) {
    nsCOMPtr<nsIFile> file;
    mErrorResult =
      NS_NewNativeLocalFile(NS_ConvertUTF16toUTF8(mTargetPathArray[i].mRealPath),
                            true, getter_AddRefs(file));
    if (NS_WARN_IF(NS_FAILED(mErrorResult))) {
      mFiles.Clear();
      return;
    }

    RefPtr<File> domFile =
      File::CreateFromFile(mGlobal, file);
    MOZ_ASSERT(domFile);

    domFile->SetPath(mTargetPathArray[i].mDomPath);

    if (!mFiles.AppendElement(domFile, fallible)) {
      mErrorResult = NS_ERROR_OUT_OF_MEMORY;
      mFiles.Clear();
      return;
    }
  }
}

nsresult
GetFilesHelper::ExploreDirectory(const nsAString& aDOMPath, nsIFile* aFile)
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(aFile);

  // We check if this operation has to be terminated at each recursion.
  if (IsCanceled()) {
    return NS_OK;
  }

  nsCOMPtr<nsISimpleEnumerator> entries;
  nsresult rv = aFile->GetDirectoryEntries(getter_AddRefs(entries));
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
        isLink || isSpecial) {
      continue;
    }

    if (NS_WARN_IF(NS_FAILED(currFile->IsFile(&isFile)) ||
                   NS_FAILED(currFile->IsDirectory(&isDir))) ||
        !(isFile || isDir)) {
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
      FileData* data = mTargetPathArray.AppendElement(fallible);
      if (!data) {
        return NS_ERROR_OUT_OF_MEMORY;
      }

      if (NS_WARN_IF(NS_FAILED(currFile->GetPath(data->mRealPath)))) {
        continue;
      }

      data->mDomPath = domPath;
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

} // dom namespace
} // mozilla namespace
