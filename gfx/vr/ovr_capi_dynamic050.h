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

#ifndef mozilla_ovr_capi_dynamic050_h_
#define mozilla_ovr_capi_dynamic050_h_

#define OVR_CAPI_050_LIMITED_MOZILLA 1

#if defined(_WIN32)
#define OVR_PFN __cdecl
#else
#define OVR_PFN
#endif

#if !defined(OVR_ALIGNAS)
#if defined(__GNUC__) && (((__GNUC__ * 100) + __GNUC_MINOR__) >= 408) && (defined(__GXX_EXPERIMENTAL_CXX0X__) || (__cplusplus >= 201103L))
#define OVR_ALIGNAS(n) alignas(n)
#elif defined(__clang__) && !defined(__APPLE__) && (((__clang_major__ * 100) + __clang_minor__) >= 300) && (__cplusplus >= 201103L)
#define OVR_ALIGNAS(n) alignas(n)
#elif defined(__clang__) && defined(__APPLE__) && (((__clang_major__ * 100) + __clang_minor__) >= 401) && (__cplusplus >= 201103L)
#define OVR_ALIGNAS(n) alignas(n)
#elif defined(_MSC_VER) && (_MSC_VER >= 1900)
#define OVR_ALIGNAS(n) alignas(n)
#elif defined(__EDG_VERSION__) && (__EDG_VERSION__ >= 408)
#define OVR_ALIGNAS(n) alignas(n)
#elif defined(__GNUC__) || defined(__clang__)
#define OVR_ALIGNAS(n) __attribute__((aligned(n)))
#elif defined(_MSC_VER) || defined(__INTEL_COMPILER)
#define OVR_ALIGNAS(n) __declspec(align(n))
#else
#error Need to define OVR_ALIGNAS
#endif
#endif

