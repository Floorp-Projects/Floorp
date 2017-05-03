/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MEDIAENGINE_H_
#define MEDIAENGINE_H_

#include "mozilla/RefPtr.h"
#include "DOMMediaStream.h"
#include "MediaStreamGraph.h"
#include "MediaTrackConstraints.h"
#include "mozilla/dom/MediaStreamTrackBinding.h"
#include "mozilla/dom/VideoStreamTrack.h"
#include "mozilla/ipc/PBackgroundSharedTypes.h"
#include "mozilla/media/DeviceChangeCallback.h"

namespace mozilla {

namespace dom {
class Blob;
} // namespace dom

enum {
  kVideoTrack = 1,
  kAudioTrack = 2,
  kTrackCount
};

/**
 * Abstract interface for managing audio and video devices. Each platform
 * must implement a concrete class that will map these classes and methods
 * to the appropriate backend. For example, on Desktop platforms, these will
 * correspond to equivalent webrtc (GIPS) calls, and on B2G they will map to
 * a Gonk interface.
 */
class MediaEngineVideoSource;
class MediaEngineAudioSource;

enum MediaEngineState {
  kAllocated,
  kStarted,
  kStopped,
  kReleased
};

class MediaEngine : public DeviceChangeCallback
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MediaEngine)

  static const int DEFAULT_VIDEO_FPS = 30;
  static const int DEFAULT_VIDEO_MIN_FPS = 10;
  static const int DEFAULT_43_VIDEO_WIDTH = 640;
  static const int DEFAULT_43_VIDEO_HEIGHT = 480;
  static const int DEFAULT_169_VIDEO_WIDTH = 1280;
  static const int DEFAULT_169_VIDEO_HEIGHT = 720;

#ifndef MOZ_B2G
  static const int DEFAULT_SAMPLE_RATE = 32000;
#else
  static const int DEFAULT_SAMPLE_RATE = 16000;
#endif
  // This allows using whatever rate the graph is using for the
  // MediaStreamTrack. This is useful for microphone data, we know it's already
  // at the correct rate for insertion in the MSG.
  static const int USE_GRAPH_RATE = -1;

  /* Populate an array of video sources in the nsTArray. Also include devices
   * that are currently unavailable. */
  virtual void EnumerateVideoDevices(dom::MediaSourceEnum,
                                     nsTArray<RefPtr<MediaEngineVideoSource> >*) = 0;

  /* Populate an array of audio sources in the nsTArray. Also include devices
   * that are currently unavailable. */
  virtual void EnumerateAudioDevices(dom::MediaSourceEnum,
                                     nsTArray<RefPtr<MediaEngineAudioSource> >*) = 0;

  virtual void Shutdown() = 0;

  virtual void SetFakeDeviceChangeEvents() {}

protected:
  virtual ~MediaEngine() {}
};

/**
 * Video source and friends.
 */
class MediaEnginePrefs {
public:
  MediaEnginePrefs()
    : mWidth(0)
    , mHeight(0)
    , mFPS(0)
    , mMinFPS(0)
    , mFreq(0)
    , mAecOn(false)
    , mAgcOn(false)
    , mNoiseOn(false)
    , mAec(0)
    , mAgc(0)
    , mNoise(0)
    , mPlayoutDelay(0)
    , mFullDuplex(false)
    , mExtendedFilter(false)
    , mDelayAgnostic(false)
    , mFakeDeviceChangeEventOn(false)
  {}

  int32_t mWidth;
  int32_t mHeight;
  int32_t mFPS;
  int32_t mMinFPS;
  int32_t mFreq; // for test tones (fake:true)
  bool mAecOn;
  bool mAgcOn;
  bool mNoiseOn;
  int32_t mAec;
  int32_t mAgc;
  int32_t mNoise;
  int32_t mPlayoutDelay;
  bool mFullDuplex;
  bool mExtendedFilter;
  bool mDelayAgnostic;
  bool mFakeDeviceChangeEventOn;

  // mWidth and/or mHeight may be zero (=adaptive default), so use functions.

  int32_t GetWidth(bool aHD = false) const {
    return mWidth? mWidth : (mHeight?
                             (mHeight * GetDefWidth(aHD)) / GetDefHeight(aHD) :
                             GetDefWidth(aHD));
  }

