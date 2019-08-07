/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef VideoFrameConverter_h
#define VideoFrameConverter_h

#include "ImageContainer.h"
#include "ImageToI420.h"
#include "MediaTimer.h"
#include "VideoSegment.h"
#include "VideoUtils.h"
#include "nsISupportsImpl.h"
#include "nsThreadUtils.h"
#include "mozilla/TaskQueue.h"
#include "mozilla/dom/ImageBitmapBinding.h"
#include "mozilla/dom/ImageUtils.h"
#include "webrtc/api/video/video_frame.h"
#include "webrtc/common_video/include/i420_buffer_pool.h"
#include "webrtc/common_video/include/video_frame_buffer.h"
#include "webrtc/rtc_base/keep_ref_until_done.h"
#include "webrtc/system_wrappers/include/clock.h"

// The number of frame buffers VideoFrameConverter may create before returning
// errors.
// Sometimes these are released synchronously but they can be forwarded all the
// way to the encoder for asynchronous encoding. With a pool size of 5,
// we allow 1 buffer for the current conversion, and 4 buffers to be queued at
// the encoder.
#define CONVERTER_BUFFER_POOL_SIZE 5

namespace mozilla {

static mozilla::LazyLogModule gVideoFrameConverterLog("VideoFrameConverter");

class VideoConverterListener {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(VideoConverterListener)

  virtual void OnVideoFrameConverted(const webrtc::VideoFrame& aVideoFrame) = 0;

 protected:
  virtual ~VideoConverterListener() = default;
};

// An async video frame format converter.
//
// Input is typically a MediaStreamTrackListener driven by MediaStreamGraph.
//
// Output is passed through to all added VideoConverterListeners on a TaskQueue
// thread whenever a frame is converted.
class VideoFrameConverter {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(VideoFrameConverter)

  VideoFrameConverter()
      : mTaskQueue(
            new TaskQueue(GetMediaThreadPool(MediaThreadType::WEBRTC_DECODER),
                          "VideoFrameConverter")),
        mPacingTimer(new MediaTimer()),
        mBufferPool(false, CONVERTER_BUFFER_POOL_SIZE),
        mActive(false),
        mTrackEnabled(true) {
    MOZ_COUNT_CTOR(VideoFrameConverter);
  }

  void QueueVideoChunk(const VideoChunk& aChunk, bool aForceBlack) {
    gfx::IntSize size = aChunk.mFrame.GetIntrinsicSize();
    if (size.width == 0 || size.height == 0) {
      return;
    }

    TimeStamp t = aChunk.mTimeStamp;
    MOZ_ASSERT(!t.IsNull());

    if (!mLastFrameQueuedForPacing.IsNull() && t < mLastFrameQueuedForPacing) {
      // With a direct listener we can have buffered up future frames in
      // mPacingTimer. The source could start sending us frames that start
      // before some previously buffered frames (e.g., a MediaDecoder does that
      // when it seeks). We don't want to just append these to the pacing timer,
      // as frames at different times on the MediaDecoder timeline would get
      // passed to the encoder in a mixed order. We don't have an explicit way
      // of signaling this, so we must detect here if time goes backwards.
      MOZ_LOG(gVideoFrameConverterLog, LogLevel::Debug,
              ("Clearing pacer because of source reset (%.3f)",
               (mLastFrameQueuedForPacing - t).ToSeconds()));
      mPacingTimer->Cancel();
    }

    mLastFrameQueuedForPacing = t;

    mPacingTimer->WaitUntil(t, __func__)
        ->Then(
            mTaskQueue, __func__,
            [self = RefPtr<VideoFrameConverter>(this), this,
             image = RefPtr<layers::Image>(aChunk.mFrame.GetImage()), t, size,
             aForceBlack]() mutable {
              QueueForProcessing(std::move(image), t, size, aForceBlack);
            },
            [] {});
  }

