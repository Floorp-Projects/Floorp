/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AudioParam_h_
#define AudioParam_h_

#include "AudioEventTimeline.h"
#include "nsWrapperCache.h"
#include "nsCycleCollectionParticipant.h"
#include "nsCOMPtr.h"
#include "EnableWebAudioCheck.h"
#include "nsAutoPtr.h"
#include "AudioNode.h"
#include "mozilla/dom/TypedArray.h"
#include "mozilla/Util.h"
#include "mozilla/ErrorResult.h"
#include "WebAudioUtils.h"

struct JSContext;
class nsIDOMWindow;

namespace mozilla {

namespace dom {

typedef AudioEventTimeline<ErrorResult> AudioParamTimeline;

class AudioParam MOZ_FINAL : public nsWrapperCache,
                             public EnableWebAudioCheck,
                             public AudioParamTimeline
{
public:
  typedef void (*CallbackType)(AudioNode*);

  AudioParam(AudioNode* aNode,
             CallbackType aCallback,
             float aDefaultValue,
             float aMinValue,
             float aMaxValue);
  virtual ~AudioParam();

  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(AudioParam)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(AudioParam)

  AudioContext* GetParentObject() const
  {
    return mNode->Context();
  }

  virtual JSObject* WrapObject(JSContext* aCx, JSObject* aScope) MOZ_OVERRIDE;

  // We override SetValueCurveAtTime to convert the Float32Array to the wrapper
  // object.
  void SetValueCurveAtTime(JSContext* cx, const Float32Array& aValues, double aStartTime, double aDuration, ErrorResult& aRv)
  {
    AudioParamTimeline::SetValueCurveAtTime(aValues.Data(), aValues.Length(),
                                            aStartTime, aDuration, aRv);
    mCallback(mNode);
  }

  // We override the rest of the mutating AudioParamTimeline methods in order to make
  // sure that the callback is called every time that this object gets mutated.
  void SetValue(float aValue)
  {
    // Optimize away setting the same value on an AudioParam
    if (HasSimpleValue() &&
        WebAudioUtils::FuzzyEqual(GetValue(), aValue)) {
      return;
    }
    AudioParamTimeline::SetValue(aValue);
    mCallback(mNode);
  }
  void SetValueAtTime(float aValue, double aStartTime, ErrorResult& aRv)
  {
    AudioParamTimeline::SetValueAtTime(aValue, aStartTime, aRv);
    mCallback(mNode);
  }
  void LinearRampToValueAtTime(float aValue, double aEndTime, ErrorResult& aRv)
  {
    AudioParamTimeline::LinearRampToValueAtTime(aValue, aEndTime, aRv);
    mCallback(mNode);
  }
  void ExponentialRampToValueAtTime(float aValue, double aEndTime, ErrorResult& aRv)
  {
    AudioParamTimeline::ExponentialRampToValueAtTime(aValue, aEndTime, aRv);
    mCallback(mNode);
  }
  void SetTargetAtTime(float aTarget, double aStartTime, double aTimeConstant, ErrorResult& aRv)
  {
    AudioParamTimeline::SetTargetAtTime(aTarget, aStartTime, aTimeConstant, aRv);
    mCallback(mNode);
  }
  void CancelScheduledValues(double aStartTime)
  {
    AudioParamTimeline::CancelScheduledValues(aStartTime);
    mCallback(mNode);
  }

  float MinValue() const
  {
    return mMinValue;
  }

  float MaxValue() const
  {
    return mMaxValue;
  }

  float DefaultValue() const
  {
    return mDefaultValue;
  }

private:
  nsRefPtr<AudioNode> mNode;
  CallbackType mCallback;
  const float mDefaultValue;
  const float mMinValue;
  const float mMaxValue;
};

}
}

#endif

