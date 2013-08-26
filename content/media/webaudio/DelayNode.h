/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DelayNode_h_
#define DelayNode_h_

#include "AudioNode.h"
#include "AudioParam.h"

namespace mozilla {
namespace dom {

class AudioContext;
template <class T> class PlayingRefChangeHandler;

class DelayNode : public AudioNode
{
public:
  DelayNode(AudioContext* aContext, double aMaxDelay);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(DelayNode, AudioNode)

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;

  AudioParam* DelayTime() const
  {
    return mDelay;
  }

  virtual const DelayNode* AsDelayNode() const MOZ_OVERRIDE
  {
    return this;
  }

  virtual void NotifyInputConnected() MOZ_OVERRIDE
  {
    mMediaStreamGraphUpdateIndexAtLastInputConnection =
      mStream->Graph()->GetCurrentGraphUpdateIndex();
  }
  bool AcceptPlayingRefRelease(int64_t aLastGraphUpdateIndexProcessed) const
  {
    // Reject any requests to release the playing ref if the request was issued
    // before the MediaStreamGraph was aware of the most-recently-added input
    // connection.
    return aLastGraphUpdateIndexProcessed >= mMediaStreamGraphUpdateIndexAtLastInputConnection;
  }

private:
  static void SendDelayToStream(AudioNode* aNode);
  friend class DelayNodeEngine;

private:
  int64_t mMediaStreamGraphUpdateIndexAtLastInputConnection;
  nsRefPtr<AudioParam> mDelay;
};

}
}

#endif

