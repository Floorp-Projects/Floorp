/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioContext.h"
#include "AudioListener.h"
#include "MediaTrackGraph.h"
#include "Tracing.h"
#include "mozilla/dom/AudioListenerBinding.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(AudioListener, mContext)

AudioListenerEngine::AudioListenerEngine()
    : mFrontVector(0., 0., -1.), mRightVector(1., 0., 0.) {}

void AudioListenerEngine::RecvListenerEngineEvent(
    AudioListenerEngine::AudioListenerParameter aParameter,
    const ThreeDPoint& aValue) {
  switch (aParameter) {
    case AudioListenerParameter::POSITION:
      mPosition = aValue;
      break;
    case AudioListenerParameter::FRONT:
      mFrontVector = aValue;
      break;
    case AudioListenerParameter::RIGHT:
      mRightVector = aValue;
      break;
    default:
      MOZ_CRASH("Not handled");
  }
}

const ThreeDPoint& AudioListenerEngine::Position() const { return mPosition; }
const ThreeDPoint& AudioListenerEngine::FrontVector() const {
  return mFrontVector;
}
const ThreeDPoint& AudioListenerEngine::RightVector() const {
  return mRightVector;
}

AudioListener::AudioListener(AudioContext* aContext)
    : mContext(aContext),
      mEngine(new AudioListenerEngine()),

      mFrontVector(0., 0., -1.),
      mRightVector(1., 0., 0.) {
  MOZ_ASSERT(aContext);
}

JSObject* AudioListener::WrapObject(JSContext* aCx,
                                    JS::Handle<JSObject*> aGivenProto) {
  return AudioListener_Binding::Wrap(aCx, this, aGivenProto);
}

void AudioListener::SetOrientation(double aX, double aY, double aZ, double aXUp,
                                   double aYUp, double aZUp) {
  ThreeDPoint front(aX, aY, aZ);
  // The panning effect and the azimuth and elevation calculation in the Web
  // Audio spec becomes undefined with linearly dependent vectors, so keep
  // existing state in these situations.
  if (front.IsZero()) {
    return;
  }
  // Normalize before using CrossProduct() to avoid overflow.
  front.Normalize();
  ThreeDPoint up(aXUp, aYUp, aZUp);
  if (up.IsZero()) {
    return;
  }
  up.Normalize();
  ThreeDPoint right = front.CrossProduct(up);
  if (right.IsZero()) {
    return;
  }
  right.Normalize();

  if (!mFrontVector.FuzzyEqual(front)) {
    mFrontVector = front;
    SendListenerEngineEvent(AudioListenerEngine::AudioListenerParameter::FRONT,
                            mFrontVector);
  }
  if (!mRightVector.FuzzyEqual(right)) {
    mRightVector = right;
    SendListenerEngineEvent(AudioListenerEngine::AudioListenerParameter::RIGHT,
                            mRightVector);
  }
}

void AudioListener::SetPosition(double aX, double aY, double aZ) {
  if (WebAudioUtils::FuzzyEqual(mPosition.x, aX) &&
      WebAudioUtils::FuzzyEqual(mPosition.y, aY) &&
      WebAudioUtils::FuzzyEqual(mPosition.z, aZ)) {
    return;
  }
  mPosition.x = aX;
  mPosition.y = aY;
  mPosition.z = aZ;
  SendListenerEngineEvent(AudioListenerEngine::AudioListenerParameter::POSITION,
                          mPosition);
}

void AudioListener::SendListenerEngineEvent(
    AudioListenerEngine::AudioListenerParameter aParameter,
    const ThreeDPoint& aValue) {
  mContext->DestinationTrack()->QueueControlMessageWithNoShutdown(
      [engine = RefPtr(Engine()), aParameter, aValue] {
        TRACE("AudioListener::RecvListenerEngineEvent");
        engine->RecvListenerEngineEvent(aParameter, aValue);
      });
}

size_t AudioListener::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const {
  return aMallocSizeOf(this);
}

}  // namespace mozilla::dom
