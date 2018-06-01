/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef AudioWorkletNode_h_
#define AudioWorkletNode_h_

#include "AudioNode.h"

namespace mozilla {
namespace dom {

class AudioParamMap;
struct AudioWorkletNodeOptions;
class MessagePort;

class AudioWorkletNode : public AudioNode
{
public:

  NS_DECL_ISUPPORTS_INHERITED

  IMPL_EVENT_HANDLER(processorerror)

  static already_AddRefed<AudioWorkletNode>
  Constructor(const GlobalObject& aGlobal,
              AudioContext& aAudioContext,
              const nsAString& aName,
              const AudioWorkletNodeOptions& aOptions,
              ErrorResult& aRv);

  AudioParamMap* GetParameters(ErrorResult& aRv) const;

  MessagePort* GetPort(ErrorResult& aRv) const;

  JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  const char* NodeType() const override
  {
    return "AudioWorkletNode";
  }

  size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const override;
  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override;

private:
  AudioWorkletNode(AudioContext* aAudioContext, const nsAString& aName);
  ~AudioWorkletNode() = default;

  nsString mNodeName;
};

} // namespace dom
} // namespace mozilla

#endif // AudioWorkletNode_h_