/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WMFReader.h"
#include "WMFDecoder.h"
#include "WMFUtils.h"
#include "WMFByteStream.h"
#include "WMFSourceReaderCallback.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/dom/TimeRanges.h"
#include "mozilla/dom/HTMLMediaElement.h"
#include "mozilla/Preferences.h"
#include "DXVA2Manager.h"
#include "ImageContainer.h"
#include "Layers.h"
#include "mozilla/layers/LayersTypes.h"
#include "gfxWindowsPlatform.h"

#ifndef MOZ_SAMPLE_TYPE_FLOAT32
#error We expect 32bit float audio samples on desktop for the Windows Media Foundation media backend.
#endif

#include "MediaDecoder.h"
#include "VideoUtils.h"
#include "gfx2DGlue.h"

using namespace mozilla::gfx;
using mozilla::layers::Image;
using mozilla::layers::LayerManager;
using mozilla::layers::LayersBackend;

namespace mozilla {

extern PRLogModuleInfo* gMediaDecoderLog;
#define DECODER_LOG(...) PR_LOG(gMediaDecoderLog, PR_LOG_DEBUG, (__VA_ARGS__))

// Uncomment to enable verbose per-sample logging.
//#define LOG_SAMPLE_DECODE 1

WMFReader::WMFReader(AbstractMediaDecoder* aDecoder)
  : MediaDecoderReader(aDecoder),
    mSourceReader(nullptr),
    mAudioChannels(0),
    mAudioBytesPerSample(0),
    mAudioRate(0),
    mVideoWidth(0),
    mVideoHeight(0),
    mVideoStride(0),
    mAudioFrameOffset(0),
    mAudioFrameSum(0),
    mMustRecaptureAudioPosition(true),
    mHasAudio(false),
    mHasVideo(false),
    mUseHwAccel(false),
    mIsMP3Enabled(WMFDecoder::IsMP3Supported()),
    mCOMInitialized(false)
{
  NS_ASSERTION(NS_IsMainThread(), "Must be on main thread.");
  MOZ_COUNT_CTOR(WMFReader);
}

WMFReader::~WMFReader()
{
  NS_ASSERTION(NS_IsMainThread(), "Must be on main thread.");

  // Note: We must shutdown the byte stream before calling MFShutdown, else we
  // get assertion failures when unlocking the byte stream's work queue.
  if (mByteStream) {
    DebugOnly<nsresult> rv = mByteStream->Shutdown();
    NS_ASSERTION(NS_SUCCEEDED(rv), "Failed to shutdown WMFByteStream");
  }
  DebugOnly<HRESULT> hr = wmf::MFShutdown();
  NS_ASSERTION(SUCCEEDED(hr), "MFShutdown failed");
  MOZ_COUNT_DTOR(WMFReader);
}

bool
WMFReader::InitializeDXVA()
{
  if (gfxWindowsPlatform::GetPlatform()->IsWARP() ||
      !gfxPlatform::CanUseHardwareVideoDecoding()) {
    return false;
  }

  MOZ_ASSERT(mDecoder->GetImageContainer());

  // Extract the layer manager backend type so that we can determine
  // whether it's worthwhile using DXVA. If we're not running with a D3D
  // layer manager then the readback of decoded video frames from GPU to
  // CPU memory grinds painting to a halt, and makes playback performance
  // *worse*.
  MediaDecoderOwner* owner = mDecoder->GetOwner();
  NS_ENSURE_TRUE(owner, false);

  dom::HTMLMediaElement* element = owner->GetMediaElement();
  NS_ENSURE_TRUE(element, false);

  nsRefPtr<LayerManager> layerManager =
    nsContentUtils::LayerManagerForDocument(element->OwnerDoc());
  NS_ENSURE_TRUE(layerManager, false);

  LayersBackend backend = layerManager->GetCompositorBackendType();
  if (backend != LayersBackend::LAYERS_D3D9 &&
      backend != LayersBackend::LAYERS_D3D11) {
    return false;
  }

  mDXVA2Manager = DXVA2Manager::CreateD3D9DXVA();

  return mDXVA2Manager != nullptr;
}

nsresult
WMFReader::Init(MediaDecoderReader* aCloneDonor)
{
  NS_ASSERTION(NS_IsMainThread(), "Must be on main thread.");

  nsresult rv = WMFDecoder::LoadDLLs();
  NS_ENSURE_SUCCESS(rv, rv);

  if (FAILED(wmf::MFStartup())) {
    NS_WARNING("Failed to initialize Windows Media Foundation");
    return NS_ERROR_FAILURE;
  }

  mSourceReaderCallback = new WMFSourceReaderCallback();

  // Must be created on main thread.
  mByteStream = new WMFByteStream(mDecoder->GetResource(), mSourceReaderCallback);
  rv = mByteStream->Init();
  NS_ENSURE_SUCCESS(rv, rv);

  if (mDecoder->GetImageContainer() != nullptr &&
      IsVideoContentType(mDecoder->GetResource()->GetContentType())) {
    mUseHwAccel = InitializeDXVA();
  } else {
    mUseHwAccel = false;
  }

  return NS_OK;
}

bool
WMFReader::HasAudio()
{
  MOZ_ASSERT(OnTaskQueue());
  return mHasAudio;
}

bool
WMFReader::HasVideo()
{
  MOZ_ASSERT(OnTaskQueue());
  return mHasVideo;
}

static HRESULT
ConfigureSourceReaderStream(IMFSourceReader *aReader,
                            const DWORD aStreamIndex,
                            const GUID& aOutputSubType,
                            const GUID* aAllowedInSubTypes,
                            const uint32_t aNumAllowedInSubTypes)
{
  NS_ENSURE_TRUE(aReader, E_POINTER);
  NS_ENSURE_TRUE(aAllowedInSubTypes, E_POINTER);

  RefPtr<IMFMediaType> nativeType;
  RefPtr<IMFMediaType> type;
  HRESULT hr;

  // Find the native format of the stream.
  hr = aReader->GetNativeMediaType(aStreamIndex, 0, byRef(nativeType));
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  // Get the native output subtype of the stream. This denotes the uncompressed
  // type.
  GUID subType;
  hr = nativeType->GetGUID(MF_MT_SUBTYPE, &subType);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  // Ensure the input type of the media is in the allowed formats list.
  bool isSubTypeAllowed = false;
  for (uint32_t i = 0; i < aNumAllowedInSubTypes; i++) {
    if (aAllowedInSubTypes[i] == subType) {
      isSubTypeAllowed = true;
      break;
    }
  }
  if (!isSubTypeAllowed) {
    nsCString name = GetGUIDName(subType);
    DECODER_LOG("ConfigureSourceReaderStream subType=%s is not allowed to be decoded", name.get());
    return E_FAIL;
  }

  // Find the major type.
  GUID majorType;
  hr = nativeType->GetGUID(MF_MT_MAJOR_TYPE, &majorType);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  // Define the output type.
  hr = wmf::MFCreateMediaType(byRef(type));
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  hr = type->SetGUID(MF_MT_MAJOR_TYPE, majorType);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  hr = type->SetGUID(MF_MT_SUBTYPE, aOutputSubType);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  // Set the uncompressed format. This can fail if the decoder can't produce
  // that type.
  return aReader->SetCurrentMediaType(aStreamIndex, nullptr, type);
}

// Returns the duration of the resource, in microseconds.
HRESULT
GetSourceReaderDuration(IMFSourceReader *aReader,
                        int64_t& aOutDuration)
{
  AutoPropVar var;
  HRESULT hr = aReader->GetPresentationAttribute(MF_SOURCE_READER_MEDIASOURCE,
                                                 MF_PD_DURATION,
                                                 &var);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  // WMF stores duration in hundred nanosecond units.
  int64_t duration_hns = 0;
  hr = wmf::PropVariantToInt64(var, &duration_hns);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  aOutDuration = HNsToUsecs(duration_hns);

  return S_OK;
}

HRESULT
GetSourceReaderCanSeek(IMFSourceReader* aReader, bool& aOutCanSeek)
{
  NS_ENSURE_TRUE(aReader, E_FAIL);

  HRESULT hr;
  AutoPropVar var;
  hr = aReader->GetPresentationAttribute(MF_SOURCE_READER_MEDIASOURCE,
                                         MF_SOURCE_READER_MEDIASOURCE_CHARACTERISTICS,
                                         &var);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  ULONG flags = 0;
  hr = wmf::PropVariantToUInt32(var, &flags);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  aOutCanSeek = ((flags & MFMEDIASOURCE_CAN_SEEK) == MFMEDIASOURCE_CAN_SEEK);

  return S_OK;
}

HRESULT
WMFReader::ConfigureVideoFrameGeometry(IMFMediaType* aMediaType)
{
  NS_ENSURE_TRUE(aMediaType != nullptr, E_POINTER);
  HRESULT hr;

  // Verify that the video subtype is what we expect it to be.
  // When using hardware acceleration/DXVA2 the video format should
  // be NV12, which is DXVA2's preferred format. For software decoding
  // we use YV12, as that's easier for us to stick into our rendering
  // pipeline than NV12. NV12 has interleaved UV samples, whereas YV12
  // is a planar format.
  GUID videoFormat;
  hr = aMediaType->GetGUID(MF_MT_SUBTYPE, &videoFormat);
  NS_ENSURE_TRUE(videoFormat == MFVideoFormat_NV12 || !mUseHwAccel, E_FAIL);
  NS_ENSURE_TRUE(videoFormat == MFVideoFormat_YV12 || mUseHwAccel, E_FAIL);

  nsIntRect pictureRegion;
  hr = GetPictureRegion(aMediaType, pictureRegion);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  UINT32 width = 0, height = 0;
  hr = MFGetAttributeSize(aMediaType, MF_MT_FRAME_SIZE, &width, &height);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  uint32_t aspectNum = 0, aspectDenom = 0;
  hr = MFGetAttributeRatio(aMediaType,
                           MF_MT_PIXEL_ASPECT_RATIO,
                           &aspectNum,
                           &aspectDenom);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  // Calculate and validate the picture region and frame dimensions after
  // scaling by the pixel aspect ratio.
  nsIntSize frameSize = nsIntSize(width, height);
  nsIntSize displaySize = nsIntSize(pictureRegion.width, pictureRegion.height);
  ScaleDisplayByAspectRatio(displaySize, float(aspectNum) / float(aspectDenom));
  if (!IsValidVideoRegion(frameSize, pictureRegion, displaySize)) {
    // Video track's frame sizes will overflow. Ignore the video track.
    return E_FAIL;
  }

  // Success! Save state.
  mInfo.mVideo.mDisplay = displaySize;
  GetDefaultStride(aMediaType, &mVideoStride);
  mVideoWidth = width;
  mVideoHeight = height;
  mPictureRegion = pictureRegion;

  DECODER_LOG("WMFReader frame geometry frame=(%u,%u) stride=%u picture=(%d, %d, %d, %d) display=(%d,%d) PAR=%d:%d",
              width, height,
              mVideoStride,
              mPictureRegion.x, mPictureRegion.y, mPictureRegion.width, mPictureRegion.height,
              displaySize.width, displaySize.height,
              aspectNum, aspectDenom);

  return S_OK;
}

HRESULT
WMFReader::ConfigureVideoDecoder()
{
  NS_ASSERTION(mSourceReader, "Must have a SourceReader before configuring decoders!");

  // Determine if we have video.
  if (!mSourceReader ||
      !SourceReaderHasStream(mSourceReader, MF_SOURCE_READER_FIRST_VIDEO_STREAM)) {
    // No stream, no error.
    return S_OK;
  }

  if (!mDecoder->GetImageContainer()) {
    // We can't display the video, so don't bother to decode; disable the stream.
    return mSourceReader->SetStreamSelection(MF_SOURCE_READER_FIRST_VIDEO_STREAM, FALSE);
  }

  static const GUID MP4VideoTypes[] = {
    MFVideoFormat_H264
  };
  HRESULT hr = ConfigureSourceReaderStream(mSourceReader,
                                           MF_SOURCE_READER_FIRST_VIDEO_STREAM,
                                           mUseHwAccel ? MFVideoFormat_NV12 : MFVideoFormat_YV12,
                                           MP4VideoTypes,
                                           ArrayLength(MP4VideoTypes));
  if (FAILED(hr)) {
    DECODER_LOG("Failed to configured video output");
    return hr;
  }

  RefPtr<IMFMediaType> mediaType;
  hr = mSourceReader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM,
                                          byRef(mediaType));
  if (FAILED(hr)) {
    NS_WARNING("Failed to get configured video media type");
    return hr;
  }

