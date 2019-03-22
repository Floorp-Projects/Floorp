/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef VideoFrameConverter_h
#define VideoFrameConverter_h

#include "ImageContainer.h"
#include "ImageToI420.h"
#include "VideoSegment.h"
#include "VideoUtils.h"
#include "nsISupportsImpl.h"
#include "nsThreadUtils.h"
#include "mozilla/TaskQueue.h"
#include "mozilla/dom/ImageBitmapBinding.h"
#include "mozilla/dom/ImageUtils.h"
#include "webrtc/common_video/include/i420_buffer_pool.h"
#include "webrtc/common_video/include/video_frame_buffer.h"
#include "webrtc/rtc_base/keep_ref_until_done.h"

// The number of frame buffers VideoFrameConverter may create before returning
// errors.
// Sometimes these are released synchronously but they can be forwarded all the
// way to the encoder for asynchronous encoding. With a pool size of 5,
// we allow 1 buffer for the current conversion, and 4 buffers to be queued at
// the encoder.
#define CONVERTER_BUFFER_POOL_SIZE 5

namespace mozilla {

mozilla::LazyLogModule gVideoFrameConverterLog("VideoFrameConverter");

class VideoConverterListener {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(VideoConverterListener)

  virtual void OnVideoFrameConverted(const webrtc::VideoFrame& aVideoFrame) = 0;

 protected:
  virtual ~VideoConverterListener() {}
};

// An async video frame format converter.
//
// Input is typically a MediaStream(Track)Listener driven by MediaStreamGraph.
//
// We keep track of the size of the TaskQueue so we can drop frames if
// conversion is taking too long.
//
// Output is passed through to all added VideoConverterListeners on a TaskQueue
// thread whenever a frame is converted.
class VideoFrameConverter {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(VideoFrameConverter)

  VideoFrameConverter()
      : mLength(0),
        mTaskQueue(
            new TaskQueue(GetMediaThreadPool(MediaThreadType::WEBRTC_DECODER),
                          "VideoFrameConverter")),
        mBufferPool(false, CONVERTER_BUFFER_POOL_SIZE),
        mLastImage(
            -1)  // -1 is not a guaranteed invalid serial. See bug 1262134.
#ifdef DEBUG
        ,
        mThrottleCount(0),
        mThrottleRecord(0)
#endif
        ,
        mLastFrame(nullptr, 0, 0, webrtc::kVideoRotation_0) {
    MOZ_COUNT_CTOR(VideoFrameConverter);
  }

