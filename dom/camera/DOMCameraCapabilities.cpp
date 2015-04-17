/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DOMCameraCapabilities.h"
#include "nsPIDOMWindow.h"
#include "nsContentUtils.h"
#include "nsProxyRelease.h"
#include "mozilla/dom/CameraManagerBinding.h"
#include "mozilla/dom/CameraCapabilitiesBinding.h"
#include "Navigator.h"
#include "CameraCommon.h"
#include "ICameraControl.h"
#include "CameraControlListener.h"

namespace mozilla {
namespace dom {

/**
 * CameraClosedListenerProxy and CameraClosedMessage
 */
template<class T>
class CameraClosedMessage : public nsRunnable
{
public:
  explicit CameraClosedMessage(nsMainThreadPtrHandle<T> aListener)
    : mListener(aListener)
  {
    DOM_CAMERA_LOGT("%s:%d : this=%p\n", __func__, __LINE__, this);
  }

  NS_IMETHODIMP
  Run() override
  {
    MOZ_ASSERT(NS_IsMainThread());

    nsRefPtr<T> listener = mListener.get();
    if (listener) {
      listener->OnHardwareClosed();
    }
    return NS_OK;
  }

protected:
  virtual ~CameraClosedMessage()
  {
    DOM_CAMERA_LOGT("%s:%d : this=%p\n", __func__, __LINE__, this);
  }

  nsMainThreadPtrHandle<T> mListener;
};

template<class T>
class CameraClosedListenerProxy : public CameraControlListener
{
public:
  explicit CameraClosedListenerProxy(T* aListener)
    : mListener(new nsMainThreadPtrHolder<T>(aListener))
  {
    DOM_CAMERA_LOGT("%s:%d : this=%p\n", __func__, __LINE__, this);
  }

  virtual void
  OnHardwareStateChange(HardwareState aState, nsresult aReason) override
  {
    if (aState != kHardwareClosed) {
      return;
    }
    NS_DispatchToMainThread(new CameraClosedMessage<T>(mListener));
  }

protected:
  virtual ~CameraClosedListenerProxy()
  {
    DOM_CAMERA_LOGT("%s:%d : this=%p\n", __func__, __LINE__, this);
  }