  if (FAILED(ConfigureVideoFrameGeometry(mediaType))) {
    NS_WARNING("Failed configured video frame dimensions");
    return hr;
  }

  DECODER_LOG("Successfully configured video stream");

  mHasVideo = true;

  return S_OK;
}

void
WMFReader::GetSupportedAudioCodecs(const GUID** aCodecs, uint32_t* aNumCodecs)
{
  MOZ_ASSERT(aCodecs);
  MOZ_ASSERT(aNumCodecs);

  if (mIsMP3Enabled) {
    GUID aacOrMp3 = MFMPEG4Format_Base;
    aacOrMp3.Data1 = 0x6D703461;// FOURCC('m','p','4','a');
    static const GUID codecs[] = {
      MFAudioFormat_AAC,
      MFAudioFormat_MP3,
      aacOrMp3
    };
    *aCodecs = codecs;
    *aNumCodecs = ArrayLength(codecs);
  } else {
    static const GUID codecs[] = {
      MFAudioFormat_AAC
    };
    *aCodecs = codecs;
    *aNumCodecs = ArrayLength(codecs);
  }
}

HRESULT
WMFReader::ConfigureAudioDecoder()
{
  NS_ASSERTION(mSourceReader, "Must have a SourceReader before configuring decoders!");

  if (!mSourceReader ||
      !SourceReaderHasStream(mSourceReader, MF_SOURCE_READER_FIRST_AUDIO_STREAM)) {
    // No stream, no error.
    return S_OK;
  }

  const GUID* codecs;
  uint32_t numCodecs = 0;
  GetSupportedAudioCodecs(&codecs, &numCodecs);

  HRESULT hr = ConfigureSourceReaderStream(mSourceReader,
                                           MF_SOURCE_READER_FIRST_AUDIO_STREAM,
                                           MFAudioFormat_Float,
                                           codecs,
                                           numCodecs);
  if (FAILED(hr)) {
    NS_WARNING("Failed to configure WMF Audio decoder for PCM output");
    return hr;
  }

  RefPtr<IMFMediaType> mediaType;
  hr = mSourceReader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM,
                                          byRef(mediaType));
  if (FAILED(hr)) {
    NS_WARNING("Failed to get configured audio media type");
    return hr;
  }

  mAudioRate = MFGetAttributeUINT32(mediaType, MF_MT_AUDIO_SAMPLES_PER_SECOND, 0);
  mAudioChannels = MFGetAttributeUINT32(mediaType, MF_MT_AUDIO_NUM_CHANNELS, 0);
  mAudioBytesPerSample = MFGetAttributeUINT32(mediaType, MF_MT_AUDIO_BITS_PER_SAMPLE, 16) / 8;

  mInfo.mAudio.mChannels = mAudioChannels;
  mInfo.mAudio.mRate = mAudioRate;
  mHasAudio = true;

  DECODER_LOG("Successfully configured audio stream. rate=%u channels=%u bitsPerSample=%u",
              mAudioRate, mAudioChannels, mAudioBytesPerSample);

  return S_OK;
}

