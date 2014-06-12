/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DOMCameraCapabilities.h"
#include "nsPIDOMWindow.h"
#include "nsContentUtils.h"
#include "mozilla/dom/CameraManagerBinding.h"
#include "mozilla/dom/CameraCapabilitiesBinding.h"
#include "Navigator.h"
#include "CameraCommon.h"
#include "ICameraControl.h"
#include "CameraRecorderProfiles.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_CLASS(CameraCapabilities)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(CameraCapabilities)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mWindow)
  tmp->mRecorderProfiles = JS::UndefinedValue();
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(CameraCapabilities)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mWindow)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(CameraCapabilities)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JSVAL_MEMBER_CALLBACK(mRecorderProfiles)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(CameraCapabilities)
NS_IMPL_CYCLE_COLLECTING_RELEASE(CameraCapabilities)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(CameraCapabilities)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

/* static */
bool
CameraCapabilities::HasSupport(JSContext* aCx, JSObject* aGlobal)
{
  return Navigator::HasCameraSupport(aCx, aGlobal);
}

CameraCapabilities::CameraCapabilities(nsPIDOMWindow* aWindow)
  : mRecorderProfiles(JS::UndefinedValue())
  , mWindow(aWindow)
{
  DOM_CAMERA_LOGT("%s:%d : this=%p\n", __func__, __LINE__, this);
  MOZ_COUNT_CTOR(CameraCapabilities);
  mozilla::HoldJSObjects(this);
  SetIsDOMBinding();
}

CameraCapabilities::~CameraCapabilities()
{
  DOM_CAMERA_LOGT("%s:%d : this=%p\n", __func__, __LINE__, this);
  mRecorderProfiles = JS::UndefinedValue();
  mozilla::DropJSObjects(this);
  MOZ_COUNT_DTOR(CameraCapabilities);
}

JSObject*
CameraCapabilities::WrapObject(JSContext* aCx)
{
  return CameraCapabilitiesBinding::Wrap(aCx, this);
}

#define LOG_IF_ERROR(rv, param)                               \
  do {                                                        \
    if (NS_FAILED(rv)) {                                      \
      DOM_CAMERA_LOGW("Error %x trying to get " #param "\n",  \
        (rv));                                                \
    }                                                         \
  } while(0)

nsresult
CameraCapabilities::TranslateToDictionary(ICameraControl* aCameraControl,
                                          uint32_t aKey, nsTArray<CameraSize>& aSizes)
{
  nsresult rv;
  nsTArray<ICameraControl::Size> sizes;

  rv = aCameraControl->Get(aKey, sizes);
  if (NS_FAILED(rv)) {
    return rv;
  }

  aSizes.Clear();
  aSizes.SetCapacity(sizes.Length());
  for (uint32_t i = 0; i < sizes.Length(); ++i) {
    CameraSize* s = aSizes.AppendElement();
    s->mWidth = sizes[i].width;
    s->mHeight = sizes[i].height;
  }

  return NS_OK;
}

