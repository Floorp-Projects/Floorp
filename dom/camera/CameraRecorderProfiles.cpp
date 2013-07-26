/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsapi.h"
#include "CameraRecorderProfiles.h"
#include "CameraCommon.h"

using namespace mozilla;

/**
 * Video profile implementation.
 */
RecorderVideoProfile::RecorderVideoProfile(uint32_t aCameraId, uint32_t aQualityIndex)
  : mCameraId(aCameraId)
  , mQualityIndex(aQualityIndex)
{
  DOM_CAMERA_LOGT("%s:%d : this=%p\n", __func__, __LINE__, this);
}

RecorderVideoProfile::~RecorderVideoProfile()
{
  DOM_CAMERA_LOGT("%s:%d : this=%p\n", __func__, __LINE__, this);
}

nsresult
RecorderVideoProfile::GetJsObject(JSContext* aCx, JSObject** aObject)
{
  NS_ENSURE_TRUE(aObject, NS_ERROR_INVALID_ARG);

  JS::Rooted<JSObject*> o(aCx, JS_NewObject(aCx, nullptr, nullptr, nullptr));
  NS_ENSURE_TRUE(o, NS_ERROR_OUT_OF_MEMORY);

  const char* codec = GetCodecName();
  NS_ENSURE_TRUE(codec, NS_ERROR_FAILURE);

  JS::Rooted<JSString*> s(aCx, JS_NewStringCopyZ(aCx, codec));
  JS::Rooted<JS::Value> v(aCx, STRING_TO_JSVAL(s));
  if (!JS_SetProperty(aCx, o, "codec", v)) {
    return NS_ERROR_FAILURE;
  }

  if (mBitrate != -1) {
    v = INT_TO_JSVAL(mBitrate);
    if (!JS_SetProperty(aCx, o, "bitrate", v)) {
      return NS_ERROR_FAILURE;
    }
  }
  if (mFramerate != -1) {
    v = INT_TO_JSVAL(mFramerate);
    if (!JS_SetProperty(aCx, o, "framerate", v)) {
      return NS_ERROR_FAILURE;
    }
  }
  if (mWidth != -1) {
    v = INT_TO_JSVAL(mWidth);
    if (!JS_SetProperty(aCx, o, "width", v)) {
      return NS_ERROR_FAILURE;
    }
  }
  if (mHeight != -1) {
    v = INT_TO_JSVAL(mHeight);
    if (!JS_SetProperty(aCx, o, "height", v)) {
      return NS_ERROR_FAILURE;
    }
  }

  *aObject = o;
  return NS_OK;
}

/**
 * Audio profile implementation.
 */
RecorderAudioProfile::RecorderAudioProfile(uint32_t aCameraId, uint32_t aQualityIndex)
  : mCameraId(aCameraId)
  , mQualityIndex(aQualityIndex)
{
  DOM_CAMERA_LOGT("%s:%d : this=%p\n", __func__, __LINE__, this);
}

RecorderAudioProfile::~RecorderAudioProfile()
{
  DOM_CAMERA_LOGT("%s:%d : this=%p\n", __func__, __LINE__, this);
}

nsresult
RecorderAudioProfile::GetJsObject(JSContext* aCx, JSObject** aObject)
{
  NS_ENSURE_TRUE(aObject, NS_ERROR_INVALID_ARG);

  JS::Rooted<JSObject*> o(aCx, JS_NewObject(aCx, nullptr, nullptr, nullptr));
  NS_ENSURE_TRUE(o, NS_ERROR_OUT_OF_MEMORY);

  const char* codec = GetCodecName();
  NS_ENSURE_TRUE(codec, NS_ERROR_FAILURE);

  JS::Rooted<JSString*> s(aCx, JS_NewStringCopyZ(aCx, codec));
  JS::Rooted<JS::Value> v(aCx, STRING_TO_JSVAL(s));
  if (!JS_SetProperty(aCx, o, "codec", v)) {
    return NS_ERROR_FAILURE;
  }

  if (mBitrate != -1) {
    v = INT_TO_JSVAL(mBitrate);
    if (!JS_SetProperty(aCx, o, "bitrate", v)) {
      return NS_ERROR_FAILURE;
    }
  }
  if (mSamplerate != -1) {
    v = INT_TO_JSVAL(mSamplerate);
    if (!JS_SetProperty(aCx, o, "samplerate", v)) {
      return NS_ERROR_FAILURE;
    }
  }
  if (mChannels != -1) {
    v = INT_TO_JSVAL(mChannels);
    if (!JS_SetProperty(aCx, o, "channels", v)) {
      return NS_ERROR_FAILURE;
    }
  }

  *aObject = o;
  return NS_OK;
}

/**
 * Recorder Profile
 */
RecorderProfile::RecorderProfile(uint32_t aCameraId, uint32_t aQualityIndex)
  : mCameraId(aCameraId)
  , mQualityIndex(aQualityIndex)
  , mName(nullptr)
{
  DOM_CAMERA_LOGT("%s:%d : this=%p\n", __func__, __LINE__, this);
}

RecorderProfile::~RecorderProfile()
{
  DOM_CAMERA_LOGT("%s:%d : this=%p\n", __func__, __LINE__, this);
}

/**
 * Recorder profile manager implementation.
 */
RecorderProfileManager::RecorderProfileManager(uint32_t aCameraId)
  : mCameraId(aCameraId)
{
  DOM_CAMERA_LOGT("%s:%d : this=%p\n", __func__, __LINE__, this);
}

RecorderProfileManager::~RecorderProfileManager()
{
  DOM_CAMERA_LOGT("%s:%d : this=%p\n", __func__, __LINE__, this);
}

nsresult
RecorderProfileManager::GetJsObject(JSContext* aCx, JSObject** aObject) const
{
  NS_ENSURE_TRUE(aObject, NS_ERROR_INVALID_ARG);

  JS::Rooted<JSObject*> o(aCx, JS_NewObject(aCx, nullptr, nullptr, nullptr));
  if (!o) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  for (uint32_t q = 0; q < GetMaxQualityIndex(); ++q) {
    if (!IsSupported(q)) {
      continue;
    }

    nsRefPtr<RecorderProfile> profile = Get(q);
    if (!profile) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    const char* profileName = profile->GetName();
    if (!profileName) {
      // don't allow anonymous recorder profiles
      continue;
    }

    JS::Rooted<JSObject*> p(aCx);
    nsresult rv = profile->GetJsObject(aCx, p.address());
    NS_ENSURE_SUCCESS(rv, rv);
    JS::Rooted<JS::Value> v(aCx, OBJECT_TO_JSVAL(p));

    if (!JS_SetProperty(aCx, o, profileName, v)) {
      return NS_ERROR_FAILURE;
    }
  }

  *aObject = o;
  return NS_OK;
}
