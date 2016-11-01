/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_MEDIAMANAGER_H
#define MOZILLA_MEDIAMANAGER_H

#include "MediaEngine.h"
#include "mozilla/media/DeviceChangeCallback.h"
#include "mozilla/Services.h"
#include "mozilla/Unused.h"
#include "nsAutoPtr.h"
#include "nsIMediaManager.h"

#include "nsHashKeys.h"
#include "nsGlobalWindow.h"
#include "nsClassHashtable.h"
#include "nsRefPtrHashtable.h"
#include "nsIObserver.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"

#include "nsPIDOMWindow.h"
#include "nsIDOMNavigatorUserMedia.h"
#include "nsXULAppAPI.h"
#include "mozilla/Attributes.h"
#include "mozilla/Preferences.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/dom/MediaStreamBinding.h"
#include "mozilla/dom/MediaStreamTrackBinding.h"
#include "mozilla/dom/MediaStreamError.h"
#include "mozilla/media/MediaChild.h"
#include "mozilla/media/MediaParent.h"
#include "mozilla/Logging.h"
#include "mozilla/UniquePtr.h"
#include "DOMMediaStream.h"

#ifdef MOZ_WEBRTC
#include "mtransport/runnable_utils.h"
#endif

// Note, these suck in Windows headers, unfortunately.
#include "base/thread.h"
#include "base/task.h"

namespace mozilla {
namespace dom {
struct MediaStreamConstraints;
struct MediaTrackConstraints;
struct MediaTrackConstraintSet;
} // namespace dom

class MediaManager;
class GetUserMediaCallbackMediaStreamListener;
class GetUserMediaTask;

extern LogModule* GetMediaManagerLog();
#define MM_LOG(msg) MOZ_LOG(GetMediaManagerLog(), mozilla::LogLevel::Debug, msg)

class MediaDevice : public nsIMediaDevice
{
public:
  typedef MediaEngineSource Source;

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIMEDIADEVICE

  void SetId(const nsAString& aID);
  void SetRawId(const nsAString& aID);
  virtual uint32_t GetBestFitnessDistance(
      const nsTArray<const NormalizedConstraintSet*>& aConstraintSets,
      bool aIsChrome);
  virtual Source* GetSource() = 0;
  nsresult Allocate(const dom::MediaTrackConstraints &aConstraints,
                    const MediaEnginePrefs &aPrefs,
                    const nsACString& aOrigin,
                    const char** aOutBadConstraint);
  nsresult Restart(const dom::MediaTrackConstraints &aConstraints,
                   const MediaEnginePrefs &aPrefs,
                   const char** aOutBadConstraint);
  nsresult Deallocate();
protected:
  virtual ~MediaDevice() {}
  explicit MediaDevice(MediaEngineSource* aSource, bool aIsVideo);

  static uint32_t FitnessDistance(nsString aN,
    const dom::OwningStringOrStringSequenceOrConstrainDOMStringParameters& aConstraint);
private:
  static bool StringsContain(const dom::OwningStringOrStringSequence& aStrings,
                             nsString aN);
  static uint32_t FitnessDistance(nsString aN,
      const dom::ConstrainDOMStringParameters& aParams);
protected:
  nsString mName;
  nsString mID;
  nsString mRawID;
  bool mScary;
  dom::MediaSourceEnum mMediaSource;
  RefPtr<MediaEngineSource> mSource;
  RefPtr<MediaEngineSource::AllocationHandle> mAllocationHandle;
public:
  dom::MediaSourceEnum GetMediaSource() {
    return mMediaSource;
  }
  bool mIsVideo;
};

class VideoDevice : public MediaDevice
{
public:
  typedef MediaEngineVideoSource Source;

  explicit VideoDevice(Source* aSource);
  NS_IMETHOD GetType(nsAString& aType) override;
  Source* GetSource() override;
};

class AudioDevice : public MediaDevice
{
public:
  typedef MediaEngineAudioSource Source;

  explicit AudioDevice(Source* aSource);
  NS_IMETHOD GetType(nsAString& aType) override;
  Source* GetSource() override;
};

class GetUserMediaNotificationEvent: public Runnable
{
  public:
    enum GetUserMediaStatus {
      STARTING,
      STOPPING,
      STOPPED_TRACK,
    };
    GetUserMediaNotificationEvent(GetUserMediaCallbackMediaStreamListener* aListener,
                                  GetUserMediaStatus aStatus,
                                  bool aIsAudio, bool aIsVideo, uint64_t aWindowID);

