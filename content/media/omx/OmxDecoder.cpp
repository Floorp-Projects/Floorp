/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <unistd.h>
#include <fcntl.h>

#include "base/basictypes.h"
#include <cutils/properties.h>
#include <stagefright/foundation/ADebug.h>
#include <stagefright/foundation/AMessage.h>
#include <stagefright/MediaExtractor.h>
#include <stagefright/MetaData.h>
#include <stagefright/OMXClient.h>
#include <stagefright/OMXCodec.h>
#include <OMX.h>

#include "mozilla/Preferences.h"
#include "mozilla/Types.h"
#include "mozilla/Monitor.h"
#include "nsMimeTypes.h"
#include "MPAPI.h"
#include "prlog.h"

#include "GonkNativeWindow.h"
#include "GonkNativeWindowClient.h"
#include "OMXCodecProxy.h"
#include "OmxDecoder.h"
#include "nsISeekableStream.h"

#ifdef PR_LOGGING
PRLogModuleInfo *gOmxDecoderLog;
#define LOG(type, msg...) PR_LOG(gOmxDecoderLog, type, (msg))
#else
#define LOG(x...)
#endif

using namespace MPAPI;
using namespace mozilla;

namespace mozilla {

class OmxDecoderProcessCachedDataTask : public Task
{
public:
  OmxDecoderProcessCachedDataTask(android::OmxDecoder* aOmxDecoder, int64_t aOffset)
  : mOmxDecoder(aOmxDecoder),
    mOffset(aOffset)
  { }

  void Run()
  {
    MOZ_ASSERT(!NS_IsMainThread());
    MOZ_ASSERT(mOmxDecoder.get());
    mOmxDecoder->ProcessCachedData(mOffset, false);
  }

private:
  android::sp<android::OmxDecoder> mOmxDecoder;
  int64_t                          mOffset;
};

// When loading an MP3 stream from a file, we need to parse the file's
// content to find its duration. Reading files of 100 Mib or more can
// delay the player app noticably, so the file os read and decoded in
// smaller chunks.
//
// We first read on the decode thread, but parsing must be done on the
// main thread. After we read the file's initial MiBs in the decode
// thread, an instance of this class is scheduled to the main thread for
// parsing the MP3 stream. The decode thread waits until it has finished.
//
// If there is more data available from the file, the runnable dispatches
// a task to the IO thread for retrieving the next chunk of data, and
// the IO task dispatches a runnable to the main thread for parsing the
// data. This goes on until all of the MP3 file has been parsed.

class OmxDecoderNotifyDataArrivedRunnable : public nsRunnable
{
public:
  OmxDecoderNotifyDataArrivedRunnable(android::OmxDecoder* aOmxDecoder, const char* aBuffer, uint64_t aLength, int64_t aOffset, uint64_t aFullLength)
  : mOmxDecoder(aOmxDecoder),
    mBuffer(aBuffer),
    mLength(aLength),
    mOffset(aOffset),
    mFullLength(aFullLength),
    mCompletedMonitor("OmxDecoderNotifyDataArrived.mCompleted"),
    mCompleted(false)
  {
    MOZ_ASSERT(mOmxDecoder.get());
    MOZ_ASSERT(mBuffer.get() || !mLength);
  }

  NS_IMETHOD Run()
  {
    NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");

    const char* buffer = mBuffer.get();

    while (mLength) {
      uint32_t length = std::min<uint64_t>(mLength, UINT32_MAX);
      mOmxDecoder->NotifyDataArrived(mBuffer.get(), mLength, mOffset);

      buffer  += length;
      mLength -= length;
      mOffset += length;
    }

    if (mOffset < mFullLength) {
      // We cannot read data in the main thread because it
      // might block for too long. Instead we post an IO task
      // to the IO thread if there is more data available.
      XRE_GetIOMessageLoop()->PostTask(FROM_HERE, new OmxDecoderProcessCachedDataTask(mOmxDecoder.get(), mOffset));
    }

    Completed();

    return NS_OK;
  }

