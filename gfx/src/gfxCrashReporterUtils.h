/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Corporation code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
