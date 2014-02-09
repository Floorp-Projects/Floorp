/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DirectShowReader.h"
#include "MediaDecoderReader.h"
#include "mozilla/RefPtr.h"
#include "dshow.h"
#include "AudioSinkFilter.h"
#include "SourceFilter.h"
#include "DirectShowUtils.h"
#include "SampleSink.h"
#include "MediaResource.h"
#include "VideoUtils.h"

namespace mozilla {


#ifdef PR_LOGGING

PRLogModuleInfo*
GetDirectShowLog() {
  static PRLogModuleInfo* log = nullptr;
  if (!log) {
    log = PR_NewLogModule("DirectShowDecoder");
  }
  return log;
}

#define LOG(...) PR_LOG(GetDirectShowLog(), PR_LOG_DEBUG, (__VA_ARGS__))

#else
#define LOG(...)
#endif

DirectShowReader::DirectShowReader(AbstractMediaDecoder* aDecoder)
  : MediaDecoderReader(aDecoder),
    mMP3FrameParser(aDecoder->GetResource()->GetLength()),
#ifdef DEBUG
    mRotRegister(0),
#endif
    mNumChannels(0),
    mAudioRate(0),
    mBytesPerSample(0),
    mDuration(0)
{
  MOZ_ASSERT(NS_IsMainThread(), "Must be on main thread.");
  MOZ_COUNT_CTOR(DirectShowReader);
}

DirectShowReader::~DirectShowReader()
{
  MOZ_ASSERT(NS_IsMainThread(), "Must be on main thread.");
  MOZ_COUNT_DTOR(DirectShowReader);
#ifdef DEBUG
  if (mRotRegister) {
    RemoveGraphFromRunningObjectTable(mRotRegister);
  }
#endif
}

nsresult
DirectShowReader::Init(MediaDecoderReader* aCloneDonor)
{
  MOZ_ASSERT(NS_IsMainThread(), "Must be on main thread.");
  return NS_OK;
}

// Try to parse the MP3 stream to make sure this is indeed an MP3, get the
// estimated duration of the stream, and find the offset of the actual MP3
// frames in the stream, as DirectShow doesn't like large ID3 sections.
static nsresult
ParseMP3Headers(MP3FrameParser *aParser, MediaResource *aResource)
{
  const uint32_t MAX_READ_SIZE = 4096;

  uint64_t offset = 0;
  while (aParser->NeedsData() && !aParser->ParsedHeaders()) {
    uint32_t bytesRead;
    char buffer[MAX_READ_SIZE];
    nsresult rv = aResource->ReadAt(offset, buffer,
                                    MAX_READ_SIZE, &bytesRead);
    NS_ENSURE_SUCCESS(rv, rv);

    aParser->Parse(buffer, bytesRead, offset);
    offset += bytesRead;
  }

  return aParser->IsMP3() ? NS_OK : NS_ERROR_FAILURE;
}

// Windows XP's MP3 decoder filter. This is available on XP only, on Vista
// and later we can use the DMO Wrapper filter and MP3 decoder DMO.
static const GUID CLSID_MPEG_LAYER_3_DECODER_FILTER =
{ 0x38BE3000, 0xDBF4, 0x11D0, 0x86, 0x0E, 0x00, 0xA0, 0x24, 0xCF, 0xEF, 0x6D };

nsresult
DirectShowReader::ReadMetadata(MediaInfo* aInfo,
                               MetadataTags** aTags)
{
  MOZ_ASSERT(mDecoder->OnDecodeThread(), "Should be on decode thread.");
  HRESULT hr;
  nsresult rv;

  // Create the filter graph, reference it by the GraphBuilder interface,
  // to make graph building more convenient.
  hr = CoCreateInstance(CLSID_FilterGraph,
                        nullptr,
                        CLSCTX_INPROC_SERVER,
                        IID_IGraphBuilder,
                        reinterpret_cast<void**>(static_cast<IGraphBuilder**>(byRef(mGraph))));
  NS_ENSURE_TRUE(SUCCEEDED(hr) && mGraph, NS_ERROR_FAILURE);

  rv = ParseMP3Headers(&mMP3FrameParser, mDecoder->GetResource());
  NS_ENSURE_SUCCESS(rv, rv);

  #ifdef DEBUG
  // Add the graph to the Running Object Table so that we can connect
  // to this graph with GraphEdit/GraphStudio. Note: on Vista and up you must
  // also regsvr32 proppage.dll from the Windows SDK.
  // See: http://msdn.microsoft.com/en-us/library/ms787252(VS.85).aspx
  hr = AddGraphToRunningObjectTable(mGraph, &mRotRegister);
  NS_ENSURE_TRUE(SUCCEEDED(hr), NS_ERROR_FAILURE);
  #endif

  // Extract the interface pointers we'll need from the filter graph.
  hr = mGraph->QueryInterface(static_cast<IMediaControl**>(byRef(mControl)));
  NS_ENSURE_TRUE(SUCCEEDED(hr) && mControl, NS_ERROR_FAILURE);

  hr = mGraph->QueryInterface(static_cast<IMediaSeeking**>(byRef(mMediaSeeking)));
  NS_ENSURE_TRUE(SUCCEEDED(hr) && mMediaSeeking, NS_ERROR_FAILURE);

  // Build the graph. Create the filters we need, and connect them. We
  // build the entire graph ourselves to prevent other decoders installed
  // on the system being created and used.

  // Our source filters, wraps the MediaResource.
  mSourceFilter = new SourceFilter(MEDIATYPE_Stream, MEDIASUBTYPE_MPEG1Audio);
  NS_ENSURE_TRUE(mSourceFilter, NS_ERROR_FAILURE);

  rv = mSourceFilter->Init(mDecoder->GetResource(), mMP3FrameParser.GetMP3Offset());
  NS_ENSURE_SUCCESS(rv, rv);

  hr = mGraph->AddFilter(mSourceFilter, L"MozillaDirectShowSource");
  NS_ENSURE_TRUE(SUCCEEDED(hr), NS_ERROR_FAILURE);

  // The MPEG demuxer.
  RefPtr<IBaseFilter> demuxer;
  hr = CreateAndAddFilter(mGraph,
                          CLSID_MPEG1Splitter,
                          L"MPEG1Splitter",
                          byRef(demuxer));
  NS_ENSURE_TRUE(SUCCEEDED(hr), NS_ERROR_FAILURE);

  // Platform MP3 decoder.
  RefPtr<IBaseFilter> decoder;
  // Firstly try to create the MP3 decoder filter that ships with WinXP
  // directly. This filter doesn't normally exist on later versions of
  // Windows.
  hr = CreateAndAddFilter(mGraph,
                          CLSID_MPEG_LAYER_3_DECODER_FILTER,
                          L"MPEG Layer 3 Decoder",
                          byRef(decoder));
  if (FAILED(hr)) {
    // Failed to create MP3 decoder filter. Try to instantiate
    // the MP3 decoder DMO.
    hr = AddMP3DMOWrapperFilter(mGraph, byRef(decoder));
    NS_ENSURE_TRUE(SUCCEEDED(hr), NS_ERROR_FAILURE);
  }

  // Sink, captures audio samples and inserts them into our pipeline.
  static const wchar_t* AudioSinkFilterName = L"MozAudioSinkFilter";
  mAudioSinkFilter = new AudioSinkFilter(AudioSinkFilterName, &hr);
  NS_ENSURE_TRUE(mAudioSinkFilter && SUCCEEDED(hr), NS_ERROR_FAILURE);
  hr = mGraph->AddFilter(mAudioSinkFilter, AudioSinkFilterName);
  NS_ENSURE_TRUE(SUCCEEDED(hr), NS_ERROR_FAILURE);

  // Join the filters.
  hr = ConnectFilters(mGraph, mSourceFilter, demuxer);
  NS_ENSURE_TRUE(SUCCEEDED(hr), NS_ERROR_FAILURE);

  hr = ConnectFilters(mGraph, demuxer, decoder);
  NS_ENSURE_TRUE(SUCCEEDED(hr), NS_ERROR_FAILURE);

  hr = ConnectFilters(mGraph, decoder, mAudioSinkFilter);
  NS_ENSURE_TRUE(SUCCEEDED(hr), NS_ERROR_FAILURE);

  WAVEFORMATEX format;
  mAudioSinkFilter->GetSampleSink()->GetAudioFormat(&format);
  NS_ENSURE_TRUE(format.wFormatTag == WAVE_FORMAT_PCM, NS_ERROR_FAILURE);

  mInfo.mAudio.mChannels = mNumChannels = format.nChannels;
  mInfo.mAudio.mRate = mAudioRate = format.nSamplesPerSec;
  mBytesPerSample = format.wBitsPerSample / 8;
  mInfo.mAudio.mHasAudio = true;

  *aInfo = mInfo;
  // Note: The SourceFilter strips ID3v2 tags out of the stream.
  *aTags = nullptr;

  // Begin decoding!
  hr = mControl->Run();
  NS_ENSURE_TRUE(SUCCEEDED(hr), NS_ERROR_FAILURE);

  DWORD seekCaps = 0;
  hr = mMediaSeeking->GetCapabilities(&seekCaps);
  bool canSeek = ((AM_SEEKING_CanSeekAbsolute & seekCaps) == AM_SEEKING_CanSeekAbsolute);
  if (!canSeek) {
    mDecoder->SetMediaSeekable(false);
  }

  int64_t duration = mMP3FrameParser.GetDuration();
  if (SUCCEEDED(hr)) {
    ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
    mDecoder->SetMediaDuration(duration);
  }

  LOG("Successfully initialized DirectShow MP3 decoder.");
  LOG("Channels=%u Hz=%u duration=%lld bytesPerSample=%d",
      mInfo.mAudio.mChannels,
      mInfo.mAudio.mRate,
      RefTimeToUsecs(duration),
      mBytesPerSample);

  return NS_OK;
}

inline float
UnsignedByteToAudioSample(uint8_t aValue)
{
  return aValue * (2.0f / UINT8_MAX) - 1.0f;
}

bool
DirectShowReader::Finish(HRESULT aStatus)
{
  MOZ_ASSERT(mDecoder->OnDecodeThread(), "Should be on decode thread.");

  LOG("DirectShowReader::Finish(0x%x)", aStatus);
  // Notify the filter graph of end of stream.
  RefPtr<IMediaEventSink> eventSink;
  HRESULT hr = mGraph->QueryInterface(static_cast<IMediaEventSink**>(byRef(eventSink)));
  if (SUCCEEDED(hr) && eventSink) {
    eventSink->Notify(EC_COMPLETE, aStatus, 0);
  }
  return false;
}

class DirectShowCopy
{
public:
  DirectShowCopy(uint8_t *aSource, uint32_t aBytesPerSample,
                 uint32_t aSamples, uint32_t aChannels)
    : mSource(aSource)
    , mBytesPerSample(aBytesPerSample)
    , mSamples(aSamples)
    , mChannels(aChannels)
    , mNextSample(0)
  { }

