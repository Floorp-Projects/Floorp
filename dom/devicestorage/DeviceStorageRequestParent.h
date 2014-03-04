/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_devicestorage_DeviceStorageRequestParent_h
#define mozilla_dom_devicestorage_DeviceStorageRequestParent_h

#include "mozilla/Attributes.h"
#include "mozilla/dom/devicestorage/PDeviceStorageRequestParent.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/ContentParent.h"

#include "nsThreadUtils.h"
#include "nsDeviceStorage.h"
#include "nsTArray.h"

namespace mozilla {
namespace dom {
namespace devicestorage {

class DeviceStorageRequestParent : public PDeviceStorageRequestParent
{
public:
  DeviceStorageRequestParent(const DeviceStorageParams& aParams);

  NS_IMETHOD_(nsrefcnt) AddRef();
  NS_IMETHOD_(nsrefcnt) Release();

  bool EnsureRequiredPermissions(mozilla::dom::ContentParent* aParent);
  void Dispatch();

  virtual void ActorDestroy(ActorDestroyReason);

protected:
  ~DeviceStorageRequestParent();

private:
  ThreadSafeAutoRefCnt mRefCnt;
  NS_DECL_OWNINGTHREAD
  DeviceStorageParams mParams;

  class CancelableRunnable : public nsRunnable
  {
  public:
    CancelableRunnable(DeviceStorageRequestParent* aParent)
      : mParent(aParent)
    {
      mCanceled = !(mParent->AddRunnable(this));
    }

    virtual ~CancelableRunnable() {
    }

    NS_IMETHOD Run() MOZ_OVERRIDE {
      nsresult rv = NS_OK;
      if (!mCanceled) {
        rv = CancelableRun();
        mParent->RemoveRunnable(this);
      }
      return rv;
    }

    void Cancel() {
      mCanceled = true;
    }

    virtual nsresult CancelableRun() = 0;

  protected:
    nsRefPtr<DeviceStorageRequestParent> mParent;
  private:
    bool mCanceled;
  };

  class PostErrorEvent : public CancelableRunnable
  {
    public:
      PostErrorEvent(DeviceStorageRequestParent* aParent, const char* aError);
      virtual ~PostErrorEvent();
      virtual nsresult CancelableRun();
    private:
      nsString mError;
  };

  class PostSuccessEvent : public CancelableRunnable
  {
    public:
      PostSuccessEvent(DeviceStorageRequestParent* aParent);
      virtual ~PostSuccessEvent();
      virtual nsresult CancelableRun();
  };

  class PostBlobSuccessEvent : public CancelableRunnable
  {
    public:
      PostBlobSuccessEvent(DeviceStorageRequestParent* aParent, DeviceStorageFile* aFile, uint32_t aLength, nsACString& aMimeType, uint64_t aLastModifiedDate);
      virtual ~PostBlobSuccessEvent();
      virtual nsresult CancelableRun();
    private:
      uint32_t mLength;
      uint64_t mLastModificationDate;
      nsRefPtr<DeviceStorageFile> mFile;
      nsCString mMimeType;
  };

  class PostEnumerationSuccessEvent : public CancelableRunnable
  {
    public:
      PostEnumerationSuccessEvent(DeviceStorageRequestParent* aParent,
                                  const nsAString& aStorageType,
                                  const nsAString& aRelPath,
                                  InfallibleTArray<DeviceStorageFileValue>& aPaths);
      virtual ~PostEnumerationSuccessEvent();
      virtual nsresult CancelableRun();
    private:
      const nsString mStorageType;
      const nsString mRelPath;
      InfallibleTArray<DeviceStorageFileValue> mPaths;
  };

  class CreateFdEvent : public CancelableRunnable
  {
    public:
      CreateFdEvent(DeviceStorageRequestParent* aParent, DeviceStorageFile* aFile);
      virtual ~CreateFdEvent();
      virtual nsresult CancelableRun();
    private:
      nsRefPtr<DeviceStorageFile> mFile;
  };

  class WriteFileEvent : public CancelableRunnable
  {
    public:
      WriteFileEvent(DeviceStorageRequestParent* aParent, DeviceStorageFile* aFile, nsIInputStream* aInputStream);
      virtual ~WriteFileEvent();
      virtual nsresult CancelableRun();
    private:
      nsRefPtr<DeviceStorageFile> mFile;
      nsCOMPtr<nsIInputStream> mInputStream;
  };

  class DeleteFileEvent : public CancelableRunnable
  {
    public:
      DeleteFileEvent(DeviceStorageRequestParent* aParent, DeviceStorageFile* aFile);
      virtual ~DeleteFileEvent();
      virtual nsresult CancelableRun();
    private:
      nsRefPtr<DeviceStorageFile> mFile;
  };

  class FreeSpaceFileEvent : public CancelableRunnable
  {
    public:
      FreeSpaceFileEvent(DeviceStorageRequestParent* aParent, DeviceStorageFile* aFile);
      virtual ~FreeSpaceFileEvent();
      virtual nsresult CancelableRun();
     private:
       nsRefPtr<DeviceStorageFile> mFile;
  };

