/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ConvolverNode_h_
#define ConvolverNode_h_

#include "AudioNode.h"
#include "AudioBuffer.h"

namespace mozilla {
namespace dom {

class AudioContext;
struct ConvolverOptions;

class ConvolverNode final : public AudioNode
{
public:
  static already_AddRefed<ConvolverNode>
  Create(JSContext* aCx, AudioContext& aAudioContext,
         const ConvolverOptions& aOptions, ErrorResult& aRv);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(ConvolverNode, AudioNode);

  static already_AddRefed<ConvolverNode>
  Constructor(const GlobalObject& aGlobal, AudioContext& aAudioContext,
              const ConvolverOptions& aOptions, ErrorResult& aRv)
  {
    return Create(aGlobal.Context(), aAudioContext, aOptions, aRv);
  }

  JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  AudioBuffer* GetBuffer(JSContext* aCx) const
  {
    return mBuffer;
  }

  void SetBuffer(JSContext* aCx, AudioBuffer* aBuffer, ErrorResult& aRv);

  bool Normalize() const
  {
    return mNormalize;
  }

  void SetNormalize(bool aNormal);

  void SetChannelCount(uint32_t aChannelCount, ErrorResult& aRv) override
  {
    if (aChannelCount > 2) {
      aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
      return;
    }
    AudioNode::SetChannelCount(aChannelCount, aRv);
  }
  void SetChannelCountModeValue(ChannelCountMode aMode, ErrorResult& aRv) override
  {
    if (aMode == ChannelCountMode::Max) {
      aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
      return;
    }
    AudioNode::SetChannelCountModeValue(aMode, aRv);
  }

  const char* NodeType() const override
  {
    return "ConvolverNode";
  }

  size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const override;
  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override;

private:
  explicit ConvolverNode(AudioContext* aContext);
  ~ConvolverNode() = default;

  RefPtr<AudioBuffer> mBuffer;
  bool mNormalize;
};


} //end namespace dom
} //end namespace mozilla

#endif
