/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WMFReader.h"
#include "WMFDecoder.h"
#include "WMFUtils.h"
#include "WMFByteStream.h"

#ifndef MOZ_SAMPLE_TYPE_FLOAT32
#error We expect 32bit float audio samples on desktop for the Windows Media Foundation media backend.
#endif

#include "MediaDecoder.h"
#include "VideoUtils.h"

namespace mozilla {

#ifdef PR_LOGGING
extern PRLogModuleInfo* gMediaDecoderLog;
#define LOG(...) PR_LOG(gMediaDecoderLog, PR_LOG_DEBUG, (__VA_ARGS__))
#else
#define LOG(...)
#endif

// Uncomment to enable verbose per-sample logging.
//#define LOG_SAMPLE_DECODE 1

WMFReader::WMFReader(MediaDecoder* aDecoder)
  : MediaDecoderReader(aDecoder),
    mSourceReader(nullptr),
    mAudioChannels(0),
    mAudioBytesPerSample(0),
    mAudioRate(0),
    mVideoWidth(0),
    mVideoHeight(0),
    mVideoStride(0),
    mHasAudio(false),
    mHasVideo(false),
    mCanSeek(false)
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
    nsresult rv = mByteStream->Shutdown();
    NS_ASSERTION(NS_SUCCEEDED(rv), "Failed to shutdown WMFByteStream");
  }
  HRESULT hr = wmf::MFShutdown();
  NS_ASSERTION(SUCCEEDED(hr), "MFShutdown failed");
  MOZ_COUNT_DTOR(WMFReader);
}

void
WMFReader::OnDecodeThreadStart()
{
  NS_ASSERTION(mDecoder->OnDecodeThread(), "Should be on decode thread.");
  HRESULT hr = CoInitializeEx(0, COINIT_MULTITHREADED);
  NS_ENSURE_TRUE_VOID(SUCCEEDED(hr));
}

void
WMFReader::OnDecodeThreadFinish()
{
  NS_ASSERTION(mDecoder->OnDecodeThread(), "Should be on decode thread.");
  CoUninitialize();
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

  // Must be created on main thread.
  mByteStream = new WMFByteStream(mDecoder->GetResource());

  return mByteStream->Init();
}

bool
WMFReader::HasAudio()
{
  NS_ASSERTION(mDecoder->OnDecodeThread(), "Should be on decode thread.");
  return mHasAudio;
}

bool
WMFReader::HasVideo()
{
  NS_ASSERTION(mDecoder->OnDecodeThread(), "Should be on decode thread.");
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

  IMFMediaTypePtr nativeType;
  IMFMediaTypePtr type;
  HRESULT hr;

  // Find the native format of the stream.
  hr = aReader->GetNativeMediaType(aStreamIndex, 0, &nativeType);
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
    LOG("ConfigureSourceReaderStream subType=%s is not allowed to be decoded", name.get());
    return E_FAIL;
  }

  // Find the major type.
  GUID majorType;
  hr = nativeType->GetGUID(MF_MT_MAJOR_TYPE, &majorType);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  // Define the output type.
  hr = wmf::MFCreateMediaType(&type);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  hr = type->SetGUID(MF_MT_MAJOR_TYPE, majorType);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  hr = type->SetGUID(MF_MT_SUBTYPE, aOutputSubType);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  // Set the uncompressed format. This can fail if the decoder can't produce
  // that type.
  return aReader->SetCurrentMediaType(aStreamIndex, NULL, type);
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

static HRESULT
GetDefaultStride(IMFMediaType *aType, uint32_t* aOutStride)
{
  // Try to get the default stride from the media type.
  HRESULT hr = aType->GetUINT32(MF_MT_DEFAULT_STRIDE, aOutStride);
  if (SUCCEEDED(hr)) {
    return S_OK;
  }

  // Stride attribute not set, calculate it.
  GUID subtype = GUID_NULL;
  uint32_t width = 0;
  uint32_t height = 0;

  hr = aType->GetGUID(MF_MT_SUBTYPE, &subtype);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  hr = MFGetAttributeSize(aType, MF_MT_FRAME_SIZE, &width, &height);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  hr = wmf::MFGetStrideForBitmapInfoHeader(subtype.Data1, width, (LONG*)(aOutStride));
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  return hr;
}