HRESULT
WMFReader::CreateSourceReader()
{
  HRESULT hr;

  RefPtr<IMFAttributes> attr;
  hr = wmf::MFCreateAttributes(byRef(attr), 1);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  hr = attr->SetUnknown(MF_SOURCE_READER_ASYNC_CALLBACK, mSourceReaderCallback);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  if (mUseHwAccel) {
    hr = attr->SetUnknown(MF_SOURCE_READER_D3D_MANAGER,
                          mDXVA2Manager->GetDXVADeviceManager());
    if (FAILED(hr)) {
      DECODER_LOG("Failed to set DXVA2 D3D Device manager on source reader attributes");
      mUseHwAccel = false;
    }
  }

  hr = wmf::MFCreateSourceReaderFromByteStream(mByteStream, attr, byRef(mSourceReader));
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  hr = ConfigureVideoDecoder();
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  hr = ConfigureAudioDecoder();
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  if (mUseHwAccel && mInfo.HasVideo()) {
    RefPtr<IMFTransform> videoDecoder;
    hr = mSourceReader->GetServiceForStream(MF_SOURCE_READER_FIRST_VIDEO_STREAM,
                                            GUID_NULL,
                                            IID_IMFTransform,
                                            (void**)(IMFTransform**)(byRef(videoDecoder)));

    if (SUCCEEDED(hr)) {
      ULONG_PTR manager = ULONG_PTR(mDXVA2Manager->GetDXVADeviceManager());
      hr = videoDecoder->ProcessMessage(MFT_MESSAGE_SET_D3D_MANAGER,
                                        manager);
      if (hr == MF_E_TRANSFORM_TYPE_NOT_SET) {
        // Ignore MF_E_TRANSFORM_TYPE_NOT_SET. Vista returns this here
        // on some, perhaps all, video cards. This may be because activating
        // DXVA changes the available output types. It seems to be safe to
        // ignore this error.
        hr = S_OK;
      }
    }
    if (FAILED(hr)) {
      DECODER_LOG("Failed to set DXVA2 D3D Device manager on decoder hr=0x%x", hr);
      mUseHwAccel = false;
    }
  }
  return hr;
}

