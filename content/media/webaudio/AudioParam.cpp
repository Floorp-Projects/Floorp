/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioParam.h"
#include "nsContentUtils.h"
#include "nsIDOMWindow.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/AudioParamBinding.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_1(AudioParam, mNode)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(AudioParam, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(AudioParam, Release)

AudioParam::AudioParam(AudioNode* aNode,
                       AudioParam::CallbackType aCallback,
                       float aDefaultValue)
  : AudioParamTimeline(aDefaultValue)
  , mNode(aNode)
  , mCallback(aCallback)
  , mDefaultValue(aDefaultValue)
{
  SetIsDOMBinding();
}

AudioParam::~AudioParam()
{
}

JSObject*
AudioParam::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return AudioParamBinding::Wrap(aCx, aScope, this);
}

}
}

