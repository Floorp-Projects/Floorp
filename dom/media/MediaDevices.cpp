/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/MediaDevices.h"
#include "mozilla/dom/MediaStreamBinding.h"
#include "mozilla/dom/MediaDeviceInfo.h"
#include "mozilla/dom/MediaDevicesBinding.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/MediaManager.h"
#include "MediaTrackConstraints.h"
#include "nsIEventTarget.h"
#include "nsIScriptGlobalObject.h"
#include "nsIPermissionManager.h"
#include "nsPIDOMWindow.h"
#include "nsQueryObject.h"

namespace mozilla {
namespace dom {

class MediaDevices::GumResolver : public nsIDOMGetUserMediaSuccessCallback
{
public:
  NS_DECL_ISUPPORTS

  explicit GumResolver(Promise* aPromise) : mPromise(aPromise) {}

  NS_IMETHOD
  OnSuccess(nsISupports* aStream) override
  {
    RefPtr<DOMMediaStream> stream = do_QueryObject(aStream);
    if (!stream) {
      return NS_ERROR_FAILURE;
    }
    mPromise->MaybeResolve(stream);
    return NS_OK;
  }

private:
  virtual ~GumResolver() {}
  RefPtr<Promise> mPromise;
};

class MediaDevices::EnumDevResolver : public nsIGetUserMediaDevicesSuccessCallback
{
public:
  NS_DECL_ISUPPORTS

  EnumDevResolver(Promise* aPromise, uint64_t aWindowId)
  : mPromise(aPromise), mWindowId(aWindowId) {}

  NS_IMETHOD
  OnSuccess(nsIVariant* aDevices) override
  {
    // Cribbed from MediaPermissionGonk.cpp

    // Create array for nsIMediaDevice
    nsTArray<nsCOMPtr<nsIMediaDevice>> devices;
    // Contain the fumes
    {
      uint16_t vtype;
      nsresult rv = aDevices->GetDataType(&vtype);
      NS_ENSURE_SUCCESS(rv, rv);
      if (vtype != nsIDataType::VTYPE_EMPTY_ARRAY) {
        nsIID elementIID;
        uint16_t elementType;
        void* rawArray;
        uint32_t arrayLen;
        rv = aDevices->GetAsArray(&elementType, &elementIID, &arrayLen, &rawArray);
        NS_ENSURE_SUCCESS(rv, rv);
        if (elementType != nsIDataType::VTYPE_INTERFACE) {
          free(rawArray);
          return NS_ERROR_FAILURE;
        }

        nsISupports **supportsArray = reinterpret_cast<nsISupports **>(rawArray);
        for (uint32_t i = 0; i < arrayLen; ++i) {
          nsCOMPtr<nsIMediaDevice> device(do_QueryInterface(supportsArray[i]));
          devices.AppendElement(device);
          NS_IF_RELEASE(supportsArray[i]); // explicitly decrease refcount for rawptr
        }
        free(rawArray); // explicitly free memory from nsIVariant::GetAsArray
      }
    }
    nsTArray<RefPtr<MediaDeviceInfo>> infos;
    for (auto& device : devices) {
      nsString type;
      device->GetType(type);
      bool isVideo = type.EqualsLiteral("video");
      bool isAudio = type.EqualsLiteral("audio");
      if (isVideo || isAudio) {
        MediaDeviceKind kind = isVideo ?
            MediaDeviceKind::Videoinput : MediaDeviceKind::Audioinput;
        nsString id;
        nsString name;
        device->GetId(id);
        // Include name only if page currently has a gUM stream active or
        // persistent permissions (audio or video) have been granted
        if (MediaManager::Get()->IsActivelyCapturingOrHasAPermission(mWindowId) ||
            Preferences::GetBool("media.navigator.permission.disabled", false)) {
          device->GetName(name);
        }
        RefPtr<MediaDeviceInfo> info = new MediaDeviceInfo(id, kind, name);
        infos.AppendElement(info);
      }
    }
    mPromise->MaybeResolve(infos);
    return NS_OK;
  }

private:
  virtual ~EnumDevResolver() {}
  RefPtr<Promise> mPromise;
  uint64_t mWindowId;
};

class MediaDevices::GumRejecter : public nsIDOMGetUserMediaErrorCallback
{
public:
  NS_DECL_ISUPPORTS

  explicit GumRejecter(Promise* aPromise) : mPromise(aPromise) {}

