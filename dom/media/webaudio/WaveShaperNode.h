/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WaveShaperNode_h_
#define WaveShaperNode_h_

#include "AudioNode.h"
#include "mozilla/dom/WaveShaperNodeBinding.h"
#include "mozilla/dom/TypedArray.h"

namespace mozilla::dom {

class AudioContext;
struct WaveShaperOptions;

class WaveShaperNode final : public AudioNode {
 public:
  static already_AddRefed<WaveShaperNode> Create(
      AudioContext& aAudioContext, const WaveShaperOptions& aOptions,
      ErrorResult& aRv);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(WaveShaperNode,
                                                         AudioNode)

  static already_AddRefed<WaveShaperNode> Constructor(
      const GlobalObject& aGlobal, AudioContext& aAudioContext,
      const WaveShaperOptions& aOptions, ErrorResult& aRv) {
    return Create(aAudioContext, aOptions, aRv);
  }

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  void GetCurve(JSContext* aCx, JS::MutableHandle<JSObject*> aRetval,
                ErrorResult& aRv);
  void SetCurve(const Nullable<Float32Array>& aData, ErrorResult& aRv);

  OverSampleType Oversample() const { return mType; }
  void SetOversample(OverSampleType aType);

  size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const override {
    // Possibly track in the future:
    // - mCurve
    return AudioNode::SizeOfExcludingThis(aMallocSizeOf);
  }

  const char* NodeType() const override { return "WaveShaperNode"; }

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }

 private:
  explicit WaveShaperNode(AudioContext* aContext);
  ~WaveShaperNode() = default;

  void SetCurveInternal(const nsTArray<float>& aCurve, ErrorResult& aRv);
  void CleanCurveInternal();

  void SendCurveToTrack();

  nsTArray<float> mCurve;
  OverSampleType mType;
};

}  // namespace mozilla::dom

#endif