nsresult
WMFReader::ReadMetadata(MediaInfo* aInfo,
                        MetadataTags** aTags)
{
  MOZ_ASSERT(OnTaskQueue());

  DECODER_LOG("WMFReader::ReadMetadata()");
  HRESULT hr;

  const bool triedToInitDXVA = mUseHwAccel;
  if (FAILED(CreateSourceReader())) {
    mSourceReader = nullptr;
    if (triedToInitDXVA && !mUseHwAccel) {
      // We tried to initialize DXVA and failed. Try again to create the
      // IMFSourceReader but this time we won't use DXVA. Note that we
      // must recreate the IMFSourceReader from scratch, as on some systems
      // (AMD Radeon 3000) we cannot successfully reconfigure an existing
      // reader to not use DXVA after we've failed to configure DXVA.
      // See bug 987127.
      if (FAILED(CreateSourceReader())) {
        mSourceReader = nullptr;
      }
    }
  }

  if (!mSourceReader) {
    NS_WARNING("Failed to create IMFSourceReader");
    return NS_ERROR_FAILURE;
  }

  if (mInfo.HasVideo()) {
    DECODER_LOG("Using DXVA: %s", (mUseHwAccel ? "Yes" : "No"));
  }

  // Abort if both video and audio failed to initialize.
  NS_ENSURE_TRUE(mInfo.HasValidMedia(), NS_ERROR_FAILURE);

  // Get the duration, and report it to the decoder if we have it.
  int64_t duration = 0;
  hr = GetSourceReaderDuration(mSourceReader, duration);
  if (SUCCEEDED(hr)) {
    ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
    mDecoder->SetMediaEndTime(duration);
  }

  *aInfo = mInfo;
  *aTags = nullptr;
  // aTags can be retrieved using techniques like used here:
  // http://blogs.msdn.com/b/mf/archive/2010/01/12/mfmediapropdump.aspx

  return NS_OK;
}

