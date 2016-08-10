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

class PlayingRefChangeHandler final : public Runnable
{
public:
  enum ChangeType { ADDREF, RELEASE };
  PlayingRefChangeHandler(AudioNodeStream* aStream, ChangeType aChange)
    : mStream(aStream)
    , mChange(aChange)
  {
  }

  NS_IMETHOD Run() override
  {
    RefPtr<AudioNode> node = mStream->Engine()->NodeMainThread();
    if (node) {
      if (mChange == ADDREF) {
        node->MarkActive();
      } else if (mChange == RELEASE) {
        node->MarkInactive();
      }
    }
    return NS_OK;
  }

private:
  RefPtr<AudioNodeStream> mStream;
  ChangeType mChange;
};

} // namespace dom
} // namespace mozilla

#endif