    GetUserMediaNotificationEvent(GetUserMediaStatus aStatus,
                                  already_AddRefed<DOMMediaStream> aStream,
                                  OnTracksAvailableCallback* aOnTracksAvailableCallback,
                                  bool aIsAudio, bool aIsVideo, uint64_t aWindowID,
                                  already_AddRefed<nsIDOMGetUserMediaErrorCallback> aError);
    virtual ~GetUserMediaNotificationEvent();

    NS_IMETHOD Run() override;

  protected:
    RefPtr<GetUserMediaCallbackMediaStreamListener> mListener; // threadsafe
    RefPtr<DOMMediaStream> mStream;
    nsAutoPtr<OnTracksAvailableCallback> mOnTracksAvailableCallback;
    GetUserMediaStatus mStatus;
    bool mIsAudio;
    bool mIsVideo;
    uint64_t mWindowID;
    RefPtr<nsIDOMGetUserMediaErrorCallback> mOnFailure;
};

typedef enum {
  MEDIA_START,
  MEDIA_STOP,
  MEDIA_STOP_TRACK,
  MEDIA_DIRECT_LISTENERS,
} MediaOperation;

class ReleaseMediaOperationResource : public Runnable
{
public:
  ReleaseMediaOperationResource(already_AddRefed<DOMMediaStream> aStream,
    OnTracksAvailableCallback* aOnTracksAvailableCallback):
    mStream(aStream),
    mOnTracksAvailableCallback(aOnTracksAvailableCallback) {}
  NS_IMETHOD Run() override {return NS_OK;}
private:
  RefPtr<DOMMediaStream> mStream;
  nsAutoPtr<OnTracksAvailableCallback> mOnTracksAvailableCallback;
};

typedef nsTArray<RefPtr<GetUserMediaCallbackMediaStreamListener> > StreamListeners;
typedef nsClassHashtable<nsUint64HashKey, StreamListeners> WindowTable;

// we could add MediaManager if needed
typedef void (*WindowListenerCallback)(MediaManager *aThis,
                                       uint64_t aWindowID,
                                       StreamListeners *aListeners,
                                       void *aData);

class MediaManager final : public nsIMediaManagerService,
                           public nsIObserver
                          ,public DeviceChangeCallback
{
  friend GetUserMediaCallbackMediaStreamListener;
public:
  static already_AddRefed<MediaManager> GetInstance();

  // NOTE: never Dispatch(....,NS_DISPATCH_SYNC) to the MediaManager
  // thread from the MainThread, as we NS_DISPATCH_SYNC to MainThread
  // from MediaManager thread.
  static MediaManager* Get();
  static MediaManager* GetIfExists();
  static void StartupInit();
  static void PostTask(already_AddRefed<Runnable> task);
#ifdef DEBUG
  static bool IsInMediaThread();
#endif

  static bool Exists()
  {
    return !!sSingleton;
  }

  static nsresult NotifyRecordingStatusChange(nsPIDOMWindowInner* aWindow,
                                              const nsString& aMsg,
                                              const bool& aIsAudio,
                                              const bool& aIsVideo);

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIOBSERVER
  NS_DECL_NSIMEDIAMANAGERSERVICE

  media::Parent<media::NonE10s>* GetNonE10sParent();
  MediaEngine* GetBackend(uint64_t aWindowId = 0);
  StreamListeners *GetWindowListeners(uint64_t aWindowId) {
    MOZ_ASSERT(NS_IsMainThread());
    return mActiveWindows.Get(aWindowId);
  }
  void RemoveWindowID(uint64_t aWindowId);
  bool IsWindowStillActive(uint64_t aWindowId) {
    return !!GetWindowListeners(aWindowId);
  }
  // Note: also calls aListener->Remove(), even if inactive
  void RemoveFromWindowList(uint64_t aWindowID,
    GetUserMediaCallbackMediaStreamListener *aListener);

  nsresult GetUserMedia(
    nsPIDOMWindowInner* aWindow,
    const dom::MediaStreamConstraints& aConstraints,
    nsIDOMGetUserMediaSuccessCallback* onSuccess,
    nsIDOMGetUserMediaErrorCallback* onError);

  nsresult GetUserMediaDevices(nsPIDOMWindowInner* aWindow,
                               const dom::MediaStreamConstraints& aConstraints,
                               nsIGetUserMediaDevicesSuccessCallback* onSuccess,
                               nsIDOMGetUserMediaErrorCallback* onError,
                               uint64_t aInnerWindowID = 0,
                               const nsAString& aCallID = nsString());

  nsresult EnumerateDevices(nsPIDOMWindowInner* aWindow,
                            nsIGetUserMediaDevicesSuccessCallback* aOnSuccess,
                            nsIDOMGetUserMediaErrorCallback* aOnFailure);

  nsresult EnumerateDevices(nsPIDOMWindowInner* aWindow, dom::Promise& aPromise);
  void OnNavigation(uint64_t aWindowID);
  bool IsActivelyCapturingOrHasAPermission(uint64_t aWindowId);

  MediaEnginePrefs mPrefs;

  typedef nsTArray<RefPtr<MediaDevice>> SourceSet;
  static bool IsPrivateBrowsing(nsPIDOMWindowInner* window);

  virtual int AddDeviceChangeCallback(DeviceChangeCallback* aCallback) override;
  virtual void OnDeviceChange() override;
private:
  typedef media::Pledge<SourceSet*, dom::MediaStreamError*> PledgeSourceSet;
  typedef media::Pledge<const char*, dom::MediaStreamError*> PledgeChar;
  typedef media::Pledge<bool, dom::MediaStreamError*> PledgeVoid;

  static nsresult GenerateUUID(nsAString& aResult);
  static nsresult AnonymizeId(nsAString& aId, const nsACString& aOriginKey);
public: // TODO: make private once we upgrade to GCC 4.8+ on linux.
  static void AnonymizeDevices(SourceSet& aDevices, const nsACString& aOriginKey);
  static already_AddRefed<nsIWritableVariant> ToJSArray(SourceSet& aDevices);
private:
  already_AddRefed<PledgeSourceSet>
  EnumerateRawDevices(uint64_t aWindowId,
                      dom::MediaSourceEnum aVideoType,
                      dom::MediaSourceEnum aAudioType,
                      bool aFake);
  already_AddRefed<PledgeSourceSet>
  EnumerateDevicesImpl(uint64_t aWindowId,
                       dom::MediaSourceEnum aVideoSrcType,
                       dom::MediaSourceEnum aAudioSrcType,
                       bool aFake = false);
  already_AddRefed<PledgeChar>
  SelectSettings(
      dom::MediaStreamConstraints& aConstraints,
      bool aIsChrome,
      RefPtr<media::Refcountable<UniquePtr<SourceSet>>>& aSources);

  StreamListeners* AddWindowID(uint64_t aWindowId);
  WindowTable *GetActiveWindows() {
    MOZ_ASSERT(NS_IsMainThread());
    return &mActiveWindows;
  }

  void GetPref(nsIPrefBranch *aBranch, const char *aPref,
               const char *aData, int32_t *aVal);
  void GetPrefBool(nsIPrefBranch *aBranch, const char *aPref,
                   const char *aData, bool *aVal);
  void GetPrefs(nsIPrefBranch *aBranch, const char *aData);

  // Make private because we want only one instance of this class
  MediaManager();

  ~MediaManager() {}
  void Shutdown();

  void StopScreensharing(uint64_t aWindowID);
  void IterateWindowListeners(nsPIDOMWindowInner *aWindow,
                              WindowListenerCallback aCallback,
                              void *aData);

  void StopMediaStreams();
  void RemoveMediaDevicesCallback(uint64_t aWindowID);

  // ONLY access from MainThread so we don't need to lock
  WindowTable mActiveWindows;
  nsRefPtrHashtable<nsStringHashKey, GetUserMediaTask> mActiveCallbacks;
  nsClassHashtable<nsUint64HashKey, nsTArray<nsString>> mCallIds;

  // Always exists
  nsAutoPtr<base::Thread> mMediaThread;
  nsCOMPtr<nsIAsyncShutdownBlocker> mShutdownBlocker;

  // ONLY accessed from MediaManagerThread
  RefPtr<MediaEngine> mBackend;

  static StaticRefPtr<MediaManager> sSingleton;

  media::CoatCheck<PledgeSourceSet> mOutstandingPledges;
  media::CoatCheck<PledgeChar> mOutstandingCharPledges;
  media::CoatCheck<PledgeVoid> mOutstandingVoidPledges;
public:
  media::CoatCheck<media::Pledge<nsCString>> mGetOriginKeyPledges;
  RefPtr<media::Parent<media::NonE10s>> mNonE10sParent;
};

} // namespace mozilla

#endif // MOZILLA_MEDIAMANAGER_H
