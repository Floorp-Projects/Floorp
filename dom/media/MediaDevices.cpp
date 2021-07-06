/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/MediaDevices.h"

#include "AudioDeviceInfo.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/MediaStreamBinding.h"
#include "mozilla/dom/MediaDeviceInfo.h"
#include "mozilla/dom/MediaDevicesBinding.h"
#include "mozilla/dom/NavigatorBinding.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/WindowContext.h"
#include "mozilla/MediaManager.h"
#include "MediaTrackConstraints.h"
#include "nsContentUtils.h"
#include "nsINamed.h"
#include "nsIScriptGlobalObject.h"
#include "nsPIDOMWindow.h"
#include "nsQueryObject.h"

#define DEVICECHANGE_HOLD_TIME_IN_MS 1000

namespace mozilla::dom {

using DeviceEnumerationType = MediaManager::DeviceEnumerationType;

MediaDevices::MediaDevices(nsPIDOMWindowInner* aWindow)
    : DOMEventTargetHelper(aWindow) {}

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
  MOZ_ASSERT(NS_IsMainThread());
  // Get the relevant global for the promise from the wrapper cache because
  // DOMEventTargetHelper::GetOwner() returns null if the document is unloaded.
  // We know the wrapper exists because it is being used for |this| from JS.
  // See https://github.com/heycam/webidl/issues/932 for why the relevant
  // global is used instead of the current global.
  nsCOMPtr<nsIGlobalObject> global = xpc::NativeGlobal(GetWrapper());
  // global is a window because MediaDevices is exposed only to Window.
  nsCOMPtr<nsPIDOMWindowInner> owner = do_QueryInterface(global);
  if (Document* doc = owner->GetExtantDoc()) {
    if (!owner->IsSecureContext()) {
      doc->SetUseCounter(eUseCounter_custom_GetUserMediaInsec);
    }
    Document* topDoc = doc->GetTopLevelContentDocumentIfSameProcess();
    IgnoredErrorResult ignored;
    if (topDoc && !topDoc->HasFocus(ignored)) {
      doc->SetUseCounter(eUseCounter_custom_GetUserMediaUnfocused);
    }
  }
  RefPtr<Promise> p = Promise::Create(global, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }
  /* If requestedMediaTypes is the empty set, return a promise rejected with a
   * TypeError. */
  if (!MediaManager::IsOn(aConstraints.mVideo) &&
      !MediaManager::IsOn(aConstraints.mAudio)) {
    p->MaybeRejectWithTypeError("audio and/or video is required");
    return p.forget();
  }
  /* If the relevant settings object's responsible document is NOT fully
   * active, return a promise rejected with a DOMException object whose name
   * attribute has the value "InvalidStateError". */
  if (!owner->IsFullyActive()) {
    p->MaybeRejectWithInvalidStateError("The document is not fully active.");
    return p.forget();
  }
  const OwningBooleanOrMediaTrackConstraints& audio = aConstraints.mAudio;
  bool isMicrophone =
      audio.IsBoolean()
          ? audio.GetAsBoolean()
          : !audio.GetAsMediaTrackConstraints().mMediaSource.WasPassed();
  RefPtr<MediaDevices> self(this);
  MediaManager::Get()
      ->GetUserMedia(owner, aConstraints, aCallerType)
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [this, self, p, isMicrophone](RefPtr<DOMMediaStream>&& aStream) {
            if (!GetWindowIfCurrent()) {
              return;  // Leave Promise pending after navigation by design.
            }
            if (isMicrophone) {
              mCanExposeMicrophoneInfo = true;
            }
            p->MaybeResolve(std::move(aStream));
          },
          [this, self, p](const RefPtr<MediaMgrError>& error) {
            nsPIDOMWindowInner* window = GetWindowIfCurrent();
            if (!window) {
              return;  // Leave Promise pending after navigation by design.
            }
            error->Reject(p);
          });
  return p.forget();
}

