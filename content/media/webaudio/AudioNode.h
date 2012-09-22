/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

#include "nsWrapperCache.h"
#include "nsCycleCollectionParticipant.h"
#include "mozilla/Attributes.h"
#include "EnableWebAudioCheck.h"
#include "nsAutoPtr.h"
#include "AudioContext.h"

struct JSContext;

namespace mozilla {
namespace dom {

class AudioNode : public nsISupports,
                  public nsWrapperCache,
                  public EnableWebAudioCheck
{
public:
  explicit AudioNode(AudioContext* aContext);
  virtual ~AudioNode() {}

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(AudioNode)

  AudioContext* GetParentObject() const
  {
    return mContext;
  }

  AudioContext* Context() const
  {
    return mContext;
  }

  void Connect(AudioNode& aDestination, uint32_t aOutput,
               uint32_t aInput)
  { /* no-op for now */ }

  void Disconnect(uint32_t aOutput)
  { /* no-op for now */ }

private:
  nsRefPtr<AudioContext> mContext;
};

}
}

