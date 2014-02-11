/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDOMClassInfoID.h"
#include "DOMCameraControl.h"
#include "DOMCameraCapabilities.h"
#include "CameraCommon.h"

using namespace mozilla;

DOMCI_DATA(CameraCapabilities, nsICameraCapabilities)

NS_INTERFACE_MAP_BEGIN(DOMCameraCapabilities)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsICameraCapabilities)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(CameraCapabilities)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(DOMCameraCapabilities)
NS_IMPL_RELEASE(DOMCameraCapabilities)

/* [implicit_jscontext] readonly attribute jsval previewSizes; */
NS_IMETHODIMP
DOMCameraCapabilities::GetPreviewSizes(JSContext* cx, JS::Value* aPreviewSizes)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* [implicit_jscontext] readonly attribute jsval pictureSizes; */
NS_IMETHODIMP
DOMCameraCapabilities::GetPictureSizes(JSContext* cx, JS::Value* aPictureSizes)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* [implicit_jscontext] readonly attribute jsval fileFormats; */
NS_IMETHODIMP
DOMCameraCapabilities::GetFileFormats(JSContext* cx, JS::Value* aFileFormats)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* [implicit_jscontext] readonly attribute jsval whiteBalanceModes; */
NS_IMETHODIMP
DOMCameraCapabilities::GetWhiteBalanceModes(JSContext* cx, JS::Value* aWhiteBalanceModes)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* [implicit_jscontext] readonly attribute jsval sceneModes; */
NS_IMETHODIMP
DOMCameraCapabilities::GetSceneModes(JSContext* cx, JS::Value* aSceneModes)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* [implicit_jscontext] readonly attribute jsval effects; */
NS_IMETHODIMP
DOMCameraCapabilities::GetEffects(JSContext* cx, JS::Value* aEffects)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* [implicit_jscontext] readonly attribute jsval flashModes; */
NS_IMETHODIMP
DOMCameraCapabilities::GetFlashModes(JSContext* cx, JS::Value* aFlashModes)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* [implicit_jscontext] readonly attribute jsval focusModes; */
NS_IMETHODIMP
DOMCameraCapabilities::GetFocusModes(JSContext* cx, JS::Value* aFocusModes)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* [implicit_jscontext] readonly attribute long maxFocusAreas; */
NS_IMETHODIMP
DOMCameraCapabilities::GetMaxFocusAreas(JSContext* cx, int32_t* aMaxFocusAreas)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* [implicit_jscontext] readonly attribute double minExposureCompensation; */
NS_IMETHODIMP
DOMCameraCapabilities::GetMinExposureCompensation(JSContext* cx, double* aMinExposureCompensation)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* [implicit_jscontext] readonly attribute double maxExposureCompensation; */
NS_IMETHODIMP
DOMCameraCapabilities::GetMaxExposureCompensation(JSContext* cx, double* aMaxExposureCompensation)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* [implicit_jscontext] readonly attribute double stepExposureCompensation; */
NS_IMETHODIMP
DOMCameraCapabilities::GetStepExposureCompensation(JSContext* cx, double* aStepExposureCompensation)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* [implicit_jscontext] readonly attribute long maxMeteringAreas; */
NS_IMETHODIMP
DOMCameraCapabilities::GetMaxMeteringAreas(JSContext* cx, int32_t* aMaxMeteringAreas)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* [implicit_jscontext] readonly attribute jsval zoomRatios; */
NS_IMETHODIMP
DOMCameraCapabilities::GetZoomRatios(JSContext* cx, JS::Value* aZoomRatios)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* [implicit_jscontext] readonly attribute jsval videoSizes; */
NS_IMETHODIMP
DOMCameraCapabilities::GetVideoSizes(JSContext* cx, JS::Value* aVideoSizes)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
