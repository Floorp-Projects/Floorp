/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ChannelMergerNode_h_
#define ChannelMergerNode_h_

#include "AudioNode.h"

namespace mozilla {
namespace dom {

class AudioContext;
struct ChannelMergerOptions;

class ChannelMergerNode final : public AudioNode
{
public:
  static already_AddRefed<ChannelMergerNode>
  Create(AudioContext& aAudioContext, const ChannelMergerOptions& aOptions,
         ErrorResult& aRv);

  NS_DECL_ISUPPORTS_INHERITED

  static already_AddRefed<ChannelMergerNode>
  Constructor(const GlobalObject& aGlobal, AudioContext& aAudioContext,
              const ChannelMergerOptions& aOptions, ErrorResult& aRv)
  {
    return Create(aAudioContext, aOptions, aRv);
  }

  JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  uint16_t NumberOfInputs() const override { return mInputCount; }

  const char* NodeType() const override
  {
    return "ChannelMergerNode";
  }

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override
  {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }

private:
  ChannelMergerNode(AudioContext* aContext,
                    uint16_t aInputCount);
  ~ChannelMergerNode() = default;

  const uint16_t mInputCount;
};

} // namespace dom
} // namespace mozilla

#endif