bool
WMFReader::IsMediaSeekable()
{
  // Get the duration
  int64_t duration = 0;
  HRESULT hr = GetSourceReaderDuration(mSourceReader, duration);
  // We can seek if we get a duration *and* the reader reports that it's
  // seekable.
  bool canSeek = false;
  if (FAILED(hr) || FAILED(GetSourceReaderCanSeek(mSourceReader, canSeek)) ||
      !canSeek) {
    return false;
  }
  return true;
}

bool
WMFReader::DecodeAudioData()
{
  MOZ_ASSERT(OnTaskQueue());

  HRESULT hr;
  hr = mSourceReader->ReadSample(MF_SOURCE_READER_FIRST_AUDIO_STREAM,
                                 0, // control flags
                                 0, // read stream index
                                 nullptr,
                                 nullptr,
                                 nullptr);

  if (FAILED(hr)) {
    DECODER_LOG("WMFReader::DecodeAudioData() ReadSample failed with hr=0x%x", hr);
    // End the stream.
    return false;
  }

  DWORD flags = 0;
  LONGLONG timestampHns = 0;
  RefPtr<IMFSample> sample;
  hr = mSourceReaderCallback->Wait(&flags, &timestampHns, byRef(sample));
  if (FAILED(hr) ||
      (flags & MF_SOURCE_READERF_ERROR) ||
      (flags & MF_SOURCE_READERF_ENDOFSTREAM) ||
      (flags & MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED)) {
    DECODER_LOG("WMFReader::DecodeAudioData() ReadSample failed with hr=0x%x flags=0x%x",
                hr, flags);
    // End the stream.
    return false;
  }

  if (!sample) {
    // Not enough data? Try again...
    return true;
  }

  RefPtr<IMFMediaBuffer> buffer;
  hr = sample->ConvertToContiguousBuffer(byRef(buffer));
  NS_ENSURE_TRUE(SUCCEEDED(hr), false);

  BYTE* data = nullptr; // Note: *data will be owned by the IMFMediaBuffer, we don't need to free it.
  DWORD maxLength = 0, currentLength = 0;
  hr = buffer->Lock(&data, &maxLength, &currentLength);
  NS_ENSURE_TRUE(SUCCEEDED(hr), false);

  uint32_t numFrames = currentLength / mAudioBytesPerSample / mAudioChannels;
  NS_ASSERTION(sizeof(AudioDataValue) == mAudioBytesPerSample, "Size calculation is wrong");
  nsAutoArrayPtr<AudioDataValue> pcmSamples(new AudioDataValue[numFrames * mAudioChannels]);
  memcpy(pcmSamples.get(), data, currentLength);
  buffer->Unlock();

  // We calculate the timestamp and the duration based on the number of audio
  // frames we've already played. We don't trust the timestamp stored on the
  // IMFSample, as sometimes it's wrong, possibly due to buggy encoders?

  // If this sample block comes after a discontinuity (i.e. a gap or seek)
  // reset the frame counters, and capture the timestamp. Future timestamps
  // will be offset from this block's timestamp.
  UINT32 discontinuity = false;
  sample->GetUINT32(MFSampleExtension_Discontinuity, &discontinuity);
  if (mMustRecaptureAudioPosition || discontinuity) {
    mAudioFrameSum = 0;
    hr = HNsToFrames(timestampHns, mAudioRate, &mAudioFrameOffset);
    NS_ENSURE_TRUE(SUCCEEDED(hr), false);
    mMustRecaptureAudioPosition = false;
  }

  int64_t timestamp;
  hr = FramesToUsecs(mAudioFrameOffset + mAudioFrameSum, mAudioRate, &timestamp);
  NS_ENSURE_TRUE(SUCCEEDED(hr), false);

  mAudioFrameSum += numFrames;

  int64_t duration;
  hr = FramesToUsecs(numFrames, mAudioRate, &duration);
  NS_ENSURE_TRUE(SUCCEEDED(hr), false);

  mAudioQueue.Push(new AudioData(mDecoder->GetResource()->Tell(),
                                 timestamp,
                                 duration,
                                 numFrames,
                                 pcmSamples.forget(),
                                 mAudioChannels,
                                 mAudioRate));

  #ifdef LOG_SAMPLE_DECODE
  DECODER_LOG("Decoded audio sample! timestamp=%lld duration=%lld currentLength=%u",
              timestamp, duration, currentLength);
  #endif

  return true;
}

