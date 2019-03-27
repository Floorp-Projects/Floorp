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

namespace mozilla {

namespace layers {
class ImageContainer;
}  // namespace layers

class MediaEngineDefault;

/**
 * The default implementation of the MediaEngine interface.
 */
class MediaEngineDefaultVideoSource : public MediaEngineSource {
 public:
  MediaEngineDefaultVideoSource();

  nsString GetName() const override;
  nsCString GetUUID() const override;
  nsString GetGroupId() const override;

  nsresult Allocate(const dom::MediaTrackConstraints& aConstraints,
                    const MediaEnginePrefs& aPrefs, const nsString& aDeviceId,
                    const ipc::PrincipalInfo& aPrincipalInfo,
                    const char** aOutBadConstraint) override;
  void SetTrack(const RefPtr<SourceMediaStream>& aStream, TrackID aTrackID,
                const PrincipalHandle& aPrincipal) override;
  nsresult Start() override;
  nsresult Reconfigure(const dom::MediaTrackConstraints& aConstraints,
                       const MediaEnginePrefs& aPrefs,
                       const nsString& aDeviceId,
                       const char** aOutBadConstraint) override;
  nsresult Stop() override;
  nsresult Deallocate() override;

  uint32_t GetBestFitnessDistance(
      const nsTArray<const NormalizedConstraintSet*>& aConstraintSets,
      const nsString& aDeviceId) const override;

  bool IsFake() const override { return true; }

  dom::MediaSourceEnum GetMediaSource() const override {
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

  // Current state of this source.
  MediaEngineSourceState mState = kReleased;
  RefPtr<layers::Image> mImage;
  RefPtr<SourceMediaStream> mStream;
  TrackID mTrackID = TRACK_NONE;
  PrincipalHandle mPrincipalHandle = PRINCIPAL_HANDLE_NONE;

  MediaEnginePrefs mOpts;
  int mCb = 16;
  int mCr = 16;

 private:
  const nsString mName;
};

class AudioSourcePullListener;

class MediaEngineDefaultAudioSource : public MediaEngineSource {
 public:
  MediaEngineDefaultAudioSource();

  nsString GetName() const override;
  nsCString GetUUID() const override;
  nsString GetGroupId() const override;

  nsresult Allocate(const dom::MediaTrackConstraints& aConstraints,
                    const MediaEnginePrefs& aPrefs, const nsString& aDeviceId,
                    const ipc::PrincipalInfo& aPrincipalInfo,
                    const char** aOutBadConstraint) override;
  void SetTrack(const RefPtr<SourceMediaStream>& aStream, TrackID aTrackID,
                const PrincipalHandle& aPrincipal) override;
  nsresult Start() override;
  nsresult Reconfigure(const dom::MediaTrackConstraints& aConstraints,
                       const MediaEnginePrefs& aPrefs,
                       const nsString& aDeviceId,
                       const char** aOutBadConstraint) override;
  nsresult Stop() override;
  nsresult Deallocate() override;

  bool IsFake() const override { return true; }

  dom::MediaSourceEnum GetMediaSource() const override {
    return dom::MediaSourceEnum::Microphone;
  }

  uint32_t GetBestFitnessDistance(
      const nsTArray<const NormalizedConstraintSet*>& aConstraintSets,
      const nsString& aDeviceId) const override;

 protected:
  ~MediaEngineDefaultAudioSource();

  // Current state of this source.
  MediaEngineSourceState mState = kReleased;
  RefPtr<SourceMediaStream> mStream;
  TrackID mTrackID = TRACK_NONE;
  PrincipalHandle mPrincipalHandle = PRINCIPAL_HANDLE_NONE;
  uint32_t mFrequency = 1000;
  RefPtr<AudioSourcePullListener> mPullListener;
};

class MediaEngineDefault : public MediaEngine {
 public:
  MediaEngineDefault() = default;

  void EnumerateDevices(uint64_t aWindowId, dom::MediaSourceEnum, MediaSinkEnum,
                        nsTArray<RefPtr<MediaDevice>>*) override;
  void Shutdown() override {}

 private:
  ~MediaEngineDefault() = default;
};

}  // namespace mozilla

#endif /* NSMEDIAENGINEDEFAULT_H_ */
