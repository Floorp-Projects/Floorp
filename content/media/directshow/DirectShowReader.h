/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(DirectShowReader_h_)
#define DirectShowReader_h_

#include "Windows.h" // HRESULT, DWORD
#include "MediaDecoderReader.h"
#include "mozilla/RefPtr.h"

class IGraphBuilder;
class IMediaControl;
class IMediaSeeking;
class IMediaEventEx;

namespace mozilla {

class AudioSinkFilter;
class SourceFilter;

namespace dom {
class TimeRanges;
}

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
  DirectShowReader(AbstractMediaDecoder* aDecoder);

  virtual ~DirectShowReader();

  nsresult Init(MediaDecoderReader* aCloneDonor) MOZ_OVERRIDE;

  bool DecodeAudioData() MOZ_OVERRIDE;
  bool DecodeVideoFrame(bool &aKeyframeSkip,
                        int64_t aTimeThreshold) MOZ_OVERRIDE;

  bool HasAudio() MOZ_OVERRIDE;
  bool HasVideo() MOZ_OVERRIDE;

  nsresult ReadMetadata(VideoInfo* aInfo,
                        MetadataTags** aTags) MOZ_OVERRIDE;

  nsresult Seek(int64_t aTime,
                int64_t aStartTime,
                int64_t aEndTime,
                int64_t aCurrentTime) MOZ_OVERRIDE;

  nsresult GetBuffered(mozilla::dom::TimeRanges* aBuffered,
                       int64_t aStartTime) MOZ_OVERRIDE;

  void OnDecodeThreadStart() MOZ_OVERRIDE;
  void OnDecodeThreadFinish() MOZ_OVERRIDE;

private:

  // Calls mAudioQueue.Finish(), and notifies the filter graph that playback
  // is complete. aStatus is the code to send to the filter graph.
  // Always returns false, so that we can just "return Finish()" from
  // DecodeAudioData().
  bool Finish(HRESULT aStatus);

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

#ifdef DEBUG
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
