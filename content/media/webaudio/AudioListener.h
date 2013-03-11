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

  virtual JSObject* WrapObject(JSContext* aCx, JSObject* aScope) MOZ_OVERRIDE;

  double DopplerFactor() const
  {
    return mDopplerFactor;
  }
  void SetDopplerFactor(double aDopplerFactor)
  {
    mDopplerFactor = aDopplerFactor;
  }

  double SpeedOfSound() const
  {
    return mSpeedOfSound;
  }
  void SetSpeedOfSound(double aSpeedOfSound)
  {
    mSpeedOfSound = aSpeedOfSound;
  }

  void SetPosition(double aX, double aY, double aZ)
  {
    mPosition.x = aX;
    mPosition.y = aY;
    mPosition.z = aZ;
  }

  void SetOrientation(double aX, double aY, double aZ,
                      double aXUp, double aYUp, double aZUp)
  {
    mOrientation.x = aX;
    mOrientation.y = aY;
    mOrientation.z = aZ;
    mUpVector.x = aXUp;
    mUpVector.y = aYUp;
    mUpVector.z = aZUp;
  }

  void SetVelocity(double aX, double aY, double aZ)
  {
    mVelocity.x = aX;
    mVelocity.y = aY;
    mVelocity.z = aZ;
  }

private:
  nsRefPtr<AudioContext> mContext;
  ThreeDPoint mPosition;
  ThreeDPoint mOrientation;
  ThreeDPoint mUpVector;
  ThreeDPoint mVelocity;
  double mDopplerFactor;
  double mSpeedOfSound;
};

}
}

#endif

