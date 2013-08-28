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
#include "mozilla/dom/TimeRanges.h"
#include "mozilla/Preferences.h"
#include "DXVA2Manager.h"
#include "ImageContainer.h"
#include "Layers.h"
#include "mozilla/layers/LayersTypes.h"
#include "WinUtils.h"

using namespace mozilla::widget;

#ifndef MOZ_SAMPLE_TYPE_FLOAT32
#error We expect 32bit float audio samples on desktop for the Windows Media Foundation media backend.
#endif

#include "MediaDecoder.h"
#include "VideoUtils.h"

using mozilla::layers::Image;
using mozilla::layers::LayerManager;
using mozilla::layers::LayersBackend;

namespace mozilla {

#ifdef PR_LOGGING
extern PRLogModuleInfo* gMediaDecoderLog;
#define LOG(...) PR_LOG(gMediaDecoderLog, PR_LOG_DEBUG, (__VA_ARGS__))
#else
#define LOG(...)
#endif

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
    mAudioFrameSum(0),
    mAudioFrameOffset(0),
    mHasAudio(false),
    mHasVideo(false),
    mUseHwAccel(false),
    mMustRecaptureAudioPosition(true),
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

void
WMFReader::OnDecodeThreadStart()
{
  NS_ASSERTION(mDecoder->OnDecodeThread(), "Should be on decode thread.");

  // XXX WebAudio will call this on the main thread so CoInit will definitely
  // fail. You cannot change the concurrency model once already set.
  // The main thread will continue to be STA, which seems to work, but MSDN
  // recommends that MTA be used.
  mCOMInitialized = SUCCEEDED(CoInitializeEx(0, COINIT_MULTITHREADED));
  NS_ENSURE_TRUE_VOID(mCOMInitialized);
}

void
WMFReader::OnDecodeThreadFinish()
{
  NS_ASSERTION(mDecoder->OnDecodeThread(), "Should be on decode thread.");
  if (mCOMInitialized) {
    CoUninitialize();
  }
}

bool
WMFReader::InitializeDXVA()
{
  if (!Preferences::GetBool("media.windows-media-foundation.use-dxva", false) ||
      WinUtils::GetWindowsVersion() == WinUtils::VISTA_VERSION) {
    // Don't use DXVA on Vista until bug 901944's fix has time to ride
    // ride the release train.
    return false;
  }

  // Extract the layer manager backend type so that we can determine
  // whether it's worthwhile using DXVA. If we're not running with a D3D
  // layer manager then the readback of decoded video frames from GPU to
  // CPU memory grinds painting to a halt, and makes playback performance
  // *worse*.
  MediaDecoderOwner* owner = mDecoder->GetOwner();
  NS_ENSURE_TRUE(owner, false);

  HTMLMediaElement* element = owner->GetMediaElement();
  NS_ENSURE_TRUE(element, false);

  nsIDocument* doc = element->GetOwnerDocument();
  NS_ENSURE_TRUE(doc, false);

  nsRefPtr<LayerManager> layerManager = nsContentUtils::LayerManagerForDocument(doc);
  NS_ENSURE_TRUE(layerManager, false);

  if (layerManager->GetBackendType() != LayersBackend::LAYERS_D3D9 &&
      layerManager->GetBackendType() != LayersBackend::LAYERS_D3D10) {
    return false;
  }

  mDXVA2Manager = DXVA2Manager::Create();

  return mDXVA2Manager != nullptr;
}

static bool
IsVideoContentType(const nsCString& aContentType)
{
  NS_NAMED_LITERAL_CSTRING(video, "video");
  if (FindInReadable(video, aContentType)) {
    return true;
  }
  return false;
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

  if (IsVideoContentType(mDecoder->GetResource()->GetContentType())) {
    mUseHwAccel = InitializeDXVA();
  } else {
    mUseHwAccel = false;
  }

  return NS_OK;
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
    LOG("ConfigureSourceReaderStream subType=%s is not allowed to be decoded", name.get());
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

static int32_t
MFOffsetToInt32(const MFOffset& aOffset)
{
  return int32_t(aOffset.value + (aOffset.fract / 65536.0f));
}

// Gets the sub-region of the video frame that should be displayed.
// See: http://msdn.microsoft.com/en-us/library/windows/desktop/bb530115(v=vs.85).aspx
static HRESULT
GetPictureRegion(IMFMediaType* aMediaType, nsIntRect& aOutPictureRegion)
{
  // Determine if "pan and scan" is enabled for this media. If it is, we
  // only display a region of the video frame, not the entire frame.
  BOOL panScan = MFGetAttributeUINT32(aMediaType, MF_MT_PAN_SCAN_ENABLED, FALSE);

  // If pan and scan mode is enabled. Try to get the display region.
  HRESULT hr = E_FAIL;
  MFVideoArea videoArea;
  memset(&videoArea, 0, sizeof(MFVideoArea));
  if (panScan) {
    hr = aMediaType->GetBlob(MF_MT_PAN_SCAN_APERTURE,
                             (UINT8*)&videoArea,
                             sizeof(MFVideoArea),
                             NULL);
  }

  // If we're not in pan-and-scan mode, or the pan-and-scan region is not set,
  // check for a minimimum display aperture.
  if (!panScan || hr == MF_E_ATTRIBUTENOTFOUND) {
    hr = aMediaType->GetBlob(MF_MT_MINIMUM_DISPLAY_APERTURE,
                             (UINT8*)&videoArea,
                             sizeof(MFVideoArea),
                             NULL);
  }

  if (hr == MF_E_ATTRIBUTENOTFOUND) {
    // Minimum display aperture is not set, for "backward compatibility with
    // some components", check for a geometric aperture.
    hr = aMediaType->GetBlob(MF_MT_GEOMETRIC_APERTURE,
                             (UINT8*)&videoArea,
                             sizeof(MFVideoArea),
                             NULL);
  }

  if (SUCCEEDED(hr)) {
    // The media specified a picture region, return it.
    aOutPictureRegion = nsIntRect(MFOffsetToInt32(videoArea.OffsetX),
                                  MFOffsetToInt32(videoArea.OffsetY),
                                  videoArea.Area.cx,
                                  videoArea.Area.cy);
    return S_OK;
  }

  // No picture region defined, fall back to using the entire video area.
  UINT32 width = 0, height = 0;
  hr = MFGetAttributeSize(aMediaType, MF_MT_FRAME_SIZE, &width, &height);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);
  aOutPictureRegion = nsIntRect(0, 0, width, height);
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
  if (!VideoInfo::ValidateVideoRegion(frameSize, pictureRegion, displaySize)) {
    // Video track's frame sizes will overflow. Ignore the video track.
    return E_FAIL;
  }

  // Success! Save state.
  mInfo.mDisplay = displaySize;
  GetDefaultStride(aMediaType, &mVideoStride);
  mVideoWidth = width;
  mVideoHeight = height;
  mPictureRegion = pictureRegion;

  LOG("WMFReader frame geometry frame=(%u,%u) stride=%u picture=(%d, %d, %d, %d) display=(%d,%d) PAR=%d:%d",
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

  static const GUID MP4VideoTypes[] = {
    MFVideoFormat_H264
  };
  HRESULT hr = ConfigureSourceReaderStream(mSourceReader,
                                           MF_SOURCE_READER_FIRST_VIDEO_STREAM,
                                           mUseHwAccel ? MFVideoFormat_NV12 : MFVideoFormat_YV12,
                                           MP4VideoTypes,
                                           NS_ARRAY_LENGTH(MP4VideoTypes));
  if (FAILED(hr)) {
    LOG("Failed to configured video output");
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

  LOG("Successfully configured video stream");

  mHasVideo = mInfo.mHasVideo = true;

  return S_OK;
}

void
WMFReader::GetSupportedAudioCodecs(const GUID** aCodecs, uint32_t* aNumCodecs)
{
  MOZ_ASSERT(aCodecs);
  MOZ_ASSERT(aNumCodecs);

  if (mIsMP3Enabled) {
    static const GUID codecs[] = {
      MFAudioFormat_AAC,
      MFAudioFormat_MP3
    };
    *aCodecs = codecs;
    *aNumCodecs = NS_ARRAY_LENGTH(codecs);
  } else {
    static const GUID codecs[] = {
      MFAudioFormat_AAC
    };
    *aCodecs = codecs;
    *aNumCodecs = NS_ARRAY_LENGTH(codecs);
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

  mInfo.mAudioChannels = mAudioChannels;
  mInfo.mAudioRate = mAudioRate;
  mHasAudio = mInfo.mHasAudio = true;

  LOG("Successfully configured audio stream. rate=%u channels=%u bitsPerSample=%u",
      mAudioRate, mAudioChannels, mAudioBytesPerSample);

  return S_OK;
}

nsresult
WMFReader::ReadMetadata(VideoInfo* aInfo,
                        MetadataTags** aTags)
{
  NS_ASSERTION(mDecoder->OnDecodeThread(), "Should be on decode thread.");

  LOG("WMFReader::ReadMetadata()");
  HRESULT hr;

  RefPtr<IMFAttributes> attr;
  hr = wmf::MFCreateAttributes(byRef(attr), 1);
  NS_ENSURE_TRUE(SUCCEEDED(hr), NS_ERROR_FAILURE);

  hr = attr->SetUnknown(MF_SOURCE_READER_ASYNC_CALLBACK, mSourceReaderCallback);
  NS_ENSURE_TRUE(SUCCEEDED(hr), NS_ERROR_FAILURE);

  if (mUseHwAccel) {
    hr = attr->SetUnknown(MF_SOURCE_READER_D3D_MANAGER,
                          mDXVA2Manager->GetDXVADeviceManager());
    if (FAILED(hr)) {
      LOG("Failed to set DXVA2 D3D Device manager on source reader attributes");
      mUseHwAccel = false;
    }
  }

  hr = wmf::MFCreateSourceReaderFromByteStream(mByteStream, attr, byRef(mSourceReader));
  NS_ENSURE_TRUE(SUCCEEDED(hr), NS_ERROR_FAILURE);

  hr = ConfigureVideoDecoder();
  NS_ENSURE_TRUE(SUCCEEDED(hr), NS_ERROR_FAILURE);

  hr = ConfigureAudioDecoder();
  NS_ENSURE_TRUE(SUCCEEDED(hr), NS_ERROR_FAILURE);

  if (mUseHwAccel && mInfo.mHasVideo) {
    RefPtr<IMFTransform> videoDecoder;
    hr = mSourceReader->GetServiceForStream(MF_SOURCE_READER_FIRST_VIDEO_STREAM,
                                            GUID_NULL,
                                            IID_IMFTransform,
                                            (void**)(IMFTransform**)(byRef(videoDecoder)));

    if (SUCCEEDED(hr)) {
      ULONG_PTR manager = ULONG_PTR(mDXVA2Manager->GetDXVADeviceManager());
      hr = videoDecoder->ProcessMessage(MFT_MESSAGE_SET_D3D_MANAGER,
                                        manager);
    }
    if (FAILED(hr)) {
      LOG("Failed to set DXVA2 D3D Device manager on decoder");
      mUseHwAccel = false;
      // Re-run the configuration process, so that the output video format
      // is set correctly to reflect that hardware acceleration is disabled.
      // Without this, we'd be running with !mUseHwAccel and the output format
      // set to NV12, which is the format we expect when using hardware
      // acceleration. This would cause us to misinterpret the frame contents.
      hr = ConfigureVideoDecoder();      
    }
  }
  if (mInfo.mHasVideo) {
    LOG("Using DXVA: %s", (mUseHwAccel ? "Yes" : "No"));
  }

  // Abort if both video and audio failed to initialize.
  NS_ENSURE_TRUE(mInfo.mHasAudio || mInfo.mHasVideo, NS_ERROR_FAILURE);

  // Get the duration, and report it to the decoder if we have it.
  int64_t duration = 0;
  hr = GetSourceReaderDuration(mSourceReader, duration);
  if (SUCCEEDED(hr)) {
    ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
    mDecoder->SetMediaDuration(duration);
  }
  // We can seek if we get a duration *and* the reader reports that it's
  // seekable.
  bool canSeek = false;
  if (FAILED(hr) ||
      FAILED(GetSourceReaderCanSeek(mSourceReader, canSeek)) ||
      !canSeek) {
    mDecoder->SetMediaSeekable(false);
  }

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

HRESULT
HNsToFrames(int64_t aHNs, uint32_t aRate, int64_t* aOutFrames)
{
  MOZ_ASSERT(aOutFrames);
  const int64_t HNS_PER_S = USECS_PER_S * 10;
  CheckedInt<int64_t> i = aHNs;
  i *= aRate;
  i /= HNS_PER_S;
  NS_ENSURE_TRUE(i.isValid(), E_FAIL);
  *aOutFrames = i.value();
  return S_OK;
}

HRESULT
FramesToUsecs(int64_t aSamples, uint32_t aRate, int64_t* aOutUsecs)
{
  MOZ_ASSERT(aOutUsecs);
  CheckedInt<int64_t> i = aSamples;
  i *= USECS_PER_S;
  i /= aRate;
  NS_ENSURE_TRUE(i.isValid(), E_FAIL);
  *aOutUsecs = i.value();
  return S_OK;
}

bool
WMFReader::DecodeAudioData()
{
  NS_ASSERTION(mDecoder->OnDecodeThread(), "Should be on decode thread.");

  HRESULT hr;
  hr = mSourceReader->ReadSample(MF_SOURCE_READER_FIRST_AUDIO_STREAM,
                                 0, // control flags
                                 0, // read stream index
                                 nullptr,
                                 nullptr,
                                 nullptr);

  if (FAILED(hr)) {
    LOG("WMFReader::DecodeAudioData() ReadSample failed with hr=0x%x", hr);
    // End the stream.
    mAudioQueue.Finish();
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
    LOG("WMFReader::DecodeAudioData() ReadSample failed with hr=0x%x flags=0x%x",
        hr, flags);
    // End the stream.
    mAudioQueue.Finish();
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
                                 mAudioChannels));

  #ifdef LOG_SAMPLE_DECODE
  LOG("Decoded audio sample! timestamp=%lld duration=%lld currentLength=%u",
      timestamp, duration, currentLength);
  #endif

  NotifyBytesConsumed();

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
    hr = buffer->Lock(&data, NULL, NULL);
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

  VideoData *v = VideoData::Create(mInfo,
                                   mDecoder->GetImageContainer(),
                                   aOffsetBytes,
                                   aTimestampUsecs,
                                   aTimestampUsecs + aDurationUsecs,
                                   b,
                                   false,
                                   -1,
                                   mPictureRegion);
  if (twoDBuffer) {
    twoDBuffer->Unlock2D();
  } else {
    buffer->Unlock();
  }

  *aOutVideoData = v;

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

  VideoData *v = VideoData::CreateFromImage(mInfo,
                                            mDecoder->GetImageContainer(),
                                            aOffsetBytes,
                                            aTimestampUsecs,
                                            aTimestampUsecs + aDurationUsecs,
                                            image.forget(),
                                            false,
                                            -1,
                                            mPictureRegion);

  NS_ENSURE_TRUE(v, E_FAIL);
  *aOutVideoData = v;

  return S_OK;
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

  HRESULT hr;

  hr = mSourceReader->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM,
                                 0, // control flags
                                 0, // read stream index
                                 nullptr,
                                 nullptr,
                                 nullptr);
  if (FAILED(hr)) {
    LOG("WMFReader::DecodeVideoData() ReadSample failed with hr=0x%x", hr);
    // End the stream.
    mVideoQueue.Finish();
    return false;
  }

  DWORD flags = 0;
  LONGLONG timestampHns = 0;
  RefPtr<IMFSample> sample;
  hr = mSourceReaderCallback->Wait(&flags, &timestampHns, byRef(sample));

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
      // End the stream.
      mVideoQueue.Finish();
      return false;
    }
    LOG("WMFReader; Null sample after video decode. Maybe insufficient data...");
    return true;
  }

  if ((flags & MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED)) {
    LOG("WMFReader: Video media type changed!");
    RefPtr<IMFMediaType> mediaType;
    hr = mSourceReader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM,
                                            byRef(mediaType));
    if (FAILED(hr) ||
        FAILED(ConfigureVideoFrameGeometry(mediaType))) {
      NS_WARNING("Failed to reconfigure video media type");
      mVideoQueue.Finish();
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

  parsed++;
  decoded++;
  mVideoQueue.Push(v);

  #ifdef LOG_SAMPLE_DECODE
  LOG("Decoded video sample timestamp=%lld duration=%lld stride=%d height=%u flags=%u",
      timestamp, duration, mVideoStride, mVideoHeight, flags);
  #endif

  if ((flags & MF_SOURCE_READERF_ENDOFSTREAM)) {
    // End of stream.
    mVideoQueue.Finish();
    LOG("End of video stream");
    return false;
  }

  NotifyBytesConsumed();

  return true;
}

void
WMFReader::NotifyBytesConsumed()
{
  uint32_t bytesConsumed = mByteStream->GetAndResetBytesConsumedCount();
  if (bytesConsumed > 0) {
    mDecoder->NotifyBytesConsumed(bytesConsumed);
  }
}

nsresult
WMFReader::Seek(int64_t aTargetUs,
                int64_t aStartTime,
                int64_t aEndTime,
                int64_t aCurrentTime)
{
  LOG("WMFReader::Seek() %lld", aTargetUs);

  NS_ASSERTION(mDecoder->OnDecodeThread(), "Should be on decode thread.");
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

  return DecodeToTarget(aTargetUs);
}

nsresult
WMFReader::GetBuffered(mozilla::dom::TimeRanges* aBuffered, int64_t aStartTime)
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
