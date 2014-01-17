/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDeviceStorage_h
#define nsDeviceStorage_h

class nsPIDOMWindow;
#include "mozilla/Attributes.h"
#include "PCOMContentPermissionRequestChild.h"

#include "DOMRequest.h"
#include "DOMCursor.h"
#include "nsAutoPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsDOMClassInfoID.h"
#include "nsIClassInfo.h"
#include "nsIContentPermissionPrompt.h"
#include "nsIDOMWindow.h"
#include "nsIURI.h"
#include "nsInterfaceHashtable.h"
#include "nsIPrincipal.h"
#include "nsString.h"
#include "nsWeakPtr.h"
#include "nsIDOMEventListener.h"
#include "nsIObserver.h"
#include "nsIStringBundle.h"
#include "mozilla/Mutex.h"
#include "prtime.h"
#include "DeviceStorage.h"
#include "mozilla/dom/devicestorage/DeviceStorageRequestChild.h"
#include "mozilla/RefPtr.h"
#include "mozilla/StaticPtr.h"

namespace mozilla {
class ErrorResult;
} // namespace mozilla

#define POST_ERROR_EVENT_FILE_EXISTS                 "NoModificationAllowedError"
#define POST_ERROR_EVENT_FILE_DOES_NOT_EXIST         "NotFoundError"
#define POST_ERROR_EVENT_FILE_NOT_ENUMERABLE         "TypeMismatchError"
#define POST_ERROR_EVENT_PERMISSION_DENIED           "SecurityError"
#define POST_ERROR_EVENT_ILLEGAL_TYPE                "TypeMismatchError"
#define POST_ERROR_EVENT_UNKNOWN                     "Unknown"

enum DeviceStorageRequestType {
    DEVICE_STORAGE_REQUEST_READ,
    DEVICE_STORAGE_REQUEST_WRITE,
    DEVICE_STORAGE_REQUEST_CREATE,
    DEVICE_STORAGE_REQUEST_DELETE,
    DEVICE_STORAGE_REQUEST_WATCH,
    DEVICE_STORAGE_REQUEST_FREE_SPACE,
    DEVICE_STORAGE_REQUEST_USED_SPACE,
    DEVICE_STORAGE_REQUEST_AVAILABLE,
    DEVICE_STORAGE_REQUEST_FORMAT
};

class DeviceStorageUsedSpaceCache MOZ_FINAL
{
public:
  static DeviceStorageUsedSpaceCache* CreateOrGet();

  DeviceStorageUsedSpaceCache();
  ~DeviceStorageUsedSpaceCache();


  class InvalidateRunnable MOZ_FINAL : public nsRunnable
  {
    public:
      InvalidateRunnable(DeviceStorageUsedSpaceCache* aCache, 
                         const nsAString& aStorageName)
        : mCache(aCache)
        , mStorageName(aStorageName) {}

      ~InvalidateRunnable() {}

      NS_IMETHOD Run() MOZ_OVERRIDE
      {
        mozilla::RefPtr<DeviceStorageUsedSpaceCache::CacheEntry> cacheEntry;
        cacheEntry = mCache->GetCacheEntry(mStorageName);
        if (cacheEntry) {
          cacheEntry->mDirty = true;
        }
        return NS_OK;
      }
    private:
      DeviceStorageUsedSpaceCache* mCache;
      nsString mStorageName;
  };

  void Invalidate(const nsAString& aStorageName)
  {
    NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
    NS_ASSERTION(mIOThread, "Null mIOThread!");

    nsRefPtr<InvalidateRunnable> r = new InvalidateRunnable(this, aStorageName);
    mIOThread->Dispatch(r, NS_DISPATCH_NORMAL);
  }

  void Dispatch(nsIRunnable* aRunnable)
  {
    NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
    NS_ASSERTION(mIOThread, "Null mIOThread!");

    mIOThread->Dispatch(aRunnable, NS_DISPATCH_NORMAL);
  }

  nsresult AccumUsedSizes(const nsAString& aStorageName,
                          uint64_t* aPictureSize, uint64_t* aVideosSize,
                          uint64_t* aMusicSize, uint64_t* aTotalSize);

