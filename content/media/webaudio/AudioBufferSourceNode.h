/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AudioBufferSourceNode_h_
#define AudioBufferSourceNode_h_

#include "AudioSourceNode.h"
#include "AudioBuffer.h"
#include "mozilla/dom/BindingUtils.h"

namespace mozilla {
namespace dom {

class AudioBufferSourceNode : public AudioSourceNode,
                              public MainThreadMediaStreamListener
{
public:
  explicit AudioBufferSourceNode(AudioContext* aContext);
  virtual ~AudioBufferSourceNode();

  virtual void DestroyMediaStream() MOZ_OVERRIDE
  {
    if (mStream) {
      mStream->RemoveMainThreadListener(this);
    }
    AudioSourceNode::DestroyMediaStream();
  }
  virtual bool SupportsMediaStreams() const MOZ_OVERRIDE
  {
    return true;
  }

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(AudioBufferSourceNode, AudioSourceNode)

  virtual JSObject* WrapObject(JSContext* aCx, JSObject* aScope);

  void Start(JSContext* aCx, double aWhen, double aOffset,
             const Optional<double>& aDuration, ErrorResult& aRv);
  void Stop(double aWhen, ErrorResult& aRv);

  AudioBuffer* GetBuffer() const
  {
    return mBuffer;
  }
  void SetBuffer(AudioBuffer* aBuffer)
  {
    mBuffer = aBuffer;
  }

  virtual void NotifyMainThreadStateChanged() MOZ_OVERRIDE;

private:
  nsRefPtr<AudioBuffer> mBuffer;
  bool mStartCalled;
};

}
}

#endif

