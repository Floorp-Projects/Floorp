/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/MediaDevices.h"
#include "mozilla/dom/MediaStreamBinding.h"
#include "mozilla/dom/MediaDeviceInfo.h"
#include "mozilla/dom/MediaDevicesBinding.h"
#include "mozilla/dom/NavigatorBinding.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/MediaManager.h"
#include "MediaTrackConstraints.h"
#include "nsContentUtils.h"
#include "nsINamed.h"
#include "nsIScriptGlobalObject.h"
#include "nsPIDOMWindow.h"
#include "nsQueryObject.h"

#define DEVICECHANGE_HOLD_TIME_IN_MS 1000

namespace mozilla {
namespace dom {

MediaDevices::~MediaDevices() {
  MOZ_ASSERT(NS_IsMainThread());
  if (mFuzzTimer) {
    mFuzzTimer->Cancel();
  }
  mDeviceChangeListener.DisconnectIfExists();
}

already_AddRefed<Promise> MediaDevices::GetUserMedia(
    const MediaStreamConstraints& aConstraints, CallerType aCallerType,
    ErrorResult& aRv) {
  if (RefPtr<nsPIDOMWindowInner> owner = GetOwner()) {
    if (Document* doc = owner->GetExtantDoc()) {
      if (!owner->IsSecureContext()) {
        doc->SetUseCounter(eUseCounter_custom_GetUserMediaInsec);
      }
      Document* topDoc = doc->GetTopLevelContentDocument();
      IgnoredErrorResult ignored;
      if (topDoc && !topDoc->HasFocus(ignored)) {
        doc->SetUseCounter(eUseCounter_custom_GetUserMediaUnfocused);
      }
    }
  }
  RefPtr<Promise> p = Promise::Create(GetParentObject(), aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }
  RefPtr<MediaDevices> self(this);
  MediaManager::Get()
      ->GetUserMedia(GetOwner(), aConstraints, aCallerType)
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [this, self, p](RefPtr<DOMMediaStream>&& aStream) {
            if (!GetWindowIfCurrent()) {
              return;  // Leave Promise pending after navigation by design.
            }
            p->MaybeResolve(std::move(aStream));
          },
          [this, self, p](const RefPtr<MediaMgrError>& error) {
            nsPIDOMWindowInner* window = GetWindowIfCurrent();
            if (!window) {
              return;  // Leave Promise pending after navigation by design.
            }
            p->MaybeReject(MakeRefPtr<MediaStreamError>(window, *error));
          });
  return p.forget();
}

already_AddRefed<Promise> MediaDevices::EnumerateDevices(CallerType aCallerType,
                                                         ErrorResult& aRv) {
  MOZ_ASSERT(NS_IsMainThread());

  if (RefPtr<nsPIDOMWindowInner> owner = GetOwner()) {
    if (Document* doc = owner->GetExtantDoc()) {
      if (!owner->IsSecureContext()) {
        doc->SetUseCounter(eUseCounter_custom_EnumerateDevicesInsec);
      }
      Document* topDoc = doc->GetTopLevelContentDocument();
      IgnoredErrorResult ignored;
      if (topDoc && !topDoc->HasFocus(ignored)) {
        doc->SetUseCounter(eUseCounter_custom_EnumerateDevicesUnfocused);
      }
    }
  }
  RefPtr<Promise> p = Promise::Create(GetParentObject(), aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }
  RefPtr<MediaDevices> self(this);
  MediaManager::Get()
      ->EnumerateDevices(GetOwner(), aCallerType)
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [this, self,
           p](RefPtr<MediaManager::MediaDeviceSetRefCnt>&& aDevices) {
            nsPIDOMWindowInner* window = GetWindowIfCurrent();
            if (!window) {
              return;  // Leave Promise pending after navigation by design.
            }
            auto windowId = window->WindowID();
            nsTArray<RefPtr<MediaDeviceInfo>> infos;
            bool allowLabel =
                aDevices->Length() == 0 ||
                MediaManager::Get()->IsActivelyCapturingOrHasAPermission(
                    windowId);
            for (auto& device : *aDevices) {
              MOZ_ASSERT(device->mKind == dom::MediaDeviceKind::Audioinput ||
                         device->mKind == dom::MediaDeviceKind::Videoinput ||
                         device->mKind == dom::MediaDeviceKind::Audiooutput);
              // Include name only if page currently has a gUM stream active
              // or persistent permissions (audio or video) have been granted
              nsString label;
              if (allowLabel ||
                  Preferences::GetBool("media.navigator.permission.disabled",
                                       false)) {
                label = device->mName;
              }
              infos.AppendElement(MakeRefPtr<MediaDeviceInfo>(
                  device->mID, device->mKind, label, device->mGroupID));
            }
            p->MaybeResolve(std::move(infos));
          },
          [this, self, p](const RefPtr<MediaMgrError>& error) {
            nsPIDOMWindowInner* window = GetWindowIfCurrent();
            if (!window) {
              return;  // Leave Promise pending after navigation by design.
            }
            p->MaybeReject(MakeRefPtr<MediaStreamError>(window, *error));
          });
  return p.forget();
}

