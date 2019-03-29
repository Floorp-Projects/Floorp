/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MediaEngineSource_h
#define MediaEngineSource_h

#include "MediaSegment.h"
#include "MediaTrackConstraints.h"
#include "mozilla/dom/MediaStreamTrackBinding.h"
#include "mozilla/media/MediaUtils.h"
#include "mozilla/RefPtr.h"
#include "mozilla/ThreadSafeWeakPtr.h"
#include "nsStringFwd.h"
#include "TrackID.h"

namespace mozilla {

namespace dom {
class Blob;
struct MediaTrackSettings;
}  // namespace dom

namespace ipc {
class PrincipalInfo;
}  // namespace ipc

class MediaEnginePhotoCallback;
class MediaEnginePrefs;
class SourceMediaStream;

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
  virtual ~MediaEnginePhotoCallback() = default;
};

/**
 * Lifecycle state of MediaEngineSource.
 */
enum MediaEngineSourceState {
  kAllocated,  // Allocated, not yet started.
  kStarted,    // Previously allocated or stopped, then started.
  kStopped,    // Previously started, then stopped.
  kReleased    // Not allocated.
};

/**
 * The pure interface of a MediaEngineSource.
 *
 * Most sources are helped by the defaults implemented in MediaEngineSource.
 */
class MediaEngineSourceInterface {
 public:
  /**
   * Return true if this is a fake source. I.e., if it is generating media
   * itself rather than being an interface to underlying hardware.
   */
  virtual bool IsFake() const = 0;

  /**
   * Gets the human readable name of this device.
   */
  virtual nsString GetName() const = 0;

  /**
   * Gets the UUID of this device.
   */
  virtual nsCString GetUUID() const = 0;

  /**
   * Gets the Group id of this device.
   */
  virtual nsString GetGroupId() const = 0;

  /**
   * Get the enum describing the underlying type of MediaSource.
   */
  virtual dom::MediaSourceEnum GetMediaSource() const = 0;

  /**
   * Override w/true if source does end-run around cross origin restrictions.
   */
  virtual bool GetScary() const = 0;

  /**
   * Override w/a promise if source has frames, in order to potentially allow
   * deferring success of source acquisition until first frame has arrived.
   */
  virtual RefPtr<GenericNonExclusivePromise> GetFirstFramePromise() const {
    return nullptr;
  }

  /**
   * Called by MediaEngine to allocate an instance of this source.
   */
  virtual nsresult Allocate(const dom::MediaTrackConstraints& aConstraints,
                            const MediaEnginePrefs& aPrefs,
                            const nsString& aDeviceId,
                            const mozilla::ipc::PrincipalInfo& aPrincipalInfo,
                            const char** aOutBadConstraint) = 0;

  /**
   * Called by MediaEngine when a SourceMediaStream and TrackID have been
   * provided for the source to feed data to.
   *
   * This must be called before Start.
   */
  virtual void SetTrack(const RefPtr<SourceMediaStream>& aStream,
                        TrackID aTrackID,
                        const PrincipalHandle& aPrincipal) = 0;

  /**
   * Called by MediaEngine to start feeding data to the track.
   *
   * NB: Audio sources handle the enabling of pulling themselves.
   */
  virtual nsresult Start() = 0;

  /**
   * This brings focus to the selected source, e.g. to bring a captured window
   * to the front.
   *
   * We return one of the following:
   * NS_OK                    - Success.
   * NS_ERROR_NOT_AVAILABLE   - For backends where focusing does not make sense.
   * NS_ERROR_NOT_IMPLEMENTED - For backends where focusing makes sense, but
   *                            is not yet implemented.
   * NS_ERROR_FAILURE         - Failures reported from underlying code.
   */
  virtual nsresult FocusOnSelectedSource() = 0;