void
WMFReader::ConfigureVideoDecoder()
{
  NS_ASSERTION(mSourceReader, "Must have a SourceReader before configuring decoders!");

  // Determine if we have video.
  if (!mSourceReader ||
      !SourceReaderHasStream(mSourceReader, MF_SOURCE_READER_FIRST_VIDEO_STREAM)) {
    return;
  }

  static const GUID MP4VideoTypes[] = {
    MFVideoFormat_H264
  };
  HRESULT hr = ConfigureSourceReaderStream(mSourceReader,
                                           MF_SOURCE_READER_FIRST_VIDEO_STREAM,
                                           MFVideoFormat_YV12,
                                           MP4VideoTypes,
                                           NS_ARRAY_LENGTH(MP4VideoTypes));
  if (FAILED(hr)) {
    LOG("Failed to configured video output for MFVideoFormat_YV12");
    return;
  }

  IMFMediaTypePtr mediaType;
  hr = mSourceReader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM,
                                          &mediaType);
  if (FAILED(hr)) {
    NS_WARNING("Failed to get configured video media type");
    return;
  }

  if (FAILED(MFGetAttributeSize(mediaType, MF_MT_FRAME_SIZE, &mVideoWidth, &mVideoHeight))) {
    NS_WARNING("WMF video decoder failed to get frame dimensions!");
    return;
  }

  LOG("Video frame %u x %u", mVideoWidth, mVideoHeight);
  uint32_t aspectNum = 0, aspectDenom = 0;
  if (FAILED(MFGetAttributeRatio(mediaType,
                                 MF_MT_PIXEL_ASPECT_RATIO,
                                 &aspectNum,
                                 &aspectDenom))) {
    NS_WARNING("WMF video decoder failed to get pixel aspect ratio!");
    return;
  }
  LOG("Video aspect ratio %u x %u", aspectNum, aspectDenom);

  GetDefaultStride(mediaType, &mVideoStride);

  // Calculate and validate the frame size.
  nsIntSize frameSize = nsIntSize(mVideoWidth, mVideoHeight);
  nsIntRect pictureRegion = nsIntRect(0, 0, mVideoWidth, mVideoHeight);
  nsIntSize displaySize = frameSize;
  ScaleDisplayByAspectRatio(displaySize, float(aspectNum)/float(aspectDenom));
  if (!VideoInfo::ValidateVideoRegion(frameSize, pictureRegion, displaySize)) {
    // Video track's frame sizes will overflow. Ignore the video track.
    return;
  }
  mInfo.mDisplay = displaySize;

  LOG("Successfully configured video stream");

  mHasVideo = mInfo.mHasVideo = true;
}

void
WMFReader::ConfigureAudioDecoder()
{
  NS_ASSERTION(mSourceReader, "Must have a SourceReader before configuring decoders!");

  if (!mSourceReader ||
      !SourceReaderHasStream(mSourceReader, MF_SOURCE_READER_FIRST_AUDIO_STREAM)) {
    // No audio stream.
    return;
  }

  static const GUID MP4AudioTypes[] = {
    MFAudioFormat_AAC,
    MFAudioFormat_MP3
  };
  if (FAILED(ConfigureSourceReaderStream(mSourceReader,
                                         MF_SOURCE_READER_FIRST_AUDIO_STREAM,
                                         MFAudioFormat_Float,
                                         MP4AudioTypes,
                                         NS_ARRAY_LENGTH(MP4AudioTypes)))) {
    NS_WARNING("Failed to configure WMF Audio decoder for PCM output");
    return;
  }

  IMFMediaTypePtr mediaType;
  HRESULT hr = mSourceReader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM,
                                                  &mediaType);
  if (FAILED(hr)) {
    NS_WARNING("Failed to get configured audio media type");
    return;
  }

  mAudioRate = MFGetAttributeUINT32(mediaType, MF_MT_AUDIO_SAMPLES_PER_SECOND, 0);
  mAudioChannels = MFGetAttributeUINT32(mediaType, MF_MT_AUDIO_NUM_CHANNELS, 0);
  mAudioBytesPerSample = MFGetAttributeUINT32(mediaType, MF_MT_AUDIO_BITS_PER_SAMPLE, 16) / 8;

  mInfo.mAudioChannels = mAudioChannels;
  mInfo.mAudioRate = mAudioRate;
  mHasAudio = mInfo.mHasAudio = true;

  LOG("Successfully configured audio stream. rate=%u channels=%u bitsPerSample=%u",
      mAudioRate, mAudioChannels, mAudioBytesPerSample);
}