  int32_t GetHeight(bool aHD = false) const {
    return mHeight? mHeight : (mWidth?
                               (mWidth * GetDefHeight(aHD)) / GetDefWidth(aHD) :
                               GetDefHeight(aHD));
  }
private:
  static int32_t GetDefWidth(bool aHD = false) {
    // It'd be nice if we could use the ternary operator here, but we can't
    // because of bug 1002729.
    if (aHD) {
      return MediaEngine::DEFAULT_169_VIDEO_WIDTH;
    }

    return MediaEngine::DEFAULT_43_VIDEO_WIDTH;
  }

  static int32_t GetDefHeight(bool aHD = false) {
    // It'd be nice if we could use the ternary operator here, but we can't
    // because of bug 1002729.
    if (aHD) {
      return MediaEngine::DEFAULT_169_VIDEO_HEIGHT;
    }

    return MediaEngine::DEFAULT_43_VIDEO_HEIGHT;
  }
};

/**
 * Callback interface for TakePhoto(). Either PhotoComplete() or PhotoError()
 * should be called.
 */
class MediaEnginePhotoCallback {
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MediaEnginePhotoCallback)

  // aBlob is the image captured by MediaEngineSource. It is
  // called on main thread.
  virtual nsresult PhotoComplete(already_AddRefed<dom::Blob> aBlob) = 0;

  // It is called on main thread. aRv is the error code.
  virtual nsresult PhotoError(nsresult aRv) = 0;

protected:
  virtual ~MediaEnginePhotoCallback() {}
};

/**
 * Common abstract base class for audio and video sources.
 *
 * By default, the base class implements Allocate and Deallocate using its
 * UpdateSingleSource pattern, which manages allocation handles and calculates
 * net constraints from competing allocations and updates a single shared device.
 *
 * Classes that don't operate as a single shared device can override Allocate
 * and Deallocate and simply not pass the methods up.
 */
