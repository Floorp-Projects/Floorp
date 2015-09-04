/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaStreamAudioSourceNode.h"
#include "mozilla/dom/MediaStreamAudioSourceNodeBinding.h"
#include "AudioNodeEngine.h"
#include "AudioNodeExternalInputStream.h"
#include "nsIDocument.h"
#include "mozilla/CORSMode.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_CLASS(MediaStreamAudioSourceNode)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(MediaStreamAudioSourceNode)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mInputStream)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END_INHERITED(AudioNode)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(MediaStreamAudioSourceNode, AudioNode)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mInputStream)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(MediaStreamAudioSourceNode)
NS_INTERFACE_MAP_END_INHERITING(AudioNode)

NS_IMPL_ADDREF_INHERITED(MediaStreamAudioSourceNode, AudioNode)
NS_IMPL_RELEASE_INHERITED(MediaStreamAudioSourceNode, AudioNode)

MediaStreamAudioSourceNode::MediaStreamAudioSourceNode(AudioContext* aContext,
                                                       DOMMediaStream* aMediaStream)
  : AudioNode(aContext,
              2,
              ChannelCountMode::Max,
              ChannelInterpretation::Speakers),
    mInputStream(aMediaStream)
{
  AudioNodeEngine* engine = new MediaStreamAudioSourceNodeEngine(this);
  mStream = AudioNodeExternalInputStream::Create(aContext->Graph(), engine);
  ProcessedMediaStream* outputStream = static_cast<ProcessedMediaStream*>(mStream.get());
  mInputPort = outputStream->AllocateInputPort(aMediaStream->GetStream());
  mInputStream->AddConsumerToKeepAlive(static_cast<nsIDOMEventTarget*>(this));

  PrincipalChanged(mInputStream); // trigger enabling/disabling of the connector
  mInputStream->AddPrincipalChangeObserver(this);
}

MediaStreamAudioSourceNode::~MediaStreamAudioSourceNode()
{
  if (mInputStream) {
    mInputStream->RemovePrincipalChangeObserver(this);
  }
}

/**
 * Changes the principal.  Note that this will be called on the main thread, but
 * changes will be enacted on the MediaStreamGraph thread.  If the principal
 * change results in the document principal losing access to the stream, then
 * there needs to be other measures in place to ensure that any media that is
 * governed by the new stream principal is not available to the Media Stream
 * Graph before this change completes.  Otherwise, a site could get access to
 * media that they are not authorized to receive.
 *
 * One solution is to block the altered content, call this method, then dispatch
 * another change request to the MediaStreamGraph thread that allows the content
 * under the new principal to flow.  This might be unnecessary if the principal
 * change is changing to be the document principal.
 */
void
MediaStreamAudioSourceNode::PrincipalChanged(DOMMediaStream* aDOMMediaStream)
{
  bool subsumes = false;
  nsPIDOMWindow* parent = Context()->GetParentObject();
  if (parent) {
    nsIDocument* doc = parent->GetExtantDoc();
    if (doc) {
      nsIPrincipal* docPrincipal = doc->NodePrincipal();
      nsIPrincipal* streamPrincipal = mInputStream->GetPrincipal();
      if (!streamPrincipal || NS_FAILED(docPrincipal->Subsumes(streamPrincipal, &subsumes))) {
        subsumes = false;
      }
    }
  }
  auto stream = static_cast<AudioNodeExternalInputStream*>(mStream.get());
  stream->SetInt32Parameter(MediaStreamAudioSourceNodeEngine::ENABLE,
                            subsumes || aDOMMediaStream->GetCORSMode() != CORS_NONE);
}

size_t
MediaStreamAudioSourceNode::SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const
{
  // Future:
  // - mInputStream
  size_t amount = AudioNode::SizeOfExcludingThis(aMallocSizeOf);
  amount += mInputPort->SizeOfIncludingThis(aMallocSizeOf);
  return amount;
}

size_t
MediaStreamAudioSourceNode::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
  return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
}

void
MediaStreamAudioSourceNode::DestroyMediaStream()
{
  if (mInputPort) {
    mInputPort->Destroy();
    mInputPort = nullptr;
  }
  AudioNode::DestroyMediaStream();
}

JSObject*
MediaStreamAudioSourceNode::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return MediaStreamAudioSourceNodeBinding::Wrap(aCx, this, aGivenProto);
}

} // namespace dom
} // namespace mozilla
