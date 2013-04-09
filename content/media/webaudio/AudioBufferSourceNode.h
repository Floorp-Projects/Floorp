/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AudioBufferSourceNode_h_
#define AudioBufferSourceNode_h_

#include "AudioNode.h"
#include "AudioBuffer.h"
#include "mozilla/dom/BindingUtils.h"

namespace mozilla {
namespace dom {

class AudioBufferSourceNode : public AudioNode,
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
    AudioNode::DestroyMediaStream();
  }
  virtual bool SupportsMediaStreams() const MOZ_OVERRIDE
  {
    return true;
  }
  virtual uint32_t NumberOfInputs() const MOZ_FINAL MOZ_OVERRIDE
  {
    return 0;
  }

  void JSBindingFinalized()
  {
    // If the JS binding goes away on a node which never received a start()
    // call, then it can no longer produce output.
    if (!mStartCalled) {
      SetProduceOwnOutput(false);
    }
    AudioNode::JSBindingFinalized();
  }

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(AudioBufferSourceNode, AudioNode)

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

  bool Loop() const
  {
    return mLoop;
  }
  void SetLoop(bool aLoop)
  {
    mLoop = aLoop;
  }
  double LoopStart() const
  {
    return mLoopStart;
  }
  void SetLoopStart(double aStart)
  {
    mLoopStart = aStart;
  }
  double LoopEnd() const
  {
    return mLoopEnd;
  }
  void SetLoopEnd(double aEnd)
  {
    mLoopEnd = aEnd;
  }

  virtual void NotifyMainThreadStateChanged() MOZ_OVERRIDE;

private:
  nsRefPtr<AudioBuffer> mBuffer;
  double mLoopStart;
  double mLoopEnd;
  bool mLoop;
  bool mStartCalled;
};

}
}

#endif

