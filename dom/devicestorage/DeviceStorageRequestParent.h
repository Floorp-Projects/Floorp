/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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
  explicit DeviceStorageRequestParent(const DeviceStorageParams& aParams);

  NS_IMETHOD_(MozExternalRefCountType) AddRef();
  NS_IMETHOD_(MozExternalRefCountType) Release();

  void Dispatch();

  virtual void ActorDestroy(ActorDestroyReason);

protected:
  ~DeviceStorageRequestParent();

private:
  ThreadSafeAutoRefCnt mRefCnt;
  NS_DECL_OWNINGTHREAD
  DeviceStorageParams mParams;

  // XXXkhuey name collision :(
  class CancelableRunnable : public Runnable
  {
  public:
    explicit CancelableRunnable(DeviceStorageRequestParent* aParent)
      : mParent(aParent)
    {
      mCanceled = !(mParent->AddRunnable(this));
    }

    virtual ~CancelableRunnable() {
    }

    NS_IMETHOD Run() override {
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
    RefPtr<DeviceStorageRequestParent> mParent;
  private:
    bool mCanceled;
  };

  class CancelableFileEvent : public CancelableRunnable
  {
  protected:
    CancelableFileEvent(DeviceStorageRequestParent* aParent,
                        already_AddRefed<DeviceStorageFile>&& aFile)
      : CancelableRunnable(aParent)
      , mFile(Move(aFile)) {}

    RefPtr<DeviceStorageFile> mFile;
  };

  class PostErrorEvent : public CancelableRunnable
  {
    public:
      PostErrorEvent(DeviceStorageRequestParent* aParent, const char* aError);
      virtual ~PostErrorEvent() {}
      virtual nsresult CancelableRun();
    private:
      nsString mError;
  };

  class PostSuccessEvent : public CancelableRunnable
  {
    public:
      explicit PostSuccessEvent(DeviceStorageRequestParent* aParent)
        : CancelableRunnable(aParent) {}
      virtual ~PostSuccessEvent() {}
      virtual nsresult CancelableRun();
  };

  class PostBlobSuccessEvent : public CancelableFileEvent
  {
    public:
      PostBlobSuccessEvent(DeviceStorageRequestParent* aParent,
                           already_AddRefed<DeviceStorageFile>&& aFile,
                           uint32_t aLength, nsACString& aMimeType,
                           uint64_t aLastModifiedDate)
        : CancelableFileEvent(aParent, Move(aFile))
        , mLength(aLength)
        , mLastModificationDate(aLastModifiedDate)
        , mMimeType(aMimeType) {}
      virtual ~PostBlobSuccessEvent() {}
      virtual nsresult CancelableRun();
    private:
      uint32_t mLength;
      uint64_t mLastModificationDate;
      nsCString mMimeType;
  };

  class PostEnumerationSuccessEvent : public CancelableRunnable
  {
    public:
      PostEnumerationSuccessEvent(DeviceStorageRequestParent* aParent,
                                  const nsAString& aStorageType,
                                  const nsAString& aRelPath,
                                  InfallibleTArray<DeviceStorageFileValue>& aPaths)
        : CancelableRunnable(aParent)
        , mStorageType(aStorageType)
        , mRelPath(aRelPath)
        , mPaths(aPaths) {}
      virtual ~PostEnumerationSuccessEvent() {}
      virtual nsresult CancelableRun();
    private:
      const nsString mStorageType;
      const nsString mRelPath;
      InfallibleTArray<DeviceStorageFileValue> mPaths;
  };

  class CreateFdEvent : public CancelableFileEvent
  {
    public:
      CreateFdEvent(DeviceStorageRequestParent* aParent,
                    already_AddRefed<DeviceStorageFile>&& aFile)
        : CancelableFileEvent(aParent, Move(aFile)) {}
      virtual ~CreateFdEvent() {}
      virtual nsresult CancelableRun();
  };

  class WriteFileEvent : public CancelableFileEvent
  {
    public:
      WriteFileEvent(DeviceStorageRequestParent* aParent,
                     already_AddRefed<DeviceStorageFile>&& aFile,
                     nsIInputStream* aInputStream, int32_t aRequestType)
        : CancelableFileEvent(aParent, Move(aFile))
        , mInputStream(aInputStream)
        , mRequestType(aRequestType) {}
      virtual ~WriteFileEvent() {}
      virtual nsresult CancelableRun();
    private:
      nsCOMPtr<nsIInputStream> mInputStream;
      int32_t mRequestType;
  };

  class DeleteFileEvent : public CancelableFileEvent
  {
    public:
      DeleteFileEvent(DeviceStorageRequestParent* aParent,
                      already_AddRefed<DeviceStorageFile>&& aFile)
        : CancelableFileEvent(aParent, Move(aFile)) {}
      virtual ~DeleteFileEvent() {}
      virtual nsresult CancelableRun();
  };

  class FreeSpaceFileEvent : public CancelableFileEvent
  {
    public:
      FreeSpaceFileEvent(DeviceStorageRequestParent* aParent,
                         already_AddRefed<DeviceStorageFile>&& aFile)
        : CancelableFileEvent(aParent, Move(aFile)) {}
      virtual ~FreeSpaceFileEvent() {}
      virtual nsresult CancelableRun();
  };

  class UsedSpaceFileEvent : public CancelableFileEvent
  {
    public:
      UsedSpaceFileEvent(DeviceStorageRequestParent* aParent,
                         already_AddRefed<DeviceStorageFile>&& aFile)
        : CancelableFileEvent(aParent, Move(aFile)) {}
      virtual ~UsedSpaceFileEvent() {}
      virtual nsresult CancelableRun();
  };

  class ReadFileEvent : public CancelableFileEvent
  {
    public:
      ReadFileEvent(DeviceStorageRequestParent* aParent,
                    already_AddRefed<DeviceStorageFile>&& aFile);
      virtual ~ReadFileEvent() {}
      virtual nsresult CancelableRun();
    private:
      nsCString mMimeType;
  };

  class EnumerateFileEvent : public CancelableFileEvent
  {
    public:
      EnumerateFileEvent(DeviceStorageRequestParent* aParent,
                         already_AddRefed<DeviceStorageFile>&& aFile,
                         uint64_t aSince)
        : CancelableFileEvent(aParent, Move(aFile))
        , mSince(aSince) {}
      virtual ~EnumerateFileEvent() {}
      virtual nsresult CancelableRun();
    private:
      uint64_t mSince;
  };

  class PostPathResultEvent : public CancelableRunnable
  {
    public:
      PostPathResultEvent(DeviceStorageRequestParent* aParent,
                          const nsAString& aPath)
        : CancelableRunnable(aParent)
        , mPath(aPath) {}
      virtual ~PostPathResultEvent() {}
      virtual nsresult CancelableRun();
    private:
      nsString mPath;
  };

  class PostFileDescriptorResultEvent : public CancelableRunnable
  {
    public:
      PostFileDescriptorResultEvent(DeviceStorageRequestParent* aParent,
                                    const FileDescriptor& aFileDescriptor)
        : CancelableRunnable(aParent)
        , mFileDescriptor(aFileDescriptor) {}
      virtual ~PostFileDescriptorResultEvent() {}
      virtual nsresult CancelableRun();
    private:
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
                               uint64_t aUsedSpace)
        : CancelableRunnable(aParent)
        , mType(aType)
        , mUsedSpace(aUsedSpace) {}
      virtual ~PostUsedSpaceResultEvent() {}
      virtual nsresult CancelableRun();
    private:
      nsString mType;
      uint64_t mUsedSpace;
 };

 class PostFormatResultEvent : public CancelableFileEvent
 {
    public:
      PostFormatResultEvent(DeviceStorageRequestParent* aParent,
                            already_AddRefed<DeviceStorageFile>&& aFile)
        : CancelableFileEvent(aParent, Move(aFile)) {}
      virtual ~PostFormatResultEvent() {}
      virtual nsresult CancelableRun();
 };

 class PostMountResultEvent : public CancelableFileEvent
 {
    public:
      PostMountResultEvent(DeviceStorageRequestParent* aParent,
                           already_AddRefed<DeviceStorageFile>&& aFile)
        : CancelableFileEvent(aParent, Move(aFile)) {}
      virtual ~PostMountResultEvent() {}
      virtual nsresult CancelableRun();
 };

 class PostUnmountResultEvent : public CancelableFileEvent
 {
    public:
      PostUnmountResultEvent(DeviceStorageRequestParent* aParent,
                             already_AddRefed<DeviceStorageFile>&& aFile)
        : CancelableFileEvent(aParent, Move(aFile)) {}
      virtual ~PostUnmountResultEvent() {}
      virtual nsresult CancelableRun();
 };

protected:
  bool AddRunnable(CancelableRunnable* aRunnable) {
    MutexAutoLock lock(mMutex);
    if (mActorDestroyed)
      return false;

    mRunnables.AppendElement(aRunnable);
    return true;
  }

  void RemoveRunnable(CancelableRunnable* aRunnable) {
    MutexAutoLock lock(mMutex);
    mRunnables.RemoveElement(aRunnable);
  }

  Mutex mMutex;
  bool mActorDestroyed;
  nsTArray<RefPtr<CancelableRunnable> > mRunnables;
};

} // namespace devicestorage
} // namespace dom
} // namespace mozilla

#endif
