/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <unistd.h>
#include <fcntl.h>

#include "base/basictypes.h"
#include <cutils/properties.h>
#include <stagefright/MediaExtractor.h>
#include <stagefright/MetaData.h>
#include <stagefright/OMXClient.h>
#include <stagefright/OMXCodec.h>
#include <OMX.h>

#include "mozilla/Preferences.h"
#include "mozilla/Types.h"
#include "MPAPI.h"
#include "prlog.h"

#include "GonkNativeWindow.h"
#include "GonkNativeWindowClient.h"
#include "OmxDecoder.h"

#ifdef PR_LOGGING
PRLogModuleInfo *gOmxDecoderLog;
#define LOG(type, msg...) PR_LOG(gOmxDecoderLog, type, (msg))
#else
#define LOG(x...)
#endif

using namespace MPAPI;
using namespace mozilla;

namespace mozilla {
namespace layers {

VideoGraphicBuffer::VideoGraphicBuffer(const android::wp<android::OmxDecoder> aOmxDecoder,
                                       android::MediaBuffer *aBuffer,
                                       SurfaceDescriptor *aDescriptor)
  : GraphicBufferLocked(*aDescriptor),
    mMediaBuffer(aBuffer),
    mOmxDecoder(aOmxDecoder)
{
  mMediaBuffer->add_ref();
}

VideoGraphicBuffer::~VideoGraphicBuffer()
{
  if (mMediaBuffer) {
    mMediaBuffer->release();
  }
}

void
VideoGraphicBuffer::Unlock()
{
  android::sp<android::OmxDecoder> omxDecoder = mOmxDecoder.promote();
  if (omxDecoder.get()) {
    omxDecoder->ReleaseVideoBuffer(mMediaBuffer);
  } else {
    NS_WARNING("OmxDecoder is not present");
    if (mMediaBuffer) {
      mMediaBuffer->release();
    }
  }
  mMediaBuffer = nullptr;
}

}
}

namespace android {

MediaStreamSource::MediaStreamSource(MediaResource *aResource,
                                     AbstractMediaDecoder *aDecoder) :
  mResource(aResource), mDecoder(aDecoder)
{
}

MediaStreamSource::~MediaStreamSource()
{
}

status_t MediaStreamSource::initCheck() const
{
  return OK;
}

ssize_t MediaStreamSource::readAt(off64_t offset, void *data, size_t size)
{
  char *ptr = static_cast<char *>(data);
  size_t todo = size;
  while (todo > 0) {
    uint32_t bytesRead;
    if ((offset != mResource->Tell() &&
         NS_FAILED(mResource->Seek(nsISeekableStream::NS_SEEK_SET, offset))) ||
        NS_FAILED(mResource->Read(ptr, todo, &bytesRead))) {
      return ERROR_IO;
    }

    if (bytesRead == 0) {
      return size - todo;
    }

    offset += bytesRead;
    todo -= bytesRead;
    ptr += bytesRead;
  }
  return size;
}

status_t MediaStreamSource::getSize(off64_t *size)
{
  uint64_t length = mResource->GetLength();
  if (length == static_cast<uint64_t>(-1))
    return ERROR_UNSUPPORTED;

  *size = length;

  return OK;
}

}  // namespace android

using namespace android;

OmxDecoder::OmxDecoder(MediaResource *aResource,
                       AbstractMediaDecoder *aDecoder) :
  mDecoder(aDecoder),
  mResource(aResource),
  mVideoWidth(0),
  mVideoHeight(0),
  mVideoColorFormat(0),
  mVideoStride(0),
  mVideoSliceHeight(0),
  mVideoRotation(0),
  mAudioChannels(-1),
  mAudioSampleRate(-1),
  mDurationUs(-1),
  mVideoBuffer(nullptr),
  mAudioBuffer(nullptr),
  mIsVideoSeeking(false),
  mAudioMetadataRead(false),
  mPaused(false)
{
}

OmxDecoder::~OmxDecoder()
{
  ReleaseVideoBuffer();
  ReleaseAudioBuffer();

  if (mVideoSource.get()) {
    mVideoSource->stop();
  }

  if (mAudioSource.get()) {
    mAudioSource->stop();
  }
}

