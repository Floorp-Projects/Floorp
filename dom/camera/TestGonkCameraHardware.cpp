/*
 * Copyright (C) 2013-2014 Mozilla Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "TestGonkCameraHardware.h"

#include "CameraPreferences.h"
#include "nsThreadUtils.h"

using namespace android;
using namespace mozilla;

TestGonkCameraHardware::TestGonkCameraHardware(nsGonkCameraControl* aTarget,
                                               uint32_t aCameraId,
                                               const sp<Camera>& aCamera)
  : GonkCameraHardware(aTarget, aCameraId, aCamera)
{
  DOM_CAMERA_LOGA("v===== Created TestGonkCameraHardware =====v\n");
  DOM_CAMERA_LOGT("%s:%d : this=%p (aTarget=%p)\n",
    __func__, __LINE__, this, aTarget);
  MOZ_COUNT_CTOR(TestGonkCameraHardware);
}

TestGonkCameraHardware::~TestGonkCameraHardware()
{
  MOZ_COUNT_DTOR(TestGonkCameraHardware);
  DOM_CAMERA_LOGA("^===== Destroyed TestGonkCameraHardware =====^\n");
}

nsresult
TestGonkCameraHardware::Init()
{
  if (IsTestCase("init-failure")) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  return GonkCameraHardware::Init();
}

const nsCString
TestGonkCameraHardware::TestCase()
{
  nsCString test;
  CameraPreferences::GetPref("camera.control.test.hardware", test);
  return test;
}

const nsCString
TestGonkCameraHardware::GetExtraParameters()
{
  /**
   * The contents of this pref are appended to the flattened string of
   * parameters stuffed into GonkCameraParameters by the camera library.
   * It consists of semicolon-delimited key=value pairs, e.g.
   *
   *   focus-mode=auto;flash-mode=auto;preview-size=1024x768
   *
   * The unflattening process breaks this string up on semicolon boundaries
   * and sets an entry in a hashtable of strings with the token before
   * the equals sign as the key, and the token after as the value. Because
   * the string is parsed in order, key=value pairs occuring later in the
   * string will replace value pairs appearing earlier, making it easy to
   * inject fake, testable values into the parameters table.
   *
   * One constraint of this approach is that neither the key nor the value
   * may contain equals signs or semicolons. We don't enforce that here
   * so that we can also test correct handling of improperly-formatted values.
   */
  nsCString parameters;
  CameraPreferences::GetPref("camera.control.test.hardware.gonk.parameters", parameters);
  DOM_CAMERA_LOGA("TestGonkCameraHardware : extra-parameters '%s'\n",
    parameters.get());
  return parameters;
}

bool
TestGonkCameraHardware::IsTestCaseInternal(const char* aTest, const char* aFile, int aLine)
{
  if (TestCase().EqualsASCII(aTest)) {
    DOM_CAMERA_LOGA("TestGonkCameraHardware : test-case '%s' (%s:%d)\n",
      aTest, aFile, aLine);
    return true;
  }

  return false;
}

int
TestGonkCameraHardware::TestCaseError(int aDefaultError)
{
  // for now, just return the default error
  return aDefaultError;
}

int
TestGonkCameraHardware::AutoFocus()
{
  class AutoFocusFailure : public nsRunnable
  {
  public:
    AutoFocusFailure(nsGonkCameraControl* aTarget)
      : mTarget(aTarget)
    { }

    NS_IMETHODIMP
    Run()
    {
      OnAutoFocusComplete(mTarget, false);
      return NS_OK;
    }

  protected:
    nsGonkCameraControl* mTarget;
  };

  if (IsTestCase("auto-focus-failure")) {
    return TestCaseError(UNKNOWN_ERROR);
  }
  if (IsTestCase("auto-focus-process-failure")) {
    nsresult rv = NS_DispatchToCurrentThread(new AutoFocusFailure(mTarget));
    if (NS_SUCCEEDED(rv)) {
      return OK;
    }
    DOM_CAMERA_LOGE("Failed to dispatch AutoFocusFailure runnable (0x%08x)\n", rv);
    return UNKNOWN_ERROR;
  }

  return GonkCameraHardware::AutoFocus();
}

