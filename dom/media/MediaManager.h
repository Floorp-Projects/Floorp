/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_MEDIAMANAGER_H
#define MOZILLA_MEDIAMANAGER_H

#include "MediaEngine.h"
#include "MediaEnginePrefs.h"
#include "MediaEventSource.h"
#include "mozilla/dom/GetUserMediaRequest.h"
#include "mozilla/Unused.h"
#include "nsIMediaDevice.h"
#include "nsIMediaManager.h"

#include "nsHashKeys.h"
#include "nsClassHashtable.h"
#include "nsRefPtrHashtable.h"
#include "nsIMemoryReporter.h"
#include "nsIObserver.h"

#include "nsXULAppAPI.h"
#include "mozilla/Attributes.h"
#include "mozilla/Preferences.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/dom/MediaStreamBinding.h"
#include "mozilla/dom/MediaStreamTrackBinding.h"
#include "mozilla/dom/MediaStreamError.h"
#include "mozilla/dom/NavigatorBinding.h"
#include "mozilla/media/MediaChild.h"
#include "mozilla/media/MediaParent.h"
#include "mozilla/Logging.h"
#include "mozilla/UniquePtr.h"
#include "DOMMediaStream.h"

#ifdef MOZ_WEBRTC
#  include "transport/runnable_utils.h"
#endif

class nsIPrefBranch;

namespace mozilla {
class TaskQueue;
class MediaTimer;
namespace dom {
struct AudioOutputOptions;
struct MediaStreamConstraints;
struct MediaTrackConstraints;
struct MediaTrackConstraintSet;
enum class CallerType : uint32_t;
enum class MediaDeviceKind : uint8_t;
}  // namespace dom

namespace ipc {
class PrincipalInfo;
}

class GetUserMediaTask;
class GetUserMediaWindowListener;
class MediaManager;
class DeviceListener;

class MediaDevice : public nsIMediaDevice {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIMEDIADEVICE

  MediaDevice(const RefPtr<MediaEngineSource>& aSource, const nsString& aName,
              const nsString& aID, const nsString& aGroupID,
              const nsString& aRawID);

  MediaDevice(const RefPtr<AudioDeviceInfo>& aAudioDeviceInfo,
              const nsString& aID, const nsString& aGroupID,
              const nsString& aRawID = u""_ns);

  MediaDevice(const RefPtr<MediaDevice>& aOther, const nsString& aID,
              const nsString& aGroupID, const nsString& aRawID,
              const nsString& aRawGroupID);

  MediaDevice(const RefPtr<MediaDevice>& aOther, const nsString& aID,
              const nsString& aGroupID, const nsString& aRawID,
              const nsString& aRawGroupID, const nsString& aName);

  uint32_t GetBestFitnessDistance(
      const nsTArray<const NormalizedConstraintSet*>& aConstraintSets,
      dom::CallerType aCallerType);

  nsresult Allocate(const dom::MediaTrackConstraints& aConstraints,
                    const MediaEnginePrefs& aPrefs, uint64_t aWindowId,
                    const char** aOutBadConstraint);
  void SetTrack(const RefPtr<MediaTrack>& aTrack,
                const PrincipalHandle& aPrincipal);
  nsresult Start();
  nsresult Reconfigure(const dom::MediaTrackConstraints& aConstraints,
                       const MediaEnginePrefs& aPrefs,
                       const char** aOutBadConstraint);
  nsresult FocusOnSelectedSource();
  nsresult Stop();
  nsresult Deallocate();

  void GetSettings(dom::MediaTrackSettings& aOutSettings) const;

  dom::MediaSourceEnum GetMediaSource() const;

 protected:
  virtual ~MediaDevice() = default;

  static uint32_t FitnessDistance(
      nsString aN,
      const dom::OwningStringOrStringSequenceOrConstrainDOMStringParameters&
          aConstraint);

 private:
  static bool StringsContain(const dom::OwningStringOrStringSequence& aStrings,
                             nsString aN);
  static uint32_t FitnessDistance(
      nsString aN, const dom::ConstrainDOMStringParameters& aParams);

