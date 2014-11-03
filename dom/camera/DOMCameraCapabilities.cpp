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

namespace mozilla {
namespace dom {

/**
 * CameraRecorderVideoProfile
 */
NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(CameraRecorderVideoProfile, mParent)

NS_IMPL_CYCLE_COLLECTING_ADDREF(CameraRecorderVideoProfile)
NS_IMPL_CYCLE_COLLECTING_RELEASE(CameraRecorderVideoProfile)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(CameraRecorderVideoProfile)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

JSObject*
CameraRecorderVideoProfile::WrapObject(JSContext* aCx)
{
  return CameraRecorderVideoProfileBinding::Wrap(aCx, this);
}

CameraRecorderVideoProfile::CameraRecorderVideoProfile(nsISupports* aParent,
    const ICameraControl::RecorderProfile::Video& aProfile)
  : mParent(aParent)
  , mCodec(aProfile.GetCodec())
  , mBitrate(aProfile.GetBitsPerSecond())
  , mFramerate(aProfile.GetFramesPerSecond())
{
  DOM_CAMERA_LOGT("%s:%d : this=%p\n", __func__, __LINE__, this);

  mSize.mWidth = aProfile.GetSize().width;
  mSize.mHeight = aProfile.GetSize().height;

  DOM_CAMERA_LOGI("  video: '%s' %ux%u bps=%u fps=%u\n",
    NS_ConvertUTF16toUTF8(mCodec).get(), mSize.mWidth, mSize.mHeight, mBitrate, mFramerate);
}

CameraRecorderVideoProfile::~CameraRecorderVideoProfile()
{
  DOM_CAMERA_LOGT("%s:%d : this=%p\n", __func__, __LINE__, this);
}

/**
 * CameraRecorderAudioProfile
 */
NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(CameraRecorderAudioProfile, mParent)

NS_IMPL_CYCLE_COLLECTING_ADDREF(CameraRecorderAudioProfile)
NS_IMPL_CYCLE_COLLECTING_RELEASE(CameraRecorderAudioProfile)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(CameraRecorderAudioProfile)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

JSObject*
CameraRecorderAudioProfile::WrapObject(JSContext* aCx)
{
  return CameraRecorderAudioProfileBinding::Wrap(aCx, this);
}

CameraRecorderAudioProfile::CameraRecorderAudioProfile(nsISupports* aParent,
    const ICameraControl::RecorderProfile::Audio& aProfile)
  : mParent(aParent)
  , mCodec(aProfile.GetCodec())
  , mBitrate(aProfile.GetBitsPerSecond())
  , mSamplerate(aProfile.GetSamplesPerSecond())
  , mChannels(aProfile.GetChannels())
{
  DOM_CAMERA_LOGT("%s:%d : this=%p\n", __func__, __LINE__, this);
  DOM_CAMERA_LOGI("  audio: '%s' bps=%u samples/s=%u channels=%u\n",
    NS_ConvertUTF16toUTF8(mCodec).get(), mBitrate, mSamplerate, mChannels);
}

CameraRecorderAudioProfile::~CameraRecorderAudioProfile()
{
  DOM_CAMERA_LOGT("%s:%d : this=%p\n", __func__, __LINE__, this);
}

/**
 * CameraRecorderProfile
 */
NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(CameraRecorderProfile, mParent)

NS_IMPL_CYCLE_COLLECTING_ADDREF(CameraRecorderProfile)
NS_IMPL_CYCLE_COLLECTING_RELEASE(CameraRecorderProfile)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(CameraRecorderProfile)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

JSObject*
CameraRecorderProfile::WrapObject(JSContext* aCx)
{
  return CameraRecorderProfileBinding::Wrap(aCx, this);
}

CameraRecorderProfile::CameraRecorderProfile(nsISupports* aParent,
                                             const ICameraControl::RecorderProfile& aProfile)
  : mParent(aParent)
  , mName(aProfile.GetName())
  , mContainerFormat(aProfile.GetContainer())
  , mMimeType(aProfile.GetMimeType())
  , mVideo(new CameraRecorderVideoProfile(this, aProfile.GetVideo()))
  , mAudio(new CameraRecorderAudioProfile(this, aProfile.GetAudio()))
{
  DOM_CAMERA_LOGT("%s:%d : this=%p\n", __func__, __LINE__, this);
  DOM_CAMERA_LOGI("profile: '%s' container=%s mime-type=%s\n",
    NS_ConvertUTF16toUTF8(mName).get(),
    NS_ConvertUTF16toUTF8(mContainerFormat).get(),
    NS_ConvertUTF16toUTF8(mMimeType).get());
}

CameraRecorderProfile::~CameraRecorderProfile()
{
  DOM_CAMERA_LOGT("%s:%d : this=%p\n", __func__, __LINE__, this);
}

/**
 * CameraRecorderProfiles
 */
NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(CameraRecorderProfiles, mParent)

NS_IMPL_CYCLE_COLLECTING_ADDREF(CameraRecorderProfiles)
NS_IMPL_CYCLE_COLLECTING_RELEASE(CameraRecorderProfiles)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(CameraRecorderProfiles)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

JSObject*
CameraRecorderProfiles::WrapObject(JSContext* aCx)
{
  return CameraRecorderProfilesBinding::Wrap(aCx, this);
}

CameraRecorderProfiles::CameraRecorderProfiles(nsISupports* aParent,
                                               ICameraControl* aCameraControl)
  : mParent(aParent)
  , mCameraControl(aCameraControl)
{
  DOM_CAMERA_LOGT("%s:%d : this=%p\n", __func__, __LINE__, this);
}

CameraRecorderProfiles::~CameraRecorderProfiles()
{
  DOM_CAMERA_LOGT("%s:%d : this=%p\n", __func__, __LINE__, this);
}

void
CameraRecorderProfiles::GetSupportedNames(unsigned aFlags, nsTArray<nsString>& aNames)
{
  DOM_CAMERA_LOGT("%s:%d : this=%p, flags=0x%x\n",
    __func__, __LINE__, this, aFlags);

  nsresult rv = mCameraControl->GetRecorderProfiles(aNames);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aNames.Clear();
  }
}

CameraRecorderProfile*
CameraRecorderProfiles::NamedGetter(const nsAString& aName, bool& aFound)
{
  DOM_CAMERA_LOGT("%s:%d : this=%p, name='%s'\n", __func__, __LINE__, this,
    NS_ConvertUTF16toUTF8(aName).get());

  CameraRecorderProfile* profile = mProfiles.GetWeak(aName, &aFound);
  if (!aFound || !profile) {
    nsRefPtr<ICameraControl::RecorderProfile> p = mCameraControl->GetProfileInfo(aName);
    if (p) {
      profile = new CameraRecorderProfile(this, *p);
      mProfiles.Put(aName, profile);
      aFound = true;
    }
  }
  return profile;
}

bool
CameraRecorderProfiles::NameIsEnumerable(const nsAString& aName)
{
  DOM_CAMERA_LOGT("%s:%d : this=%p, name='%s' (always returns true)\n",
    __func__, __LINE__, this, NS_ConvertUTF16toUTF8(aName).get());

  return true;
}

/**
 * CameraCapabilities
 */
NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(CameraCapabilities, mWindow)

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

CameraCapabilities::CameraCapabilities(nsPIDOMWindow* aWindow,
                                       ICameraControl* aCameraControl)
  : mMaxFocusAreas(0)
  , mMaxMeteringAreas(0)
  , mMaxDetectedFaces(0)
  , mMinExposureCompensation(0.0)
  , mMaxExposureCompensation(0.0)
  , mExposureCompensationStep(0.0)
  , mWindow(aWindow)
  , mCameraControl(aCameraControl)
{
  DOM_CAMERA_LOGT("%s:%d : this=%p\n", __func__, __LINE__, this);
  MOZ_COUNT_CTOR(CameraCapabilities);
}

CameraCapabilities::~CameraCapabilities()
{
  DOM_CAMERA_LOGT("%s:%d : this=%p\n", __func__, __LINE__, this);
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
CameraCapabilities::TranslateToDictionary(uint32_t aKey, nsTArray<CameraSize>& aSizes)
{
  nsresult rv;
  nsTArray<ICameraControl::Size> sizes;

  rv = mCameraControl->Get(aKey, sizes);
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

void
CameraCapabilities::GetPreviewSizes(nsTArray<dom::CameraSize>& retval)
{
  if (mPreviewSizes.Length() == 0) {
    nsresult rv = TranslateToDictionary(CAMERA_PARAM_SUPPORTED_PREVIEWSIZES,
                                        mPreviewSizes);
    LOG_IF_ERROR(rv, CAMERA_PARAM_SUPPORTED_PREVIEWSIZES);
  }
  retval = mPreviewSizes;
}

void
CameraCapabilities::GetPictureSizes(nsTArray<dom::CameraSize>& retval)
{
  if (mPictureSizes.Length() == 0) {
    nsresult rv = TranslateToDictionary(CAMERA_PARAM_SUPPORTED_PICTURESIZES,
                                        mPictureSizes);
    LOG_IF_ERROR(rv, CAMERA_PARAM_SUPPORTED_PICTURESIZES);
  }
  retval = mPictureSizes;
}

void
CameraCapabilities::GetThumbnailSizes(nsTArray<dom::CameraSize>& retval)
{
  if (mThumbnailSizes.Length() == 0) {
    nsresult rv = TranslateToDictionary(CAMERA_PARAM_SUPPORTED_JPEG_THUMBNAIL_SIZES,
                                        mThumbnailSizes);
    LOG_IF_ERROR(rv, CAMERA_PARAM_SUPPORTED_JPEG_THUMBNAIL_SIZES);
  }
  retval = mThumbnailSizes;
}

void
CameraCapabilities::GetVideoSizes(nsTArray<dom::CameraSize>& retval)
{
  if (mVideoSizes.Length() == 0) {
    nsresult rv = TranslateToDictionary(CAMERA_PARAM_SUPPORTED_VIDEOSIZES,
                                        mVideoSizes);
    LOG_IF_ERROR(rv, CAMERA_PARAM_SUPPORTED_VIDEOSIZES);
  }
  retval = mVideoSizes;
}

void
CameraCapabilities::GetFileFormats(nsTArray<nsString>& retval)
{
  if (mFileFormats.Length() == 0) {
    nsresult rv = mCameraControl->Get(CAMERA_PARAM_SUPPORTED_PICTUREFORMATS,
                                      mFileFormats);
    LOG_IF_ERROR(rv, CAMERA_PARAM_SUPPORTED_PICTUREFORMATS);
  }
  retval = mFileFormats;
}

void
CameraCapabilities::GetWhiteBalanceModes(nsTArray<nsString>& retval)
{
  if (mWhiteBalanceModes.Length() == 0) {
    nsresult rv = mCameraControl->Get(CAMERA_PARAM_SUPPORTED_WHITEBALANCES,
                                      mWhiteBalanceModes);
    LOG_IF_ERROR(rv, CAMERA_PARAM_SUPPORTED_WHITEBALANCES);
  }
  retval = mWhiteBalanceModes;
}

void
CameraCapabilities::GetSceneModes(nsTArray<nsString>& retval)
{
  if (mSceneModes.Length() == 0) {
    nsresult rv = mCameraControl->Get(CAMERA_PARAM_SUPPORTED_SCENEMODES,
                                      mSceneModes);
    LOG_IF_ERROR(rv, CAMERA_PARAM_SUPPORTED_SCENEMODES);
  }
  retval = mSceneModes;
}

void
CameraCapabilities::GetEffects(nsTArray<nsString>& retval)
{
  if (mEffects.Length() == 0) {
    nsresult rv = mCameraControl->Get(CAMERA_PARAM_SUPPORTED_EFFECTS,
                                      mEffects);
    LOG_IF_ERROR(rv, CAMERA_PARAM_SUPPORTED_EFFECTS);
  }
  retval = mEffects;
}

void
CameraCapabilities::GetFlashModes(nsTArray<nsString>& retval)
{
  if (mFlashModes.Length() == 0) {
    nsresult rv = mCameraControl->Get(CAMERA_PARAM_SUPPORTED_FLASHMODES,
                                      mFlashModes);
    LOG_IF_ERROR(rv, CAMERA_PARAM_SUPPORTED_FLASHMODES);
  }
  retval = mFlashModes;
}

void
CameraCapabilities::GetFocusModes(nsTArray<nsString>& retval)
{
  if (mFocusModes.Length() == 0) {
    nsresult rv = mCameraControl->Get(CAMERA_PARAM_SUPPORTED_FOCUSMODES,
                                      mFocusModes);
    LOG_IF_ERROR(rv, CAMERA_PARAM_SUPPORTED_FOCUSMODES);
  }
  retval = mFocusModes;
}

void
CameraCapabilities::GetZoomRatios(nsTArray<double>& retval)
{
  if (mZoomRatios.Length() == 0) {
    nsresult rv = mCameraControl->Get(CAMERA_PARAM_SUPPORTED_ZOOMRATIOS,
                                      mZoomRatios);
    LOG_IF_ERROR(rv, CAMERA_PARAM_SUPPORTED_ZOOMRATIOS);
  }
  retval = mZoomRatios;
}

uint32_t
CameraCapabilities::MaxFocusAreas()
{
  if (mMaxFocusAreas == 0) {
    int32_t areas;
    nsresult rv = mCameraControl->Get(CAMERA_PARAM_SUPPORTED_MAXFOCUSAREAS,
                                      areas);
    LOG_IF_ERROR(rv, CAMERA_PARAM_SUPPORTED_MAXFOCUSAREAS);
    mMaxFocusAreas = areas < 0 ? 0 : areas;
  }
  return mMaxFocusAreas;
}

uint32_t
CameraCapabilities::MaxMeteringAreas()
{
  if (mMaxMeteringAreas == 0) {
    int32_t areas;
    nsresult rv = mCameraControl->Get(CAMERA_PARAM_SUPPORTED_MAXMETERINGAREAS,
                                      areas);
    LOG_IF_ERROR(rv, CAMERA_PARAM_SUPPORTED_MAXMETERINGAREAS);
    mMaxMeteringAreas = areas < 0 ? 0 : areas;
  }
  return mMaxMeteringAreas;
}

uint32_t
CameraCapabilities::MaxDetectedFaces()
{
  if (mMaxDetectedFaces == 0) {
    int32_t faces;
    nsresult rv = mCameraControl->Get(CAMERA_PARAM_SUPPORTED_MAXDETECTEDFACES,
                                      faces);
    LOG_IF_ERROR(rv, CAMERA_PARAM_SUPPORTED_MAXDETECTEDFACES);
    mMaxDetectedFaces = faces < 0 ? 0 : faces;
  }
  return mMaxDetectedFaces;
}

double
CameraCapabilities::MinExposureCompensation()
{
  if (mMinExposureCompensation == 0.0) {
    nsresult rv = mCameraControl->Get(CAMERA_PARAM_SUPPORTED_MINEXPOSURECOMPENSATION,
                                      mMinExposureCompensation);
    LOG_IF_ERROR(rv, CAMERA_PARAM_SUPPORTED_MINEXPOSURECOMPENSATION);
  }
  return mMinExposureCompensation;
}

double
CameraCapabilities::MaxExposureCompensation()
{
  if (mMaxExposureCompensation == 0.0) {
    nsresult rv = mCameraControl->Get(CAMERA_PARAM_SUPPORTED_MAXEXPOSURECOMPENSATION,
                                      mMaxExposureCompensation);
    LOG_IF_ERROR(rv, CAMERA_PARAM_SUPPORTED_MAXEXPOSURECOMPENSATION);
  }
  return mMaxExposureCompensation;
}

double
CameraCapabilities::ExposureCompensationStep()
{
  if (mExposureCompensationStep == 0.0) {
    nsresult rv = mCameraControl->Get(CAMERA_PARAM_SUPPORTED_EXPOSURECOMPENSATIONSTEP,
                                      mExposureCompensationStep);
    LOG_IF_ERROR(rv, CAMERA_PARAM_SUPPORTED_EXPOSURECOMPENSATIONSTEP);
  }
  return mExposureCompensationStep;
}

CameraRecorderProfiles*
CameraCapabilities::RecorderProfiles()
{
  nsRefPtr<CameraRecorderProfiles> profiles = mRecorderProfiles;
  if (!mRecorderProfiles) {
    profiles = new CameraRecorderProfiles(this, mCameraControl);
    mRecorderProfiles = profiles;
  }
  return profiles;
}

void
CameraCapabilities::GetIsoModes(nsTArray<nsString>& retval)
{
  if (mIsoModes.Length() == 0) {
    nsresult rv = mCameraControl->Get(CAMERA_PARAM_SUPPORTED_ISOMODES,
                                      mIsoModes);
    LOG_IF_ERROR(rv, CAMERA_PARAM_SUPPORTED_ISOMODES);
  }
  retval = mIsoModes;
}

} // namespace dom
} // namespace mozilla
