/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaManager.h"
#include "MediaPermissionGonk.h"

#include "nsCOMPtr.h"
#include "nsIContentPermissionPrompt.h"
#include "nsIDocument.h"
#include "nsIDOMNavigatorUserMedia.h"
#include "nsIStringEnumerator.h"
#include "nsISupportsArray.h"
#include "nsJSUtils.h"
#include "nsPIDOMWindow.h"
#include "nsTArray.h"
#include "GetUserMediaRequest.h"
#include "mozilla/dom/PBrowserChild.h"
#include "mozilla/dom/MediaStreamTrackBinding.h"
#include "mozilla/dom/MediaStreamError.h"
#include "nsISupportsPrimitives.h"
#include "nsServiceManagerUtils.h"
#include "nsArrayUtils.h"
#include "nsContentPermissionHelper.h"
#include "mozilla/dom/PermissionMessageUtils.h"

#define AUDIO_PERMISSION_NAME "audio-capture"
#define VIDEO_PERMISSION_NAME "video-capture"

using namespace mozilla::dom;

namespace mozilla {

static MediaPermissionManager *gMediaPermMgr = nullptr;

static void
CreateDeviceNameList(nsTArray<nsCOMPtr<nsIMediaDevice> > &aDevices,
                     nsTArray<nsString> &aDeviceNameList)
{
  for (uint32_t i = 0; i < aDevices.Length(); ++i) {
     nsString name;
     nsresult rv = aDevices[i]->GetName(name);
     NS_ENSURE_SUCCESS_VOID(rv);
     aDeviceNameList.AppendElement(name);
  }
}

static already_AddRefed<nsIMediaDevice>
FindDeviceByName(nsTArray<nsCOMPtr<nsIMediaDevice> > &aDevices,
                 const nsAString &aDeviceName)
{
  for (uint32_t i = 0; i < aDevices.Length(); ++i) {
    nsCOMPtr<nsIMediaDevice> device = aDevices[i];
    nsString deviceName;
    device->GetName(deviceName);
    if (deviceName.Equals(aDeviceName)) {
      return device.forget();
    }
  }

  return nullptr;
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
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICONTENTPERMISSIONREQUEST

  MediaPermissionRequest(nsRefPtr<dom::GetUserMediaRequest> &aRequest,
                         nsTArray<nsCOMPtr<nsIMediaDevice> > &aDevices);

  already_AddRefed<nsPIDOMWindow> GetOwner();

protected:
  virtual ~MediaPermissionRequest() {}

private:
  nsresult DoAllow(const nsString &audioDevice, const nsString &videoDevice);

  bool mAudio; // Request for audio permission
  bool mVideo; // Request for video permission
  nsRefPtr<dom::GetUserMediaRequest> mRequest;
  nsTArray<nsCOMPtr<nsIMediaDevice> > mAudioDevices; // candidate audio devices
  nsTArray<nsCOMPtr<nsIMediaDevice> > mVideoDevices; // candidate video devices
  nsCOMPtr<nsIContentPermissionRequester> mRequester;
};

// MediaPermissionRequest
NS_IMPL_ISUPPORTS(MediaPermissionRequest, nsIContentPermissionRequest)

MediaPermissionRequest::MediaPermissionRequest(nsRefPtr<dom::GetUserMediaRequest> &aRequest,
                                               nsTArray<nsCOMPtr<nsIMediaDevice> > &aDevices)
  : mRequest(aRequest)
{
  dom::MediaStreamConstraints constraints;
  mRequest->GetConstraints(constraints);

  mAudio = !constraints.mAudio.IsBoolean() || constraints.mAudio.GetAsBoolean();
  mVideo = !constraints.mVideo.IsBoolean() || constraints.mVideo.GetAsBoolean();

  for (uint32_t i = 0; i < aDevices.Length(); ++i) {
    nsCOMPtr<nsIMediaDevice> device(aDevices[i]);
    nsAutoString deviceType;
    device->GetType(deviceType);
    if (mAudio && deviceType.EqualsLiteral("audio")) {
      mAudioDevices.AppendElement(device);
    }
    if (mVideo && deviceType.EqualsLiteral("video")) {
      mVideoDevices.AppendElement(device);
    }
  }

  nsCOMPtr<nsPIDOMWindow> window = GetOwner();
  mRequester = new nsContentPermissionRequester(window.get());
}

// nsIContentPermissionRequest methods
NS_IMETHODIMP
MediaPermissionRequest::GetTypes(nsIArray** aTypes)
{
  nsCOMPtr<nsIMutableArray> types = do_CreateInstance(NS_ARRAY_CONTRACTID);
  //XXX append device list
  if (mAudio) {
    nsTArray<nsString> audioDeviceNames;
    CreateDeviceNameList(mAudioDevices, audioDeviceNames);
    nsCOMPtr<nsISupports> AudioType =
      new ContentPermissionType(NS_LITERAL_CSTRING(AUDIO_PERMISSION_NAME),
                                NS_LITERAL_CSTRING("unused"),
                                audioDeviceNames);
    types->AppendElement(AudioType, false);
  }
  if (mVideo) {
    nsTArray<nsString> videoDeviceNames;
    CreateDeviceNameList(mVideoDevices, videoDeviceNames);
    nsCOMPtr<nsISupports> VideoType =
      new ContentPermissionType(NS_LITERAL_CSTRING(VIDEO_PERMISSION_NAME),
                                NS_LITERAL_CSTRING("unused"),
                                videoDeviceNames);
    types->AppendElement(VideoType, false);
  }
  NS_IF_ADDREF(*aTypes = types);

  return NS_OK;
}

NS_IMETHODIMP
MediaPermissionRequest::GetPrincipal(nsIPrincipal **aRequestingPrincipal)
{
  NS_ENSURE_ARG_POINTER(aRequestingPrincipal);

  nsCOMPtr<nsPIDOMWindow> window = static_cast<nsPIDOMWindow*>
      (nsGlobalWindow::GetInnerWindowWithId(mRequest->InnerWindowID()));
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
  nsCOMPtr<nsPIDOMWindow> window = static_cast<nsPIDOMWindow*>
      (nsGlobalWindow::GetInnerWindowWithId(mRequest->InnerWindowID()));
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
  NotifyPermissionDeny(callID, NS_LITERAL_STRING("PermissionDeniedError"));
  return NS_OK;
}

NS_IMETHODIMP
MediaPermissionRequest::Allow(JS::HandleValue aChoices)
{
  // check if JS object
  if (!aChoices.isObject()) {
    MOZ_ASSERT(false, "Not a correct format of PermissionChoice");
    return NS_ERROR_INVALID_ARG;
  }
  // iterate through audio-capture and video-capture
  AutoSafeJSContext cx;
  JS::Rooted<JSObject*> obj(cx, &aChoices.toObject());
  JSAutoCompartment ac(cx, obj);
  JS::Rooted<JS::Value> v(cx);

  // get selected audio device name
  nsString audioDevice;
  if (mAudio) {
    if (!JS_GetProperty(cx, obj, AUDIO_PERMISSION_NAME, &v) || !v.isString()) {
      return NS_ERROR_FAILURE;
    }
    nsAutoJSString deviceName;
    if (!deviceName.init(cx, v)) {
      MOZ_ASSERT(false, "Couldn't initialize string from aChoices");
      return NS_ERROR_FAILURE;
    }
    audioDevice = deviceName;
  }

  // get selected video device name
  nsString videoDevice;
  if (mVideo) {
    if (!JS_GetProperty(cx, obj, VIDEO_PERMISSION_NAME, &v) || !v.isString()) {
      return NS_ERROR_FAILURE;
    }
    nsAutoJSString deviceName;
    if (!deviceName.init(cx, v)) {
      MOZ_ASSERT(false, "Couldn't initialize string from aChoices");
      return NS_ERROR_FAILURE;
    }
    videoDevice = deviceName;
  }

  return DoAllow(audioDevice, videoDevice);
}

NS_IMETHODIMP
MediaPermissionRequest::GetRequester(nsIContentPermissionRequester** aRequester)
{
  NS_ENSURE_ARG_POINTER(aRequester);

  nsCOMPtr<nsIContentPermissionRequester> requester = mRequester;
  requester.forget(aRequester);
  return NS_OK;
}

nsresult
MediaPermissionRequest::DoAllow(const nsString &audioDevice,
                                const nsString &videoDevice)
{
  nsTArray<nsCOMPtr<nsIMediaDevice> > selectedDevices;
  if (mAudio) {
    nsCOMPtr<nsIMediaDevice> device =
      FindDeviceByName(mAudioDevices, audioDevice);
    if (device) {
      selectedDevices.AppendElement(device);
    }
  }

  if (mVideo) {
    nsCOMPtr<nsIMediaDevice> device =
      FindDeviceByName(mVideoDevices, videoDevice);
    if (device) {
      selectedDevices.AppendElement(device);
    }
  }

  nsString callID;
  mRequest->GetCallID(callID);
  return NotifyPermissionAllow(callID, selectedDevices);
}

already_AddRefed<nsPIDOMWindow>
MediaPermissionRequest::GetOwner()
{
  nsCOMPtr<nsPIDOMWindow> window = static_cast<nsPIDOMWindow*>
      (nsGlobalWindow::GetInnerWindowWithId(mRequest->InnerWindowID()));
  return window.forget();
}

// Success callback for MediaManager::GetUserMediaDevices().
class MediaDeviceSuccessCallback: public nsIGetUserMediaDevicesSuccessCallback
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIGETUSERMEDIADEVICESSUCCESSCALLBACK