  void WaitForCompletion()
  {
    MOZ_ASSERT(!NS_IsMainThread());

    MonitorAutoLock mon(mCompletedMonitor);
    if (!mCompleted) {
      mCompletedMonitor.Wait();
    }
  }

private:
  // Call this function at the end of Run() to notify waiting
  // threads.
  void Completed()
  {
    MonitorAutoLock mon(mCompletedMonitor);
    MOZ_ASSERT(!mCompleted);
    mCompleted = true;
    mCompletedMonitor.Notify();
  }

  android::sp<android::OmxDecoder> mOmxDecoder;
  nsAutoArrayPtr<const char>       mBuffer;
  uint64_t                         mLength;
  int64_t                          mOffset;
  uint64_t                         mFullLength;

  Monitor mCompletedMonitor;
  bool    mCompleted;
};

namespace layers {

VideoGraphicBuffer::VideoGraphicBuffer(const android::wp<android::OmxDecoder> aOmxDecoder,
                                       android::MediaBuffer *aBuffer,
                                       SurfaceDescriptor& aDescriptor)
  : GraphicBufferLocked(aDescriptor),
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
    // Post kNotifyPostReleaseVideoBuffer message to OmxDecoder via ALooper.
    // The message is delivered to OmxDecoder on ALooper thread.
    // MediaBuffer::release() could take a very long time.
    // PostReleaseVideoBuffer() prevents long time locking.
    omxDecoder->PostReleaseVideoBuffer(mMediaBuffer);
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
    Mutex::Autolock autoLock(mLock);
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
  mMP3FrameParser(aResource->GetLength()),
  mVideoBuffer(nullptr),
  mAudioBuffer(nullptr),
  mIsVideoSeeking(false),
  mAudioMetadataRead(false),
  mAudioPaused(false),
  mVideoPaused(false)
{
  mLooper = new ALooper;
  mLooper->setName("OmxDecoder");

  mReflector = new AHandlerReflector<OmxDecoder>(this);
  // Register AMessage handler to ALooper.
  mLooper->registerHandler(mReflector);
  // Start ALooper thread.
  mLooper->start();
}

OmxDecoder::~OmxDecoder()
{
  ReleaseMediaResources();

  // unregister AMessage handler from ALooper.
  mLooper->unregisterHandler(mReflector->id());
  // Stop ALooper thread.
  mLooper->stop();
}

void OmxDecoder::statusChanged()
{
  sp<AMessage> notify =
           new AMessage(kNotifyStatusChanged, mReflector->id());
 // post AMessage to OmxDecoder via ALooper.
 notify->post();
}

static sp<IOMX> sOMX = nullptr;
static sp<IOMX> GetOMX()
{
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
    }
  }

  if (videoTrackIndex == -1 && audioTrackIndex == -1) {
    NS_WARNING("OMX decoder could not find video or audio tracks");
    return false;
  }

  mResource->SetReadMode(MediaCacheStream::MODE_PLAYBACK);

  if (videoTrackIndex != -1) {
    mVideoTrack = extractor->getTrack(videoTrackIndex);
  }

  if (audioTrackIndex != -1) {
    mAudioTrack = extractor->getTrack(audioTrackIndex);
  }
  return true;
}

