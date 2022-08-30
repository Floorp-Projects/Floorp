/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef VideoFrameConverter_h
#define VideoFrameConverter_h

#include "ImageContainer.h"
#include "ImageToI420.h"
#include "Pacer.h"
#include "VideoSegment.h"
#include "VideoUtils.h"
#include "nsISupportsImpl.h"
#include "nsThreadUtils.h"
#include "jsapi/RTCStatsReport.h"
#include "mozilla/TaskQueue.h"
#include "mozilla/dom/ImageBitmapBinding.h"
#include "mozilla/dom/ImageUtils.h"
#include "api/video/video_frame.h"
#include "common_video/include/video_frame_buffer_pool.h"
#include "common_video/include/video_frame_buffer.h"

// The number of frame buffers VideoFrameConverter may create before returning
// errors.
// Sometimes these are released synchronously but they can be forwarded all the
// way to the encoder for asynchronous encoding. With a pool size of 5,
// we allow 1 buffer for the current conversion, and 4 buffers to be queued at
// the encoder.
#define CONVERTER_BUFFER_POOL_SIZE 5

namespace mozilla {

static mozilla::LazyLogModule gVideoFrameConverterLog("VideoFrameConverter");

// An async video frame format converter.
//
// Input is typically a MediaTrackListener driven by MediaTrackGraph.
//
// Output is passed through to VideoFrameConvertedEvent() whenever a frame is
// converted.
class VideoFrameConverter {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(VideoFrameConverter)

  explicit VideoFrameConverter(
      const dom::RTCStatsTimestampMaker& aTimestampMaker)
      : mTimestampMaker(aTimestampMaker),
        mTaskQueue(TaskQueue::Create(
            GetMediaThreadPool(MediaThreadType::WEBRTC_WORKER),
            "VideoFrameConverter")),
        mPacer(MakeAndAddRef<Pacer<FrameToProcess>>(
            mTaskQueue, TimeDuration::FromSeconds(1))),
        mBufferPool(false, CONVERTER_BUFFER_POOL_SIZE) {
    MOZ_COUNT_CTOR(VideoFrameConverter);

    mPacingListener = mPacer->PacedItemEvent().Connect(
        mTaskQueue, [self = RefPtr<VideoFrameConverter>(this), this](
                        FrameToProcess aFrame, TimeStamp aTime) {
          QueueForProcessing(std::move(aFrame.mImage), aTime, aFrame.mSize,
                             aFrame.mForceBlack);
        });
  }

  void QueueVideoChunk(const VideoChunk& aChunk, bool aForceBlack) {
    gfx::IntSize size = aChunk.mFrame.GetIntrinsicSize();
    if (size.width == 0 || size.height == 0) {
      return;
    }

    TimeStamp t = aChunk.mTimeStamp;
    MOZ_ASSERT(!t.IsNull());

    mPacer->Enqueue(
        FrameToProcess(aChunk.mFrame.GetImage(), t, size, aForceBlack), t);
  }

  /**
   * An active VideoFrameConverter actively converts queued video frames.
   * While inactive, we keep track of the frame most recently queued for
   * processing, so it can be immediately sent out once activated.
   */
  void SetActive(bool aActive) {
    MOZ_ALWAYS_SUCCEEDS(mTaskQueue->Dispatch(NS_NewRunnableFunction(
        __func__, [self = RefPtr<VideoFrameConverter>(this), this, aActive,
                   time = TimeStamp::Now()] {
          if (mActive == aActive) {
            return;
          }
          MOZ_LOG(gVideoFrameConverterLog, LogLevel::Debug,
                  ("VideoFrameConverter %p is now %s", this,
                   aActive ? "active" : "inactive"));
          mActive = aActive;
          if (aActive && mLastFrameQueuedForProcessing.Serial() != -2) {
            // After activating, we re-process the last image that was queued
            // for processing so it can be immediately sent.
            mLastFrameQueuedForProcessing.mTime = time;

            MOZ_ALWAYS_SUCCEEDS(mTaskQueue->Dispatch(
                NewRunnableMethod<StoreCopyPassByLRef<FrameToProcess>>(
                    "VideoFrameConverter::ProcessVideoFrame", this,
                    &VideoFrameConverter::ProcessVideoFrame,
                    mLastFrameQueuedForProcessing)));
          }
        })));
  }

