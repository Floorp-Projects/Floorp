/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsTimingFunction_h
#define nsTimingFunction_h

#include "nsStyleConsts.h"

struct nsTimingFunction
{
  enum class Type {
    Ease,         // ease
    Linear,       // linear
    EaseIn,       // ease-in
    EaseOut,      // ease-out
    EaseInOut,    // ease-in-out
    StepStart,    // step-start and steps(..., start)
    StepEnd,      // step-end, steps(..., end) and steps(...)
    CubicBezier,  // cubic-bezier()
    Frames,       // frames()
  };

  // Whether the timing function type is represented by a spline,
  // and thus will have mFunc filled in.
  static bool IsSplineType(Type aType)
  {
    return aType != Type::StepStart &&
           aType != Type::StepEnd &&
           aType != Type::Frames;
  }

  explicit nsTimingFunction(
    int32_t aTimingFunctionType = NS_STYLE_TRANSITION_TIMING_FUNCTION_EASE)
    : mType(Type::Ease)
    , mFunc{}
  {
    AssignFromKeyword(aTimingFunctionType);
  }

  nsTimingFunction(float x1, float y1, float x2, float y2)
    : mType(Type::CubicBezier)
  {
    mFunc.mX1 = x1;
    mFunc.mY1 = y1;
    mFunc.mX2 = x2;
    mFunc.mY2 = y2;
  }

  enum class Keyword { Implicit, Explicit };

  nsTimingFunction(Type aType, uint32_t aStepsOrFrames)
    : mType(aType)
  {
    MOZ_ASSERT(mType == Type::StepStart ||
               mType == Type::StepEnd ||
               mType == Type::Frames,
               "wrong type");
    mStepsOrFrames = aStepsOrFrames;
  }

  nsTimingFunction(const nsTimingFunction& aOther)
  {
    *this = aOther;
  }

  Type mType;
  union {
    struct {
      float mX1;
      float mY1;
      float mX2;
      float mY2;
    } mFunc;
    struct {
      uint32_t mStepsOrFrames;
    };
  };

  nsTimingFunction&
  operator=(const nsTimingFunction& aOther)
  {
    if (&aOther == this) {
      return *this;
    }

    mType = aOther.mType;

    if (HasSpline()) {
      mFunc.mX1 = aOther.mFunc.mX1;
      mFunc.mY1 = aOther.mFunc.mY1;
      mFunc.mX2 = aOther.mFunc.mX2;
      mFunc.mY2 = aOther.mFunc.mY2;
    } else {
      mStepsOrFrames = aOther.mStepsOrFrames;
    }

    return *this;
  }

  bool operator==(const nsTimingFunction& aOther) const
  {
    if (mType != aOther.mType) {
      return false;
    }
    if (HasSpline()) {
      return mFunc.mX1 == aOther.mFunc.mX1 && mFunc.mY1 == aOther.mFunc.mY1 &&
             mFunc.mX2 == aOther.mFunc.mX2 && mFunc.mY2 == aOther.mFunc.mY2;
    }
    return mStepsOrFrames == aOther.mStepsOrFrames;
  }

  bool operator!=(const nsTimingFunction& aOther) const
  {
    return !(*this == aOther);
  }

  bool HasSpline() const { return IsSplineType(mType); }

private:
  void AssignFromKeyword(int32_t aTimingFunctionType);
};

#endif // nsTimingFunction_h
