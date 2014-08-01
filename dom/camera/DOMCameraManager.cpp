/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DOMCameraManager.h"
#include "nsDebug.h"
#include "jsapi.h"
#include "Navigator.h"
#include "nsPIDOMWindow.h"
#include "mozilla/Services.h"
#include "nsContentPermissionHelper.h"
#include "nsIContentPermissionPrompt.h"
#include "nsIObserverService.h"
#include "nsIPermissionManager.h"
#include "nsIScriptObjectPrincipal.h"
#include "DOMCameraControl.h"
#include "nsDOMClassInfo.h"
#include "CameraCommon.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/CameraManagerBinding.h"
#include "mozilla/dom/PermissionMessageUtils.h"

using namespace mozilla;
using namespace mozilla::dom;

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(nsDOMCameraManager, mWindow)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsDOMCameraManager)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIObserver)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
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
  if (!sLog) {
    sLog = PR_NewLogModule("Camera");
  }
  return sLog;
}

WindowTable* nsDOMCameraManager::sActiveWindows = nullptr;

nsDOMCameraManager::nsDOMCameraManager(nsPIDOMWindow* aWindow)
  : mWindowId(aWindow->WindowID())
  , mPermission(nsIPermissionManager::DENY_ACTION)
  , mWindow(aWindow)
{
  /* member initializers and constructor code */
  DOM_CAMERA_LOGT("%s:%d : this=%p, windowId=%llx\n", __func__, __LINE__, this, mWindowId);
  MOZ_COUNT_CTOR(nsDOMCameraManager);
  SetIsDOMBinding();
}

nsDOMCameraManager::~nsDOMCameraManager()
{
  /* destructor code */
  MOZ_COUNT_DTOR(nsDOMCameraManager);
  DOM_CAMERA_LOGT("%s:%d : this=%p\n", __func__, __LINE__, this);
}

/* static */
void
nsDOMCameraManager::GetListOfCameras(nsTArray<nsString>& aList, ErrorResult& aRv)
{
  aRv = ICameraControl::GetListOfCameras(aList);
}

/* static */
bool
nsDOMCameraManager::HasSupport(JSContext* aCx, JSObject* aGlobal)
{
  return Navigator::HasCameraSupport(aCx, aGlobal);
}

/* static */
bool
nsDOMCameraManager::CheckPermission(nsPIDOMWindow* aWindow)
{
  nsCOMPtr<nsIPermissionManager> permMgr =
    services::GetPermissionManager();
  NS_ENSURE_TRUE(permMgr, false);

  uint32_t permission = nsIPermissionManager::DENY_ACTION;
  permMgr->TestPermissionFromWindow(aWindow, "camera", &permission);
  if (permission != nsIPermissionManager::ALLOW_ACTION &&
      permission != nsIPermissionManager::PROMPT_ACTION) {
    return false;
  }

  return true;
}

/* static */
already_AddRefed<nsDOMCameraManager>
nsDOMCameraManager::CreateInstance(nsPIDOMWindow* aWindow)
{
  // Initialize the shared active window tracker
  if (!sActiveWindows) {
    sActiveWindows = new WindowTable();
  }

  nsRefPtr<nsDOMCameraManager> cameraManager =
    new nsDOMCameraManager(aWindow);

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  obs->AddObserver(cameraManager, "xpcom-shutdown", true);

  return cameraManager.forget();
}

class CameraPermissionRequest : public nsIContentPermissionRequest
                              , public nsIRunnable
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_NSICONTENTPERMISSIONREQUEST
  NS_DECL_NSIRUNNABLE
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(CameraPermissionRequest,
                                           nsIContentPermissionRequest)

  CameraPermissionRequest(nsIPrincipal* aPrincipal,
                          nsPIDOMWindow* aWindow,
                          nsRefPtr<nsDOMCameraManager> aManager,
                          uint32_t aCameraId,
                          const CameraConfiguration& aInitialConfig,
                          nsRefPtr<GetCameraCallback> aOnSuccess,
                          nsRefPtr<CameraErrorCallback> aOnError)
    : mPrincipal(aPrincipal)
    , mWindow(aWindow)
    , mCameraManager(aManager)
    , mCameraId(aCameraId)
    , mInitialConfig(aInitialConfig)
    , mOnSuccess(aOnSuccess)
    , mOnError(aOnError)
  {
  }