HRESULT
WMFReader::CreateBasicVideoFrame(IMFSample* aSample,
                                 int64_t aTimestampUsecs,
                                 int64_t aDurationUsecs,
                                 int64_t aOffsetBytes,
                                 VideoData** aOutVideoData)
{
  NS_ENSURE_TRUE(aSample, E_POINTER);
  NS_ENSURE_TRUE(aOutVideoData, E_POINTER);

  *aOutVideoData = nullptr;

  HRESULT hr;
  RefPtr<IMFMediaBuffer> buffer;

  // Must convert to contiguous buffer to use IMD2DBuffer interface.
  hr = aSample->ConvertToContiguousBuffer(byRef(buffer));
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  // Try and use the IMF2DBuffer interface if available, otherwise fallback
  // to the IMFMediaBuffer interface. Apparently IMF2DBuffer is more efficient,
  // but only some systems (Windows 8?) support it.
  BYTE* data = nullptr;
  LONG stride = 0;
  RefPtr<IMF2DBuffer> twoDBuffer;
  hr = buffer->QueryInterface(static_cast<IMF2DBuffer**>(byRef(twoDBuffer)));
  if (SUCCEEDED(hr)) {
    hr = twoDBuffer->Lock2D(&data, &stride);
    NS_ENSURE_TRUE(SUCCEEDED(hr), hr);
  } else {
    hr = buffer->Lock(&data, nullptr, nullptr);
    NS_ENSURE_TRUE(SUCCEEDED(hr), hr);
    stride = mVideoStride;
  }

  // YV12, planar format: [YYYY....][VVVV....][UUUU....]
  // i.e., Y, then V, then U.
  VideoData::YCbCrBuffer b;

  // Y (Y') plane
  b.mPlanes[0].mData = data;
  b.mPlanes[0].mStride = stride;
  b.mPlanes[0].mHeight = mVideoHeight;
  b.mPlanes[0].mWidth = mVideoWidth;
  b.mPlanes[0].mOffset = 0;
  b.mPlanes[0].mSkip = 0;

  // The V and U planes are stored 16-row-aligned, so we need to add padding
  // to the row heights to ensure the Y'CbCr planes are referenced properly.
  uint32_t padding = 0;
  if (mVideoHeight % 16 != 0) {
    padding = 16 - (mVideoHeight % 16);
  }
  uint32_t y_size = stride * (mVideoHeight + padding);
  uint32_t v_size = stride * (mVideoHeight + padding) / 4;
  uint32_t halfStride = (stride + 1) / 2;
  uint32_t halfHeight = (mVideoHeight + 1) / 2;
  uint32_t halfWidth = (mVideoWidth + 1) / 2;

  // U plane (Cb)
  b.mPlanes[1].mData = data + y_size + v_size;
  b.mPlanes[1].mStride = halfStride;
  b.mPlanes[1].mHeight = halfHeight;
  b.mPlanes[1].mWidth = halfWidth;
  b.mPlanes[1].mOffset = 0;
  b.mPlanes[1].mSkip = 0;

  // V plane (Cr)
  b.mPlanes[2].mData = data + y_size;
  b.mPlanes[2].mStride = halfStride;
  b.mPlanes[2].mHeight = halfHeight;
  b.mPlanes[2].mWidth = halfWidth;
  b.mPlanes[2].mOffset = 0;
  b.mPlanes[2].mSkip = 0;

  nsRefPtr<VideoData> v = VideoData::Create(mInfo.mVideo,
                                            mDecoder->GetImageContainer(),
                                            aOffsetBytes,
                                            aTimestampUsecs,
                                            aDurationUsecs,
                                            b,
                                            false,
                                            -1,
                                            mPictureRegion);
  if (twoDBuffer) {
    twoDBuffer->Unlock2D();
  } else {
    buffer->Unlock();
  }

  v.forget(aOutVideoData);
  return S_OK;
}

