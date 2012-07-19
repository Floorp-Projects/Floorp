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

namespace mozilla {
namespace dom {
namespace devicestorage {

class DeviceStorageRequestParent : public PDeviceStorageRequestParent
{
public:
  DeviceStorageRequestParent(const DeviceStorageParams& aParams);
  ~DeviceStorageRequestParent();

private:
  class PostErrorEvent : public nsRunnable
  {
    public:
      PostErrorEvent(DeviceStorageRequestParent* aParent, const char* aError);
      ~PostErrorEvent();
      NS_IMETHOD Run();
    private:
      DeviceStorageRequestParent* mParent;
      nsString mError;
  };

  class PostSuccessEvent : public nsRunnable
  {
    public:
      PostSuccessEvent(DeviceStorageRequestParent* aParent);
      ~PostSuccessEvent();
      NS_IMETHOD Run();
    private:
      DeviceStorageRequestParent* mParent;
  };

  class PostBlobSuccessEvent : public nsRunnable
  {
    public:
      PostBlobSuccessEvent(DeviceStorageRequestParent* aParent, void* aBuffer, PRUint32 aLength, nsACString& aMimeType);
      ~PostBlobSuccessEvent();
      NS_IMETHOD Run();
    private:
      DeviceStorageRequestParent* mParent;
      InfallibleTArray<PRUint8> mBits;
      nsCString mMimeType;
  };

  class PostEnumerationSuccessEvent : public nsRunnable
  {
    public:
      PostEnumerationSuccessEvent(DeviceStorageRequestParent* aParent, InfallibleTArray<DeviceStorageFileValue>& aPaths);
      ~PostEnumerationSuccessEvent();
      NS_IMETHOD Run();
    private:
      DeviceStorageRequestParent* mParent;
      InfallibleTArray<DeviceStorageFileValue> mPaths;
  };

  class WriteFileEvent : public nsRunnable
  {
    public:
      WriteFileEvent(DeviceStorageRequestParent* aParent, DeviceStorageFile* aFile, InfallibleTArray<PRUint8>& aBits);
      ~WriteFileEvent();
      NS_IMETHOD Run();
    private:
      DeviceStorageRequestParent* mParent;
      nsRefPtr<DeviceStorageFile> mFile;
    InfallibleTArray<PRUint8> mBits; // another copy?
  };

  class DeleteFileEvent : public nsRunnable
  {
    public:
      DeleteFileEvent(DeviceStorageRequestParent* aParent, DeviceStorageFile* aFile);
      ~DeleteFileEvent();
      NS_IMETHOD Run();
    private:
      DeviceStorageRequestParent* mParent;
      nsRefPtr<DeviceStorageFile> mFile;
  };

  class ReadFileEvent : public nsRunnable
  {
    public:
      ReadFileEvent(DeviceStorageRequestParent* aParent, DeviceStorageFile* aFile);
      ~ReadFileEvent();
      NS_IMETHOD Run();
    private:
      DeviceStorageRequestParent* mParent;
      nsRefPtr<DeviceStorageFile> mFile;
      nsCString mMimeType;
  };

  class EnumerateFileEvent : public nsRunnable
  {
    public:
      EnumerateFileEvent(DeviceStorageRequestParent* aParent, DeviceStorageFile* aFile, PRUint32 aSince);
      ~EnumerateFileEvent();
      NS_IMETHOD Run();
    private:
      DeviceStorageRequestParent* mParent;
      nsRefPtr<DeviceStorageFile> mFile;
      PRUint32 mSince;
  };

  class PostPathResultEvent : public nsRunnable
  {
    public:
      PostPathResultEvent(DeviceStorageRequestParent* aParent, const nsAString& aPath);
      ~PostPathResultEvent();
      NS_IMETHOD Run();
    private:
      DeviceStorageRequestParent* mParent;
      nsRefPtr<DeviceStorageFile> mFile;
      InfallibleTArray<PRUint8> mBits;
      nsString mPath;
  };

};

} // namespace devicestorage
} // namespace dom
} // namespace mozilla

#endif
