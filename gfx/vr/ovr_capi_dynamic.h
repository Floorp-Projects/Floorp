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
#ifdef _MSC_VER
#pragma message("ovr_capi_dyanmic.h: OVR_CAPI.h included before ovr_capi_dynamic.h, skipping this")
#else
#warning OVR_CAPI.h included before ovr_capi_dynamic.h, skipping this
#endif
#define mozilla_ovr_capi_dynamic_h_

#else

#ifndef mozilla_ovr_capi_dynamic_h_
#define mozilla_ovr_capi_dynamic_h_

#ifdef HAVE_64BIT_BUILD
#define OVR_PTR_SIZE 8
#define OVR_ON64(x)     x
#else
#define OVR_PTR_SIZE 4
#define OVR_ON64(x)     /**/
#endif

#if defined(_WIN32)
#define OVR_PFN __cdecl
#else
#define OVR_PFN
#endif

#if !defined(OVR_ALIGNAS)
#if defined(__GNUC__) || defined(__clang__)
#define OVR_ALIGNAS(n) __attribute__((aligned(n)))
#elif defined(_MSC_VER) || defined(__INTEL_COMPILER)
#define OVR_ALIGNAS(n) __declspec(align(n))
#elif defined(__CC_ARM)
#define OVR_ALIGNAS(n) __align(n)
#else
#error Need to define OVR_ALIGNAS
#endif
#endif

#if !defined(OVR_UNUSED_STRUCT_PAD)
#define OVR_UNUSED_STRUCT_PAD(padName, size) char padName[size];
#endif