  /**
   * An active VideoFrameConverter actively converts queued video frames.
   * While inactive, we keep track of the frame most recently queued for
   * processing, so it can be immediately sent out once activated.
   */
  void SetActive(bool aActive) {
    nsresult rv = mTaskQueue->Dispatch(NS_NewRunnableFunction(
        __func__, [self = RefPtr<VideoFrameConverter>(this), this, aActive] {
          if (mActive == aActive) {
            return;
          }
          MOZ_LOG(gVideoFrameConverterLog, LogLevel::Debug,
                  ("VideoFrameConverter is now %s",
                   aActive ? "active" : "inactive"));
          mActive = aActive;
          if (aActive && mLastFrameQueuedForProcessing.Serial() != -2) {
            // After activating, we re-process the last image that was queued
            // for processing so it can be immediately sent.
            FrameToProcess f = mLastFrameQueuedForProcessing;
            f.mTime = TimeStamp::Now();
            ProcessVideoFrame(std::move(f));
          }
        }));
    MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));
    Unused << rv;
  }

  void SetTrackEnabled(bool aTrackEnabled) {
    nsresult rv = mTaskQueue->Dispatch(NS_NewRunnableFunction(
        __func__,
        [self = RefPtr<VideoFrameConverter>(this), this, aTrackEnabled] {
          if (mTrackEnabled == aTrackEnabled) {
            return;
          }
          MOZ_LOG(gVideoFrameConverterLog, LogLevel::Debug,
                  ("VideoFrameConverter Track is now %s",
                   aTrackEnabled ? "enabled" : "disabled"));
          mTrackEnabled = aTrackEnabled;
          if (!aTrackEnabled && mLastFrameConverted) {
            // After disabling, we re-send the last frame as black in case the
            // source had already stopped and no frame is coming soon.
            ProcessVideoFrame(
                FrameToProcess{nullptr, TimeStamp::Now(),
                               gfx::IntSize(mLastFrameConverted->width(),
                                            mLastFrameConverted->height()),
                               true});
          }
        }));
    MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));
    Unused << rv;
  }

  void AddListener(const RefPtr<VideoConverterListener>& aListener) {
    nsresult rv = mTaskQueue->Dispatch(NS_NewRunnableFunction(
        "VideoFrameConverter::AddListener",
        [self = RefPtr<VideoFrameConverter>(this), this, aListener] {
          MOZ_ASSERT(!mListeners.Contains(aListener));
          mListeners.AppendElement(aListener);
        }));
    MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));
    Unused << rv;
  }

  void RemoveListener(const RefPtr<VideoConverterListener>& aListener) {
    nsresult rv = mTaskQueue->Dispatch(NS_NewRunnableFunction(
        "VideoFrameConverter::RemoveListener",
        [self = RefPtr<VideoFrameConverter>(this), this, aListener] {
          mListeners.RemoveElement(aListener);
        }));
    MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));
    Unused << rv;
  }

  void Shutdown() {
    mPacingTimer->Cancel();

    nsresult rv = mTaskQueue->Dispatch(NS_NewRunnableFunction(
        "VideoFrameConverter::Shutdown",
        [self = RefPtr<VideoFrameConverter>(this), this] {
          if (mSameFrameTimer) {
            mSameFrameTimer->Cancel();
          }
          mListeners.Clear();
          mBufferPool.Release();
          mLastFrameQueuedForProcessing = FrameToProcess();
          mLastFrameConverted = nullptr;
        }));
    MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));
    Unused << rv;
  }

 protected:
  struct FrameToProcess {
    RefPtr<layers::Image> mImage;
    TimeStamp mTime = TimeStamp::Now();
    gfx::IntSize mSize;
    bool mForceBlack = false;

    int32_t Serial() {
      if (mForceBlack) {
        // Set the last-img check to indicate black.
        // -1 is not a guaranteed invalid serial. See bug 1262134.
        return -1;
      }
      if (!mImage) {
        // Set the last-img check to indicate reset.
        // -2 is not a guaranteed invalid serial. See bug 1262134.
        return -2;
      }
      return mImage->GetSerial();
    }
  };

  virtual ~VideoFrameConverter() { MOZ_COUNT_DTOR(VideoFrameConverter); }

  static void SameFrameTick(nsITimer* aTimer, void* aClosure) {
    MOZ_ASSERT(aClosure);
    VideoFrameConverter* self = static_cast<VideoFrameConverter*>(aClosure);
    MOZ_ASSERT(self->mTaskQueue->IsCurrentThreadIn());

    if (!self->mLastFrameConverted) {
      return;
    }

    self->mLastFrameConverted->set_timestamp_us(
        webrtc::Clock::GetRealTimeClock()->TimeInMicroseconds());
    for (RefPtr<VideoConverterListener>& listener : self->mListeners) {
      listener->OnVideoFrameConverted(*self->mLastFrameConverted);
    }
  }

  void VideoFrameConverted(const webrtc::VideoFrame& aVideoFrame) {
    MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn());

    if (mSameFrameTimer) {
      mSameFrameTimer->Cancel();
    }

    const int sameFrameIntervalInMs = 1000;
    NS_NewTimerWithFuncCallback(
        getter_AddRefs(mSameFrameTimer), &SameFrameTick, this,
        sameFrameIntervalInMs, nsITimer::TYPE_REPEATING_SLACK,
        "VideoFrameConverter::mSameFrameTimer", mTaskQueue);

    mLastFrameConverted = MakeUnique<webrtc::VideoFrame>(aVideoFrame);

    for (RefPtr<VideoConverterListener>& listener : mListeners) {
      listener->OnVideoFrameConverted(aVideoFrame);
    }
  }

  void QueueForProcessing(RefPtr<layers::Image> aImage, TimeStamp aTime,
                          gfx::IntSize aSize, bool aForceBlack) {
    MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn());

    FrameToProcess frame{std::move(aImage), aTime, aSize,
                         aForceBlack || !mTrackEnabled};

    if (frame.Serial() == mLastFrameQueuedForProcessing.Serial()) {
      // With a non-direct listener we get passed duplicate frames every ~10ms
      // even with no frame change.
      return;
    }

    if (frame.mTime <= mLastFrameQueuedForProcessing.mTime) {
      MOZ_LOG(
          gVideoFrameConverterLog, LogLevel::Debug,
          ("Dropping a frame because time did not progress (%.3f)",
           (mLastFrameQueuedForProcessing.mTime - frame.mTime).ToSeconds()));
      return;
    }

    mLastFrameQueuedForProcessing = std::move(frame);

    if (!mActive) {
      MOZ_LOG(gVideoFrameConverterLog, LogLevel::Debug,
              ("Ignoring a frame because we're inactive"));
      return;
    }

    nsresult rv = mTaskQueue->Dispatch(
        NewRunnableMethod<StoreCopyPassByLRef<FrameToProcess>>(
            "VideoFrameConverter::ProcessVideoFrame", this,
            &VideoFrameConverter::ProcessVideoFrame,
            mLastFrameQueuedForProcessing));
    MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));
    Unused << rv;
  }

  void ProcessVideoFrame(const FrameToProcess& aFrame) {
    MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn());

    if (aFrame.mTime < mLastFrameQueuedForProcessing.mTime) {
      MOZ_LOG(
          gVideoFrameConverterLog, LogLevel::Debug,
          ("Dropping a frame that is %.3f seconds behind latest",
           (mLastFrameQueuedForProcessing.mTime - aFrame.mTime).ToSeconds()));
      return;
    }

    // See Bug 1529581 - Ideally we'd use the mTimestamp from the chunk
    // passed into QueueVideoChunk rather than the webrtc.org clock here.
    int64_t now = webrtc::Clock::GetRealTimeClock()->TimeInMilliseconds();

    if (aFrame.mForceBlack) {
      // Send a black image.
      rtc::scoped_refptr<webrtc::I420Buffer> buffer =
          mBufferPool.CreateBuffer(aFrame.mSize.width, aFrame.mSize.height);
      if (!buffer) {
        MOZ_DIAGNOSTIC_ASSERT(false,
                              "Buffers not leaving scope except for "
                              "reconfig, should never leak");
        MOZ_LOG(gVideoFrameConverterLog, LogLevel::Warning,
                ("Creating a buffer for a black video frame failed"));
        return;
      }

      MOZ_LOG(gVideoFrameConverterLog, LogLevel::Verbose,
              ("Sending a black video frame"));
      webrtc::I420Buffer::SetBlack(buffer);

      webrtc::VideoFrame frame(buffer, 0,  // not setting rtp timestamp
                               now, webrtc::kVideoRotation_0);
      VideoFrameConverted(frame);
      return;
    }

    if (!aFrame.mImage) {
      // Don't send anything for null images.
      return;
    }

    MOZ_ASSERT(aFrame.mImage->GetSize() == aFrame.mSize);

    if (layers::PlanarYCbCrImage* image = aFrame.mImage->AsPlanarYCbCrImage()) {
      dom::ImageUtils utils(image);
      if (utils.GetFormat() == dom::ImageBitmapFormat::YUV420P &&
          image->GetData()) {
        const layers::PlanarYCbCrData* data = image->GetData();
        rtc::scoped_refptr<webrtc::WrappedI420Buffer> video_frame_buffer(
            new rtc::RefCountedObject<webrtc::WrappedI420Buffer>(
                aFrame.mImage->GetSize().width, aFrame.mImage->GetSize().height,
                data->mYChannel, data->mYStride, data->mCbChannel,
                data->mCbCrStride, data->mCrChannel, data->mCbCrStride,
                rtc::KeepRefUntilDone(image)));

        webrtc::VideoFrame i420_frame(video_frame_buffer,
                                      0,  // not setting rtp timestamp
                                      now, webrtc::kVideoRotation_0);
        MOZ_LOG(gVideoFrameConverterLog, LogLevel::Verbose,
                ("Sending an I420 video frame"));
        VideoFrameConverted(i420_frame);
        return;
      }
    }

    rtc::scoped_refptr<webrtc::I420Buffer> buffer =
        mBufferPool.CreateBuffer(aFrame.mSize.width, aFrame.mSize.height);
    if (!buffer) {
      MOZ_DIAGNOSTIC_ASSERT(++mFramesDropped <= 100, "Buffers must be leaking");
      MOZ_LOG(gVideoFrameConverterLog, LogLevel::Warning,
              ("Creating a buffer failed"));
      return;
    }