  MediaDeviceSuccessCallback(nsRefPtr<dom::GetUserMediaRequest> &aRequest)
    : mRequest(aRequest) {}

protected:
  virtual ~MediaDeviceSuccessCallback() {}

private:
  nsresult DoPrompt(nsRefPtr<MediaPermissionRequest> &req);
  nsRefPtr<dom::GetUserMediaRequest> mRequest;
};

NS_IMPL_ISUPPORTS(MediaDeviceSuccessCallback, nsIGetUserMediaDevicesSuccessCallback)

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
  nsCOMPtr<nsPIDOMWindow> window(req->GetOwner());
  return dom::nsContentPermissionUtils::AskPermission(req, window);
}

// Error callback for MediaManager::GetUserMediaDevices()
class MediaDeviceErrorCallback: public nsIDOMGetUserMediaErrorCallback
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMGETUSERMEDIAERRORCALLBACK

  MediaDeviceErrorCallback(const nsAString &aCallID)
    : mCallID(aCallID) {}

protected:
  virtual ~MediaDeviceErrorCallback() {}

private:
  const nsString mCallID;
};

NS_IMPL_ISUPPORTS(MediaDeviceErrorCallback, nsIDOMGetUserMediaErrorCallback)

// nsIDOMGetUserMediaErrorCallback method
NS_IMETHODIMP
MediaDeviceErrorCallback::OnError(nsISupports* aError)
{
  nsRefPtr<MediaStreamError> error = do_QueryObject(aError);
  if (!error) {
    return NS_ERROR_NO_INTERFACE;
  }

  nsString name;
  error->GetName(name);
  return NotifyPermissionDeny(mCallID, name);
}

} // namespace anonymous

// MediaPermissionManager
NS_IMPL_ISUPPORTS(MediaPermissionManager, nsIObserver)

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
  const char16_t* aData)
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

  nsCOMPtr<nsPIDOMWindow> innerWindow = static_cast<nsPIDOMWindow*>
      (nsGlobalWindow::GetInnerWindowWithId(req->InnerWindowID()));
  if (!innerWindow) {
    MOZ_ASSERT(false, "No inner window");
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIGetUserMediaDevicesSuccessCallback> onSuccess =
      new MediaDeviceSuccessCallback(req);
  nsCOMPtr<nsIDOMGetUserMediaErrorCallback> onError =
      new MediaDeviceErrorCallback(callID);

  dom::MediaStreamConstraints constraints;
  req->GetConstraints(constraints);

  nsRefPtr<MediaManager> MediaMgr = MediaManager::GetInstance();
  nsresult rv = MediaMgr->GetUserMediaDevices(innerWindow, constraints, onSuccess, onError);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

} // namespace mozilla