class AutoStopMediaSource {
  sp<MediaSource> mMediaSource;
public:
  AutoStopMediaSource(const sp<MediaSource>& aMediaSource) : mMediaSource(aMediaSource) {
  }

  ~AutoStopMediaSource() {
    mMediaSource->stop();
  }
};

static sp<IOMX> sOMX = nullptr;
static sp<IOMX> GetOMX() {
  if(sOMX.get() == nullptr) {
    sOMX = new OMX;
    }
  return sOMX;
}

bool OmxDecoder::Init() {
#ifdef PR_LOGGING
  if (!gOmxDecoderLog) {
    gOmxDecoderLog = PR_NewLogModule("OmxDecoder");
  }
#endif

  //register sniffers, if they are not registered in this process.
  DataSource::RegisterDefaultSniffers();

  sp<DataSource> dataSource = new MediaStreamSource(mResource, mDecoder);
  if (dataSource->initCheck()) {
    NS_WARNING("Initializing DataSource for OMX decoder failed");
    return false;
  }

  mResource->SetReadMode(MediaCacheStream::MODE_METADATA);

  sp<MediaExtractor> extractor = MediaExtractor::Create(dataSource);
  if (extractor == nullptr) {
    NS_WARNING("Could not create MediaExtractor");
    return false;
  }

  ssize_t audioTrackIndex = -1;
  ssize_t videoTrackIndex = -1;
  const char *audioMime = nullptr;

  for (size_t i = 0; i < extractor->countTracks(); ++i) {
    sp<MetaData> meta = extractor->getTrackMetaData(i);

    int32_t bitRate;
    if (!meta->findInt32(kKeyBitRate, &bitRate))
      bitRate = 0;

    const char *mime;
    if (!meta->findCString(kKeyMIMEType, &mime)) {
      continue;
    }

    if (videoTrackIndex == -1 && !strncasecmp(mime, "video/", 6)) {
      videoTrackIndex = i;
    } else if (audioTrackIndex == -1 && !strncasecmp(mime, "audio/", 6)) {
      audioTrackIndex = i;
      audioMime = mime;
    }
  }

  if (videoTrackIndex == -1 && audioTrackIndex == -1) {
    NS_WARNING("OMX decoder could not find video or audio tracks");
    return false;
  }

  mResource->SetReadMode(MediaCacheStream::MODE_PLAYBACK);

  int64_t totalDurationUs = 0;

  mNativeWindow = new GonkNativeWindow();
  mNativeWindowClient = new GonkNativeWindowClient(mNativeWindow);

  // OMXClient::connect() always returns OK and abort's fatally if
  // it can't connect.
  OMXClient client;
  DebugOnly<status_t> err = client.connect();
  NS_ASSERTION(err == OK, "Failed to connect to OMX in mediaserver.");
  sp<IOMX> omx = client.interface();

  sp<MediaSource> videoTrack;
  sp<MediaSource> videoSource;
  if (videoTrackIndex != -1 && (videoTrack = extractor->getTrack(videoTrackIndex)) != nullptr) {
    // Experience with OMX codecs is that only the HW decoders are
    // worth bothering with, at least on the platforms where this code
    // is currently used, and for formats this code is currently used
    // for (h.264).  So if we don't get a hardware decoder, just give
    // up.
    int flags = kHardwareCodecsOnly;

    char propQemu[PROPERTY_VALUE_MAX];
    property_get("ro.kernel.qemu", propQemu, "");
    if (!strncmp(propQemu, "1", 1)) {
      // If we are in emulator, allow to fall back to software.
      flags = 0;
    }
    videoSource = OMXCodec::Create(omx,
                                   videoTrack->getFormat(),
                                   false, // decoder
                                   videoTrack,
                                   nullptr,
                                   flags,
                                   mNativeWindowClient);
    if (videoSource == nullptr) {
      NS_WARNING("Couldn't create OMX video source");
      return false;
    }

    // Check if this video is sized such that we're comfortable
    // possibly using an OMX decoder.
    int32_t maxWidth, maxHeight;
    char propValue[PROPERTY_VALUE_MAX];
    property_get("ro.moz.omx.hw.max_width", propValue, "-1");
    maxWidth = atoi(propValue);
    property_get("ro.moz.omx.hw.max_height", propValue, "-1");
    maxHeight = atoi(propValue);

    int32_t width = -1, height = -1;
    if (maxWidth > 0 && maxHeight > 0 &&
        !(videoSource->getFormat()->findInt32(kKeyWidth, &width) &&
          videoSource->getFormat()->findInt32(kKeyHeight, &height) &&
          width * height <= maxWidth * maxHeight)) {
      printf_stderr("Failed to get video size, or it was too large for HW decoder (<w=%d, h=%d> but <maxW=%d, maxH=%d>)",
                    width, height, maxWidth, maxHeight);
      return false;
    }

    if (videoSource->start() != OK) {
      NS_WARNING("Couldn't start OMX video source");
      return false;
    }

    int64_t durationUs;
    if (videoTrack->getFormat()->findInt64(kKeyDuration, &durationUs)) {
      if (durationUs > totalDurationUs)
        totalDurationUs = durationUs;
    }
  }

  sp<MediaSource> audioTrack;
  sp<MediaSource> audioSource;
  if (audioTrackIndex != -1 && (audioTrack = extractor->getTrack(audioTrackIndex)) != nullptr)
  {
    if (!strcasecmp(audioMime, "audio/raw")) {
      audioSource = audioTrack;
    } else {
      // try to load hardware codec in mediaserver process.
      int flags = kHardwareCodecsOnly;
      audioSource = OMXCodec::Create(omx,
                                     audioTrack->getFormat(),
                                     false, // decoder
                                     audioTrack,
                                     nullptr,
                                     flags);
    }
    if (audioSource == nullptr) {
      // try to load software codec in this process.
      int flags = kSoftwareCodecsOnly;
      audioSource = OMXCodec::Create(GetOMX(),
                                     audioTrack->getFormat(),
                                     false, // decoder
                                     audioTrack,
                                     nullptr,
                                     flags);
      if (audioSource == nullptr) {
        NS_WARNING("Couldn't create OMX audio source");
        return false;
      }
    }
    if (audioSource->start() != OK) {
      NS_WARNING("Couldn't start OMX audio source");
      return false;
    }

    int64_t durationUs;
    if (audioTrack->getFormat()->findInt64(kKeyDuration, &durationUs)) {
      if (durationUs > totalDurationUs)
        totalDurationUs = durationUs;
    }
  }

  // set decoder state
  mVideoTrack = videoTrack;
  mVideoSource = videoSource;
  mAudioTrack = audioTrack;
  mAudioSource = audioSource;
  mDurationUs = totalDurationUs;

  if (mVideoSource.get() && !SetVideoFormat()) {
    NS_WARNING("Couldn't set OMX video format");
    return false;
  }

  // To reliably get the channel and sample rate data we need to read from the
  // audio source until we get a INFO_FORMAT_CHANGE status
  if (mAudioSource.get()) {
    status_t err = mAudioSource->read(&mAudioBuffer);
    if (err != INFO_FORMAT_CHANGED) {
      if (err != OK) {
        NS_WARNING("Couldn't read audio buffer from OMX decoder");
        return false;
      }
      sp<MetaData> meta = mAudioSource->getFormat();
      if (!meta->findInt32(kKeyChannelCount, &mAudioChannels) ||
          !meta->findInt32(kKeySampleRate, &mAudioSampleRate)) {
        NS_WARNING("Couldn't get audio metadata from OMX decoder");
        return false;
      }
      mAudioMetadataRead = true;
    }
    else if (!SetAudioFormat()) {
      NS_WARNING("Couldn't set audio format");
      return false;
    }
  }

  return true;
}

