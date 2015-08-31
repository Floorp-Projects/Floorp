/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDeviceStorage_h
#define nsDeviceStorage_h

class nsPIDOMWindow;
#include "mozilla/Atomics.h"
#include "mozilla/Attributes.h"
#include "mozilla/Logging.h"
#include "mozilla/dom/devicestorage/DeviceStorageRequestChild.h"

#include "DOMRequest.h"
#include "DOMCursor.h"
#include "nsAutoPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsDOMClassInfoID.h"
#include "nsIClassInfo.h"
#include "nsIDOMWindow.h"
#include "nsIURI.h"
#include "nsIPrincipal.h"
#include "nsString.h"
#include "nsWeakPtr.h"
#include "nsIDOMEventListener.h"
#include "nsIObserver.h"
#include "nsIStringBundle.h"
#include "mozilla/Mutex.h"
#include "prtime.h"
#include "DeviceStorage.h"
#include "mozilla/StaticPtr.h"

namespace mozilla {
class ErrorResult;

namespace dom {
class BlobImpl;
class DeviceStorageParams;
} // namespace dom
} // namespace mozilla

class nsDOMDeviceStorage;
class DeviceStorageCursorRequest;

//#define DS_LOGGING 1

#ifdef DS_LOGGING
// FIXME -- use MOZ_LOG and set to warn by default
#define DS_LOG_DEBUG(msg, ...)  printf_stderr("[%s:%d] " msg "\n", __func__, __LINE__, ##__VA_ARGS__)
#define DS_LOG_INFO DS_LOG_DEBUG
#define DS_LOG_WARN DS_LOG_DEBUG
#define DS_LOG_ERROR DS_LOG_DEBUG
#else
#define DS_LOG_DEBUG(msg, ...)
#define DS_LOG_INFO(msg, ...)
#define DS_LOG_WARN(msg, ...)
#define DS_LOG_ERROR(msg, ...)
#endif

#define POST_ERROR_EVENT_FILE_EXISTS                 "NoModificationAllowedError"
#define POST_ERROR_EVENT_FILE_DOES_NOT_EXIST         "NotFoundError"
#define POST_ERROR_EVENT_FILE_NOT_ENUMERABLE         "TypeMismatchError"
#define POST_ERROR_EVENT_PERMISSION_DENIED           "SecurityError"
#define POST_ERROR_EVENT_ILLEGAL_TYPE                "TypeMismatchError"
#define POST_ERROR_EVENT_UNKNOWN                     "Unknown"

enum DeviceStorageRequestType {
  DEVICE_STORAGE_REQUEST_READ,
  DEVICE_STORAGE_REQUEST_WRITE,
  DEVICE_STORAGE_REQUEST_APPEND,
  DEVICE_STORAGE_REQUEST_CREATE,
  DEVICE_STORAGE_REQUEST_DELETE,
  DEVICE_STORAGE_REQUEST_WATCH,
  DEVICE_STORAGE_REQUEST_FREE_SPACE,
  DEVICE_STORAGE_REQUEST_USED_SPACE,
  DEVICE_STORAGE_REQUEST_AVAILABLE,
  DEVICE_STORAGE_REQUEST_STATUS,
  DEVICE_STORAGE_REQUEST_FORMAT,
  DEVICE_STORAGE_REQUEST_MOUNT,
  DEVICE_STORAGE_REQUEST_UNMOUNT,
  DEVICE_STORAGE_REQUEST_CREATEFD,
  DEVICE_STORAGE_REQUEST_CURSOR
};

enum DeviceStorageAccessType {
  DEVICE_STORAGE_ACCESS_READ,
  DEVICE_STORAGE_ACCESS_WRITE,
  DEVICE_STORAGE_ACCESS_CREATE,
  DEVICE_STORAGE_ACCESS_UNDEFINED,
  DEVICE_STORAGE_ACCESS_COUNT
};

class DeviceStorageUsedSpaceCache final
{
public:
  static DeviceStorageUsedSpaceCache* CreateOrGet();

  DeviceStorageUsedSpaceCache();
  ~DeviceStorageUsedSpaceCache();


  class InvalidateRunnable final : public nsRunnable
  {
    public:
      InvalidateRunnable(DeviceStorageUsedSpaceCache* aCache, 
                         const nsAString& aStorageName)
        : mCache(aCache)
        , mStorageName(aStorageName) {}

      ~InvalidateRunnable() {}

      NS_IMETHOD Run() override
      {
        nsRefPtr<DeviceStorageUsedSpaceCache::CacheEntry> cacheEntry;
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
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(mIOThread);

    nsRefPtr<InvalidateRunnable> r = new InvalidateRunnable(this, aStorageName);
    mIOThread->Dispatch(r, NS_DISPATCH_NORMAL);
  }