nsresult
WMFReader::ReadMetadata(VideoInfo* aInfo,
                        MetadataTags** aTags)
{
  NS_ASSERTION(mDecoder->OnDecodeThread(), "Should be on decode thread.");

  LOG("WMFReader::ReadMetadata()");
  HRESULT hr;

  hr = wmf::MFCreateSourceReaderFromByteStream(mByteStream, NULL, &mSourceReader);
  NS_ENSURE_TRUE(SUCCEEDED(hr), NS_ERROR_FAILURE);

  ConfigureVideoDecoder();
  ConfigureAudioDecoder();

  // Abort if both video and audio failed to initialize.
  NS_ENSURE_TRUE(mInfo.mHasAudio || mInfo.mHasVideo, NS_ERROR_FAILURE);

  int64_t duration = 0;
  if (SUCCEEDED(GetSourceReaderDuration(mSourceReader, duration))) {
    ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
    mDecoder->SetMediaDuration(duration);
  }

  hr = GetSourceReaderCanSeek(mSourceReader, mCanSeek);
  NS_ASSERTION(SUCCEEDED(hr), "Can't determine if resource is seekable");

  *aInfo = mInfo;
  *aTags = nullptr;
  // aTags can be retrieved using techniques like used here:
  // http://blogs.msdn.com/b/mf/archive/2010/01/12/mfmediapropdump.aspx

  return NS_OK;
}

static int64_t
GetSampleDuration(IMFSample* aSample)
{
  int64_t duration = 0;
  aSample->GetSampleDuration(&duration);
  return HNsToUsecs(duration);
}

bool
WMFReader::DecodeAudioData()
{
  NS_ASSERTION(mDecoder->OnDecodeThread(), "Should be on decode thread.");

  DWORD flags;
  LONGLONG timestampHns;
  HRESULT hr;

  IMFSamplePtr sample;
  hr = mSourceReader->ReadSample(MF_SOURCE_READER_FIRST_AUDIO_STREAM,
                                 0, // control flags
                                 nullptr, // read stream index
                                 &flags,
                                 &timestampHns,
                                 &sample);

  if (FAILED(hr) ||
      (flags & MF_SOURCE_READERF_ERROR) ||
      (flags & MF_SOURCE_READERF_ENDOFSTREAM)) {
    // End of stream.
    mAudioQueue.Finish();
    return false;
  }

  IMFMediaBufferPtr buffer;
  hr = sample->ConvertToContiguousBuffer(&buffer);
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

  int64_t offset = mDecoder->GetResource()->Tell();
  int64_t timestamp = HNsToUsecs(timestampHns);
  int64_t duration = GetSampleDuration(sample);

  mAudioQueue.Push(new AudioData(offset,
                                 timestamp,
                                 duration,
                                 numFrames,
                                 pcmSamples.forget(),
                                 mAudioChannels));

  #ifdef LOG_SAMPLE_DECODE
  LOG("Decoded audio sample! timestamp=%lld duration=%lld currentLength=%u",
      timestamp, duration, currentLength);
  #endif

  return true;
}

