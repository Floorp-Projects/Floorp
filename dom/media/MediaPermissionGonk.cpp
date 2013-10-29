/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaManager.h"
#include "MediaPermissionGonk.h"

#include "nsCOMPtr.h"
#include "nsCxPusher.h"
#include "nsIContentPermissionPrompt.h"
#include "nsIDocument.h"
#include "nsIDOMNavigatorUserMedia.h"
#include "nsISupportsArray.h"
#include "nsPIDOMWindow.h"
#include "nsTArray.h"
#include "GetUserMediaRequest.h"
#include "PCOMContentPermissionRequestChild.h"
#include "mozilla/dom/PBrowserChild.h"
#include "mozilla/dom/TabChild.h"
#include "mozilla/dom/MediaStreamTrackBinding.h"
#include "nsISupportsPrimitives.h"
#include "nsServiceManagerUtils.h"
#include "nsArrayUtils.h"
#include "nsContentPermissionHelper.h"

#define AUDIO_PERMISSION_NAME "audio-capture"
#define VIDEO_PERMISSION_NAME "video-capture"

using namespace mozilla::dom;

namespace mozilla {

static MediaPermissionManager *gMediaPermMgr = nullptr;

static uint32_t
ConvertArrayToPermissionRequest(nsIArray* aSrcArray,
                                nsTArray<PermissionRequest>& aDesArray)
{
  uint32_t len = 0;
  aSrcArray->GetLength(&len);
  for (uint32_t i = 0; i < len; i++) {
    nsCOMPtr<nsIContentPermissionType> cpt = do_QueryElementAt(aSrcArray, i);
    nsAutoCString type;
    nsAutoCString access;
    cpt->GetType(type);
    cpt->GetAccess(access);
    aDesArray.AppendElement(PermissionRequest(type, access));
  }
  return len;
}

// Helper function for notifying permission granted
static nsresult
NotifyPermissionAllow(const nsAString &aCallID, nsTArray<nsCOMPtr<nsIMediaDevice> > &aDevices)
{
  nsresult rv;
  nsCOMPtr<nsISupportsArray> array;
  rv = NS_NewISupportsArray(getter_AddRefs(array));
  NS_ENSURE_SUCCESS(rv, rv);

  for (uint32_t i = 0; i < aDevices.Length(); ++i) {
    rv = array->AppendElement(aDevices.ElementAt(i));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  NS_ENSURE_TRUE(obs, NS_ERROR_FAILURE);

  return obs->NotifyObservers(array, "getUserMedia:response:allow",
                              aCallID.BeginReading());
}

// Helper function for notifying permision denial or error
static nsresult
NotifyPermissionDeny(const nsAString &aCallID, const nsAString &aErrorMsg)
{
  nsresult rv;
  nsCOMPtr<nsISupportsString> supportsString =
    do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = supportsString->SetData(aErrorMsg);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  NS_ENSURE_TRUE(obs, NS_ERROR_FAILURE);

  return obs->NotifyObservers(supportsString, "getUserMedia:response:deny",
                              aCallID.BeginReading());
}

namespace {

/**
 * MediaPermissionRequest will send a prompt ipdl request to b2g process according
 * to its owned type.
 */
class MediaPermissionRequest : public nsIContentPermissionRequest
                             , public PCOMContentPermissionRequestChild
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICONTENTPERMISSIONREQUEST

  MediaPermissionRequest(nsRefPtr<dom::GetUserMediaRequest> &aRequest,
                         nsTArray<nsCOMPtr<nsIMediaDevice> > &aDevices);
  virtual ~MediaPermissionRequest() {}

  // It will be called when prompt dismissed.
  virtual bool Recv__delete__(const bool &allow) MOZ_OVERRIDE;
  virtual void IPDLRelease() MOZ_OVERRIDE { Release(); }

  already_AddRefed<nsPIDOMWindow> GetOwner();

private:
  bool mAudio; // Request for audio permission
  bool mVideo; // Request for video permission
  nsRefPtr<dom::GetUserMediaRequest> mRequest;
  nsTArray<nsCOMPtr<nsIMediaDevice> > mDevices; // candiate device list
};

// MediaPermissionRequest
NS_IMPL_ISUPPORTS1(MediaPermissionRequest, nsIContentPermissionRequest)

MediaPermissionRequest::MediaPermissionRequest(nsRefPtr<dom::GetUserMediaRequest> &aRequest,
                                               nsTArray<nsCOMPtr<nsIMediaDevice> > &aDevices)
  : mRequest(aRequest)
{
  dom::MediaStreamConstraintsInternal constraints;
  mRequest->GetConstraints(constraints);

  mAudio = constraints.mAudio;
  mVideo = constraints.mVideo;

  for (uint32_t i = 0; i < aDevices.Length(); ++i) {
    nsCOMPtr<nsIMediaDevice> device(aDevices[i]);
    nsAutoString deviceType;
    device->GetType(deviceType);
    if (mAudio && deviceType.EqualsLiteral("audio")) {
      mDevices.AppendElement(device);
    }
    if (mVideo && deviceType.EqualsLiteral("video")) {
      mDevices.AppendElement(device);
    }
  }
}

// nsIContentPermissionRequest methods
NS_IMETHODIMP
MediaPermissionRequest::GetTypes(nsIArray** aTypes)
{
  nsCOMPtr<nsIMutableArray> types = do_CreateInstance(NS_ARRAY_CONTRACTID);
  if (mAudio) {
    nsCOMPtr<ContentPermissionType> AudioType =
      new ContentPermissionType(NS_LITERAL_CSTRING(AUDIO_PERMISSION_NAME),
                                NS_LITERAL_CSTRING("unused"));
    types->AppendElement(AudioType, false);
  }
  if (mVideo) {
    nsCOMPtr<ContentPermissionType> VideoType =
      new ContentPermissionType(NS_LITERAL_CSTRING(VIDEO_PERMISSION_NAME),
                                NS_LITERAL_CSTRING("unused"));
    types->AppendElement(VideoType, false);
  }
  NS_IF_ADDREF(*aTypes = types);

  return NS_OK;
}

NS_IMETHODIMP
MediaPermissionRequest::GetPrincipal(nsIPrincipal **aRequestingPrincipal)
{
  NS_ENSURE_ARG_POINTER(aRequestingPrincipal);

  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(mRequest->GetParentObject());
  NS_ENSURE_TRUE(window, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDocument> doc = window->GetExtantDoc();
  NS_ENSURE_TRUE(doc, NS_ERROR_FAILURE);

  NS_ADDREF(*aRequestingPrincipal = doc->NodePrincipal());
  return NS_OK;
}

NS_IMETHODIMP
MediaPermissionRequest::GetWindow(nsIDOMWindow** aRequestingWindow)
{
  NS_ENSURE_ARG_POINTER(aRequestingWindow);
  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(mRequest->GetParentObject());
  window.forget(aRequestingWindow);
  return NS_OK;
}

NS_IMETHODIMP
MediaPermissionRequest::GetElement(nsIDOMElement** aRequestingElement)
{
  NS_ENSURE_ARG_POINTER(aRequestingElement);
  *aRequestingElement = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
MediaPermissionRequest::Cancel()
{
  nsString callID;
  mRequest->GetCallID(callID);
  NotifyPermissionDeny(callID, NS_LITERAL_STRING("Permission Denied"));
  return NS_OK;
}

NS_IMETHODIMP
MediaPermissionRequest::Allow()
{
  nsString callID;
  mRequest->GetCallID(callID);
  NotifyPermissionAllow(callID, mDevices);
  return NS_OK;
}

already_AddRefed<nsPIDOMWindow>
MediaPermissionRequest::GetOwner()
{
  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(mRequest->GetParentObject());
  return window.forget();
}

//PCOMContentPermissionRequestChild
bool
MediaPermissionRequest::Recv__delete__(const bool& allow)
{
  if (allow) {
    (void) Allow();
  } else {
    (void) Cancel();
  }
  return true;
}

// Success callback for MediaManager::GetUserMediaDevices().
class MediaDeviceSuccessCallback: public nsIGetUserMediaDevicesSuccessCallback
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIGETUSERMEDIADEVICESSUCCESSCALLBACK

  MediaDeviceSuccessCallback(nsRefPtr<dom::GetUserMediaRequest> &aRequest)
    : mRequest(aRequest) {}
  virtual ~MediaDeviceSuccessCallback() {}

private:
  nsresult DoPrompt(nsRefPtr<MediaPermissionRequest> &req);
  nsRefPtr<dom::GetUserMediaRequest> mRequest;
};

NS_IMPL_ISUPPORTS1(MediaDeviceSuccessCallback, nsIGetUserMediaDevicesSuccessCallback)

// nsIGetUserMediaDevicesSuccessCallback method
NS_IMETHODIMP
MediaDeviceSuccessCallback::OnSuccess(nsIVariant* aDevices)
{
  nsIID elementIID;
  uint16_t elementType;
  void* rawArray;
  uint32_t arrayLen;

  nsresult rv;
  rv = aDevices->GetAsArray(&elementType, &elementIID, &arrayLen, &rawArray);
  NS_ENSURE_SUCCESS(rv, rv);

  if (elementType != nsIDataType::VTYPE_INTERFACE) {
    NS_Free(rawArray);
    return NS_ERROR_FAILURE;
  }

  // Create array for nsIMediaDevice
  nsTArray<nsCOMPtr<nsIMediaDevice> > devices;

  nsISupports **supportsArray = reinterpret_cast<nsISupports **>(rawArray);
  for (uint32_t i = 0; i < arrayLen; ++i) {
    nsCOMPtr<nsIMediaDevice> device(do_QueryInterface(supportsArray[i]));
    devices.AppendElement(device);
    NS_IF_RELEASE(supportsArray[i]); // explicitly decrease reference count for raw pointer
  }
  NS_Free(rawArray); // explicitly free for the memory from nsIVariant::GetAsArray

  // Send MediaPermissionRequest
  nsRefPtr<MediaPermissionRequest> req = new MediaPermissionRequest(mRequest, devices);
  rv = DoPrompt(req);

  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}

// Trigger permission prompt UI
nsresult
MediaDeviceSuccessCallback::DoPrompt(nsRefPtr<MediaPermissionRequest> &req)
{
  // for content process
  if (XRE_GetProcessType() == GeckoProcessType_Content) {
    MOZ_ASSERT(NS_IsMainThread()); // IPC can only be execute on main thread.

    nsresult rv;

    nsCOMPtr<nsPIDOMWindow> window(req->GetOwner());
    NS_ENSURE_TRUE(window, NS_ERROR_FAILURE);

    dom::TabChild* child = dom::TabChild::GetFrom(window->GetDocShell());
    NS_ENSURE_TRUE(child, NS_ERROR_FAILURE);

    nsCOMPtr<nsIArray> typeArray;
    rv = req->GetTypes(getter_AddRefs(typeArray));
    NS_ENSURE_SUCCESS(rv, rv);

    nsTArray<PermissionRequest> permArray;
    ConvertArrayToPermissionRequest(typeArray, permArray);

    nsCOMPtr<nsIPrincipal> principal;
    rv = req->GetPrincipal(getter_AddRefs(principal));
    NS_ENSURE_SUCCESS(rv, rv);

    req->AddRef();
    child->SendPContentPermissionRequestConstructor(req,
                                                    permArray,
                                                    IPC::Principal(principal));

    req->Sendprompt();
    return NS_OK;
  }

  // for chrome process
  nsCOMPtr<nsIContentPermissionPrompt> prompt =
      do_GetService(NS_CONTENT_PERMISSION_PROMPT_CONTRACTID);
  if (prompt) {
    prompt->Prompt(req);
  }
  return NS_OK;
}

// Error callback for MediaManager::GetUserMediaDevices()
class MediaDeviceErrorCallback: public nsIDOMGetUserMediaErrorCallback
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMGETUSERMEDIAERRORCALLBACK

  MediaDeviceErrorCallback(const nsAString &aCallID)
    : mCallID(aCallID) {}

  virtual ~MediaDeviceErrorCallback() {}

private:
  const nsString mCallID;
};

NS_IMPL_ISUPPORTS1(MediaDeviceErrorCallback, nsIDOMGetUserMediaErrorCallback)

// nsIDOMGetUserMediaErrorCallback method
NS_IMETHODIMP
MediaDeviceErrorCallback::OnError(const nsAString &aError)
{
  return NotifyPermissionDeny(mCallID, aError);
}

} // namespace anonymous

// MediaPermissionManager
NS_IMPL_ISUPPORTS1(MediaPermissionManager, nsIObserver)

MediaPermissionManager*
MediaPermissionManager::GetInstance()
{
  if (!gMediaPermMgr) {
    gMediaPermMgr = new MediaPermissionManager();
  }

  return gMediaPermMgr;
}

MediaPermissionManager::MediaPermissionManager()
{
  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (obs) {
    obs->AddObserver(this, "getUserMedia:request", false);
    obs->AddObserver(this, "xpcom-shutdown", false);
  }
}

MediaPermissionManager::~MediaPermissionManager()
{
  this->Deinit();
}

nsresult
MediaPermissionManager::Deinit()
{
  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (obs) {
    obs->RemoveObserver(this, "getUserMedia:request");
    obs->RemoveObserver(this, "xpcom-shutdown");
  }
  return NS_OK;
}

// nsIObserver method
NS_IMETHODIMP
MediaPermissionManager::Observe(nsISupports* aSubject, const char* aTopic,
  const PRUnichar* aData)
{
  nsresult rv;
  if (!strcmp(aTopic, "getUserMedia:request")) {
    nsRefPtr<dom::GetUserMediaRequest> req =
        static_cast<dom::GetUserMediaRequest*>(aSubject);
    rv = HandleRequest(req);

    if (NS_FAILED(rv)) {
      nsString callID;
      req->GetCallID(callID);
      NotifyPermissionDeny(callID, NS_LITERAL_STRING("unable to enumerate media device"));
    }
  } else if (!strcmp(aTopic, "xpcom-shutdown")) {
    rv = this->Deinit();
  } else {
    // not reachable
    rv = NS_ERROR_FAILURE;
  }
  return rv;
}

// Handle GetUserMediaRequest, query available media device first.
nsresult
MediaPermissionManager::HandleRequest(nsRefPtr<dom::GetUserMediaRequest> &req)
{
  nsString callID;
  req->GetCallID(callID);

  nsCOMPtr<nsPIDOMWindow> innerWindow = do_QueryInterface(req->GetParentObject());
  if (!innerWindow) {
    MOZ_ASSERT(false, "No inner window");
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIGetUserMediaDevicesSuccessCallback> onSuccess =
      new MediaDeviceSuccessCallback(req);
  nsCOMPtr<nsIDOMGetUserMediaErrorCallback> onError =
      new MediaDeviceErrorCallback(callID);

  dom::MediaStreamConstraintsInternal constraints;
  req->GetConstraints(constraints);

  nsRefPtr<MediaManager> MediaMgr = MediaManager::GetInstance();
  nsresult rv = MediaMgr->GetUserMediaDevices(innerWindow, constraints, onSuccess, onError);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

} // namespace mozilla