HRESULT
WMFReader::CreateD3DVideoFrame(IMFSample* aSample,
                               int64_t aTimestampUsecs,
                               int64_t aDurationUsecs,
                               int64_t aOffsetBytes,
                               VideoData** aOutVideoData)
{
  NS_ENSURE_TRUE(aSample, E_POINTER);
  NS_ENSURE_TRUE(aOutVideoData, E_POINTER);
  NS_ENSURE_TRUE(mDXVA2Manager, E_ABORT);
  NS_ENSURE_TRUE(mUseHwAccel, E_ABORT);

  *aOutVideoData = nullptr;
  HRESULT hr;

  nsRefPtr<Image> image;
  hr = mDXVA2Manager->CopyToImage(aSample,
                                  mPictureRegion,
                                  mDecoder->GetImageContainer(),
                                  getter_AddRefs(image));
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);
  NS_ENSURE_TRUE(image, E_FAIL);

  nsRefPtr<VideoData> v = VideoData::CreateFromImage(mInfo.mVideo,
                                                     mDecoder->GetImageContainer(),
                                                     aOffsetBytes,
                                                     aTimestampUsecs,
                                                     aDurationUsecs,
                                                     image.forget(),
                                                     false,
                                                     -1,
                                                     mPictureRegion);

  NS_ENSURE_TRUE(v, E_FAIL);
  v.forget(aOutVideoData);

  return S_OK;
}