#ifdef __cplusplus 
extern "C" {
#endif

typedef int32_t ovrResult;



typedef enum {
  ovrSuccess = 0,
} ovrSuccessType;

typedef char ovrBool;
typedef struct OVR_ALIGNAS(4) { float r, g, b, a; } ovrColorf;
typedef struct OVR_ALIGNAS(4) { int x, y; } ovrVector2i;
typedef struct OVR_ALIGNAS(4) { int w, h; } ovrSizei;
typedef struct OVR_ALIGNAS(4) { ovrVector2i Pos; ovrSizei Size; } ovrRecti;
typedef struct OVR_ALIGNAS(4) { float x, y, z, w; } ovrQuatf;
typedef struct OVR_ALIGNAS(4) { float x, y; } ovrVector2f;
typedef struct OVR_ALIGNAS(4) { float x, y, z; } ovrVector3f;
typedef struct OVR_ALIGNAS(4) { float M[4][4]; } ovrMatrix4f;

typedef struct OVR_ALIGNAS(4) {
  ovrQuatf Orientation;
  ovrVector3f Position;
} ovrPosef;

typedef struct OVR_ALIGNAS(8) {
  ovrPosef ThePose;
  ovrVector3f AngularVelocity;
  ovrVector3f LinearVelocity;
  ovrVector3f AngularAcceleration;
  ovrVector3f LinearAcceleration;
  OVR_UNUSED_STRUCT_PAD(pad0, 4)
  double      TimeInSeconds;
} ovrPoseStatef;

typedef struct {
  float UpTan;
  float DownTan;
  float LeftTan;
  float RightTan;
} ovrFovPort;

typedef enum {
  ovrHmd_None      = 0,    
  ovrHmd_DK1       = 3,
  ovrHmd_DKHD      = 4,
  ovrHmd_DK2       = 6,
  ovrHmd_CB        = 8,
  ovrHmd_Other     = 9,
  ovrHmd_E3_2015   = 10,
  ovrHmd_ES06      = 11,
  ovrHmd_ES09      = 12,
  ovrHmd_ES11      = 13,
  ovrHmd_CV1       = 14,
  ovrHmd_EnumSize = 0x7fffffff
} ovrHmdType;

typedef enum {
  ovrHmdCap_DebugDevice       = 0x0010,
  ovrHmdCap_EnumSize          = 0x7fffffff
} ovrHmdCaps;

typedef enum
{
  ovrTrackingCap_Orientation      = 0x0010,
  ovrTrackingCap_MagYawCorrection = 0x0020,
  ovrTrackingCap_Position         = 0x0040,
  ovrTrackingCap_EnumSize         = 0x7fffffff
} ovrTrackingCaps;

typedef enum {
  ovrEye_Left  = 0,
  ovrEye_Right = 1,
  ovrEye_Count = 2,
  ovrEye_EnumSize = 0x7fffffff
} ovrEyeType;

typedef enum {
  ovrTrackingOrigin_EyeLevel = 0,
  ovrTrackingOrigin_FloorLevel = 1,
  ovrTrackingOrigin_Count = 2,            ///< \internal Count of enumerated elements.
  ovrTrackingOrigin_EnumSize = 0x7fffffff ///< \internal Force type int32_t.
} ovrTrackingOrigin;

typedef struct OVR_ALIGNAS(OVR_PTR_SIZE) {
	char Reserved[8];
} ovrGraphicsLuid;

typedef struct OVR_ALIGNAS(OVR_PTR_SIZE) {
  ovrHmdType  Type;
  OVR_ON64(OVR_UNUSED_STRUCT_PAD(pad0, 4))
  char ProductName[64];
  char Manufacturer[64];
  short VendorId;
  short ProductId;
  char SerialNumber[24];
  short FirmwareMajor;
  short FirmwareMinor;
  unsigned int AvailableHmdCaps;
  unsigned int DefaultHmdCaps;
  unsigned int AvailableTrackingCaps;
  unsigned int DefaultTrackingCaps;
  ovrFovPort DefaultEyeFov[ovrEye_Count];
  ovrFovPort MaxEyeFov[ovrEye_Count];
  ovrSizei Resolution;
  float DisplayRefreshRate;
  OVR_ON64(OVR_UNUSED_STRUCT_PAD(pad1, 4))
} ovrHmdDesc;

typedef struct ovrHmdStruct* ovrSession;

#ifdef XP_WIN
typedef uint32_t ovrProcessId;
#else
typedef pid_t ovrProcessId;
#endif

typedef enum {
  ovrStatus_OrientationTracked    = 0x0001,
  ovrStatus_PositionTracked       = 0x0002,
  ovrStatus_EnumSize              = 0x7fffffff
} ovrStatusBits;

typedef struct OVR_ALIGNAS(OVR_PTR_SIZE) {
  float FrustumHFovInRadians;
  float FrustumVFovInRadians;
  float FrustumNearZInMeters;
  float FrustumFarZInMeters;
} ovrTrackerDesc;

typedef enum {
  ovrTracker_Connected = 0x0020,
  ovrTracker_PoseTracked = 0x0004
} ovrTrackerFlags;

typedef struct OVR_ALIGNAS(8) {
  unsigned int TrackerFlags;
  ovrPosef Pose;
  ovrPosef LeveledPose;
  OVR_UNUSED_STRUCT_PAD(pad0, 4)
} ovrTrackerPose;

typedef struct OVR_ALIGNAS(8) {
  ovrPoseStatef HeadPose;
  unsigned int StatusFlags;
  ovrPoseStatef HandPoses[2];
  unsigned int HandStatusFlags[2];
  ovrPosef CalibratedOrigin;
} ovrTrackingState;

typedef struct OVR_ALIGNAS(4) {
  ovrEyeType  Eye;
  ovrFovPort  Fov;
  ovrRecti    DistortedViewport;
  ovrVector2f PixelsPerTanAngleAtCenter;
  ovrVector3f HmdToEyeOffset;
} ovrEyeRenderDesc;

typedef struct OVR_ALIGNAS(4) {
  float Projection22;
  float Projection23;
  float Projection32;
} ovrTimewarpProjectionDesc;

typedef struct OVR_ALIGNAS(4) {
  ovrVector3f HmdToEyeOffset[ovrEye_Count];
  float HmdSpaceToWorldScaleInMeters;
} ovrViewScaleDesc;

typedef enum {
  ovrTexture_2D,
  ovrTexture_2D_External,
  ovrTexture_Cube,
  ovrTexture_Count,
  ovrTexture_EnumSize = 0x7fffffff
} ovrTextureType;

typedef enum {
  ovrTextureBind_None,
  ovrTextureBind_DX_RenderTarget = 0x0001,
  ovrTextureBind_DX_UnorderedAccess = 0x0002,
  ovrTextureBind_DX_DepthStencil = 0x0004,
  ovrTextureBind_EnumSize = 0x7fffffff
} ovrTextureBindFlags;

typedef enum {
  OVR_FORMAT_UNKNOWN = 0,
  OVR_FORMAT_B5G6R5_UNORM = 1,
  OVR_FORMAT_B5G5R5A1_UNORM = 2,
  OVR_FORMAT_B4G4R4A4_UNORM = 3,
  OVR_FORMAT_R8G8B8A8_UNORM = 4,
  OVR_FORMAT_R8G8B8A8_UNORM_SRGB = 5,
  OVR_FORMAT_B8G8R8A8_UNORM = 6,
  OVR_FORMAT_B8G8R8A8_UNORM_SRGB = 7,
  OVR_FORMAT_B8G8R8X8_UNORM = 8,
  OVR_FORMAT_B8G8R8X8_UNORM_SRGB = 9,
  OVR_FORMAT_R16G16B16A16_FLOAT = 10,
  OVR_FORMAT_R11G11B10_FLOAT = 25,
  OVR_FORMAT_D16_UNORM = 11,
  OVR_FORMAT_D24_UNORM_S8_UINT = 12,
  OVR_FORMAT_D32_FLOAT = 13,
  OVR_FORMAT_D32_FLOAT_S8X24_UINT = 14,
  OVR_FORMAT_BC1_UNORM = 15,
  OVR_FORMAT_BC1_UNORM_SRGB = 16,
  OVR_FORMAT_BC2_UNORM = 17,
  OVR_FORMAT_BC2_UNORM_SRGB = 18,
  OVR_FORMAT_BC3_UNORM = 19,
  OVR_FORMAT_BC3_UNORM_SRGB = 20,
  OVR_FORMAT_BC6H_UF16 = 21,
  OVR_FORMAT_BC6H_SF16 = 22,
  OVR_FORMAT_BC7_UNORM = 23,
  OVR_FORMAT_BC7_UNORM_SRGB = 24,

  OVR_FORMAT_ENUMSIZE = 0x7fffffff
} ovrTextureFormat;

typedef enum {
  ovrTextureMisc_None,
  ovrTextureMisc_DX_Typeless = 0x0001,
  ovrTextureMisc_AllowGenerateMips = 0x0002,
  ovrTextureMisc_ProtectedContent = 0x0004,
  ovrTextureMisc_EnumSize = 0x7fffffff
} ovrTextureFlags;

typedef struct {
  ovrTextureType Type;
  ovrTextureFormat Format;
  int ArraySize;
  int Width;
  int Height;
  int MipLevels;
  int SampleCount;
  ovrBool StaticImage;
  unsigned int MiscFlags;
  unsigned int BindFlags;
} ovrTextureSwapChainDesc;

typedef struct {
  ovrTextureFormat Format;
  int Width;
  int Height;
  unsigned int MiscFlags;
  unsigned int MirrorOptions;
} ovrMirrorTextureDesc;

typedef struct ovrTextureSwapChainData* ovrTextureSwapChain;
typedef struct ovrMirrorTextureData* ovrMirrorTexture;

typedef enum {
  ovrButton_A = 0x00000001,
  ovrButton_B = 0x00000002,
  ovrButton_RThumb = 0x00000004,
  ovrButton_RShoulder = 0x00000008,
  ovrButton_X = 0x00000100,
  ovrButton_Y = 0x00000200,
  ovrButton_LThumb = 0x00000400,
  ovrButton_LShoulder = 0x00000800,
  ovrButton_Up = 0x00010000,
  ovrButton_Down = 0x00020000,
  ovrButton_Left = 0x00040000,
  ovrButton_Right = 0x00080000,
  ovrButton_Enter = 0x00100000,
  ovrButton_Back = 0x00200000,
  ovrButton_VolUp = 0x00400000,
  ovrButton_VolDown = 0x00800000,
  ovrButton_Home = 0x01000000,
  ovrButton_Private = ovrButton_VolUp | ovrButton_VolDown | ovrButton_Home,
  ovrButton_RMask = ovrButton_A | ovrButton_B | ovrButton_RThumb | ovrButton_RShoulder,
  ovrButton_LMask = ovrButton_X | ovrButton_Y | ovrButton_LThumb | ovrButton_LShoulder | ovrButton_Enter,
  ovrButton_EnumSize = 0x7fffffff
} ovrButton;

typedef enum {
  ovrTouch_A = ovrButton_A,
  ovrTouch_B = ovrButton_B,
  ovrTouch_RThumb = ovrButton_RThumb,
  ovrTouch_RThumbRest = 0x00000008,
  ovrTouch_RIndexTrigger = 0x00000010,
  ovrTouch_RButtonMask = ovrTouch_A | ovrTouch_B | ovrTouch_RThumb | ovrTouch_RThumbRest | ovrTouch_RIndexTrigger,
  ovrTouch_X = ovrButton_X,
  ovrTouch_Y = ovrButton_Y,
  ovrTouch_LThumb = ovrButton_LThumb,
  ovrTouch_LThumbRest = 0x00000800,
  ovrTouch_LIndexTrigger = 0x00001000,
  ovrTouch_LButtonMask = ovrTouch_X | ovrTouch_Y | ovrTouch_LThumb | ovrTouch_LThumbRest | ovrTouch_LIndexTrigger,
  ovrTouch_RIndexPointing = 0x00000020,
  ovrTouch_RThumbUp = 0x00000040,
  ovrTouch_LIndexPointing = 0x00002000,
  ovrTouch_LThumbUp = 0x00004000,
  ovrTouch_RPoseMask = ovrTouch_RIndexPointing | ovrTouch_RThumbUp,
  ovrTouch_LPoseMask = ovrTouch_LIndexPointing | ovrTouch_LThumbUp,
  ovrTouch_EnumSize = 0x7fffffff
} ovrTouch;

typedef struct OVR_ALIGNAS(OVR_PTR_SIZE) {
  int SampleRateHz;
  int SampleSizeInBytes;
  int QueueMinSizeToAvoidStarvation;
  int SubmitMinSamples;
  int SubmitMaxSamples;
  int SubmitOptimalSamples;
} ovrTouchHapticsDesc;

typedef enum {
  ovrControllerType_None = 0x0000,
  ovrControllerType_LTouch = 0x0001,
  ovrControllerType_RTouch = 0x0002,
  ovrControllerType_Touch = (ovrControllerType_LTouch | ovrControllerType_RTouch),
  ovrControllerType_Remote = 0x0004,
  ovrControllerType_XBox = 0x0010,
  ovrControllerType_Object0 = 0x0100,
  ovrControllerType_Object1 = 0x0200,
  ovrControllerType_Object2 = 0x0400,
  ovrControllerType_Object3 = 0x0800,
  ovrControllerType_Active = 0xffffffff,
  ovrControllerType_EnumSize = 0x7fffffff
} ovrControllerType;

typedef enum {
  ovrHapticsBufferSubmit_Enqueue
} ovrHapticsBufferSubmitMode;

typedef struct {
  const void* Samples;
  int SamplesCount;
  ovrHapticsBufferSubmitMode SubmitMode;
} ovrHapticsBuffer;

typedef struct {
  int RemainingQueueSpace;
  int SamplesQueued;
} ovrHapticsPlaybackState;

typedef enum {
  ovrTrackedDevice_None = 0x0000,
  ovrTrackedDevice_HMD = 0x0001,
  ovrTrackedDevice_LTouch = 0x0002,
  ovrTrackedDevice_RTouch = 0x0004,
  ovrTrackedDevice_Touch = (ovrTrackedDevice_LTouch | ovrTrackedDevice_RTouch),
  ovrTrackedDevice_Object0 = 0x0010,
  ovrTrackedDevice_Object1 = 0x0020,
  ovrTrackedDevice_Object2 = 0x0040,
  ovrTrackedDevice_Object3 = 0x0080,
  ovrTrackedDevice_All = 0xFFFF,
} ovrTrackedDeviceType;

typedef enum {
  ovrBoundary_Outer = 0x0001,
  ovrBoundary_PlayArea = 0x0100,
} ovrBoundaryType;

typedef struct {
  ovrColorf Color;
} ovrBoundaryLookAndFeel;

typedef struct {
  ovrBool IsTriggering;
  float ClosestDistance;
  ovrVector3f ClosestPoint;
  ovrVector3f ClosestPointNormal;
} ovrBoundaryTestResult;

typedef enum {
  ovrHand_Left = 0,
  ovrHand_Right = 1,
  ovrHand_Count = 2,
  ovrHand_EnumSize = 0x7fffffff
} ovrHandType;

typedef struct {
  double TimeInSeconds;
  unsigned int Buttons;
  unsigned int Touches;
  float IndexTrigger[ovrHand_Count];
  float HandTrigger[ovrHand_Count];
  ovrVector2f Thumbstick[ovrHand_Count];
  ovrControllerType ControllerType;
  float IndexTriggerNoDeadzone[ovrHand_Count];
  float HandTriggerNoDeadzone[ovrHand_Count];
  ovrVector2f ThumbstickNoDeadzone[ovrHand_Count];
  float IndexTriggerRaw[ovrHand_Count];
  float HandTriggerRaw[ovrHand_Count];
  ovrVector2f ThumbstickRaw[ovrHand_Count];
} ovrInputState;

typedef struct {
  double LastChangedTime;
  ovrFovPort FOVPort;
  float VirtualNearPlaneDistanceMeters;
  float VirtualFarPlaneDistanceMeters;
  ovrSizei ImageSensorPixelResolution;
  ovrMatrix4f LensDistortionMatrix;
  double ExposurePeriodSeconds;
  double ExposureDurationSeconds;
} ovrCameraIntrinsics;

typedef enum {
  ovrCameraStatus_None = 0x0,
  ovrCameraStatus_Connected = 0x1,
  ovrCameraStatus_Calibrating = 0x2,
  ovrCameraStatus_CalibrationFailed = 0x4,
  ovrCameraStatus_Calibrated = 0x8,
  ovrCameraStatus_EnumSize = 0x7fffffff
} ovrCameraStatusFlags;

typedef struct {
  double LastChangedTimeSeconds;
  unsigned int CameraStatusFlags;
  ovrTrackedDeviceType AttachedToDevice;
  ovrPosef RelativePose;
  double LastExposureTimeSeconds;
  double ExposureLatencySeconds;
  double AdditionalLatencySeconds;
} ovrCameraExtrinsics;

#define OVR_EXTERNAL_CAMERA_NAME_SIZE 32
typedef struct {
  char Name[OVR_EXTERNAL_CAMERA_NAME_SIZE];
  ovrCameraIntrinsics Intrinsics;
  ovrCameraExtrinsics Extrinsics;
} ovrExternalCamera;

typedef enum {
  ovrInit_Debug          = 0x00000001,
  ovrInit_RequestVersion = 0x00000004,
  ovrInit_Invisible      = 0x00000010,
  ovrInit_MixedRendering = 0x00000020,
  ovrinit_WritableBits   = 0x00ffffff,
  ovrInit_EnumSize       = 0x7fffffff
} ovrInitFlags;

typedef enum {
  ovrLogLevel_Debug = 0,
  ovrLogLevel_Info  = 1,
  ovrLogLevel_Error = 2,
  ovrLogLevel_EnumSize = 0x7fffffff
} ovrLogLevel;

typedef void (OVR_PFN* ovrLogCallback)(uintptr_t userData, int level, const char* message);

typedef struct OVR_ALIGNAS(8) {
  uint32_t Flags;
  uint32_t RequestedMinorVersion;
  ovrLogCallback LogCallback;
  uintptr_t UserData;
  uint32_t ConnectionTimeoutMS;
  OVR_ON64(OVR_UNUSED_STRUCT_PAD(pad0, 4))
} ovrInitParams;

typedef ovrResult(OVR_PFN* pfn_ovr_Initialize)(const ovrInitParams* params);
typedef void (OVR_PFN* pfn_ovr_Shutdown)();

typedef struct {
  ovrResult Result;
  char      ErrorString[512];
} ovrErrorInfo;

typedef void (OVR_PFN* pfn_ovr_GetLastErrorInfo)(ovrErrorInfo* errorInfo);
typedef const char* (OVR_PFN* pfn_ovr_GetVersionString)();
typedef int (OVR_PFN* pfn_ovr_TraceMessage)(int level, const char* message);
typedef ovrResult (OVR_PFN* pfn_ovr_IdentifyClient)(const char* identity);
typedef ovrHmdDesc (OVR_PFN* pfn_ovr_GetHmdDesc)(ovrSession session);
typedef unsigned int (OVR_PFN* pfn_ovr_GetTrackerCount)(ovrSession session);
typedef ovrTrackerDesc (OVR_PFN* pfn_ovr_GetTrackerDesc)(ovrSession session, unsigned int trackerDescIndex);
typedef ovrResult (OVR_PFN* pfn_ovr_Create)(ovrSession* pSession, ovrGraphicsLuid* pLuid);
typedef void (OVR_PFN* pfn_ovr_Destroy)(ovrSession session);

typedef struct {
  ovrBool IsVisible;
  ovrBool HmdPresent;
  ovrBool HmdMounted;
  ovrBool DisplayLost;
  ovrBool ShouldQuit;
  ovrBool ShouldRecenter;
} ovrSessionStatus;

typedef ovrResult (OVR_PFN* pfn_ovr_GetSessionStatus)(ovrSession session, ovrSessionStatus* sessionStatus);
typedef ovrResult (OVR_PFN* pfn_ovr_SetTrackingOriginType)(ovrSession session, ovrTrackingOrigin origin);
typedef ovrTrackingOrigin (OVR_PFN* pfn_ovr_GetTrackingOriginType)(ovrSession session);
typedef ovrResult (OVR_PFN* pfn_ovr_RecenterTrackingOrigin)(ovrSession session);
typedef ovrResult (OVR_PFN* pfn_ovr_SpecifyTrackingOrigin)(ovrSession session, ovrPosef originPose);
typedef void (OVR_PFN* pfn_ovr_ClearShouldRecenterFlag)(ovrSession session);
typedef ovrTrackingState (OVR_PFN* pfn_ovr_GetTrackingState)(ovrSession session, double absTime, ovrBool latencyMarker);
typedef ovrResult  (OVR_PFN* pfn_ovr_GetDevicePoses)(ovrSession session,
                                                     ovrTrackedDeviceType* deviceTypes,
                                                     int deviceCount,
                                                     double absTime,
                                                     ovrPoseStatef* outDevicePoses);
typedef ovrTrackerPose (OVR_PFN* pfn_ovr_GetTrackerPose)(ovrSession session, unsigned int trackerPoseIndex);
typedef ovrResult (OVR_PFN* pfn_ovr_GetInputState)(ovrSession session, ovrControllerType controllerType, ovrInputState* inputState);
typedef unsigned int (OVR_PFN* pfn_ovr_GetConnectedControllerTypes)(ovrSession session);
typedef ovrTouchHapticsDesc (OVR_PFN* pfn_ovr_GetTouchHapticsDesc)(ovrSession session, ovrControllerType controllerType);
typedef ovrResult (OVR_PFN* pfn_ovr_SetControllerVibration)(ovrSession session, ovrControllerType controllerType, float frequency, float amplitude);
typedef ovrResult (OVR_PFN* pfn_ovr_SubmitControllerVibration)(ovrSession session,
                                                               ovrControllerType controllerType,
                                                               const ovrHapticsBuffer* buffer);
typedef ovrResult (OVR_PFN* pfn_ovr_GetControllerVibrationState)(ovrSession session,
                                                                 ovrControllerType controllerType,
                                                                 ovrHapticsPlaybackState* outState);
typedef ovrResult (OVR_PFN* pfn_ovr_TestBoundary)(ovrSession session,
                                                  ovrTrackedDeviceType deviceBitmask,
                                                  ovrBoundaryType boundaryType,
                                                  ovrBoundaryTestResult* outTestResult);
typedef ovrResult (OVR_PFN* pfn_ovr_TestBoundaryPoint)(ovrSession session,
                                                       const ovrVector3f* point,
                                                       ovrBoundaryType singleBoundaryType,
                                                       ovrBoundaryTestResult* outTestResult);
typedef ovrResult (OVR_PFN* pfn_ovr_SetBoundaryLookAndFeel)(ovrSession session, const ovrBoundaryLookAndFeel* lookAndFeel);
typedef ovrResult (OVR_PFN* pfn_ovr_ResetBoundaryLookAndFeel)(ovrSession session);
typedef ovrResult (OVR_PFN* pfn_ovr_GetBoundaryGeometry)(ovrSession session,
                                                         ovrBoundaryType boundaryType,
                                                         ovrVector3f* outFloorPoints,
                                                         int* outFloorPointsCount);
typedef ovrResult (OVR_PFN* pfn_ovr_GetBoundaryDimensions)(ovrSession session,
                                                           ovrBoundaryType boundaryType,
                                                           ovrVector3f* outDimensions);
typedef ovrResult (OVR_PFN* pfn_ovr_GetBoundaryVisible)(ovrSession session, ovrBool* outIsVisible);
typedef ovrResult (OVR_PFN* pfn_ovr_RequestBoundaryVisible)(ovrSession session, ovrBool visible);

enum {
  ovrMaxLayerCount = 16
};

typedef enum {
  ovrLayerType_Disabled       = 0,
  ovrLayerType_EyeFov         = 1,
  ovrLayerType_Quad           = 3,
  ovrLayerType_EyeMatrix      = 5,
  ovrLayerType_EnumSize       = 0x7fffffff
} ovrLayerType;

typedef enum {
  ovrLayerFlag_HighQuality               = 0x01,
  ovrLayerFlag_TextureOriginAtBottomLeft = 0x02,
  ovrLayerFlag_HeadLocked                = 0x04
} ovrLayerFlags;

typedef struct OVR_ALIGNAS(OVR_PTR_SIZE) {
  ovrLayerType Type;
  unsigned Flags;
} ovrLayerHeader;

typedef struct OVR_ALIGNAS(OVR_PTR_SIZE) {
  ovrLayerHeader Header;
  ovrTextureSwapChain ColorTexture[ovrEye_Count];
  ovrRecti Viewport[ovrEye_Count];
  ovrFovPort Fov[ovrEye_Count];
  ovrPosef RenderPose[ovrEye_Count];
  double SensorSampleTime;
} ovrLayerEyeFov;

typedef struct OVR_ALIGNAS(OVR_PTR_SIZE) {
  ovrLayerHeader Header;
  ovrTextureSwapChain ColorTexture[ovrEye_Count];
  ovrRecti Viewport[ovrEye_Count];
  ovrPosef RenderPose[ovrEye_Count];
  ovrMatrix4f Matrix[ovrEye_Count];
  double SensorSampleTime;
} ovrLayerEyeMatrix;

typedef struct OVR_ALIGNAS(OVR_PTR_SIZE) {
  ovrLayerHeader Header;
  ovrTextureSwapChain ColorTexture;
  ovrRecti Viewport;
  ovrPosef QuadPoseCenter;
  ovrVector2f QuadSize;
} ovrLayerQuad;

typedef union {
  ovrLayerHeader Header;
  ovrLayerEyeFov EyeFov;
  ovrLayerQuad Quad;
} ovrLayer_Union;


typedef ovrResult (OVR_PFN* pfn_ovr_GetTextureSwapChainLength)(ovrSession session, ovrTextureSwapChain chain, int* out_Length);
typedef ovrResult (OVR_PFN* pfn_ovr_GetTextureSwapChainCurrentIndex)(ovrSession session, ovrTextureSwapChain chain, int* out_Index);
typedef ovrResult (OVR_PFN* pfn_ovr_GetTextureSwapChainDesc)(ovrSession session, ovrTextureSwapChain chain, ovrTextureSwapChainDesc* out_Desc);
typedef ovrResult (OVR_PFN* pfn_ovr_CommitTextureSwapChain)(ovrSession session, ovrTextureSwapChain chain);
typedef void (OVR_PFN* pfn_ovr_DestroyTextureSwapChain)(ovrSession session, ovrTextureSwapChain chain);
typedef void (OVR_PFN* pfn_ovr_DestroyMirrorTexture)(ovrSession session, ovrMirrorTexture mirrorTexture);
typedef ovrSizei(OVR_PFN* pfn_ovr_GetFovTextureSize)(ovrSession session, ovrEyeType eye, ovrFovPort fov, float pixelsPerDisplayPixel);
typedef ovrEyeRenderDesc(OVR_PFN* pfn_ovr_GetRenderDesc)(ovrSession session, ovrEyeType eyeType, ovrFovPort fov);
typedef ovrResult(OVR_PFN* pfn_ovr_SubmitFrame)(ovrSession session, long long frameIndex,
	const ovrViewScaleDesc* viewScaleDesc,
	ovrLayerHeader const * const * layerPtrList, unsigned int layerCount);

typedef struct OVR_ALIGNAS(4) {
  int HmdVsyncIndex;
  int AppFrameIndex;
  int AppDroppedFrameCount;
  float AppMotionToPhotonLatency;
  float AppQueueAheadTime;
  float AppCpuElapsedTime;
  float AppGpuElapsedTime;
  int CompositorFrameIndex;
  int CompositorDroppedFrameCount;
  float CompositorLatency;
  float CompositorCpuElapsedTime;
  float CompositorGpuElapsedTime;
  float CompositorCpuStartToGpuEndElapsedTime;
  float CompositorGpuEndToVsyncElapsedTime;
  ovrBool AswIsActive;
  int AswActivatedToggleCount;
  int AswPresentedFrameCount;
  int AswFailedFrameCount;
} ovrPerfStatsPerCompositorFrame;

enum { ovrMaxProvidedFrameStats = 5 };

typedef struct OVR_ALIGNAS(4) {
  ovrPerfStatsPerCompositorFrame FrameStats[ovrMaxProvidedFrameStats];
  int FrameStatsCount;
  ovrBool AnyFrameStatsDropped;
  float AdaptiveGpuPerformanceScale;
  ovrBool AswIsAvailable;
  ovrProcessId VisibleProcessId;
} ovrPerfStats;

typedef ovrResult (OVR_PFN* pfn_ovr_GetPerfStats)(ovrSession session, ovrPerfStats* outStats);
typedef ovrResult (OVR_PFN* pfn_ovr_ResetPerfStats)(ovrSession session);
typedef double (OVR_PFN* pfn_ovr_GetPredictedDisplayTime)(ovrSession session, long long frameIndex);
typedef double (OVR_PFN* pfn_ovr_GetTimeInSeconds)();

typedef enum {
  ovrPerfHud_Off = 0,
  ovrPerfHud_PerfSummary = 1,
  ovrPerfHud_LatencyTiming = 2,
  ovrPerfHud_AppRenderTiming = 3,
  ovrPerfHud_CompRenderTiming = 4,
  ovrPerfHud_AswStats = 6,
  ovrPerfHud_VersionInfo = 5,
  ovrPerfHud_Count = 7,
  ovrPerfHud_EnumSize = 0x7fffffff
} ovrPerfHudMode;

typedef enum {
  ovrLayerHud_Off = 0,
  ovrLayerHud_Info = 1,
  ovrLayerHud_EnumSize = 0x7fffffff
} ovrLayerHudMode;

typedef enum {
  ovrDebugHudStereo_Off = 0,
  ovrDebugHudStereo_Quad = 1,
  ovrDebugHudStereo_QuadWithCrosshair = 2,
  ovrDebugHudStereo_CrosshairAtInfinity = 3,
  ovrDebugHudStereo_Count,
  ovrDebugHudStereo_EnumSize = 0x7fffffff
} ovrDebugHudStereoMode;

typedef ovrBool(OVR_PFN* pfn_ovr_GetBool)(ovrSession session, const char* propertyName, ovrBool defaultVal);
typedef ovrBool(OVR_PFN* pfn_ovr_SetBool)(ovrSession session, const char* propertyName, ovrBool value);
typedef int (OVR_PFN* pfn_ovr_GetInt)(ovrSession session, const char* propertyName, int defaultVal);
typedef ovrBool (OVR_PFN* pfn_ovr_SetInt)(ovrSession session, const char* propertyName, int value);
typedef float (OVR_PFN* pfn_ovr_GetFloat)(ovrSession session, const char* propertyName, float defaultVal);
typedef ovrBool (OVR_PFN* pfn_ovr_SetFloat)(ovrSession session, const char* propertyName, float value);
typedef unsigned int (OVR_PFN* pfn_ovr_GetFloatArray)(ovrSession session, const char* propertyName,
  float values[], unsigned int valuesCapacity);
typedef ovrBool (OVR_PFN* pfn_ovr_SetFloatArray)(ovrSession session, const char* propertyName,
  const float values[], unsigned int valuesSize);
typedef const char* (OVR_PFN* pfn_ovr_GetString)(ovrSession session, const char* propertyName,
  const char* defaultVal);
typedef ovrBool (OVR_PFN* pfn_ovr_SetString)(ovrSession session, const char* propertyName,
  const char* value);

typedef ovrResult (OVR_PFN* pfn_ovr_GetExternalCameras)(ovrSession session,
                                                    ovrExternalCamera* cameras,
                                                    unsigned int* inoutCameraCount);
typedef ovrResult (OVR_PFN* pfn_ovr_SetExternalCameraProperties)(ovrSession session,
                                                             const char* name,
                                                             const ovrCameraIntrinsics* const intrinsics,
                                                             const ovrCameraExtrinsics* const extrinsics);

typedef enum {
  ovrSuccess_NotVisible = 1000,
  ovrSuccess_BoundaryInvalid = 1001,
  ovrSuccess_DeviceUnavailable = 1002,
} ovrSuccessTypes;

typedef enum {
  ovrError_MemoryAllocationFailure = -1000,
  ovrError_InvalidSession = -1002,
  ovrError_Timeout = -1003,
  ovrError_NotInitialized = -1004,
  ovrError_InvalidParameter = -1005,
  ovrError_ServiceError = -1006,
  ovrError_NoHmd = -1007,
  ovrError_Unsupported = -1009,
  ovrError_DeviceUnavailable = -1010,
  ovrError_InvalidHeadsetOrientation = -1011,
  ovrError_ClientSkippedDestroy = -1012,
  ovrError_ClientSkippedShutdown = -1013,
  ovrError_ServiceDeadlockDetected = -1014,
  ovrError_InvalidOperation = -1015,
  ovrError_InsufficientArraySize = -1016,
  ovrError_NoExternalCameraInfo = -1017,
  ovrError_AudioDeviceNotFound = -2001,
  ovrError_AudioComError = -2002,
  ovrError_Initialize = -3000,
  ovrError_LibLoad = -3001,
  ovrError_LibVersion = -3002,
  ovrError_ServiceConnection = -3003,
  ovrError_ServiceVersion = -3004,
  ovrError_IncompatibleOS = -3005,
  ovrError_DisplayInit = -3006,
  ovrError_ServerStart = -3007,
  ovrError_Reinitialization = -3008,
  ovrError_MismatchedAdapters = -3009,
  ovrError_LeakingResources = -3010,
  ovrError_ClientVersion = -3011,
  ovrError_OutOfDateOS = -3012,
  ovrError_OutOfDateGfxDriver = -3013,
  ovrError_IncompatibleGPU = -3014,
  ovrError_NoValidVRDisplaySystem = -3015,
  ovrError_Obsolete = -3016,
  ovrError_DisabledOrDefaultAdapter = -3017,
  ovrError_HybridGraphicsNotSupported = -3018,
  ovrError_DisplayManagerInit = -3019,
  ovrError_TrackerDriverInit = -3020,
  ovrError_LibSignCheck = -3021,
  ovrError_LibPath = -3022,
  ovrError_LibSymbols = -3023,
  ovrError_RemoteSession = -3024,
  ovrError_InitializeVulkan = -3025,
  ovrError_DisplayLost = -6000,
  ovrError_TextureSwapChainFull = -6001,
  ovrError_TextureSwapChainInvalid = -6002,
  ovrError_GraphicsDeviceReset = -6003,
  ovrError_DisplayRemoved = -6004,
  ovrError_ContentProtectionNotAvailable = -6005,
  ovrError_ApplicationInvisible = -6006,
  ovrError_Disallowed = -6007,
  ovrError_DisplayPluggedIncorrectly = -6008,
  ovrError_RuntimeException = -7000,
  ovrError_NoCalibration = -9000,
  ovrError_OldVersion = -9001,
  ovrError_MisformattedBlock = -9002,
} ovrErrorType;


#ifdef XP_WIN

struct IUnknown;

typedef ovrResult (OVR_PFN* pfn_ovr_CreateTextureSwapChainDX)(ovrSession session,
	IUnknown* d3dPtr,
	const ovrTextureSwapChainDesc* desc,
	ovrTextureSwapChain* out_TextureSwapChain);

typedef ovrResult (OVR_PFN* pfn_ovr_GetTextureSwapChainBufferDX)(ovrSession session,
	ovrTextureSwapChain chain,
	int index,
	IID iid,
	void** out_Buffer);

typedef ovrResult (OVR_PFN* pfn_ovr_CreateMirrorTextureDX)(ovrSession session,
	IUnknown* d3dPtr,
	const ovrMirrorTextureDesc* desc,
	ovrMirrorTexture* out_MirrorTexture);

typedef ovrResult (OVR_PFN* pfn_ovr_GetMirrorTextureBufferDX)(ovrSession session,
	ovrMirrorTexture mirrorTexture,
	IID iid,
	void** out_Buffer);

#endif


typedef ovrResult (OVR_PFN* pfn_ovr_CreateTextureSwapChainGL)(ovrSession session,
	const ovrTextureSwapChainDesc* desc,
	ovrTextureSwapChain* out_TextureSwapChain);

typedef ovrResult (OVR_PFN* pfn_ovr_GetTextureSwapChainBufferGL)(ovrSession session,
	ovrTextureSwapChain chain,
	int index,
	unsigned int* out_TexId);

typedef ovrResult (OVR_PFN* pfn_ovr_CreateMirrorTextureGL)(ovrSession session,
	const ovrMirrorTextureDesc* desc,
	ovrMirrorTexture* out_MirrorTexture);

typedef ovrResult (OVR_PFN* pfn_ovr_GetMirrorTextureBufferGL)(ovrSession session,
	ovrMirrorTexture mirrorTexture,
	unsigned int* out_TexId);

#define OVR_KEY_EYE_HEIGHT "EyeHeight" // float meters
#define OVR_DEFAULT_EYE_HEIGHT 1.675f

#if !defined(OVR_SUCCESS)
#define OVR_SUCCESS(result) (result >= 0)
#endif

#if !defined(OVR_UNQUALIFIED_SUCCESS)
#define OVR_UNQUALIFIED_SUCCESS(result) (result == ovrSuccess)
#endif

#if !defined(OVR_FAILURE)
#define OVR_FAILURE(result) (!OVR_SUCCESS(result))
#endif

#ifdef __cplusplus
}
#endif

#endif /* mozilla_ovr_capi_dynamic_h_ */
#endif /* OVR_CAPI_h */
