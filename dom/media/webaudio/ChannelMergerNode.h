/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ChannelMergerNode_h_
#define ChannelMergerNode_h_

#include "AudioNode.h"

namespace mozilla::dom {

class AudioContext;
struct ChannelMergerOptions;

class ChannelMergerNode final : public AudioNode {
 public:
  static already_AddRefed<ChannelMergerNode> Create(
      AudioContext& aAudioContext, const ChannelMergerOptions& aOptions,
      ErrorResult& aRv);

  NS_INLINE_DECL_REFCOUNTING_INHERITED(ChannelMergerNode, AudioNode)

  static already_AddRefed<ChannelMergerNode> Constructor(
      const GlobalObject& aGlobal, AudioContext& aAudioContext,
      const ChannelMergerOptions& aOptions, ErrorResult& aRv) {
    return Create(aAudioContext, aOptions, aRv);
  }

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  uint16_t NumberOfInputs() const override { return mInputCount; }

  const char* NodeType() const override { return "ChannelMergerNode"; }

  virtual void SetChannelCount(uint32_t aChannelCount,
                               ErrorResult& aRv) override {
    if (aChannelCount != 1) {
      aRv.ThrowInvalidStateError(
          "Cannot change channel count of ChannelMergerNode");
    }
  }

  virtual void SetChannelCountModeValue(ChannelCountMode aMode,
                                        ErrorResult& aRv) override {
    if (aMode != ChannelCountMode::Explicit) {
      aRv.ThrowInvalidStateError(
          "Cannot change channel count mode of ChannelMergerNode");
    }
  }

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }

 private:
  ChannelMergerNode(AudioContext* aContext, uint16_t aInputCount);
  ~ChannelMergerNode() = default;

  const uint16_t mInputCount;
};

}  // namespace mozilla::dom

#endif
