/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "OMXCodecWrapper.h"
#include "OMXCodecDescriptorUtil.h"
#include "TrackEncoder.h"

#include <binder/ProcessState.h>
#include <cutils/properties.h>
#include <media/ICrypto.h>
#include <media/IOMX.h>
#include <OMX_Component.h>
#include <stagefright/MediaDefs.h>
#include <stagefright/MediaErrors.h>

#include "AudioChannelFormat.h"
#include <mozilla/Monitor.h>
#include "mozilla/layers/GrallocTextureClient.h"

using namespace mozilla;
using namespace mozilla::gfx;
using namespace mozilla::layers;

#define INPUT_BUFFER_TIMEOUT_US (5 * 1000ll)
// AMR NB kbps
#define AMRNB_BITRATE 12200

#define CODEC_ERROR(args...)                                                   \
  do {                                                                         \
    __android_log_print(ANDROID_LOG_ERROR, "OMXCodecWrapper", ##args);         \
  } while (0)

namespace android {

bool
OMXCodecReservation::ReserveOMXCodec()
{
  if (!mManagerService.get()) {
    sp<MediaResourceManagerClient::EventListener> listener = this;
    mClient = new MediaResourceManagerClient(listener);

    mManagerService = mClient->getMediaResourceManagerService();
    if (!mManagerService.get()) {
      mClient = nullptr;
      return true; // not really in use, but not usable
    }
  }
  return (mManagerService->requestMediaResource(mClient, mType, false) == OK); // don't wait
}

void
OMXCodecReservation::ReleaseOMXCodec()
{
  if (!mManagerService.get() || !mClient.get()) {
    return;
  }

  mManagerService->cancelClient(mClient, mType);
}

OMXAudioEncoder*
OMXCodecWrapper::CreateAACEncoder()
{
  nsAutoPtr<OMXAudioEncoder> aac(new OMXAudioEncoder(CodecType::AAC_ENC));
  // Return the object only when media codec is valid.
  NS_ENSURE_TRUE(aac->IsValid(), nullptr);

  return aac.forget();
}

OMXAudioEncoder*
OMXCodecWrapper::CreateAMRNBEncoder()
{
  nsAutoPtr<OMXAudioEncoder> amr(new OMXAudioEncoder(CodecType::AMR_NB_ENC));
  // Return the object only when media codec is valid.
  NS_ENSURE_TRUE(amr->IsValid(), nullptr);

  return amr.forget();
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
  : mCodecType(aCodecType)
  , mStarted(false)
  , mAMRCSDProvided(false)
{
  ProcessState::self()->startThreadPool();

  mLooper = new ALooper();
  mLooper->start();

  if (aCodecType == CodecType::AVC_ENC) {
    mCodec = MediaCodec::CreateByType(mLooper, MEDIA_MIMETYPE_VIDEO_AVC, true);
  } else if (aCodecType == CodecType::AMR_NB_ENC) {
    mCodec = MediaCodec::CreateByType(mLooper, MEDIA_MIMETYPE_AUDIO_AMR_NB, true);
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

// Check system property to see if we're running on emulator.
static bool
IsRunningOnEmulator()
{
  char qemu[PROPERTY_VALUE_MAX];
  property_get("ro.kernel.qemu", qemu, "");
  return strncmp(qemu, "1", 1) == 0;
}

#define ENCODER_CONFIG_BITRATE 2000000 // bps
// How many seconds between I-frames.
#define ENCODER_CONFIG_I_FRAME_INTERVAL 1
// Wait up to 5ms for input buffers.

nsresult
OMXVideoEncoder::Configure(int aWidth, int aHeight, int aFrameRate,
                           BlobFormat aBlobFormat)
{
  NS_ENSURE_TRUE(aWidth > 0 && aHeight > 0 && aFrameRate > 0,
                 NS_ERROR_INVALID_ARG);

  OMX_VIDEO_AVCLEVELTYPE level = OMX_VIDEO_AVCLevel3;
  OMX_VIDEO_CONTROLRATETYPE bitrateMode = OMX_Video_ControlRateConstant;

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
  format->setInt32("level", level);
  format->setInt32("bitrate-mode", bitrateMode);
  format->setInt32("store-metadata-in-buffers", 0);
  format->setInt32("prepend-sps-pps-to-idr-frames", 0);
  // Input values.
  format->setInt32("width", aWidth);
  format->setInt32("height", aHeight);
  format->setInt32("stride", aWidth);
  format->setInt32("slice-height", aHeight);
  format->setInt32("frame-rate", aFrameRate);

  return ConfigureDirect(format, aBlobFormat);
}

nsresult
OMXVideoEncoder::ConfigureDirect(sp<AMessage>& aFormat,
                                 BlobFormat aBlobFormat)
{
  // We now allow re-configuration to handle resolution/framerate/etc changes
  if (mStarted) {
    Stop();
  }
  MOZ_ASSERT(!mStarted, "OMX Stop() failed?");

  int width = 0;
  int height = 0;
  int frameRate = 0;
  aFormat->findInt32("width", &width);
  aFormat->findInt32("height", &height);
  aFormat->findInt32("frame-rate", &frameRate);
  NS_ENSURE_TRUE(width > 0 && height > 0 && frameRate > 0,
                 NS_ERROR_INVALID_ARG);

  // Limitation of soft AVC/H.264 encoder running on emulator in stagefright.
  static bool emu = IsRunningOnEmulator();
  if (emu) {
    if (width > 352 || height > 288) {
      CODEC_ERROR("SoftAVCEncoder doesn't support resolution larger than CIF");
      return NS_ERROR_INVALID_ARG;
    }
    aFormat->setInt32("level", OMX_VIDEO_AVCLevel2);
    aFormat->setInt32("bitrate-mode", OMX_Video_ControlRateVariable);
  }


  status_t result = mCodec->configure(aFormat, nullptr, nullptr,
                                      MediaCodec::CONFIGURE_FLAG_ENCODE);
  NS_ENSURE_TRUE(result == OK, NS_ERROR_FAILURE);

  mWidth = width;
  mHeight = height;
  mBlobFormat = aBlobFormat;

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
static void
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

// Convert pixels in graphic buffer to NV12 format. aSource is the layer image
// containing source graphic buffer, and aDestination is the destination of
// conversion. Currently only 2 source format are supported:
// - NV21/HAL_PIXEL_FORMAT_YCrCb_420_SP (from camera preview window).
// - YV12/HAL_PIXEL_FORMAT_YV12 (from video decoder).
static void
ConvertGrallocImageToNV12(GrallocImage* aSource, uint8_t* aDestination)
{
  // Get graphic buffer.
  sp<GraphicBuffer> graphicBuffer = aSource->GetGraphicBuffer();

  int pixelFormat = graphicBuffer->getPixelFormat();
  // Only support NV21 (from camera) or YV12 (from HW decoder output) for now.
  NS_ENSURE_TRUE_VOID(pixelFormat == HAL_PIXEL_FORMAT_YCrCb_420_SP ||
                      pixelFormat == HAL_PIXEL_FORMAT_YV12);

  void* imgPtr = nullptr;
  graphicBuffer->lock(GraphicBuffer::USAGE_SW_READ_MASK, &imgPtr);
  // Build PlanarYCbCrData for NV21 or YV12 buffer.
  PlanarYCbCrData yuv;
  switch (pixelFormat) {
    case HAL_PIXEL_FORMAT_YCrCb_420_SP: // From camera.
      yuv.mYChannel = static_cast<uint8_t*>(imgPtr);
      yuv.mYSkip = 0;
      yuv.mYSize.width = graphicBuffer->getWidth();
      yuv.mYSize.height = graphicBuffer->getHeight();
      yuv.mYStride = graphicBuffer->getStride();
      // 4:2:0.
      yuv.mCbCrSize.width = yuv.mYSize.width / 2;
      yuv.mCbCrSize.height = yuv.mYSize.height / 2;
      // Interleaved VU plane.
      yuv.mCrChannel = yuv.mYChannel + (yuv.mYStride * yuv.mYSize.height);
      yuv.mCrSkip = 1;
      yuv.mCbChannel = yuv.mCrChannel + 1;
      yuv.mCbSkip = 1;
      yuv.mCbCrStride = yuv.mYStride;
      ConvertPlanarYCbCrToNV12(&yuv, aDestination);
      break;
    case HAL_PIXEL_FORMAT_YV12: // From video decoder.
      // Android YV12 format is defined in system/core/include/system/graphics.h
      yuv.mYChannel = static_cast<uint8_t*>(imgPtr);
      yuv.mYSkip = 0;
      yuv.mYSize.width = graphicBuffer->getWidth();
      yuv.mYSize.height = graphicBuffer->getHeight();
      yuv.mYStride = graphicBuffer->getStride();
      // 4:2:0.
      yuv.mCbCrSize.width = yuv.mYSize.width / 2;
      yuv.mCbCrSize.height = yuv.mYSize.height / 2;
      yuv.mCrChannel = yuv.mYChannel + (yuv.mYStride * yuv.mYSize.height);
      // Aligned to 16 bytes boundary.
      yuv.mCbCrStride = (yuv.mYStride / 2 + 15) & ~0x0F;
      yuv.mCrSkip = 0;
      yuv.mCbChannel = yuv.mCrChannel + (yuv.mCbCrStride * yuv.mCbCrSize.height);
      yuv.mCbSkip = 0;
      ConvertPlanarYCbCrToNV12(&yuv, aDestination);
      break;
    default:
      NS_ERROR("Unsupported input gralloc image type. Should never be here.");
  }

  graphicBuffer->unlock();
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
      ConvertGrallocImageToNV12(static_cast<GrallocImage*>(img), dst);
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
  // Codec already parsed aData. Using its result makes generating config blob
  // much easier.
  sp<AMessage> format;
  mCodec->getOutputFormat(&format);

  // NAL unit format is needed by WebRTC for RTP packets; AVC/H.264 decoder
  // config descriptor is needed to construct MP4 'avcC' box.
  status_t result = GenerateAVCDescriptorBlob(format, aOutputBuf, mBlobFormat);

  return result;
}

// Override to replace NAL unit start code with 4-bytes unit length.
// See ISO/IEC 14496-15 5.2.3.
void
OMXVideoEncoder::AppendFrame(nsTArray<uint8_t>* aOutputBuf,
                             const uint8_t* aData, size_t aSize)
{
  aOutputBuf->SetCapacity(aSize);

  if (mBlobFormat == BlobFormat::AVC_NAL) {
    // Append NAL format data without modification.
    aOutputBuf->AppendElements(aData, aSize);
    return;
  }
  // Replace start code with data length.
  uint8_t length[] = {
    (aSize >> 24) & 0xFF,
    (aSize >> 16) & 0xFF,
    (aSize >> 8) & 0xFF,
    aSize & 0xFF,
  };
  aOutputBuf->AppendElements(length, sizeof(length));
  aOutputBuf->AppendElements(aData + sizeof(length), aSize);
}

// MediaCodec::setParameters() is available only after API level 18.
#if ANDROID_VERSION >= 18
nsresult
OMXVideoEncoder::SetBitrate(int32_t aKbps)
{
  sp<AMessage> msg = new AMessage();
#if ANDROID_VERSION >= 19
  // XXX Do we need a runtime check here?
  msg->setInt32("video-bitrate", aKbps * 1000 /* kbps -> bps */);
#else
  msg->setInt32("videoBitrate", aKbps * 1000 /* kbps -> bps */);
#endif
  status_t result = mCodec->setParameters(msg);
  MOZ_ASSERT(result == OK);
  return result == OK ? NS_OK : NS_ERROR_FAILURE;
}
#endif

nsresult
OMXVideoEncoder::RequestIDRFrame()
{
  MOZ_ASSERT(mStarted, "Configure() should be called before RequestIDRFrame().");
  return mCodec->requestIDRFrame() == OK ? NS_OK : NS_ERROR_FAILURE;
}

nsresult
OMXAudioEncoder::Configure(int aChannels, int aInputSampleRate,
                           int aEncodedSampleRate)
{
  MOZ_ASSERT(!mStarted);

  NS_ENSURE_TRUE(aChannels > 0 && aInputSampleRate > 0 && aEncodedSampleRate >= 0,
                 NS_ERROR_INVALID_ARG);

  if (aInputSampleRate != aEncodedSampleRate) {
    int error;
    mResampler = speex_resampler_init(aChannels,
                                      aInputSampleRate,
                                      aEncodedSampleRate,
                                      SPEEX_RESAMPLER_QUALITY_DEFAULT,
                                      &error);

    if (error != RESAMPLER_ERR_SUCCESS) {
      return NS_ERROR_FAILURE;
    }
    speex_resampler_skip_zeros(mResampler);
  }
  // Set up configuration parameters for AAC encoder.
  sp<AMessage> format = new AMessage;
  // Fixed values.
  if (mCodecType == AAC_ENC) {
    format->setString("mime", MEDIA_MIMETYPE_AUDIO_AAC);
    format->setInt32("aac-profile", OMX_AUDIO_AACObjectLC);
    format->setInt32("bitrate", kAACBitrate);
    format->setInt32("sample-rate", aInputSampleRate);
  } else if (mCodecType == AMR_NB_ENC) {
    format->setString("mime", MEDIA_MIMETYPE_AUDIO_AMR_NB);
    format->setInt32("bitrate", AMRNB_BITRATE);
    format->setInt32("sample-rate", aEncodedSampleRate);
  } else {
    MOZ_ASSERT(false, "Can't support this codec type!!");
  }
  // Input values.
  format->setInt32("channel-count", aChannels);

  status_t result = mCodec->configure(format, nullptr, nullptr,
                                      MediaCodec::CONFIGURE_FLAG_ENCODE);
  NS_ENSURE_TRUE(result == OK, NS_ERROR_FAILURE);

  mChannels = aChannels;
  mSampleDuration = 1000000 / aInputSampleRate;
  mResamplingRatio = aEncodedSampleRate > 0 ? 1.0 *
                      aEncodedSampleRate / aInputSampleRate : 1.0;
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

OMXAudioEncoder::~OMXAudioEncoder()
{
  if (mResampler) {
    speex_resampler_destroy(mResampler);
    mResampler = nullptr;
  }
}

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

  size_t sourceSamplesCopied = 0; // Number of copied samples.

  if (numSamples > 0) {
    // Copy input PCM data to input buffer until queue is empty.
    AudioSegment::ChunkIterator iter(const_cast<AudioSegment&>(aSegment));
    while (!iter.IsEnded()) {
      AudioChunk chunk = *iter;
      size_t sourceSamplesToCopy = chunk.GetDuration(); // Number of samples to copy.
      size_t bytesToCopy = sourceSamplesToCopy * mChannels *
                           sizeof(AudioDataValue) * mResamplingRatio;
      if (bytesToCopy > buffer.AvailableSize()) {
        // Not enough space left in input buffer. Send it to encoder and get a
        // new one.
        result = buffer.Enqueue(mTimestamp, aInputFlags & ~BUFFER_EOS);
        NS_ENSURE_TRUE(result == OK, NS_ERROR_FAILURE);

        result = buffer.Dequeue();
        if (result == -EAGAIN) {
          // All input buffers are full. Caller can try again later after
          // consuming some output buffers.
          aSegment.RemoveLeading(sourceSamplesCopied);
          return NS_OK;
        }

        mTimestamp += sourceSamplesCopied * mSampleDuration;
        sourceSamplesCopied = 0;

        NS_ENSURE_TRUE(result == OK, NS_ERROR_FAILURE);
      }

      AudioDataValue* dst = reinterpret_cast<AudioDataValue*>(buffer.GetPointer());
      uint32_t dstSamplesCopied = sourceSamplesToCopy;
      if (!chunk.IsNull()) {
        if (mResampler) {
          nsAutoTArray<AudioDataValue, 9600> pcm;
          pcm.SetLength(bytesToCopy);
          // Append the interleaved data to input buffer.
          AudioTrackEncoder::InterleaveTrackData(chunk, sourceSamplesToCopy,
                                                 mChannels,
                                                 pcm.Elements());
          uint32_t inframes = sourceSamplesToCopy;
          short* in = reinterpret_cast<short*>(pcm.Elements());
          speex_resampler_process_interleaved_int(mResampler, in, &inframes,
                                                              dst, &dstSamplesCopied);
        } else {
          AudioTrackEncoder::InterleaveTrackData(chunk, sourceSamplesToCopy,
                                                 mChannels,
                                                 dst);
          dstSamplesCopied = sourceSamplesToCopy * mChannels;
        }
      } else {
        // Silence.
        memset(dst, 0, mResamplingRatio * sourceSamplesToCopy * sizeof(AudioDataValue));
      }

      sourceSamplesCopied += sourceSamplesToCopy;
      buffer.IncreaseOffset(dstSamplesCopied * sizeof(AudioDataValue));
      iter.Next();
    }
    if (sourceSamplesCopied > 0) {
      aSegment.RemoveLeading(sourceSamplesCopied);
    }
  } else if (aInputFlags & BUFFER_EOS) {
    // No audio data left in segment but we still have to feed something to
    // MediaCodec in order to notify EOS.
    size_t bytesToCopy = mChannels * sizeof(AudioDataValue);
    memset(buffer.GetPointer(), 0, bytesToCopy);
    buffer.IncreaseOffset(bytesToCopy);
    sourceSamplesCopied = 1;
  }

  if (sourceSamplesCopied > 0) {
    int flags = aInputFlags;
    if (aSegment.GetDuration() > 0) {
      // Don't signal EOS until source segment is empty.
      flags &= ~BUFFER_EOS;
    }
    result = buffer.Enqueue(mTimestamp, flags);
    NS_ENSURE_TRUE(result == OK, NS_ERROR_FAILURE);

    mTimestamp += sourceSamplesCopied * mSampleDuration;
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
    } else if ((mCodecType == AMR_NB_ENC) && !mAMRCSDProvided){
      // OMX AMR codec won't provide csd data, need to generate a fake one.
      nsRefPtr<EncodedFrame> audiodata = new EncodedFrame();
      // Decoder config descriptor
      const uint8_t decConfig[] = {
        0x0, 0x0, 0x0, 0x0, // vendor: 4 bytes
        0x0,                // decoder version
        0x83, 0xFF,         // mode set: all enabled
        0x00,               // mode change period
        0x01,               // frames per sample
      };
      aOutputBuf->AppendElements(decConfig, sizeof(decConfig));
      outFlags |= MediaCodec::BUFFER_FLAG_CODECCONFIG;
      mAMRCSDProvided = true;
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
