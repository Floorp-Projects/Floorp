/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef StereoPannerNode_h_
#define StereoPannerNode_h_

#include "AudioNode.h"
#include "mozilla/dom/StereoPannerNodeBinding.h"

namespace mozilla {
namespace dom {

class AudioContext;

class StereoPannerNode final : public AudioNode
{
public:
  MOZ_DECLARE_REFCOUNTED_TYPENAME(StereoPannerNode)
  explicit StereoPannerNode(AudioContext* aContext);

  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  virtual void SetChannelCount(uint32_t aChannelCount, ErrorResult& aRv) override
  {
    if (aChannelCount > 2) {
      aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
      return;
    }
    AudioNode::SetChannelCount(aChannelCount, aRv);
  }
  virtual void SetChannelCountModeValue(ChannelCountMode aMode, ErrorResult& aRv) override
  {
    if (aMode == ChannelCountMode::Max) {
      aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
      return;
    }
    AudioNode::SetChannelCountModeValue(aMode, aRv);
  }

  AudioParam* Pan() const
  {
    return mPan;
  }

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(StereoPannerNode, AudioNode)

  virtual const char* NodeType() const override
  {
    return "StereoPannerNode";
  }

  virtual size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const override;
  virtual size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override;

protected:
  virtual ~StereoPannerNode();

private:
  static void SendPanToStream(AudioNode* aNode,
                              const AudioTimelineEvent& aEvent);
  nsRefPtr<AudioParam> mPan;
};

} // namespace dom
} // namespace mozilla

#endif

