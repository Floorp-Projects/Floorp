/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MediaDevices_h
#define mozilla_dom_MediaDevices_h

#include "MediaEventSource.h"
#include "js/RootingAPI.h"
#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/UseCounter.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "nsCOMPtr.h"
#include "nsID.h"
#include "nsISupports.h"
#include "nsITimer.h"
#include "nsTHashSet.h"

class AudioDeviceInfo;

namespace mozilla {

template <typename ResolveValueT, typename RejectValueT, bool IsExclusive>
class MozPromise;

namespace dom {

class Promise;
struct MediaStreamConstraints;
struct DisplayMediaStreamConstraints;
struct MediaTrackSupportedConstraints;
struct AudioOutputOptions;

class MediaDevices final : public DOMEventTargetHelper {
 public:
  using SinkInfoPromise = MozPromise<RefPtr<AudioDeviceInfo>, nsresult, true>;

  explicit MediaDevices(nsPIDOMWindowInner* aWindow);

  NS_DECL_ISUPPORTS_INHERITED

  JSObject* WrapObject(JSContext* cx,
                       JS::Handle<JSObject*> aGivenProto) override;

  // No code needed, as MediaTrackSupportedConstraints members default to true.
  void GetSupportedConstraints(MediaTrackSupportedConstraints& aResult){};

  already_AddRefed<Promise> GetUserMedia(
      const MediaStreamConstraints& aConstraints, CallerType aCallerType,
      ErrorResult& aRv);

  already_AddRefed<Promise> EnumerateDevices(ErrorResult& aRv);

  already_AddRefed<Promise> GetDisplayMedia(
      const DisplayMediaStreamConstraints& aConstraints, CallerType aCallerType,
      ErrorResult& aRv);

  already_AddRefed<Promise> SelectAudioOutput(
      const AudioOutputOptions& aOptions, CallerType aCallerType,
      ErrorResult& aRv);

  // Get the sink that corresponds to the given device id.
  // The returned promise will be resolved with the device
  // information if the aDeviceId matches a device that would be exposed by
  // enumerateDevices().
  // The promise will be rejected with NS_ERROR_NOT_AVAILABLE if aDeviceId
  // does not match any exposed device.
  RefPtr<SinkInfoPromise> GetSinkDevice(const nsString& aDeviceId);

  // Called when MediaManager encountered a change in its device lists.
  void OnDeviceChange();

  void SetupDeviceChangeListener();

  mozilla::dom::EventHandlerNonNull* GetOndevicechange();
  void SetOndevicechange(mozilla::dom::EventHandlerNonNull* aCallback);

  void EventListenerAdded(nsAtom* aType) override;
  using DOMEventTargetHelper::EventListenerAdded;

 private:
  class GumResolver;
  class EnumDevResolver;
  class GumRejecter;

  virtual ~MediaDevices();

  nsTHashSet<nsString> mExplicitlyGrantedAudioOutputIds;
  nsCOMPtr<nsITimer> mFuzzTimer;

  // Connect/Disconnect on main thread only
  MediaEventListener mDeviceChangeListener;
  bool mIsDeviceChangeListenerSetUp = false;
  bool mCanExposeMicrophoneInfo = false;

  void RecordAccessTelemetry(const UseCounter counter) const;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_MediaDevices_h
