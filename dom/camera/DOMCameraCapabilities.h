/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_CameraCapabilities_h__
#define mozilla_dom_CameraCapabilities_h__

#include "nsString.h"
#include "nsAutoPtr.h"
#include "base/basictypes.h"
#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/CameraManagerBinding.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"
#include "nsPIDOMWindow.h"
#include "nsHashKeys.h"
#include "nsRefPtrHashtable.h"
#include "nsDataHashtable.h"
#include "ICameraControl.h"

struct JSContext;

namespace mozilla {
namespace dom {

/**
 * CameraRecorderVideoProfile
 */
class CameraRecorderVideoProfile final : public nsISupports
                                       , public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(CameraRecorderVideoProfile)

  explicit CameraRecorderVideoProfile(nsISupports* aParent,
    const ICameraControl::RecorderProfile::Video& aProfile);
  nsISupports* GetParentObject() const        { return mParent; }
  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  uint32_t BitsPerSecond() const              { return mBitrate; }
  uint32_t FramesPerSecond() const            { return mFramerate; }
  void GetCodec(nsAString& aCodec) const      { aCodec = mCodec; }

  void GetSize(dom::CameraSize& aSize) const  { aSize = mSize; }

  // XXXmikeh - legacy, remove these when the Camera app is updated
  uint32_t Width() const                      { return mSize.mWidth; }
  uint32_t Height() const                     { return mSize.mHeight; }

protected:
  virtual ~CameraRecorderVideoProfile();

  nsCOMPtr<nsISupports> mParent;

  const nsString mCodec;
  uint32_t mBitrate;
  uint32_t mFramerate;
  dom::CameraSize mSize;

private:
  DISALLOW_EVIL_CONSTRUCTORS(CameraRecorderVideoProfile);
};

/**
 * CameraRecorderAudioProfile
 */
class CameraRecorderAudioProfile final : public nsISupports
                                       , public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(CameraRecorderAudioProfile)

  explicit CameraRecorderAudioProfile(nsISupports* aParent,
    const ICameraControl::RecorderProfile::Audio& aProfile);
  nsISupports* GetParentObject() const    { return mParent; }
  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  uint32_t BitsPerSecond() const          { return mBitrate; }
  uint32_t SamplesPerSecond() const       { return mSamplerate; }
  uint32_t Channels() const               { return mChannels; }
  void GetCodec(nsAString& aCodec) const  { aCodec = mCodec; }

protected:
  virtual ~CameraRecorderAudioProfile();

  nsCOMPtr<nsISupports> mParent;

  const nsString mCodec;
  uint32_t mBitrate;
  uint32_t mSamplerate;
  uint32_t mChannels;

private:
  DISALLOW_EVIL_CONSTRUCTORS(CameraRecorderAudioProfile);
};

/**
 * CameraRecorderProfile
 */
class CameraRecorderProfile final : public nsISupports
                                  , public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(CameraRecorderProfile)

  explicit CameraRecorderProfile(nsISupports* aParent,
                                 const ICameraControl::RecorderProfile& aProfile);
  nsISupports* GetParentObject() const          { return mParent; }
  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  void GetMimeType(nsAString& aMimeType) const  { aMimeType = mMimeType; }

  CameraRecorderVideoProfile* Video()           { return mVideo; }
  CameraRecorderAudioProfile* Audio()           { return mAudio; }

  void GetName(nsAString& aName) const          { aName = mName; }

  void
  GetContainerFormat(nsAString& aContainerFormat) const
  {
    aContainerFormat = mContainerFormat;
  }

protected:
  virtual ~CameraRecorderProfile();

  nsCOMPtr<nsISupports> mParent;

  const nsString mName;
  const nsString mContainerFormat;
  const nsString mMimeType;

  RefPtr<CameraRecorderVideoProfile> mVideo;
  RefPtr<CameraRecorderAudioProfile> mAudio;

private:
  DISALLOW_EVIL_CONSTRUCTORS(CameraRecorderProfile);
};

/**
 * CameraRecorderProfiles
 */
template<class T> class CameraClosedListenerProxy;

class CameraRecorderProfiles final : public nsISupports
                                   , public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(CameraRecorderProfiles)

  explicit CameraRecorderProfiles(nsISupports* aParent,
                                  ICameraControl* aCameraControl);
  nsISupports* GetParentObject() const { return mParent; }
  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  CameraRecorderProfile* NamedGetter(const nsAString& aName, bool& aFound);
  void GetSupportedNames(unsigned aFlags, nsTArray<nsString>& aNames);

  virtual void OnHardwareClosed();

protected:
  virtual ~CameraRecorderProfiles();

  nsCOMPtr<nsISupports> mParent;
  RefPtr<ICameraControl> mCameraControl;
  nsRefPtrHashtable<nsStringHashKey, CameraRecorderProfile> mProfiles;
  RefPtr<CameraClosedListenerProxy<CameraRecorderProfiles>> mListener;

private:
  DISALLOW_EVIL_CONSTRUCTORS(CameraRecorderProfiles);
};

/**
 * CameraCapabilities
 */
class CameraCapabilities final : public nsISupports
                               , public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(CameraCapabilities)

  // Because this header's filename doesn't match its C++ or DOM-facing
  // classname, we can't rely on the [Func="..."] WebIDL tag to implicitly
  // include the right header for us; instead we must explicitly include a
  // HasSupport() method in each header. We can get rid of these with the
  // Great Renaming proposed in bug 983177.
  static bool HasSupport(JSContext* aCx, JSObject* aGlobal);

  explicit CameraCapabilities(nsPIDOMWindowInner* aWindow,
                              ICameraControl* aCameraControl);

  nsPIDOMWindowInner* GetParentObject() const { return mWindow; }

  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  void GetPreviewSizes(nsTArray<CameraSize>& aRetVal);
  void GetPictureSizes(nsTArray<CameraSize>& aRetVal);
  void GetThumbnailSizes(nsTArray<CameraSize>& aRetVal);
  void GetVideoSizes(nsTArray<CameraSize>& aRetVal);
  void GetFileFormats(nsTArray<nsString>& aRetVal);
  void GetWhiteBalanceModes(nsTArray<nsString>& aRetVal);
  void GetSceneModes(nsTArray<nsString>& aRetVal);
  void GetEffects(nsTArray<nsString>& aRetVal);
  void GetFlashModes(nsTArray<nsString>& aRetVal);
  void GetFocusModes(nsTArray<nsString>& aRetVal);
  void GetZoomRatios(nsTArray<double>& aRetVal);
  uint32_t MaxFocusAreas();
  uint32_t MaxMeteringAreas();
  uint32_t MaxDetectedFaces();
  double MinExposureCompensation();
  double MaxExposureCompensation();
  double ExposureCompensationStep();
  void GetIsoModes(nsTArray<nsString>& aRetVal);
  void GetMeteringModes(nsTArray<nsString>& aRetVal);

  CameraRecorderProfiles* RecorderProfiles();

  virtual void OnHardwareClosed();

protected:
  ~CameraCapabilities();

  nsresult TranslateToDictionary(uint32_t aKey, nsTArray<CameraSize>& aSizes);

  RefPtr<nsPIDOMWindowInner> mWindow;
  RefPtr<ICameraControl> mCameraControl;
  RefPtr<CameraClosedListenerProxy<CameraCapabilities>> mListener;

private:
  DISALLOW_EVIL_CONSTRUCTORS(CameraCapabilities);
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_CameraCapabilities_h__
