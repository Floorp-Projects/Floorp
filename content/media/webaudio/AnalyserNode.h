/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AnalyserNode_h_
#define AnalyserNode_h_

#include "AudioNode.h"
#include "FFTBlock.h"

namespace mozilla {
namespace dom {

class AudioContext;

class AnalyserNode : public AudioNode
{
public:
  explicit AnalyserNode(AudioContext* aContext);

  NS_DECL_ISUPPORTS_INHERITED

  virtual JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  void GetFloatFrequencyData(const Float32Array& aArray);
  void GetByteFrequencyData(const Uint8Array& aArray);
  void GetFloatTimeDomainData(const Float32Array& aArray);
  void GetByteTimeDomainData(const Uint8Array& aArray);
  uint32_t FftSize() const
  {
    return mAnalysisBlock.FFTSize();
  }
  void SetFftSize(uint32_t aValue, ErrorResult& aRv);
  uint32_t FrequencyBinCount() const
  {
    return FftSize() / 2;
  }
  double MinDecibels() const
  {
    return mMinDecibels;
  }
  void SetMinDecibels(double aValue, ErrorResult& aRv);
  double MaxDecibels() const
  {
    return mMaxDecibels;
  }
  void SetMaxDecibels(double aValue, ErrorResult& aRv);
  double SmoothingTimeConstant() const
  {
    return mSmoothingTimeConstant;
  }
  void SetSmoothingTimeConstant(double aValue, ErrorResult& aRv);

private:
  friend class AnalyserNodeEngine;
  void AppendChunk(const AudioChunk& aChunk);
  bool AllocateBuffer();
  bool FFTAnalysis();
  void ApplyBlackmanWindow(float* aBuffer, uint32_t aSize);

private:
  FFTBlock mAnalysisBlock;
  double mMinDecibels;
  double mMaxDecibels;
  double mSmoothingTimeConstant;
  uint32_t mWriteIndex;
  FallibleTArray<float> mBuffer;
  FallibleTArray<float> mOutputBuffer;
};

}
}

#endif