  /**
   * Applies new constraints to the capability selection for the underlying
   * device.
   *
   * Should the constraints lead to choosing a new capability while the device
   * is actively being captured, the device will restart using the new
   * capability.
   *
   * We return one of the following:
   * NS_OK                - Successful reconfigure.
   * NS_ERROR_INVALID_ARG - Couldn't find a capability fitting aConstraints.
   *                        See aBadConstraint for details.
   * NS_ERROR_UNEXPECTED  - Reconfiguring the underlying device failed
   *                        unexpectedly. This leaves the device in a stopped
   *                        state.
   */
  virtual nsresult Reconfigure(const dom::MediaTrackConstraints& aConstraints,
                               const MediaEnginePrefs& aPrefs,
                               const nsString& aDeviceId,
                               const char** aOutBadConstraint) = 0;

  /**
   * Called by MediaEngine to stop feeding data to the track.
   *
   * Double-stopping is allowed and will return NS_OK. This is necessary
   * sometimes during shutdown.
   *
   * NB: Audio sources handle the disabling of pulling themselves.
   */
  virtual nsresult Stop() = 0;

  /**
   * Called by MediaEngine to deallocate an underlying device.
   */
  virtual nsresult Deallocate() = 0;

  /**
   * Called by MediaEngine when it knows this MediaEngineSource won't be used
   * anymore. Use it to clean up anything that needs to be cleaned up.
   */
  virtual void Shutdown() = 0;

  /**
   * If implementation of MediaEngineSource supports TakePhoto(), the picture
   * should be returned via aCallback object. Otherwise, it returns
   * NS_ERROR_NOT_IMPLEMENTED.
   */
  virtual nsresult TakePhoto(MediaEnginePhotoCallback* aCallback) = 0;

  /**
   * GetBestFitnessDistance returns the best distance the capture device can
   * offer as a whole, given an accumulated number of ConstraintSets. Ideal
   * values are considered in the first ConstraintSet only. Plain values are
   * treated as Ideal in the first ConstraintSet. Plain values are treated as
   * Exact in subsequent ConstraintSets. Infinity = UINT32_MAX e.g. device
   * cannot satisfy accumulated ConstraintSets. A finite result may be used to
   * calculate this device's ranking as a choice.
   */
  virtual uint32_t GetBestFitnessDistance(
      const nsTArray<const NormalizedConstraintSet*>& aConstraintSets,
      const nsString& aDeviceId) const = 0;

  /**
   * Returns the current settings of the underlying device.
   *
   * Note that this might not be the settings of the underlying hardware.
   * In case of a camera where we intervene and scale frames to avoid
   * leaking information from other documents than the current one,
   * GetSettings() will return the scaled resolution. I.e., the
   * device settings as seen by js.
   */
  virtual void GetSettings(dom::MediaTrackSettings& aOutSettings) const = 0;
};

/**
 * Abstract base class for MediaEngineSources.
 *
 * Implements defaults for some common MediaEngineSourceInterface methods below.
 * Also implements RefPtr support and an owning-thread model for thread safety
 * checks in subclasses.
 */
class MediaEngineSource : public MediaEngineSourceInterface {
 public:
  // code inside webrtc.org assumes these sizes; don't use anything smaller
  // without verifying it's ok
  static const unsigned int kMaxDeviceNameLength = 128;
  static const unsigned int kMaxUniqueIdLength = 256;

  /**
   * Returns true if the given source type is for video, false otherwise.
   * Only call with real types.
   */
  static bool IsVideo(dom::MediaSourceEnum aSource);

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MediaEngineSource)
  NS_DECL_OWNINGTHREAD

  void AssertIsOnOwningThread() const {
    NS_ASSERT_OWNINGTHREAD(MediaEngineSource);
  }

  // Not fake by default.
  bool IsFake() const override;

  // Not scary by default.
  bool GetScary() const override;

  // Returns NS_ERROR_NOT_AVAILABLE by default.
  nsresult FocusOnSelectedSource() override;

  // Shutdown does nothing by default.
  void Shutdown() override;

  // TakePhoto returns NS_ERROR_NOT_IMPLEMENTED by default,
  // to tell the caller to fallback to other methods.
  nsresult TakePhoto(MediaEnginePhotoCallback* aCallback) override;

  // Makes aOutSettings empty by default.
  void GetSettings(dom::MediaTrackSettings& aOutSettings) const override;

 protected:
  virtual ~MediaEngineSource();
};

}  // namespace mozilla

#endif /* MediaEngineSource_h */
