/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef IIRFilterNode_h_
#define IIRFilterNode_h_

#include "AudioNode.h"
#include "AudioParam.h"
#include "mozilla/dom/IIRFilterNodeBinding.h"

namespace mozilla {
namespace dom {

class AudioContext;

class IIRFilterNode final : public AudioNode
{
public:
  explicit IIRFilterNode(AudioContext* aContext,
                         const mozilla::dom::binding_detail::AutoSequence<double>& aFeedforward,
                         const mozilla::dom::binding_detail::AutoSequence<double>& aFeedback);

  NS_DECL_ISUPPORTS_INHERITED

  JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;


  void GetFrequencyResponse(const Float32Array& aFrequencyHz,
                            const Float32Array& aMagResponse,
                            const Float32Array& aPhaseResponse);

  const char* NodeType() const override
  {
    return "IIRFilterNode";
  }

  size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const override;
  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override;

protected:
  virtual ~IIRFilterNode();

private:
    nsTArray<double> mFeedback;
    nsTArray<double> mFeedforward;
};

} // namespace dom
} // namespace mozilla

#endif