protected:
  virtual ~CameraPermissionRequest()
  {
  }

  nsresult DispatchCallback(uint32_t aPermission);
  void CallAllow();
  void CallCancel();
  nsCOMPtr<nsIPrincipal> mPrincipal;
  nsCOMPtr<nsPIDOMWindow> mWindow;
  nsRefPtr<nsDOMCameraManager> mCameraManager;
  uint32_t mCameraId;
  CameraConfiguration mInitialConfig;
  nsRefPtr<GetCameraCallback> mOnSuccess;
  nsRefPtr<CameraErrorCallback> mOnError;
};

NS_IMPL_CYCLE_COLLECTION(CameraPermissionRequest, mWindow, mOnSuccess, mOnError)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(CameraPermissionRequest)
  NS_INTERFACE_MAP_ENTRY(nsIContentPermissionRequest)
  NS_INTERFACE_MAP_ENTRY(nsIRunnable)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIContentPermissionRequest)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(CameraPermissionRequest)
NS_IMPL_CYCLE_COLLECTING_RELEASE(CameraPermissionRequest)

NS_IMETHODIMP
CameraPermissionRequest::Run()
{
  return nsContentPermissionUtils::AskPermission(this, mWindow);
}

NS_IMETHODIMP
CameraPermissionRequest::GetPrincipal(nsIPrincipal** aRequestingPrincipal)
{
  NS_ADDREF(*aRequestingPrincipal = mPrincipal);
  return NS_OK;
}

NS_IMETHODIMP
CameraPermissionRequest::GetWindow(nsIDOMWindow** aRequestingWindow)
{
  NS_ADDREF(*aRequestingWindow = mWindow);
  return NS_OK;
}

NS_IMETHODIMP
CameraPermissionRequest::GetElement(nsIDOMElement** aElement)
{
  *aElement = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
CameraPermissionRequest::Cancel()
{
  return DispatchCallback(nsIPermissionManager::DENY_ACTION);
}

NS_IMETHODIMP
CameraPermissionRequest::Allow(JS::HandleValue aChoices)
{
  MOZ_ASSERT(aChoices.isUndefined());
  return DispatchCallback(nsIPermissionManager::ALLOW_ACTION);
}

nsresult
CameraPermissionRequest::DispatchCallback(uint32_t aPermission)
{
  nsCOMPtr<nsIRunnable> callbackRunnable;
  if (aPermission == nsIPermissionManager::ALLOW_ACTION) {
    callbackRunnable = NS_NewRunnableMethod(this, &CameraPermissionRequest::CallAllow);
  } else {
    callbackRunnable = NS_NewRunnableMethod(this, &CameraPermissionRequest::CallCancel);
  }
  return NS_DispatchToMainThread(callbackRunnable);
}

void
CameraPermissionRequest::CallAllow()
{
  mCameraManager->PermissionAllowed(mCameraId, mInitialConfig, mOnSuccess, mOnError);
}

void
CameraPermissionRequest::CallCancel()
{
  mCameraManager->PermissionCancelled(mCameraId, mInitialConfig, mOnSuccess, mOnError);
}

NS_IMETHODIMP
CameraPermissionRequest::GetTypes(nsIArray** aTypes)
{
  nsTArray<nsString> emptyOptions;
  return nsContentPermissionUtils::CreatePermissionArray(NS_LITERAL_CSTRING("camera"),
                                                         NS_LITERAL_CSTRING("unused"),
                                                         emptyOptions,
                                                         aTypes);
}

void
nsDOMCameraManager::GetCamera(const nsAString& aCamera,
                              const CameraConfiguration& aInitialConfig,
                              GetCameraCallback& aOnSuccess,
                              const OptionalNonNullCameraErrorCallback& aOnError,
                              ErrorResult& aRv)
{
  DOM_CAMERA_LOGT("%s:%d\n", __func__, __LINE__);

  uint32_t cameraId = 0;  // back (or forward-facing) camera by default
  if (aCamera.EqualsLiteral("front")) {
    cameraId = 1;
  }

  nsRefPtr<CameraErrorCallback> errorCallback = nullptr;
  if (aOnError.WasPassed()) {
    errorCallback = &aOnError.Value();
  }

  if (mPermission == nsIPermissionManager::ALLOW_ACTION) {
    PermissionAllowed(cameraId, aInitialConfig, &aOnSuccess, errorCallback);
    return;
  }

  nsCOMPtr<nsIScriptObjectPrincipal> sop = do_QueryInterface(mWindow);
  if (!sop) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }

  nsCOMPtr<nsIPrincipal> principal = sop->GetPrincipal();

  nsCOMPtr<nsIRunnable> permissionRequest =
    new CameraPermissionRequest(principal, mWindow, this, cameraId, aInitialConfig,
                                &aOnSuccess, errorCallback);

  NS_DispatchToMainThread(permissionRequest);
}