bool OmxDecoder::SetVideoFormat() {
  const char *componentName;

  if (!mVideoSource->getFormat()->findInt32(kKeyWidth, &mVideoWidth) ||
      !mVideoSource->getFormat()->findInt32(kKeyHeight, &mVideoHeight) ||
      !mVideoSource->getFormat()->findCString(kKeyDecoderComponent, &componentName) ||
      !mVideoSource->getFormat()->findInt32(kKeyColorFormat, &mVideoColorFormat) ) {
    return false;
  }

  if (!mVideoSource->getFormat()->findInt32(kKeyStride, &mVideoStride)) {
    mVideoStride = mVideoWidth;
    NS_WARNING("stride not available, assuming width");
  }

  if (!mVideoSource->getFormat()->findInt32(kKeySliceHeight, &mVideoSliceHeight)) {
    mVideoSliceHeight = mVideoHeight;
    NS_WARNING("slice height not available, assuming height");
  }

  if (!mVideoSource->getFormat()->findInt32(kKeyRotation, &mVideoRotation)) {
    mVideoRotation = 0;
    NS_WARNING("rotation not available, assuming 0");
  }

  LOG(PR_LOG_DEBUG, "width: %d height: %d component: %s format: %d stride: %d sliceHeight: %d rotation: %d",
      mVideoWidth, mVideoHeight, componentName, mVideoColorFormat,
      mVideoStride, mVideoSliceHeight, mVideoRotation);

  return true;
}

