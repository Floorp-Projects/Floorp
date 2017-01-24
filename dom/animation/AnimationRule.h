/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_AnimationRule_h
#define mozilla_AnimationRule_h

#include "AnimValuesStyleRule.h"
#include "ServoAnimationRule.h"

namespace mozilla {

// A wrapper for animation rules.
struct AnimationRule
{
  RefPtr<AnimValuesStyleRule> mGecko;
  RefPtr<ServoAnimationRule> mServo;
};

} // namespace mozilla

#endif // mozilla_AnimationRule_h
