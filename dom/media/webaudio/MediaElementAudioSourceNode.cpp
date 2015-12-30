/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaElementAudioSourceNode.h"
#include "mozilla/dom/MediaElementAudioSourceNodeBinding.h"

namespace mozilla {
namespace dom {

MediaElementAudioSourceNode::MediaElementAudioSourceNode(AudioContext* aContext)
  : MediaStreamAudioSourceNode(aContext)
{
}

/* static */ already_AddRefed<MediaElementAudioSourceNode>
MediaElementAudioSourceNode::Create(AudioContext* aContext,
                                    DOMMediaStream* aStream, ErrorResult& aRv)
{
  RefPtr<MediaElementAudioSourceNode> node =
    new MediaElementAudioSourceNode(aContext);

  node->Init(aStream, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  return node.forget();
}

JSObject*
MediaElementAudioSourceNode::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return MediaElementAudioSourceNodeBinding::Wrap(aCx, this, aGivenProto);
}

} // namespace dom
} // namespace mozilla