already_AddRefed<Promise> MediaDevices::EnumerateDevices(CallerType aCallerType,
                                                         ErrorResult& aRv) {
  MOZ_ASSERT(NS_IsMainThread());
  nsCOMPtr<nsIGlobalObject> global = xpc::NativeGlobal(GetWrapper());
  nsCOMPtr<nsPIDOMWindowInner> owner = do_QueryInterface(global);
  if (Document* doc = owner->GetExtantDoc()) {
    if (!owner->IsSecureContext()) {
      doc->SetUseCounter(eUseCounter_custom_EnumerateDevicesInsec);
    }
    Document* topDoc = doc->GetTopLevelContentDocumentIfSameProcess();
    IgnoredErrorResult ignored;
    if (topDoc && !topDoc->HasFocus(ignored)) {
      doc->SetUseCounter(eUseCounter_custom_EnumerateDevicesUnfocused);
    }
  }
  RefPtr<Promise> p = Promise::Create(global, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }
  RefPtr<MediaDevices> self(this);
  MediaManager::Get()
      ->EnumerateDevices(owner, aCallerType)
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
            nsTHashSet<nsString> exposedMicrophoneGroupIds;
            for (auto& device : *aDevices) {
              nsString label;
              MOZ_ASSERT(device->mKind < MediaDeviceKind::EndGuard_);
              switch (device->mKind) {
                case MediaDeviceKind::Audioinput:
                  if (mCanExposeMicrophoneInfo) {
                    exposedMicrophoneGroupIds.Insert(device->mGroupID);
                  }
                  [[fallthrough]];
                case MediaDeviceKind::Videoinput:
                  // Include name only if page currently has a gUM stream
                  // active or persistent permissions (audio or video) have
                  // been granted.  See bug 1528042 for using
                  // mCanExposeMicrophoneInfo.
                  if (allowLabel ||
                      Preferences::GetBool(
                          "media.navigator.permission.disabled", false)) {
                    label = device->mName;
                  }
                  break;
                case MediaDeviceKind::Audiooutput:
                  if (!mExplicitlyGrantedAudioOutputIds.Contains(device->mID) &&
                      // Assumes aDevices order has microphones before speakers.
                      !exposedMicrophoneGroupIds.Contains(device->mGroupID)) {
                    continue;
                  }
                  label = device->mName;
                  break;
                case MediaDeviceKind::EndGuard_:
                  break;
                  // Avoid `default:` so that `-Wswitch` catches missing
                  // enumerators at compile time.
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
            error->Reject(p);
          });
  return p.forget();
}

