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
    : MediaStreamAudioSourceNode(aContext) {}

/* static */
already_AddRefed<MediaElementAudioSourceNode>
MediaElementAudioSourceNode::Create(
    AudioContext& aAudioContext, const MediaElementAudioSourceOptions& aOptions,
    ErrorResult& aRv) {
  if (aAudioContext.IsOffline()) {
    aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
    return nullptr;
  }

  RefPtr<MediaElementAudioSourceNode> node =
      new MediaElementAudioSourceNode(&aAudioContext);

  RefPtr<DOMMediaStream> stream = aOptions.mMediaElement->CaptureAudio(
      aRv, aAudioContext.Destination()->Stream()->Graph());
  if (aRv.Failed()) {
    return nullptr;
  }

  node->Init(stream, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  node->ListenForAllowedToPlay(aOptions);
  return node.forget();
}

JSObject* MediaElementAudioSourceNode::WrapObject(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return MediaElementAudioSourceNode_Binding::Wrap(aCx, this, aGivenProto);
}

void MediaElementAudioSourceNode::ListenForAllowedToPlay(
    const MediaElementAudioSourceOptions& aOptions) {
  aOptions.mMediaElement->GetAllowedToPlayPromise()
      ->Then(
          GetAbstractMainThread(), __func__,
          // Capture by reference to bypass the mozilla-refcounted-inside-lambda
          // static analysis. We capture a non-owning reference so as to allow
          // cycle collection of the node. The reference is cleared via
          // DisconnectIfExists() from Destroy() when the node is collected.
          [& self = *this]() {
            self.Context()->StartBlockedAudioContextIfAllowed();
            self.mAllowedToPlayRequest.Complete();
          })
      ->Track(mAllowedToPlayRequest);
}

void MediaElementAudioSourceNode::Destroy() {
  mAllowedToPlayRequest.DisconnectIfExists();
  MediaStreamAudioSourceNode::Destroy();
}

}  // namespace dom
}  // namespace mozilla
