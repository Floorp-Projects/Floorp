/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_CAMERA_CAMERAPREFERENCES_H
#define DOM_CAMERA_CAMERAPREFERENCES_H

#include "nsString.h"
#include "nsIObserver.h"

#if defined(MOZ_HAVE_CXX11_STRONG_ENUMS) || defined(MOZ_HAVE_CXX11_ENUM_TYPE)
// Older compilers that don't support strongly-typed enums
// just typedef uint32_t to nsresult, which results in conflicting
// overloaded members in CameraPreferences.
#define CAMERAPREFERENCES_HAVE_SEPARATE_UINT32_AND_NSRESULT
#endif

namespace mozilla {

template<class T> class StaticAutoPtr;

class CameraPreferences
#ifdef MOZ_WIDGET_GONK
  : public nsIObserver
#endif
{
public:
#ifdef MOZ_WIDGET_GONK
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER
#endif

  static bool Initialize();
  static void Shutdown();

  static bool GetPref(const char* aPref, nsACString& aVal);
#ifdef CAMERAPREFERENCES_HAVE_SEPARATE_UINT32_AND_NSRESULT
  static bool GetPref(const char* aPref, nsresult& aVal);
#endif
  static bool GetPref(const char* aPref, uint32_t& aVal);
  static bool GetPref(const char* aPref, bool& aVal);

protected:
  static const uint32_t kPrefNotFound = UINT32_MAX;
  static uint32_t PrefToIndex(const char* aPref);

  static void PreferenceChanged(const char* aPref, void* aClosure);
#ifdef CAMERAPREFERENCES_HAVE_SEPARATE_UINT32_AND_NSRESULT
  static nsresult UpdatePref(const char* aPref, nsresult& aVar);
#endif
  static nsresult UpdatePref(const char* aPref, uint32_t& aVar);
  static nsresult UpdatePref(const char* aPref, nsACString& aVar);
  static nsresult UpdatePref(const char* aPref, bool& aVar);

  enum PrefValueType {
    kPrefValueIsNsResult,
    kPrefValueIsUint32,
    kPrefValueIsCString,
    kPrefValueIsBoolean
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
      uint32_t*                 mAsUint32;
      bool*                     mAsBoolean;
    } mValue;
  };
  static Pref sPrefs[];

  static StaticAutoPtr<nsCString> sPrefTestEnabled;
  static StaticAutoPtr<nsCString> sPrefHardwareTest;
  static StaticAutoPtr<nsCString> sPrefGonkParameters;

  static nsresult sPrefCameraControlMethodErrorOverride;
  static nsresult sPrefCameraControlAsyncErrorOverride;

  static uint32_t sPrefCameraControlLowMemoryThresholdMB;

  static bool sPrefCameraParametersIsLowMemory;

#ifdef MOZ_WIDGET_GONK
  static StaticRefPtr<CameraPreferences> sObserver;

  nsresult PreinitCameraHardware();

protected:
  // Objects may be instantiated for use as observers.
  CameraPreferences() { }
  ~CameraPreferences() { }
#else
private:
  // Static class only.
  CameraPreferences();
  ~CameraPreferences();
#endif
};

} // namespace mozilla

#endif // DOM_CAMERA_CAMERAPREFERENCES_H