  void SetTrackEnabled(bool aTrackEnabled) {
    MOZ_ALWAYS_SUCCEEDS(mTaskQueue->Dispatch(NS_NewRunnableFunction(
        __func__, [self = RefPtr<VideoFrameConverter>(this), this,
                   aTrackEnabled, time = TimeStamp::Now()] {
          if (mTrackEnabled == aTrackEnabled) {
            return;
          }
          MOZ_LOG(gVideoFrameConverterLog, LogLevel::Debug,
                  ("VideoFrameConverter %p Track is now %s", this,
                   aTrackEnabled ? "enabled" : "disabled"));
          mTrackEnabled = aTrackEnabled;
          if (!aTrackEnabled) {
            // After disabling we immediately send a frame as black, so it can
            // be seen quickly, even if no frames are flowing. If no frame has
            // been queued for processing yet, we use the FrameToProcess default
            // size (640x480).
            mLastFrameQueuedForProcessing.mTime = time;
            mLastFrameQueuedForProcessing.mForceBlack = true;
            mLastFrameQueuedForProcessing.mImage = nullptr;

            MOZ_ALWAYS_SUCCEEDS(mTaskQueue->Dispatch(
                NewRunnableMethod<StoreCopyPassByLRef<FrameToProcess>>(
                    "VideoFrameConverter::ProcessVideoFrame", this,
                    &VideoFrameConverter::ProcessVideoFrame,
                    mLastFrameQueuedForProcessing)));
          }
        })));
  }

  void Shutdown() {
    mPacer->Shutdown()->Then(mTaskQueue, __func__,
                             [self = RefPtr<VideoFrameConverter>(this), this] {
                               mPacingListener.DisconnectIfExists();
                               mBufferPool.Release();
                               mLastFrameQueuedForProcessing = FrameToProcess();
                               mLastFrameConverted = Nothing();
                             });
  }

  MediaEventSourceExc<webrtc::VideoFrame>& VideoFrameConvertedEvent() {
    return mVideoFrameConvertedEvent;
  }

 protected:
  struct FrameToProcess {
    FrameToProcess() = default;

    FrameToProcess(RefPtr<layers::Image> aImage, TimeStamp aTime,
                   gfx::IntSize aSize, bool aForceBlack)
        : mImage(std::move(aImage)),
          mTime(aTime),
          mSize(aSize),
          mForceBlack(aForceBlack) {}

    RefPtr<layers::Image> mImage;
    TimeStamp mTime = TimeStamp::Now();
    gfx::IntSize mSize = gfx::IntSize(640, 480);
    bool mForceBlack = false;

    int32_t Serial() const {
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

  struct FrameConverted {
    FrameConverted(webrtc::VideoFrame aFrame, int32_t aSerial)
        : mFrame(std::move(aFrame)), mSerial(aSerial) {}

    webrtc::VideoFrame mFrame;
    int32_t mSerial;
  };

  MOZ_COUNTED_DTOR_VIRTUAL(VideoFrameConverter)

  void VideoFrameConverted(webrtc::VideoFrame aVideoFrame, int32_t aSerial) {
    MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn());

    MOZ_LOG(
        gVideoFrameConverterLog, LogLevel::Verbose,
        ("VideoFrameConverter %p: Converted a frame. Diff from last: %.3fms",
         this,
         static_cast<double>(aVideoFrame.timestamp_us() -
                             (mLastFrameConverted
                                  ? mLastFrameConverted->mFrame.timestamp_us()
                                  : aVideoFrame.timestamp_us())) /
             1000));

    // Check that time doesn't go backwards
    MOZ_ASSERT_IF(mLastFrameConverted,
                  aVideoFrame.timestamp_us() >
                      mLastFrameConverted->mFrame.timestamp_us());

    mLastFrameConverted = Some(FrameConverted(aVideoFrame, aSerial));

    mVideoFrameConvertedEvent.Notify(std::move(aVideoFrame));
  }

