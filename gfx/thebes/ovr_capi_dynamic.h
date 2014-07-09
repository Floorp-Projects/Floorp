/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* This file contains just the needed struct definitions for
 * interacting with the Oculus VR C API, without needing to #include
 * OVR_CAPI.h directly.  Note that it uses the same type names as the
 * CAPI, and cannot be #included at the same time as OVR_CAPI.h.  It
 * does not include the entire C API, just want's needed.
 */

#ifdef OVR_CAPI_h
#warning OVR_CAPI.h included before ovr_capi_dynamic.h, skpping this
#define mozilla_ovr_capi_dynamic_h_
#endif

#ifndef mozilla_ovr_capi_dynamic_h_
#define mozilla_ovr_capi_dynamic_h_

#define OVR_CAPI_LIMITED_MOZILLA 1

#ifdef __cplusplus 
extern "C" {
#endif

typedef char ovrBool;
typedef struct { int x, y; } ovrVector2i;
typedef struct { int w, h; } ovrSizei;
typedef struct { ovrVector2i Pos; ovrSizei Size; } ovrRecti;
typedef struct { float x, y, z, w; } ovrQuatf;
typedef struct { float x, y; } ovrVector2f;
typedef struct { float x, y, z; } ovrVector3f;
typedef struct { float M[4][4]; } ovrMatrix4f;

typedef struct {
  ovrQuatf Orientation;
  ovrVector3f Position;
} ovrPosef;

typedef struct {
  ovrPosef ThePose;
  ovrVector3f AngularVelocity;
  ovrVector3f LinearVelocity;
  ovrVector3f AngularAcceleration;
  ovrVector3f LinearAcceleration;
  double TimeInSeconds;
} ovrPoseStatef;

typedef struct {
  float UpTan;
  float DownTan;
  float LeftTan;
  float RightTan;
} ovrFovPort;

typedef enum {
  ovrHmd_None             = 0,    
  ovrHmd_DK1              = 3,
  ovrHmd_DKHD             = 4,
  ovrHmd_DK2              = 6,
  ovrHmd_Other
} ovrHmdType;

typedef enum {
  ovrHmdCap_Present           = 0x0001,
  ovrHmdCap_Available         = 0x0002,
  ovrHmdCap_Captured          = 0x0004,
  ovrHmdCap_ExtendDesktop     = 0x0008,
  ovrHmdCap_DisplayOff        = 0x0040,
  ovrHmdCap_LowPersistence    = 0x0080,
  ovrHmdCap_DynamicPrediction = 0x0200,
  ovrHmdCap_NoVSync           = 0x1000,
  ovrHmdCap_NoMirrorToWindow  = 0x2000
} ovrHmdCapBits;

typedef enum
{
  ovrTrackingCap_Orientation      = 0x0010,
  ovrTrackingCap_MagYawCorrection = 0x0020,
  ovrTrackingCap_Position         = 0x0040,
  ovrTrackingCap_Idle             = 0x0100
} ovrTrackingCaps;

typedef enum {
  ovrDistortionCap_Chromatic = 0x01,
  ovrDistortionCap_TimeWarp  = 0x02,
  ovrDistortionCap_Vignette  = 0x08,
  ovrDistortionCap_NoRestore = 0x10,
  ovrDistortionCap_FlipInput = 0x20,
  ovrDistortionCap_SRGB      = 0x40,
  ovrDistortionCap_Overdrive = 0x80,
  ovrDistortionCap_ProfileNoTimewarpSpinWaits = 0x10000
} ovrDistortionCaps;

typedef enum {
  ovrEye_Left  = 0,
  ovrEye_Right = 1,
  ovrEye_Count = 2
} ovrEyeType;

typedef struct ovrHmdDesc_ {
  void* Handle;
  ovrHmdType  Type;
  const char* ProductName;    
  const char* Manufacturer;
  short VendorId;
  short ProductId;
  char SerialNumber[24];
  short FirmwareMajor;
  short FirmwareMinor;
  float CameraFrustumHFovInRadians;
  float CameraFrustumVFovInRadians;
  float CameraFrustumNearZInMeters;
  float CameraFrustumFarZInMeters;

  unsigned int HmdCaps;
  unsigned int TrackingCaps;
  unsigned int DistortionCaps;

  ovrFovPort  DefaultEyeFov[ovrEye_Count];
  ovrFovPort  MaxEyeFov[ovrEye_Count];
  ovrEyeType  EyeRenderOrder[ovrEye_Count];

  ovrSizei    Resolution;
  ovrVector2i WindowsPos;

  const char* DisplayDeviceName;
  int         DisplayId;
} ovrHmdDesc;

typedef const ovrHmdDesc* ovrHmd;

typedef enum {
  ovrStatus_OrientationTracked    = 0x0001,
  ovrStatus_PositionTracked       = 0x0002,
  ovrStatus_CameraPoseTracked     = 0x0004,
  ovrStatus_PositionConnected     = 0x0020,
  ovrStatus_HmdConnected          = 0x0080
} ovrStatusBits;

typedef struct ovrSensorData_ {
  ovrVector3f    Accelerometer;
  ovrVector3f    Gyro;
  ovrVector3f    Magnetometer;
  float          Temperature;
  float          TimeInSeconds;
} ovrSensorData;


typedef struct ovrTrackingState_ {
  ovrPoseStatef HeadPose;
  ovrPosef CameraPose;
  ovrPosef LeveledCameraPose;
  ovrSensorData RawSensorData;
  unsigned int StatusFlags;
  double LastVisionProcessingTime;
  double LastVisionFrameLatency;
  uint32_t LastCameraFrameCounter;
} ovrTrackingState;

typedef struct ovrFrameTiming_ {
  float DeltaSeconds;
  double ThisFrameSeconds;
  double TimewarpPointSeconds;
  double NextFrameSeconds;
  double ScanoutMidpointSeconds;
  double EyeScanoutSeconds[2];    
} ovrFrameTiming;

typedef struct ovrEyeRenderDesc_ {
  ovrEyeType  Eye;        
  ovrFovPort  Fov;
  ovrRecti DistortedViewport;
  ovrVector2f PixelsPerTanAngleAtCenter;
  ovrVector3f ViewAdjust;
} ovrEyeRenderDesc;

typedef struct ovrDistortionVertex_ {
  ovrVector2f ScreenPosNDC;
  float       TimeWarpFactor;
  float       VignetteFactor;
  ovrVector2f TanEyeAnglesR;
  ovrVector2f TanEyeAnglesG;
  ovrVector2f TanEyeAnglesB;    
} ovrDistortionVertex;

typedef struct ovrDistortionMesh_ {
  ovrDistortionVertex* pVertexData;
  unsigned short*      pIndexData;
  unsigned int         VertexCount;
  unsigned int         IndexCount;
} ovrDistortionMesh;

typedef ovrBool (*pfn_ovr_Initialize)();
typedef void (*pfn_ovr_Shutdown)();
typedef int (*pfn_ovrHmd_Detect)();
typedef ovrHmd (*pfn_ovrHmd_Create)(int index);
typedef void (*pfn_ovrHmd_Destroy)(ovrHmd hmd);
typedef ovrHmd (*pfn_ovrHmd_CreateDebug)(ovrHmdType type);
typedef const char* (*pfn_ovrHmd_GetLastError)(ovrHmd hmd);
typedef ovrBool (*pfn_ovrHmd_AttachToWindow)(ovrHmd hmd, void* window, const ovrRecti* destMirrorRect, const ovrRecti* sourceRenderTargetRect);
typedef unsigned int (*pfn_ovrHmd_GetEnabledCaps)(ovrHmd hmd);
typedef void (*pfn_ovrHmd_SetEnabledCaps)(ovrHmd hmd, unsigned int hmdCaps);
typedef ovrBool (*pfn_ovrHmd_ConfigureTracking)(ovrHmd hmd, unsigned int supportedTrackingCaps, unsigned int requiredTrackingCaps); 
typedef void (*pfn_ovrHmd_RecenterPose)(ovrHmd hmd);
typedef ovrTrackingState (*pfn_ovrHmd_GetTrackingState)(ovrHmd hmd, double absTime);
typedef ovrSizei (*pfn_ovrHmd_GetFovTextureSize)(ovrHmd hmd, ovrEyeType eye, ovrFovPort fov, float pixelsPerDisplayPixel);
typedef ovrEyeRenderDesc (*pfn_ovrHmd_GetRenderDesc)(ovrHmd hmd, ovrEyeType eyeType, ovrFovPort fov);
typedef ovrBool (*pfn_ovrHmd_CreateDistortionMesh)(ovrHmd hmd, ovrEyeType eyeType, ovrFovPort fov, unsigned int distortionCaps, ovrDistortionMesh *meshData);
typedef void (*pfn_ovrHmd_DestroyDistortionMesh)(ovrDistortionMesh* meshData);
typedef void (*pfn_ovrHmd_GetRenderScaleAndOffset)(ovrFovPort fov, ovrSizei textureSize, ovrRecti renderViewport, ovrVector2f uvScaleOffsetOut[2]);
typedef ovrFrameTiming (*pfn_ovrHmd_GetFrameTiming)(ovrHmd hmd, unsigned int frameIndex);
typedef ovrFrameTiming (*pfn_ovrHmd_BeginFrameTiming)(ovrHmd hmd, unsigned int frameIndex);
typedef void (*pfn_ovrHmd_EndFrameTiming)(ovrHmd hmd);
typedef void (*pfn_ovrHmd_ResetFrameTiming)(ovrHmd hmd, unsigned int frameIndex, bool vsync);
typedef void (*pfn_ovrHmd_GetEyePoses)(ovrHmd hmd, unsigned int frameIndex, ovrVector3f hmdToEyeViewOffset[2], ovrPosef outEyePoses[2], ovrTrackingState* outHmdTrackingState);
typedef ovrPosef (*pfn_ovrHmd_GetHmdPosePerEye)(ovrHmd hmd, ovrEyeType eye);
typedef void (*pfn_ovrHmd_GetEyeTimewarpMatrices)(ovrHmd hmd, ovrEyeType eye, ovrPosef renderPose, ovrMatrix4f twmOut[2]);
typedef ovrMatrix4f (*pfn_ovrMatrix4f_Projection) (ovrFovPort fov, float znear, float zfar, ovrBool rightHanded );
typedef ovrMatrix4f (*pfn_ovrMatrix4f_OrthoSubProjection) (ovrFovPort fov, ovrVector2f orthoScale, float orthoDistance, float eyeViewAdjustX);
typedef double (*pfn_ovr_GetTimeInSeconds)();

#ifdef __cplusplus 
}
#endif

#endif /* mozilla_ovr_capi_dynamic_h_ */
