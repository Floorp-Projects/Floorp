/*
 * Copyright (C) 2013-2014 Mozilla Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef DOM_CAMERA_GONKCAMERAPARAMETERS_H
#define DOM_CAMERA_GONKCAMERAPARAMETERS_H

#include <math.h>
#include "camera/CameraParameters.h"
#include "nsTArray.h"
#include "nsString.h"
#include "AutoRwLock.h"
#include "nsPrintfCString.h"
#include "nsClassHashtable.h"
#include "ICameraControl.h"

namespace mozilla {

class GonkCameraParameters
{
public:
  GonkCameraParameters();
  virtual ~GonkCameraParameters();

  // IMPORTANT: This class is read and written by multiple threads --
  // ALL public methods must hold mLock, for either reading or writing,
  // for the life of their operation. Not doing so was the cause of
  // bug 928856, which was -painful- to track down.
  //
  // Return values:
  //  - see return values for GetTranslated() and SetTranslated() below.
  template<class T> nsresult
  Set(uint32_t aKey, const T& aValue)
  {
    RwLockAutoEnterWrite lock(mLock);
    nsresult rv = SetTranslated(aKey, aValue);
    mDirty = mDirty || NS_SUCCEEDED(rv);
    return rv;
  }

  template<class T> nsresult
  Get(uint32_t aKey, T& aValue)
  {
    RwLockAutoEnterRead lock(mLock);
    return GetTranslated(aKey, aValue);
  }

  bool
  TestAndClearDirtyFlag()
  {
    bool dirty;

    RwLockAutoEnterWrite lock(mLock);
    dirty = mDirty;
    mDirty = false;
    return dirty;
  }

  android::String8
  Flatten() const
  {
    RwLockAutoEnterRead lock(mLock);
    return mParams.flatten();
  }

  nsresult
  Unflatten(const android::String8& aFlatParameters)
  {
    RwLockAutoEnterWrite lock(mLock);
    mParams.unflatten(aFlatParameters);
    if (mInitialized) {
      return NS_OK;
    }

    // We call Initialize() once when the parameter set is first loaded,
    // to set up any constant values this class requires internally,
    // e.g. the exposure compensation step and limits.
    return Initialize();
  }

protected:
  PRRWLock* mLock;
  bool mDirty;
  bool mInitialized;

  // Required internal properties
  double mExposureCompensationStep;
  int32_t mExposureCompensationMinIndex;
  int32_t mExposureCompensationMaxIndex;
  nsTArray<int> mZoomRatios;
  nsTArray<nsString> mIsoModes;
  nsTArray<nsString> mSceneModes;
  nsClassHashtable<nsStringHashKey, nsCString> mIsoModeMap;

  // This subclass of android::CameraParameters just gives
  // all of the AOSP getters and setters the same signature.
  class Parameters : public android::CameraParameters
  {
  public:
    using android::CameraParameters::set;
    using android::CameraParameters::get;
    using android::CameraParameters::TRUE;
    using android::CameraParameters::FALSE;

    void set(const char* aKey, float aValue)      { setFloat(aKey, aValue); }
    void set(const char* aKey, double aValue)     { setFloat(aKey, aValue); }
    void set(const char* aKey, bool aValue)       { set(aKey, aValue ? TRUE : FALSE); }
    void get(const char* aKey, float& aRet)       { aRet = getFloat(aKey); }
    void get(const char* aKey, double& aRet)      { aRet = getFloat(aKey); }
    void get(const char* aKey, const char*& aRet) { aRet = get(aKey); }
    void get(const char* aKey, int& aRet)         { aRet = getInt(aKey); }

    void
    get(const char* aKey, bool& aRet)
    {
      const char* value = get(aKey);
      aRet = value ? strcmp(value, TRUE) == 0 : false;
    }

    void remove(const char* aKey)                 { android::CameraParameters::remove(aKey); }

    static const char* GetTextKey(uint32_t aKey);
  };

  Parameters mParams;

  // The *Impl() templates handle converting the parameter keys from
  // their enum values to string types, if necessary. These are the
  // bottom layer accessors to mParams.
  //
  // Return values:
  //  - NS_OK on success;
  //  - NS_ERROR_NOT_IMPLEMENTED if the numeric 'aKey' value is invalid.
  template<typename T> nsresult
  SetImpl(uint32_t aKey, const T& aValue)
  {
    const char* key = Parameters::GetTextKey(aKey);
    NS_ENSURE_TRUE(key, NS_ERROR_NOT_IMPLEMENTED);

    mParams.set(key, aValue);
    return NS_OK;
  }

  template<typename T> nsresult
  GetImpl(uint32_t aKey, T& aValue)
  {
    const char* key = Parameters::GetTextKey(aKey);
    NS_ENSURE_TRUE(key, NS_ERROR_NOT_IMPLEMENTED);

    mParams.get(key, aValue);
    return NS_OK;
  }

  template<class T> nsresult
  SetImpl(const char* aKey, const T& aValue)
  {
    mParams.set(aKey, aValue);
    return NS_OK;
  }

  template<class T> nsresult
  GetImpl(const char* aKey, T& aValue)
  {
    mParams.get(aKey, aValue);
    return NS_OK;
  }

  nsresult
  ClearImpl(const char* aKey)
  {
    mParams.remove(aKey);
    return NS_OK;
  }

  // The *Translated() functions allow us to handle special cases;
  // for example, where the thumbnail size setting is exposed as an
  // ICameraControl::Size object, but is handled by the AOSP layer
  // as two separate parameters.
  //
  // Return values:
  //  - NS_OK on success;
  //  - NS_ERROR_INVALID_ARG if 'aValue' contains an invalid value;
  //  - NS_ERROR_NOT_IMPLEMENTED if 'aKey' is invalid;
  //  - NS_ERROR_NOT_AVAILABLE if the getter fails to retrieve a valid value,
  //      or if a setter fails because it requires one or more values that
  //      could not be retrieved;
  //  - NS_ERROR_FAILURE on unexpected internal failures.
  nsresult SetTranslated(uint32_t aKey, const nsAString& aValue);
  nsresult GetTranslated(uint32_t aKey, nsAString& aValue);
  nsresult SetTranslated(uint32_t aKey, const ICameraControl::Size& aSize);
  nsresult GetTranslated(uint32_t aKey, ICameraControl::Size& aSize);
  nsresult GetTranslated(uint32_t aKey, nsTArray<ICameraControl::Size>& aSizes);
  nsresult SetTranslated(uint32_t aKey, const nsTArray<ICameraControl::Region>& aRegions);
  nsresult GetTranslated(uint32_t aKey, nsTArray<ICameraControl::Region>& aRegions);
  nsresult SetTranslated(uint32_t aKey, const ICameraControl::Position& aPosition);
  nsresult SetTranslated(uint32_t aKey, const int64_t& aValue);
  nsresult GetTranslated(uint32_t aKey, int64_t& aValue);
  nsresult SetTranslated(uint32_t aKey, const double& aValue);
  nsresult GetTranslated(uint32_t aKey, double& aValue);
  nsresult SetTranslated(uint32_t aKey, const int& aValue);
  nsresult GetTranslated(uint32_t aKey, int& aValue);
  nsresult SetTranslated(uint32_t aKey, const uint32_t& aValue);
  nsresult GetTranslated(uint32_t aKey, uint32_t& aValue);
  nsresult SetTranslated(uint32_t aKey, const bool& aValue);
  nsresult GetTranslated(uint32_t aKey, bool& aValue);
  nsresult GetTranslated(uint32_t aKey, nsTArray<nsString>& aValues);
  nsresult GetTranslated(uint32_t aKey, nsTArray<double>& aValues);

  // Converts a string of multiple, comma-separated values into an array
  // of the appropriate type.
  //
  // Return values:
  //  - NS_OK on success;
  //  - NS_ERROR_NOT_IMPLEMENTED if 'aKey' is invalid;
  //  - NS_ERROR_NOT_AVAILABLE if a valid value could not be returned.
  template<class T> nsresult GetListAsArray(uint32_t aKey, nsTArray<T>& aArray);

  // Converts ISO values (e.g., "auto", "hjr", "100", "200", etc.) to and from
  // values understood by Gonk (e.g., "auto", "ISO_HJR", "ISO100", "ISO200",
  // respectively).
  //
  // Return values:
  //  - NS_OK on success;
  //  - NS_ERROR_INVALID_ARG if the 'aIso' argument is not a valid form.
  nsresult MapIsoToGonk(const nsAString& aIso, nsACString& aIsoOut);
  nsresult MapIsoFromGonk(const char* aIso, nsAString& aIsoOut);

  // Call once to initialize local cached values used in translating other
  // arguments between Gecko and Gonk. Always returns NS_OK.
  nsresult Initialize();

  // Returns true if we're a memory-constrained platform that requires
  // certain features to be disabled; returns false otherwise.
  static bool IsLowMemoryPlatform();
};

} // namespace mozilla

#endif // DOM_CAMERA_GONKCAMERAPARAMETERS_H