  void QueueForProcessing(RefPtr<layers::Image> aImage, TimeStamp aTime,
                          gfx::IntSize aSize, bool aForceBlack) {
    MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn());

    FrameToProcess frame{std::move(aImage), aTime, aSize,
                         aForceBlack || !mTrackEnabled};

    if (frame.mTime <= mLastFrameQueuedForProcessing.mTime) {
      MOZ_LOG(
          gVideoFrameConverterLog, LogLevel::Debug,
          ("VideoFrameConverter %p: Dropping a frame because time did not "
           "progress (%.3fs)",
           this,
           (mLastFrameQueuedForProcessing.mTime - frame.mTime).ToSeconds()));
      return;
    }

    if (frame.Serial() == mLastFrameQueuedForProcessing.Serial()) {
      // This is the same frame as the last one. We limit the same-frame rate to
      // 1 second, and rewrite the time so the frame-gap is in whole seconds.
      //
      // The pacer only starts duplicating frames every second if there is no
      // flow of frames into it. There are other reasons the same frame could
      // repeat here, and at a shorter interval than one second. For instance
      // after the sender is disabled (SetTrackEnabled) but there is still a
      // flow of frames into the pacer. All disabled frames have the same
      // serial.
      if (int32_t diffSec = static_cast<int32_t>(
              (frame.mTime - mLastFrameQueuedForProcessing.mTime).ToSeconds());
          diffSec != 0) {
        MOZ_LOG(
            gVideoFrameConverterLog, LogLevel::Verbose,
            ("VideoFrameConverter %p: Rewrote time interval for a duplicate "
             "frame from %.3fs to %.3fs",
             this,
             (frame.mTime - mLastFrameQueuedForProcessing.mTime).ToSeconds(),
             static_cast<float>(diffSec)));
        frame.mTime = mLastFrameQueuedForProcessing.mTime +
                      TimeDuration::FromSeconds(diffSec);
      } else {
        MOZ_LOG(
            gVideoFrameConverterLog, LogLevel::Verbose,
            ("VideoFrameConverter %p: Dropping a duplicate frame because a "
             "second hasn't passed (%.3fs)",
             this,
             (frame.mTime - mLastFrameQueuedForProcessing.mTime).ToSeconds()));
        return;
      }
    }

    mLastFrameQueuedForProcessing = std::move(frame);

    if (!mActive) {
      MOZ_LOG(
          gVideoFrameConverterLog, LogLevel::Debug,
          ("VideoFrameConverter %p: Ignoring a frame because we're inactive",
           this));
      return;
    }

    MOZ_ALWAYS_SUCCEEDS(mTaskQueue->Dispatch(
        NewRunnableMethod<StoreCopyPassByLRef<FrameToProcess>>(
            "VideoFrameConverter::ProcessVideoFrame", this,
            &VideoFrameConverter::ProcessVideoFrame,
            mLastFrameQueuedForProcessing)));
  }

  void ProcessVideoFrame(const FrameToProcess& aFrame) {
    MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn());

    if (aFrame.mTime < mLastFrameQueuedForProcessing.mTime) {
      MOZ_LOG(
          gVideoFrameConverterLog, LogLevel::Debug,
          ("VideoFrameConverter %p: Dropping a frame that is %.3f seconds "
           "behind latest",
           this,
           (mLastFrameQueuedForProcessing.mTime - aFrame.mTime).ToSeconds()));
      return;
    }

    const webrtc::Timestamp time =
        mTimestampMaker.ConvertMozTimeToRealtime(aFrame.mTime);

    if (mLastFrameConverted &&
        aFrame.Serial() == mLastFrameConverted->mSerial) {
      // This is the same input frame as last time. Avoid a conversion.
      webrtc::VideoFrame frame = mLastFrameConverted->mFrame;
      frame.set_timestamp_us(time.us());
      VideoFrameConverted(std::move(frame), mLastFrameConverted->mSerial);
      return;
    }

