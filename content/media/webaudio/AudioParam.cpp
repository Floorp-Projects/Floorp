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

NS_IMPL_CYCLE_COLLECTION_CLASS(AudioParam)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_NATIVE(AudioParam)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mContext)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER_NATIVE
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NATIVE_BEGIN(AudioParam)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mContext)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END
NS_IMPL_CYCLE_COLLECTION_TRACE_NATIVE_BEGIN(AudioParam)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_TRACE_END
NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(AudioParam, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(AudioParam, Release)

AudioParam::AudioParam(AudioContext* aContext,
                       float aDefaultValue,
                       float aMinValue,
                       float aMaxValue)
  : AudioParamTimeline(aDefaultValue, aMinValue, aMaxValue)
  , mContext(aContext)
{
  SetIsDOMBinding();
}

AudioParam::~AudioParam()
{
}

JSObject*
AudioParam::WrapObject(JSContext* aCx, JSObject* aScope,
                         bool* aTriedToWrap)
{
  return AudioParamBinding::Wrap(aCx, aScope, this, aTriedToWrap);
}

}
}