void
nsDOMCameraManager::PermissionAllowed(uint32_t aCameraId,
                                      const CameraConfiguration& aInitialConfig,
                                      GetCameraCallback* aOnSuccess,
                                      CameraErrorCallback* aOnError)
{
  mPermission = nsIPermissionManager::ALLOW_ACTION;

  // Creating this object will trigger the aOnSuccess callback
  //  (or the aOnError one, if it fails).
  nsRefPtr<nsDOMCameraControl> cameraControl =
    new nsDOMCameraControl(aCameraId, aInitialConfig, aOnSuccess, aOnError, mWindow);

  Register(cameraControl);
}

void
nsDOMCameraManager::PermissionCancelled(uint32_t aCameraId,
                                        const CameraConfiguration& aInitialConfig,
                                        GetCameraCallback* aOnSuccess,
                                        CameraErrorCallback* aOnError)
{
  mPermission = nsIPermissionManager::DENY_ACTION;

  if (aOnError) {
    ErrorResult ignored;
    aOnError->Call(NS_LITERAL_STRING("Permission denied."), ignored);
  }
}

void
nsDOMCameraManager::Register(nsDOMCameraControl* aDOMCameraControl)
{
  DOM_CAMERA_LOGI(">>> Register( aDOMCameraControl = %p ) mWindowId = 0x%llx\n", aDOMCameraControl, mWindowId);
  MOZ_ASSERT(NS_IsMainThread());

  // Put the camera control into the hash table
  CameraControls* controls = sActiveWindows->Get(mWindowId);
  if (!controls) {
    controls = new CameraControls;
    sActiveWindows->Put(mWindowId, controls);
  }
  controls->AppendElement(aDOMCameraControl);
}

void
nsDOMCameraManager::Shutdown(uint64_t aWindowId)
{
  DOM_CAMERA_LOGI(">>> Shutdown( aWindowId = 0x%llx )\n", aWindowId);
  MOZ_ASSERT(NS_IsMainThread());

  CameraControls* controls = sActiveWindows->Get(aWindowId);
  if (!controls) {
    return;
  }

  uint32_t length = controls->Length();
  for (uint32_t i = 0; i < length; i++) {
    nsRefPtr<nsDOMCameraControl> cameraControl = controls->ElementAt(i);
    cameraControl->Shutdown();
  }
  controls->Clear();

  sActiveWindows->Remove(aWindowId);
}

void
nsDOMCameraManager::XpComShutdown()
{
  DOM_CAMERA_LOGI(">>> XPCOM Shutdown\n");
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  obs->RemoveObserver(this, "xpcom-shutdown");

  delete sActiveWindows;
  sActiveWindows = nullptr;
}

nsresult
nsDOMCameraManager::Observe(nsISupports* aSubject, const char* aTopic, const char16_t* aData)
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

  if (!sActiveWindows) {
    return false;
  }

  return !!sActiveWindows->Get(aWindowId);
}

JSObject*
nsDOMCameraManager::WrapObject(JSContext* aCx)
{
  return CameraManagerBinding::Wrap(aCx, this);
}