  void SetUsedSizes(const nsAString& aStorageName,
                    uint64_t aPictureSize, uint64_t aVideosSize,
                    uint64_t aMusicSize, uint64_t aTotalSize);

private:
  friend class InvalidateRunnable;

  class CacheEntry : public mozilla::RefCounted<CacheEntry> 
  {
  public:
    bool mDirty;
    nsString mStorageName;
    int64_t  mFreeBytes;
    uint64_t mPicturesUsedSize;
    uint64_t mVideosUsedSize;
    uint64_t mMusicUsedSize;
    uint64_t mTotalUsedSize;
  };
  mozilla::TemporaryRef<CacheEntry> GetCacheEntry(const nsAString& aStorageName);

  nsTArray<mozilla::RefPtr<CacheEntry> > mCacheEntries;

  nsCOMPtr<nsIThread> mIOThread;

  static mozilla::StaticAutoPtr<DeviceStorageUsedSpaceCache> sDeviceStorageUsedSpaceCache;
};

class DeviceStorageTypeChecker MOZ_FINAL
{
public:
  static DeviceStorageTypeChecker* CreateOrGet();

  DeviceStorageTypeChecker();
  ~DeviceStorageTypeChecker();

  void InitFromBundle(nsIStringBundle* aBundle);

  bool Check(const nsAString& aType, nsIDOMBlob* aBlob);
  bool Check(const nsAString& aType, nsIFile* aFile);
  void GetTypeFromFile(nsIFile* aFile, nsAString& aType);
  void GetTypeFromFileName(const nsAString& aFileName, nsAString& aType);

  static nsresult GetPermissionForType(const nsAString& aType, nsACString& aPermissionResult);
  static nsresult GetAccessForRequest(const DeviceStorageRequestType aRequestType, nsACString& aAccessResult);
  static bool IsVolumeBased(const nsAString& aType);

private:
  nsString mPicturesExtensions;
  nsString mVideosExtensions;
  nsString mMusicExtensions;

  static mozilla::StaticAutoPtr<DeviceStorageTypeChecker> sDeviceStorageTypeChecker;
};

class ContinueCursorEvent MOZ_FINAL : public nsRunnable
{
public:
  ContinueCursorEvent(already_AddRefed<mozilla::dom::DOMRequest> aRequest);
  ContinueCursorEvent(mozilla::dom::DOMRequest* aRequest);
  ~ContinueCursorEvent();
  void Continue();

  NS_IMETHOD Run() MOZ_OVERRIDE;
private:
  already_AddRefed<DeviceStorageFile> GetNextFile();
  nsRefPtr<mozilla::dom::DOMRequest> mRequest;
};

class nsDOMDeviceStorageCursor MOZ_FINAL
  : public mozilla::dom::DOMCursor
  , public nsIContentPermissionRequest
  , public PCOMContentPermissionRequestChild
  , public mozilla::dom::devicestorage::DeviceStorageRequestChildCallback
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSICONTENTPERMISSIONREQUEST
  NS_FORWARD_NSIDOMDOMCURSOR(mozilla::dom::DOMCursor::)

  // DOMCursor
  virtual void Continue(mozilla::ErrorResult& aRv) MOZ_OVERRIDE;

  nsDOMDeviceStorageCursor(nsPIDOMWindow* aWindow,
                           nsIPrincipal* aPrincipal,
                           DeviceStorageFile* aFile,
                           PRTime aSince);


  nsTArray<nsRefPtr<DeviceStorageFile> > mFiles;
  bool mOkToCallContinue;
  PRTime mSince;

  virtual bool Recv__delete__(const bool& allow) MOZ_OVERRIDE;
  virtual void IPDLRelease() MOZ_OVERRIDE;

  void GetStorageType(nsAString & aType);

  void RequestComplete() MOZ_OVERRIDE;

private:
  ~nsDOMDeviceStorageCursor();

  nsRefPtr<DeviceStorageFile> mFile;
  nsCOMPtr<nsIPrincipal> mPrincipal;
};

//helpers
JS::Value
StringToJsval(nsPIDOMWindow* aWindow, nsAString& aString);

JS::Value
nsIFileToJsval(nsPIDOMWindow* aWindow, DeviceStorageFile* aFile);

JS::Value
InterfaceToJsval(nsPIDOMWindow* aWindow, nsISupports* aObject, const nsIID* aIID);

#endif
