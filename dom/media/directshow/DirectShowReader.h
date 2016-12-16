/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(DirectShowReader_h_)
#define DirectShowReader_h_

#include "windows.h" // HRESULT, DWORD
#include "MediaDecoderReader.h"
#include "MediaResource.h"
#include "mozilla/RefPtr.h"
#include "MP3FrameParser.h"

// Add the graph to the Running Object Table so that we can connect
// to this graph with GraphEdit/GraphStudio. Note: on Vista and up you must
// also regsvr32 proppage.dll from the Windows SDK.
// See: http://msdn.microsoft.com/en-us/library/ms787252(VS.85).aspx
// #define DIRECTSHOW_REGISTER_GRAPH

struct IGraphBuilder;
struct IMediaControl;
struct IMediaSeeking;

namespace mozilla {

class AudioSinkFilter;
class SourceFilter;

// Decoder backend for decoding MP3 using DirectShow. DirectShow operates as
// a filter graph. The basic design of the DirectShowReader is that we have
// a SourceFilter that wraps the MediaResource that connects to the
// MP3 decoder filter. The MP3 decoder filter "pulls" data as it requires it
// downstream on its own thread. When the MP3 decoder has produced a block of
// decoded samples, its thread calls downstream into our AudioSinkFilter,
// passing the decoded buffer in. The AudioSinkFilter inserts the samples into
// a SampleSink object. The SampleSink blocks the MP3 decoder's thread until
// the decode thread calls DecodeAudioData(), whereupon the SampleSink
// releases the decoded samples to the decode thread, and unblocks the MP3
// decoder's thread. The MP3 decoder can then request more data from the
// SourceFilter, and decode more data. If the decode thread calls
// DecodeAudioData() and there's no decoded samples waiting to be extracted
// in the SampleSink, the SampleSink blocks the decode thread until the MP3
// decoder produces a decoded sample.
class DirectShowReader : public MediaDecoderReader
{
public:
  explicit DirectShowReader(AbstractMediaDecoder* aDecoder);

  virtual ~DirectShowReader();

  bool DecodeAudioData() override;
  bool DecodeVideoFrame(bool &aKeyframeSkip,
                        int64_t aTimeThreshold) override;

  nsresult ReadMetadata(MediaInfo* aInfo,
                        MetadataTags** aTags) override;

  RefPtr<SeekPromise> Seek(const SeekTarget& aTarget) override;

  static const GUID CLSID_MPEG_LAYER_3_DECODER_FILTER;

private:
  // Notifies the filter graph that playback is complete. aStatus is
  // the code to send to the filter graph. Always returns false, so
  // that we can just "return Finish()" from DecodeAudioData().
  bool Finish(HRESULT aStatus);

  nsresult SeekInternal(int64_t aTime);

  // DirectShow filter graph, and associated playback and seeking
  // control interfaces.
  RefPtr<IGraphBuilder> mGraph;
  RefPtr<IMediaControl> mControl;
  RefPtr<IMediaSeeking> mMediaSeeking;

  // Wraps the MediaResource, and feeds undecoded data into the filter graph.
  RefPtr<SourceFilter> mSourceFilter;

  // Sits at the end of the graph, removing decoded samples from the graph.
  // The graph will block while this is blocked, i.e. it will pause decoding.
  RefPtr<AudioSinkFilter> mAudioSinkFilter;

  // Some MP3s are variable bitrate, so DirectShow's duration estimation
  // can make its duration estimation based on the wrong bitrate. So we parse
  // the MP3 frames to get a more accuate estimate of the duration.
  MP3FrameParser mMP3FrameParser;

#ifdef DIRECTSHOW_REGISTER_GRAPH
  // Used to add/remove the filter graph to the Running Object Table. You can
  // connect GraphEdit/GraphStudio to the graph to observe and/or debug its
  // topology and state.
  DWORD mRotRegister;
#endif

  // Number of channels in the audio stream.
  uint32_t mNumChannels;

  // Samples per second in the audio stream.
  uint32_t mAudioRate;

  // Number of bytes per sample. Can be either 1 or 2.
  uint32_t mBytesPerSample;
};

} // namespace mozilla

#endif
