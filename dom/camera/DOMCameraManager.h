/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=40: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_CAMERA_DOMCAMERAMANAGER_H
#define DOM_CAMERA_DOMCAMERAMANAGER_H

#include "mozilla/dom/BindingDeclarations.h"
#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsIThread.h"
#include "nsIObserver.h"
#include "nsThreadUtils.h"
#include "nsHashKeys.h"
#include "nsWrapperCache.h"
#include "nsWeakReference.h"
#include "nsClassHashtable.h"
#include "nsIDOMCameraManager.h"
#include "nsCycleCollectionParticipant.h"
#include "mozilla/Attributes.h"
#include "nsPIDOMWindow.h"

namespace mozilla {
  class ErrorResult;
class nsDOMCameraControl;
namespace dom {
class CameraSelector;
}
}

typedef nsTArray<nsRefPtr<mozilla::nsDOMCameraControl> > CameraControls;
typedef nsClassHashtable<nsUint64HashKey, CameraControls> WindowTable;

class nsDOMCameraManager MOZ_FINAL
  : public nsIObserver
  , public nsSupportsWeakReference
  , public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_AMBIGUOUS(nsDOMCameraManager,
                                                         nsIObserver)
  NS_DECL_NSIOBSERVER

  static bool CheckPermission(nsPIDOMWindow* aWindow);
  static already_AddRefed<nsDOMCameraManager>
    CreateInstance(nsPIDOMWindow* aWindow);
  static bool IsWindowStillActive(uint64_t aWindowId);

  void Register(mozilla::nsDOMCameraControl* aDOMCameraControl);
  void OnNavigation(uint64_t aWindowId);

  nsresult GetNumberOfCameras(int32_t& aDeviceCount);
  nsresult GetCameraName(uint32_t aDeviceNum, nsCString& aDeviceName);

  // WebIDL
  void GetCamera(const mozilla::dom::CameraSelector& aOptions,
                 nsICameraGetCameraCallback* aCallback,
                 const mozilla::dom::Optional<nsICameraErrorCallback*>& ErrorCallback,
                 mozilla::ErrorResult& aRv);
  void GetListOfCameras(nsTArray<nsString>& aList, mozilla::ErrorResult& aRv);

  nsPIDOMWindow* GetParentObject() const { return mWindow; }
  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
    MOZ_OVERRIDE;

protected:
  void XpComShutdown();
  void Shutdown(uint64_t aWindowId);
  ~nsDOMCameraManager();

private:
  nsDOMCameraManager() MOZ_DELETE;
  nsDOMCameraManager(nsPIDOMWindow* aWindow);
  nsDOMCameraManager(const nsDOMCameraManager&) MOZ_DELETE;
  nsDOMCameraManager& operator=(const nsDOMCameraManager&) MOZ_DELETE;

protected:
  uint64_t mWindowId;
  nsCOMPtr<nsIThread> mCameraThread;
  nsCOMPtr<nsPIDOMWindow> mWindow;
  /**
   * 'mActiveWindows' is only ever accessed while in the main thread,
   * so it is not otherwise protected.
   */
  static ::WindowTable* sActiveWindows;
};

class GetCameraTask : public nsRunnable
{
public:
  GetCameraTask(uint32_t aCameraId, nsICameraGetCameraCallback* onSuccess, nsICameraErrorCallback* onError, nsIThread* aCameraThread)
    : mCameraId(aCameraId)
    , mOnSuccessCb(onSuccess)
    , mOnErrorCb(onError)
    , mCameraThread(aCameraThread)
  { }

  NS_IMETHOD Run() MOZ_OVERRIDE;

protected:
  uint32_t mCameraId;
  nsCOMPtr<nsICameraGetCameraCallback> mOnSuccessCb;
  nsCOMPtr<nsICameraErrorCallback> mOnErrorCb;
  nsCOMPtr<nsIThread> mCameraThread;
};

#endif // DOM_CAMERA_DOMCAMERAMANAGER_H