  void Dispatch(nsIRunnable* aRunnable)
  {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(mIOThread);

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

  struct CacheEntry
  {
    // Technically, this doesn't need to be threadsafe, but the implementation
    // of the non-thread safe one causes ASSERTS due to the underlying thread
    // associated with a LazyIdleThread changing from time to time.
    NS_INLINE_DECL_THREADSAFE_REFCOUNTING(CacheEntry)

    bool mDirty;
    nsString mStorageName;
    int64_t  mFreeBytes;
    uint64_t mPicturesUsedSize;
    uint64_t mVideosUsedSize;
    uint64_t mMusicUsedSize;
    uint64_t mTotalUsedSize;

  private:
    ~CacheEntry() {}
  };
  already_AddRefed<CacheEntry> GetCacheEntry(const nsAString& aStorageName);

  nsTArray<nsRefPtr<CacheEntry>> mCacheEntries;

  nsCOMPtr<nsIThread> mIOThread;

  static mozilla::StaticAutoPtr<DeviceStorageUsedSpaceCache> sDeviceStorageUsedSpaceCache;
};

class DeviceStorageTypeChecker final
{
public:
  static DeviceStorageTypeChecker* CreateOrGet();

  DeviceStorageTypeChecker();
  ~DeviceStorageTypeChecker();

  void InitFromBundle(nsIStringBundle* aBundle);

  bool Check(const nsAString& aType, mozilla::dom::BlobImpl* aBlob);
  bool Check(const nsAString& aType, nsIFile* aFile);
  bool Check(const nsAString& aType, const nsString& aPath);
  void GetTypeFromFile(nsIFile* aFile, nsAString& aType);
  void GetTypeFromFileName(const nsAString& aFileName, nsAString& aType);
  static nsresult GetPermissionForType(const nsAString& aType,
                                       nsACString& aPermissionResult);
  static nsresult GetAccessForRequest(const DeviceStorageRequestType aRequestType,
                                      nsACString& aAccessResult);
  static nsresult GetAccessForIndex(size_t aAccessIndex, nsACString& aAccessResult);
  static size_t GetAccessIndexForRequest(const DeviceStorageRequestType aRequestType);
  static bool IsVolumeBased(const nsAString& aType);
  static bool IsSharedMediaRoot(const nsAString& aType);

private:
  nsString mPicturesExtensions;
  nsString mVideosExtensions;
  nsString mMusicExtensions;

  static mozilla::StaticAutoPtr<DeviceStorageTypeChecker> sDeviceStorageTypeChecker;
};

class nsDOMDeviceStorageCursor final
  : public mozilla::dom::DOMCursor
{
public:
  NS_FORWARD_NSIDOMDOMCURSOR(mozilla::dom::DOMCursor::)

  // DOMCursor
  virtual void Continue(mozilla::ErrorResult& aRv) override;

  nsDOMDeviceStorageCursor(nsIGlobalObject* aGlobal,
                           DeviceStorageCursorRequest* aRequest);

  void FireSuccess(JS::Handle<JS::Value> aResult);
  void FireError(const nsString& aReason);
  void FireDone();

private:
  virtual ~nsDOMDeviceStorageCursor();

  bool mOkToCallContinue;
  nsRefPtr<DeviceStorageCursorRequest> mRequest;
};

class DeviceStorageRequestManager final
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(DeviceStorageRequestManager)

  static const uint32_t INVALID_ID = 0;

  DeviceStorageRequestManager();

  bool IsOwningThread();
  nsresult DispatchToOwningThread(nsIRunnable* aRunnable);

  void StorePermission(size_t aAccess, bool aAllow);
  uint32_t CheckPermission(size_t aAccess);

  /* These must be called on the owning thread context of the device
     storage object. It will hold onto a device storage reference until
     all of the pending requests are completed or shutdown is called. */
  uint32_t Create(nsDOMDeviceStorage* aDeviceStorage,
                  mozilla::dom::DOMRequest** aRequest);
  uint32_t Create(nsDOMDeviceStorage* aDeviceStorage,
                  DeviceStorageCursorRequest* aRequest,
                  nsDOMDeviceStorageCursor** aCursor);

  /* These may be called from any thread context and post a request
     to the owning thread to resolve the underlying DOMRequest or
     DOMCursor. In order to trigger FireDone for a DOMCursor, one
     should call Resolve with only the request ID. */
  nsresult Resolve(uint32_t aId, bool aForceDispatch);
  nsresult Resolve(uint32_t aId, const nsString& aValue, bool aForceDispatch);
  nsresult Resolve(uint32_t aId, uint64_t aValue, bool aForceDispatch);
  nsresult Resolve(uint32_t aId, DeviceStorageFile* aValue, bool aForceDispatch);
  nsresult Resolve(uint32_t aId, mozilla::dom::BlobImpl* aValue, bool aForceDispatch);
  nsresult Reject(uint32_t aId, const nsString& aReason);
  nsresult Reject(uint32_t aId, const char* aReason);

  void Shutdown();

private:
  DeviceStorageRequestManager(const DeviceStorageRequestManager&) = delete;
  DeviceStorageRequestManager& operator=(const DeviceStorageRequestManager&) = delete;