  nsMainThreadPtrHandle<T> mListener;
};

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
CameraRecorderVideoProfile::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return CameraRecorderVideoProfileBinding::Wrap(aCx, this, aGivenProto);
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
CameraRecorderAudioProfile::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return CameraRecorderAudioProfileBinding::Wrap(aCx, this, aGivenProto);
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
NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(CameraRecorderProfile,
                                      mParent,
                                      mVideo,
                                      mAudio)

NS_IMPL_CYCLE_COLLECTING_ADDREF(CameraRecorderProfile)
NS_IMPL_CYCLE_COLLECTING_RELEASE(CameraRecorderProfile)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(CameraRecorderProfile)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

JSObject*
CameraRecorderProfile::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return CameraRecorderProfileBinding::Wrap(aCx, this, aGivenProto);
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
NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(CameraRecorderProfiles,
                                      mParent,
                                      mProfiles)

NS_IMPL_CYCLE_COLLECTING_ADDREF(CameraRecorderProfiles)
NS_IMPL_CYCLE_COLLECTING_RELEASE(CameraRecorderProfiles)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(CameraRecorderProfiles)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

JSObject*
CameraRecorderProfiles::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return CameraRecorderProfilesBinding::Wrap(aCx, this, aGivenProto);
}

CameraRecorderProfiles::CameraRecorderProfiles(nsISupports* aParent,
                                               ICameraControl* aCameraControl)
  : mParent(aParent)
  , mCameraControl(aCameraControl)
{
  DOM_CAMERA_LOGT("%s:%d : this=%p\n", __func__, __LINE__, this);
  if (mCameraControl) {
    mListener = new CameraClosedListenerProxy<CameraRecorderProfiles>(this);
    mCameraControl->AddListener(mListener);
  }
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
  if (!mCameraControl) {
    aNames.Clear();
    return;
  }

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
  if (!mCameraControl) {
    return nullptr;
  }

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

void
CameraRecorderProfiles::OnHardwareClosed()
{
  DOM_CAMERA_LOGT("%s:%d : this=%p\n", __func__, __LINE__, this);
  MOZ_ASSERT(NS_IsMainThread());

  if (mCameraControl) {
    mCameraControl->RemoveListener(mListener);
    mCameraControl = nullptr;
  }
  mListener = nullptr;
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
  : mWindow(aWindow)
  , mCameraControl(aCameraControl)
{
  DOM_CAMERA_LOGT("%s:%d : this=%p\n", __func__, __LINE__, this);
  MOZ_COUNT_CTOR(CameraCapabilities);
  if (mCameraControl) {
    mListener = new CameraClosedListenerProxy<CameraCapabilities>(this);
    mCameraControl->AddListener(mListener);
  }
}

CameraCapabilities::~CameraCapabilities()
{
  DOM_CAMERA_LOGT("%s:%d : this=%p\n", __func__, __LINE__, this);
  MOZ_COUNT_DTOR(CameraCapabilities);
}

void
CameraCapabilities::OnHardwareClosed()
{
  DOM_CAMERA_LOGT("%s:%d : this=%p\n", __func__, __LINE__, this);
  MOZ_ASSERT(NS_IsMainThread());

  if (mCameraControl) {
    mCameraControl->RemoveListener(mListener);
    mCameraControl = nullptr;
  }
  mListener = nullptr;
}

JSObject*
CameraCapabilities::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return CameraCapabilitiesBinding::Wrap(aCx, this, aGivenProto);
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
  if (NS_WARN_IF(!mCameraControl)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

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

// The following attributes are tagged [Cached, Constant] in the WebIDL, so
// the framework will handle caching them for us.

void
CameraCapabilities::GetPreviewSizes(nsTArray<dom::CameraSize>& aRetVal)
{
  nsresult rv = TranslateToDictionary(CAMERA_PARAM_SUPPORTED_PREVIEWSIZES, aRetVal);
  LOG_IF_ERROR(rv, CAMERA_PARAM_SUPPORTED_PREVIEWSIZES);
}

void
CameraCapabilities::GetPictureSizes(nsTArray<dom::CameraSize>& aRetVal)
{
  nsresult rv = TranslateToDictionary(CAMERA_PARAM_SUPPORTED_PICTURESIZES, aRetVal);
  LOG_IF_ERROR(rv, CAMERA_PARAM_SUPPORTED_PICTURESIZES);
}

void
CameraCapabilities::GetThumbnailSizes(nsTArray<dom::CameraSize>& aRetVal)
{
  nsresult rv = TranslateToDictionary(CAMERA_PARAM_SUPPORTED_JPEG_THUMBNAIL_SIZES, aRetVal);
  LOG_IF_ERROR(rv, CAMERA_PARAM_SUPPORTED_JPEG_THUMBNAIL_SIZES);
}

void
CameraCapabilities::GetVideoSizes(nsTArray<dom::CameraSize>& aRetVal)
{
  nsresult rv = TranslateToDictionary(CAMERA_PARAM_SUPPORTED_VIDEOSIZES, aRetVal);
  LOG_IF_ERROR(rv, CAMERA_PARAM_SUPPORTED_VIDEOSIZES);
}

void
CameraCapabilities::GetFileFormats(nsTArray<nsString>& aRetVal)
{
  if (NS_WARN_IF(!mCameraControl)) {
    return;
  }

  nsresult rv = mCameraControl->Get(CAMERA_PARAM_SUPPORTED_PICTUREFORMATS, aRetVal);
  LOG_IF_ERROR(rv, CAMERA_PARAM_SUPPORTED_PICTUREFORMATS);
}

void
CameraCapabilities::GetWhiteBalanceModes(nsTArray<nsString>& aRetVal)
{
  if (NS_WARN_IF(!mCameraControl)) {
    return;
  }

  nsresult rv = mCameraControl->Get(CAMERA_PARAM_SUPPORTED_WHITEBALANCES, aRetVal);
  LOG_IF_ERROR(rv, CAMERA_PARAM_SUPPORTED_WHITEBALANCES);
}

void
CameraCapabilities::GetSceneModes(nsTArray<nsString>& aRetVal)
{
  if (NS_WARN_IF(!mCameraControl)) {
    return;
  }

  nsresult rv = mCameraControl->Get(CAMERA_PARAM_SUPPORTED_SCENEMODES, aRetVal);
  LOG_IF_ERROR(rv, CAMERA_PARAM_SUPPORTED_SCENEMODES);
}

void
CameraCapabilities::GetEffects(nsTArray<nsString>& aRetVal)
{
  if (NS_WARN_IF(!mCameraControl)) {
    return;
  }

  nsresult rv = mCameraControl->Get(CAMERA_PARAM_SUPPORTED_EFFECTS, aRetVal);
  LOG_IF_ERROR(rv, CAMERA_PARAM_SUPPORTED_EFFECTS);
}

void
CameraCapabilities::GetFlashModes(nsTArray<nsString>& aRetVal)
{
  if (NS_WARN_IF(!mCameraControl)) {
    return;
  }

  nsresult rv = mCameraControl->Get(CAMERA_PARAM_SUPPORTED_FLASHMODES, aRetVal);
  LOG_IF_ERROR(rv, CAMERA_PARAM_SUPPORTED_FLASHMODES);
}

void
CameraCapabilities::GetFocusModes(nsTArray<nsString>& aRetVal)
{
  if (NS_WARN_IF(!mCameraControl)) {
    return;
  }

  nsresult rv = mCameraControl->Get(CAMERA_PARAM_SUPPORTED_FOCUSMODES, aRetVal);
  LOG_IF_ERROR(rv, CAMERA_PARAM_SUPPORTED_FOCUSMODES);
}

void
CameraCapabilities::GetZoomRatios(nsTArray<double>& aRetVal)
{
  if (NS_WARN_IF(!mCameraControl)) {
    return;
  }

  nsresult rv = mCameraControl->Get(CAMERA_PARAM_SUPPORTED_ZOOMRATIOS, aRetVal);
  LOG_IF_ERROR(rv, CAMERA_PARAM_SUPPORTED_ZOOMRATIOS);
}

uint32_t
CameraCapabilities::MaxFocusAreas()
{
  if (NS_WARN_IF(!mCameraControl)) {
    return 0;
  }

  int32_t areas = 0;
  nsresult rv = mCameraControl->Get(CAMERA_PARAM_SUPPORTED_MAXFOCUSAREAS, areas);
  LOG_IF_ERROR(rv, CAMERA_PARAM_SUPPORTED_MAXFOCUSAREAS);
  return areas < 0 ? 0 : areas;
}

uint32_t
CameraCapabilities::MaxMeteringAreas()
{
  if (NS_WARN_IF(!mCameraControl)) {
    return 0;
  }

  int32_t areas = 0;
  nsresult rv = mCameraControl->Get(CAMERA_PARAM_SUPPORTED_MAXMETERINGAREAS, areas);
  LOG_IF_ERROR(rv, CAMERA_PARAM_SUPPORTED_MAXMETERINGAREAS);
  return areas < 0 ? 0 : areas;
}

uint32_t
CameraCapabilities::MaxDetectedFaces()
{
  if (NS_WARN_IF(!mCameraControl)) {
    return 0;
  }

  int32_t faces = 0;
  nsresult rv = mCameraControl->Get(CAMERA_PARAM_SUPPORTED_MAXDETECTEDFACES, faces);
  LOG_IF_ERROR(rv, CAMERA_PARAM_SUPPORTED_MAXDETECTEDFACES);
  return faces < 0 ? 0 : faces;
}

double
CameraCapabilities::MinExposureCompensation()
{
  if (NS_WARN_IF(!mCameraControl)) {
    return 0.0;
  }

  double minEv = 0.0;
  nsresult rv = mCameraControl->Get(CAMERA_PARAM_SUPPORTED_MINEXPOSURECOMPENSATION, minEv);
  LOG_IF_ERROR(rv, CAMERA_PARAM_SUPPORTED_MINEXPOSURECOMPENSATION);
  return minEv;
}

double
CameraCapabilities::MaxExposureCompensation()
{
  if (NS_WARN_IF(!mCameraControl)) {
    return 0.0;
  }

  double maxEv = 0.0;
  nsresult rv = mCameraControl->Get(CAMERA_PARAM_SUPPORTED_MAXEXPOSURECOMPENSATION, maxEv);
  LOG_IF_ERROR(rv, CAMERA_PARAM_SUPPORTED_MAXEXPOSURECOMPENSATION);
  return maxEv;
}

double
CameraCapabilities::ExposureCompensationStep()
{
  if (NS_WARN_IF(!mCameraControl)) {
    return 0.0;
  }

  double evStep = 0.0;
  nsresult rv = mCameraControl->Get(CAMERA_PARAM_SUPPORTED_EXPOSURECOMPENSATIONSTEP, evStep);
  LOG_IF_ERROR(rv, CAMERA_PARAM_SUPPORTED_EXPOSURECOMPENSATIONSTEP);
  return evStep;
}

CameraRecorderProfiles*
CameraCapabilities::RecorderProfiles()
{
  if (NS_WARN_IF(!mCameraControl)) {
    return nullptr;
  }

  nsRefPtr<CameraRecorderProfiles> profiles =
    new CameraRecorderProfiles(this, mCameraControl);
  return profiles;
}

void
CameraCapabilities::GetIsoModes(nsTArray<nsString>& aRetVal)
{
  if (NS_WARN_IF(!mCameraControl)) {
    return;
  }

  nsresult rv = mCameraControl->Get(CAMERA_PARAM_SUPPORTED_ISOMODES, aRetVal);
  LOG_IF_ERROR(rv, CAMERA_PARAM_SUPPORTED_ISOMODES);
}

void
CameraCapabilities::GetMeteringModes(nsTArray<nsString>& aRetVal)
{
  if (NS_WARN_IF(!mCameraControl)) {
    return;
  }

  nsresult rv = mCameraControl->Get(CAMERA_PARAM_SUPPORTED_METERINGMODES, aRetVal);
  LOG_IF_ERROR(rv, CAMERA_PARAM_SUPPORTED_METERINGMODES);
}

} // namespace dom
} // namespace mozilla
