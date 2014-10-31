/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_CameraCapabilities_h__
#define mozilla_dom_CameraCapabilities_h__

#include "nsString.h"
#include "nsAutoPtr.h"
#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/CameraManagerBinding.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"
#include "nsPIDOMWindow.h"

struct JSContext;
class nsPIDOMWindow;

namespace mozilla {

class ICameraControl;
class RecorderProfileManager;

namespace dom {

class CameraCapabilities MOZ_FINAL : public nsISupports
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

  explicit CameraCapabilities(nsPIDOMWindow* aWindow);

  // Populate the camera capabilities interface from the specific
  // camera control object.
  //
  // Return values:
  //  - NS_OK on success;
  //  - NS_ERROR_INVALID_ARG if 'aCameraControl' is null.
  nsresult Populate(ICameraControl* aCameraControl);

  nsPIDOMWindow* GetParentObject() const { return mWindow; }

  virtual JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  void GetPreviewSizes(nsTArray<CameraSize>& aRetVal) const;
  void GetPictureSizes(nsTArray<CameraSize>& aRetVal) const;
  void GetThumbnailSizes(nsTArray<CameraSize>& aRetVal) const;
  void GetVideoSizes(nsTArray<CameraSize>& aRetVal) const;
  void GetFileFormats(nsTArray<nsString>& aRetVal) const;
  void GetWhiteBalanceModes(nsTArray<nsString>& aRetVal) const;
  void GetSceneModes(nsTArray<nsString>& aRetVal) const;
  void GetEffects(nsTArray<nsString>& aRetVal) const;
  void GetFlashModes(nsTArray<nsString>& aRetVal) const;
  void GetFocusModes(nsTArray<nsString>& aRetVal) const;
  void GetZoomRatios(nsTArray<double>& aRetVal) const;
  uint32_t MaxFocusAreas() const;
  uint32_t MaxMeteringAreas() const;
  uint32_t MaxDetectedFaces() const;
  double MinExposureCompensation() const;
  double MaxExposureCompensation() const;
  double ExposureCompensationStep() const;
  void GetRecorderProfiles(JSContext* aCx, JS::MutableHandle<JS::Value> aRetval) const;
  void GetIsoModes(nsTArray<nsString>& aRetVal) const;

protected:
  ~CameraCapabilities();

  nsresult TranslateToDictionary(ICameraControl* aCameraControl,
                                 uint32_t aKey, nsTArray<CameraSize>& aSizes);

  nsTArray<CameraSize> mPreviewSizes;
  nsTArray<CameraSize> mPictureSizes;
  nsTArray<CameraSize> mThumbnailSizes;
  nsTArray<CameraSize> mVideoSizes;

  nsTArray<nsString> mFileFormats;
  nsTArray<nsString> mWhiteBalanceModes;
  nsTArray<nsString> mSceneModes;
  nsTArray<nsString> mEffects;
  nsTArray<nsString> mFlashModes;
  nsTArray<nsString> mFocusModes;
  nsTArray<nsString> mIsoModes;

  nsTArray<double> mZoomRatios;

  uint32_t mMaxFocusAreas;
  uint32_t mMaxMeteringAreas;
  uint32_t mMaxDetectedFaces;

  double mMinExposureCompensation;
  double mMaxExposureCompensation;
  double mExposureCompensationStep;

  nsRefPtr<RecorderProfileManager> mRecorderProfileManager;
  JS::Heap<JS::Value> mRecorderProfiles;

  nsRefPtr<nsPIDOMWindow> mWindow;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_CameraCapabilities_h__