class MediaEngineSource : public nsISupports,
                          protected MediaConstraintsHelper
{
public:
  // code inside webrtc.org assumes these sizes; don't use anything smaller
  // without verifying it's ok
  static const unsigned int kMaxDeviceNameLength = 128;
  static const unsigned int kMaxUniqueIdLength = 256;

  virtual ~MediaEngineSource()
  {
    if (!mInShutdown) {
      Shutdown();
    }
  }

  virtual void Shutdown()
  {
    mInShutdown = true;
  };

  /* Populate the human readable name of this device in the nsAString */
  virtual void GetName(nsAString&) const = 0;

  /* Populate the UUID of this device in the nsACString */
  virtual void GetUUID(nsACString&) const = 0;

  /* Override w/true if source does end-run around cross origin restrictions. */
  virtual bool GetScary() const { return false; };

  class AllocationHandle
  {
  public:
    NS_INLINE_DECL_THREADSAFE_REFCOUNTING(AllocationHandle);
  protected:
    ~AllocationHandle() {}
  public:
    AllocationHandle(const dom::MediaTrackConstraints& aConstraints,
                     const mozilla::ipc::PrincipalInfo& aPrincipalInfo,
                     const MediaEnginePrefs& aPrefs,
                     const nsString& aDeviceId)

    : mConstraints(aConstraints),
      mPrincipalInfo(aPrincipalInfo),
      mPrefs(aPrefs),
      mDeviceId(aDeviceId) {}
  public:
    NormalizedConstraints mConstraints;
    mozilla::ipc::PrincipalInfo mPrincipalInfo;
    MediaEnginePrefs mPrefs;
    nsString mDeviceId;
  };

  /* Release the device back to the system. */
  virtual nsresult Deallocate(AllocationHandle* aHandle)
  {
    MOZ_ASSERT(aHandle);
    RefPtr<AllocationHandle> handle = aHandle;

    class Comparator {
    public:
      static bool Equals(const RefPtr<AllocationHandle>& a,
                         const RefPtr<AllocationHandle>& b) {
        return a.get() == b.get();
      }
    };

    auto ix = mRegisteredHandles.IndexOf(handle, 0, Comparator());
    if (ix == mRegisteredHandles.NoIndex) {
      MOZ_ASSERT(false);
      return NS_ERROR_FAILURE;
    }

    mRegisteredHandles.RemoveElementAt(ix);
    if (mRegisteredHandles.Length() && !mInShutdown) {
      // Whenever constraints are removed, other parties may get closer to ideal.
      auto& first = mRegisteredHandles[0];
      const char* badConstraint = nullptr;
      return ReevaluateAllocation(nullptr, nullptr, first->mPrefs,
                                  first->mDeviceId, &badConstraint);
    }
    return NS_OK;
  }

  /* Start the device and add the track to the provided SourceMediaStream, with
   * the provided TrackID. You may start appending data to the track
   * immediately after. */
  virtual nsresult Start(SourceMediaStream*, TrackID, const PrincipalHandle&) = 0;

  /* tell the source if there are any direct listeners attached */
  virtual void SetDirectListeners(bool) = 0;

  /* Called when the stream wants more data */
  virtual void NotifyPull(MediaStreamGraph* aGraph,
                          SourceMediaStream *aSource,
                          TrackID aId,
                          StreamTime aDesiredTime,
                          const PrincipalHandle& aPrincipalHandle) = 0;

  /* Stop the device and release the corresponding MediaStream */
  virtual nsresult Stop(SourceMediaStream *aSource, TrackID aID) = 0;

  /* Restart with new capability */
  virtual nsresult Restart(AllocationHandle* aHandle,
                           const dom::MediaTrackConstraints& aConstraints,
                           const MediaEnginePrefs &aPrefs,
                           const nsString& aDeviceId,
                           const char** aOutBadConstraint) = 0;

  /* Returns true if a source represents a fake capture device and
   * false otherwise
   */
  virtual bool IsFake() = 0;

  /* Returns the type of media source (camera, microphone, screen, window, etc) */
  virtual dom::MediaSourceEnum GetMediaSource() const = 0;

  /* If implementation of MediaEngineSource supports TakePhoto(), the picture
   * should be return via aCallback object. Otherwise, it returns NS_ERROR_NOT_IMPLEMENTED.
   * Currently, only Gonk MediaEngineSource implementation supports it.
   */
  virtual nsresult TakePhoto(MediaEnginePhotoCallback* aCallback) = 0;

  /* Return false if device is currently allocated or started */
  bool IsAvailable() {
    if (mState == kAllocated || mState == kStarted) {
      return false;
    } else {
      return true;
    }
  }

  /* It is an error to call Start() before an Allocate(), and Stop() before
   * a Start(). Only Allocate() may be called after a Deallocate(). */

  /* This call reserves but does not start the device. */
  virtual nsresult Allocate(const dom::MediaTrackConstraints &aConstraints,
                            const MediaEnginePrefs &aPrefs,
                            const nsString& aDeviceId,
                            const mozilla::ipc::PrincipalInfo& aPrincipalInfo,
                            AllocationHandle** aOutHandle,
                            const char** aOutBadConstraint)
  {
    AssertIsOnOwningThread();
    MOZ_ASSERT(aOutHandle);
    RefPtr<AllocationHandle> handle =
      new AllocationHandle(aConstraints, aPrincipalInfo, aPrefs, aDeviceId);
    nsresult rv = ReevaluateAllocation(handle, nullptr, aPrefs, aDeviceId,
                                       aOutBadConstraint);
    if (NS_FAILED(rv)) {
      return rv;
    }
    mRegisteredHandles.AppendElement(handle);
    handle.forget(aOutHandle);
    return NS_OK;
  }

  virtual uint32_t GetBestFitnessDistance(
      const nsTArray<const NormalizedConstraintSet*>& aConstraintSets,
      const nsString& aDeviceId) const = 0;

  void GetSettings(dom::MediaTrackSettings& aOutSettings)
  {
    MOZ_ASSERT(NS_IsMainThread());
    aOutSettings = mSettings;
  }

protected:
  // Only class' own members can be initialized in constructor initializer list.
  explicit MediaEngineSource(MediaEngineState aState)
    : mState(aState)
    , mInShutdown(false)
  {}

  /* UpdateSingleSource - Centralized abstract function to implement in those
   * cases where a single device is being shared between users. Should apply net
   * constraints and restart the device as needed.
   *
   * aHandle           - New or existing handle, or null to update after removal.
   * aNetConstraints   - Net constraints to be applied to the single device.
   * aPrefs            - As passed in (in case of changes in about:config).
   * aDeviceId         - As passed in (origin dependent).
   * aOutBadConstraint - Result: nonzero if failed to apply. Name of culprit.
   */

  virtual nsresult
  UpdateSingleSource(const AllocationHandle* aHandle,
                     const NormalizedConstraints& aNetConstraints,
                     const MediaEnginePrefs& aPrefs,
                     const nsString& aDeviceId,
                     const char** aOutBadConstraint) {
    return NS_ERROR_NOT_IMPLEMENTED;
  };

  /* ReevaluateAllocation - Call to change constraints for an allocation of
   * a single device. Manages allocation handles, calculates net constraints
   * from all competing allocations, and calls UpdateSingleSource with the net
   * result, to restart the single device as needed.
   *
   * aHandle            - New or existing handle, or null to update after removal.
   * aConstraintsUpdate - Constraints to be applied to existing handle, or null.
   * aPrefs             - As passed in (in case of changes from about:config).
   * aDeviceId          - As passed in (origin-dependent id).
   * aOutBadConstraint  - Result: nonzero if failed to apply. Name of culprit.
   */

  nsresult
  ReevaluateAllocation(AllocationHandle* aHandle,
                       NormalizedConstraints* aConstraintsUpdate,
                       const MediaEnginePrefs& aPrefs,
                       const nsString& aDeviceId,
                       const char** aOutBadConstraint)
  {
    // aHandle and/or aConstraintsUpdate may be nullptr (see below)

    AutoTArray<const NormalizedConstraints*, 10> allConstraints;
    for (auto& registered : mRegisteredHandles) {
      if (aConstraintsUpdate && registered.get() == aHandle) {
        continue; // Don't count old constraints
      }
      allConstraints.AppendElement(&registered->mConstraints);
    }
    if (aConstraintsUpdate) {
      allConstraints.AppendElement(aConstraintsUpdate);
    } else if (aHandle) {
      // In the case of AddShareOfSingleSource, the handle isn't registered yet.
      allConstraints.AppendElement(&aHandle->mConstraints);
    }

    NormalizedConstraints netConstraints(allConstraints);
    if (netConstraints.mBadConstraint) {
      *aOutBadConstraint = netConstraints.mBadConstraint;
      return NS_ERROR_FAILURE;
    }

    nsresult rv = UpdateSingleSource(aHandle, netConstraints, aPrefs, aDeviceId,
                                     aOutBadConstraint);
    if (NS_FAILED(rv)) {
      return rv;
    }
    if (aHandle && aConstraintsUpdate) {
      aHandle->mConstraints = *aConstraintsUpdate;
    }
    return NS_OK;
  }

  void AssertIsOnOwningThread()
  {
    NS_ASSERT_OWNINGTHREAD(MediaEngineSource);
  }

  MediaEngineState mState;

  NS_DECL_OWNINGTHREAD

  nsTArray<RefPtr<AllocationHandle>> mRegisteredHandles;
  bool mInShutdown;

  // Main-thread only:
  dom::MediaTrackSettings mSettings;
};

class MediaEngineVideoSource : public MediaEngineSource
{
public:
  virtual ~MediaEngineVideoSource() {}

protected:
  explicit MediaEngineVideoSource(MediaEngineState aState)
    : MediaEngineSource(aState) {}
  MediaEngineVideoSource()
    : MediaEngineSource(kReleased) {}
};

/**
 * Audio source and friends.
 */
class MediaEngineAudioSource : public MediaEngineSource,
                               public AudioDataListenerInterface
{
public:
  virtual ~MediaEngineAudioSource() {}

protected:
  explicit MediaEngineAudioSource(MediaEngineState aState)
    : MediaEngineSource(aState) {}
  MediaEngineAudioSource()
    : MediaEngineSource(kReleased) {}

};

} // namespace mozilla

#endif /* MEDIAENGINE_H_ */
