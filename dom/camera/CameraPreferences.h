/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_CAMERA_CAMERAPREFERENCES_H
#define DOM_CAMERA_CAMERAPREFERENCES_H

#include "nsString.h"

namespace mozilla {

template<class T> class StaticAutoPtr;

class CameraPreferences
{
public:
  static bool Initialize();
  static void Shutdown();

  static bool GetPref(const char* aPref, nsACString& aVal);
  static bool GetPref(const char* aPref, nsresult& aVal);

protected:
  static const uint32_t kPrefNotFound = UINT32_MAX;
  static uint32_t PrefToIndex(const char* aPref);

  static void PreferenceChanged(const char* aPref, void* aClosure);
  static nsresult UpdatePref(const char* aPref, nsresult& aVar);
  static nsresult UpdatePref(const char* aPref, nsACString& aVar);

  enum PrefValueType {
    kPrefValueIsNSResult,
    kPrefValueIsCString
  };
  struct Pref {
    const char* const           mPref;
    PrefValueType               mValueType;
    union {
      // The 'mAsVoid' member must be first and is required to allow 'mValue'
      // to be initialized with any pointer type, as not all of our platforms
      // support the use of designated initializers; in their absence, only
      // the first element of a union can be statically initialized, and
      // 'void*' lets us stuff any pointer type into it.
      void*                     mAsVoid;
      StaticAutoPtr<nsCString>* mAsCString;
      nsresult*                 mAsNsResult;
    } mValue;
  };
  static Pref sPrefs[];

  static StaticAutoPtr<nsCString> sPrefTestEnabled;
  static StaticAutoPtr<nsCString> sPrefHardwareTest;
  static StaticAutoPtr<nsCString> sPrefGonkParameters;

  static nsresult sPrefCameraControlMethodErrorOverride;
  static nsresult sPrefCameraControlAsyncErrorOverride;

private:
  // static class only
  CameraPreferences();
  ~CameraPreferences();
};

} // namespace mozilla

#endif // DOM_CAMERA_CAMERAPREFERENCES_H