  NS_IMETHOD
  OnError(nsISupports* aError) override
  {
    RefPtr<MediaStreamError> error = do_QueryObject(aError);
    if (!error) {
      return NS_ERROR_FAILURE;
    }
    mPromise->MaybeReject(error);
    return NS_OK;
  }

private:
  virtual ~GumRejecter() {}
  RefPtr<Promise> mPromise;
};

NS_IMPL_ISUPPORTS(MediaDevices::GumResolver, nsIDOMGetUserMediaSuccessCallback)
NS_IMPL_ISUPPORTS(MediaDevices::EnumDevResolver, nsIGetUserMediaDevicesSuccessCallback)
NS_IMPL_ISUPPORTS(MediaDevices::GumRejecter, nsIDOMGetUserMediaErrorCallback)

already_AddRefed<Promise>
MediaDevices::GetUserMedia(const MediaStreamConstraints& aConstraints,
                           ErrorResult &aRv)
{
  nsPIDOMWindowInner* window = GetOwner();
  nsCOMPtr<nsIGlobalObject> go = do_QueryInterface(window);
  RefPtr<Promise> p = Promise::Create(go, aRv);
  NS_ENSURE_TRUE(!aRv.Failed(), nullptr);

  RefPtr<GumResolver> resolver = new GumResolver(p);
  RefPtr<GumRejecter> rejecter = new GumRejecter(p);

  aRv = MediaManager::Get()->GetUserMedia(window, aConstraints,
                                          resolver, rejecter);
  return p.forget();
}

already_AddRefed<Promise>
MediaDevices::EnumerateDevices(ErrorResult &aRv)
{
  nsPIDOMWindowInner* window = GetOwner();
  nsCOMPtr<nsIGlobalObject> go = do_QueryInterface(window);
  RefPtr<Promise> p = Promise::Create(go, aRv);
  NS_ENSURE_TRUE(!aRv.Failed(), nullptr);

  RefPtr<EnumDevResolver> resolver = new EnumDevResolver(p, window->WindowID());
  RefPtr<GumRejecter> rejecter = new GumRejecter(p);

  aRv = MediaManager::Get()->EnumerateDevices(window, resolver, rejecter);
  return p.forget();
}

NS_IMPL_ADDREF_INHERITED(MediaDevices, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(MediaDevices, DOMEventTargetHelper)
NS_INTERFACE_MAP_BEGIN(MediaDevices)
  NS_INTERFACE_MAP_ENTRY(MediaDevices)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

void
MediaDevices::OnDeviceChange()
{
  MOZ_ASSERT(NS_IsMainThread());
  nsresult rv = CheckInnerWindowCorrectness();
  if (NS_FAILED(rv))
    return;

  if (!(MediaManager::Get()->IsActivelyCapturingOrHasAPermission(GetOwner()->WindowID()) ||
    Preferences::GetBool("media.navigator.permission.disabled", false))) {
    return;
  }

  DispatchTrustedEvent(NS_LITERAL_STRING("devicechange"));
}

mozilla::dom::EventHandlerNonNull*
MediaDevices::GetOndevicechange()
{
  if (NS_IsMainThread()) {
    return GetEventHandler(nsGkAtoms::ondevicechange, EmptyString());
  }
  return GetEventHandler(nullptr, NS_LITERAL_STRING("devicechange"));
}

void
MediaDevices::SetOndevicechange(mozilla::dom::EventHandlerNonNull* aCallback)
{
  if (NS_IsMainThread()) {
    SetEventHandler(nsGkAtoms::ondevicechange, EmptyString(), aCallback);
  } else {
    SetEventHandler(nullptr, NS_LITERAL_STRING("devicechange"), aCallback);
  }

  MediaManager::Get()->AddDeviceChangeCallback(this);
}

nsresult
MediaDevices::AddEventListener(const nsAString& aType,
  nsIDOMEventListener* aListener,
  bool aUseCapture, bool aWantsUntrusted,
  uint8_t optional_argc)
{
  MediaManager::Get()->AddDeviceChangeCallback(this);

  return mozilla::DOMEventTargetHelper::AddEventListener(aType, aListener,
    aUseCapture,
    aWantsUntrusted,
    optional_argc);
}

void
MediaDevices::AddEventListener(const nsAString& aType,
  dom::EventListener* aListener,
  const dom::AddEventListenerOptionsOrBoolean& aOptions,
  const dom::Nullable<bool>& aWantsUntrusted,
  ErrorResult& aRv)
{
  MediaManager::Get()->AddDeviceChangeCallback(this);

  return mozilla::DOMEventTargetHelper::AddEventListener(aType, aListener,
    aOptions,
    aWantsUntrusted,
    aRv);
}

JSObject*
MediaDevices::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return MediaDevicesBinding::Wrap(aCx, this, aGivenProto);
}

} // namespace dom
} // namespace mozilla