bool
WMFReader::DecodeVideoFrame(bool &aKeyframeSkip,
                            int64_t aTimeThreshold)
{
  NS_ASSERTION(mDecoder->OnDecodeThread(), "Should be on decode thread.");

  // Record number of frames decoded and parsed. Automatically update the
  // stats counters using the AutoNotifyDecoded stack-based class.
  uint32_t parsed = 0, decoded = 0;
  AbstractMediaDecoder::AutoNotifyDecoded autoNotify(mDecoder, parsed, decoded);

  DWORD flags;
  LONGLONG timestampHns;
  HRESULT hr;

  IMFSamplePtr sample;
  hr = mSourceReader->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM,
                                 0, // control flags
                                 nullptr, // read stream index
                                 &flags,
                                 &timestampHns,
                                 &sample);
  if (flags & MF_SOURCE_READERF_ERROR) {
    NS_WARNING("WMFReader: Catastrophic failure reading video sample");
    // Future ReadSample() calls will fail, so give up and report end of stream.
    mVideoQueue.Finish();
    return false;
  }

  if (FAILED(hr)) {
    // Unknown failure, ask caller to try again?
    return true;
  }

  if (!sample) {
    if ((flags & MF_SOURCE_READERF_ENDOFSTREAM)) {
      LOG("WMFReader; Null sample after video decode, at end of stream");
      // End of stream.
      mVideoQueue.Finish();
      return false;
    }
    LOG("WMFReader; Null sample after video decode. Maybe insufficient data...");
    return true;
  }

  int64_t timestamp = HNsToUsecs(timestampHns);
  if (timestamp < aTimeThreshold) {
    return true;
  }
  int64_t offset = mDecoder->GetResource()->Tell();
  int64_t duration = GetSampleDuration(sample);

  IMFMediaBufferPtr buffer;

  // Must convert to contiguous buffer to use IMD2DBuffer interface.
  hr = sample->ConvertToContiguousBuffer(&buffer);
  if (FAILED(hr)) {
    NS_WARNING("ConvertToContiguousBuffer() failed!");
    return true;
  }

  // Try and use the IMF2DBuffer interface if available, otherwise fallback
  // to the IMFMediaBuffer interface. Apparently IMF2DBuffer is more efficient,
  // but only some systems (Windows 8?) support it.
  BYTE* data = nullptr;
  LONG stride = 0;
  IMF2DBufferPtr twoDBuffer = buffer;
  if (twoDBuffer) {
    hr = twoDBuffer->Lock2D(&data, &stride);
    NS_ENSURE_TRUE(SUCCEEDED(hr), false);
  } else {
    hr = buffer->Lock(&data, NULL, NULL);
    NS_ENSURE_TRUE(SUCCEEDED(hr), false);
    stride = mVideoStride;
  }

  // YV12, planar format: [YYYY....][VVVV....][UUUU....]
  // i.e., Y, then V, then U.
  VideoData::YCbCrBuffer b;

  // Y (Y') plane
  b.mPlanes[0].mData = data;
  b.mPlanes[0].mStride = stride;
  b.mPlanes[0].mHeight = mVideoHeight;
  b.mPlanes[0].mWidth = stride;
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

  // U plane (Cb)
  b.mPlanes[1].mData = data + y_size + v_size;
  b.mPlanes[1].mStride = halfStride;
  b.mPlanes[1].mHeight = halfHeight;
  b.mPlanes[1].mWidth = halfStride;
  b.mPlanes[1].mOffset = 0;
  b.mPlanes[1].mSkip = 0;

  // V plane (Cr)
  b.mPlanes[2].mData = data + y_size;
  b.mPlanes[2].mStride = halfStride;
  b.mPlanes[2].mHeight = halfHeight;
  b.mPlanes[2].mWidth = halfStride;
  b.mPlanes[2].mOffset = 0;
  b.mPlanes[2].mSkip = 0;

  VideoData *v = VideoData::Create(mInfo,
                                   mDecoder->GetImageContainer(),
                                   offset,
                                   timestamp,
                                   timestamp + duration,
                                   b,
                                   false,
                                   -1,
                                   nsIntRect(0, 0, mVideoWidth, mVideoHeight));
  if (twoDBuffer) {
    twoDBuffer->Unlock2D();
  } else {
    buffer->Unlock();
  }

  if (!v) {
    NS_WARNING("Failed to create VideoData");
    return false;
  }
  parsed++;
  decoded++;
  mVideoQueue.Push(v);

  #ifdef LOG_SAMPLE_DECODE
  LOG("Decoded video sample timestamp=%lld duration=%lld stride=%d width=%u height=%u flags=%u",
      timestamp, duration, stride, mVideoWidth, mVideoHeight, flags);
  #endif

  if ((flags & MF_SOURCE_READERF_ENDOFSTREAM)) {
    // End of stream.
    mVideoQueue.Finish();
    LOG("End of video stream");
    return false;
  }

  return true;
}

nsresult
WMFReader::Seek(int64_t aTargetUs,
                int64_t aStartTime,
                int64_t aEndTime,
                int64_t aCurrentTime)
{
  LOG("WMFReader::Seek() %lld", aTargetUs);

  NS_ASSERTION(mDecoder->OnDecodeThread(), "Should be on decode thread.");
  if (!mCanSeek) {
    return NS_ERROR_FAILURE;
  }

  nsresult rv = ResetDecode();
  NS_ENSURE_SUCCESS(rv, rv);

  AutoPropVar var;
  HRESULT hr = InitPropVariantFromInt64(UsecsToHNs(aTargetUs), &var);
  NS_ENSURE_TRUE(SUCCEEDED(hr), NS_ERROR_FAILURE);

  hr = mSourceReader->SetCurrentPosition(GUID_NULL, var);
  NS_ENSURE_TRUE(SUCCEEDED(hr), NS_ERROR_FAILURE);

  return DecodeToTarget(aTargetUs);
}

nsresult
WMFReader::GetBuffered(nsTimeRanges* aBuffered, int64_t aStartTime)
{
  MediaResource* stream = mDecoder->GetResource();
  int64_t durationUs = 0;
  {
    ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
    durationUs = mDecoder->GetMediaDuration();
  }
  GetEstimatedBufferedTimeRanges(stream, durationUs, aBuffered);
  return NS_OK;
}

} // namespace mozilla
