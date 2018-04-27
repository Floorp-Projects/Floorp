/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AndroidFlingPhysics.h"

#include <cmath>

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/StaticPtr.h"

namespace mozilla {
namespace layers {

// The fling physics calculations implemented here are adapted from
// Chrome's implementation of fling physics on Android:
// https://cs.chromium.org/chromium/src/ui/events/android/scroller.cc?rcl=3ae3aaff927038a5c644926842cb0c31dea60c79

static double ComputeDeceleration(float aDPI)
{
  const float kFriction = 0.84f;
  const float kGravityEarth = 9.80665f;
  return kGravityEarth  // g (m/s^2)
         * 39.37f       // inch/meter
         * aDPI         // pixels/inch
         * kFriction;
}

// == std::log(0.78f) / std::log(0.9f)
const float kDecelerationRate = 2.3582018f;

// Default friction constant in android.view.ViewConfiguration.
static float GetFlingFriction()
{
  return gfxPrefs::APZChromeFlingPhysicsFriction();
}

// Tension lines cross at (GetInflexion(), 1).
static float GetInflexion()
{
  // Clamp the inflexion to the range [0,1]. Values outside of this range
  // do not make sense in the physics model, and for negative values the
  // approximation used to compute the spline curve does not converge.
  const float inflexion = gfxPrefs::APZChromeFlingPhysicsInflexion();
  if (inflexion < 0.0f) {
    return 0.0f;
  }
  if (inflexion > 1.0f) {
    return 1.0f;
  }
  return inflexion;
}

// Fling scroll is stopped when the scroll position is |kThresholdForFlingEnd|
// pixels or closer from the end.
static float GetThresholdForFlingEnd()
{
  return gfxPrefs::APZChromeFlingPhysicsStopThreshold();
}

static double ComputeSplineDeceleration(ParentLayerCoord aVelocity, double aTuningCoeff)
{
  float velocityPerSec = aVelocity * 1000.0f;
  return std::log(GetInflexion() * velocityPerSec / (GetFlingFriction() * aTuningCoeff));
}

static TimeDuration ComputeFlingDuration(ParentLayerCoord aVelocity, double aTuningCoeff)
{
  const double splineDecel = ComputeSplineDeceleration(aVelocity, aTuningCoeff);
  const double timeSeconds = std::exp(splineDecel / (kDecelerationRate - 1.0));
  return TimeDuration::FromSeconds(timeSeconds);
}

static ParentLayerCoord ComputeFlingDistance(ParentLayerCoord aVelocity, double aTuningCoeff)
{
  const double splineDecel = ComputeSplineDeceleration(aVelocity, aTuningCoeff);
  return GetFlingFriction() * aTuningCoeff *
      std::exp(kDecelerationRate / (kDecelerationRate - 1.0) * splineDecel);
}

struct SplineConstants {
public:
  SplineConstants() {
    const float kStartTension = 0.5f;
    const float kEndTension = 1.0f;
    const float kP1 = kStartTension * GetInflexion();
    const float kP2 = 1.0f - kEndTension * (1.0f - GetInflexion());

    float xMin = 0.0f;
    for (int i = 0; i < kNumSamples; i++) {
      const float alpha = static_cast<float>(i) / kNumSamples;

      float xMax = 1.0f;
      float x, tx, coef;
      // While the inflexion can be overridden by the user, it's clamped to
      // [0,1]. For values in this range, the approximation algorithm below
      // should converge in < 20 iterations. For good measure, we impose an
      // iteration limit as well.
      static const int sIterationLimit = 100;
      int iterations = 0;
      while (iterations++ < sIterationLimit) {
        x = xMin + (xMax - xMin) / 2.0f;
        coef = 3.0f * x * (1.0f - x);
        tx = coef * ((1.0f - x) * kP1 + x * kP2) + x * x * x;
        if (FuzzyEqualsAdditive(tx, alpha)) {
          break;
        }
        if (tx > alpha) {
          xMax = x;
        } else {
          xMin = x;
        }
      }
      mSplinePositions[i] = coef * ((1.0f - x) * kStartTension + x) + x * x * x;
    }
    mSplinePositions[kNumSamples] = 1.0f;
  }