// These classes have to be external to StartFaceDetection(), at least
// until we pick up gcc 4.5, which supports local classes as template
// arguments.
class FaceDetected : public nsRunnable
{
public:
  FaceDetected(nsGonkCameraControl* aTarget)
    : mTarget(aTarget)
  { }

  ~FaceDetected()
  {
    ReleaseFacesArray();
  }

  NS_IMETHODIMP
  Run()
  {
    InitMetaData();
    OnFacesDetected(mTarget, &mMetaData);
    return NS_OK;
  }

protected:
  virtual nsresult InitMetaData() = 0;

  nsresult
  AllocateFacesArray(uint32_t num)
  {
    mMetaData.faces = new camera_face_t[num];
    return NS_OK;
  }

  nsresult
  ReleaseFacesArray()
  {
    delete [] mMetaData.faces;
    mMetaData.faces = nullptr;
    return NS_OK;
  }

  nsRefPtr<nsGonkCameraControl> mTarget;
  camera_frame_metadata_t mMetaData;
};

class OneFaceDetected : public FaceDetected
{
public:
  OneFaceDetected(nsGonkCameraControl* aTarget)
    : FaceDetected(aTarget)
  { }

  nsresult
  InitMetaData() MOZ_OVERRIDE
  {
    mMetaData.number_of_faces = 1;
    AllocateFacesArray(1);
    mMetaData.faces[0].id = 1;
    mMetaData.faces[0].score = 2;
    mMetaData.faces[0].rect[0] = 3;
    mMetaData.faces[0].rect[1] = 4;
    mMetaData.faces[0].rect[2] = 5;
    mMetaData.faces[0].rect[3] = 6;
    mMetaData.faces[0].left_eye[0] = 7;
    mMetaData.faces[0].left_eye[1] = 8;
    mMetaData.faces[0].right_eye[0] = 9;
    mMetaData.faces[0].right_eye[1] = 10;
    mMetaData.faces[0].mouth[0] = 11;
    mMetaData.faces[0].mouth[1] = 12;

    return NS_OK;
  }
};

class TwoFacesDetected : public FaceDetected
{
public:
  TwoFacesDetected(nsGonkCameraControl* aTarget)
    : FaceDetected(aTarget)
  { }

  nsresult
  InitMetaData() MOZ_OVERRIDE
  {
    mMetaData.number_of_faces = 2;
    AllocateFacesArray(2);
    mMetaData.faces[0].id = 1;
    mMetaData.faces[0].score = 2;
    mMetaData.faces[0].rect[0] = 3;
    mMetaData.faces[0].rect[1] = 4;
    mMetaData.faces[0].rect[2] = 5;
    mMetaData.faces[0].rect[3] = 6;
    mMetaData.faces[0].left_eye[0] = 7;
    mMetaData.faces[0].left_eye[1] = 8;
    mMetaData.faces[0].right_eye[0] = 9;
    mMetaData.faces[0].right_eye[1] = 10;
    mMetaData.faces[0].mouth[0] = 11;
    mMetaData.faces[0].mouth[1] = 12;
    mMetaData.faces[1].id = 13;
    mMetaData.faces[1].score = 14;
    mMetaData.faces[1].rect[0] = 15;
    mMetaData.faces[1].rect[1] = 16;
    mMetaData.faces[1].rect[2] = 17;
    mMetaData.faces[1].rect[3] = 18;
    mMetaData.faces[1].left_eye[0] = 19;
    mMetaData.faces[1].left_eye[1] = 20;
    mMetaData.faces[1].right_eye[0] = 21;
    mMetaData.faces[1].right_eye[1] = 22;
    mMetaData.faces[1].mouth[0] = 23;
    mMetaData.faces[1].mouth[1] = 24;

    return NS_OK;
  }
};