nsresult
CameraCapabilities::Populate(ICameraControl* aCameraControl)
{
  NS_ENSURE_TRUE(aCameraControl, NS_ERROR_INVALID_ARG);

  nsresult rv;

  rv = TranslateToDictionary(aCameraControl, CAMERA_PARAM_SUPPORTED_PREVIEWSIZES, mPreviewSizes);
  LOG_IF_ERROR(rv, CAMERA_PARAM_SUPPORTED_PREVIEWSIZES);

  rv = TranslateToDictionary(aCameraControl, CAMERA_PARAM_SUPPORTED_PICTURESIZES, mPictureSizes);
  LOG_IF_ERROR(rv, CAMERA_PARAM_SUPPORTED_PICTURESIZES);

  rv = TranslateToDictionary(aCameraControl, CAMERA_PARAM_SUPPORTED_JPEG_THUMBNAIL_SIZES, mThumbnailSizes);
  LOG_IF_ERROR(rv, CAMERA_PARAM_SUPPORTED_JPEG_THUMBNAIL_SIZES);

  rv = TranslateToDictionary(aCameraControl, CAMERA_PARAM_SUPPORTED_VIDEOSIZES, mVideoSizes);
  LOG_IF_ERROR(rv, CAMERA_PARAM_SUPPORTED_VIDEOSIZES);

  rv = aCameraControl->Get(CAMERA_PARAM_SUPPORTED_PICTUREFORMATS, mFileFormats);
  LOG_IF_ERROR(rv, CAMERA_PARAM_SUPPORTED_PICTUREFORMATS);

  rv = aCameraControl->Get(CAMERA_PARAM_SUPPORTED_WHITEBALANCES, mWhiteBalanceModes);
  LOG_IF_ERROR(rv, CAMERA_PARAM_SUPPORTED_WHITEBALANCES);

  rv = aCameraControl->Get(CAMERA_PARAM_SUPPORTED_SCENEMODES, mSceneModes);
  LOG_IF_ERROR(rv, CAMERA_PARAM_SUPPORTED_SCENEMODES);

  rv = aCameraControl->Get(CAMERA_PARAM_SUPPORTED_EFFECTS, mEffects);
  LOG_IF_ERROR(rv, CAMERA_PARAM_SUPPORTED_EFFECTS);

  rv = aCameraControl->Get(CAMERA_PARAM_SUPPORTED_FLASHMODES, mFlashModes);
  LOG_IF_ERROR(rv, CAMERA_PARAM_SUPPORTED_FLASHMODES);

  rv = aCameraControl->Get(CAMERA_PARAM_SUPPORTED_FOCUSMODES, mFocusModes);
  LOG_IF_ERROR(rv, CAMERA_PARAM_SUPPORTED_FOCUSMODES);

  rv = aCameraControl->Get(CAMERA_PARAM_SUPPORTED_ISOMODES, mIsoModes);
  LOG_IF_ERROR(rv, CAMERA_PARAM_SUPPORTED_ISOMODES);

  rv = aCameraControl->Get(CAMERA_PARAM_SUPPORTED_ZOOMRATIOS, mZoomRatios);
  LOG_IF_ERROR(rv, CAMERA_PARAM_SUPPORTED_ZOOMRATIOS);

  int32_t areas;
  rv = aCameraControl->Get(CAMERA_PARAM_SUPPORTED_MAXFOCUSAREAS, areas);
  LOG_IF_ERROR(rv, CAMERA_PARAM_SUPPORTED_MAXFOCUSAREAS);
  mMaxFocusAreas = areas < 0 ? 0 : areas;

  rv = aCameraControl->Get(CAMERA_PARAM_SUPPORTED_MAXMETERINGAREAS, areas);
  LOG_IF_ERROR(rv, CAMERA_PARAM_SUPPORTED_MAXMETERINGAREAS);
  mMaxMeteringAreas = areas < 0 ? 0 : areas;

  int32_t faces;
  rv = aCameraControl->Get(CAMERA_PARAM_SUPPORTED_MAXDETECTEDFACES, faces);
  LOG_IF_ERROR(rv, CAMERA_PARAM_SUPPORTED_MAXDETECTEDFACES);
  mMaxDetectedFaces = faces < 0 ? 0 : faces;

  rv = aCameraControl->Get(CAMERA_PARAM_SUPPORTED_MINEXPOSURECOMPENSATION, mMinExposureCompensation);
  LOG_IF_ERROR(rv, CAMERA_PARAM_SUPPORTED_MINEXPOSURECOMPENSATION);

  rv = aCameraControl->Get(CAMERA_PARAM_SUPPORTED_MAXEXPOSURECOMPENSATION, mMaxExposureCompensation);
  LOG_IF_ERROR(rv, CAMERA_PARAM_SUPPORTED_MAXEXPOSURECOMPENSATION);

  rv = aCameraControl->Get(CAMERA_PARAM_SUPPORTED_EXPOSURECOMPENSATIONSTEP, mExposureCompensationStep);
  LOG_IF_ERROR(rv, CAMERA_PARAM_SUPPORTED_EXPOSURECOMPENSATIONSTEP);

  mRecorderProfileManager = aCameraControl->GetRecorderProfileManager();
  if (!mRecorderProfileManager) {
    DOM_CAMERA_LOGW("Unable to get recorder profile manager\n");
  } else {
    AutoJSContext js;

    JS::Rooted<JSObject*> o(js);
    nsresult rv = mRecorderProfileManager->GetJsObject(js, o.address());
    if (NS_FAILED(rv)) {
      DOM_CAMERA_LOGE("Failed to JS-objectify profile manager (0x%x)\n", rv);
    } else {
      mRecorderProfiles = JS::ObjectValue(*o);
    }
  }

  // For now, always return success, since the presence or absence of capabilities
  // indicates whether or not they are supported.
  return NS_OK;
}

void
CameraCapabilities::GetPreviewSizes(nsTArray<dom::CameraSize>& retval) const
{
  retval = mPreviewSizes;
}

void
CameraCapabilities::GetPictureSizes(nsTArray<dom::CameraSize>& retval) const
{
  retval = mPictureSizes;
}

void
CameraCapabilities::GetThumbnailSizes(nsTArray<dom::CameraSize>& retval) const
{
  retval = mThumbnailSizes;
}

void
CameraCapabilities::GetVideoSizes(nsTArray<dom::CameraSize>& retval) const
{
  retval = mVideoSizes;
}

void
CameraCapabilities::GetFileFormats(nsTArray<nsString>& retval) const
{
  retval = mFileFormats;
}

void
CameraCapabilities::GetWhiteBalanceModes(nsTArray<nsString>& retval) const
{
  retval = mWhiteBalanceModes;
}

void
CameraCapabilities::GetSceneModes(nsTArray<nsString>& retval) const
{
  retval = mSceneModes;
}

void
CameraCapabilities::GetEffects(nsTArray<nsString>& retval) const
{
  retval = mEffects;
}

void
CameraCapabilities::GetFlashModes(nsTArray<nsString>& retval) const
{
  retval = mFlashModes;
}

void
CameraCapabilities::GetFocusModes(nsTArray<nsString>& retval) const
{
  retval = mFocusModes;
}

void
CameraCapabilities::GetZoomRatios(nsTArray<double>& retval) const
{
  retval = mZoomRatios;
}

uint32_t
CameraCapabilities::MaxFocusAreas() const
{
  return mMaxFocusAreas;
}

uint32_t
CameraCapabilities::MaxMeteringAreas() const
{
  return mMaxMeteringAreas;
}

uint32_t
CameraCapabilities::MaxDetectedFaces() const
{
  return mMaxDetectedFaces;
}

double
CameraCapabilities::MinExposureCompensation() const
{
  return mMinExposureCompensation;
}

double
CameraCapabilities::MaxExposureCompensation() const
{
  return mMaxExposureCompensation;
}

double
CameraCapabilities::ExposureCompensationStep() const
{
  return mExposureCompensationStep;
}

void
CameraCapabilities::GetRecorderProfiles(JSContext* aCx,
                                        JS::MutableHandle<JS::Value> aRetval) const
{
  JS::ExposeValueToActiveJS(mRecorderProfiles);
  aRetval.set(mRecorderProfiles);
}

void
CameraCapabilities::GetIsoModes(nsTArray<nsString>& retval) const
{
  retval = mIsoModes;
}

} // namespace dom
} // namespace mozilla
