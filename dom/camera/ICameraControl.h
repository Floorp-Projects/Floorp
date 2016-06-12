/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_CAMERA_ICAMERACONTROL_H
#define DOM_CAMERA_ICAMERACONTROL_H

#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsISupportsImpl.h"
#include "base/basictypes.h"

struct DeviceStorageFileDescriptor;

namespace mozilla {

class CameraControlListener;

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
  CAMERA_PARAM_PICTURE_QUALITY,
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
  CAMERA_PARAM_THUMBNAILSIZE,
  CAMERA_PARAM_THUMBNAILQUALITY,
  CAMERA_PARAM_SENSORANGLE,
  CAMERA_PARAM_ISOMODE,
  CAMERA_PARAM_LUMINANCE,
  CAMERA_PARAM_SCENEMODE_HDR_RETURNNORMALPICTURE,
  CAMERA_PARAM_RECORDINGHINT,
  CAMERA_PARAM_PREFERRED_PREVIEWSIZE_FOR_VIDEO,
  CAMERA_PARAM_METERINGMODE,

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
  CAMERA_PARAM_SUPPORTED_MAXDETECTEDFACES,
  CAMERA_PARAM_SUPPORTED_JPEG_THUMBNAIL_SIZES,
  CAMERA_PARAM_SUPPORTED_ISOMODES,
  CAMERA_PARAM_SUPPORTED_METERINGMODES
};