#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
    mFramesDropped = 0;
#endif

    nsresult rv =
        ConvertToI420(aFrame.mImage, buffer->MutableDataY(), buffer->StrideY(),
                      buffer->MutableDataU(), buffer->StrideU(),
                      buffer->MutableDataV(), buffer->StrideV());

    if (NS_FAILED(rv)) {
      MOZ_LOG(gVideoFrameConverterLog, LogLevel::Warning,
              ("Image conversion failed"));
      return;
    }

    webrtc::VideoFrame frame(buffer, 0,  // not setting rtp timestamp
                             now, webrtc::kVideoRotation_0);
    VideoFrameConverted(frame);
  }

  const RefPtr<TaskQueue> mTaskQueue;

  // Used to pace future frames close to their rendering-time. Thread-safe.
  const RefPtr<MediaTimer> mPacingTimer;

  // Written and read from the queueing thread (normally MSG).
  // Last time we queued a frame in the pacer
  TimeStamp mLastFrameQueuedForPacing;

  // Accessed only from mTaskQueue.
  webrtc::I420BufferPool mBufferPool;
  nsCOMPtr<nsITimer> mSameFrameTimer;
  FrameToProcess mLastFrameQueuedForProcessing;
  UniquePtr<webrtc::VideoFrame> mLastFrameConverted;
  bool mActive;
  bool mTrackEnabled;
#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
  size_t mFramesDropped = 0;
#endif
  nsTArray<RefPtr<VideoConverterListener>> mListeners;
};

}  // namespace mozilla

#endif  // VideoFrameConverter_h
