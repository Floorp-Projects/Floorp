/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/Directory.h"

#include "GetDirectoryListingTask.h"
#include "GetFilesTask.h"
#include "WorkerPrivate.h"

#include "nsCharSeparatedTokenizer.h"
#include "nsString.h"
#include "mozilla/dom/DirectoryBinding.h"
#include "mozilla/dom/FileSystemBase.h"
#include "mozilla/dom/FileSystemUtils.h"
#include "mozilla/dom/OSFileSystem.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_CLASS(Directory)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(Directory)
  if (tmp->mFileSystem) {
    tmp->mFileSystem->Unlink();
    tmp->mFileSystem = nullptr;
  }
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mParent)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(Directory)
  if (tmp->mFileSystem) {
    tmp->mFileSystem->Traverse(cb);
  }
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mParent)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_WRAPPERCACHE(Directory)

NS_IMPL_CYCLE_COLLECTING_ADDREF(Directory)
NS_IMPL_CYCLE_COLLECTING_RELEASE(Directory)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(Directory)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

/* static */ already_AddRefed<Directory>
Directory::Constructor(const GlobalObject& aGlobal,
                       const nsAString& aRealPath,
                       ErrorResult& aRv)
{
  nsCOMPtr<nsIFile> path;
  aRv = NS_NewLocalFile(aRealPath, true, getter_AddRefs(path));
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  return Create(aGlobal.GetAsSupports(), path);
}

/* static */ already_AddRefed<Directory>
Directory::Create(nsISupports* aParent, nsIFile* aFile,
                  FileSystemBase* aFileSystem)
{
  MOZ_ASSERT(aParent);
  MOZ_ASSERT(aFile);

  RefPtr<Directory> directory = new Directory(aParent, aFile, aFileSystem);
  return directory.forget();
}

Directory::Directory(nsISupports* aParent,
                     nsIFile* aFile,
                     FileSystemBase* aFileSystem)
  : mParent(aParent)
  , mFile(aFile)
{
  MOZ_ASSERT(aFile);

  // aFileSystem can be null. In this case we create a OSFileSystem when needed.
  if (aFileSystem) {
    // More likely, this is a OSFileSystem. This object keeps a reference of
    // mParent but it's not cycle collectable and to avoid manual
    // addref/release, it's better to have 1 object per directory. For this
    // reason we clone it here.
    mFileSystem = aFileSystem->Clone();
  }
}

Directory::~Directory()
{
}

nsISupports*
Directory::GetParentObject() const
{
  return mParent;
}

JSObject*
Directory::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return DirectoryBinding::Wrap(aCx, this, aGivenProto);
}

void
Directory::GetName(nsAString& aRetval, ErrorResult& aRv)
{
  aRetval.Truncate();

  RefPtr<FileSystemBase> fs = GetFileSystem(aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  fs->GetDirectoryName(mFile, aRetval, aRv);
}

void
Directory::GetPath(nsAString& aRetval, ErrorResult& aRv)
{
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

nsresult
Directory::GetFullRealPath(nsAString& aPath)
{
  nsresult rv = mFile->GetPath(aPath);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

already_AddRefed<Promise>
Directory::GetFilesAndDirectories(ErrorResult& aRv)
{
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

already_AddRefed<Promise>
Directory::GetFiles(bool aRecursiveFlag, ErrorResult& aRv)
{
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

void
Directory::SetContentFilters(const nsAString& aFilters)
{
  mFilters = aFilters;
}

FileSystemBase*
Directory::GetFileSystem(ErrorResult& aRv)
{
  if (!mFileSystem) {
    nsAutoString path;
    aRv = mFile->GetPath(path);
    if (NS_WARN_IF(aRv.Failed())) {
      return nullptr;
    }

    RefPtr<OSFileSystem> fs = new OSFileSystem(path);
    fs->Init(mParent);

    mFileSystem = fs;
  }

  return mFileSystem;
}

} // namespace dom
} // namespace mozilla