  uint32_t operator()(AudioDataValue *aBuffer, uint32_t aSamples)
  {
    uint32_t maxSamples = std::min(aSamples, mSamples - mNextSample);
    uint32_t frames = maxSamples / mChannels;
    size_t byteOffset = mNextSample * mBytesPerSample;
    if (mBytesPerSample == 1) {
      for (uint32_t i = 0; i < maxSamples; ++i) {
        uint8_t *sample = mSource + byteOffset;
        aBuffer[i] = UnsignedByteToAudioSample(*sample);
        byteOffset += mBytesPerSample;
      }
    } else if (mBytesPerSample == 2) {
      for (uint32_t i = 0; i < maxSamples; ++i) {
        int16_t *sample = reinterpret_cast<int16_t *>(mSource + byteOffset);
        aBuffer[i] = AudioSampleToFloat(*sample);
        byteOffset += mBytesPerSample;
      }
    }
    mNextSample += maxSamples;
    return frames;
  }

private:
  uint8_t * const mSource;
  const uint32_t mBytesPerSample;
  const uint32_t mSamples;
  const uint32_t mChannels;
  uint32_t mNextSample;
};

bool
DirectShowReader::DecodeAudioData()
{
  MOZ_ASSERT(mDecoder->OnDecodeThread(), "Should be on decode thread.");
  HRESULT hr;

  SampleSink* sink = mAudioSinkFilter->GetSampleSink();
  if (sink->AtEOS()) {
    // End of stream.
    return Finish(S_OK);
  }

  // Get the next chunk of audio samples. This blocks until the sample
  // arrives, or an error occurs (like the stream is shutdown).
  RefPtr<IMediaSample> sample;
  hr = sink->Extract(sample);
  if (FAILED(hr) || hr == S_FALSE) {
    return Finish(hr);
  }

  int64_t start = 0, end = 0;
  sample->GetMediaTime(&start, &end);
  LOG("DirectShowReader::DecodeAudioData [%4.2lf-%4.2lf]",
      RefTimeToSeconds(start),
      RefTimeToSeconds(end));

  LONG length = sample->GetActualDataLength();
  LONG numSamples = length / mBytesPerSample;
  LONG numFrames = length / mBytesPerSample / mNumChannels;

  BYTE* data = nullptr;
  hr = sample->GetPointer(&data);
  NS_ENSURE_TRUE(SUCCEEDED(hr), Finish(hr));

  mAudioCompactor.Push(mDecoder->GetResource()->Tell(),
                       RefTimeToUsecs(start),
                       mInfo.mAudio.mRate,
                       numFrames,
                       mNumChannels,
                       DirectShowCopy(reinterpret_cast<uint8_t *>(data),
                                      mBytesPerSample,
                                      numSamples,
                                      mNumChannels));
  return true;
}

bool
DirectShowReader::DecodeVideoFrame(bool &aKeyframeSkip,
                                   int64_t aTimeThreshold)
{
  MOZ_ASSERT(mDecoder->OnDecodeThread(), "Should be on decode thread.");
  return false;
}

bool
DirectShowReader::HasAudio()
{
  MOZ_ASSERT(mDecoder->OnDecodeThread(), "Should be on decode thread.");
  return true;
}

bool
DirectShowReader::HasVideo()
{
  MOZ_ASSERT(mDecoder->OnDecodeThread(), "Should be on decode thread.");
  return false;
}

nsresult
DirectShowReader::Seek(int64_t aTargetUs,
                       int64_t aStartTime,
                       int64_t aEndTime,
                       int64_t aCurrentTime)
{
  HRESULT hr;
  MOZ_ASSERT(mDecoder->OnDecodeThread(), "Should be on decode thread.");\

  LOG("DirectShowReader::Seek() target=%lld", aTargetUs);

  hr = mControl->Pause();
  NS_ENSURE_TRUE(SUCCEEDED(hr), NS_ERROR_FAILURE);

  nsresult rv = ResetDecode();
  NS_ENSURE_SUCCESS(rv, rv);

  LONGLONG seekPosition = UsecsToRefTime(aTargetUs);
  hr = mMediaSeeking->SetPositions(&seekPosition,
                                   AM_SEEKING_AbsolutePositioning,
                                   nullptr,
                                   AM_SEEKING_NoPositioning);
  NS_ENSURE_TRUE(SUCCEEDED(hr), NS_ERROR_FAILURE);

  hr = mControl->Run();
  NS_ENSURE_TRUE(SUCCEEDED(hr), NS_ERROR_FAILURE);

  return DecodeToTarget(aTargetUs);
}

void
DirectShowReader::OnDecodeThreadStart()
{
  MOZ_ASSERT(mDecoder->OnDecodeThread(), "Should be on decode thread.");
  HRESULT hr = CoInitializeEx(0, COINIT_MULTITHREADED);
  NS_ENSURE_TRUE_VOID(SUCCEEDED(hr));
}

void
DirectShowReader::OnDecodeThreadFinish()
{
  NS_ASSERTION(mDecoder->OnDecodeThread(), "Should be on decode thread.");
  CoUninitialize();
}

void
DirectShowReader::NotifyDataArrived(const char* aBuffer, uint32_t aLength, int64_t aOffset)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (!mMP3FrameParser.IsMP3()) {
    return;
  }
  mMP3FrameParser.Parse(aBuffer, aLength, aOffset);
  int64_t duration = mMP3FrameParser.GetDuration();
  if (duration != mDuration) {
    mDuration = duration;
    MOZ_ASSERT(mDecoder);
    ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
    mDecoder->UpdateEstimatedMediaDuration(mDuration);
  }
}

} // namespace mozilla
