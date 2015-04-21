/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/MediaDevices.h"
#include "mozilla/dom/MediaStreamBinding.h"
#include "mozilla/dom/MediaDevicesBinding.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/MediaManager.h"
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
    nsRefPtr<DOMLocalMediaStream> stream = do_QueryObject(aStream);
    if (!stream) {
      return NS_ERROR_FAILURE;
    }
    mPromise->MaybeResolve(stream);
    return NS_OK;
  }

private:
  virtual ~GumResolver() {}
  nsRefPtr<Promise> mPromise;
};

class MediaDevices::EnumDevResolver : public nsIGetUserMediaDevicesSuccessCallback
{
  static bool HasAPersistentPermission(uint64_t aWindowId)
  {
    nsPIDOMWindow *window = static_cast<nsPIDOMWindow*>
        (nsGlobalWindow::GetInnerWindowWithId(aWindowId));
    if (NS_WARN_IF(!window)) {
      return false;
    }
    // Check if this site has persistent permissions.
    nsresult rv;
    nsCOMPtr<nsIPermissionManager> mgr =
      do_GetService(NS_PERMISSIONMANAGER_CONTRACTID, &rv);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return false; // no permission manager no permissions!
    }

    uint32_t audio = nsIPermissionManager::UNKNOWN_ACTION;
    uint32_t video = nsIPermissionManager::UNKNOWN_ACTION;
    {
      auto* principal = window->GetExtantDoc()->NodePrincipal();
      rv = mgr->TestExactPermissionFromPrincipal(principal, "microphone", &audio);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return false;
      }
      rv = mgr->TestExactPermissionFromPrincipal(principal, "camera", &video);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return false;
      }
    }
    return audio == nsIPermissionManager::ALLOW_ACTION ||
           video == nsIPermissionManager::ALLOW_ACTION;
  }

public:
  NS_DECL_ISUPPORTS

  EnumDevResolver(Promise* aPromise, uint64_t aWindowId)
  : mPromise(aPromise), mWindowId(aWindowId) {}

  NS_IMETHOD
  OnSuccess(nsIVariant* aDevices) override
  {
    // Cribbed from MediaPermissionGonk.cpp
    nsIID elementIID;
    uint16_t elementType;

    // Create array for nsIMediaDevice
    nsTArray<nsCOMPtr<nsIMediaDevice>> devices;
    // Contain the fumes
    {
      void* rawArray;
      uint32_t arrayLen;
      nsresult rv;
      rv = aDevices->GetAsArray(&elementType, &elementIID, &arrayLen, &rawArray);
      NS_ENSURE_SUCCESS(rv, rv);

      if (elementType != nsIDataType::VTYPE_INTERFACE) {
        NS_Free(rawArray);
        return NS_ERROR_FAILURE;
      }

      nsISupports **supportsArray = reinterpret_cast<nsISupports **>(rawArray);
      for (uint32_t i = 0; i < arrayLen; ++i) {
        nsCOMPtr<nsIMediaDevice> device(do_QueryInterface(supportsArray[i]));
        devices.AppendElement(device);
        NS_IF_RELEASE(supportsArray[i]); // explicitly decrease refcount for rawptr
      }
      NS_Free(rawArray); // explicitly free memory from nsIVariant::GetAsArray
    }
    nsTArray<nsRefPtr<MediaDeviceInfo>> infos;
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
        if (MediaManager::Get()->IsWindowActivelyCapturing(mWindowId) ||
            HasAPersistentPermission(mWindowId) ||
            Preferences::GetBool("media.navigator.permission.disabled", false)) {
          device->GetName(name);
        }
        nsRefPtr<MediaDeviceInfo> info = new MediaDeviceInfo(id, kind, name);
        infos.AppendElement(info);
      }
    }
    mPromise->MaybeResolve(infos);
    return NS_OK;
  }

private:
  virtual ~EnumDevResolver() {}
  nsRefPtr<Promise> mPromise;
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
    nsRefPtr<MediaStreamError> error = do_QueryObject(aError);
    if (!error) {
      return NS_ERROR_FAILURE;
    }
    mPromise->MaybeReject(error);
    return NS_OK;
  }

private:
  virtual ~GumRejecter() {}
  nsRefPtr<Promise> mPromise;
};

NS_IMPL_ISUPPORTS(MediaDevices::GumResolver, nsIDOMGetUserMediaSuccessCallback)
NS_IMPL_ISUPPORTS(MediaDevices::EnumDevResolver, nsIGetUserMediaDevicesSuccessCallback)
NS_IMPL_ISUPPORTS(MediaDevices::GumRejecter, nsIDOMGetUserMediaErrorCallback)

already_AddRefed<Promise>
MediaDevices::GetUserMedia(const MediaStreamConstraints& aConstraints,
                           ErrorResult &aRv)
{
  nsPIDOMWindow* window = GetOwner();
  nsCOMPtr<nsIGlobalObject> go = do_QueryInterface(window);
  nsRefPtr<Promise> p = Promise::Create(go, aRv);
  NS_ENSURE_TRUE(!aRv.Failed(), nullptr);

  nsRefPtr<GumResolver> resolver = new GumResolver(p);
  nsRefPtr<GumRejecter> rejecter = new GumRejecter(p);

  aRv = MediaManager::Get()->GetUserMedia(window, aConstraints,
                                          resolver, rejecter);
  return p.forget();
}

already_AddRefed<Promise>
MediaDevices::EnumerateDevices(ErrorResult &aRv)
{
  nsPIDOMWindow* window = GetOwner();
  nsCOMPtr<nsIGlobalObject> go = do_QueryInterface(window);
  nsRefPtr<Promise> p = Promise::Create(go, aRv);
  NS_ENSURE_TRUE(!aRv.Failed(), nullptr);

  nsRefPtr<EnumDevResolver> resolver = new EnumDevResolver(p, window->WindowID());
  nsRefPtr<GumRejecter> rejecter = new GumRejecter(p);

  aRv = MediaManager::Get()->EnumerateDevices(window, resolver, rejecter);
  return p.forget();
}

NS_IMPL_ADDREF_INHERITED(MediaDevices, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(MediaDevices, DOMEventTargetHelper)
NS_INTERFACE_MAP_BEGIN(MediaDevices)
  NS_INTERFACE_MAP_ENTRY(MediaDevices)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

JSObject*
MediaDevices::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return MediaDevicesBinding::Wrap(aCx, this, aGivenProto);
}

} // namespace dom
} // namespace mozilla