already_AddRefed<Promise> MediaDevices::GetDisplayMedia(
    const DisplayMediaStreamConstraints& aConstraints, CallerType aCallerType,
    ErrorResult& aRv) {
  RefPtr<Promise> p = Promise::Create(GetParentObject(), aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }
  RefPtr<MediaDevices> self(this);
  MediaManager::Get()
      ->GetDisplayMedia(GetOwner(), aConstraints, aCallerType)
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [this, self, p](RefPtr<DOMMediaStream>&& aStream) {
            if (!GetWindowIfCurrent()) {
              return;  // leave promise pending after navigation.
            }
            p->MaybeResolve(std::move(aStream));
          },
          [this, self, p](RefPtr<MediaMgrError>&& error) {
            nsPIDOMWindowInner* window = GetWindowIfCurrent();
            if (!window) {
              return;  // leave promise pending after navigation.
            }
            p->MaybeReject(MakeRefPtr<MediaStreamError>(window, *error));
          });
  return p.forget();
}

NS_IMPL_ADDREF_INHERITED(MediaDevices, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(MediaDevices, DOMEventTargetHelper)
NS_INTERFACE_MAP_BEGIN(MediaDevices)
  NS_INTERFACE_MAP_ENTRY(MediaDevices)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

void MediaDevices::OnDeviceChange() {
  MOZ_ASSERT(NS_IsMainThread());
  if (NS_FAILED(CheckCurrentGlobalCorrectness())) {
    // This is a ghost window, don't do anything.
    return;
  }

  if (!(MediaManager::Get()->IsActivelyCapturingOrHasAPermission(
            GetOwner()->WindowID()) ||
        Preferences::GetBool("media.navigator.permission.disabled", false))) {
    return;
  }

  // Do not fire event to content script when
  // privacy.resistFingerprinting is true.
  if (nsContentUtils::ShouldResistFingerprinting()) {
    return;
  }

  if (mFuzzTimer) {
    // An event is already in flight.
    return;
  }

  mFuzzTimer = NS_NewTimer();

  if (!mFuzzTimer) {
    MOZ_ASSERT(false);
    return;
  }

  mFuzzTimer->InitWithNamedFuncCallback(
      [](nsITimer*, void* aClosure) {
        MediaDevices* md = static_cast<MediaDevices*>(aClosure);
        md->DispatchTrustedEvent(u"devicechange"_ns);
        md->mFuzzTimer = nullptr;
      },
      this, DEVICECHANGE_HOLD_TIME_IN_MS, nsITimer::TYPE_ONE_SHOT,
      "MediaDevices::mFuzzTimer Callback");
}

mozilla::dom::EventHandlerNonNull* MediaDevices::GetOndevicechange() {
  return GetEventHandler(nsGkAtoms::ondevicechange);
}

void MediaDevices::SetupDeviceChangeListener() {
  if (mIsDeviceChangeListenerSetUp) {
    return;
  }

  nsPIDOMWindowInner* window = GetOwner();
  if (!window) {
    return;
  }

  nsISerialEventTarget* mainThread =
      window->EventTargetFor(TaskCategory::Other);
  if (!mainThread) {
    return;
  }

  mDeviceChangeListener = MediaManager::Get()->DeviceListChangeEvent().Connect(
      mainThread, this, &MediaDevices::OnDeviceChange);
  mIsDeviceChangeListenerSetUp = true;
}

void MediaDevices::SetOndevicechange(
    mozilla::dom::EventHandlerNonNull* aCallback) {
  SetEventHandler(nsGkAtoms::ondevicechange, aCallback);
  SetupDeviceChangeListener();
}

void MediaDevices::EventListenerAdded(nsAtom* aType) {
  DOMEventTargetHelper::EventListenerAdded(aType);
  SetupDeviceChangeListener();
}

JSObject* MediaDevices::WrapObject(JSContext* aCx,
                                   JS::Handle<JSObject*> aGivenProto) {
  return MediaDevices_Binding::Wrap(aCx, this, aGivenProto);
}

}  // namespace dom
}  // namespace mozilla