already_AddRefed<Promise> MediaDevices::GetDisplayMedia(
    const DisplayMediaStreamConstraints& aConstraints, CallerType aCallerType,
    ErrorResult& aRv) {
  nsCOMPtr<nsIGlobalObject> global = xpc::NativeGlobal(GetWrapper());
  RefPtr<Promise> p = Promise::Create(global, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }
  nsCOMPtr<nsPIDOMWindowInner> owner = do_QueryInterface(global);
  /* TODO: bug 1705289
   * If the relevant global object of this does not have transient activation,
   * return a promise rejected with a DOMException object whose name attribute
   * has the value InvalidStateError. */
  WindowContext* wc = owner->GetWindowContext();
  if (!wc || !wc->HasBeenUserGestureActivated()) {
    p->MaybeRejectWithInvalidStateError(
        "getDisplayMedia must be called from a user gesture handler.");
    return p.forget();
  }
  /* If constraints.video is false, return a promise rejected with a newly
   * created TypeError. */
  if (!MediaManager::IsOn(aConstraints.mVideo)) {
    p->MaybeRejectWithTypeError("video is required");
    return p.forget();
  }
  MediaStreamConstraints c;
  auto& vc = c.mVideo.SetAsMediaTrackConstraints();

  if (aConstraints.mVideo.IsMediaTrackConstraints()) {
    vc = aConstraints.mVideo.GetAsMediaTrackConstraints();
    /* If CS contains a member named advanced, return a promise rejected with
     * a newly created TypeError. */
    if (vc.mAdvanced.WasPassed()) {
      p->MaybeRejectWithTypeError("advanced not allowed");
      return p.forget();
    }
    auto getCLR = [](const auto& aCon) -> const ConstrainLongRange& {
      static ConstrainLongRange empty;
      return (aCon.WasPassed() && !aCon.Value().IsLong())
                 ? aCon.Value().GetAsConstrainLongRange()
                 : empty;
    };
    auto getCDR = [](auto&& aCon) -> const ConstrainDoubleRange& {
      static ConstrainDoubleRange empty;
      return (aCon.WasPassed() && !aCon.Value().IsDouble())
                 ? aCon.Value().GetAsConstrainDoubleRange()
                 : empty;
    };
    const auto& w = getCLR(vc.mWidth);
    const auto& h = getCLR(vc.mHeight);
    const auto& f = getCDR(vc.mFrameRate);
    /* If CS contains a member whose name specifies a constrainable property
     * applicable to display surfaces, and whose value in turn is a dictionary
     * containing a member named either min or exact, return a promise
     * rejected with a newly created TypeError. */
    if (w.mMin.WasPassed() || h.mMin.WasPassed() || f.mMin.WasPassed()) {
      p->MaybeRejectWithTypeError("min not allowed");
      return p.forget();
    }
    if (w.mExact.WasPassed() || h.mExact.WasPassed() || f.mExact.WasPassed()) {
      p->MaybeRejectWithTypeError("exact not allowed");
      return p.forget();
    }
    /* If CS contains a member whose name, failedConstraint specifies a
     * constrainable property, constraint, applicable to display surfaces, and
     * whose value in turn is a dictionary containing a member named max, and
     * that member's value in turn is less than the constrainable property's
     * floor value, then let failedConstraint be the name of the constraint,
     * let message be either undefined or an informative human-readable
     * message, and return a promise rejected with a new OverconstrainedError
     * created by calling OverconstrainedError(failedConstraint, message). */
    // We fail early without incurring a prompt, on known-to-fail constraint
    // values that don't reveal anything about the user's system.
    const char* badConstraint = nullptr;
    if (w.mMax.WasPassed() && w.mMax.Value() < 1) {
      badConstraint = "width";
    }
    if (h.mMax.WasPassed() && h.mMax.Value() < 1) {
      badConstraint = "height";
    }
    if (f.mMax.WasPassed() && f.mMax.Value() < 1) {
      badConstraint = "frameRate";
    }
    if (badConstraint) {
      p->MaybeReject(MakeRefPtr<dom::MediaStreamError>(
          owner, *MakeRefPtr<MediaMgrError>(
                     MediaMgrError::Name::OverconstrainedError, "",
                     NS_ConvertASCIItoUTF16(badConstraint))));
      return p.forget();
    }
  }
  /* If the relevant settings object's responsible document is NOT fully
   * active, return a promise rejected with a DOMException object whose name
   * attribute has the value "InvalidStateError". */
  if (!owner->IsFullyActive()) {
    p->MaybeRejectWithInvalidStateError("The document is not fully active.");
    return p.forget();
  }
  // We ask for "screen" sharing.
  //
  // If this is a privileged call or permission is disabled, this gives us full
  // screen sharing by default, which is useful for internal testing.
  //
  // If this is a non-priviliged call, GetUserMedia() will change it to "window"
  // for us.
  vc.mMediaSource.Reset();
  vc.mMediaSource.Construct().AssignASCII(
      dom::MediaSourceEnumValues::GetString(MediaSourceEnum::Screen));

  RefPtr<MediaDevices> self(this);
  MediaManager::Get()
      ->GetUserMedia(owner, c, aCallerType)
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
            error->Reject(p);
          });
  return p.forget();
}

already_AddRefed<Promise> MediaDevices::SelectAudioOutput(
    const AudioOutputOptions& aOptions, CallerType aCallerType,
    ErrorResult& aRv) {
  nsCOMPtr<nsIGlobalObject> global = xpc::NativeGlobal(GetWrapper());
  RefPtr<Promise> p = Promise::Create(global, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }
  /* (This includes the expected user activation update of
   * https://github.com/w3c/mediacapture-output/issues/107)
   * If the relevant global object of this does not have transient activation,
   * return a promise rejected with a DOMException object whose name attribute
   * has the value InvalidStateError. */
  nsCOMPtr<nsPIDOMWindowInner> owner = do_QueryInterface(global);
  WindowContext* wc = owner->GetWindowContext();
  if (!wc || !wc->HasValidTransientUserGestureActivation()) {
    p->MaybeRejectWithInvalidStateError(
        "selectAudioOutput requires transient user activation.");
    return p.forget();
  }
  RefPtr<MediaDevices> self(this);
  MediaManager::Get()
      ->SelectAudioOutput(owner, aOptions, aCallerType)
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [this, self, p](RefPtr<MediaDevice> aDevice) {
            nsPIDOMWindowInner* window = GetWindowIfCurrent();
            if (!window) {
              return;  // Leave Promise pending after navigation by design.
            }
            MOZ_ASSERT(aDevice->mKind == dom::MediaDeviceKind::Audiooutput);
            mExplicitlyGrantedAudioOutputIds.Insert(aDevice->mID);
            p->MaybeResolve(
                MakeRefPtr<MediaDeviceInfo>(aDevice->mID, aDevice->mKind,
                                            aDevice->mName, aDevice->mGroupID));
          },
          [this, self, p](const RefPtr<MediaMgrError>& error) {
            nsPIDOMWindowInner* window = GetWindowIfCurrent();
            if (!window) {
              return;  // Leave Promise pending after navigation by design.
            }
            error->Reject(p);
          });
  return p.forget();
}

