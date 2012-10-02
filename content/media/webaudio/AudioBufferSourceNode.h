/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

#include "AudioSourceNode.h"
#include "AudioBuffer.h"

namespace mozilla {
namespace dom {

class AudioBufferSourceNode : public AudioSourceNode
{
public:
  explicit AudioBufferSourceNode(AudioContext* aContext);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(AudioBufferSourceNode, AudioSourceNode)

  virtual JSObject* WrapObject(JSContext* aCx, JSObject* aScope,
                               bool* aTriedToWrap);

  void Start(double) { /* no-op for now */ }
  void Stop(double) { /* no-op for now */ }

  AudioBuffer* GetBuffer() const
  {
    return mBuffer;
  }
  void SetBuffer(AudioBuffer* aBuffer);

private:
  nsRefPtr<AudioBuffer> mBuffer;
};

}
}

