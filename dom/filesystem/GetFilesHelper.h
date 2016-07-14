/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_GetFilesHelper_h
#define mozilla_dom_GetFilesHelper_h

#include "mozilla/Mutex.h"
#include "mozilla/RefPtr.h"
#include "nsCycleCollectionTraversalCallback.h"
#include "nsTArray.h"

class nsIGlobalObject;

namespace mozilla {
namespace dom {

class File;
class GetFilesHelperParent;
class OwningFileOrDirectory;
class Promise;

class GetFilesCallback
{
public:
  NS_INLINE_DECL_REFCOUNTING(GetFilesCallback);

  virtual void
  Callback(nsresult aStatus, const Sequence<RefPtr<File>>& aFiles) = 0;

protected:
  virtual ~GetFilesCallback() {}
};

// Retrieving the list of files can be very time/IO consuming. We use this
// helper class to do it just once.
class GetFilesHelper : public Runnable
{
  friend class GetFilesHelperParent;

public:
  static already_AddRefed<GetFilesHelper>
  Create(nsIGlobalObject* aGlobal,
         const nsTArray<OwningFileOrDirectory>& aFilesOrDirectory,
         bool aRecursiveFlag, ErrorResult& aRv);

  void
  AddPromise(Promise* aPromise);

  void
  AddCallback(GetFilesCallback* aCallback);

  // CC methods
  void Unlink();
  void Traverse(nsCycleCollectionTraversalCallback &cb);

protected:
  GetFilesHelper(nsIGlobalObject* aGlobal, bool aRecursiveFlag);

  virtual ~GetFilesHelper() {}

  void
  SetDirectoryPath(const nsAString& aDirectoryPath)
  {
    mDirectoryPath = aDirectoryPath;
  }

  bool
  IsCanceled()
  {
    MutexAutoLock lock(mMutex);
    return mCanceled;
  }

  NS_IMETHOD
  Run() override;

  void
  RunIO();

  void
  RunMainThread();

  nsresult
  ExploreDirectory(const nsAString& aDOMPath, nsIFile* aFile);

  void
  ResolveOrRejectPromise(Promise* aPromise);

  void
  RunCallback(GetFilesCallback* aCallback);

  nsCOMPtr<nsIGlobalObject> mGlobal;

  bool mRecursiveFlag;
  bool mListingCompleted;
  nsString mDirectoryPath;

  // We populate this array in the I/O thread with the paths of the Files that
  // we want to send as result to the promise objects.
  struct FileData {
    nsString mDomPath;
    nsString mRealPath;
  };
  FallibleTArray<FileData> mTargetPathArray;

  // This is the real File sequence that we expose via Promises.
  Sequence<RefPtr<File>> mFiles;

  // Error code to propagate.
  nsresult mErrorResult;

  nsTArray<RefPtr<Promise>> mPromises;
  nsTArray<RefPtr<GetFilesCallback>> mCallbacks;

  Mutex mMutex;

  // This variable is protected by mutex.
  bool mCanceled;
};

} // dom namespace
} // mozilla namespace

#endif // mozilla_dom_GetFilesHelper_h
