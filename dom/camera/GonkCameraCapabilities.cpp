/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <string.h>
#include <stdlib.h>
#include "nsDOMClassInfo.h"
#include "jsapi.h"
#include "camera/CameraParameters.h"
#include "CameraControl.h"
#include "CameraCapabilities.h"

#define DOM_CAMERA_LOG_LEVEL  3
#include "CameraCommon.h"

using namespace android;
using namespace mozilla;

DOMCI_DATA(CameraCapabilities, nsICameraCapabilities)

NS_INTERFACE_MAP_BEGIN(nsCameraCapabilities)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsICameraCapabilities)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(CameraCapabilities)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(nsCameraCapabilities)
NS_IMPL_RELEASE(nsCameraCapabilities)


nsCameraCapabilities::nsCameraCapabilities(nsCameraControl* aCamera)
  : mCamera(aCamera)
{
  // member initializers and constructor code
  DOM_CAMERA_LOGI("%s:%d : this=%p\n", __func__, __LINE__, this);
}

nsCameraCapabilities::~nsCameraCapabilities()
{
  // destructor code
  DOM_CAMERA_LOGI("%s:%d : this=%p, mCamera=%p\n", __func__, __LINE__, this, mCamera.get());
}

static nsresult
ParseZoomRatioItemAndAdd(JSContext* aCx, JSObject* aArray, PRUint32 aIndex, const char* aStart, char** aEnd)
{
  if (!*aEnd) {
    // make 'aEnd' follow the same semantics as strchr().
    aEnd = nullptr;
  }

  double d = strtod(aStart, aEnd);
  d /= 100;

  jsval v = JS_NumberValue(d);

  if (!JS_SetElement(aCx, aArray, aIndex, &v)) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

static nsresult
ParseStringItemAndAdd(JSContext* aCx, JSObject* aArray, PRUint32 aIndex, const char* aStart, char** aEnd)
{
  JSString* s;

  if (*aEnd) {
    s = JS_NewStringCopyN(aCx, aStart, *aEnd - aStart);
  } else {
    s = JS_NewStringCopyZ(aCx, aStart);
  }
  if (!s) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  jsval v = STRING_TO_JSVAL(s);
  if (!JS_SetElement(aCx, aArray, aIndex, &v)) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

static nsresult
ParseDimensionItemAndAdd(JSContext* aCx, JSObject* aArray, PRUint32 aIndex, const char* aStart, char** aEnd)
{
  char* x;

  if (!*aEnd) {
    // make 'aEnd' follow the same semantics as strchr().
    aEnd = nullptr;
  }

  jsval w = INT_TO_JSVAL(strtol(aStart, &x, 10));
  jsval h = INT_TO_JSVAL(strtol(x + 1, aEnd, 10));

  JSObject* o = JS_NewObject(aCx, nullptr, nullptr, nullptr);
  if (!o) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  if (!JS_SetProperty(aCx, o, "width", &w)) {
    return NS_ERROR_FAILURE;
  }
  if (!JS_SetProperty(aCx, o, "height", &h)) {
    return NS_ERROR_FAILURE;
  }

  jsval v = OBJECT_TO_JSVAL(o);
  if (!JS_SetElement(aCx, aArray, aIndex, &v)) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult
nsCameraCapabilities::ParameterListToNewArray(JSContext* aCx, JSObject** aArray, PRUint32 aKey, ParseItemAndAddFunc aParseItemAndAdd)
{
  NS_ENSURE_TRUE(mCamera, NS_ERROR_NOT_AVAILABLE);

  const char* value = mCamera->GetParameterConstChar(aKey);
  if (!value) {
    // in case we get nonsense data back
    *aArray = nullptr;
    return NS_OK;
  }

  *aArray = JS_NewArrayObject(aCx, 0, nullptr);
  if (!*aArray) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  const char* p = value;
  PRUint32 index = 0;
  nsresult rv;
  char* q;

  while (p) {
    q = strchr(p, ',');
    if (q != p) { // skip consecutive delimiters, just in case
      rv = aParseItemAndAdd(aCx, *aArray, index, p, &q);
      NS_ENSURE_SUCCESS(rv, rv);
      ++index;
    }
    p = q;
    if (p) {
      ++p;
    }
  }

  return JS_FreezeObject(aCx, *aArray) ? NS_OK : NS_ERROR_FAILURE;
}

nsresult
nsCameraCapabilities::StringListToNewObject(JSContext* aCx, JS::Value* aArray, PRUint32 aKey)
{
  JSObject* array;

  nsresult rv = ParameterListToNewArray(aCx, &array, aKey, ParseStringItemAndAdd);
  NS_ENSURE_SUCCESS(rv, rv);

  *aArray = OBJECT_TO_JSVAL(array);
  return NS_OK;
}

nsresult
nsCameraCapabilities::DimensionListToNewObject(JSContext* aCx, JS::Value* aArray, PRUint32 aKey)
{
  JSObject* array;
  nsresult rv;

  rv = ParameterListToNewArray(aCx, &array, aKey, ParseDimensionItemAndAdd);
  NS_ENSURE_SUCCESS(rv, rv);

  *aArray = OBJECT_TO_JSVAL(array);
  return NS_OK;
}

/* readonly attribute jsval previewSizes; */
NS_IMETHODIMP
nsCameraCapabilities::GetPreviewSizes(JSContext* cx, JS::Value* aPreviewSizes)
{
  return DimensionListToNewObject(cx, aPreviewSizes, nsCameraControl::CAMERA_PARAM_SUPPORTED_PREVIEWSIZES);
}

/* readonly attribute jsval pictureSizes; */
NS_IMETHODIMP
nsCameraCapabilities::GetPictureSizes(JSContext* cx, JS::Value* aPictureSizes)
{
  return DimensionListToNewObject(cx, aPictureSizes, nsCameraControl::CAMERA_PARAM_SUPPORTED_PICTURESIZES);
}

/* readonly attribute jsval fileFormats; */
NS_IMETHODIMP
nsCameraCapabilities::GetFileFormats(JSContext* cx, JS::Value* aFileFormats)
{
  return StringListToNewObject(cx, aFileFormats, nsCameraControl::CAMERA_PARAM_SUPPORTED_PICTUREFORMATS);
}

/* readonly attribute jsval whiteBalanceModes; */
NS_IMETHODIMP
nsCameraCapabilities::GetWhiteBalanceModes(JSContext* cx, JS::Value* aWhiteBalanceModes)
{
  return StringListToNewObject(cx, aWhiteBalanceModes, nsCameraControl::CAMERA_PARAM_SUPPORTED_WHITEBALANCES);
}

/* readonly attribute jsval sceneModes; */
NS_IMETHODIMP
nsCameraCapabilities::GetSceneModes(JSContext* cx, JS::Value* aSceneModes)
{
  return StringListToNewObject(cx, aSceneModes, nsCameraControl::CAMERA_PARAM_SUPPORTED_SCENEMODES);
}

/* readonly attribute jsval effects; */
NS_IMETHODIMP
nsCameraCapabilities::GetEffects(JSContext* cx, JS::Value* aEffects)
{
  return StringListToNewObject(cx, aEffects, nsCameraControl::CAMERA_PARAM_SUPPORTED_EFFECTS);
}

/* readonly attribute jsval flashModes; */
NS_IMETHODIMP
nsCameraCapabilities::GetFlashModes(JSContext* cx, JS::Value* aFlashModes)
{
  return StringListToNewObject(cx, aFlashModes, nsCameraControl::CAMERA_PARAM_SUPPORTED_FLASHMODES);
}

/* readonly attribute jsval focusModes; */
NS_IMETHODIMP
nsCameraCapabilities::GetFocusModes(JSContext* cx, JS::Value* aFocusModes)
{
  return StringListToNewObject(cx, aFocusModes, nsCameraControl::CAMERA_PARAM_SUPPORTED_FOCUSMODES);
}

/* readonly attribute long maxFocusAreas; */
NS_IMETHODIMP
nsCameraCapabilities::GetMaxFocusAreas(JSContext* cx, PRInt32* aMaxFocusAreas)
{
  NS_ENSURE_TRUE(mCamera, NS_ERROR_NOT_AVAILABLE);

  const char* value = mCamera->GetParameterConstChar(nsCameraControl::CAMERA_PARAM_SUPPORTED_MAXFOCUSAREAS);
  if (!value) {
    // in case we get nonsense data back
    *aMaxFocusAreas = 0;
    return NS_OK;
  }

  *aMaxFocusAreas = atoi(value);
  return NS_OK;
}

/* readonly attribute double minExposureCompensation; */
NS_IMETHODIMP
nsCameraCapabilities::GetMinExposureCompensation(JSContext* cx, double* aMinExposureCompensation)
{
  NS_ENSURE_TRUE(mCamera, NS_ERROR_NOT_AVAILABLE);

  const char* value = mCamera->GetParameterConstChar(nsCameraControl::CAMERA_PARAM_SUPPORTED_MINEXPOSURECOMPENSATION);
  if (!value) {
    // in case we get nonsense data back
    *aMinExposureCompensation = 0;
    return NS_OK;
  }

  *aMinExposureCompensation = atof(value);
  return NS_OK;
}

/* readonly attribute double maxExposureCompensation; */
NS_IMETHODIMP
nsCameraCapabilities::GetMaxExposureCompensation(JSContext* cx, double* aMaxExposureCompensation)
{
  NS_ENSURE_TRUE(mCamera, NS_ERROR_NOT_AVAILABLE);

  const char* value = mCamera->GetParameterConstChar(nsCameraControl::CAMERA_PARAM_SUPPORTED_MAXEXPOSURECOMPENSATION);
  if (!value) {
    // in case we get nonsense data back
    *aMaxExposureCompensation = 0;
    return NS_OK;
  }

  *aMaxExposureCompensation = atof(value);
  return NS_OK;
}

/* readonly attribute double stepExposureCompensation; */
NS_IMETHODIMP
nsCameraCapabilities::GetStepExposureCompensation(JSContext* cx, double* aStepExposureCompensation)
{
  NS_ENSURE_TRUE(mCamera, NS_ERROR_NOT_AVAILABLE);

  const char* value = mCamera->GetParameterConstChar(nsCameraControl::CAMERA_PARAM_SUPPORTED_EXPOSURECOMPENSATIONSTEP);
  if (!value) {
    // in case we get nonsense data back
    *aStepExposureCompensation = 0;
    return NS_OK;
  }

  *aStepExposureCompensation = atof(value);
  return NS_OK;
}

/* readonly attribute long maxMeteringAreas; */
NS_IMETHODIMP
nsCameraCapabilities::GetMaxMeteringAreas(JSContext* cx, PRInt32* aMaxMeteringAreas)
{
  NS_ENSURE_TRUE(mCamera, NS_ERROR_NOT_AVAILABLE);

  const char* value = mCamera->GetParameterConstChar(nsCameraControl::CAMERA_PARAM_SUPPORTED_MAXMETERINGAREAS);
  if (!value) {
    // in case we get nonsense data back
    *aMaxMeteringAreas = 0;
    return NS_OK;
  }

  *aMaxMeteringAreas = atoi(value);
  return NS_OK;
}

/* readonly attribute jsval zoomRatios; */
NS_IMETHODIMP
nsCameraCapabilities::GetZoomRatios(JSContext* cx, JS::Value* aZoomRatios)
{
  NS_ENSURE_TRUE(mCamera, NS_ERROR_NOT_AVAILABLE);

  const char* value = mCamera->GetParameterConstChar(nsCameraControl::CAMERA_PARAM_SUPPORTED_ZOOM);
  if (!value || strcmp(value, CameraParameters::TRUE) != 0) {
    // if zoom is not supported, return a null object
    *aZoomRatios = JSVAL_NULL;
    return NS_OK;
  }

  JSObject* array;

  nsresult rv = ParameterListToNewArray(cx, &array, nsCameraControl::CAMERA_PARAM_SUPPORTED_ZOOMRATIOS, ParseZoomRatioItemAndAdd);
  NS_ENSURE_SUCCESS(rv, rv);

  *aZoomRatios = OBJECT_TO_JSVAL(array);
  return NS_OK;
}

/* readonly attribute jsval videoSizes; */
NS_IMETHODIMP
nsCameraCapabilities::GetVideoSizes(JSContext* cx, JS::Value* aVideoSizes)
{
  return DimensionListToNewObject(cx, aVideoSizes, nsCameraControl::CAMERA_PARAM_SUPPORTED_VIDEOSIZES);
}