  void QueueVideoChunk(const VideoChunk& aChunk, bool aForceBlack) {
    gfx::IntSize size = aChunk.mFrame.GetIntrinsicSize();
    if (size.width == 0 || size.width == 0) {
      return;
    }

    if (aChunk.IsNull()) {
      aForceBlack = true;
    } else {
      aForceBlack = aChunk.mFrame.GetForceBlack();
    }

    int32_t serial;
    if (aForceBlack) {
      // Reset the last-img check.
      // -1 is not a guaranteed invalid serial. See bug 1262134.
      serial = -1;
    } else {
      serial = aChunk.mFrame.GetImage()->GetSerial();
    }

    TimeStamp t = aChunk.mTimeStamp;
    MOZ_ASSERT(!t.IsNull());
    if (serial == mLastImage && !mLastFrameSent.IsNull()) {
      // With a non-direct listener we get passed duplicate frames every ~10ms
      // even with no frame change.
      return;
    }
    mLastFrameSent = t;
    mLastImage = serial;

    // A throttling limit of 1 allows us to convert 2 frames concurrently.
    // It's short enough to not build up too significant a delay, while
    // giving us a margin to not cause some machines to drop every other frame.
    const int32_t queueThrottlingLimit = 1;
    if (mLength > queueThrottlingLimit) {
      MOZ_LOG(gVideoFrameConverterLog, LogLevel::Debug,
              ("VideoFrameConverter %p queue is full. Throttling by "
               "throwing away a frame.",
               this));
#ifdef DEBUG
      ++mThrottleCount;
      mThrottleRecord = std::max(mThrottleCount, mThrottleRecord);
#endif
      return;
    }

#ifdef DEBUG
    if (mThrottleCount > 0) {
      if (mThrottleCount > 5) {
        // Log at a higher level when we have large drops.
        MOZ_LOG(gVideoFrameConverterLog, LogLevel::Info,
                ("VideoFrameConverter %p stopped throttling after throwing "
                 "away %d frames. Longest throttle so far was %d frames.",
                 this, mThrottleCount, mThrottleRecord));
      } else {
        MOZ_LOG(gVideoFrameConverterLog, LogLevel::Debug,
                ("VideoFrameConverter %p stopped throttling after throwing "
                 "away %d frames. Longest throttle so far was %d frames.",
                 this, mThrottleCount, mThrottleRecord));
      }
      mThrottleCount = 0;
    }
#endif

    ++mLength;  // Atomic

    nsCOMPtr<nsIRunnable> runnable =
        NewRunnableMethod<StoreRefPtrPassByPtr<layers::Image>, gfx::IntSize,
                          bool>("VideoFrameConverter::ProcessVideoFrame", this,
                                &VideoFrameConverter::ProcessVideoFrame,
                                aChunk.mFrame.GetImage(), size, aForceBlack);
    nsresult rv = mTaskQueue->Dispatch(runnable.forget());
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
    nsresult rv = mTaskQueue->Dispatch(NS_NewRunnableFunction(
        "VideoFrameConverter::Shutdown",
        [self = RefPtr<VideoFrameConverter>(this), this] {
          if (mSameFrameTimer) {
            mSameFrameTimer->Cancel();
          }
          mListeners.Clear();
        }));
    MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));
    Unused << rv;
  }

 protected:
  virtual ~VideoFrameConverter() { MOZ_COUNT_DTOR(VideoFrameConverter); }

  static void SameFrameTick(nsITimer* aTimer, void* aClosure) {
    MOZ_ASSERT(aClosure);
    VideoFrameConverter* self = static_cast<VideoFrameConverter*>(aClosure);
    MOZ_ASSERT(self->mTaskQueue->IsCurrentThreadIn());

    if (!self->mLastFrame.video_frame_buffer()) {
      return;
    }

    for (RefPtr<VideoConverterListener>& listener : self->mListeners) {
      listener->OnVideoFrameConverted(self->mLastFrame);
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

    mLastFrame = aVideoFrame;

    for (RefPtr<VideoConverterListener>& listener : mListeners) {
      listener->OnVideoFrameConverted(aVideoFrame);
    }
  }

  void ProcessVideoFrame(layers::Image* aImage, gfx::IntSize aSize,
                         bool aForceBlack) {
    --mLength;  // Atomic
    MOZ_ASSERT(mLength >= 0);

    // See Bug 1529581 - Ideally we'd use the mTimestamp from the chunk
    // passed into QueueVideoChunk rather than the webrtc.org clock here.
    int64_t now = webrtc::Clock::GetRealTimeClock()->TimeInMilliseconds();

    if (aForceBlack) {
      // Send a black image.
      rtc::scoped_refptr<webrtc::I420Buffer> buffer =
          mBufferPool.CreateBuffer(aSize.width, aSize.height);
      if (!buffer) {
        MOZ_DIAGNOSTIC_ASSERT(false,
                              "Buffers not leaving scope except for "
                              "reconfig, should never leak");
        MOZ_LOG(gVideoFrameConverterLog, LogLevel::Warning,
                ("Creating a buffer for a black video frame failed"));
        return;
      }

      MOZ_LOG(gVideoFrameConverterLog, LogLevel::Debug,
              ("Sending a black video frame"));
      webrtc::I420Buffer::SetBlack(buffer);

      webrtc::VideoFrame frame(buffer, 0, // not setting rtp timestamp
                               now, webrtc::kVideoRotation_0);
      VideoFrameConverted(frame);
      return;
    }

    MOZ_RELEASE_ASSERT(aImage, "Must have image if not forcing black");
    MOZ_ASSERT(aImage->GetSize() == aSize);

    if (layers::PlanarYCbCrImage* image = aImage->AsPlanarYCbCrImage()) {
      dom::ImageUtils utils(image);
      if (utils.GetFormat() == dom::ImageBitmapFormat::YUV420P &&
          image->GetData()) {
        const layers::PlanarYCbCrData* data = image->GetData();
        rtc::scoped_refptr<webrtc::WrappedI420Buffer> video_frame_buffer(
            new rtc::RefCountedObject<webrtc::WrappedI420Buffer>(
                aImage->GetSize().width, aImage->GetSize().height,
                data->mYChannel, data->mYStride, data->mCbChannel,
                data->mCbCrStride, data->mCrChannel, data->mCbCrStride,
                rtc::KeepRefUntilDone(image)));

        webrtc::VideoFrame i420_frame(video_frame_buffer,
                                      0, // not setting rtp timestamp
                                      now, webrtc::kVideoRotation_0);
        MOZ_LOG(gVideoFrameConverterLog, LogLevel::Debug,
                ("Sending an I420 video frame"));
        VideoFrameConverted(i420_frame);
        return;
      }
    }

    rtc::scoped_refptr<webrtc::I420Buffer> buffer =
        mBufferPool.CreateBuffer(aSize.width, aSize.height);
    if (!buffer) {
      MOZ_DIAGNOSTIC_ASSERT(false,
                            "Buffers not leaving scope except for "
                            "reconfig, should never leak");
      MOZ_LOG(gVideoFrameConverterLog, LogLevel::Warning,
              ("Creating a buffer for a black video frame failed"));
      return;
    }

    nsresult rv =
        ConvertToI420(aImage, buffer->MutableDataY(), buffer->StrideY(),
                      buffer->MutableDataU(), buffer->StrideU(),
                      buffer->MutableDataV(), buffer->StrideV());

    if (NS_FAILED(rv)) {
      MOZ_LOG(gVideoFrameConverterLog, LogLevel::Warning,
              ("Image conversion failed"));
      return;
    }

    webrtc::VideoFrame frame(buffer, 0, //not setting rtp timestamp
                             now, webrtc::kVideoRotation_0);
    VideoFrameConverted(frame);
  }

  Atomic<int32_t, Relaxed> mLength;
  const RefPtr<TaskQueue> mTaskQueue;
  webrtc::I420BufferPool mBufferPool;

  // Written and read from the queueing thread (normally MSG).
  int32_t mLastImage;        // serial number of last Image
  TimeStamp mLastFrameSent;  // The time we sent the last frame.
#ifdef DEBUG
  uint32_t mThrottleCount;
  uint32_t mThrottleRecord;
#endif

  // Accessed only from mTaskQueue.
  nsCOMPtr<nsITimer> mSameFrameTimer;
  webrtc::VideoFrame mLastFrame;
  nsTArray<RefPtr<VideoConverterListener>> mListeners;
};

}  // namespace mozilla

#endif // VideoFrameConverter_h
