/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(AudioSinkFilter_h_)
#define AudioSinkFilter_h_

#include "BaseFilter.h"
#include "DirectShowUtils.h"
#include "nsAutoPtr.h"
#include "mozilla/RefPtr.h"

namespace mozilla {

class AudioSinkInputPin;
class SampleSink;

// Filter that acts as the end of the graph. Audio samples input into
// this filter block the calling thread, and the calling thread is
// unblocked when the decode thread extracts the sample. The samples
// input into this filter are stored in the SampleSink, where the blocking
// is implemented. The input pin owns the SampleSink.
class AudioSinkFilter: public mozilla::media::BaseFilter,
                       public IMediaSeeking
{

public:
  AudioSinkFilter(const wchar_t* aObjectName, HRESULT* aOutResult);
  virtual ~AudioSinkFilter();

  // Gets the input pin's sample sink.
  SampleSink* GetSampleSink();

  // IUnknown implementation.
  STDMETHODIMP QueryInterface(REFIID aIId, void **aInterface);
  STDMETHODIMP_(ULONG) AddRef();
  STDMETHODIMP_(ULONG) Release();

  //  --------------------------------------------------------------------
  //  CBaseFilter methods
  int GetPinCount ();
  mozilla::media::BasePin* GetPin ( IN int Index);
  STDMETHODIMP Pause ();
  STDMETHODIMP Stop ();
  STDMETHODIMP GetClassID ( OUT CLSID * pCLSID);
  STDMETHODIMP Run(REFERENCE_TIME tStart);
  // IMediaSeeking Methods...

  // We defer to SourceFilter, but we must expose the interface on
  // the output pins. Seeking commands come upstream from the renderers,
  // but they must be actioned at the source filters.
  STDMETHODIMP GetCapabilities(DWORD* aCapabilities);
  STDMETHODIMP CheckCapabilities(DWORD* aCapabilities);
  STDMETHODIMP IsFormatSupported(const GUID* aFormat);
  STDMETHODIMP QueryPreferredFormat(GUID* aFormat);
  STDMETHODIMP GetTimeFormat(GUID* aFormat);
  STDMETHODIMP IsUsingTimeFormat(const GUID* aFormat);
  STDMETHODIMP SetTimeFormat(const GUID* aFormat);
  STDMETHODIMP GetDuration(LONGLONG* pDuration);
  STDMETHODIMP GetStopPosition(LONGLONG* pStop);
  STDMETHODIMP GetCurrentPosition(LONGLONG* aCurrent);
  STDMETHODIMP ConvertTimeFormat(LONGLONG* aTarget,
                                 const GUID* aTargetFormat,
                                 LONGLONG aSource,
                                 const GUID* aSourceFormat);
  STDMETHODIMP SetPositions(LONGLONG* aCurrent,
                            DWORD aCurrentFlags,
                            LONGLONG* aStop,
                            DWORD aStopFlags);
  STDMETHODIMP GetPositions(LONGLONG* aCurrent,
                            LONGLONG* aStop);
  STDMETHODIMP GetAvailable(LONGLONG* aEarliest,
                            LONGLONG* aLatest);
  STDMETHODIMP SetRate(double aRate);
  STDMETHODIMP GetRate(double* aRate);
  STDMETHODIMP GetPreroll(LONGLONG* aPreroll);

  //  --------------------------------------------------------------------
  //  class factory calls this
  static IUnknown * CreateInstance (IN LPUNKNOWN punk, OUT HRESULT * phr);

private:
  CriticalSection mFilterCritSec;

  // Note: The input pin defers its refcounting to the sink filter, so when
  // the input pin is addrefed, what actually happens is the sink filter is
  // addrefed.
  nsAutoPtr<AudioSinkInputPin> mInputPin;
};

} // namespace mozilla

#endif // AudioSinkFilter_h_
