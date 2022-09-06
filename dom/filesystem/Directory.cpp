/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/Directory.h"

#include "GetDirectoryListingTask.h"
#include "GetFilesTask.h"

#include "nsIFile.h"
#include "nsString.h"
#include "mozilla/dom/BlobImpl.h"
#include "mozilla/dom/DirectoryBinding.h"
#include "mozilla/dom/FileSystemBase.h"
#include "mozilla/dom/FileSystemUtils.h"
#include "mozilla/dom/OSFileSystem.h"
#include "mozilla/dom/WorkerPrivate.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(Directory)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(Directory)
  if (tmp->mFileSystem) {
    tmp->mFileSystem->Unlink();
    tmp->mFileSystem = nullptr;
  }
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mGlobal)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(Directory)
  if (tmp->mFileSystem) {
    tmp->mFileSystem->Traverse(cb);
  }
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mGlobal)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(Directory)
NS_IMPL_CYCLE_COLLECTING_RELEASE(Directory)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(Directory)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

/* static */
already_AddRefed<Directory> Directory::Constructor(const GlobalObject& aGlobal,
                                                   const nsAString& aRealPath,
                                                   ErrorResult& aRv) {
  nsCOMPtr<nsIFile> path;
  aRv = NS_NewLocalFile(aRealPath, true, getter_AddRefs(path));
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());
  if (NS_WARN_IF(!global)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  return Create(global, path);
}

/* static */
already_AddRefed<Directory> Directory::Create(nsIGlobalObject* aGlobal,
                                              nsIFile* aFile,
                                              FileSystemBase* aFileSystem) {
  MOZ_ASSERT(aGlobal);
  MOZ_ASSERT(aFile);

  RefPtr<Directory> directory = new Directory(aGlobal, aFile, aFileSystem);
  return directory.forget();
}

Directory::Directory(nsIGlobalObject* aGlobal, nsIFile* aFile,
                     FileSystemBase* aFileSystem)
    : mGlobal(aGlobal), mFile(aFile) {
  MOZ_ASSERT(aFile);

  // aFileSystem can be null. In this case we create a OSFileSystem when needed.
  if (aFileSystem) {
    // More likely, this is a OSFileSystem. This object keeps a reference of
    // mGlobal but it's not cycle collectable and to avoid manual
    // addref/release, it's better to have 1 object per directory. For this
    // reason we clone it here.
    mFileSystem = aFileSystem->Clone();
  }
}

Directory::~Directory() = default;

nsIGlobalObject* Directory::GetParentObject() const { return mGlobal; }

JSObject* Directory::WrapObject(JSContext* aCx,
                                JS::Handle<JSObject*> aGivenProto) {
  return Directory_Binding::Wrap(aCx, this, aGivenProto);
}

void Directory::GetName(nsAString& aRetval, ErrorResult& aRv) {
  aRetval.Truncate();

  RefPtr<FileSystemBase> fs = GetFileSystem(aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  fs->GetDirectoryName(mFile, aRetval, aRv);
}

void Directory::GetPath(nsAString& aRetval, ErrorResult& aRv) {
  // This operation is expensive. Better to cache the result.
  if (mPath.IsEmpty()) {
    RefPtr<FileSystemBase> fs = GetFileSystem(aRv);
    if (NS_WARN_IF(aRv.Failed())) {
      return;
    }

    fs->GetDOMPath(mFile, mPath, aRv);
    if (NS_WARN_IF(aRv.Failed())) {
      return;
    }
  }

  aRetval = mPath;
}

nsresult Directory::GetFullRealPath(nsAString& aPath) {
  nsresult rv = mFile->GetPath(aPath);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

already_AddRefed<Promise> Directory::GetFilesAndDirectories(ErrorResult& aRv) {
  RefPtr<FileSystemBase> fs = GetFileSystem(aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  RefPtr<GetDirectoryListingTaskChild> task =
      GetDirectoryListingTaskChild::Create(fs, this, mFile, mFilters, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  task->Start();

  return task->GetPromise();
}

already_AddRefed<Promise> Directory::GetFiles(bool aRecursiveFlag,
                                              ErrorResult& aRv) {
  ErrorResult rv;
  RefPtr<FileSystemBase> fs = GetFileSystem(rv);
  if (NS_WARN_IF(rv.Failed())) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  RefPtr<GetFilesTaskChild> task =
      GetFilesTaskChild::Create(fs, this, mFile, aRecursiveFlag, rv);
  if (NS_WARN_IF(rv.Failed())) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  task->Start();

  return task->GetPromise();
}

void Directory::SetContentFilters(const nsAString& aFilters) {
  mFilters = aFilters;
}

FileSystemBase* Directory::GetFileSystem(ErrorResult& aRv) {
  if (!mFileSystem) {
    nsAutoString path;
    aRv = mFile->GetPath(path);
    if (NS_WARN_IF(aRv.Failed())) {
      return nullptr;
    }

    RefPtr<OSFileSystem> fs = new OSFileSystem(path);
    fs->Init(mGlobal);

    mFileSystem = fs;
  }

  return mFileSystem;
}

}  // namespace mozilla::dom
