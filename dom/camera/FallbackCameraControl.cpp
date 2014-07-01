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
  FallbackCameraControl(uint32_t aCameraId) : CameraControlImpl(aCameraId) { }

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

  nsresult PushParameters() { return NS_ERROR_NOT_INITIALIZED; }
  nsresult PullParameters() { return NS_ERROR_NOT_INITIALIZED; }

protected:
  ~FallbackCameraControl();

  virtual nsresult StartPreviewImpl() { return NS_ERROR_NOT_INITIALIZED; }
  virtual nsresult StopPreviewImpl() { return NS_ERROR_NOT_INITIALIZED; }
  virtual nsresult AutoFocusImpl() { return NS_ERROR_NOT_INITIALIZED; }
  virtual nsresult StartFaceDetectionImpl() { return NS_ERROR_NOT_INITIALIZED; }
  virtual nsresult StopFaceDetectionImpl() { return NS_ERROR_NOT_INITIALIZED; }
  virtual nsresult TakePictureImpl() { return NS_ERROR_NOT_INITIALIZED; }
  virtual nsresult StartRecordingImpl(DeviceStorageFileDescriptor* aFileDescriptor,
                                      const StartRecordingOptions* aOptions = nullptr) MOZ_OVERRIDE
                                        { return NS_ERROR_NOT_INITIALIZED; }
  virtual nsresult StopRecordingImpl() { return NS_ERROR_NOT_INITIALIZED; }
  virtual nsresult PushParametersImpl() { return NS_ERROR_NOT_INITIALIZED; }
  virtual nsresult PullParametersImpl() { return NS_ERROR_NOT_INITIALIZED; }
  virtual already_AddRefed<RecorderProfileManager> GetRecorderProfileManagerImpl() MOZ_OVERRIDE { return nullptr; }

private:
  FallbackCameraControl(const FallbackCameraControl&) MOZ_DELETE;
  FallbackCameraControl& operator=(const FallbackCameraControl&) MOZ_DELETE;
};
