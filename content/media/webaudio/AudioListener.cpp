/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioListener.h"
#include "AudioContext.h"
#include "mozilla/dom/AudioListenerBinding.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(AudioListener, mContext)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(AudioListener, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(AudioListener, Release)

AudioListener::AudioListener(AudioContext* aContext)
  : mContext(aContext)
  , mPosition()
  , mFrontVector(0., 0., -1.)
  , mRightVector(1., 0., 0.)
  , mVelocity()
  , mDopplerFactor(1.)
  , mSpeedOfSound(343.3) // meters/second
{
  MOZ_ASSERT(aContext);
  SetIsDOMBinding();
}

JSObject*
AudioListener::WrapObject(JSContext* aCx)
{
  return AudioListenerBinding::Wrap(aCx, this);
}

void
AudioListener::SetOrientation(double aX, double aY, double aZ,
                              double aXUp, double aYUp, double aZUp)
{
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
    SendThreeDPointParameterToStream(PannerNode::LISTENER_FRONT_VECTOR, front);
  }
  if (!mRightVector.FuzzyEqual(right)) {
    mRightVector = right;
    SendThreeDPointParameterToStream(PannerNode::LISTENER_RIGHT_VECTOR, right);
  }
}

void
AudioListener::RegisterPannerNode(PannerNode* aPannerNode)
{
  mPanners.AppendElement(aPannerNode);

  // Let the panner node know about our parameters
  aPannerNode->SendThreeDPointParameterToStream(PannerNode::LISTENER_POSITION, mPosition);
  aPannerNode->SendThreeDPointParameterToStream(PannerNode::LISTENER_FRONT_VECTOR, mFrontVector);
  aPannerNode->SendThreeDPointParameterToStream(PannerNode::LISTENER_RIGHT_VECTOR, mRightVector);
  aPannerNode->SendThreeDPointParameterToStream(PannerNode::LISTENER_VELOCITY, mVelocity);
  aPannerNode->SendDoubleParameterToStream(PannerNode::LISTENER_DOPPLER_FACTOR, mDopplerFactor);
  aPannerNode->SendDoubleParameterToStream(PannerNode::LISTENER_SPEED_OF_SOUND, mSpeedOfSound);
  UpdatePannersVelocity();
}

void AudioListener::UnregisterPannerNode(PannerNode* aPannerNode)
{
  mPanners.RemoveElement(aPannerNode);
}

void
AudioListener::SendDoubleParameterToStream(uint32_t aIndex, double aValue)
{
  for (uint32_t i = 0; i < mPanners.Length(); ++i) {
    if (mPanners[i]) {
      mPanners[i]->SendDoubleParameterToStream(aIndex, aValue);
    }
  }
}

void
AudioListener::SendThreeDPointParameterToStream(uint32_t aIndex, const ThreeDPoint& aValue)
{
  for (uint32_t i = 0; i < mPanners.Length(); ++i) {
    if (mPanners[i]) {
      mPanners[i]->SendThreeDPointParameterToStream(aIndex, aValue);
    }
  }
}

void AudioListener::UpdatePannersVelocity()
{
  for (uint32_t i = 0; i < mPanners.Length(); ++i) {
    if (mPanners[i]) {
      mPanners[i]->SendDopplerToSourcesIfNeeded();
    }
  }
}

size_t
AudioListener::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
  size_t amount = aMallocSizeOf(this);
  // AudioNodes are tracked separately
  amount += mPanners.SizeOfExcludingThis(aMallocSizeOf);
  return amount;
}

}
}