class OneFaceNoFeaturesDetected : public FaceDetected
{
public:
  OneFaceNoFeaturesDetected(nsGonkCameraControl* aTarget)
    : FaceDetected(aTarget)
  { }

  nsresult
  InitMetaData() MOZ_OVERRIDE
  {
    mMetaData.number_of_faces = 1;
    AllocateFacesArray(1);
    mMetaData.faces[0].id = 1;
    // Test clamping 'score' to 100.
    mMetaData.faces[0].score = 1000;
    mMetaData.faces[0].rect[0] = 3;
    mMetaData.faces[0].rect[1] = 4;
    mMetaData.faces[0].rect[2] = 5;
    mMetaData.faces[0].rect[3] = 6;
    // Nullable values set to 'not-supported' specific values
    mMetaData.faces[0].left_eye[0] = -2000;
    mMetaData.faces[0].left_eye[1] = -2000;
    // Test other 'not-supported' values as well. We treat
    // anything outside the range [-1000, 1000] as invalid.
    mMetaData.faces[0].right_eye[0] = 1001;
    mMetaData.faces[0].right_eye[1] = -1001;
    mMetaData.faces[0].mouth[0] = -2000;
    mMetaData.faces[0].mouth[1] = 2000;

    return NS_OK;
  }
};

class NoFacesDetected : public FaceDetected
{
public:
  NoFacesDetected(nsGonkCameraControl* aTarget)
    : FaceDetected(aTarget)
  { }

  nsresult
  InitMetaData() MOZ_OVERRIDE
  {
    mMetaData.number_of_faces = 0;
    mMetaData.faces = nullptr;

    return NS_OK;
  }
};

int
TestGonkCameraHardware::StartFaceDetection()
{
  nsRefPtr<FaceDetected> faceDetected;

  if (IsTestCase("face-detection-detected-one-face")) {
    faceDetected = new OneFaceDetected(mTarget);
  } else if (IsTestCase("face-detection-detected-two-faces")) {
    faceDetected = new TwoFacesDetected(mTarget);
  } else if (IsTestCase("face-detection-detected-one-face-no-features")) {
    faceDetected = new OneFaceNoFeaturesDetected(mTarget);
  } else if (IsTestCase("face-detection-no-faces-detected")) {
    faceDetected = new NoFacesDetected(mTarget);
  }

  if (!faceDetected) {
    return GonkCameraHardware::StartFaceDetection();
  }

  nsresult rv = NS_DispatchToCurrentThread(faceDetected);
  if (NS_FAILED(rv)) {
    DOM_CAMERA_LOGE("Failed to dispatch FaceDetected runnable (0x%08x)\n", rv);
    return UNKNOWN_ERROR;
  }

  return OK;
}

int
TestGonkCameraHardware::StopFaceDetection()
{
  if (IsTestCase("face-detection-detected-one-face") ||
      IsTestCase("face-detection-detected-two-faces") ||
      IsTestCase("face-detection-detected-one-face-no-features") ||
      IsTestCase("face-detection-no-faces-detected"))
  {
    return OK;
  }

  return GonkCameraHardware::StopFaceDetection();
}

int
TestGonkCameraHardware::TakePicture()
{
  class TakePictureFailure : public nsRunnable
  {
  public:
    TakePictureFailure(nsGonkCameraControl* aTarget)
      : mTarget(aTarget)
    { }

    NS_IMETHODIMP
    Run()
    {
      OnTakePictureError(mTarget);
      return NS_OK;
    }

  protected:
    nsGonkCameraControl* mTarget;
  };

  if (IsTestCase("take-picture-failure")) {
    return TestCaseError(UNKNOWN_ERROR);
  }
  if (IsTestCase("take-picture-process-failure")) {
    nsresult rv = NS_DispatchToCurrentThread(new TakePictureFailure(mTarget));
    if (NS_SUCCEEDED(rv)) {
      return OK;
    }
    DOM_CAMERA_LOGE("Failed to dispatch TakePictureFailure runnable (0x%08x)\n", rv);
    return UNKNOWN_ERROR;
  }

  return GonkCameraHardware::TakePicture();
}

