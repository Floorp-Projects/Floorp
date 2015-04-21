/*
 * Copyright (C) 2013-2015 Mozilla Foundation
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
#include "nsTArray.h"
#include "nsString.h"
#include "mozilla/Mutex.h"
#include "nsClassHashtable.h"
#include "ICameraControl.h"

#ifdef MOZ_WIDGET_GONK
#include <camera/CameraParameters.h>
#else
#include "FallbackCameraPlatform.h"
#endif

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
    MutexAutoLock lock(mLock);
    nsresult rv = SetTranslated(aKey, aValue);
    mDirty = mDirty || NS_SUCCEEDED(rv);
    return rv;
  }

  template<class T> nsresult
  Get(uint32_t aKey, T& aValue)
  {
    MutexAutoLock lock(mLock);
    return GetTranslated(aKey, aValue);
  }

  bool
  TestAndClearDirtyFlag()
  {
    bool dirty;

    MutexAutoLock lock(mLock);
    dirty = mDirty;
    mDirty = false;
    return dirty;
  }

  android::String8 Flatten() const;
  nsresult Unflatten(const android::String8& aFlatParameters);

protected:
  mutable Mutex mLock;
  bool mDirty;
  bool mInitialized;

  // Required internal properties
  double mExposureCompensationStep;
  int32_t mExposureCompensationMinIndex;
  int32_t mExposureCompensationMaxIndex;
  const char* mVendorSpecificKeyIsoMode;
  const char* mVendorSpecificKeySupportedIsoModes;
  nsTArray<int> mZoomRatios;
  nsTArray<nsString> mIsoModes;
  nsTArray<nsString> mSceneModes;
  nsTArray<nsString> mMeteringModes;
  nsClassHashtable<nsStringHashKey, nsCString> mIsoModeMap;
  nsClassHashtable<nsCStringHashKey, nsCString> mParams;

  static PLDHashOperator EnumerateFlatten(const nsACString& aKey, nsCString* aValue, void* aUserArg);

  nsresult SetImpl(const char* aKey, const char* aValue)
  {
    if (!aValue || strchr(aValue, ';') || strchr(aValue, '=')) {
      return NS_ERROR_ILLEGAL_VALUE;
    }
    nsDependentCString key(aKey);
    mParams.Put(key, new nsCString(aValue));
    return NS_OK;
  }

  nsresult SetImpl(const char* aKey, int aValue)
  {
    nsDependentCString key(aKey);
    nsCString* value = new nsCString();
    value->AppendInt(aValue);
    mParams.Put(key, value);
    return NS_OK;
  }

  nsresult SetImpl(const char* aKey, double aValue)
  {
    nsDependentCString key(aKey);
    nsCString* value = new nsCString();
    value->AppendFloat(aValue);
    mParams.Put(key, value);
    return NS_OK;
  }

  nsresult SetImpl(const char* aKey, float aValue)
  {
    nsDependentCString key(aKey);
    nsCString* value = new nsCString();
    value->AppendFloat(aValue);
    mParams.Put(key, value);
    return NS_OK;
  }

  nsresult SetImpl(const char* aKey, bool aValue)
  {
    nsDependentCString key(aKey);
    mParams.Put(key, new nsCString(aValue ? "true" : "false"));
    return NS_OK;
  }

  nsresult GetImpl(const char* aKey, const char*& aRet)
  {
    nsDependentCString key(aKey);
    nsCString* value;
    if (!mParams.Get(key, &value)) {
      aRet = nullptr;
      return NS_ERROR_FAILURE;
    }
    aRet = value->Data();
    return NS_OK;
  }

  nsresult GetImpl(const char* aKey, float& aRet)
  {
    nsDependentCString key(aKey);
    nsCString* value;
    nsresult rv = NS_ERROR_FAILURE;
    if (mParams.Get(key, &value)) {
      aRet = value->ToFloat(&rv);
    } else {
      aRet = 0.0;
    }
    return rv;
  }

  nsresult GetImpl(const char* aKey, double& aRet)
  {
    nsDependentCString key(aKey);
    nsCString* value;
    nsresult rv = NS_ERROR_FAILURE;
    if (mParams.Get(key, &value)) {
      aRet = value->ToFloat(&rv);
    } else {
      aRet = 0.0;
    }
    return rv;
  }

  nsresult GetImpl(const char* aKey, int& aRet)
  {
    nsDependentCString key(aKey);
    nsCString* value;
    nsresult rv = NS_ERROR_FAILURE;
    if (mParams.Get(key, &value)) {
      aRet = value->ToInteger(&rv);
    } else {
      aRet = 0.0;
    }
    return rv;
  }

  nsresult GetImpl(const char* aKey, bool& aRet)
  {
    nsDependentCString key(aKey);
    nsCString* value;
    if (!mParams.Get(key, &value)) {
      aRet = false;
      return NS_ERROR_FAILURE;
    }
    aRet = value->EqualsLiteral("true");
    return NS_OK;
  }

  const char* GetTextKey(uint32_t aKey);
  const char* FindVendorSpecificKey(const char* aPotentialKeys[], size_t aPotentialKeyCount);

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
    const char* key = GetTextKey(aKey);
    NS_ENSURE_TRUE(key, NS_ERROR_NOT_IMPLEMENTED);
    return SetImpl(key, aValue);
  }

  template<typename T> nsresult
  GetImpl(uint32_t aKey, T& aValue)
  {
    const char* key = GetTextKey(aKey);
    NS_ENSURE_TRUE(key, NS_ERROR_NOT_IMPLEMENTED);
    return GetImpl(key, aValue);
  }

  nsresult
  ClearImpl(const char* aKey)
  {
    nsDependentCString key(aKey);
    mParams.Remove(key);
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