bool
WMFReader::DecodeVideoFrame(bool &aKeyframeSkip,
                            int64_t aTimeThreshold)
{
  MOZ_ASSERT(OnTaskQueue());

  // Record number of frames decoded and parsed. Automatically update the
  // stats counters using the AutoNotifyDecoded stack-based class.
  AbstractMediaDecoder::AutoNotifyDecoded a(mDecoder);

  HRESULT hr;

  hr = mSourceReader->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM,
                                 0, // control flags
                                 0, // read stream index
                                 nullptr,
                                 nullptr,
                                 nullptr);
  if (FAILED(hr)) {
    DECODER_LOG("WMFReader::DecodeVideoData() ReadSample failed with hr=0x%x", hr);
    return false;
  }

  DWORD flags = 0;
  LONGLONG timestampHns = 0;
  RefPtr<IMFSample> sample;
  hr = mSourceReaderCallback->Wait(&flags, &timestampHns, byRef(sample));

  if (flags & MF_SOURCE_READERF_ERROR) {
    NS_WARNING("WMFReader: Catastrophic failure reading video sample");
    // Future ReadSample() calls will fail, so give up and report end of stream.
    return false;
  }

  if (FAILED(hr)) {
    // Unknown failure, ask caller to try again?
    return true;
  }

  if (!sample) {
    if ((flags & MF_SOURCE_READERF_ENDOFSTREAM)) {
      DECODER_LOG("WMFReader; Null sample after video decode, at end of stream");
      return false;
    }
    DECODER_LOG("WMFReader; Null sample after video decode. Maybe insufficient data...");
    return true;
  }

  if ((flags & MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED)) {
    DECODER_LOG("WMFReader: Video media type changed!");
    RefPtr<IMFMediaType> mediaType;
    hr = mSourceReader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM,
                                            byRef(mediaType));
    if (FAILED(hr) ||
        FAILED(ConfigureVideoFrameGeometry(mediaType))) {
      NS_WARNING("Failed to reconfigure video media type");
      return false;
    }
  }

  int64_t timestamp = HNsToUsecs(timestampHns);
  if (timestamp < aTimeThreshold) {
    return true;
  }
  int64_t offset = mDecoder->GetResource()->Tell();
  int64_t duration = GetSampleDuration(sample);

  VideoData* v = nullptr;
  if (mUseHwAccel) {
    hr = CreateD3DVideoFrame(sample, timestamp, duration, offset, &v);
  } else {
    hr = CreateBasicVideoFrame(sample, timestamp, duration, offset, &v);
  }
  NS_ENSURE_TRUE(SUCCEEDED(hr) && v, false);

  a.mParsed++;
  a.mDecoded++;
  mVideoQueue.Push(v);

  #ifdef LOG_SAMPLE_DECODE
  DECODER_LOG("Decoded video sample timestamp=%lld duration=%lld stride=%d height=%u flags=%u",
              timestamp, duration, mVideoStride, mVideoHeight, flags);
  #endif

  if ((flags & MF_SOURCE_READERF_ENDOFSTREAM)) {
    // End of stream.
    DECODER_LOG("End of video stream");
    return false;
  }

  return true;
}

nsRefPtr<MediaDecoderReader::SeekPromise>
WMFReader::Seek(int64_t aTargetUs, int64_t aEndTime)
{
  nsresult res = SeekInternal(aTargetUs);
  if (NS_FAILED(res)) {
    return SeekPromise::CreateAndReject(res, __func__);
  } else {
    return SeekPromise::CreateAndResolve(aTargetUs, __func__);
  }
}

nsresult
WMFReader::SeekInternal(int64_t aTargetUs)
{
  DECODER_LOG("WMFReader::Seek() %lld", aTargetUs);

  MOZ_ASSERT(OnTaskQueue());
#ifdef DEBUG
  bool canSeek = false;
  GetSourceReaderCanSeek(mSourceReader, canSeek);
  NS_ASSERTION(canSeek, "WMFReader::Seek() should only be called if we can seek!");
#endif

  nsresult rv = ResetDecode();
  NS_ENSURE_SUCCESS(rv, rv);

  // Mark that we must recapture the audio frame count from the next sample.
  // WMF doesn't set a discontinuity marker when we seek to time 0, so we
  // must remember to recapture the audio frame offset and reset the frame
  // sum on the next audio packet we decode.
  mMustRecaptureAudioPosition = true;

  AutoPropVar var;
  HRESULT hr = InitPropVariantFromInt64(UsecsToHNs(aTargetUs), &var);
  NS_ENSURE_TRUE(SUCCEEDED(hr), NS_ERROR_FAILURE);

  hr = mSourceReader->SetCurrentPosition(GUID_NULL, var);
  NS_ENSURE_TRUE(SUCCEEDED(hr), NS_ERROR_FAILURE);

  return NS_OK;
}

} // namespace mozilla