  class UsedSpaceFileEvent : public CancelableRunnable
  {
    public:
      UsedSpaceFileEvent(DeviceStorageRequestParent* aParent, DeviceStorageFile* aFile);
      virtual ~UsedSpaceFileEvent();
      virtual nsresult CancelableRun();
     private:
       nsRefPtr<DeviceStorageFile> mFile;
  };

  class ReadFileEvent : public CancelableRunnable
  {
    public:
      ReadFileEvent(DeviceStorageRequestParent* aParent, DeviceStorageFile* aFile);
      virtual ~ReadFileEvent();
      virtual nsresult CancelableRun();
    private:
      nsRefPtr<DeviceStorageFile> mFile;
      nsCString mMimeType;
  };

  class EnumerateFileEvent : public CancelableRunnable
  {
    public:
      EnumerateFileEvent(DeviceStorageRequestParent* aParent, DeviceStorageFile* aFile, uint64_t aSince);
      virtual ~EnumerateFileEvent();
      virtual nsresult CancelableRun();
    private:
      nsRefPtr<DeviceStorageFile> mFile;
      uint64_t mSince;
  };

  class PostPathResultEvent : public CancelableRunnable
  {
    public:
      PostPathResultEvent(DeviceStorageRequestParent* aParent, const nsAString& aPath);
      virtual ~PostPathResultEvent();
      virtual nsresult CancelableRun();
    private:
      nsRefPtr<DeviceStorageFile> mFile;
      nsString mPath;
  };

  class PostFileDescriptorResultEvent : public CancelableRunnable
  {
    public:
      PostFileDescriptorResultEvent(DeviceStorageRequestParent* aParent,
                                    const FileDescriptor& aFileDescriptor);
      virtual ~PostFileDescriptorResultEvent();
      virtual nsresult CancelableRun();
    private:
      nsRefPtr<DeviceStorageFile> mFile;
      FileDescriptor mFileDescriptor;
  };

 class PostFreeSpaceResultEvent : public CancelableRunnable
 {
    public:
      PostFreeSpaceResultEvent(DeviceStorageRequestParent* aParent,
                               uint64_t aFreeSpace);
      virtual ~PostFreeSpaceResultEvent();
      virtual nsresult CancelableRun();
    private:
      uint64_t mFreeSpace;
 };

 class PostUsedSpaceResultEvent : public CancelableRunnable
 {
    public:
      PostUsedSpaceResultEvent(DeviceStorageRequestParent* aParent,
                               const nsAString& aType,
                               uint64_t aUsedSpace);
      virtual ~PostUsedSpaceResultEvent();
      virtual nsresult CancelableRun();
    private:
      nsString mType;
      uint64_t mUsedSpace;
 };

 class PostAvailableResultEvent : public CancelableRunnable
 {
    public:
      PostAvailableResultEvent(DeviceStorageRequestParent* aParent, DeviceStorageFile* aFile);
      virtual ~PostAvailableResultEvent();
      virtual nsresult CancelableRun();
    private:
      nsRefPtr<DeviceStorageFile> mFile;
 };

 class PostStatusResultEvent : public CancelableRunnable
 {
    public:
      PostStatusResultEvent(DeviceStorageRequestParent* aParent, DeviceStorageFile* aFile);
      virtual ~PostStatusResultEvent();
      virtual nsresult CancelableRun();
    private:
      nsRefPtr<DeviceStorageFile> mFile;
 };

 class PostFormatResultEvent : public CancelableRunnable
 {
    public:
      PostFormatResultEvent(DeviceStorageRequestParent* aParent, DeviceStorageFile* aFile);
      virtual ~PostFormatResultEvent();
      virtual nsresult CancelableRun();
    private:
      nsRefPtr<DeviceStorageFile> mFile;
 };

 class PostMountResultEvent : public CancelableRunnable
 {
    public:
      PostMountResultEvent(DeviceStorageRequestParent* aParent, DeviceStorageFile* aFile);
      virtual ~PostMountResultEvent();
      virtual nsresult CancelableRun();
    private:
      nsRefPtr<DeviceStorageFile> mFile;
 };

 class PostUnmountResultEvent : public CancelableRunnable
 {
    public:
      PostUnmountResultEvent(DeviceStorageRequestParent* aParent, DeviceStorageFile* aFile);
      virtual ~PostUnmountResultEvent();
      virtual nsresult CancelableRun();
    private:
      nsRefPtr<DeviceStorageFile> mFile;
 };

protected:
  bool AddRunnable(CancelableRunnable* aRunnable) {
    MutexAutoLock lock(mMutex);
    if (mActorDestoryed)
      return false;

    mRunnables.AppendElement(aRunnable);
    return true;
  }

  void RemoveRunnable(CancelableRunnable* aRunnable) {
    MutexAutoLock lock(mMutex);
    mRunnables.RemoveElement(aRunnable);
  }

  Mutex mMutex;
  bool mActorDestoryed;
  nsTArray<nsRefPtr<CancelableRunnable> > mRunnables;
};

} // namespace devicestorage
} // namespace dom
} // namespace mozilla

#endif