class ICameraControl
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(ICameraControl)

  // Returns the number of cameras supported by the system.
  //
  // Return values:
  //  - NS_OK on success;
  //  - NS_ERROR_FAILURE if the camera count cannot be retrieved.
  static nsresult GetNumberOfCameras(int32_t& aDeviceCount);

  // Gets the (possibly-meaningful) name of a particular camera.
  //
  // Return values:
  //  - NS_OK on success;
  //  - NS_ERROR_INVALID_ARG if 'aDeviceNum' is not a valid camera number;
  //  - NS_ERROR_NOT_AVAILABLE if 'aDeviceNum' is valid but the camera name
  //      could still not be retrieved.
  static nsresult GetCameraName(uint32_t aDeviceNum, nsCString& aDeviceName);

  // Returns a list of names of all cameras supported by the system.
  //
  // Return values:
  //  - NS_OK on success, even if no camera names are returned (in which
  //      case 'aList' will be empty);
  //  - NS_ERROR_NOT_AVAILABLE if the list of cameras cannot be retrieved.
  static nsresult GetListOfCameras(nsTArray<nsString>& aList);

  enum Mode {
    kUnspecifiedMode,
    kPictureMode,
    kVideoMode,
  };

  struct Size {
    uint32_t  width;
    uint32_t  height;

    bool Equals(const Size& aSize) const
    {
      return width == aSize.width && height == aSize.height;
    }
  };

  struct Region {
    int32_t   top;
    int32_t   left;
    int32_t   bottom;
    int32_t   right;
    uint32_t  weight;
  };

  struct Position {
    double    latitude;
    double    longitude;
    double    altitude;
    double    timestamp;
  };

  struct StartRecordingOptions {
    uint32_t  rotation;
    uint64_t  maxFileSizeBytes;
    uint64_t  maxVideoLengthMs;
    bool      autoEnableLowLightTorch;
    bool      createPoster;
  };

  struct Configuration {
    Mode      mMode;
    Size      mPreviewSize;
    Size      mPictureSize;
    nsString  mRecorderProfile;
  };

  struct Point {
    int32_t   x;
    int32_t   y;
  };

  struct Face {
    uint32_t  id;
    uint32_t  score;
    Region    bound;
    bool      hasLeftEye;
    Point     leftEye;
    bool      hasRightEye;
    Point     rightEye;
    bool      hasMouth;
    Point     mouth;
  };

  class RecorderProfile
  {
  public:
    class Video
    {
    public:
      Video() { }
      virtual ~Video() { }

      const nsString& GetCodec() const    { return mCodec; }
      const Size& GetSize() const         { return mSize; }
      uint32_t GetBitsPerSecond() const   { return mBitsPerSecond; }
      uint32_t GetFramesPerSecond() const { return mFramesPerSecond; }

    protected:
      nsString  mCodec;
      Size      mSize;
      uint32_t  mBitsPerSecond;
      uint32_t  mFramesPerSecond;

    private:
      DISALLOW_EVIL_CONSTRUCTORS(Video);
    };

    class Audio
    {
    public:
      Audio() { }
      virtual ~Audio() { }

      const nsString& GetCodec() const    { return mCodec; }

      uint32_t GetChannels() const        { return mChannels; }
      uint32_t GetBitsPerSecond() const   { return mBitsPerSecond; }
      uint32_t GetSamplesPerSecond() const { return mSamplesPerSecond; }

    protected:
      nsString  mCodec;
      uint32_t  mChannels;
      uint32_t  mBitsPerSecond;
      uint32_t  mSamplesPerSecond;

    private:
      DISALLOW_EVIL_CONSTRUCTORS(Audio);
    };

    NS_INLINE_DECL_THREADSAFE_REFCOUNTING(RecorderProfile)

    RecorderProfile()
    { }

    const nsString& GetName() const       { return mName; }
    const nsString& GetContainer() const  { return mContainer; }
    const nsString& GetMimeType() const   { return mMimeType; }
    uint32_t GetPriority() const          { return mPriority; }

    virtual const Video& GetVideo() const = 0;
    virtual const Audio& GetAudio() const = 0;

  protected:
    virtual ~RecorderProfile() { }

    nsString    mName;
    nsString    mContainer;
    nsString    mMimeType;
    uint32_t    mPriority;

  private:
    DISALLOW_EVIL_CONSTRUCTORS(RecorderProfile);
  };

  static already_AddRefed<ICameraControl> Create(uint32_t aCameraId);

  virtual void AddListener(CameraControlListener* aListener) = 0;
  virtual void RemoveListener(CameraControlListener* aListener) = 0;

  // Camera control methods.
  //
  // Return values:
  //  - NS_OK on success (if the method requires an asynchronous process,
  //      this value indicates that the process has begun successfully);
  //  - NS_ERROR_INVALID_ARG if one or more arguments is invalid;
  //  - NS_ERROR_FAILURE if an asynchronous method could not be dispatched.
  virtual nsresult Start(const Configuration* aInitialConfig = nullptr) = 0;
  virtual nsresult Stop() = 0;
  virtual nsresult SetConfiguration(const Configuration& aConfig) = 0;
  virtual nsresult StartPreview() = 0;
  virtual nsresult StopPreview() = 0;
  virtual nsresult AutoFocus() = 0;
  virtual nsresult TakePicture() = 0;
  virtual nsresult StartRecording(DeviceStorageFileDescriptor* aFileDescriptor,
                                  const StartRecordingOptions* aOptions = nullptr) = 0;
  virtual nsresult StopRecording() = 0;
  virtual nsresult PauseRecording() = 0;
  virtual nsresult ResumeRecording() = 0;
  virtual nsresult StartFaceDetection() = 0;
  virtual nsresult StopFaceDetection() = 0;
  virtual nsresult ResumeContinuousFocus() = 0;

  // Camera parameter getters and setters. 'aKey' must be one of the
  // CAMERA_PARAM_* values enumerated above.
  //
  // Return values:
  //  - NS_OK on success;
  //  - NS_ERROR_INVALID_ARG if 'aValue' contains an invalid value;
  //  - NS_ERROR_NOT_IMPLEMENTED if 'aKey' is invalid;
  //  - NS_ERROR_NOT_AVAILABLE if the getter fails to retrieve a valid value,
  //      or if a setter fails because it requires one or more values that
  //      could not be retrieved;
  //  - NS_ERROR_FAILURE on unexpected internal failures.
  virtual nsresult Set(uint32_t aKey, const nsAString& aValue) = 0;
  virtual nsresult Get(uint32_t aKey, nsAString& aValue) = 0;
  virtual nsresult Set(uint32_t aKey, double aValue) = 0;
  virtual nsresult Get(uint32_t aKey, double& aValue) = 0;
  virtual nsresult Set(uint32_t aKey, int32_t aValue) = 0;
  virtual nsresult Get(uint32_t aKey, int32_t& aValue) = 0;
  virtual nsresult Set(uint32_t aKey, int64_t aValue) = 0;
  virtual nsresult Get(uint32_t aKey, int64_t& aValue) = 0;
  virtual nsresult Set(uint32_t aKey, bool aValue) = 0;
  virtual nsresult Get(uint32_t aKey, bool& aValue) = 0;
  virtual nsresult Set(uint32_t aKey, const Size& aValue) = 0;
  virtual nsresult Get(uint32_t aKey, Size& aValue) = 0;
  virtual nsresult Set(uint32_t aKey, const nsTArray<Region>& aRegions) = 0;
  virtual nsresult Get(uint32_t aKey, nsTArray<Region>& aRegions) = 0;

  virtual nsresult SetLocation(const Position& aLocation) = 0;

  virtual nsresult Get(uint32_t aKey, nsTArray<Size>& aSizes) = 0;
  virtual nsresult Get(uint32_t aKey, nsTArray<nsString>& aValues) = 0;
  virtual nsresult Get(uint32_t aKey, nsTArray<double>& aValues) = 0;

  virtual nsresult GetRecorderProfiles(nsTArray<nsString>& aProfiles) = 0;
  virtual RecorderProfile* GetProfileInfo(const nsAString& aProfile) = 0;

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
  explicit ICameraControlParameterSetAutoEnter(ICameraControl* aCameraControl)
    : mCameraControl(aCameraControl)
  {
    mCameraControl->BeginBatchParameterSet();
  }
  virtual ~ICameraControlParameterSetAutoEnter()
  {
    mCameraControl->EndBatchParameterSet();
  }

protected:
  RefPtr<ICameraControl> mCameraControl;
};

} // namespace mozilla

#endif // DOM_CAMERA_ICAMERACONTROL_H
