/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_CAMERA_ICAMERACONTROL_H
#define DOM_CAMERA_ICAMERACONTROL_H

#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsAutoPtr.h"
#include "nsISupportsImpl.h"

class DeviceStorageFileDescriptor;

class nsIFile;

namespace mozilla {

class CameraControlListener;
class RecorderProfileManager;

// XXXmikeh - In some future patch this should move into the ICameraControl
//  class as well, and the names updated to the 'k'-style;
//  e.g. kParamPreviewSize, etc.
enum {
  // current settings
  CAMERA_PARAM_PREVIEWSIZE,
  CAMERA_PARAM_PREVIEWFORMAT,
  CAMERA_PARAM_PREVIEWFRAMERATE,
  CAMERA_PARAM_VIDEOSIZE,
  CAMERA_PARAM_PICTURE_SIZE,
  CAMERA_PARAM_PICTURE_FILEFORMAT,
  CAMERA_PARAM_PICTURE_ROTATION,
  CAMERA_PARAM_PICTURE_LOCATION,
  CAMERA_PARAM_PICTURE_DATETIME,
  CAMERA_PARAM_EFFECT,
  CAMERA_PARAM_WHITEBALANCE,
  CAMERA_PARAM_SCENEMODE,
  CAMERA_PARAM_FLASHMODE,
  CAMERA_PARAM_FOCUSMODE,
  CAMERA_PARAM_ZOOM,
  CAMERA_PARAM_METERINGAREAS,
  CAMERA_PARAM_FOCUSAREAS,
  CAMERA_PARAM_FOCALLENGTH,
  CAMERA_PARAM_FOCUSDISTANCENEAR,
  CAMERA_PARAM_FOCUSDISTANCEOPTIMUM,
  CAMERA_PARAM_FOCUSDISTANCEFAR,
  CAMERA_PARAM_EXPOSURECOMPENSATION,
  CAMERA_PARAM_PICTURESIZE,
  CAMERA_PARAM_THUMBNAILSIZE,
  CAMERA_PARAM_THUMBNAILQUALITY,
  CAMERA_PARAM_SENSORANGLE,
  CAMERA_PARAM_ISOMODE,

  // supported features
  CAMERA_PARAM_SUPPORTED_PREVIEWSIZES,
  CAMERA_PARAM_SUPPORTED_PICTURESIZES,
  CAMERA_PARAM_SUPPORTED_VIDEOSIZES,
  CAMERA_PARAM_SUPPORTED_PICTUREFORMATS,
  CAMERA_PARAM_SUPPORTED_WHITEBALANCES,
  CAMERA_PARAM_SUPPORTED_SCENEMODES,
  CAMERA_PARAM_SUPPORTED_EFFECTS,
  CAMERA_PARAM_SUPPORTED_FLASHMODES,
  CAMERA_PARAM_SUPPORTED_FOCUSMODES,
  CAMERA_PARAM_SUPPORTED_MAXFOCUSAREAS,
  CAMERA_PARAM_SUPPORTED_MAXMETERINGAREAS,
  CAMERA_PARAM_SUPPORTED_MINEXPOSURECOMPENSATION,
  CAMERA_PARAM_SUPPORTED_MAXEXPOSURECOMPENSATION,
  CAMERA_PARAM_SUPPORTED_EXPOSURECOMPENSATIONSTEP,
  CAMERA_PARAM_SUPPORTED_ZOOM,
  CAMERA_PARAM_SUPPORTED_ZOOMRATIOS,
  CAMERA_PARAM_SUPPORTED_JPEG_THUMBNAIL_SIZES,
  CAMERA_PARAM_SUPPORTED_ISOMODES
};

class ICameraControl
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(ICameraControl)

  static nsresult GetNumberOfCameras(int32_t& aDeviceCount);
  static nsresult GetCameraName(uint32_t aDeviceNum, nsCString& aDeviceName);
  static nsresult GetListOfCameras(nsTArray<nsString>& aList);

  enum Mode {
    kUnspecifiedMode,
    kPictureMode,
    kVideoMode,
  };

