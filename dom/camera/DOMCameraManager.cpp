/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsContentUtils.h"
#include "nsDebug.h"
#include "nsPIDOMWindow.h"
#include "mozilla/Services.h"
#include "nsObserverService.h"
#include "nsIPermissionManager.h"
#include "DOMCameraControl.h"
#include "DOMCameraManager.h"
#include "nsDOMClassInfo.h"
#include "DictionaryHelpers.h"
#include "CameraCommon.h"
#include "mozilla/dom/CameraManagerBinding.h"

using namespace mozilla;
using namespace mozilla::dom;

DOMCI_DATA(CameraManager, nsIDOMCameraManager)

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_1(nsDOMCameraManager, mWindow)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsDOMCameraManager)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMCameraManager)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCameraManager)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(CameraManager)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsDOMCameraManager)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsDOMCameraManager)

/**
 * Global camera logging object
 *
 * Set the NSPR_LOG_MODULES environment variable to enable logging
 * in a debug build, e.g. NSPR_LOG_MODULES=Camera:5
 */
PRLogModuleInfo*
GetCameraLog()
{
  static PRLogModuleInfo *sLog;
  if (!sLog)
    sLog = PR_NewLogModule("Camera");
  return sLog;
}

/**
 * nsDOMCameraManager::GetListOfCameras
 * is implementation-specific, and can be found in (e.g.)
 * GonkCameraManager.cpp and FallbackCameraManager.cpp.
 */

WindowTable nsDOMCameraManager::sActiveWindows;
bool nsDOMCameraManager::sActiveWindowsInitialized = false;

nsDOMCameraManager::nsDOMCameraManager(nsPIDOMWindow* aWindow)
  : mWindowId(aWindow->WindowID())
  , mCameraThread(nullptr)
  , mWindow(aWindow)
{
  /* member initializers and constructor code */
  DOM_CAMERA_LOGT("%s:%d : this=%p, windowId=%llx\n", __func__, __LINE__, this, mWindowId);
  SetIsDOMBinding();
}

nsDOMCameraManager::~nsDOMCameraManager()
{
  /* destructor code */
  DOM_CAMERA_LOGT("%s:%d : this=%p\n", __func__, __LINE__, this);
  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  obs->RemoveObserver(this, "xpcom-shutdown");
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

  // Initialize the shared active window tracker
  if (!sActiveWindowsInitialized) {
    sActiveWindows.Init();
    sActiveWindowsInitialized = true;
  }

  nsRefPtr<nsDOMCameraManager> cameraManager =
    new nsDOMCameraManager(aWindow);

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  obs->AddObserver(cameraManager, "xpcom-shutdown", true);

  return cameraManager.forget();
}

/* [implicit_jscontext] void getCamera ([optional] in jsval aOptions, in nsICameraGetCameraCallback onSuccess, [optional] in nsICameraErrorCallback onError); */
NS_IMETHODIMP
nsDOMCameraManager::GetCamera(const JS::Value& aOptions, nsICameraGetCameraCallback* onSuccess, nsICameraErrorCallback* onError, JSContext* cx)
{
  NS_ENSURE_TRUE(onSuccess, NS_ERROR_INVALID_ARG);

  uint32_t cameraId = 0;  // back (or forward-facing) camera by default
  mozilla::idl::CameraSelector selector;

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
  nsCOMPtr<nsDOMCameraControl> cameraControl = new nsDOMCameraControl(cameraId, mCameraThread, onSuccess, onError, mWindowId);

  Register(cameraControl);
  return NS_OK;
}

void
nsDOMCameraManager::GetCamera(JSContext* aCx, const JS::Value aOptions,
                              nsICameraGetCameraCallback* onSuccess,
                              const Optional<nsICameraErrorCallback*>& onError,
                              ErrorResult& aRv)
{
  uint32_t cameraId = 0;  // back (or forward-facing) camera by default
  mozilla::idl::CameraSelector selector;

  aRv = selector.Init(aCx, &aOptions);
  if (aRv.Failed()) {
    return;
  }

  if (selector.camera.EqualsLiteral("front")) {
    cameraId = 1;
  }

  // reuse the same camera thread to conserve resources
  if (!mCameraThread) {
    aRv = NS_NewThread(getter_AddRefs(mCameraThread));
    if (aRv.Failed()) {
      return;
    }
  }

  DOM_CAMERA_LOGT("%s:%d\n", __func__, __LINE__);

  // Creating this object will trigger the onSuccess handler
  nsCOMPtr<nsDOMCameraControl> cameraControl =
    new nsDOMCameraControl(cameraId, mCameraThread,
                           onSuccess, onError.WasPassed() ? onError.Value() : nullptr, mWindowId);

  Register(cameraControl);
}

void
nsDOMCameraManager::Register(nsDOMCameraControl* aDOMCameraControl)
{
  DOM_CAMERA_LOGI(">>> Register( aDOMCameraControl = %p ) mWindowId = 0x%llx\n", aDOMCameraControl, mWindowId);
  MOZ_ASSERT(NS_IsMainThread());

  // Put the camera control into the hash table
  CameraControls* controls = sActiveWindows.Get(mWindowId);
  if (!controls) {
    controls = new CameraControls;
    sActiveWindows.Put(mWindowId, controls);
  }
  controls->AppendElement(aDOMCameraControl);
}

void
nsDOMCameraManager::Shutdown(uint64_t aWindowId)
{
  DOM_CAMERA_LOGI(">>> Shutdown( aWindowId = 0x%llx )\n", aWindowId);
  MOZ_ASSERT(NS_IsMainThread());

  CameraControls* controls = sActiveWindows.Get(aWindowId);
  if (!controls) {
    return;
  }

  uint32_t length = controls->Length();
  for (uint32_t i = 0; i < length; i++) {
    nsRefPtr<nsDOMCameraControl> cameraControl = controls->ElementAt(i);
    cameraControl->Shutdown();
  }
  controls->Clear();

  sActiveWindows.Remove(aWindowId);
}

void
nsDOMCameraManager::XpComShutdown()
{
  DOM_CAMERA_LOGI(">>> XPCOM Shutdown\n");
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  obs->RemoveObserver(this, "xpcom-shutdown");

  sActiveWindows.Clear();
}

nsresult
nsDOMCameraManager::Observe(nsISupports* aSubject, const char* aTopic, const PRUnichar* aData)
{
  if (strcmp(aTopic, "xpcom-shutdown") == 0) {
    XpComShutdown();
  }
  return NS_OK;
}

void
nsDOMCameraManager::OnNavigation(uint64_t aWindowId)
{
  DOM_CAMERA_LOGI(">>> OnNavigation event\n");
  Shutdown(aWindowId);
}

bool
nsDOMCameraManager::IsWindowStillActive(uint64_t aWindowId)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!sActiveWindowsInitialized) {
    return false;
  }

  return !!sActiveWindows.Get(aWindowId);
}

JSObject*
nsDOMCameraManager::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return CameraManagerBinding::Wrap(aCx, aScope, this);
}
