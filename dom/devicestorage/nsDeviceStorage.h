/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDeviceStorage_h
#define nsDeviceStorage_h

class nsPIDOMWindow;
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
    DEVICE_STORAGE_REQUEST_AVAILABLE
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
      InvalidateRunnable(DeviceStorageUsedSpaceCache* aCache) {
        mOwner = aCache;
      }

      ~InvalidateRunnable() {}

      NS_IMETHOD Run() {
        mOwner->mDirty = true;
        return NS_OK;
      }
    private:
      DeviceStorageUsedSpaceCache* mOwner;
  };

  void Invalidate() {
    NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
    NS_ASSERTION(mIOThread, "Null mIOThread!");

    nsRefPtr<InvalidateRunnable> r = new InvalidateRunnable(this);
    mIOThread->Dispatch(r, NS_DISPATCH_NORMAL);
  }

  void Dispatch(nsIRunnable* aRunnable) {
    NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
    NS_ASSERTION(mIOThread, "Null mIOThread!");

    mIOThread->Dispatch(aRunnable, NS_DISPATCH_NORMAL);
  }

  nsresult GetUsedSizeForType(const nsAString& aStorageType, uint64_t* usedSize);
  void SetUsedSizes(uint64_t aPictureSize, uint64_t aVideosSize,
                    uint64_t aMusicSize, uint64_t aTotalSize);

private:
  friend class InvalidateRunnable;

  nsresult GetPicturesUsedSize(uint64_t* usedSize);
  nsresult GetMusicUsedSize(uint64_t* usedSize);
  nsresult GetVideosUsedSize(uint64_t* usedSize);
  nsresult GetTotalUsedSize(uint64_t* usedSize);

  bool mDirty;
  uint64_t mPicturesUsedSize;
  uint64_t mVideosUsedSize;
  uint64_t mMusicUsedSize;
  uint64_t mTotalUsedSize;

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
  ContinueCursorEvent(nsRefPtr<mozilla::dom::DOMRequest>& aRequest);
  ContinueCursorEvent(mozilla::dom::DOMRequest* aRequest);
  ~ContinueCursorEvent();
  void Continue();

  NS_IMETHOD Run();
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

  nsDOMDeviceStorageCursor(nsIDOMWindow* aWindow,
                           nsIPrincipal* aPrincipal,
                           DeviceStorageFile* aFile,
                           PRTime aSince);


  nsTArray<nsRefPtr<DeviceStorageFile> > mFiles;
  bool mOkToCallContinue;
  PRTime mSince;

  virtual bool Recv__delete__(const bool& allow);
  virtual void IPDLRelease();

  void GetStorageType(nsAString & aType);

  void RequestComplete();

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

#ifdef MOZ_WIDGET_GONK
nsresult GetSDCardStatus(nsAString& aPath, nsAString& aState);
#endif

#endif
