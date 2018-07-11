/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MEDIAENGINEDEFAULT_H_
#define MEDIAENGINEDEFAULT_H_

#include "nsINamed.h"
#include "nsITimer.h"

#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "DOMMediaStream.h"
#include "nsComponentManagerUtils.h"
#include "mozilla/Mutex.h"

#include "VideoUtils.h"
#include "MediaEngine.h"
#include "MediaEnginePrefs.h"
#include "VideoSegment.h"
#include "AudioSegment.h"
#include "StreamTracks.h"
#include "MediaEngineSource.h"
#include "MediaStreamGraph.h"
#include "SineWaveGenerator.h"

namespace mozilla {

namespace layers {
class ImageContainer;
} // namespace layers

class MediaEngineDefault;

/**
 * The default implementation of the MediaEngine interface.
 */
class MediaEngineDefaultVideoSource : public MediaEngineSource
{
public:
  MediaEngineDefaultVideoSource();

  nsString GetName() const override;
  nsCString GetUUID() const override;

  nsresult Allocate(const dom::MediaTrackConstraints &aConstraints,
                    const MediaEnginePrefs &aPrefs,
                    const nsString& aDeviceId,
                    const ipc::PrincipalInfo& aPrincipalInfo,
                    AllocationHandle** aOutHandle,
                    const char** aOutBadConstraint) override;
  nsresult SetTrack(const RefPtr<const AllocationHandle>& aHandle,
                    const RefPtr<SourceMediaStream>& aStream,
                    TrackID aTrackID,
                    const PrincipalHandle& aPrincipal) override;
  nsresult Start(const RefPtr<const AllocationHandle>& aHandle) override;
  nsresult Reconfigure(const RefPtr<AllocationHandle>& aHandle,
                       const dom::MediaTrackConstraints& aConstraints,
                       const MediaEnginePrefs& aPrefs,
                       const nsString& aDeviceId,
                       const char** aOutBadConstraint) override;
  nsresult Stop(const RefPtr<const AllocationHandle>& aHandle) override;
  nsresult Deallocate(const RefPtr<const AllocationHandle>& aHandle) override;
  void Pull(const RefPtr<const AllocationHandle>& aHandle,
            const RefPtr<SourceMediaStream>& aStream,
            TrackID aTrackID,
            StreamTime aDesiredTime,
            const PrincipalHandle& aPrincipalHandle) override;

  uint32_t GetBestFitnessDistance(
      const nsTArray<const NormalizedConstraintSet*>& aConstraintSets,
      const nsString& aDeviceId) const override;

  bool IsFake() const override
  {
    return true;
  }

  dom::MediaSourceEnum GetMediaSource() const override
  {
    return dom::MediaSourceEnum::Camera;
  }

protected:
  ~MediaEngineDefaultVideoSource();

  /**
   * Called by mTimer when it's time to generate a new frame.
   */
  void GenerateFrame();

  nsCOMPtr<nsITimer> mTimer;

  RefPtr<layers::ImageContainer> mImageContainer;

  // mMutex protects mState, mImage, mStream, mTrackID
  Mutex mMutex;

  // Current state of this source.
  // Set under mMutex on the owning thread. Accessed under one of the two.
  MediaEngineSourceState mState = kReleased;
  RefPtr<layers::Image> mImage;
  RefPtr<SourceMediaStream> mStream;
  TrackID mTrackID = TRACK_NONE;

  MediaEnginePrefs mOpts;
  int mCb = 16;
  int mCr = 16;
};

class SineWaveGenerator;

class MediaEngineDefaultAudioSource : public MediaEngineSource
{
public:
  MediaEngineDefaultAudioSource();

  nsString GetName() const override;
  nsCString GetUUID() const override;

  nsresult Allocate(const dom::MediaTrackConstraints &aConstraints,
                    const MediaEnginePrefs &aPrefs,
                    const nsString& aDeviceId,
                    const ipc::PrincipalInfo& aPrincipalInfo,
                    AllocationHandle** aOutHandle,
                    const char** aOutBadConstraint) override;
  nsresult SetTrack(const RefPtr<const AllocationHandle>& aHandle,
                    const RefPtr<SourceMediaStream>& aStream,
                    TrackID aTrackID,
                    const PrincipalHandle& aPrincipal) override;
  nsresult Start(const RefPtr<const AllocationHandle>& aHandle) override;
  nsresult Reconfigure(const RefPtr<AllocationHandle>& aHandle,
                       const dom::MediaTrackConstraints& aConstraints,
                       const MediaEnginePrefs& aPrefs,
                       const nsString& aDeviceId,
                       const char** aOutBadConstraint) override;
  nsresult Stop(const RefPtr<const AllocationHandle>& aHandle) override;
  nsresult Deallocate(const RefPtr<const AllocationHandle>& aHandle) override;
  void inline AppendToSegment(AudioSegment& aSegment,
                              TrackTicks aSamples,
                              const PrincipalHandle& aPrincipalHandle);
  void Pull(const RefPtr<const AllocationHandle>& aHandle,
            const RefPtr<SourceMediaStream>& aStream,
            TrackID aTrackID,
            StreamTime aDesiredTime,
            const PrincipalHandle& aPrincipalHandle) override;

  bool IsFake() const override
  {
    return true;
  }

  dom::MediaSourceEnum GetMediaSource() const override
  {
    return dom::MediaSourceEnum::Microphone;
  }

  uint32_t GetBestFitnessDistance(
      const nsTArray<const NormalizedConstraintSet*>& aConstraintSets,
      const nsString& aDeviceId) const override;

  bool IsAvailable() const;

protected:
  ~MediaEngineDefaultAudioSource();

  // mMutex protects mState, mStream, mTrackID
  Mutex mMutex;

  // Current state of this source.
  // Set under mMutex on the owning thread. Accessed under one of the two.
  MediaEngineSourceState mState = kReleased;
  RefPtr<SourceMediaStream> mStream;
  TrackID mTrackID = TRACK_NONE;

  // Accessed in Pull (from MSG thread)
  TrackTicks mLastNotify = 0;
  uint32_t mFreq = 1000; // ditto

  // Created on Start, then accessed from Pull (MSG thread)
  nsAutoPtr<SineWaveGenerator> mSineGenerator;
};


class MediaEngineDefault : public MediaEngine
{
public:
  MediaEngineDefault() = default;

  void EnumerateDevices(uint64_t aWindowId,
                        dom::MediaSourceEnum,
                        nsTArray<RefPtr<MediaDevice>>*) override;
  void Shutdown() override;
  void ReleaseResourcesForWindow(uint64_t aWindowId) override;

private:
  ~MediaEngineDefault() = default;

  // WindowID -> Array of devices.
  nsClassHashtable<nsUint64HashKey,
                   nsTArray<RefPtr<MediaEngineSource>>> mVSources;
  nsClassHashtable<nsUint64HashKey,
                   nsTArray<RefPtr<MediaEngineDefaultAudioSource>>> mASources;
};

} // namespace mozilla

#endif /* NSMEDIAENGINEDEFAULT_H_ */