  struct ListEntry {
    nsRefPtr<mozilla::dom::DOMRequest> mRequest;
    uint32_t mId;
    bool mCursor;
  };

  typedef nsTArray<ListEntry> ListType;
  typedef ListType::index_type ListIndex;

  virtual ~DeviceStorageRequestManager();
  uint32_t CreateInternal(mozilla::dom::DOMRequest* aRequest, bool aCursor);
  nsresult ResolveInternal(ListIndex aIndex, JS::HandleValue aResult);
  nsresult RejectInternal(ListIndex aIndex, const nsString& aReason);
  nsresult DispatchOrAbandon(uint32_t aId, nsIRunnable* aRunnable);
  ListType::index_type Find(uint32_t aId);

  nsCOMPtr<nsIThread> mOwningThread;
  ListType mPending; // owning thread or destructor only

  mozilla::Mutex mMutex;
  uint32_t mPermissionCache[DEVICE_STORAGE_ACCESS_COUNT];
  bool mShutdown;

  static mozilla::Atomic<uint32_t> sLastRequestId;
};

class DeviceStorageRequest
  : public nsRunnable
{
protected:
  DeviceStorageRequest();

public:
  virtual void Initialize(DeviceStorageRequestManager* aManager,
                          DeviceStorageFile* aFile,
                          uint32_t aRequest);

  virtual void Initialize(DeviceStorageRequestManager* aManager,
                          DeviceStorageFile* aFile,
                          uint32_t aRequest,
                          mozilla::dom::BlobImpl* aBlob);

  virtual void Initialize(DeviceStorageRequestManager* aManager,
                          DeviceStorageFile* aFile,
                          uint32_t aRequest,
                          DeviceStorageFileDescriptor* aDSFileDescriptor);

  DeviceStorageAccessType GetAccess() const;
  void GetStorageType(nsAString& aType) const;
  DeviceStorageFile* GetFile() const;
  DeviceStorageFileDescriptor* GetFileDescriptor() const;
  DeviceStorageRequestManager* GetManager() const;

  uint32_t GetId() const
  {
    return mId;
  }

  void PermissionCacheMissed()
  {
    mPermissionCached = false;
  }

  nsresult Cancel();
  nsresult Allow();

  nsresult Resolve()
  {
    /* Always dispatch an empty resolve because that signals a cursor end
       and should not be executed directly from the caller's context due
       to the object potentially getting freed before we return. */
    uint32_t id = mId;
    mId = DeviceStorageRequestManager::INVALID_ID;
    return mManager->Resolve(id, true);
  }

  template<class T>
  nsresult Resolve(T aValue)
  {
    uint32_t id = mId;
    if (!mMultipleResolve) {
      mId = DeviceStorageRequestManager::INVALID_ID;
    }
    return mManager->Resolve(id, aValue, ForceDispatch());
  }

  template<class T>
  nsresult Reject(T aReason)
  {
    uint32_t id = mId;
    mId = DeviceStorageRequestManager::INVALID_ID;
    return mManager->Reject(id, aReason);
  }

protected:
  bool ForceDispatch() const
  {
    return !mSendToParent && mPermissionCached;
  }

  virtual ~DeviceStorageRequest();
  virtual void Prepare();
  virtual nsresult CreateSendParams(mozilla::dom::DeviceStorageParams& aParams);
  nsresult AllowInternal();
  nsresult SendToParentProcess();

  nsRefPtr<DeviceStorageRequestManager> mManager;
  nsRefPtr<DeviceStorageFile> mFile;
  uint32_t mId;
  nsRefPtr<mozilla::dom::BlobImpl> mBlob;
  nsRefPtr<DeviceStorageFileDescriptor> mDSFileDescriptor;
  DeviceStorageAccessType mAccess;
  bool mSendToParent;
  bool mUseStreamTransport;
  bool mCheckFile;
  bool mCheckBlob;
  bool mMultipleResolve;
  bool mPermissionCached;

private:
  DeviceStorageRequest(const DeviceStorageRequest&) = delete;
  DeviceStorageRequest& operator=(const DeviceStorageRequest&) = delete;
};

class DeviceStorageCursorRequest final
  : public DeviceStorageRequest
{
public:
  DeviceStorageCursorRequest();

  using DeviceStorageRequest::Initialize;

  virtual void Initialize(DeviceStorageRequestManager* aManager,
                          DeviceStorageFile* aFile,
                          uint32_t aRequest,
                          PRTime aSince);

  void AddFiles(size_t aSize);
  void AddFile(already_AddRefed<DeviceStorageFile> aFile);
  nsresult Continue();
  NS_IMETHOD Run() override;

protected:
  virtual ~DeviceStorageCursorRequest()
  { };

  nsresult SendContinueToParentProcess();
  nsresult CreateSendParams(mozilla::dom::DeviceStorageParams& aParams) override;

  size_t mIndex;
  PRTime mSince;
  nsString mStorageType;
  nsTArray<nsRefPtr<DeviceStorageFile> > mFiles;
};

#endif
