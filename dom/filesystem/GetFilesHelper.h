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
#include "nsThreadUtils.h"

class nsIGlobalObject;

namespace mozilla {
class ErrorResult;

namespace dom {

class BlobImpl;
class ContentParent;
class File;
class GetFilesHelperParent;
class OwningFileOrDirectory;
class Promise;

class GetFilesCallback {
 public:
  NS_INLINE_DECL_REFCOUNTING(GetFilesCallback);

  virtual void Callback(nsresult aStatus,
                        const FallibleTArray<RefPtr<BlobImpl>>& aBlobImpls) = 0;

 protected:
  virtual ~GetFilesCallback() = default;
};

class GetFilesHelperBase {
 protected:
  explicit GetFilesHelperBase(bool aRecursiveFlag)
      : mRecursiveFlag(aRecursiveFlag) {}

  virtual ~GetFilesHelperBase() = default;

  virtual bool IsCanceled() { return false; }

  nsresult ExploreDirectory(const nsAString& aDOMPath, nsIFile* aFile);

  bool mRecursiveFlag;

  // We populate this array in the I/O thread with the BlobImpl.
  FallibleTArray<RefPtr<BlobImpl>> mTargetBlobImplArray;
};

// Retrieving the list of files can be very time/IO consuming. We use this
// helper class to do it just once.
class GetFilesHelper : public Runnable, public GetFilesHelperBase {
  friend class GetFilesHelperParent;

 public:
  static already_AddRefed<GetFilesHelper> Create(
      const nsTArray<OwningFileOrDirectory>& aFilesOrDirectory,
      bool aRecursiveFlag, ErrorResult& aRv);

  void AddPromise(Promise* aPromise);

  void AddCallback(GetFilesCallback* aCallback);

  // CC methods
  void Unlink();
  void Traverse(nsCycleCollectionTraversalCallback& cb);

 protected:
  explicit GetFilesHelper(bool aRecursiveFlag);

  virtual ~GetFilesHelper();

  void SetDirectoryPath(const nsAString& aDirectoryPath) {
    mDirectoryPath = aDirectoryPath;
  }

  virtual bool IsCanceled() override {
    MutexAutoLock lock(mMutex);
    return mCanceled;
  }

  virtual void Work(ErrorResult& aRv);

  virtual void Cancel(){};

  NS_IMETHOD
  Run() override;

  void RunIO();

  void OperationCompleted();

  void ResolveOrRejectPromise(Promise* aPromise);

  void RunCallback(GetFilesCallback* aCallback);

  bool mListingCompleted;
  nsString mDirectoryPath;

  // Error code to propagate.
  nsresult mErrorResult;

  nsTArray<RefPtr<Promise>> mPromises;
  nsTArray<RefPtr<GetFilesCallback>> mCallbacks;

  Mutex mMutex MOZ_UNANNOTATED;

  // This variable is protected by mutex.
  bool mCanceled;
};

class GetFilesHelperChild final : public GetFilesHelper {
 public:
  explicit GetFilesHelperChild(bool aRecursiveFlag)
      : GetFilesHelper(aRecursiveFlag), mPendingOperation(false) {}

  virtual void Work(ErrorResult& aRv) override;

  virtual void Cancel() override;

  bool AppendBlobImpl(BlobImpl* aBlobImpl);

  void Finished(nsresult aResult);

 private:
  nsID mUUID;
  bool mPendingOperation;
};

class GetFilesHelperParentCallback;

class GetFilesHelperParent final : public GetFilesHelper {
  friend class GetFilesHelperParentCallback;

 public:
  static already_AddRefed<GetFilesHelperParent> Create(
      const nsID& aUUID, const nsAString& aDirectoryPath, bool aRecursiveFlag,
      ContentParent* aContentParent, ErrorResult& aRv);

 private:
  GetFilesHelperParent(const nsID& aUUID, ContentParent* aContentParent,
                       bool aRecursiveFlag);

  ~GetFilesHelperParent();

  RefPtr<ContentParent> mContentParent;
  nsID mUUID;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_GetFilesHelper_h
