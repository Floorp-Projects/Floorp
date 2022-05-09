/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef IIRFilterNode_h_
#define IIRFilterNode_h_

#include "AudioNode.h"
#include "AudioParam.h"
#include "mozilla/dom/IIRFilterNodeBinding.h"

namespace mozilla::dom {

class AudioContext;
struct IIRFilterOptions;

class IIRFilterNode final : public AudioNode {
 public:
  static already_AddRefed<IIRFilterNode> Create(
      AudioContext& aAudioContext, const IIRFilterOptions& aOptions,
      ErrorResult& aRv);

  NS_INLINE_DECL_REFCOUNTING_INHERITED(IIRFilterNode, AudioNode)

  static already_AddRefed<IIRFilterNode> Constructor(
      const GlobalObject& aGlobal, AudioContext& aAudioContext,
      const IIRFilterOptions& aOptions, ErrorResult& aRv) {
    return Create(aAudioContext, aOptions, aRv);
  }

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  void GetFrequencyResponse(const Float32Array& aFrequencyHz,
                            const Float32Array& aMagResponse,
                            const Float32Array& aPhaseResponse);

  const char* NodeType() const override { return "IIRFilterNode"; }

  size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const override;
  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override;

 private:
  IIRFilterNode(AudioContext* aContext, const Sequence<double>& aFeedforward,
                const Sequence<double>& aFeedback);
  ~IIRFilterNode() = default;

  nsTArray<double> mFeedback;
  nsTArray<double> mFeedforward;
};

}  // namespace mozilla::dom

#endif
