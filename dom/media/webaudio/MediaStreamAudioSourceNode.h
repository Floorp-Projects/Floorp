/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MediaStreamAudioSourceNode_h_
#define MediaStreamAudioSourceNode_h_

#include "AudioNode.h"
#include "DOMMediaStream.h"
#include "AudioNodeEngine.h"

namespace mozilla {

namespace dom {

class MediaStreamAudioSourceNodeEngine final : public AudioNodeEngine
{
public:
  explicit MediaStreamAudioSourceNodeEngine(AudioNode* aNode)
    : AudioNodeEngine(aNode), mEnabled(false) {}

  bool IsEnabled() const { return mEnabled; }
  enum Parameters {
    ENABLE
  };
  void SetInt32Parameter(uint32_t aIndex, int32_t aValue) override
  {
    switch (aIndex) {
    case ENABLE:
      mEnabled = !!aValue;
      break;
    default:
      NS_ERROR("MediaStreamAudioSourceNodeEngine bad parameter index");
    }
  }

private:
  bool mEnabled;
};

class MediaStreamAudioSourceNode : public AudioNode,
                                   public DOMMediaStream::TrackListener,
                                   public PrincipalChangeObserver<MediaStreamTrack>
{
public:
  static already_AddRefed<MediaStreamAudioSourceNode>
  Create(AudioContext* aContext, DOMMediaStream* aStream, ErrorResult& aRv);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(MediaStreamAudioSourceNode, AudioNode)

  JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  void DestroyMediaStream() override;

  uint16_t NumberOfInputs() const override { return 0; }

  const char* NodeType() const override
  {
    return "MediaStreamAudioSourceNode";
  }

  virtual const char* CrossOriginErrorString() const
  {
    return "MediaStreamAudioSourceNodeCrossOrigin";
  }

  size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const override;
  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override;

  // Attaches to aTrack so that its audio content will be used as input.
  void AttachToTrack(const RefPtr<MediaStreamTrack>& aTrack);

  // Detaches from the currently attached track if there is one.
  void DetachFromTrack();

  // Attaches to the first available audio track in aMediaStream.
  void AttachToFirstTrack(const RefPtr<DOMMediaStream>& aMediaStream);

  // From DOMMediaStream::TrackListener.
  void NotifyTrackAdded(const RefPtr<MediaStreamTrack>& aTrack) override;
  void NotifyTrackRemoved(const RefPtr<MediaStreamTrack>& aTrack) override;

  // From PrincipalChangeObserver<MediaStreamTrack>.
  void PrincipalChanged(MediaStreamTrack* aMediaStreamTrack) override;

protected:
  explicit MediaStreamAudioSourceNode(AudioContext* aContext);
  void Init(DOMMediaStream* aMediaStream, ErrorResult& aRv);
  void Destroy();
  virtual ~MediaStreamAudioSourceNode();

private:
  RefPtr<MediaInputPort> mInputPort;
  RefPtr<DOMMediaStream> mInputStream;

  // On construction we set this to the first audio track of mInputStream.
  RefPtr<MediaStreamTrack> mInputTrack;
};

} // namespace dom
} // namespace mozilla

#endif
