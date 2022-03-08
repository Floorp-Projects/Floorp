/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_OriginTrials_h
#define mozilla_OriginTrials_h

#include "mozilla/origin_trials_ffi_generated.h"
#include "mozilla/EnumSet.h"
#include "nsStringFwd.h"

class nsIPrincipal;
struct JSContext;
class JSObject;

namespace mozilla {

using OriginTrial = origin_trials_ffi::OriginTrial;

// A class that keeps a set of enabled trials / features for a particular
// origin.
//
// These allow sites to opt-in and provide feedback into experimental features
// before we ship it to the general public.
class OriginTrials final {
 public:
  // Parses and verifies a base64-encoded token from either a header or a meta
  // tag. If the token is valid and not expired, this will enable the relevant
  // feature.
  void UpdateFromToken(const nsAString& aBase64EncodedToken,
                       nsIPrincipal* aPrincipal);

  bool IsEnabled(OriginTrial aTrial) const {
    return mEnabledTrials.contains(aTrial);
  }

  // Checks whether a given origin trial is enabled for a given call.
  static bool IsEnabled(JSContext*, JSObject*, OriginTrial);

 private:
  EnumSet<OriginTrial> mEnabledTrials;
};

}  // namespace mozilla

#endif
