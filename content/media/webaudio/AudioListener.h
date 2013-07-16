/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AudioListener_h_
#define AudioListener_h_

#include "nsWrapperCache.h"
#include "nsCycleCollectionParticipant.h"
#include "mozilla/Attributes.h"
#include "EnableWebAudioCheck.h"
#include "nsAutoPtr.h"
#include "ThreeDPoint.h"
#include "AudioContext.h"
#include "PannerNode.h"
#include "WebAudioUtils.h"

struct JSContext;

namespace mozilla {

namespace dom {

class AudioContext;

class AudioListener MOZ_FINAL : public nsWrapperCache,
                                public EnableWebAudioCheck
{
public:
  explicit AudioListener(AudioContext* aContext);

  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(AudioListener)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(AudioListener)

  AudioContext* GetParentObject() const
  {
    return mContext;
  }

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;

  double DopplerFactor() const
  {
    return mDopplerFactor;
  }
  void SetDopplerFactor(double aDopplerFactor)
  {
    if (WebAudioUtils::FuzzyEqual(mDopplerFactor, aDopplerFactor)) {
      return;
    }
    mDopplerFactor = aDopplerFactor;
    SendDoubleParameterToStream(PannerNode::LISTENER_DOPPLER_FACTOR, mDopplerFactor);
  }

  double SpeedOfSound() const
  {
    return mSpeedOfSound;
  }
  void SetSpeedOfSound(double aSpeedOfSound)
  {
    if (WebAudioUtils::FuzzyEqual(mSpeedOfSound, aSpeedOfSound)) {
      return;
    }
    mSpeedOfSound = aSpeedOfSound;
    SendDoubleParameterToStream(PannerNode::LISTENER_SPEED_OF_SOUND, mSpeedOfSound);
  }

  void SetPosition(double aX, double aY, double aZ)
  {
    if (WebAudioUtils::FuzzyEqual(mPosition.x, aX) &&
        WebAudioUtils::FuzzyEqual(mPosition.y, aY) &&
        WebAudioUtils::FuzzyEqual(mPosition.z, aZ)) {
      return;
    }
    mPosition.x = aX;
    mPosition.y = aY;
    mPosition.z = aZ;
    SendThreeDPointParameterToStream(PannerNode::LISTENER_POSITION, mPosition);
  }

  const ThreeDPoint& Position() const
  {
    return mPosition;
  }

  void SetOrientation(double aX, double aY, double aZ,
                      double aXUp, double aYUp, double aZUp)
  {
    if (WebAudioUtils::FuzzyEqual(mOrientation.x, aX) &&
        WebAudioUtils::FuzzyEqual(mOrientation.y, aY) &&
        WebAudioUtils::FuzzyEqual(mOrientation.z, aZ) &&
        WebAudioUtils::FuzzyEqual(mUpVector.x, aX) &&
        WebAudioUtils::FuzzyEqual(mUpVector.y, aY) &&
        WebAudioUtils::FuzzyEqual(mUpVector.z, aZ)) {
      return;
    }
    mOrientation.x = aX;
    mOrientation.y = aY;
    mOrientation.z = aZ;
    mUpVector.x = aXUp;
    mUpVector.y = aYUp;
    mUpVector.z = aZUp;
    SendThreeDPointParameterToStream(PannerNode::LISTENER_ORIENTATION, mOrientation);
    SendThreeDPointParameterToStream(PannerNode::LISTENER_UPVECTOR, mUpVector);
  }

  const ThreeDPoint& Velocity() const
  {
    return mVelocity;
  }

  void SetVelocity(double aX, double aY, double aZ)
  {
    if (WebAudioUtils::FuzzyEqual(mVelocity.x, aX) &&
        WebAudioUtils::FuzzyEqual(mVelocity.y, aY) &&
        WebAudioUtils::FuzzyEqual(mVelocity.z, aZ)) {
      return;
    }
    mVelocity.x = aX;
    mVelocity.y = aY;
    mVelocity.z = aZ;
    SendThreeDPointParameterToStream(PannerNode::LISTENER_VELOCITY, mVelocity);
    UpdatePannersVelocity();
  }

  void RegisterPannerNode(PannerNode* aPannerNode);
  void UnregisterPannerNode(PannerNode* aPannerNode);

private:
  void SendDoubleParameterToStream(uint32_t aIndex, double aValue);
  void SendThreeDPointParameterToStream(uint32_t aIndex, const ThreeDPoint& aValue);
  void UpdatePannersVelocity();

private:
  friend class PannerNode;
  nsRefPtr<AudioContext> mContext;
  ThreeDPoint mPosition;
  ThreeDPoint mOrientation;
  ThreeDPoint mUpVector;
  ThreeDPoint mVelocity;
  double mDopplerFactor;
  double mSpeedOfSound;
  nsTArray<WeakPtr<PannerNode> > mPanners;
};

}
}

#endif