bool OmxDecoder::TryLoad() {

  if (!AllocateMediaResources()) {
    return false;
  }

  //check if video is waiting resources
  if (mVideoSource.get()) {
    if (mVideoSource->IsWaitingResources()) {
      return true;
    }
  }

  // calculate duration
  int64_t totalDurationUs = 0;
  int64_t durationUs = 0;
  if (mVideoTrack.get() && mVideoTrack->getFormat()->findInt64(kKeyDuration, &durationUs)) {
    if (durationUs > totalDurationUs)
      totalDurationUs = durationUs;
  }
  if (mAudioTrack.get()) {
    durationUs = -1;
    const char* audioMime;
    sp<MetaData> meta = mAudioTrack->getFormat();

    if (meta->findCString(kKeyMIMEType, &audioMime) && !strcasecmp(audioMime, AUDIO_MP3)) {
      // Feed MP3 parser with cached data. Local files will be fully
      // cached already, network streams will update with sucessive
      // calls to NotifyDataArrived.
      if (ProcessCachedData(0, true)) {
        durationUs = mMP3FrameParser.GetDuration();
        if (durationUs > totalDurationUs) {
          totalDurationUs = durationUs;
        }
      }
    }
    if ((durationUs == -1) && meta->findInt64(kKeyDuration, &durationUs)) {
      if (durationUs > totalDurationUs) {
        totalDurationUs = durationUs;
      }
    }
  }
  mDurationUs = totalDurationUs;

  // read video metadata
  if (mVideoSource.get() && !SetVideoFormat()) {
    NS_WARNING("Couldn't set OMX video format");
    return false;
  }

  // read audio metadata
  if (mAudioSource.get()) {
    // To reliably get the channel and sample rate data we need to read from the
    // audio source until we get a INFO_FORMAT_CHANGE status
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

bool OmxDecoder::IsDormantNeeded()
{
  if (mVideoTrack.get()) {
    return true;
  }
  return false;
}

bool OmxDecoder::IsWaitingMediaResources()
{
  if (mVideoSource.get()) {
    return mVideoSource->IsWaitingResources();
  }
  return false;
}

bool OmxDecoder::AllocateMediaResources()
{
  // OMXClient::connect() always returns OK and abort's fatally if
  // it can't connect.
  OMXClient client;
  DebugOnly<status_t> err = client.connect();
  NS_ASSERTION(err == OK, "Failed to connect to OMX in mediaserver.");
  sp<IOMX> omx = client.interface();

  if ((mVideoTrack != nullptr) && (mVideoSource == nullptr)) {
    mNativeWindow = new GonkNativeWindow();
#if defined(MOZ_WIDGET_GONK) && ANDROID_VERSION >= 18
    mNativeWindowClient = new GonkNativeWindowClient(mNativeWindow->getBufferQueue());
#else
    mNativeWindowClient = new GonkNativeWindowClient(mNativeWindow);
#endif

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
    mVideoSource =
          OMXCodecProxy::Create(omx,
                                mVideoTrack->getFormat(),
                                false, // decoder
                                mVideoTrack,
                                nullptr,
                                flags,
                                mNativeWindowClient);
    if (mVideoSource == nullptr) {
      NS_WARNING("Couldn't create OMX video source");
      return false;
    } else {
      sp<OMXCodecProxy::EventListener> listener = this;
      mVideoSource->setEventListener(listener);
      mVideoSource->requestResource();
    }
  }

  if ((mAudioTrack != nullptr) && (mAudioSource == nullptr)) {
    const char *audioMime = nullptr;
    sp<MetaData> meta = mAudioTrack->getFormat();
    if (!meta->findCString(kKeyMIMEType, &audioMime)) {
      return false;
    }
    if (!strcasecmp(audioMime, "audio/raw")) {
      mAudioSource = mAudioTrack;
    } else {
      // try to load hardware codec in mediaserver process.
      int flags = kHardwareCodecsOnly;
      mAudioSource = OMXCodec::Create(omx,
                                     mAudioTrack->getFormat(),
                                     false, // decoder
                                     mAudioTrack,
                                     nullptr,
                                     flags);
    }

    if (mAudioSource == nullptr) {
      // try to load software codec in this process.
      int flags = kSoftwareCodecsOnly;
      mAudioSource = OMXCodec::Create(GetOMX(),
                                     mAudioTrack->getFormat(),
                                     false, // decoder
                                     mAudioTrack,
                                     nullptr,
                                     flags);
      if (mAudioSource == nullptr) {
        NS_WARNING("Couldn't create OMX audio source");
        return false;
      }
    }
    if (mAudioSource->start() != OK) {
      NS_WARNING("Couldn't start OMX audio source");
      mAudioSource.clear();
      return false;
    }
  }
  return true;
}


void OmxDecoder::ReleaseMediaResources() {
  {
    // Free all pending video buffers.
    Mutex::Autolock autoLock(mSeekLock);
    ReleaseAllPendingVideoBuffersLocked();
  }

  ReleaseVideoBuffer();
  ReleaseAudioBuffer();

  if (mVideoSource.get()) {
    mVideoSource->stop();
    mVideoSource.clear();
  }

  if (mAudioSource.get()) {
    mAudioSource->stop();
    mAudioSource.clear();
  }

  mNativeWindowClient.clear();
  mNativeWindow.clear();
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

void OmxDecoder::NotifyDataArrived(const char* aBuffer, uint32_t aLength, int64_t aOffset)
{
  if (!mMP3FrameParser.IsMP3()) {
    return;
  }

  mMP3FrameParser.Parse(aBuffer, aLength, aOffset);

  int64_t durationUs = mMP3FrameParser.GetDuration();

  if (durationUs != mDurationUs) {
    mDurationUs = durationUs;

    MOZ_ASSERT(mDecoder);
    ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
    mDecoder->UpdateEstimatedMediaDuration(mDurationUs);
  }
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

    aDoSeek = false;
  } else {
    err = mVideoSource->read(&mVideoBuffer);
  }

  aFrame->mSize = 0;

  if (err == OK) {
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
      mozilla::layers::SurfaceDescriptorGralloc newDescriptor = descriptor->get_SurfaceDescriptorGralloc();
      newDescriptor.size() = nsIntSize(mVideoWidth, mVideoHeight);

      mozilla::layers::SurfaceDescriptor descWrapper(newDescriptor);
      aFrame->mGraphicBuffer = new mozilla::layers::VideoGraphicBuffer(this, mVideoBuffer, descWrapper);
      aFrame->mRotation = mVideoRotation;
      aFrame->mTimeUs = timeUs;
      aFrame->mKeyFrame = keyFrame;
      aFrame->Y.mWidth = mVideoWidth;
      aFrame->Y.mHeight = mVideoHeight;
    } else if (mVideoBuffer->range_length() > 0) {
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
  else if (err == -ETIMEDOUT) {
    LOG(PR_LOG_DEBUG, "OmxDecoder::ReadVideo timed out, will retry");
    return true;
  }
  else {
    // UNKNOWN_ERROR is sometimes is used to mean "out of memory", but
    // regardless, don't keep trying to decode if the decoder doesn't want to.
    LOG(PR_LOG_DEBUG, "OmxDecoder::ReadVideo failed, err=%d", err);
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
  aFrame->mSize = 0;

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
  else if (err == -ETIMEDOUT) {
    LOG(PR_LOG_DEBUG, "OmxDecoder::ReadAudio timed out, will retry");
    return true;
  }
  else if (err != OK) {
    LOG(PR_LOG_DEBUG, "OmxDecoder::ReadAudio failed, err=%d", err);
    return false;
  }

  return true;
}

nsresult OmxDecoder::Play()
{
  if (!mVideoPaused && !mAudioPaused) {
    return NS_OK;
  }

  if (mVideoPaused && mVideoSource.get() && mVideoSource->start() != OK) {
    return NS_ERROR_UNEXPECTED;
  }
  mVideoPaused = false;

  if (mAudioPaused && mAudioSource.get() && mAudioSource->start() != OK) {
    return NS_ERROR_UNEXPECTED;
  }
  mAudioPaused = false;

  return NS_OK;
}

// AOSP didn't give implementation on OMXCodec::Pause() and not define
// OMXCodec::Start() should be called for resuming the decoding. Currently
// it is customized by a specific open source repository only.
// ToDo The one not supported OMXCodec::Pause() should return error code here,
// so OMXCodec::Start() doesn't be called again for resuming. But if someone
// implement the OMXCodec::Pause() and need a following OMXCodec::Read() with
// seek option (define in MediaSource.h) then it is still not supported here.
// We need to fix it until it is really happened.
void OmxDecoder::Pause()
{
  if (mVideoPaused || mAudioPaused) {
    return;
  }

  if (mVideoSource.get() && mVideoSource->pause() == OK) {
    mVideoPaused = true;
  }

  if (mAudioSource.get() && mAudioSource->pause() == OK) {
    mAudioPaused = true;
  }
}

// Called on ALooper thread.
void OmxDecoder::onMessageReceived(const sp<AMessage> &msg)
{
  switch (msg->what()) {
    case kNotifyPostReleaseVideoBuffer:
    {
      Mutex::Autolock autoLock(mSeekLock);
      // Free pending video buffers when OmxDecoder is not seeking video.
      // If OmxDecoder is seeking video, the buffers are freed on seek exit.
      if (!mIsVideoSeeking) {
        ReleaseAllPendingVideoBuffersLocked();
      }
      break;
    }

    case kNotifyStatusChanged:
    {
      mozilla::ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
      mDecoder->GetReentrantMonitor().NotifyAll();
      break;
    }

    default:
      TRESPASS();
      break;
  }
}

void OmxDecoder::PostReleaseVideoBuffer(MediaBuffer *aBuffer)
{
  {
    Mutex::Autolock autoLock(mPendingVideoBuffersLock);
    mPendingVideoBuffers.push(aBuffer);
  }

  sp<AMessage> notify =
            new AMessage(kNotifyPostReleaseVideoBuffer, mReflector->id());
  // post AMessage to OmxDecoder via ALooper.
  notify->post();
}

void OmxDecoder::ReleaseAllPendingVideoBuffersLocked()
{
  Vector<MediaBuffer *> releasingVideoBuffers;
  {
    Mutex::Autolock autoLock(mPendingVideoBuffersLock);

    int size = mPendingVideoBuffers.size();
    for (int i = 0; i < size; i++) {
      MediaBuffer *buffer = mPendingVideoBuffers[i];
      releasingVideoBuffers.push(buffer);
    }
    mPendingVideoBuffers.clear();
  }
  // Free all pending video buffers without holding mPendingVideoBuffersLock.
  int size = releasingVideoBuffers.size();
  for (int i = 0; i < size; i++) {
    MediaBuffer *buffer;
    buffer = releasingVideoBuffers[i];
    buffer->release();
  }
  releasingVideoBuffers.clear();
}

bool OmxDecoder::ProcessCachedData(int64_t aOffset, bool aWaitForCompletion)
{
  // We read data in chunks of 32 KiB. We can reduce this
  // value if media, such as sdcards, is too slow.
  // Because of SD card's slowness, need to keep sReadSize to small size.
  // See Bug 914870.
  static const int64_t sReadSize = 32 * 1024;

  NS_ASSERTION(!NS_IsMainThread(), "Should not be on main thread.");

  MOZ_ASSERT(mResource);

  int64_t resourceLength = mResource->GetCachedDataEnd(0);
  NS_ENSURE_TRUE(resourceLength >= 0, false);

  if (aOffset >= resourceLength) {
    return true; // Cache is empty, nothing to do
  }

  int64_t bufferLength = std::min<int64_t>(resourceLength-aOffset, sReadSize);

  nsAutoArrayPtr<char> buffer(new char[bufferLength]);

  nsresult rv = mResource->ReadFromCache(buffer.get(), aOffset, bufferLength);
  NS_ENSURE_SUCCESS(rv, false);

  nsRefPtr<OmxDecoderNotifyDataArrivedRunnable> runnable(
    new OmxDecoderNotifyDataArrivedRunnable(this,
                                            buffer.forget(),
                                            bufferLength,
                                            aOffset,
                                            resourceLength));

  rv = NS_DispatchToMainThread(runnable.get());
  NS_ENSURE_SUCCESS(rv, false);

  if (aWaitForCompletion) {
    runnable->WaitForCompletion();
  }

  return true;
}
