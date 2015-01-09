/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CameraControlImpl.h"

using namespace mozilla;
using namespace mozilla::dom;

namespace mozilla {
  class RecorderProfileManager;

  namespace layers {
    class GraphicBufferLocked;
  }
}

/**
 * Fallback camera control subclass. Can be used as a template for the
 * definition of new camera support classes.
 */
class FallbackCameraControl : public CameraControlImpl
{
public:
  explicit FallbackCameraControl() : CameraControlImpl() { }

  virtual nsresult Set(uint32_t aKey, const nsAString& aValue) MOZ_OVERRIDE { return NS_ERROR_NOT_IMPLEMENTED; }
  virtual nsresult Get(uint32_t aKey, nsAString& aValue) MOZ_OVERRIDE { return NS_ERROR_NOT_IMPLEMENTED; }
  virtual nsresult Set(uint32_t aKey, double aValue) MOZ_OVERRIDE { return NS_ERROR_NOT_IMPLEMENTED; }
  virtual nsresult Get(uint32_t aKey, double& aValue) MOZ_OVERRIDE { return NS_ERROR_NOT_IMPLEMENTED; }
  virtual nsresult Set(uint32_t aKey, int32_t aValue) MOZ_OVERRIDE { return NS_ERROR_NOT_IMPLEMENTED; }
  virtual nsresult Get(uint32_t aKey, int32_t& aValue) MOZ_OVERRIDE { return NS_ERROR_NOT_IMPLEMENTED; }
  virtual nsresult Set(uint32_t aKey, int64_t aValue) MOZ_OVERRIDE { return NS_ERROR_NOT_IMPLEMENTED; }
  virtual nsresult Get(uint32_t aKey, int64_t& aValue) MOZ_OVERRIDE { return NS_ERROR_NOT_IMPLEMENTED; }
  virtual nsresult Set(uint32_t aKey, bool aValue) MOZ_OVERRIDE { return NS_ERROR_NOT_IMPLEMENTED; }
  virtual nsresult Get(uint32_t aKey, bool& aValue) MOZ_OVERRIDE { return NS_ERROR_NOT_IMPLEMENTED; }
  virtual nsresult Set(uint32_t aKey, const Size& aValue) MOZ_OVERRIDE { return NS_ERROR_NOT_IMPLEMENTED; }
  virtual nsresult Get(uint32_t aKey, Size& aValue) MOZ_OVERRIDE { return NS_ERROR_NOT_IMPLEMENTED; }
  virtual nsresult Set(uint32_t aKey, const nsTArray<Region>& aRegions) MOZ_OVERRIDE { return NS_ERROR_NOT_IMPLEMENTED; }
  virtual nsresult Get(uint32_t aKey, nsTArray<Region>& aRegions) MOZ_OVERRIDE { return NS_ERROR_NOT_IMPLEMENTED; }

  virtual nsresult SetLocation(const Position& aLocation) MOZ_OVERRIDE { return NS_ERROR_NOT_IMPLEMENTED; }

  virtual nsresult Get(uint32_t aKey, nsTArray<Size>& aSizes) MOZ_OVERRIDE { return NS_ERROR_NOT_IMPLEMENTED; }
  virtual nsresult Get(uint32_t aKey, nsTArray<nsString>& aValues) MOZ_OVERRIDE { return NS_ERROR_NOT_IMPLEMENTED; }
  virtual nsresult Get(uint32_t aKey, nsTArray<double>& aValues) MOZ_OVERRIDE { return NS_ERROR_NOT_IMPLEMENTED; }

  virtual nsresult GetRecorderProfiles(nsTArray<nsString>& aProfiles) MOZ_OVERRIDE { return NS_ERROR_NOT_IMPLEMENTED; }
  virtual RecorderProfile* GetProfileInfo(const nsAString& aProfile) MOZ_OVERRIDE { return nullptr; }

  nsresult PushParameters() { return NS_ERROR_NOT_INITIALIZED; }
  nsresult PullParameters() { return NS_ERROR_NOT_INITIALIZED; }

protected:
  ~FallbackCameraControl();

  virtual nsresult StartPreviewImpl() MOZ_OVERRIDE { return NS_ERROR_NOT_INITIALIZED; }
  virtual nsresult StopPreviewImpl() MOZ_OVERRIDE { return NS_ERROR_NOT_INITIALIZED; }
  virtual nsresult AutoFocusImpl() MOZ_OVERRIDE { return NS_ERROR_NOT_INITIALIZED; }
  virtual nsresult StartFaceDetectionImpl() MOZ_OVERRIDE { return NS_ERROR_NOT_INITIALIZED; }
  virtual nsresult StopFaceDetectionImpl() MOZ_OVERRIDE { return NS_ERROR_NOT_INITIALIZED; }
  virtual nsresult TakePictureImpl() MOZ_OVERRIDE { return NS_ERROR_NOT_INITIALIZED; }
  virtual nsresult StartRecordingImpl(DeviceStorageFileDescriptor* aFileDescriptor,
                                      const StartRecordingOptions* aOptions = nullptr) MOZ_OVERRIDE
                                        { return NS_ERROR_NOT_INITIALIZED; }
  virtual nsresult StopRecordingImpl() MOZ_OVERRIDE { return NS_ERROR_NOT_INITIALIZED; }
  virtual nsresult PushParametersImpl() MOZ_OVERRIDE { return NS_ERROR_NOT_INITIALIZED; }
  virtual nsresult PullParametersImpl() MOZ_OVERRIDE { return NS_ERROR_NOT_INITIALIZED; }

private:
  FallbackCameraControl(const FallbackCameraControl&) = delete;
  FallbackCameraControl& operator=(const FallbackCameraControl&) = delete;
};
