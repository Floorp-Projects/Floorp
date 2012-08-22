/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_devicestorage_DeviceStorageRequestParent_h
#define mozilla_dom_devicestorage_DeviceStorageRequestParent_h

#include "mozilla/dom/devicestorage/PDeviceStorageRequestParent.h"
#include "mozilla/dom/ContentChild.h"

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
  virtual void ActorDestroy(ActorDestroyReason);

protected:
  ~DeviceStorageRequestParent();

private:
  nsAutoRefCnt mRefCnt;

  class CancelableRunnable : public nsRunnable
  {
  public:
    CancelableRunnable(DeviceStorageRequestParent* aParent)
      : mParent(aParent)
      , mCanceled(false)
    {
      mParent->AddRunnable(this);
    }

    virtual ~CancelableRunnable() {
    }

    NS_IMETHOD Run() {
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
      PostBlobSuccessEvent(DeviceStorageRequestParent* aParent, DeviceStorageFile* aFile, uint32_t aLength, nsACString& aMimeType);
      virtual ~PostBlobSuccessEvent();
      virtual nsresult CancelableRun();
    private:
      uint32_t mLength;
      nsRefPtr<DeviceStorageFile> mFile;
      nsCString mMimeType;
  };

  class PostEnumerationSuccessEvent : public CancelableRunnable
  {
    public:
      PostEnumerationSuccessEvent(DeviceStorageRequestParent* aParent, InfallibleTArray<DeviceStorageFileValue>& aPaths);
      virtual ~PostEnumerationSuccessEvent();
      virtual nsresult CancelableRun();
    private:
      InfallibleTArray<DeviceStorageFileValue> mPaths;
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

  class StatFileEvent : public CancelableRunnable
  {
    public:
      StatFileEvent(DeviceStorageRequestParent* aParent, DeviceStorageFile* aFile);
      virtual ~StatFileEvent();
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

  class PostStatResultEvent : public CancelableRunnable
 {
    public:
      PostStatResultEvent(DeviceStorageRequestParent* aParent,
                          int64_t aFreeBytes,
                          int64_t aTotalBytes);
      virtual ~PostStatResultEvent();
      virtual nsresult CancelableRun();
    private:
      int64_t mFreeBytes, mTotalBytes;
   };

protected:
  void AddRunnable(CancelableRunnable* aRunnable) {
    mRunnables.AppendElement(aRunnable);
  }
  void RemoveRunnable(CancelableRunnable* aRunnable) {
    mRunnables.RemoveElement(aRunnable);
  }
  nsTArray<nsRefPtr<CancelableRunnable> > mRunnables;
};

} // namespace devicestorage
} // namespace dom
} // namespace mozilla

#endif