 public:
  const RefPtr<MediaEngineSource> mSource;
  const RefPtr<AudioDeviceInfo> mSinkInfo;
  const dom::MediaDeviceKind mKind;
  const bool mScary;
  const bool mIsFake;
  const nsString mType;
  const nsString mName;
  const nsString mID;
  const nsString mGroupID;
  const nsString mRawID;
  const nsString mRawGroupID;
  const nsString mRawName;
};

typedef nsRefPtrHashtable<nsUint64HashKey, GetUserMediaWindowListener>
    WindowTable;

class MediaManager final : public nsIMediaManagerService,
                           public nsIMemoryReporter,
                           public nsIObserver {
  friend DeviceListener;

 public:
  static already_AddRefed<MediaManager> GetInstance();

  // NOTE: never Dispatch(....,NS_DISPATCH_SYNC) to the MediaManager
  // thread from the MainThread, as we NS_DISPATCH_SYNC to MainThread
  // from MediaManager thread.
  static MediaManager* Get();
  static MediaManager* GetIfExists();
  static void StartupInit();
  static void Dispatch(already_AddRefed<Runnable> task);

  /**
   * Posts an async operation to the media manager thread.
   * FunctionType must be a function that takes a `MozPromiseHolder&`.
   *
   * The returned promise is resolved or rejected by aFunction on the media
   * manager thread.
   */
  template <typename MozPromiseType, typename FunctionType>
  static RefPtr<MozPromiseType> Dispatch(const char* aName,
                                         FunctionType&& aFunction);

#ifdef DEBUG
  static bool IsInMediaThread();
#endif

  static bool Exists() { return !!GetIfExists(); }

  static nsresult NotifyRecordingStatusChange(nsPIDOMWindowInner* aWindow);

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIOBSERVER
  NS_DECL_NSIMEMORYREPORTER
  NS_DECL_NSIMEDIAMANAGERSERVICE

  media::Parent<media::NonE10s>* GetNonE10sParent();
  MediaEngine* GetBackend();

  // If the window has not been destroyed, then return the
  // GetUserMediaWindowListener for this window.
  // If the window has been destroyed, then return null.
  RefPtr<GetUserMediaWindowListener> GetOrMakeWindowListener(
      nsPIDOMWindowInner* aWindow);
  WindowTable* GetActiveWindows() {
    MOZ_ASSERT(NS_IsMainThread());
    return &mActiveWindows;
  }
  GetUserMediaWindowListener* GetWindowListener(uint64_t aWindowId) {
    MOZ_ASSERT(NS_IsMainThread());
    return mActiveWindows.GetWeak(aWindowId);
  }
  void AddWindowID(uint64_t aWindowId,
                   RefPtr<GetUserMediaWindowListener> aListener);
  void RemoveWindowID(uint64_t aWindowId);
  void SendPendingGUMRequest();
  bool IsWindowStillActive(uint64_t aWindowId) {
    return !!GetWindowListener(aWindowId);
  }
  bool IsWindowListenerStillActive(
      const RefPtr<GetUserMediaWindowListener>& aListener);

  static bool IsOn(const dom::OwningBooleanOrMediaTrackConstraints& aUnion) {
    return !aUnion.IsBoolean() || aUnion.GetAsBoolean();
  }
  using GetUserMediaSuccessCallback = dom::NavigatorUserMediaSuccessCallback;
  using GetUserMediaErrorCallback = dom::NavigatorUserMediaErrorCallback;

  MOZ_CAN_RUN_SCRIPT
  static void CallOnError(GetUserMediaErrorCallback& aCallback,
                          dom::MediaStreamError& aError);
  MOZ_CAN_RUN_SCRIPT
  static void CallOnSuccess(GetUserMediaSuccessCallback& aCallback,
                            DOMMediaStream& aTrack);

  using MediaDeviceSet = nsTArray<RefPtr<MediaDevice>>;
  using MediaDeviceSetRefCnt = media::Refcountable<MediaDeviceSet>;

  using StreamPromise =
      MozPromise<RefPtr<DOMMediaStream>, RefPtr<MediaMgrError>, true>;
  using DevicePromise =
      MozPromise<RefPtr<MediaDevice>, RefPtr<MediaMgrError>, true>;
  using DeviceSetPromise =
      MozPromise<RefPtr<MediaDeviceSetRefCnt>, RefPtr<MediaMgrError>, true>;
  using MgrPromise = MozPromise<bool, RefPtr<MediaMgrError>, true>;
  using BadConstraintsPromise =
      MozPromise<const char*, RefPtr<MediaMgrError>, true>;

  RefPtr<StreamPromise> GetUserMedia(
      nsPIDOMWindowInner* aWindow,
      const dom::MediaStreamConstraints& aConstraints,
      dom::CallerType aCallerType);

  RefPtr<DeviceSetPromise> EnumerateDevices(nsPIDOMWindowInner* aWindow,
                                            dom::CallerType aCallerType);

  enum class DeviceEnumerationType : uint8_t {
    Normal,  // Enumeration should not return loopback or fake devices
    Fake,    // Enumeration should return fake device(s)
    Loopback /* Enumeration should return loopback device(s) (possibly in
             addition to normal devices) */
  };
  RefPtr<MgrPromise> EnumerateDevicesImpl(
      nsPIDOMWindowInner* aWindow, dom::MediaSourceEnum aVideoInputType,
      dom::MediaSourceEnum aAudioInputType, MediaSinkEnum aAudioOutputType,
      DeviceEnumerationType aVideoInputEnumType,
      DeviceEnumerationType aAudioInputEnumType, bool aForceNoPermRequest,
      const RefPtr<MediaDeviceSetRefCnt>& aOutDevices);

  RefPtr<DevicePromise> SelectAudioOutput(
      nsPIDOMWindowInner* aWindow, const dom::AudioOutputOptions& aOptions,
      dom::CallerType aCallerType);

  void OnNavigation(uint64_t aWindowID);
  void OnCameraMute(bool aMute);
  void OnMicrophoneMute(bool aMute);
  bool IsActivelyCapturingOrHasAPermission(uint64_t aWindowId);

  MediaEventSource<void>& DeviceListChangeEvent() {
    return mDeviceListChangeEvent;
  }

  MediaEnginePrefs mPrefs;

 private:
  static nsresult GenerateUUID(nsAString& aResult);
  static nsresult AnonymizeId(nsAString& aId, const nsACString& aOriginKey);

 public:  // TODO: make private once we upgrade to GCC 4.8+ on linux.
  static void AnonymizeDevices(MediaDeviceSet& aDevices,
                               const nsACString& aOriginKey,
                               const uint64_t aWindowId);

  /**
   * This function tries to guess the group id for a video device in aDevices
   * based on the device name. If the name of only one audio device in aAudios
   * contains the name of the video device, then, this video device will take
   * the group id of the audio device. Since this is a guess we try to minimize
   * the probability of false positive. If we fail to find a correlation we
   * leave the video group id untouched. In that case the group id will be the
   * video device name.
   */
  static void GuessVideoDeviceGroupIDs(MediaDeviceSet& aDevices,
                                       const MediaDeviceSet& aAudios);

 private:
  RefPtr<MgrPromise> EnumerateRawDevices(
      uint64_t aWindowId, dom::MediaSourceEnum aVideoInputType,
      dom::MediaSourceEnum aAudioInputType, MediaSinkEnum aAudioOutputType,
      DeviceEnumerationType aVideoInputEnumType,
      DeviceEnumerationType aAudioInputEnumType, bool aForceNoPermRequest,
      const RefPtr<MediaDeviceSetRefCnt>& aOutDevices);

  RefPtr<BadConstraintsPromise> SelectSettings(
      const dom::MediaStreamConstraints& aConstraints,
      dom::CallerType aCallerType,
      const RefPtr<MediaDeviceSetRefCnt>& aSources);

  void GetPref(nsIPrefBranch* aBranch, const char* aPref, const char* aData,
               int32_t* aVal);
  void GetPrefBool(nsIPrefBranch* aBranch, const char* aPref, const char* aData,
                   bool* aVal);
  void GetPrefs(nsIPrefBranch* aBranch, const char* aData);

  // Make private because we want only one instance of this class
  explicit MediaManager(already_AddRefed<TaskQueue> aMediaThread);

  ~MediaManager() = default;
  void Shutdown();

  void StopScreensharing(uint64_t aWindowID);

  void RemoveMediaDevicesCallback(uint64_t aWindowID);
  void DeviceListChanged();

  // Returns the number of incomplete tasks associated with this window,
  // including the newly added task.
  size_t AddTaskAndGetCount(uint64_t aWindowID, const nsAString& aCallID,
                            RefPtr<GetUserMediaTask> aTask);
  // Finds the task corresponding to aCallID and removes it from tracking.
  RefPtr<GetUserMediaTask> TakeGetUserMediaTask(const nsAString& aCallID);
  // Intended for use with "media.navigator.permission.disabled" to bypass the
  // permission prompt and use the first appropriate device.
  void NotifyAllowed(const nsString& aCallID, const MediaDeviceSet& aDevices);

  MOZ_DEFINE_MALLOC_SIZE_OF(MallocSizeOf);

  struct nsStringHasher {
    using Key = nsString;
    using Lookup = nsString;

    static HashNumber hash(const Lookup& aLookup) {
      return HashString(aLookup.get());
    }

    static bool match(const Key& aKey, const Lookup& aLookup) {
      return aKey == aLookup;
    }
  };

  // ONLY access from MainThread so we don't need to lock
  WindowTable mActiveWindows;
  nsRefPtrHashtable<nsStringHashKey, GetUserMediaTask> mActiveCallbacks;
  nsClassHashtable<nsUint64HashKey, nsTArray<nsString>> mCallIds;
  nsTArray<RefPtr<dom::GetUserMediaRequest>> mPendingGUMRequest;
  using DeviceIdSet = HashSet<nsString, nsStringHasher, InfallibleAllocPolicy>;
  DeviceIdSet mDeviceIDs;
  RefPtr<MediaTimer> mDeviceChangeTimer;
  bool mCamerasMuted = false;
  bool mMicrophonesMuted = false;

  // Always exists
  const RefPtr<TaskQueue> mMediaThread;
  nsCOMPtr<nsIAsyncShutdownBlocker> mShutdownBlocker;

  // ONLY accessed from MediaManagerThread
  RefPtr<MediaEngine> mBackend;

  static StaticRefPtr<MediaManager> sSingleton;
  static StaticMutex sSingletonMutex;

  // Connect/Disconnect on media thread only
  MediaEventListener mDeviceListChangeListener;

  MediaEventProducer<void> mDeviceListChangeEvent;

 public:
  RefPtr<media::Parent<media::NonE10s>> mNonE10sParent;
};

}  // namespace mozilla

#endif  // MOZILLA_MEDIAMANAGER_H
