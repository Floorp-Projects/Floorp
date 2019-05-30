/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_WebRenderCompositionRecorder_h
#define mozilla_layers_WebRenderCompositionRecorder_h

#include "CompositionRecorder.h"

#include "mozilla/Mutex.h"
#include "mozilla/ScopeExit.h"

#include <unordered_map>

namespace mozilla {

namespace wr {
class WebRenderPipelineInfo;
}

namespace layers {

/**
 * A thread-safe version of the |CompositionRecorder|.
 *
 * Composition recording for WebRender occurs on the |RenderThread| whereas the
 * frames are written on the thread holding the |CompositorBridgeParent|.
 *
 */
class WebRenderCompositionRecorder final : public CompositionRecorder {
 public:
  explicit WebRenderCompositionRecorder(TimeStamp aRecordingStart,
                                        wr::WrPipelineId aRootPipelineId)
      : CompositionRecorder(aRecordingStart),
        mMutex("CompositionRecorder"),
        mFinishedRecording(false),
        mRootPipelineId(aRootPipelineId) {}

  WebRenderCompositionRecorder() = delete;
  WebRenderCompositionRecorder(WebRenderCompositionRecorder&) = delete;
  WebRenderCompositionRecorder(WebRenderCompositionRecorder&&) = delete;

  WebRenderCompositionRecorder& operator=(WebRenderCompositionRecorder&) =
      delete;
  WebRenderCompositionRecorder& operator=(WebRenderCompositionRecorder&&) =
      delete;

  /**
   * Do not call this method.
   *
   * Instead, call |MaybeRecordFrame|, which will only attempt to record a
   * frame if we have not yet written frames to disk.
   */
  void RecordFrame(RecordedFrame* aFrame) override;

  /**
   * Write the collected frames to disk.
   *
   * This method should not be called if frames have already been written or if
   * |ForceFinishRecording| has been called as the object will be in an invalid
   * state to write to disk.
   *
   * Note: This method will block acquiring a lock.
   */
  void WriteCollectedFrames() override;

  /**
   * Attempt to record a frame from the given renderer.
   *
   * This method will only record a frame if the following are true:
   *
   * - this object's lock was acquired immediately (i.e., we are not currently
   *   writing frames to disk);
   * - we have not yet written frames to disk; and
   * - one of the pipelines in |aFrameEpochs| has updated and it is not the
   *   root pipeline.
   *
   * Returns whether or not the recorder has finished recording frames. If
   * true, it is safe to release both this object and Web Render's composition
   * recorder structures.
   */
  bool MaybeRecordFrame(wr::Renderer* aRenderer,
                        wr::WebRenderPipelineInfo* aFrameEpochs);

  /**
   * Force the composition recorder to finish recording.
   *
   * This should only be called if |WriteCollectedFrames| is not to be called,
   * since the recorder will be in an invalid state to do so.
   *
   * This returns whether or not the recorder was recording before this method
   * was called.
   *
   * Note: This method will block acquiring a lock.
   */
  bool ForceFinishRecording();

 protected:
  ~WebRenderCompositionRecorder() = default;

  /**
   * Determine if any content pipelines updated.
   */
  bool DidPaintContent(wr::WebRenderPipelineInfo* aFrameEpochs);

 private:
  Mutex mMutex;

  // Whether or not we have finished recording.
  bool mFinishedRecording;

  // The id of the root WebRender pipeline.
  //
  // All other pipelines are considered content.
  wr::PipelineId mRootPipelineId;

  // A mapping of wr::PipelineId to the epochs when last they updated.
  //
  // We need to use uint64_t here since wr::PipelineId is not default
  // constructable.
  std::unordered_map<uint64_t, wr::Epoch> mContentPipelines;
};

}  // namespace layers
}  // namespace mozilla

#endif  // mozilla_layers_WebRenderCompositionRecorder_h
