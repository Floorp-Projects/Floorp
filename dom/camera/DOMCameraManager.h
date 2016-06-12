/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=40: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_CAMERA_DOMCAMERAMANAGER_H
#define DOM_CAMERA_DOMCAMERAMANAGER_H

#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/Promise.h"
#include "nsCOMPtr.h"
#include "nsIObserver.h"
#include "nsHashKeys.h"
#include "nsWrapperCache.h"
#include "nsWeakReference.h"
#include "nsClassHashtable.h"
#include "nsCycleCollectionParticipant.h"
#include "mozilla/Attributes.h"

class nsPIDOMWindowInner;

namespace mozilla {
  class ErrorResult;
  class nsDOMCameraControl;
  namespace dom {
    struct CameraConfiguration;
  } // namespace dom
} // namespace mozilla

typedef nsTArray<nsWeakPtr> CameraControls;
typedef nsClassHashtable<nsUint64HashKey, CameraControls> WindowTable;

class nsDOMCameraManager final
  : public nsIObserver
  , public nsSupportsWeakReference
  , public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_AMBIGUOUS(nsDOMCameraManager,
                                                         nsIObserver)
  NS_DECL_NSIOBSERVER

  // Because this header's filename doesn't match its C++ or DOM-facing
  // classname, we can't rely on the [Func="..."] WebIDL tag to implicitly
  // include the right header for us; instead we must explicitly include a
  // HasSupport() method in each header. We can get rid of these with the
  // Great Renaming proposed in bug 983177.
  static bool HasSupport(JSContext* aCx, JSObject* aGlobal);

  static bool CheckPermission(nsPIDOMWindowInner* aWindow);
  static already_AddRefed<nsDOMCameraManager>
    CreateInstance(nsPIDOMWindowInner* aWindow);
  static bool IsWindowStillActive(uint64_t aWindowId);

  void Register(mozilla::nsDOMCameraControl* aDOMCameraControl);
  void OnNavigation(uint64_t aWindowId);

  void PermissionAllowed(uint32_t aCameraId,
                         const mozilla::dom::CameraConfiguration& aOptions,
                         mozilla::dom::Promise* aPromise);

  void PermissionCancelled(uint32_t aCameraId,
                           const mozilla::dom::CameraConfiguration& aOptions,
                           mozilla::dom::Promise* aPromise);

  // WebIDL
  already_AddRefed<mozilla::dom::Promise>
  GetCamera(const nsAString& aCamera,
            const mozilla::dom::CameraConfiguration& aOptions,
            mozilla::ErrorResult& aRv);
  void GetListOfCameras(nsTArray<nsString>& aList, mozilla::ErrorResult& aRv);

  nsPIDOMWindowInner* GetParentObject() const { return mWindow; }
  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

#ifdef MOZ_WIDGET_GONK
  static void PreinitCameraHardware();
#endif

protected:
  void XpComShutdown();
  void Shutdown(uint64_t aWindowId);
  ~nsDOMCameraManager();

private:
  nsDOMCameraManager() = delete;
  explicit nsDOMCameraManager(nsPIDOMWindowInner* aWindow);
  nsDOMCameraManager(const nsDOMCameraManager&) = delete;
  nsDOMCameraManager& operator=(const nsDOMCameraManager&) = delete;

protected:
  uint64_t mWindowId;
  uint32_t mPermission;
  nsCOMPtr<nsPIDOMWindowInner> mWindow;
  /**
   * 'sActiveWindows' is only ever accessed while in the Main Thread,
   * so it is not otherwise protected.
   */
  static ::WindowTable* sActiveWindows;
};

#endif // DOM_CAMERA_DOMCAMERAMANAGER_H
