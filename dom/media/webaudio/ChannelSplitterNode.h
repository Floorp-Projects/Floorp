/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ChannelSplitterNode_h_
#define ChannelSplitterNode_h_

#include "AudioNode.h"

namespace mozilla::dom {

class AudioContext;
struct ChannelSplitterOptions;

class ChannelSplitterNode final : public AudioNode {
 public:
  static already_AddRefed<ChannelSplitterNode> Create(
      AudioContext& aAudioContext, const ChannelSplitterOptions& aOptions,
      ErrorResult& aRv);

  NS_INLINE_DECL_REFCOUNTING_INHERITED(ChannelSplitterNode, AudioNode)

  static already_AddRefed<ChannelSplitterNode> Constructor(
      const GlobalObject& aGlobal, AudioContext& aAudioContext,
      const ChannelSplitterOptions& aOptions, ErrorResult& aRv) {
    return Create(aAudioContext, aOptions, aRv);
  }

  void SetChannelCount(uint32_t aChannelCount, ErrorResult& aRv) override {
    if (aChannelCount != ChannelCount()) {
      aRv.ThrowInvalidStateError(
          "Cannot change channel count of ChannelSplitterNode");
    }
  }
  void SetChannelCountModeValue(ChannelCountMode aMode,
                                ErrorResult& aRv) override {
    if (aMode != ChannelCountModeValue()) {
      aRv.ThrowInvalidStateError(
          "Cannot change channel count mode of ChannelSplitterNode");
    }
  }
  void SetChannelInterpretationValue(ChannelInterpretation aMode,
                                     ErrorResult& aRv) override {
    if (aMode != ChannelInterpretationValue()) {
      aRv.ThrowInvalidStateError(
          "Cannot change channel interpretation of ChannelSplitterNode");
    }
  }

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  uint16_t NumberOfOutputs() const override { return mOutputCount; }

  const char* NodeType() const override { return "ChannelSplitterNode"; }

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }

 private:
  ChannelSplitterNode(AudioContext* aContext, uint16_t aOutputCount);
  ~ChannelSplitterNode() = default;

  const uint16_t mOutputCount;
};

}  // namespace mozilla::dom

#endif
