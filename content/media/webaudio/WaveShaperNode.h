/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WaveShaperNode_h_
#define WaveShaperNode_h_

#include "AudioNode.h"
#include "AudioParam.h"

namespace mozilla {
namespace dom {

class AudioContext;

class WaveShaperNode : public AudioNode
{
public:
  explicit WaveShaperNode(AudioContext *aContext);
  virtual ~WaveShaperNode();

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(WaveShaperNode, AudioNode)

  virtual JSObject* WrapObject(JSContext *aCx,
                               JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;

  JSObject* GetCurve(JSContext* aCx) const
  {
    return mCurve;
  }
  void SetCurve(const Float32Array* aData);

private:
  void ClearCurve();

private:
  JSObject* mCurve;
};

}
}

#endif