bool OmxDecoder::SetAudioFormat() {
  // If the format changed, update our cached info.
  if (!mAudioSource->getFormat()->findInt32(kKeyChannelCount, &mAudioChannels) ||
      !mAudioSource->getFormat()->findInt32(kKeySampleRate, &mAudioSampleRate)) {
    return false;
  }

  LOG(PR_LOG_DEBUG, "channelCount: %d sampleRate: %d",
      mAudioChannels, mAudioSampleRate);

  return true;
}

void OmxDecoder::ReleaseVideoBuffer() {
  if (mVideoBuffer) {
    mVideoBuffer->release();
    mVideoBuffer = nullptr;
  }
}

void OmxDecoder::ReleaseAudioBuffer() {
  if (mAudioBuffer) {
    mAudioBuffer->release();
    mAudioBuffer = nullptr;
  }
}

void OmxDecoder::PlanarYUV420Frame(VideoFrame *aFrame, int64_t aTimeUs, void *aData, size_t aSize, bool aKeyFrame) {
  void *y = aData;
  void *u = static_cast<uint8_t *>(y) + mVideoStride * mVideoSliceHeight;
  void *v = static_cast<uint8_t *>(u) + mVideoStride/2 * mVideoSliceHeight/2;

  aFrame->Set(aTimeUs, aKeyFrame,
              aData, aSize, mVideoStride, mVideoSliceHeight, mVideoRotation,
              y, mVideoStride, mVideoWidth, mVideoHeight, 0, 0,
              u, mVideoStride/2, mVideoWidth/2, mVideoHeight/2, 0, 0,
              v, mVideoStride/2, mVideoWidth/2, mVideoHeight/2, 0, 0);
}

void OmxDecoder::CbYCrYFrame(VideoFrame *aFrame, int64_t aTimeUs, void *aData, size_t aSize, bool aKeyFrame) {
  aFrame->Set(aTimeUs, aKeyFrame,
              aData, aSize, mVideoStride, mVideoSliceHeight, mVideoRotation,
              aData, mVideoStride, mVideoWidth, mVideoHeight, 1, 1,
              aData, mVideoStride, mVideoWidth/2, mVideoHeight/2, 0, 3,
              aData, mVideoStride, mVideoWidth/2, mVideoHeight/2, 2, 3);
}

void OmxDecoder::SemiPlanarYUV420Frame(VideoFrame *aFrame, int64_t aTimeUs, void *aData, size_t aSize, bool aKeyFrame) {
  void *y = aData;
  void *uv = static_cast<uint8_t *>(y) + (mVideoStride * mVideoSliceHeight);

  aFrame->Set(aTimeUs, aKeyFrame,
              aData, aSize, mVideoStride, mVideoSliceHeight, mVideoRotation,
              y, mVideoStride, mVideoWidth, mVideoHeight, 0, 0,
              uv, mVideoStride, mVideoWidth/2, mVideoHeight/2, 0, 1,
              uv, mVideoStride, mVideoWidth/2, mVideoHeight/2, 1, 1);
}

void OmxDecoder::SemiPlanarYVU420Frame(VideoFrame *aFrame, int64_t aTimeUs, void *aData, size_t aSize, bool aKeyFrame) {
  SemiPlanarYUV420Frame(aFrame, aTimeUs, aData, aSize, aKeyFrame);
  aFrame->Cb.mOffset = 1;
  aFrame->Cr.mOffset = 0;
}