  struct Size {
    uint32_t  width;
    uint32_t  height;
  };

  struct Region {
    int32_t   top;
    int32_t   left;
    int32_t   bottom;
    int32_t   right;
    uint32_t  weight;
  };

  struct Position {
    double latitude;
    double longitude;
    double altitude;
    double timestamp;
  };

  struct StartRecordingOptions {
    uint32_t  rotation;
    uint32_t  maxFileSizeBytes;
    uint32_t  maxVideoLengthMs;
  };

  struct Configuration {
    Mode      mMode;
    Size      mPreviewSize;
    nsString  mRecorderProfile;
  };
  static already_AddRefed<ICameraControl> Create(uint32_t aCameraId);

  virtual nsresult Start(const Configuration* aInitialConfig = nullptr) = 0;
  virtual nsresult Stop() = 0;

  virtual nsresult SetConfiguration(const Configuration& aConfig) = 0;

  virtual void AddListener(CameraControlListener* aListener) = 0;
  virtual void RemoveListener(CameraControlListener* aListener) = 0;

  virtual nsresult StartPreview() = 0;
  virtual nsresult StopPreview() = 0;
  virtual nsresult AutoFocus(bool aCancelExistingCall) = 0;
  virtual nsresult TakePicture() = 0;
  virtual nsresult StartRecording(DeviceStorageFileDescriptor *aFileDescriptor,
                                  const StartRecordingOptions* aOptions = nullptr) = 0;
  virtual nsresult StopRecording() = 0;

  virtual nsresult Set(uint32_t aKey, const nsAString& aValue) = 0;
  virtual nsresult Get(uint32_t aKey, nsAString& aValue) = 0;
  virtual nsresult Set(uint32_t aKey, double aValue) = 0;
  virtual nsresult Get(uint32_t aKey, double& aValue) = 0;
  virtual nsresult Set(uint32_t aKey, int32_t aValue) = 0;
  virtual nsresult Get(uint32_t aKey, int32_t& aValue) = 0;
  virtual nsresult Set(uint32_t aKey, int64_t aValue) = 0;
  virtual nsresult Get(uint32_t aKey, int64_t& aValue) = 0;
  virtual nsresult Set(uint32_t aKey, const Size& aValue) = 0;
  virtual nsresult Get(uint32_t aKey, Size& aValue) = 0;
  virtual nsresult Set(uint32_t aKey, const nsTArray<Region>& aRegions) = 0;
  virtual nsresult Get(uint32_t aKey, nsTArray<Region>& aRegions) = 0;

  virtual nsresult SetLocation(const Position& aLocation) = 0;

  virtual nsresult Get(uint32_t aKey, nsTArray<Size>& aSizes) = 0;
  virtual nsresult Get(uint32_t aKey, nsTArray<nsString>& aValues) = 0;
  virtual nsresult Get(uint32_t aKey, nsTArray<double>& aValues) = 0;

  virtual already_AddRefed<RecorderProfileManager> GetRecorderProfileManager() = 0;
  virtual uint32_t GetCameraId() = 0;

  virtual void Shutdown() = 0;

protected:
  virtual ~ICameraControl() { }

  friend class ICameraControlParameterSetAutoEnter;

  virtual void BeginBatchParameterSet() = 0;
  virtual void EndBatchParameterSet() = 0;
};

// Helper class to make it easy to update a batch of camera parameters;
// the parameters are applied atomically when this object goes out of
// scope.
class ICameraControlParameterSetAutoEnter
{
public:
  ICameraControlParameterSetAutoEnter(ICameraControl* aCameraControl)
    : mCameraControl(aCameraControl)
  {
    mCameraControl->BeginBatchParameterSet();
  }
  virtual ~ICameraControlParameterSetAutoEnter()
  {
    mCameraControl->EndBatchParameterSet();
  }

protected:
  nsRefPtr<ICameraControl> mCameraControl;
};

} // namespace mozilla

#endif // DOM_CAMERA_ICAMERACONTROL_H