  void CalculateCoefficients(float aTime,
                             float* aOutDistanceCoef,
                             float* aOutVelocityCoef)
  {
    *aOutDistanceCoef = 1.0f;
    *aOutVelocityCoef = 0.0f;
    const int index = static_cast<int>(kNumSamples * aTime);
    if (index < kNumSamples) {
      const float tInf = static_cast<float>(index) / kNumSamples;
      const float dInf = mSplinePositions[index];
      const float tSup = static_cast<float>(index + 1) / kNumSamples;
      const float dSup = mSplinePositions[index + 1];
      *aOutVelocityCoef = (dSup - dInf) / (tSup - tInf);
      *aOutDistanceCoef = dInf + (aTime - tInf) * *aOutVelocityCoef;
    }
  }
private:
  static const int kNumSamples = 100;
  float mSplinePositions[kNumSamples + 1];
};

StaticAutoPtr<SplineConstants> gSplineConstants;

/* static */ void AndroidFlingPhysics::InitializeGlobalState()
{
  gSplineConstants = new SplineConstants();
  ClearOnShutdown(&gSplineConstants);
}

void AndroidFlingPhysics::Init(const ParentLayerPoint& aStartingVelocity,
                               float aPLPPI)
{
  mVelocity = aStartingVelocity.Length();
  const double tuningCoeff = ComputeDeceleration(aPLPPI);
  mTargetDuration = ComputeFlingDuration(mVelocity, tuningCoeff);
  MOZ_ASSERT(!mTargetDuration.IsZero());
  mDurationSoFar = TimeDuration();
  mLastPos = ParentLayerPoint();
  mCurrentPos = ParentLayerPoint();
  float coeffX = mVelocity == 0 ? 1.0f : aStartingVelocity.x / mVelocity;
  float coeffY = mVelocity == 0 ? 1.0f : aStartingVelocity.y / mVelocity;
  mTargetDistance = ComputeFlingDistance(mVelocity, tuningCoeff);
  mTargetPos = ParentLayerPoint(mTargetDistance * coeffX,
                                mTargetDistance * coeffY);
  const float hyp = mTargetPos.Length();
  if (FuzzyEqualsAdditive(hyp, 0.0f)) {
    mDeltaNorm = ParentLayerPoint(1, 1);
  } else {
    mDeltaNorm = ParentLayerPoint(mTargetPos.x / hyp, mTargetPos.y / hyp);
  }
}
void AndroidFlingPhysics::Sample(const TimeDuration& aDelta,
                                 ParentLayerPoint* aOutVelocity,
                                 ParentLayerPoint* aOutOffset)
{
  float newVelocity;
  if (SampleImpl(aDelta, &newVelocity)) {
    *aOutOffset = (mCurrentPos - mLastPos);
    *aOutVelocity = ParentLayerPoint(mDeltaNorm.x * newVelocity,
                                     mDeltaNorm.y * newVelocity);
    mLastPos = mCurrentPos;
  } else {
    *aOutOffset = (mTargetPos - mLastPos);
    *aOutVelocity = ParentLayerPoint();
  }
}

bool AndroidFlingPhysics::SampleImpl(const TimeDuration& aDelta,
                                     float* aOutVelocity)
{
  mDurationSoFar += aDelta;
  if (mDurationSoFar >= mTargetDuration) {
    return false;
  }

  const float timeRatio = mDurationSoFar.ToSeconds() / mTargetDuration.ToSeconds();
  float distanceCoef = 1.0f;
  float velocityCoef = 0.0f;
  gSplineConstants->CalculateCoefficients(timeRatio, &distanceCoef, &velocityCoef);

  // The caller expects the velocity in pixels per _millisecond_.
  *aOutVelocity = velocityCoef * mTargetDistance / mTargetDuration.ToMilliseconds();

  mCurrentPos = mTargetPos * distanceCoef;

  ParentLayerPoint remainder = mTargetPos - mCurrentPos;
  const float threshold = GetThresholdForFlingEnd();
  if (fabsf(remainder.x) < threshold && fabsf(remainder.y) < threshold) {
    return false;
  }

  return true;
}


} // namespace layers
} // namespace mozilla
