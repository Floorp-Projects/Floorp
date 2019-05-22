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
#include "nsIEventTarget.h"
#include "nsINamed.h"
#include "nsIScriptGlobalObject.h"
#include "nsIPermissionManager.h"
#include "nsPIDOMWindow.h"
#include "nsQueryObject.h"

#define DEVICECHANGE_HOLD_TIME_IN_MS 1000

namespace mozilla {
namespace dom {

class FuzzTimerCallBack final : public nsITimerCallback, public nsINamed {
  ~FuzzTimerCallBack() {}

 public:
  explicit FuzzTimerCallBack(MediaDevices* aMediaDevices)
      : mMediaDevices(aMediaDevices) {}

  NS_DECL_ISUPPORTS

  NS_IMETHOD Notify(nsITimer* aTimer) final {
    mMediaDevices->DispatchTrustedEvent(NS_LITERAL_STRING("devicechange"));
    return NS_OK;
  }

  NS_IMETHOD GetName(nsACString& aName) override {
    aName.AssignLiteral("FuzzTimerCallBack");
    return NS_OK;
  }

 private:
  nsCOMPtr<MediaDevices> mMediaDevices;
};

NS_IMPL_ISUPPORTS(FuzzTimerCallBack, nsITimerCallback, nsINamed)

MediaDevices::~MediaDevices() {
  MediaManager* mediamanager = MediaManager::GetIfExists();
  if (mediamanager) {
    mediamanager->RemoveDeviceChangeCallback(this);
  }
}

static bool IsSameOriginWithAllParentDocs(nsINode* aDoc) {
  MOZ_ASSERT(aDoc);
  nsINode* node = aDoc;
  while ((node = nsContentUtils::GetCrossDocParentNode(node))) {
    if (NS_FAILED(nsContentUtils::CheckSameOrigin(aDoc, node))) {
      return false;
    }
  }
  return true;
}

already_AddRefed<Promise> MediaDevices::GetUserMedia(
    const MediaStreamConstraints& aConstraints, CallerType aCallerType,
    ErrorResult& aRv) {
  if (RefPtr<nsPIDOMWindowInner> owner = GetOwner()) {
    if (Document* doc = owner->GetExtantDoc()) {
      if (!owner->IsSecureContext()) {
        doc->SetDocumentAndPageUseCounter(eUseCounter_custom_GetUserMediaInsec);
      }
      if (!IsSameOriginWithAllParentDocs(doc)) {
        doc->SetDocumentAndPageUseCounter(
            eUseCounter_custom_GetUserMediaXOrigin);
      }
      Document* topDoc = doc->GetTopLevelContentDocument();
      IgnoredErrorResult ignored;
      if (topDoc && !topDoc->HasFocus(ignored)) {
        doc->SetDocumentAndPageUseCounter(
            eUseCounter_custom_GetUserMediaUnfocused);
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
          GetCurrentThreadSerialEventTarget(), __func__,
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
        doc->SetDocumentAndPageUseCounter(
            eUseCounter_custom_EnumerateDevicesInsec);
      }
      Document* topDoc = doc->GetTopLevelContentDocument();
      IgnoredErrorResult ignored;
      if (topDoc && !topDoc->HasFocus(ignored)) {
        doc->SetDocumentAndPageUseCounter(
            eUseCounter_custom_EnumerateDevicesUnfocused);
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
          GetCurrentThreadSerialEventTarget(), __func__,
          [this, self,
           p](RefPtr<MediaManager::MediaDeviceSetRefCnt>&& aDevices) {
            nsPIDOMWindowInner* window = GetWindowIfCurrent();
            if (!window) {
              return;  // Leave Promise pending after navigation by design.
            }
            auto windowId = window->WindowID();
            nsTArray<RefPtr<MediaDeviceInfo>> infos;
            for (auto& device : *aDevices) {
              MOZ_ASSERT(device->mKind == dom::MediaDeviceKind::Audioinput ||
                         device->mKind == dom::MediaDeviceKind::Videoinput ||
                         device->mKind == dom::MediaDeviceKind::Audiooutput);
              // Include name only if page currently has a gUM stream active
              // or persistent permissions (audio or video) have been granted
              nsString label;
              if (MediaManager::Get()->IsActivelyCapturingOrHasAPermission(
                      windowId) ||
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
  if (RefPtr<nsPIDOMWindowInner> owner = GetOwner()) {
    if (Document* doc = owner->GetExtantDoc()) {
      if (!IsSameOriginWithAllParentDocs(doc)) {
        doc->SetDocumentAndPageUseCounter(
            eUseCounter_custom_GetDisplayMediaXOrigin);
      }
    }
  }
  RefPtr<Promise> p = Promise::Create(GetParentObject(), aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }
  RefPtr<MediaDevices> self(this);
  MediaManager::Get()
      ->GetDisplayMedia(GetOwner(), aConstraints, aCallerType)
      ->Then(
          GetCurrentThreadSerialEventTarget(), __func__,
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
  nsresult rv = CheckCurrentGlobalCorrectness();
  if (NS_FAILED(rv)) {
    MOZ_ASSERT(false);
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

  if (!mFuzzTimer) {
    mFuzzTimer = NS_NewTimer();
  }

  if (!mFuzzTimer) {
    MOZ_ASSERT(false);
    return;
  }

  mFuzzTimer->Cancel();
  RefPtr<FuzzTimerCallBack> cb = new FuzzTimerCallBack(this);
  mFuzzTimer->InitWithCallback(cb, DEVICECHANGE_HOLD_TIME_IN_MS,
                               nsITimer::TYPE_ONE_SHOT);
}

mozilla::dom::EventHandlerNonNull* MediaDevices::GetOndevicechange() {
  return GetEventHandler(nsGkAtoms::ondevicechange);
}

void MediaDevices::SetOndevicechange(
    mozilla::dom::EventHandlerNonNull* aCallback) {
  SetEventHandler(nsGkAtoms::ondevicechange, aCallback);

  MediaManager::Get()->AddDeviceChangeCallback(this);
}

void MediaDevices::EventListenerAdded(nsAtom* aType) {
  MediaManager::Get()->AddDeviceChangeCallback(this);
  DOMEventTargetHelper::EventListenerAdded(aType);
}

JSObject* MediaDevices::WrapObject(JSContext* aCx,
                                   JS::Handle<JSObject*> aGivenProto) {
  return MediaDevices_Binding::Wrap(aCx, this, aGivenProto);
}

}  // namespace dom
}  // namespace mozilla
