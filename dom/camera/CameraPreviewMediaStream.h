/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_CAMERA_CAMERAPREVIEWMEDIASTREAM_H
#define DOM_CAMERA_CAMERAPREVIEWMEDIASTREAM_H

#include "MediaStreamGraph.h"
#include "mozilla/Mutex.h"

namespace mozilla {

class MediaStreamVideoSink;

class FakeMediaStreamGraph : public MediaStreamGraph
{
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(FakeMediaStreamGraph)
public:
  FakeMediaStreamGraph()
    : MediaStreamGraph(16000)
  {
  }

  virtual void
  DispatchToMainThreadAfterStreamStateUpdate(already_AddRefed<nsIRunnable> aRunnable) override;

protected:
  ~FakeMediaStreamGraph()
  {}
};

/**
 * This is a stream for camera preview.
 *
 * XXX It is a temporary fix of SourceMediaStream.
 * A camera preview requests no delay and no buffering stream,
 * but the SourceMediaStream does not support it.
 */
class CameraPreviewMediaStream : public ProcessedMediaStream
{
  typedef mozilla::layers::Image Image;

public:
  CameraPreviewMediaStream();

  void AddAudioOutput(void* aKey) override;
  void SetAudioOutputVolume(void* aKey, float aVolume) override;
  void RemoveAudioOutput(void* aKey) override;
  void AddVideoOutput(MediaStreamVideoSink* aSink, TrackID aID) override;
  void RemoveVideoOutput(MediaStreamVideoSink* aSink, TrackID aID) override;
  void Suspend() override {}
  void Resume() override {}
  void AddListener(MediaStreamListener* aListener) override;
  void RemoveListener(MediaStreamListener* aListener) override;
  void Destroy() override;
  void OnPreviewStateChange(bool aActive);

  void Invalidate();

  void ProcessInput(GraphTime aFrom, GraphTime aTo, uint32_t aFlags) override;

  // Call these on any thread.
  void SetCurrentFrame(const gfx::IntSize& aIntrinsicSize, Image* aImage);
  void ClearCurrentFrame();
  void RateLimit(bool aLimit);

protected:
  // mMutex protects all the class' fields.
  // This class is not registered to MediaStreamGraph.
  // It needs to protect all the fields.
  Mutex mMutex;
  int32_t mInvalidatePending;
  uint32_t mDiscardedFrames;
  bool mRateLimit;
  bool mTrackCreated;
  RefPtr<FakeMediaStreamGraph> mFakeMediaStreamGraph;
};

} // namespace mozilla

#endif // DOM_CAMERA_CAMERAPREVIEWMEDIASTREAM_H