    if (aFrame.mForceBlack) {
      // Send a black image.
      rtc::scoped_refptr<webrtc::I420Buffer> buffer =
          mBufferPool.CreateI420Buffer(aFrame.mSize.width, aFrame.mSize.height);
      if (!buffer) {
        MOZ_DIAGNOSTIC_ASSERT(false,
                              "Buffers not leaving scope except for "
                              "reconfig, should never leak");
        MOZ_LOG(gVideoFrameConverterLog, LogLevel::Warning,
                ("VideoFrameConverter %p: Creating a buffer for a black video "
                 "frame failed",
                 this));
        return;
      }

      MOZ_LOG(gVideoFrameConverterLog, LogLevel::Verbose,
              ("VideoFrameConverter %p: Sending a black video frame", this));
      webrtc::I420Buffer::SetBlack(buffer.get());

      VideoFrameConverted(webrtc::VideoFrame::Builder()
                              .set_video_frame_buffer(buffer)
                              .set_timestamp_us(time.us())
                              .build(),
                          aFrame.Serial());
      return;
    }

    if (!aFrame.mImage) {
      // Don't send anything for null images.
      return;
    }

    MOZ_ASSERT(aFrame.mImage->GetSize() == aFrame.mSize);

    RefPtr<layers::PlanarYCbCrImage> image =
        aFrame.mImage->AsPlanarYCbCrImage();
    if (image) {
      dom::ImageUtils utils(image);
      if (utils.GetFormat() == dom::ImageBitmapFormat::YUV420P &&
          image->GetData()) {
        const layers::PlanarYCbCrData* data = image->GetData();
        rtc::scoped_refptr<webrtc::I420BufferInterface> video_frame_buffer =
            webrtc::WrapI420Buffer(
                aFrame.mImage->GetSize().width, aFrame.mImage->GetSize().height,
                data->mYChannel, data->mYStride, data->mCbChannel,
                data->mCbCrStride, data->mCrChannel, data->mCbCrStride,
                [image] { /* keep reference alive*/ });

        MOZ_LOG(gVideoFrameConverterLog, LogLevel::Verbose,
                ("VideoFrameConverter %p: Sending an I420 video frame", this));
        VideoFrameConverted(webrtc::VideoFrame::Builder()
                                .set_video_frame_buffer(video_frame_buffer)
                                .set_timestamp_us(time.us())
                                .build(),
                            aFrame.Serial());
        return;
      }
    }

    rtc::scoped_refptr<webrtc::I420Buffer> buffer =
        mBufferPool.CreateI420Buffer(aFrame.mSize.width, aFrame.mSize.height);
    if (!buffer) {
#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
      ++mFramesDropped;
#endif
      MOZ_DIAGNOSTIC_ASSERT(mFramesDropped <= 100, "Buffers must be leaking");
      MOZ_LOG(gVideoFrameConverterLog, LogLevel::Warning,
              ("VideoFrameConverter %p: Creating a buffer failed", this));
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
              ("VideoFrameConverter %p: Image conversion failed", this));
      return;
    }

    VideoFrameConverted(webrtc::VideoFrame::Builder()
                            .set_video_frame_buffer(buffer)
                            .set_timestamp_us(time.us())
                            .build(),
                        aFrame.Serial());
  }

 public:
  const dom::RTCStatsTimestampMaker mTimestampMaker;

  const RefPtr<TaskQueue> mTaskQueue;

 protected:
  // Used to pace future frames close to their rendering-time. Thread-safe.
  const RefPtr<Pacer<FrameToProcess>> mPacer;

  MediaEventProducerExc<webrtc::VideoFrame> mVideoFrameConvertedEvent;

  // Accessed only from mTaskQueue.
  MediaEventListener mPacingListener;
  webrtc::VideoFrameBufferPool mBufferPool;
  FrameToProcess mLastFrameQueuedForProcessing;
  Maybe<FrameConverted> mLastFrameConverted;
  bool mActive = false;
  bool mTrackEnabled = true;
#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
  size_t mFramesDropped = 0;
#endif
};

}  // namespace mozilla

#endif  // VideoFrameConverter_h
