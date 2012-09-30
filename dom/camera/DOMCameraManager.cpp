/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDebug.h"
#include "nsIDocument.h"
#include "nsIPermissionManager.h"
#include "DOMCameraControl.h"
#include "DOMCameraManager.h"
#include "nsDOMClassInfo.h"
#include "DictionaryHelpers.h"
#include "CameraCommon.h"

using namespace mozilla;

DOMCI_DATA(CameraManager, nsIDOMCameraManager)

NS_INTERFACE_MAP_BEGIN(nsDOMCameraManager)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCameraManager)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(CameraManager)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(nsDOMCameraManager)
NS_IMPL_RELEASE(nsDOMCameraManager)

/**
 * Global camera logging object
 *
 * Set the NSPR_LOG_MODULES environment variable to enable logging
 * in a debug build, e.g. NSPR_LOG_MODULES=Camera:5
 */
PRLogModuleInfo* gCameraLog = PR_LOG_DEFINE("Camera");

/**
 * nsDOMCameraManager::GetListOfCameras
 * is implementation-specific, and can be found in (e.g.)
 * GonkCameraManager.cpp and FallbackCameraManager.cpp.
 */

nsDOMCameraManager::nsDOMCameraManager(uint64_t aWindowId)
  : mWindowId(aWindowId)
{
  /* member initializers and constructor code */
  DOM_CAMERA_LOGT("%s:%d : this=%p, windowId=%llx\n", __func__, __LINE__, this, mWindowId);
}

nsDOMCameraManager::~nsDOMCameraManager()
{
  /* destructor code */
  DOM_CAMERA_LOGT("%s:%d : this=%p\n", __func__, __LINE__, this);
}

void
nsDOMCameraManager::OnNavigation(uint64_t aWindowId)
{
  // TODO: see bug 779145.
}

// static creator
already_AddRefed<nsDOMCameraManager>
nsDOMCameraManager::CheckPermissionAndCreateInstance(nsPIDOMWindow* aWindow)
{
  nsCOMPtr<nsIPermissionManager> permMgr =
    do_GetService(NS_PERMISSIONMANAGER_CONTRACTID);
  NS_ENSURE_TRUE(permMgr, nullptr);

  uint32_t permission = nsIPermissionManager::DENY_ACTION;
  permMgr->TestPermissionFromWindow(aWindow, "camera", &permission);
  if (permission != nsIPermissionManager::ALLOW_ACTION) {
    NS_WARNING("No permission to access camera");
    return nullptr;
  }

  nsRefPtr<nsDOMCameraManager> cameraManager =
    new nsDOMCameraManager(aWindow->WindowID());
  return cameraManager.forget();
}

/* [implicit_jscontext] void getCamera ([optional] in jsval aOptions, in nsICameraGetCameraCallback onSuccess, [optional] in nsICameraErrorCallback onError); */
NS_IMETHODIMP
nsDOMCameraManager::GetCamera(const JS::Value& aOptions, nsICameraGetCameraCallback* onSuccess, nsICameraErrorCallback* onError, JSContext* cx)
{
  NS_ENSURE_TRUE(onSuccess, NS_ERROR_INVALID_ARG);

  uint32_t cameraId = 0;  // back (or forward-facing) camera by default
  CameraSelector selector;

  nsresult rv = selector.Init(cx, &aOptions);
  NS_ENSURE_SUCCESS(rv, rv);

  if (selector.camera.EqualsASCII("front")) {
    cameraId = 1;
  }

  // reuse the same camera thread to conserve resources
  if (!mCameraThread) {
    rv = NS_NewThread(getter_AddRefs(mCameraThread));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  DOM_CAMERA_LOGT("%s:%d\n", __func__, __LINE__);

  // Creating this object will trigger the onSuccess handler
  nsCOMPtr<nsICameraControl> cameraControl = new nsDOMCameraControl(cameraId, mCameraThread, onSuccess, onError);

  return NS_OK;
}
