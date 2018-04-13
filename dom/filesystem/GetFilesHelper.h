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
#include "nsTHashtable.h"

class nsIGlobalObject;

namespace mozilla {
namespace dom {

class BlobImpl;
class ContentParent;
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

class GetFilesHelperBase
{
protected:
  explicit GetFilesHelperBase(bool aRecursiveFlag)
    : mRecursiveFlag(aRecursiveFlag)
  {}

  virtual ~GetFilesHelperBase() {}

  virtual bool
  IsCanceled()
  {
    return false;
  }

  nsresult
  ExploreDirectory(const nsAString& aDOMPath, nsIFile* aFile);

  nsresult
  AddExploredDirectory(nsIFile* aDirectory);

  bool
  ShouldFollowSymLink(nsIFile* aDirectory);

  bool mRecursiveFlag;

  // We populate this array in the I/O thread with the BlobImpl.
  FallibleTArray<RefPtr<BlobImpl>> mTargetBlobImplArray;
  nsTHashtable<nsStringHashKey> mExploredDirectories;
};

// Retrieving the list of files can be very time/IO consuming. We use this
// helper class to do it just once.
class GetFilesHelper : public Runnable
                     , public GetFilesHelperBase
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

  virtual ~GetFilesHelper();

  void
  SetDirectoryPath(const nsAString& aDirectoryPath)
  {
    mDirectoryPath = aDirectoryPath;
  }

  virtual bool
  IsCanceled() override
  {
    MutexAutoLock lock(mMutex);
    return mCanceled;
  }

  virtual void
  Work(ErrorResult& aRv);

  virtual void
  Cancel() {};

  NS_IMETHOD
  Run() override;

  void
  RunIO();

  void
  RunMainThread();

  void
  OperationCompleted();

  void
  ResolveOrRejectPromise(Promise* aPromise);

  void
  RunCallback(GetFilesCallback* aCallback);

  nsCOMPtr<nsIGlobalObject> mGlobal;

  bool mListingCompleted;
  nsString mDirectoryPath;

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

class GetFilesHelperChild final : public GetFilesHelper
{
public:
  GetFilesHelperChild(nsIGlobalObject* aGlobal, bool aRecursiveFlag)
    : GetFilesHelper(aGlobal, aRecursiveFlag)
    , mPendingOperation(false)
  {}

  virtual void
  Work(ErrorResult& aRv) override;

  virtual void
  Cancel() override;

  bool
  AppendBlobImpl(BlobImpl* aBlobImpl);

  void
  Finished(nsresult aResult);

private:
  nsID mUUID;
  bool mPendingOperation;
};

class GetFilesHelperParentCallback;

class GetFilesHelperParent final : public GetFilesHelper
{
  friend class GetFilesHelperParentCallback;

public:
  static already_AddRefed<GetFilesHelperParent>
  Create(const nsID& aUUID, const nsAString& aDirectoryPath,
         bool aRecursiveFlag, ContentParent* aContentParent, ErrorResult& aRv);

private:
  GetFilesHelperParent(const nsID& aUUID, ContentParent* aContentParent,
                       bool aRecursiveFlag);

  ~GetFilesHelperParent();

  RefPtr<ContentParent> mContentParent;
  nsID mUUID;
};

} // dom namespace
} // mozilla namespace

#endif // mozilla_dom_GetFilesHelper_h