static RefPtr<AudioDeviceInfo> CopyWithNullDeviceId(
    AudioDeviceInfo* aDeviceInfo) {
  MOZ_ASSERT(aDeviceInfo->Preferred());

  nsString vendor;
  aDeviceInfo->GetVendor(vendor);
  uint16_t type;
  aDeviceInfo->GetType(&type);
  uint16_t state;
  aDeviceInfo->GetState(&state);
  uint16_t pref;
  aDeviceInfo->GetPreferred(&pref);
  uint16_t supportedFormat;
  aDeviceInfo->GetSupportedFormat(&supportedFormat);
  uint16_t defaultFormat;
  aDeviceInfo->GetDefaultFormat(&defaultFormat);
  uint32_t maxChannels;
  aDeviceInfo->GetMaxChannels(&maxChannels);
  uint32_t defaultRate;
  aDeviceInfo->GetDefaultRate(&defaultRate);
  uint32_t maxRate;
  aDeviceInfo->GetMaxRate(&maxRate);
  uint32_t minRate;
  aDeviceInfo->GetMinRate(&minRate);
  uint32_t maxLatency;
  aDeviceInfo->GetMaxLatency(&maxLatency);
  uint32_t minLatency;
  aDeviceInfo->GetMinLatency(&minLatency);

  return MakeRefPtr<AudioDeviceInfo>(
      nullptr, aDeviceInfo->Name(), aDeviceInfo->GroupID(), vendor, type, state,
      pref, supportedFormat, defaultFormat, maxChannels, defaultRate, maxRate,
      minRate, maxLatency, minLatency);
}

RefPtr<MediaDevices::SinkInfoPromise> MediaDevices::GetSinkDevice(
    const nsString& aDeviceId) {
  MOZ_ASSERT(NS_IsMainThread());

  bool isExposed = aDeviceId.IsEmpty() ||
                   mExplicitlyGrantedAudioOutputIds.Contains(aDeviceId);
  // If the device id is not exposed, then check microphone groupIds.
  MediaSourceEnum audioInputType = isExposed || !mCanExposeMicrophoneInfo
                                       ? MediaSourceEnum::Other
                                       : MediaSourceEnum::Microphone;

  auto devices = MakeRefPtr<MediaManager::MediaDeviceSetRefCnt>();
  return MediaManager::Get()
      ->EnumerateDevicesImpl(GetOwner(), MediaSourceEnum::Other, audioInputType,
                             MediaSinkEnum::Speaker,
                             DeviceEnumerationType::Normal,
                             DeviceEnumerationType::Normal, true, devices)
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [aDeviceId, isExposed, devices](bool) mutable {
            RefPtr<AudioDeviceInfo> outputInfo;
            nsString groupId;
            // Check for a matching device.
            for (const RefPtr<MediaDevice>& device : *devices) {
              if (device->mKind != dom::MediaDeviceKind::Audiooutput) {
                continue;
              }
              if (aDeviceId.IsEmpty()) {
                if (device->mSinkInfo->Preferred()) {
                  outputInfo = CopyWithNullDeviceId(device->mSinkInfo);
                  break;
                }
              } else if (aDeviceId.Equals(device->mID)) {
                outputInfo = device->mSinkInfo;
                groupId = device->mGroupID;
                break;
              }
            }
            if (outputInfo && !isExposed) {
              // Check microphone groups.
              MOZ_ASSERT(!groupId.IsEmpty());
              for (const RefPtr<MediaDevice>& device : *devices) {
                if (device->mKind != dom::MediaDeviceKind::Audioinput) {
                  continue;
                }
                if (groupId.Equals(device->mGroupID)) {
                  isExposed = true;
                  break;
                }
              }
            }
            /* If sinkId is not the empty string and does not match any audio
             * output device identified by the result that would be provided
             * by enumerateDevices(), reject p with a new DOMException whose
             * name is NotFoundError and abort these substeps. */
            if (!outputInfo || !isExposed) {
              return SinkInfoPromise::CreateAndReject(NS_ERROR_NOT_AVAILABLE,
                                                      __func__);
            }
            return SinkInfoPromise::CreateAndResolve(outputInfo, __func__);
          },
          // aRejectMethod =
          [](RefPtr<MediaMgrError>&& aError) {
            return SinkInfoPromise::CreateAndReject(NS_ERROR_NOT_AVAILABLE,
                                                    __func__);
          });
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

}  // namespace mozilla::dom
