/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_MEDIAMANAGER_H
#define MOZILLA_MEDIAMANAGER_H

#include "MediaEngine.h"
#include "MediaEnginePrefs.h"
#include "mozilla/media/DeviceChangeCallback.h"
#include "mozilla/dom/GetUserMediaRequest.h"
#include "mozilla/Unused.h"
#include "nsAutoPtr.h"
#include "nsIMediaManager.h"

#include "nsHashKeys.h"
#include "nsClassHashtable.h"
#include "nsRefPtrHashtable.h"
#include "nsIObserver.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"

#include "nsIDOMNavigatorUserMedia.h"
#include "nsXULAppAPI.h"
#include "mozilla/Attributes.h"
#include "mozilla/Preferences.h"
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
#  include "mtransport/runnable_utils.h"
#endif

// Note, these suck in Windows headers, unfortunately.
#include "base/thread.h"
#include "base/task.h"

namespace mozilla {
namespace dom {
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
class SourceListener;

class MediaDevice : public nsIMediaDevice {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIMEDIADEVICE

  MediaDevice(const RefPtr<MediaEngineSource>& aSource, const nsString& aName,
              const nsString& aID, const nsString& aGroupID,
              const nsString& aRawID);

  MediaDevice(const RefPtr<AudioDeviceInfo>& aAudioDeviceInfo,
              const nsString& aID, const nsString& aGroupID,
              const nsString& aRawID = NS_LITERAL_STRING(""));

  MediaDevice(const RefPtr<MediaDevice>& aOther, const nsString& aID,
              const nsString& aGroupID, const nsString& aRawID);

  uint32_t GetBestFitnessDistance(
      const nsTArray<const NormalizedConstraintSet*>& aConstraintSets,
      bool aIsChrome);

  nsresult Allocate(const dom::MediaTrackConstraints& aConstraints,
                    const MediaEnginePrefs& aPrefs,
                    const mozilla::ipc::PrincipalInfo& aPrincipalInfo,
                    const char** aOutBadConstraint);
  void SetTrack(const RefPtr<SourceMediaStream>& aStream, TrackID aTrackID,
                const PrincipalHandle& aPrincipal);
  nsresult Start();
  nsresult Reconfigure(const dom::MediaTrackConstraints& aConstraints,
                       const MediaEnginePrefs& aPrefs,
                       const char** aOutBadConstraint);
  nsresult FocusOnSelectedSource();
  nsresult Stop();
  nsresult Deallocate();

