/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebRenderCompositionRecorder.h"

#include "mozilla/webrender/RenderThread.h"

namespace mozilla {

namespace layers {

class RendererRecordedFrame final : public layers::RecordedFrame {
 public:
  RendererRecordedFrame(const TimeStamp& aTimeStamp, wr::Renderer* aRenderer,
                        const wr::RecordedFrameHandle aHandle,
                        const gfx::IntSize& aSize)
      : RecordedFrame(aTimeStamp),
        mRenderer(aRenderer),
        mSize(aSize),
        mHandle(aHandle) {}

  already_AddRefed<gfx::DataSourceSurface> GetSourceSurface() override {
    if (!mSurface) {
      mSurface = gfx::Factory::CreateDataSourceSurface(
          mSize, gfx::SurfaceFormat::B8G8R8A8, /* aZero = */ false);

      gfx::DataSourceSurface::ScopedMap map(mSurface,
                                            gfx::DataSourceSurface::WRITE);

      if (!wr_renderer_map_recorded_frame(mRenderer, mHandle, map.GetData(),
                                          mSize.width * mSize.height * 4,
                                          mSize.width * 4)) {
        return nullptr;
      }
    }

    return do_AddRef(mSurface);
  }

 private:
  wr::Renderer* mRenderer;
  RefPtr<gfx::DataSourceSurface> mSurface;
  gfx::IntSize mSize;
  wr::RecordedFrameHandle mHandle;
};

void WebRenderCompositionRecorder::RecordFrame(RecordedFrame* aFrame) {
  MOZ_CRASH(
      "WebRenderCompositionRecorder::RecordFrame should not be called; call "
      "MaybeRecordFrame instead.");
}

bool WebRenderCompositionRecorder::MaybeRecordFrame(
    wr::Renderer* aRenderer, wr::WebRenderPipelineInfo* aFrameEpochs) {
  MOZ_ASSERT(wr::RenderThread::IsInRenderThread());

  if (!aRenderer || !aFrameEpochs) {
    return false;
  }

  if (!mMutex.TryLock()) {
    // If we cannot lock the mutex, then either (a) the |CompositorBridgeParent|
    // is holding the mutex in |WriteCollectedFrames| or (b) the |RenderThread|
    // is holding the mutex in |ForceFinishRecording|.
    //
    // In either case we do not want to wait to acquire the mutex to record a
    // frame since frames recorded now will not be written to disk.

    return false;
  }

  auto unlockGuard = MakeScopeExit([&]() { mMutex.Unlock(); });

  if (mFinishedRecording) {
    return true;
  }

  if (!DidPaintContent(aFrameEpochs)) {
    return false;
  }

  wr::RecordedFrameHandle handle{0};
  gfx::IntSize size(0, 0);

  if (wr_renderer_record_frame(aRenderer, wr::ImageFormat::BGRA8, &handle,
                               &size.width, &size.height)) {
    RefPtr<RecordedFrame> frame =
        new RendererRecordedFrame(TimeStamp::Now(), aRenderer, handle, size);

    CompositionRecorder::RecordFrame(frame);
  }

  return false;
}

void WebRenderCompositionRecorder::WriteCollectedFrames() {
  MutexAutoLock guard(mMutex);

  MOZ_RELEASE_ASSERT(
      !mFinishedRecording,
      "WebRenderCompositionRecorder: Attempting to write frames from invalid "
      "state.");

  CompositionRecorder::WriteCollectedFrames();

  mFinishedRecording = true;
}

bool WebRenderCompositionRecorder::ForceFinishRecording() {
  MutexAutoLock guard(mMutex);

  bool wasRecording = !mFinishedRecording;
  mFinishedRecording = true;

  ClearCollectedFrames();

  return wasRecording;
}

bool WebRenderCompositionRecorder::DidPaintContent(
    wr::WebRenderPipelineInfo* aFrameEpochs) {
  const wr::WrPipelineInfo& info = aFrameEpochs->Raw();
  bool didPaintContent = false;

  for (wr::usize i = 0; i < info.epochs.length; i++) {
    const wr::PipelineId pipelineId = info.epochs.data[i].pipeline_id;

    if (pipelineId == mRootPipelineId) {
      continue;
    }

    const auto it = mContentPipelines.find(AsUint64(pipelineId));
    if (it == mContentPipelines.end() ||
        it->second != info.epochs.data[i].epoch) {
      // This content pipeline has updated list last render or has newly
      // rendered.
      didPaintContent = true;
      mContentPipelines[AsUint64(pipelineId)] = info.epochs.data[i].epoch;
    }
  }

  for (wr::usize i = 0; i < info.removed_pipelines.length; i++) {
    const wr::PipelineId pipelineId =
        info.removed_pipelines.data[i].pipeline_id;
    if (pipelineId == mRootPipelineId) {
      continue;
    }
    mContentPipelines.erase(AsUint64(pipelineId));
  }

  return didPaintContent;
}

}  // namespace layers
}  // namespace mozilla