int
TestGonkCameraHardware::StartPreview()
{
  if (IsTestCase("start-preview-failure")) {
    return TestCaseError(UNKNOWN_ERROR);
  }

  return GonkCameraHardware::StartPreview();
}

int
TestGonkCameraHardware::StartAutoFocusMoving(bool aIsMoving)
{
  class AutoFocusMoving : public nsRunnable
  {
  public:
    AutoFocusMoving(nsGonkCameraControl* aTarget, bool aIsMoving)
      : mTarget(aTarget)
      , mIsMoving(aIsMoving)
    { }

    NS_IMETHODIMP
    Run()
    {
      OnAutoFocusMoving(mTarget, mIsMoving);
      return NS_OK;
    }

  protected:
    nsGonkCameraControl* mTarget;
    bool mIsMoving;
  };

  nsresult rv = NS_DispatchToCurrentThread(new AutoFocusMoving(mTarget, aIsMoving));
  if (NS_SUCCEEDED(rv)) {
    return OK;
  }
  DOM_CAMERA_LOGE("Failed to dispatch AutoFocusMoving runnable (0x%08x)\n", rv);
  return UNKNOWN_ERROR;
}

int
TestGonkCameraHardware::PushParameters(const GonkCameraParameters& aParams)
{
  if (IsTestCase("push-parameters-failure")) {
    return TestCaseError(UNKNOWN_ERROR);
  }

  nsString focusMode;
  GonkCameraParameters& params = const_cast<GonkCameraParameters&>(aParams);
  params.Get(CAMERA_PARAM_FOCUSMODE, focusMode);
  if (focusMode.EqualsASCII("continuous-picture") ||
      focusMode.EqualsASCII("continuous-video"))
  {
    if (IsTestCase("autofocus-moving-true")) {
      return StartAutoFocusMoving(true);
    } else if (IsTestCase("autofocus-moving-false")) {
      return StartAutoFocusMoving(false);
    }
  }

  return GonkCameraHardware::PushParameters(aParams);
}

nsresult
TestGonkCameraHardware::PullParameters(GonkCameraParameters& aParams)
{
  if (IsTestCase("pull-parameters-failure")) {
    return static_cast<nsresult>(TestCaseError(UNKNOWN_ERROR));
  }

  String8 s = mCamera->getParameters();
  nsCString extra = GetExtraParameters();
  if (!extra.IsEmpty()) {
    s += ";";
    s += extra.get();
  }

  return aParams.Unflatten(s);
}

int
TestGonkCameraHardware::StartRecording()
{
  if (IsTestCase("start-recording-failure")) {
    return TestCaseError(UNKNOWN_ERROR);
  }

  return GonkCameraHardware::StartRecording();
}

int
TestGonkCameraHardware::StopRecording()
{
  if (IsTestCase("stop-recording-failure")) {
    return TestCaseError(UNKNOWN_ERROR);
  }

  return GonkCameraHardware::StopRecording();
}

int
TestGonkCameraHardware::SetListener(const sp<GonkCameraListener>& aListener)
{
  if (IsTestCase("set-listener-failure")) {
    return TestCaseError(UNKNOWN_ERROR);
  }

  return GonkCameraHardware::SetListener(aListener);
}

int
TestGonkCameraHardware::StoreMetaDataInBuffers(bool aEnabled)
{
  if (IsTestCase("store-metadata-in-buffers-failure")) {
    return TestCaseError(UNKNOWN_ERROR);
  }

  return GonkCameraHardware::StoreMetaDataInBuffers(aEnabled);
}
