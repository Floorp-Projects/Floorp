/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <cstring>
#include <cstdlib>
#include "base/basictypes.h"
#include "nsDOMClassInfo.h"
#include "jsapi.h"
#include "DOMCameraControl.h"
#include "DOMCameraCapabilities.h"
#include "CameraCommon.h"

using namespace mozilla;

DOMCI_DATA(CameraCapabilities, nsICameraCapabilities)

NS_IMPL_CYCLE_COLLECTION_0(DOMCameraCapabilities)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DOMCameraCapabilities)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsICameraCapabilities)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(CameraCapabilities)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(DOMCameraCapabilities)
NS_IMPL_CYCLE_COLLECTING_RELEASE(DOMCameraCapabilities)

static nsresult
ParseZoomRatioItemAndAdd(JSContext* aCx, JSObject* aArray, uint32_t aIndex, const char* aStart, char** aEnd)
{
  if (!*aEnd) {
    // make 'aEnd' follow the same semantics as strchr().
    aEnd = nullptr;
  }

  /**
   * The by-100 divisor is Gonk-specific.  For now, assume other platforms
   * return actual fractoinal multipliers.
   */
  double d = strtod(aStart, aEnd);
#if MOZ_WIDGET_GONK
  d /= 100;
#endif

  jsval v = JS_NumberValue(d);

  if (!JS_SetElement(aCx, aArray, aIndex, &v)) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

static nsresult
ParseStringItemAndAdd(JSContext* aCx, JSObject* aArray, uint32_t aIndex, const char* aStart, char** aEnd)
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
ParseDimensionItemAndAdd(JSContext* aCx, JSObject* aArray, uint32_t aIndex, const char* aStart, char** aEnd)
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
DOMCameraCapabilities::ParameterListToNewArray(JSContext* aCx, JSObject** aArray, uint32_t aKey, ParseItemAndAddFunc aParseItemAndAdd)
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
  uint32_t index = 0;
  nsresult rv;
  char* q;

  while (p) {
    /**
     * In C's string.h, strchr() is declared as returning 'char*'; in C++'s
     * cstring, it is declared as returning 'const char*', _except_ in MSVC,
     * where the C version is declared to return const like the C++ version.
     *
     * Unfortunately, for both cases, strtod() and strtol() take a 'char**' as
     * the end-of-conversion pointer, so we need to cast away strchr()'s
     * const-ness here to make the MSVC build everything happy.
     */
    q = const_cast<char*>(strchr(p, ','));
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
DOMCameraCapabilities::StringListToNewObject(JSContext* aCx, JS::Value* aArray, uint32_t aKey)
{
  JSObject* array;

  nsresult rv = ParameterListToNewArray(aCx, &array, aKey, ParseStringItemAndAdd);
  NS_ENSURE_SUCCESS(rv, rv);

  *aArray = OBJECT_TO_JSVAL(array);
  return NS_OK;
}

nsresult
DOMCameraCapabilities::DimensionListToNewObject(JSContext* aCx, JS::Value* aArray, uint32_t aKey)
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
DOMCameraCapabilities::GetPreviewSizes(JSContext* cx, JS::Value* aPreviewSizes)
{
  return DimensionListToNewObject(cx, aPreviewSizes, CAMERA_PARAM_SUPPORTED_PREVIEWSIZES);
}

/* readonly attribute jsval pictureSizes; */
NS_IMETHODIMP
DOMCameraCapabilities::GetPictureSizes(JSContext* cx, JS::Value* aPictureSizes)
{
  return DimensionListToNewObject(cx, aPictureSizes, CAMERA_PARAM_SUPPORTED_PICTURESIZES);
}

/* readonly attribute jsval fileFormats; */
NS_IMETHODIMP
DOMCameraCapabilities::GetFileFormats(JSContext* cx, JS::Value* aFileFormats)
{
  return StringListToNewObject(cx, aFileFormats, CAMERA_PARAM_SUPPORTED_PICTUREFORMATS);
}

/* readonly attribute jsval whiteBalanceModes; */
NS_IMETHODIMP
DOMCameraCapabilities::GetWhiteBalanceModes(JSContext* cx, JS::Value* aWhiteBalanceModes)
{
  return StringListToNewObject(cx, aWhiteBalanceModes, CAMERA_PARAM_SUPPORTED_WHITEBALANCES);
}

/* readonly attribute jsval sceneModes; */
NS_IMETHODIMP
DOMCameraCapabilities::GetSceneModes(JSContext* cx, JS::Value* aSceneModes)
{
  return StringListToNewObject(cx, aSceneModes, CAMERA_PARAM_SUPPORTED_SCENEMODES);
}

/* readonly attribute jsval effects; */
NS_IMETHODIMP
DOMCameraCapabilities::GetEffects(JSContext* cx, JS::Value* aEffects)
{
  return StringListToNewObject(cx, aEffects, CAMERA_PARAM_SUPPORTED_EFFECTS);
}

/* readonly attribute jsval flashModes; */
NS_IMETHODIMP
DOMCameraCapabilities::GetFlashModes(JSContext* cx, JS::Value* aFlashModes)
{
  return StringListToNewObject(cx, aFlashModes, CAMERA_PARAM_SUPPORTED_FLASHMODES);
}

/* readonly attribute jsval focusModes; */
NS_IMETHODIMP
DOMCameraCapabilities::GetFocusModes(JSContext* cx, JS::Value* aFocusModes)
{
  return StringListToNewObject(cx, aFocusModes, CAMERA_PARAM_SUPPORTED_FOCUSMODES);
}

/* readonly attribute long maxFocusAreas; */
NS_IMETHODIMP
DOMCameraCapabilities::GetMaxFocusAreas(JSContext* cx, int32_t* aMaxFocusAreas)
{
  NS_ENSURE_TRUE(mCamera, NS_ERROR_NOT_AVAILABLE);

  const char* value = mCamera->GetParameterConstChar(CAMERA_PARAM_SUPPORTED_MAXFOCUSAREAS);
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
DOMCameraCapabilities::GetMinExposureCompensation(JSContext* cx, double* aMinExposureCompensation)
{
  NS_ENSURE_TRUE(mCamera, NS_ERROR_NOT_AVAILABLE);

  const char* value = mCamera->GetParameterConstChar(CAMERA_PARAM_SUPPORTED_MINEXPOSURECOMPENSATION);
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
DOMCameraCapabilities::GetMaxExposureCompensation(JSContext* cx, double* aMaxExposureCompensation)
{
  NS_ENSURE_TRUE(mCamera, NS_ERROR_NOT_AVAILABLE);

  const char* value = mCamera->GetParameterConstChar(CAMERA_PARAM_SUPPORTED_MAXEXPOSURECOMPENSATION);
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
DOMCameraCapabilities::GetStepExposureCompensation(JSContext* cx, double* aStepExposureCompensation)
{
  NS_ENSURE_TRUE(mCamera, NS_ERROR_NOT_AVAILABLE);

  const char* value = mCamera->GetParameterConstChar(CAMERA_PARAM_SUPPORTED_EXPOSURECOMPENSATIONSTEP);
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
DOMCameraCapabilities::GetMaxMeteringAreas(JSContext* cx, int32_t* aMaxMeteringAreas)
{
  NS_ENSURE_TRUE(mCamera, NS_ERROR_NOT_AVAILABLE);

  const char* value = mCamera->GetParameterConstChar(CAMERA_PARAM_SUPPORTED_MAXMETERINGAREAS);
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
DOMCameraCapabilities::GetZoomRatios(JSContext* cx, JS::Value* aZoomRatios)
{
  NS_ENSURE_TRUE(mCamera, NS_ERROR_NOT_AVAILABLE);

  const char* value = mCamera->GetParameterConstChar(CAMERA_PARAM_SUPPORTED_ZOOM);
  if (!value || strcmp(value, "true") != 0) {
    // if zoom is not supported, return a null object
    *aZoomRatios = JSVAL_NULL;
    return NS_OK;
  }

  JSObject* array;

  nsresult rv = ParameterListToNewArray(cx, &array, CAMERA_PARAM_SUPPORTED_ZOOMRATIOS, ParseZoomRatioItemAndAdd);
  NS_ENSURE_SUCCESS(rv, rv);

  *aZoomRatios = OBJECT_TO_JSVAL(array);
  return NS_OK;
}

/* readonly attribute jsval videoSizes; */
NS_IMETHODIMP
DOMCameraCapabilities::GetVideoSizes(JSContext* cx, JS::Value* aVideoSizes)
{
  return DimensionListToNewObject(cx, aVideoSizes, CAMERA_PARAM_SUPPORTED_VIDEOSIZES);
}