  void Pull(const RefPtr<SourceMediaStream>& aStream, TrackID aTrackID,
            StreamTime aEndOfAppendedData, StreamTime aDesiredTime,
            const PrincipalHandle& aPrincipal);

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
  const nsString mType;
  const nsString mName;
  const nsString mID;
  const nsString mGroupID;
  const nsString mRawID;
};

typedef nsRefPtrHashtable<nsUint64HashKey, GetUserMediaWindowListener>
    WindowTable;
typedef MozPromise<RefPtr<AudioDeviceInfo>, nsresult, true> SinkInfoPromise;

class MediaManager final : public nsIMediaManagerService,
                           public nsIObserver,
                           public DeviceChangeCallback {
  friend SourceListener;

 public:
  static already_AddRefed<MediaManager> GetInstance();

  // NOTE: never Dispatch(....,NS_DISPATCH_SYNC) to the MediaManager
  // thread from the MainThread, as we NS_DISPATCH_SYNC to MainThread
  // from MediaManager thread.
  static MediaManager* Get();
  static MediaManager* GetIfExists();
  static void StartupInit();
  static void PostTask(already_AddRefed<Runnable> task);

  /**
   * Posts an async operation to the media manager thread.
   * FunctionType must be a function that takes a `MozPromiseHolder&`.
   *
   * The returned promise is resolved or rejected by aFunction on the media
   * manager thread.
   */
  template <typename MozPromiseType, typename FunctionType>
  static RefPtr<MozPromiseType> PostTask(const char* aName,
                                         FunctionType&& aFunction);

#ifdef DEBUG
  static bool IsInMediaThread();
#endif

  static bool Exists() { return !!sSingleton; }

  static nsresult NotifyRecordingStatusChange(nsPIDOMWindowInner* aWindow);

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIOBSERVER
  NS_DECL_NSIMEDIAMANAGERSERVICE

  media::Parent<media::NonE10s>* GetNonE10sParent();
  MediaEngine* GetBackend();

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

  typedef dom::NavigatorUserMediaSuccessCallback GetUserMediaSuccessCallback;
  typedef dom::NavigatorUserMediaErrorCallback GetUserMediaErrorCallback;

  MOZ_CAN_RUN_SCRIPT
  static void CallOnError(GetUserMediaErrorCallback& aCallback,
                          dom::MediaStreamError& aError);
  MOZ_CAN_RUN_SCRIPT
  static void CallOnSuccess(GetUserMediaSuccessCallback& aCallback,
                            DOMMediaStream& aStream);

  typedef nsTArray<RefPtr<MediaDevice>> MediaDeviceSet;
  typedef media::Refcountable<MediaDeviceSet> MediaDeviceSetRefCnt;

  typedef MozPromise<RefPtr<DOMMediaStream>, RefPtr<MediaMgrError>, true>
      StreamPromise;
  typedef MozPromise<RefPtr<MediaDeviceSetRefCnt>, RefPtr<MediaMgrError>, true>
      DevicesPromise;
  typedef MozPromise<bool, RefPtr<MediaMgrError>, true> MgrPromise;
  typedef MozPromise<const char*, RefPtr<MediaMgrError>, true>
      BadConstraintsPromise;

  RefPtr<StreamPromise> GetUserMedia(
      nsPIDOMWindowInner* aWindow,
      const dom::MediaStreamConstraints& aConstraints,
      dom::CallerType aCallerType);

  MOZ_CAN_RUN_SCRIPT
  nsresult GetUserMediaDevices(
      nsPIDOMWindowInner* aWindow,
      const dom::MediaStreamConstraints& aConstraints,
      dom::MozGetUserMediaDevicesSuccessCallback& aOnSuccess,
      uint64_t aInnerWindowID = 0, const nsAString& aCallID = nsString());
  RefPtr<DevicesPromise> EnumerateDevices(nsPIDOMWindowInner* aWindow,
                                          dom::CallerType aCallerType);

  nsresult EnumerateDevices(nsPIDOMWindowInner* aWindow,
                            dom::Promise& aPromise);

  RefPtr<StreamPromise> GetDisplayMedia(
      nsPIDOMWindowInner* aWindow,
      const dom::DisplayMediaStreamConstraints& aConstraintsPassedIn,
      dom::CallerType aCallerType);

  // Get the sink that corresponds to the given device id.
  // It is resposible to check if an application is
  // authorized to play audio through the requested device.
  // The returned promise will be resolved with the device
  // information if the device id matches one and operation is
  // allowed. The default device is always allowed. Non default
  // devices are allowed only in secure context. It is pending to
  // implement an user authorization model. The promise will be
  // rejected in the following cases:
  // NS_ERROR_NOT_AVAILABLE: Device id does not exist.
  // NS_ERROR_DOM_MEDIA_NOT_ALLOWED_ERR:
  //   The requested device exists but it is not allowed to be used.
  //   Currently, this happens only on non-default default devices
  //   and non https connections. TODO, authorization model to allow
  //   an application to play audio through the device (Bug 1493982).
  // NS_ERROR_ABORT: General error.
  RefPtr<SinkInfoPromise> GetSinkDevice(nsPIDOMWindowInner* aWindow,
                                        const nsString& aDeviceId);

  void OnNavigation(uint64_t aWindowID);
  bool IsActivelyCapturingOrHasAPermission(uint64_t aWindowId);

  MediaEnginePrefs mPrefs;

  virtual int AddDeviceChangeCallback(DeviceChangeCallback* aCallback) override;
  virtual void OnDeviceChange() override;

 private:
  static nsresult GenerateUUID(nsAString& aResult);
  static nsresult AnonymizeId(nsAString& aId, const nsACString& aOriginKey);

 public:  // TODO: make private once we upgrade to GCC 4.8+ on linux.
  static void AnonymizeDevices(MediaDeviceSet& aDevices,
                               const nsACString& aOriginKey,
                               const uint64_t aWindowId);
  static already_AddRefed<nsIWritableVariant> ToJSArray(
      MediaDeviceSet& aDevices);
  static void GuessVideoDeviceGroupIDs(MediaManager::MediaDeviceSet& aDevices);

 private:
  enum class DeviceEnumerationType : uint8_t {
    Normal,  // Enumeration should not return loopback or fake devices
    Fake,    // Enumeration should return fake device(s)
    Loopback /* Enumeration should return loopback device(s) (possibly in
             addition to normal devices) */
  };

  RefPtr<MgrPromise> EnumerateRawDevices(
      uint64_t aWindowId, dom::MediaSourceEnum aVideoInputType,
      dom::MediaSourceEnum aAudioInputType, MediaSinkEnum aAudioOutputType,
      DeviceEnumerationType aVideoInputEnumType,
      DeviceEnumerationType aAudioInputEnumType, bool aForceNoPermRequest,
      const RefPtr<MediaDeviceSetRefCnt>& aOutDevices);

  RefPtr<MgrPromise> EnumerateDevicesImpl(
      uint64_t aWindowId, dom::MediaSourceEnum aVideoInputType,
      dom::MediaSourceEnum aAudioInputType, MediaSinkEnum aAudioOutputType,
      DeviceEnumerationType aVideoInputEnumType,
      DeviceEnumerationType aAudioInputEnumType, bool aForceNoPermRequest,
      const RefPtr<MediaDeviceSetRefCnt>& aOutDevices);

  RefPtr<BadConstraintsPromise> SelectSettings(
      const dom::MediaStreamConstraints& aConstraints, bool aIsChrome,
      const RefPtr<MediaDeviceSetRefCnt>& aSources);

  void GetPref(nsIPrefBranch* aBranch, const char* aPref, const char* aData,
               int32_t* aVal);
  void GetPrefBool(nsIPrefBranch* aBranch, const char* aPref, const char* aData,
                   bool* aVal);
  void GetPrefs(nsIPrefBranch* aBranch, const char* aData);

  // Make private because we want only one instance of this class
  MediaManager();

  ~MediaManager() {}
  void Shutdown();

  void StopScreensharing(uint64_t aWindowID);

  /**
   * Calls aCallback with a GetUserMediaWindowListener argument once for
   * each window listener associated with aWindow and its child windows.
   */
  template <typename FunctionType>
  void IterateWindowListeners(nsPIDOMWindowInner* aWindow,
                              const FunctionType& aCallback);

  void StopMediaStreams();
  void RemoveMediaDevicesCallback(uint64_t aWindowID);

  // ONLY access from MainThread so we don't need to lock
  WindowTable mActiveWindows;
  nsRefPtrHashtable<nsStringHashKey, GetUserMediaTask> mActiveCallbacks;
  nsClassHashtable<nsUint64HashKey, nsTArray<nsString>> mCallIds;
  nsTArray<RefPtr<dom::GetUserMediaRequest>> mPendingGUMRequest;

  // Always exists
  nsAutoPtr<base::Thread> mMediaThread;
  nsCOMPtr<nsIAsyncShutdownBlocker> mShutdownBlocker;

  // ONLY accessed from MediaManagerThread
  RefPtr<MediaEngine> mBackend;

  static StaticRefPtr<MediaManager> sSingleton;

  nsTArray<nsString> mDeviceIDs;

 public:
  RefPtr<media::Parent<media::NonE10s>> mNonE10sParent;
};

}  // namespace mozilla

#endif  // MOZILLA_MEDIAMANAGER_H
