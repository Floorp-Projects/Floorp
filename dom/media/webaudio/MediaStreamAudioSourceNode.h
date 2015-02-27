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

class MediaStreamAudioSourceNodeEngine : public AudioNodeEngine
{
public:
  explicit MediaStreamAudioSourceNodeEngine(AudioNode* aNode)
    : AudioNodeEngine(aNode), mEnabled(false) {}

  bool IsEnabled() const { return mEnabled; }
  enum Parameters {
    ENABLE
  };
  virtual void SetInt32Parameter(uint32_t aIndex, int32_t aValue) override
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
                                   public DOMMediaStream::PrincipalChangeObserver
{
public:
  MediaStreamAudioSourceNode(AudioContext* aContext, DOMMediaStream* aMediaStream);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(MediaStreamAudioSourceNode, AudioNode)

  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  virtual void DestroyMediaStream() override;

  virtual uint16_t NumberOfInputs() const override { return 0; }

  virtual const char* NodeType() const override
  {
    return "MediaStreamAudioSourceNode";
  }

  virtual size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const override;
  virtual size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override;

  virtual void PrincipalChanged(DOMMediaStream* aMediaStream) override;

protected:
  virtual ~MediaStreamAudioSourceNode();

private:
  nsRefPtr<MediaInputPort> mInputPort;
  nsRefPtr<DOMMediaStream> mInputStream;
};

}
}

#endif
