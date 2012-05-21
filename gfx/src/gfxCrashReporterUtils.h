/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef gfxCrashReporterUtils_h__
#define gfxCrashReporterUtils_h__

#include "gfxCore.h"

namespace mozilla {

/** \class ScopedGfxFeatureReporter
  *
  * On creation, adds "FeatureName?" to AppNotes
  * On destruction, adds "FeatureName-", or "FeatureName+" if you called SetSuccessful().
  *
  * Any such string is added at most once to AppNotes, and is subsequently skipped.
  *
  * This ScopedGfxFeatureReporter class is designed to be fool-proof to use in functions that
  * have many exit points. We don't want to encourage having function with many exit points.
  * It just happens that our graphics features initialization functions are like that.
  */
class NS_GFX ScopedGfxFeatureReporter
{
public:
  ScopedGfxFeatureReporter(const char *aFeature, bool force = false)
    : mFeature(aFeature), mStatusChar('-')
  {
    WriteAppNote(force ? '!' : '?');
  }
  ~ScopedGfxFeatureReporter() {
    WriteAppNote(mStatusChar);
  }
  void SetSuccessful() { mStatusChar = '+'; }

protected:
  const char *mFeature;
  char mStatusChar;

private:
  void WriteAppNote(char statusChar);
};

} // end namespace mozilla

#endif // gfxCrashReporterUtils_h__
