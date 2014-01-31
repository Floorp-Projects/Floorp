/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "OMXCodecWrapper.h"
#include "OMXCodecDescriptorUtil.h"
#include "TrackEncoder.h"

#include <binder/ProcessState.h>
#include <media/ICrypto.h>
#include <media/IOMX.h>
#include <OMX_Component.h>
#include <stagefright/MediaDefs.h>
#include <stagefright/MediaErrors.h>

#include "AudioChannelFormat.h"
#include <mozilla/Monitor.h>

using namespace mozilla;
using namespace mozilla::gfx;
using namespace mozilla::layers;

#define ENCODER_CONFIG_BITRATE 2000000 // bps
// How many seconds between I-frames.
#define ENCODER_CONFIG_I_FRAME_INTERVAL 1
// Wait up to 5ms for input buffers.
#define INPUT_BUFFER_TIMEOUT_US (5 * 1000ll)

#define CODEC_ERROR(args...)                                                   \
  do {                                                                         \
    __android_log_print(ANDROID_LOG_ERROR, "OMXCodecWrapper", ##args);         \
  } while (0)

namespace android {

OMXAudioEncoder*
OMXCodecWrapper::CreateAACEncoder()
{
  nsAutoPtr<OMXAudioEncoder> aac(new OMXAudioEncoder(CodecType::AAC_ENC));
  // Return the object only when media codec is valid.
  NS_ENSURE_TRUE(aac->IsValid(), nullptr);

  return aac.forget();
}

OMXVideoEncoder*
OMXCodecWrapper::CreateAVCEncoder()
{
  nsAutoPtr<OMXVideoEncoder> avc(new OMXVideoEncoder(CodecType::AVC_ENC));
  // Return the object only when media codec is valid.
  NS_ENSURE_TRUE(avc->IsValid(), nullptr);

  return avc.forget();
}

OMXCodecWrapper::OMXCodecWrapper(CodecType aCodecType)
  : mStarted(false)
{
  ProcessState::self()->startThreadPool();

  mLooper = new ALooper();
  mLooper->start();

  if (aCodecType == CodecType::AVC_ENC) {
    mCodec = MediaCodec::CreateByType(mLooper, MEDIA_MIMETYPE_VIDEO_AVC, true);
  } else if (aCodecType == CodecType::AAC_ENC) {
    mCodec = MediaCodec::CreateByType(mLooper, MEDIA_MIMETYPE_AUDIO_AAC, true);
  } else {
    NS_ERROR("Unknown codec type.");
  }
}

OMXCodecWrapper::~OMXCodecWrapper()
{
  if (mCodec.get()) {
    Stop();
    mCodec->release();
  }
  mLooper->stop();
}

status_t
OMXCodecWrapper::Start()
{
  // Already started.
  NS_ENSURE_FALSE(mStarted, OK);

  status_t result = mCodec->start();
  mStarted = (result == OK);

  // Get references to MediaCodec buffers.
  if (result == OK) {
    mCodec->getInputBuffers(&mInputBufs);
    mCodec->getOutputBuffers(&mOutputBufs);
  }

  return result;
}

status_t
OMXCodecWrapper::Stop()
{
  // Already stopped.
  NS_ENSURE_TRUE(mStarted, OK);

  status_t result = mCodec->stop();
  mStarted = !(result == OK);

  return result;
}

nsresult
OMXVideoEncoder::Configure(int aWidth, int aHeight, int aFrameRate)
{
  MOZ_ASSERT(!mStarted, "Configure() was called already.");

  NS_ENSURE_TRUE(aWidth > 0 && aHeight > 0 && aFrameRate > 0,
                 NS_ERROR_INVALID_ARG);

  // Set up configuration parameters for AVC/H.264 encoder.
  sp<AMessage> format = new AMessage;
  // Fixed values
  format->setString("mime", MEDIA_MIMETYPE_VIDEO_AVC);
  format->setInt32("bitrate", ENCODER_CONFIG_BITRATE);
  format->setInt32("i-frame-interval", ENCODER_CONFIG_I_FRAME_INTERVAL);
  // See mozilla::layers::GrallocImage, supports YUV 4:2:0, CbCr width and
  // height is half that of Y
  format->setInt32("color-format", OMX_COLOR_FormatYUV420SemiPlanar);
  format->setInt32("profile", OMX_VIDEO_AVCProfileBaseline);
  format->setInt32("level", OMX_VIDEO_AVCLevel3);
  format->setInt32("bitrate-mode", OMX_Video_ControlRateConstant);
  format->setInt32("store-metadata-in-buffers", 0);
  format->setInt32("prepend-sps-pps-to-idr-frames", 0);
  // Input values.
  format->setInt32("width", aWidth);
  format->setInt32("height", aHeight);
  format->setInt32("stride", aWidth);
  format->setInt32("slice-height", aHeight);
  format->setInt32("frame-rate", aFrameRate);

  status_t result = mCodec->configure(format, nullptr, nullptr,
                                      MediaCodec::CONFIGURE_FLAG_ENCODE);
  NS_ENSURE_TRUE(result == OK, NS_ERROR_FAILURE);

  mWidth = aWidth;
  mHeight = aHeight;

  result = Start();

  return result == OK ? NS_OK : NS_ERROR_FAILURE;
}

// Copy pixels from planar YUV (4:4:4/4:2:2/4:2:0) or NV21 (semi-planar 4:2:0)
// format to NV12 (semi-planar 4:2:0) format for QCOM HW encoder.
// Planar YUV:  YYY...UUU...VVV...
// NV21:        YYY...VUVU...
// NV12:        YYY...UVUV...
// For 4:4:4/4:2:2 -> 4:2:0, subsample using odd row/column without
// interpolation.
// aSource contains info about source image data, and the result will be stored
// in aDestination, whose size needs to be >= Y plane size * 3 / 2.
static
void
ConvertPlanarYCbCrToNV12(const PlanarYCbCrData* aSource, uint8_t* aDestination)
{
  // Fill Y plane.
  uint8_t* y = aSource->mYChannel;
  IntSize ySize = aSource->mYSize;

  // Y plane.
  for (int i = 0; i < ySize.height; i++) {
    memcpy(aDestination, y, ySize.width);
    aDestination += ySize.width;
    y += aSource->mYStride;
  }

  // Fill interleaved UV plane.
  uint8_t* u = aSource->mCbChannel;
  uint8_t* v = aSource->mCrChannel;
  IntSize uvSize = aSource->mCbCrSize;
  // Subsample to 4:2:0 if source is 4:4:4 or 4:2:2.
  // Y plane width & height should be multiple of U/V plane width & height.
  MOZ_ASSERT(ySize.width % uvSize.width == 0 &&
             ySize.height % uvSize.height == 0);
  size_t uvWidth = ySize.width / 2;
  size_t uvHeight = ySize.height / 2;
  size_t horiSubsample = uvSize.width / uvWidth;
  size_t uPixStride = horiSubsample * (1 + aSource->mCbSkip);
  size_t vPixStride = horiSubsample * (1 + aSource->mCrSkip);
  size_t lineStride = uvSize.height / uvHeight * aSource->mCbCrStride;

  for (int i = 0; i < uvHeight; i++) {
    // 1st pixel per line.
    uint8_t* uSrc = u;
    uint8_t* vSrc = v;
    for (int j = 0; j < uvWidth; j++) {
      *aDestination++ = *uSrc;
      *aDestination++ = *vSrc;
      // Pick next source pixel.
      uSrc += uPixStride;
      vSrc += vPixStride;
    }
    // Pick next source line.
    u += lineStride;
    v += lineStride;
  }
}

nsresult
OMXVideoEncoder::Encode(const Image* aImage, int aWidth, int aHeight,
                        int64_t aTimestamp, int aInputFlags)
{
  MOZ_ASSERT(mStarted, "Configure() should be called before Encode().");

  NS_ENSURE_TRUE(aWidth == mWidth && aHeight == mHeight && aTimestamp >= 0,
                 NS_ERROR_INVALID_ARG);

  status_t result;

  // Dequeue an input buffer.
  uint32_t index;
  result = mCodec->dequeueInputBuffer(&index, INPUT_BUFFER_TIMEOUT_US);
  NS_ENSURE_TRUE(result == OK, NS_ERROR_FAILURE);

  const sp<ABuffer>& inBuf = mInputBufs.itemAt(index);
  uint8_t* dst = inBuf->data();
  size_t dstSize = inBuf->capacity();

  size_t yLen = aWidth * aHeight;
  size_t uvLen = yLen / 2;

  // Buffer should be large enough to hold input image data.
  MOZ_ASSERT(dstSize >= yLen + uvLen);

  inBuf->setRange(0, yLen + uvLen);

  if (!aImage) {
    // Generate muted/black image directly in buffer.
    dstSize = yLen + uvLen;
    // Fill Y plane.
    memset(dst, 0x10, yLen);
    // Fill UV plane.
    memset(dst + yLen, 0x80, uvLen);
  } else {
    Image* img = const_cast<Image*>(aImage);
    ImageFormat format = img->GetFormat();

    MOZ_ASSERT(aWidth == img->GetSize().width &&
               aHeight == img->GetSize().height);

    if (format == ImageFormat::GRALLOC_PLANAR_YCBCR) {
      // Get graphic buffer pointer.
      void* imgPtr = nullptr;
      GrallocImage* nativeImage = static_cast<GrallocImage*>(img);
      SurfaceDescriptor handle = nativeImage->GetSurfaceDescriptor();
      SurfaceDescriptorGralloc gralloc = handle.get_SurfaceDescriptorGralloc();
      sp<GraphicBuffer> graphicBuffer = GrallocBufferActor::GetFrom(gralloc);
      graphicBuffer->lock(GraphicBuffer::USAGE_SW_READ_MASK, &imgPtr);
      uint8_t* src = static_cast<uint8_t*>(imgPtr);

      // Only support NV21 for now.
      MOZ_ASSERT(graphicBuffer->getPixelFormat() ==
                 HAL_PIXEL_FORMAT_YCrCb_420_SP);

      // Build PlanarYCbCrData for NV21 buffer.
      PlanarYCbCrData nv21;
      // Y plane.
      nv21.mYChannel = src;
      nv21.mYSize.width = aWidth;
      nv21.mYSize.height = aHeight;
      nv21.mYStride = aWidth;
      nv21.mYSkip = 0;
      // Interleaved VU plane.
      nv21.mCrChannel = src + yLen;
      nv21.mCrSkip = 1;
      nv21.mCbChannel = nv21.mCrChannel + 1;
      nv21.mCbSkip = 1;
      nv21.mCbCrStride = aWidth;
      // 4:2:0.
      nv21.mCbCrSize.width = aWidth / 2;
      nv21.mCbCrSize.height = aHeight / 2;

      ConvertPlanarYCbCrToNV12(&nv21, dst);

      graphicBuffer->unlock();
    } else if (format == ImageFormat::PLANAR_YCBCR) {
      ConvertPlanarYCbCrToNV12(static_cast<PlanarYCbCrImage*>(img)->GetData(),
                             dst);
    } else {
      // TODO: support RGB to YUV color conversion.
      NS_ERROR("Unsupported input image type.");
    }
  }

  // Queue this input buffer.
  result = mCodec->queueInputBuffer(index, 0, dstSize, aTimestamp, aInputFlags);

  return result == OK ? NS_OK : NS_ERROR_FAILURE;
}

status_t
OMXVideoEncoder::AppendDecoderConfig(nsTArray<uint8_t>* aOutputBuf,
                                     ABuffer* aData)
{
  // AVC/H.264 decoder config descriptor is needed to construct MP4 'avcC' box
  // (defined in ISO/IEC 14496-15 5.2.4.1.1).
  return GenerateAVCDescriptorBlob(aData, aOutputBuf);
}

// Override to replace NAL unit start code with 4-bytes unit length.
// See ISO/IEC 14496-15 5.2.3.
void OMXVideoEncoder::AppendFrame(nsTArray<uint8_t>* aOutputBuf,
                                  const uint8_t* aData, size_t aSize)
{
  uint8_t length[] = {
    (aSize >> 24) & 0xFF,
    (aSize >> 16) & 0xFF,
    (aSize >> 8) & 0xFF,
    aSize & 0xFF,
  };
  aOutputBuf->SetCapacity(aSize);
  aOutputBuf->AppendElements(length, sizeof(length));
  aOutputBuf->AppendElements(aData + sizeof(length), aSize);
}

nsresult
OMXAudioEncoder::Configure(int aChannels, int aSamplingRate)
{
  MOZ_ASSERT(!mStarted);

  NS_ENSURE_TRUE(aChannels > 0 && aSamplingRate > 0, NS_ERROR_INVALID_ARG);

  // Set up configuration parameters for AAC encoder.
  sp<AMessage> format = new AMessage;
  // Fixed values.
  format->setString("mime", MEDIA_MIMETYPE_AUDIO_AAC);
  format->setInt32("bitrate", kAACBitrate);
  format->setInt32("aac-profile", OMX_AUDIO_AACObjectLC);
  // Input values.
  format->setInt32("channel-count", aChannels);
  format->setInt32("sample-rate", aSamplingRate);

  status_t result = mCodec->configure(format, nullptr, nullptr,
                                      MediaCodec::CONFIGURE_FLAG_ENCODE);
  NS_ENSURE_TRUE(result == OK, NS_ERROR_FAILURE);

  mChannels = aChannels;
  mSampleDuration = 1000000 / aSamplingRate;
  result = Start();

  return result == OK ? NS_OK : NS_ERROR_FAILURE;
}

class InputBufferHelper MOZ_FINAL {
public:
  InputBufferHelper(sp<MediaCodec>& aCodec, Vector<sp<ABuffer> >& aBuffers)
    : mCodec(aCodec)
    , mBuffers(aBuffers)
    , mIndex(0)
    , mData(nullptr)
    , mOffset(0)
    , mCapicity(0)
  {}

  ~InputBufferHelper()
  {
    // Unflushed data in buffer.
    MOZ_ASSERT(!mData);
  }

  status_t Dequeue()
  {
    // Shouldn't have dequeued buffer.
    MOZ_ASSERT(!mData);

    status_t result = mCodec->dequeueInputBuffer(&mIndex,
                                                 INPUT_BUFFER_TIMEOUT_US);
    NS_ENSURE_TRUE(result == OK, result);
    sp<ABuffer> inBuf = mBuffers.itemAt(mIndex);
    mData = inBuf->data();
    mCapicity = inBuf->capacity();
    mOffset = 0;

    return OK;
  }

  uint8_t* GetPointer() { return mData + mOffset; }

  const size_t AvailableSize() { return mCapicity - mOffset; }

  void IncreaseOffset(size_t aValue)
  {
    // Should never out of bound.
    MOZ_ASSERT(mOffset + aValue <= mCapicity);
    mOffset += aValue;
  }

  status_t Enqueue(int64_t aTimestamp, int aFlags)
  {
    // Should have dequeued buffer.
    MOZ_ASSERT(mData);

    // Queue this buffer.
    status_t result = mCodec->queueInputBuffer(mIndex, 0, mOffset, aTimestamp,
                                               aFlags);
    NS_ENSURE_TRUE(result == OK, result);
    mData = nullptr;

    return OK;
  }

private:
  sp<MediaCodec>& mCodec;
  Vector<sp<ABuffer> >& mBuffers;
  size_t mIndex;
  uint8_t* mData;
  size_t mCapicity;
  size_t mOffset;
};

nsresult
OMXAudioEncoder::Encode(AudioSegment& aSegment, int aInputFlags)
{
#ifndef MOZ_SAMPLE_TYPE_S16
#error MediaCodec accepts only 16-bit PCM data.
#endif

  MOZ_ASSERT(mStarted, "Configure() should be called before Encode().");

  size_t numSamples = aSegment.GetDuration();

  // Get input buffer.
  InputBufferHelper buffer(mCodec, mInputBufs);
  status_t result = buffer.Dequeue();
  if (result == -EAGAIN) {
    // All input buffers are full. Caller can try again later after consuming
    // some output buffers.
    return NS_OK;
  }
  NS_ENSURE_TRUE(result == OK, NS_ERROR_FAILURE);

  size_t samplesCopied = 0; // Number of copied samples.

  if (numSamples > 0) {
    // Copy input PCM data to input buffer until queue is empty.
    AudioSegment::ChunkIterator iter(const_cast<AudioSegment&>(aSegment));
    while (!iter.IsEnded()) {
      AudioChunk chunk = *iter;
      size_t samplesToCopy = chunk.GetDuration(); // Number of samples to copy.
      size_t bytesToCopy = samplesToCopy * mChannels * sizeof(AudioDataValue);

      if (bytesToCopy > buffer.AvailableSize()) {
        // Not enough space left in input buffer. Send it to encoder and get a
        // new one.
        result = buffer.Enqueue(mTimestamp, aInputFlags & ~BUFFER_EOS);
        NS_ENSURE_TRUE(result == OK, NS_ERROR_FAILURE);

        result = buffer.Dequeue();
        if (result == -EAGAIN) {
          // All input buffers are full. Caller can try again later after
          // consuming some output buffers.
          aSegment.RemoveLeading(samplesCopied);
          return NS_OK;
        }

        mTimestamp += samplesCopied * mSampleDuration;
        samplesCopied = 0;

        NS_ENSURE_TRUE(result == OK, NS_ERROR_FAILURE);
      }

      AudioDataValue* dst = reinterpret_cast<AudioDataValue*>(buffer.GetPointer());
      if (!chunk.IsNull()) {
        // Append the interleaved data to input buffer.
        AudioTrackEncoder::InterleaveTrackData(chunk, samplesToCopy, mChannels,
                                               dst);
      } else {
        // Silence.
        memset(dst, 0, bytesToCopy);
      }

      samplesCopied += samplesToCopy;
      buffer.IncreaseOffset(bytesToCopy);
      iter.Next();
    }
    if (samplesCopied > 0) {
      aSegment.RemoveLeading(samplesCopied);
    }
  } else if (aInputFlags & BUFFER_EOS) {
    // No audio data left in segment but we still have to feed something to
    // MediaCodec in order to notify EOS.
    size_t bytesToCopy = mChannels * sizeof(AudioDataValue);
    memset(buffer.GetPointer(), 0, bytesToCopy);
    buffer.IncreaseOffset(bytesToCopy);
    samplesCopied = 1;
  }

  if (samplesCopied > 0) {
    int flags = aInputFlags;
    if (aSegment.GetDuration() > 0) {
      // Don't signal EOS until source segment is empty.
      flags &= ~BUFFER_EOS;
    }
    result = buffer.Enqueue(mTimestamp, flags);
    NS_ENSURE_TRUE(result == OK, NS_ERROR_FAILURE);

    mTimestamp += samplesCopied * mSampleDuration;
  }

  return NS_OK;
}

// Generate decoder config descriptor (defined in ISO/IEC 14496-1 8.3.4.1) for
// AAC. The hard-coded bytes are copied from
// MPEG4Writer::Track::writeMp4aEsdsBox() implementation in libstagefright.
status_t
OMXAudioEncoder::AppendDecoderConfig(nsTArray<uint8_t>* aOutputBuf,
                                     ABuffer* aData)
{
  MOZ_ASSERT(aData);

  const size_t csdSize = aData->size();

  // See
  // http://wiki.multimedia.cx/index.php?title=Understanding_AAC#Packaging.2FEncapsulation_And_Setup_Data
  // AAC decoder specific descriptor contains 2 bytes.
  NS_ENSURE_TRUE(csdSize == 2, ERROR_MALFORMED);
  // Encoder output must be consistent with kAACFrameDuration:
  // 14th bit (frame length flag) == 0 => 1024 (kAACFrameDuration) samples.
  NS_ENSURE_TRUE((aData->data()[1] & 0x04) == 0, ERROR_MALFORMED);

  // Decoder config descriptor
  const uint8_t decConfig[] = {
    0x04,                   // Decoder config descriptor tag.
    15 + csdSize,           // Size: following bytes + csd size.
    0x40,                   // Object type: MPEG-4 audio.
    0x15,                   // Stream type: audio, reserved: 1.
    0x00, 0x03, 0x00,       // Buffer size: 768 (kAACFrameSize).
    0x00, 0x01, 0x77, 0x00, // Max bitrate: 96000 (kAACBitrate).
    0x00, 0x01, 0x77, 0x00, // Avg bitrate: 96000 (kAACBitrate).
    0x05,                   // Decoder specific descriptor tag.
    csdSize,                // Data size.
  };
  // SL config descriptor.
  const uint8_t slConfig[] = {
    0x06, // SL config descriptor tag.
    0x01, // Size.
    0x02, // Fixed value.
  };

  aOutputBuf->SetCapacity(sizeof(decConfig) + csdSize + sizeof(slConfig));
  aOutputBuf->AppendElements(decConfig, sizeof(decConfig));
  aOutputBuf->AppendElements(aData->data(), csdSize);
  aOutputBuf->AppendElements(slConfig, sizeof(slConfig));

  return OK;
}

nsresult
OMXCodecWrapper::GetNextEncodedFrame(nsTArray<uint8_t>* aOutputBuf,
                                     int64_t* aOutputTimestamp,
                                     int* aOutputFlags, int64_t aTimeOut)
{
  MOZ_ASSERT(mStarted,
             "Configure() should be called before GetNextEncodedFrame().");

  // Dequeue a buffer from output buffers.
  size_t index = 0;
  size_t outOffset = 0;
  size_t outSize = 0;
  int64_t outTimeUs = 0;
  uint32_t outFlags = 0;
  bool retry = false;
  do {
    status_t result = mCodec->dequeueOutputBuffer(&index, &outOffset, &outSize,
                                                  &outTimeUs, &outFlags,
                                                  aTimeOut);
    switch (result) {
      case OK:
        break;
      case INFO_OUTPUT_BUFFERS_CHANGED:
        // Update our references to new buffers.
        result = mCodec->getOutputBuffers(&mOutputBufs);
        // Get output from a new buffer.
        retry = true;
        break;
      case INFO_FORMAT_CHANGED:
        // It's okay: for encoder, MediaCodec reports this only to inform caller
        // that there will be a codec config buffer next.
        return NS_OK;
      case -EAGAIN:
        // Output buffer not available. Caller can try again later.
        return NS_OK;
      default:
        CODEC_ERROR("MediaCodec error:%d", result);
        MOZ_ASSERT(false, "MediaCodec error.");
        return NS_ERROR_FAILURE;
    }
  } while (retry);

  if (aOutputBuf) {
    aOutputBuf->Clear();
    const sp<ABuffer> omxBuf = mOutputBufs.itemAt(index);
    if (outFlags & MediaCodec::BUFFER_FLAG_CODECCONFIG) {
      // Codec specific data.
      if (AppendDecoderConfig(aOutputBuf, omxBuf.get()) != OK) {
        mCodec->releaseOutputBuffer(index);
        return NS_ERROR_FAILURE;
      }
    } else {
      AppendFrame(aOutputBuf, omxBuf->data(), omxBuf->size());
    }
  }
  mCodec->releaseOutputBuffer(index);

  if (aOutputTimestamp) {
    *aOutputTimestamp = outTimeUs;
  }

  if (aOutputFlags) {
    *aOutputFlags = outFlags;
  }

  return NS_OK;
}

}
