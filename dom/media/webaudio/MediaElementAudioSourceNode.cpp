/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaElementAudioSourceNode.h"
#include "mozilla/dom/MediaElementAudioSourceNodeBinding.h"
#include "AudioDestinationNode.h"
#include "nsIScriptError.h"
#include "AudioNodeStream.h"

namespace mozilla {
namespace dom {

MediaElementAudioSourceNode::MediaElementAudioSourceNode(AudioContext* aContext)
  : MediaStreamAudioSourceNode(aContext)
{
}

/* static */ already_AddRefed<MediaElementAudioSourceNode>
MediaElementAudioSourceNode::Create(AudioContext& aAudioContext,
                                    const MediaElementAudioSourceOptions& aOptions,
                                    ErrorResult& aRv)
{
  if (aAudioContext.IsOffline()) {
    aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
    return nullptr;
  }

  if (aAudioContext.CheckClosed(aRv)) {
    return nullptr;
  }

  RefPtr<MediaElementAudioSourceNode> node =
    new MediaElementAudioSourceNode(&aAudioContext);

  RefPtr<DOMMediaStream> stream =
    aOptions.mMediaElement->CaptureAudio(aRv, aAudioContext.Destination()->Stream()->Graph());
  if (aRv.Failed()) {
    return nullptr;
  }

  node->Init(stream, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  return node.forget();
}

JSObject*
MediaElementAudioSourceNode::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return MediaElementAudioSourceNode_Binding::Wrap(aCx, this, aGivenProto);
}

} // namespace dom
} // namespace mozilla
