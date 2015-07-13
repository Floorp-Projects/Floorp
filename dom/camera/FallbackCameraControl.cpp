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
  } // namespace layers
} // namespace mozilla

/**
 * Fallback camera control subclass. Can be used as a template for the
 * definition of new camera support classes.
 */
class FallbackCameraControl : public CameraControlImpl
{
public:
  explicit FallbackCameraControl() : CameraControlImpl() { }

  virtual nsresult Set(uint32_t aKey, const nsAString& aValue) override { return NS_ERROR_NOT_IMPLEMENTED; }
  virtual nsresult Get(uint32_t aKey, nsAString& aValue) override { return NS_ERROR_NOT_IMPLEMENTED; }
  virtual nsresult Set(uint32_t aKey, double aValue) override { return NS_ERROR_NOT_IMPLEMENTED; }
  virtual nsresult Get(uint32_t aKey, double& aValue) override { return NS_ERROR_NOT_IMPLEMENTED; }
  virtual nsresult Set(uint32_t aKey, int32_t aValue) override { return NS_ERROR_NOT_IMPLEMENTED; }
  virtual nsresult Get(uint32_t aKey, int32_t& aValue) override { return NS_ERROR_NOT_IMPLEMENTED; }
  virtual nsresult Set(uint32_t aKey, int64_t aValue) override { return NS_ERROR_NOT_IMPLEMENTED; }
  virtual nsresult Get(uint32_t aKey, int64_t& aValue) override { return NS_ERROR_NOT_IMPLEMENTED; }
  virtual nsresult Set(uint32_t aKey, bool aValue) override { return NS_ERROR_NOT_IMPLEMENTED; }
  virtual nsresult Get(uint32_t aKey, bool& aValue) override { return NS_ERROR_NOT_IMPLEMENTED; }
  virtual nsresult Set(uint32_t aKey, const Size& aValue) override { return NS_ERROR_NOT_IMPLEMENTED; }
  virtual nsresult Get(uint32_t aKey, Size& aValue) override { return NS_ERROR_NOT_IMPLEMENTED; }
  virtual nsresult Set(uint32_t aKey, const nsTArray<Region>& aRegions) override { return NS_ERROR_NOT_IMPLEMENTED; }
  virtual nsresult Get(uint32_t aKey, nsTArray<Region>& aRegions) override { return NS_ERROR_NOT_IMPLEMENTED; }

  virtual nsresult SetLocation(const Position& aLocation) override { return NS_ERROR_NOT_IMPLEMENTED; }

  virtual nsresult Get(uint32_t aKey, nsTArray<Size>& aSizes) override { return NS_ERROR_NOT_IMPLEMENTED; }
  virtual nsresult Get(uint32_t aKey, nsTArray<nsString>& aValues) override { return NS_ERROR_NOT_IMPLEMENTED; }
  virtual nsresult Get(uint32_t aKey, nsTArray<double>& aValues) override { return NS_ERROR_NOT_IMPLEMENTED; }

  virtual nsresult GetRecorderProfiles(nsTArray<nsString>& aProfiles) override { return NS_ERROR_NOT_IMPLEMENTED; }
  virtual RecorderProfile* GetProfileInfo(const nsAString& aProfile) override { return nullptr; }

  nsresult PushParameters() { return NS_ERROR_NOT_INITIALIZED; }
  nsresult PullParameters() { return NS_ERROR_NOT_INITIALIZED; }

protected:
  ~FallbackCameraControl();

  virtual nsresult StartPreviewImpl() override { return NS_ERROR_NOT_INITIALIZED; }
  virtual nsresult StopPreviewImpl() override { return NS_ERROR_NOT_INITIALIZED; }
  virtual nsresult AutoFocusImpl() override { return NS_ERROR_NOT_INITIALIZED; }
  virtual nsresult StartFaceDetectionImpl() override { return NS_ERROR_NOT_INITIALIZED; }
  virtual nsresult StopFaceDetectionImpl() override { return NS_ERROR_NOT_INITIALIZED; }
  virtual nsresult TakePictureImpl() override { return NS_ERROR_NOT_INITIALIZED; }
  virtual nsresult StartRecordingImpl(DeviceStorageFileDescriptor* aFileDescriptor,
                                      const StartRecordingOptions* aOptions = nullptr) override
                                        { return NS_ERROR_NOT_INITIALIZED; }
  virtual nsresult StopRecordingImpl() override { return NS_ERROR_NOT_INITIALIZED; }
  virtual nsresult PushParametersImpl() override { return NS_ERROR_NOT_INITIALIZED; }
  virtual nsresult PullParametersImpl() override { return NS_ERROR_NOT_INITIALIZED; }

private:
  FallbackCameraControl(const FallbackCameraControl&) = delete;
  FallbackCameraControl& operator=(const FallbackCameraControl&) = delete;
};
