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
#include "AudioContext.h"
#include "mozilla/dom/TypedArray.h"
#include "mozilla/Util.h"

struct JSContext;
class nsIDOMWindow;

namespace mozilla {

class ErrorResult;

namespace dom {

namespace detail {

// This class wraps a JS typed array so that AudioEventTimeline does not need
// to know about JSObjects directly.
class FloatArrayWrapper
{
public:
  FloatArrayWrapper() // creates an uninitialized object
  {
  }
  FloatArrayWrapper(const Float32Array& array)
  {
    mArray.construct(array);
  }
  FloatArrayWrapper(const FloatArrayWrapper& rhs)
  {
    MOZ_ASSERT(!rhs.mArray.empty());
    mArray.construct(rhs.mArray.ref());
  }

  FloatArrayWrapper& operator=(const FloatArrayWrapper& rhs)
  {
    MOZ_ASSERT(!rhs.mArray.empty());
    mArray.destroyIfConstructed();
    mArray.construct(rhs.mArray.ref());
    return *this;
  }

  // Expose the API used by AudioEventTimeline
  float* Data() const
  {
    MOZ_ASSERT(inited());
    return mArray.ref().Data();
  }
  uint32_t Length() const
  {
    MOZ_ASSERT(inited());
    return mArray.ref().Length();
  }
  bool inited() const
  {
    return !mArray.empty();
  }

private:
  Maybe<Float32Array> mArray;
};

}

typedef AudioEventTimeline<detail::FloatArrayWrapper, ErrorResult> AudioParamTimeline;

class AudioParam MOZ_FINAL : public nsWrapperCache,
                             public EnableWebAudioCheck,
                             public AudioParamTimeline
{
public:
  AudioParam(AudioContext* aContext,
             float aDefaultValue,
             float aMinValue,
             float aMaxValue);
  virtual ~AudioParam();

  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(AudioParam)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(AudioParam)

  AudioContext* GetParentObject() const
  {
    return mContext;
  }

  virtual JSObject* WrapObject(JSContext* aCx, JSObject* aScope,
                               bool* aTriedToWrap);

  // We override SetValueCurveAtTime to convert the Float32Array to the wrapper
  // object.
  void SetValueCurveAtTime(JSContext* cx, const Float32Array& aValues, double aStartTime, double aDuration, ErrorResult& aRv)
  {
    AudioParamTimeline::SetValueCurveAtTime(detail::FloatArrayWrapper(aValues),
                                            aStartTime, aDuration, aRv);
  }

private:
  nsRefPtr<AudioContext> mContext;
};

}
}

#endif

