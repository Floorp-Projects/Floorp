/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MediaElementAudioSourceNode_h_
#define MediaElementAudioSourceNode_h_

#include "MediaStreamAudioSourceNode.h"

namespace mozilla {
namespace dom {

class MediaElementAudioSourceNode final : public MediaStreamAudioSourceNode
{
public:
  static already_AddRefed<MediaElementAudioSourceNode>
  Create(AudioContext* aContext, DOMMediaStream* aStream, ErrorResult& aRv);

  JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  const char* NodeType() const override
  {
    return "MediaElementAudioSourceNode";
  }

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override
  {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }
private:
  explicit MediaElementAudioSourceNode(AudioContext* aContext);
};

} // namespace dom
} // namespace mozilla

#endif