namespace ovr050 {

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

typedef struct OVR_ALIGNAS(8) {
  ovrPosef ThePose;
  ovrVector3f AngularVelocity;
  ovrVector3f LinearVelocity;
  ovrVector3f AngularAcceleration;
  ovrVector3f LinearAcceleration;
  float       Pad;
  double      TimeInSeconds;
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
  ovrHmd_BlackStar        = 7,
  ovrHmd_CB               = 8,
  ovrHmd_Other            = 9,
  ovrHmd_EnumSize         = 0x7fffffff
} ovrHmdType;

typedef enum {
  ovrHmdCap_Present           = 0x0001,
  ovrHmdCap_Available         = 0x0002,
  ovrHmdCap_Captured          = 0x0004,
  ovrHmdCap_ExtendDesktop     = 0x0008,
  ovrHmdCap_DebugDevice       = 0x0010,
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
  ovrTrackingCap_Idle             = 0x0100,
  ovrTrackingCap_EnumSize         = 0x7fffffff
} ovrTrackingCaps;

typedef enum {
  ovrDistortionCap_Chromatic = 0x01,
  ovrDistortionCap_TimeWarp  = 0x02,
  ovrDistortionCap_Vignette  = 0x08,
  ovrDistortionCap_NoRestore = 0x10,
  ovrDistortionCap_FlipInput = 0x20,
  ovrDistortionCap_SRGB      = 0x40,
  ovrDistortionCap_Overdrive = 0x80,
  ovrDistortionCap_HqDistortion = 0x100,
  ovrDistortionCap_LinuxDevFullscreen = 0x200,
  ovrDistortionCap_ComputeShader = 0x400,
  ovrDistortionCap_TimewarpJitDelay = 0x1000,
  ovrDistortionCap_ProfileNoSpinWaits = 0x10000,
  ovrDistortionCap_EnumSize = 0x7fffffff
} ovrDistortionCaps;

typedef enum {
  ovrEye_Left  = 0,
  ovrEye_Right = 1,
  ovrEye_Count = 2,
  ovrEye_EnumSize = 0x7fffffff
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
  ovrStatus_HmdConnected          = 0x0080,
  ovrStatus_EnumSize              = 0x7fffffff
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
  uint32_t LastCameraFrameCounter;
  uint32_t Pad;
} ovrTrackingState;

typedef struct OVR_ALIGNAS(8) ovrFrameTiming_ {
  float DeltaSeconds;
  float Pad; 
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
  ovrVector3f HmdToEyeViewOffset;
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

typedef enum {
  ovrInit_Debug          = 0x00000001,
  ovrInit_ServerOptional = 0x00000002,
  ovrInit_RequestVersion = 0x00000004,
  ovrInit_ForceNoDebug   = 0x00000008
} ovrInitFlags;

typedef enum {
  ovrLogLevel_Debug = 0,
  ovrLogLevel_Info  = 1,
  ovrLogLevel_Error = 2
} ovrLogLevel;

typedef void (OVR_PFN *ovrLogCallback)(int level, const char* message);

typedef struct {
  uint32_t Flags;
  uint32_t RequestedMinorVersion;
  ovrLogCallback LogCallback;
  uint32_t ConnectionTimeoutMS;
} ovrInitParams;

extern "C" {

typedef ovrBool (OVR_PFN *pfn_ovr_Initialize)(ovrInitParams const* params);
typedef void (OVR_PFN *pfn_ovr_Shutdown)();
typedef int (OVR_PFN *pfn_ovrHmd_Detect)();
typedef ovrHmd (OVR_PFN *pfn_ovrHmd_Create)(int index);
typedef void (OVR_PFN *pfn_ovrHmd_Destroy)(ovrHmd hmd);
typedef ovrHmd (OVR_PFN *pfn_ovrHmd_CreateDebug)(ovrHmdType type);
typedef const char* (OVR_PFN *pfn_ovrHmd_GetLastError)(ovrHmd hmd);
typedef ovrBool (OVR_PFN *pfn_ovrHmd_AttachToWindow)(ovrHmd hmd, void* window, const ovrRecti* destMirrorRect, const ovrRecti* sourceRenderTargetRect);
typedef unsigned int (OVR_PFN *pfn_ovrHmd_GetEnabledCaps)(ovrHmd hmd);
typedef void (OVR_PFN *pfn_ovrHmd_SetEnabledCaps)(ovrHmd hmd, unsigned int hmdCaps);
typedef ovrBool (OVR_PFN *pfn_ovrHmd_ConfigureTracking)(ovrHmd hmd, unsigned int supportedTrackingCaps, unsigned int requiredTrackingCaps); 
typedef void (OVR_PFN *pfn_ovrHmd_RecenterPose)(ovrHmd hmd);
typedef ovrTrackingState (OVR_PFN *pfn_ovrHmd_GetTrackingState)(ovrHmd hmd, double absTime);
typedef ovrSizei (OVR_PFN *pfn_ovrHmd_GetFovTextureSize)(ovrHmd hmd, ovrEyeType eye, ovrFovPort fov, float pixelsPerDisplayPixel);
typedef ovrEyeRenderDesc (OVR_PFN *pfn_ovrHmd_GetRenderDesc)(ovrHmd hmd, ovrEyeType eyeType, ovrFovPort fov);
typedef ovrBool (OVR_PFN *pfn_ovrHmd_CreateDistortionMesh)(ovrHmd hmd, ovrEyeType eyeType, ovrFovPort fov, unsigned int distortionCaps, ovrDistortionMesh *meshData);
typedef void (OVR_PFN *pfn_ovrHmd_DestroyDistortionMesh)(ovrDistortionMesh* meshData);
typedef void (OVR_PFN *pfn_ovrHmd_GetRenderScaleAndOffset)(ovrFovPort fov, ovrSizei textureSize, ovrRecti renderViewport, ovrVector2f uvScaleOffsetOut[2]);
typedef ovrFrameTiming (OVR_PFN *pfn_ovrHmd_GetFrameTiming)(ovrHmd hmd, unsigned int frameIndex);
typedef ovrFrameTiming (OVR_PFN *pfn_ovrHmd_BeginFrameTiming)(ovrHmd hmd, unsigned int frameIndex);
typedef void (OVR_PFN *pfn_ovrHmd_EndFrameTiming)(ovrHmd hmd);
typedef void (OVR_PFN *pfn_ovrHmd_ResetFrameTiming)(ovrHmd hmd, unsigned int frameIndex, bool vsync);
typedef void (OVR_PFN *pfn_ovrHmd_GetEyePoses)(ovrHmd hmd, unsigned int frameIndex, ovrVector3f hmdToEyeViewOffset[2], ovrPosef outEyePoses[2], ovrTrackingState* outHmdTrackingState);
typedef ovrPosef (OVR_PFN *pfn_ovrHmd_GetHmdPosePerEye)(ovrHmd hmd, ovrEyeType eye);
typedef void (OVR_PFN *pfn_ovrHmd_GetEyeTimewarpMatrices)(ovrHmd hmd, ovrEyeType eye, ovrPosef renderPose, ovrMatrix4f twmOut[2]);
typedef ovrMatrix4f (OVR_PFN *pfn_ovrMatrix4f_Projection) (ovrFovPort fov, float znear, float zfar, ovrBool rightHanded );
typedef ovrMatrix4f (OVR_PFN *pfn_ovrMatrix4f_OrthoSubProjection) (ovrFovPort fov, ovrVector2f orthoScale, float orthoDistance, float eyeViewAdjustX);
typedef double (OVR_PFN *pfn_ovr_GetTimeInSeconds)();

} // extern "C"
} // namespace ovr050

#endif /* mozilla_ovr_capi_dynamic050_h_ */
