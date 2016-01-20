/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RtspExtractor.h"

#include "mozilla/ReentrantMonitor.h"

using namespace android;

#define FRAME_DEFAULT_SIZE 1024

namespace mozilla {

/* class RtspMediaSource : implements MediaSource for OMX.
 * The decoder thread will trigger the MediaDecodeStateMachine to read a/v frame.
 * Then RtspOmxReader calls OMX decoder to decode a/v frame. Finally the code
 * path run into the read() here, it reads un-decoded frame data from mResource
 * and construct a MediaBuffer for output to OMX decoder.
 * */
class RtspMediaSource final : public MediaSource {
public:
  RtspMediaSource(RtspMediaResource* aRtspMediaResource,
                  ssize_t aTrackIdx,
                  uint32_t aFrameMaxSize,
                  const sp<MetaData>& aMeta)
  : mRtspResource(aRtspMediaResource)
  , mFormat(aMeta)
  , mTrackIdx(aTrackIdx)
  , mMonitor("RtspMediaSource.mMonitor")
  , mIsStarted(false)
  , mGroup(nullptr)
  , mBuffer(nullptr)
  , mFrameMaxSize(aFrameMaxSize) {}
  virtual ~RtspMediaSource() {}
  status_t start(MetaData* params = nullptr) override;
  status_t stop() override;
  sp<MetaData> getFormat() override {
    ReentrantMonitorAutoEnter mon(mMonitor);
    return mFormat;
  }
  status_t read(MediaBuffer** buffer,
                const ReadOptions* options = nullptr) override ;
private:
  RefPtr<RtspMediaResource> mRtspResource;
  sp<MetaData> mFormat;
  uint32_t mTrackIdx;
  ReentrantMonitor mMonitor;
  bool mIsStarted;

  // mGroup owns the mBuffer. mFrameMaxSize is the mBuffer size.
  // mBuffer is the input buffer for omx decoder.
  nsAutoPtr<MediaBufferGroup> mGroup;
  MediaBuffer* mBuffer;
  uint32_t mFrameMaxSize;
};

status_t
RtspMediaSource::start(MetaData* params)
{
  ReentrantMonitorAutoEnter mon(mMonitor);
  if (!mIsStarted) {
    // RtspMediaSource relinquish the ownership of MediaBuffer |buf| to mGroup.
    mGroup = new MediaBufferGroup();
    MediaBuffer* buf = new MediaBuffer(mFrameMaxSize);
    mGroup->add_buffer(buf);
    mIsStarted = true;
  }
  return OK;
}

status_t
RtspMediaSource::stop()
{
  ReentrantMonitorAutoEnter mon(mMonitor);
  if (mIsStarted) {
    if (mBuffer) {
      mBuffer->release();
      mBuffer = nullptr;
    }
    mGroup = nullptr;
    mIsStarted = false;
  }
  return OK;
}

status_t
RtspMediaSource::read(MediaBuffer** out, const ReadOptions* options)
{
  ReentrantMonitorAutoEnter mon(mMonitor);
  NS_ENSURE_TRUE(mIsStarted, MEDIA_ERROR_BASE);
  NS_ENSURE_TRUE(out, MEDIA_ERROR_BASE);
  *out = nullptr;

  // Video/audio track's initial frame size is FRAME_DEFAULT_SIZE.
  // We need to realloc the mBuffer if the mBuffer doesn't have enough space
  // for next ReadFrameFromTrack function. (actualFrameSize > mFrameMaxSize)
  status_t err;
  uint32_t readCount;
  uint32_t actualFrameSize;
  uint64_t time;
  nsresult rv;

  while (1) {
    err = mGroup->acquire_buffer(&mBuffer);
    NS_ENSURE_TRUE(err == OK, err);
    rv = mRtspResource->ReadFrameFromTrack((uint8_t *)mBuffer->data(),
                                           mFrameMaxSize, mTrackIdx, readCount,
                                           time, actualFrameSize);
    if (NS_FAILED(rv)) {
      // Release mGroup and mBuffer.
      stop();
      // Since RtspMediaSource is an implementation of Android media source,
      // it's held by OMXCodec and isn't released yet. So we have to re-construct
      // mGroup and mBuffer.
      start();
      NS_WARNING("ReadFrameFromTrack failed; releasing buffers and returning.");
      return ERROR_END_OF_STREAM;
    }
    if (actualFrameSize > mFrameMaxSize) {
      // release mGroup and mBuffer
      stop();
      // re-construct mGroup and mBuffer
      mFrameMaxSize = actualFrameSize;
      err = start();
      NS_ENSURE_TRUE(err == OK, err);
    } else {
      // ReadFrameFromTrack success, break the while loop.
      break;
    }
  }
  mBuffer->set_range(0, readCount);
  if (NS_SUCCEEDED(rv)) {
    mBuffer->meta_data()->clear();
    // fill the meta data
    mBuffer->meta_data()->setInt64(kKeyTime, time);
    *out = mBuffer;
    mBuffer = nullptr;
    return OK;
  }

  return ERROR_END_OF_STREAM;
}

size_t
RtspExtractor::countTracks()
{
  uint8_t tracks = 0;
  if (mController) {
    mController->GetTotalTracks(&tracks);
  }
  return size_t(tracks);
}

sp<MediaSource>
RtspExtractor::getTrack(size_t index)
{
  NS_ENSURE_TRUE(index < countTracks(), nullptr);
  sp<MetaData> meta = getTrackMetaData(index);
  sp<MediaSource> source = new RtspMediaSource(mRtspResource,
                                               index,
                                               FRAME_DEFAULT_SIZE,
                                               meta);
  return source;
}

sp<MetaData>
RtspExtractor::getTrackMetaData(size_t index, uint32_t flag)
{
  NS_ENSURE_TRUE(index < countTracks(), nullptr);
  sp<MetaData> meta = new MetaData();
  nsCOMPtr<nsIStreamingProtocolMetaData> rtspMetadata;
  mController->GetTrackMetaData(index, getter_AddRefs(rtspMetadata));

  if (rtspMetadata) {
    // Convert msMeta into meta.
    // The getter function of nsIStreamingProtocolMetaData will initialize the
    // metadata values to 0 before setting them.
    nsCString mime;
    rtspMetadata->GetMimeType(mime);
    meta->setCString(kKeyMIMEType, mime.get());
    uint32_t temp32;
    rtspMetadata->GetWidth(&temp32);
    meta->setInt32(kKeyWidth, temp32);
    rtspMetadata->GetHeight(&temp32);
    meta->setInt32(kKeyHeight, temp32);
    rtspMetadata->GetSampleRate(&temp32);
    meta->setInt32(kKeySampleRate, temp32);
    rtspMetadata->GetChannelCount(&temp32);
    meta->setInt32(kKeyChannelCount, temp32);
    uint64_t temp64;
    rtspMetadata->GetDuration(&temp64);
    meta->setInt64(kKeyDuration, temp64);

    nsCString tempCString;
    rtspMetadata->GetEsdsData(tempCString);
    if (tempCString.Length()) {
      meta->setData(kKeyESDS, 0, tempCString.get(), tempCString.Length());
    }
    rtspMetadata->GetAvccData(tempCString);
    if (tempCString.Length()) {
      meta->setData(kKeyAVCC, 0, tempCString.get(), tempCString.Length());
    }
  }
  return meta;
}

uint32_t
RtspExtractor::flags() const
{
  if (mRtspResource->IsRealTime()) {
    return 0;
  } else {
    return MediaExtractor::CAN_SEEK;
  }
}

} // namespace mozilla

