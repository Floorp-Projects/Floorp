/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(AudioSinkInputPin_h_)
#define AudioSinkInputPin_h_

#include "BaseInputPin.h"
#include "DirectShowUtils.h"
#include "mozilla/RefPtr.h"
#include "nsAutoPtr.h"

namespace mozilla {

namespace media {
  class MediaType;
}

class AudioSinkFilter;
class SampleSink;


// Input pin for capturing audio output of a DirectShow filter graph.
// This is the input pin for the AudioSinkFilter.
class AudioSinkInputPin: public mozilla::media::BaseInputPin
{
public:
  AudioSinkInputPin(wchar_t* aObjectName,
                    AudioSinkFilter* aFilter,
                    mozilla::CriticalSection* aLock,
                    HRESULT* aOutResult);
  virtual ~AudioSinkInputPin();

  HRESULT GetMediaType (IN int iPos, OUT mozilla::media::MediaType * pmt);
  HRESULT CheckMediaType (IN const mozilla::media::MediaType * pmt);
  STDMETHODIMP Receive (IN IMediaSample *);
  STDMETHODIMP BeginFlush() override;
  STDMETHODIMP EndFlush() override;

  // Called when we start decoding a new segment, that happens directly after
  // a seek. This captures the segment's start time. Samples decoded by the
  // MP3 decoder have their timestamps offset from the segment start time.
  // Storing the segment start time enables us to set each sample's MediaTime
  // as an offset in the stream relative to the start of the stream, rather
  // than the start of the segment, i.e. its absolute time in the stream.
  STDMETHODIMP NewSegment(REFERENCE_TIME tStart,
                          REFERENCE_TIME tStop,
                          double dRate) override;

  STDMETHODIMP EndOfStream() override;

  // Returns the IMediaSeeking interface of the connected output pin.
  // We forward seeking requests upstream from the sink to the source
  // filters.
  already_AddRefed<IMediaSeeking> GetConnectedPinSeeking();

  SampleSink* GetSampleSink();

private:
  AudioSinkFilter* GetAudioSinkFilter();

  // Sets the media time on the media sample, relative to the segment
  // start time.
  HRESULT SetAbsoluteMediaTime(IMediaSample* aSample);

  nsAutoPtr<SampleSink> mSampleSink;

  // Synchronized by the filter lock; BaseInputPin::mLock.
  REFERENCE_TIME mSegmentStartTime;
};

} // namespace mozilla

#endif // AudioSinkInputPin_h_