bool OmxDecoder::ToVideoFrame(VideoFrame *aFrame, int64_t aTimeUs, void *aData, size_t aSize, bool aKeyFrame) {
  const int OMX_QCOM_COLOR_FormatYVU420SemiPlanar = 0x7FA30C00;

  aFrame->mGraphicBuffer = nullptr;

  switch (mVideoColorFormat) {
  case OMX_COLOR_FormatYUV420Planar:
    PlanarYUV420Frame(aFrame, aTimeUs, aData, aSize, aKeyFrame);
    break;
  case OMX_COLOR_FormatCbYCrY:
    CbYCrYFrame(aFrame, aTimeUs, aData, aSize, aKeyFrame);
    break;
  case OMX_COLOR_FormatYUV420SemiPlanar:
    SemiPlanarYUV420Frame(aFrame, aTimeUs, aData, aSize, aKeyFrame);
    break;
  case OMX_QCOM_COLOR_FormatYVU420SemiPlanar:
    SemiPlanarYVU420Frame(aFrame, aTimeUs, aData, aSize, aKeyFrame);
    break;
  default:
    LOG(PR_LOG_DEBUG, "Unknown video color format %08x", mVideoColorFormat);
    return false;
  }
  return true;
}

bool OmxDecoder::ToAudioFrame(AudioFrame *aFrame, int64_t aTimeUs, void *aData, size_t aDataOffset, size_t aSize, int32_t aAudioChannels, int32_t aAudioSampleRate)
{
  aFrame->Set(aTimeUs, static_cast<char *>(aData) + aDataOffset, aSize, aAudioChannels, aAudioSampleRate);
  return true;
}

bool OmxDecoder::ReadVideo(VideoFrame *aFrame, int64_t aTimeUs,
                           bool aKeyframeSkip, bool aDoSeek)
{
  if (!mVideoSource.get())
    return false;

  ReleaseVideoBuffer();

  status_t err;

  if (aDoSeek) {
    {
      Mutex::Autolock autoLock(mSeekLock);
      mIsVideoSeeking = true;
    }
    MediaSource::ReadOptions options;
    options.setSeekTo(aTimeUs, MediaSource::ReadOptions::SEEK_PREVIOUS_SYNC);
    err = mVideoSource->read(&mVideoBuffer, &options);
    {
      Mutex::Autolock autoLock(mSeekLock);
      mIsVideoSeeking = false;
      ReleaseAllPendingVideoBuffersLocked();
    }
  } else {
    err = mVideoSource->read(&mVideoBuffer);
  }

  if (err == OK && mVideoBuffer->range_length() > 0) {
    int64_t timeUs;
    int32_t unreadable;
    int32_t keyFrame;

    if (!mVideoBuffer->meta_data()->findInt64(kKeyTime, &timeUs) ) {
      NS_WARNING("OMX decoder did not return frame time");
      return false;
    }

    if (!mVideoBuffer->meta_data()->findInt32(kKeyIsSyncFrame, &keyFrame)) {
      keyFrame = 0;
    }

    if (!mVideoBuffer->meta_data()->findInt32(kKeyIsUnreadable, &unreadable)) {
      unreadable = 0;
    }

    mozilla::layers::SurfaceDescriptor *descriptor = nullptr;
    if ((mVideoBuffer->graphicBuffer().get())) {
      descriptor = mNativeWindow->getSurfaceDescriptorFromBuffer(mVideoBuffer->graphicBuffer().get());
    }

    if (descriptor) {
      // Change the descriptor's size to video's size. There are cases that
      // GraphicBuffer's size and actual video size is different.
      // See Bug 850566.
      const mozilla::layers::SurfaceDescriptorGralloc& grallocDesc = descriptor->get_SurfaceDescriptorGralloc();
      mozilla::layers::SurfaceDescriptor newDescriptor = mozilla::layers::SurfaceDescriptorGralloc(grallocDesc.bufferParent(),
                                                               grallocDesc.bufferChild(), nsIntSize(mVideoWidth, mVideoHeight), grallocDesc.external());

      aFrame->mGraphicBuffer = new mozilla::layers::VideoGraphicBuffer(this, mVideoBuffer, &newDescriptor);
      aFrame->mRotation = mVideoRotation;
      aFrame->mTimeUs = timeUs;
      aFrame->mKeyFrame = keyFrame;
      aFrame->Y.mWidth = mVideoWidth;
      aFrame->Y.mHeight = mVideoHeight;
    } else {
      char *data = static_cast<char *>(mVideoBuffer->data()) + mVideoBuffer->range_offset();
      size_t length = mVideoBuffer->range_length();

      if (unreadable) {
        LOG(PR_LOG_DEBUG, "video frame is unreadable");
      }

      if (!ToVideoFrame(aFrame, timeUs, data, length, keyFrame)) {
        return false;
      }
    }

    if (aKeyframeSkip && timeUs < aTimeUs) {
      aFrame->mShouldSkip = true;
    }

  }
  else if (err == INFO_FORMAT_CHANGED) {
    // If the format changed, update our cached info.
    if (!SetVideoFormat()) {
      return false;
    } else {
      return ReadVideo(aFrame, aTimeUs, aKeyframeSkip, aDoSeek);
    }
  }
  else if (err == ERROR_END_OF_STREAM) {
    return false;
  }
  else if (err == UNKNOWN_ERROR) {
    // This sometimes is used to mean "out of memory", but regardless,
    // don't keep trying to decode if the decoder doesn't want to.
    return false;
  }

  return true;
}

