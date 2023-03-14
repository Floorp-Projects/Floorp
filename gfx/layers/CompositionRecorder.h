/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_CompositionRecorder_h
#define mozilla_layers_CompositionRecorder_h

#include "mozilla/RefPtr.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/layers/PCompositorBridgeTypes.h"
#include "nsISupportsImpl.h"
#include "nsTArray.h"
#include "nsString.h"

namespace mozilla {

namespace gfx {
class DataSourceSurface;
}

namespace layers {

/**
 * A captured frame from a |LayerManager|.
 */
class RecordedFrame {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(RecordedFrame)

  // The resulting DataSourceSurface must not be kept alive beyond the lifetime
  // of the RecordedFrame object, since it may refer to data owned by the frame.
  virtual already_AddRefed<gfx::DataSourceSurface> GetSourceSurface() = 0;
  TimeStamp GetTimeStamp() { return mTimeStamp; }

 protected:
  virtual ~RecordedFrame() = default;
  RecordedFrame(const TimeStamp& aTimeStamp) : mTimeStamp(aTimeStamp) {}

 private:
  TimeStamp mTimeStamp;
};

/**
 * A recorder for composited frames.
 *
 * This object collects frames sent to it by a |LayerManager| and writes them
 * out as a series of images until recording has finished.
 *
 * If GPU-accelerated rendering is used, the frames will not be mapped into
 * memory until |WriteCollectedFrames| is called.
 */
class CompositionRecorder {
 public:
  explicit CompositionRecorder(TimeStamp aRecordingStart);

  /**
   * Record a composited frame.
   */
  void RecordFrame(RecordedFrame* aFrame);

  /**
   * Get the array of frames that were recorded since the last call to
   * GetRecording(), where each frame consists of a presentation time and
   * the PNG-encoded contents as an array of bytes
   */
  Maybe<FrameRecording> GetRecording();

 private:
  nsTArray<RefPtr<RecordedFrame>> mRecordedFrames;
  TimeStamp mRecordingStart;
};

}  // namespace layers
}  // namespace mozilla

#endif  // mozilla_layers_CompositionRecorder_h
