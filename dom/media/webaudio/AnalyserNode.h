/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AnalyserNode_h_
#define AnalyserNode_h_

#include "AudioNode.h"
#include "FFTBlock.h"
#include "AlignedTArray.h"

namespace mozilla {
namespace dom {

class AudioContext;
struct AnalyserOptions;

class AnalyserNode final : public AudioNode {
 public:
  static already_AddRefed<AnalyserNode> Create(AudioContext& aAudioContext,
                                               const AnalyserOptions& aOptions,
                                               ErrorResult& aRv);

  NS_INLINE_DECL_REFCOUNTING_INHERITED(AnalyserNode, AudioNode)

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  static already_AddRefed<AnalyserNode> Constructor(
      const GlobalObject& aGlobal, AudioContext& aAudioContext,
      const AnalyserOptions& aOptions, ErrorResult& aRv) {
    return Create(aAudioContext, aOptions, aRv);
  }

  void GetFloatFrequencyData(const Float32Array& aArray);
  void GetByteFrequencyData(const Uint8Array& aArray);
  void GetFloatTimeDomainData(const Float32Array& aArray);
  void GetByteTimeDomainData(const Uint8Array& aArray);
  uint32_t FftSize() const { return mAnalysisBlock.FFTSize(); }
  void SetFftSize(uint32_t aValue, ErrorResult& aRv);
  uint32_t FrequencyBinCount() const { return FftSize() / 2; }
  double MinDecibels() const { return mMinDecibels; }
  void SetMinDecibels(double aValue, ErrorResult& aRv);
  double MaxDecibels() const { return mMaxDecibels; }
  void SetMaxDecibels(double aValue, ErrorResult& aRv);
  double SmoothingTimeConstant() const { return mSmoothingTimeConstant; }
  void SetSmoothingTimeConstant(double aValue, ErrorResult& aRv);

  virtual const char* NodeType() const override { return "AnalyserNode"; }

  virtual size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const override;
  virtual size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override;

  void SetMinAndMaxDecibels(double aMinValue, double aMaxValue,
                            ErrorResult& aRv);

 private:
  ~AnalyserNode() = default;

  friend class AnalyserNodeEngine;
  void AppendChunk(const AudioChunk& aChunk);
  bool AllocateBuffer();
  bool FFTAnalysis();
  void ApplyBlackmanWindow(float* aBuffer, uint32_t aSize);
  void GetTimeDomainData(float* aData, size_t aLength);

 private:
  explicit AnalyserNode(AudioContext* aContext);

  FFTBlock mAnalysisBlock;
  nsTArray<AudioChunk> mChunks;
  double mMinDecibels;
  double mMaxDecibels;
  double mSmoothingTimeConstant;
  size_t mCurrentChunk = 0;
  AlignedTArray<float> mOutputBuffer;
};

}  // namespace dom
}  // namespace mozilla

#endif