bool OmxDecoder::ReadAudio(AudioFrame *aFrame, int64_t aSeekTimeUs)
{
  status_t err;

  if (mAudioMetadataRead && aSeekTimeUs == -1) {
    // Use the data read into the buffer during metadata time
    err = OK;
  }
  else {
    ReleaseAudioBuffer();
    if (aSeekTimeUs != -1) {
      MediaSource::ReadOptions options;
      options.setSeekTo(aSeekTimeUs);
      err = mAudioSource->read(&mAudioBuffer, &options);
    } else {
      err = mAudioSource->read(&mAudioBuffer);
    }
  }
  mAudioMetadataRead = false;

  aSeekTimeUs = -1;

  if (err == OK && mAudioBuffer && mAudioBuffer->range_length() != 0) {
    int64_t timeUs;
    if (!mAudioBuffer->meta_data()->findInt64(kKeyTime, &timeUs))
      return false;

    return ToAudioFrame(aFrame, timeUs,
                        mAudioBuffer->data(),
                        mAudioBuffer->range_offset(),
                        mAudioBuffer->range_length(),
                        mAudioChannels, mAudioSampleRate);
  }
  else if (err == INFO_FORMAT_CHANGED) {
    // If the format changed, update our cached info.
    if (!SetAudioFormat()) {
      return false;
    } else {
      return ReadAudio(aFrame, aSeekTimeUs);
    }
  }
  else if (err == ERROR_END_OF_STREAM) {
    if (aFrame->mSize == 0) {
      return false;
    }
  }
  else if (err == UNKNOWN_ERROR) {
    return false;
  }

  return true;
}

bool OmxDecoder::ReleaseVideoBuffer(MediaBuffer *aBuffer)
{
  Mutex::Autolock autoLock(mSeekLock);

  if (!aBuffer) {
    return false;
  }

  if (mIsVideoSeeking == true) {
    mPendingVideoBuffers.push(aBuffer);
  } else {
    aBuffer->release();
  }
  return true;
}

void OmxDecoder::ReleaseAllPendingVideoBuffersLocked()
{
  int size = mPendingVideoBuffers.size();
  for (int i = 0; i < size; i++) {
    MediaBuffer *buffer = mPendingVideoBuffers[i];
    buffer->release();
  }
  mPendingVideoBuffers.clear();
}

nsresult OmxDecoder::Play() {
  if (!mPaused) {
    return NS_OK;
  }
  if (mVideoSource.get() && mVideoSource->start() != OK) {
    return NS_ERROR_UNEXPECTED;
  }

  if (mAudioSource.get()&& mAudioSource->start() != OK) {
    return NS_ERROR_UNEXPECTED;
  }
  mPaused = false;
  return NS_OK;
}

void OmxDecoder::Pause() {
  if (mPaused) {
    return;
  }
  if (mVideoSource.get()) {
    mVideoSource->pause();
  }

  if (mAudioSource.get()) {
    mAudioSource->pause();
  }
  mPaused = true;
}

