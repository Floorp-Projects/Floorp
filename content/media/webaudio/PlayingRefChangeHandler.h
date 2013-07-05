/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PlayingRefChangeHandler_h__
#define PlayingRefChangeHandler_h__

#include "nsThreadUtils.h"
#include "AudioNodeStream.h"

namespace mozilla {
namespace dom {

template<class NodeType>
class PlayingRefChangeHandler : public nsRunnable
{
public:
  enum ChangeType { ADDREF, RELEASE };
  PlayingRefChangeHandler(AudioNodeStream* aStream, ChangeType aChange)
    : mLastProcessedGraphUpdateIndex(aStream->GetProcessingGraphUpdateIndex())
    , mStream(aStream)
    , mChange(aChange)
  {
  }

  NS_IMETHOD Run()
  {
    nsRefPtr<NodeType> node;
    {
      // No need to keep holding the lock for the whole duration of this
      // function, since we're holding a strong reference to it, so if
      // we can obtain the reference, we will hold the node alive in
      // this function.
      MutexAutoLock lock(mStream->Engine()->NodeMutex());
      node = static_cast<NodeType*>(mStream->Engine()->Node());
    }
    if (node) {
      if (mChange == ADDREF) {
        node->mPlayingRef.Take(node);
      } else if (mChange == RELEASE &&
                 node->AcceptPlayingRefRelease(mLastProcessedGraphUpdateIndex)) {
        node->mPlayingRef.Drop(node);
      }
    }
    return NS_OK;
  }

private:
  int64_t mLastProcessedGraphUpdateIndex;
  nsRefPtr<AudioNodeStream> mStream;
  ChangeType mChange;
};

}
}

#endif

