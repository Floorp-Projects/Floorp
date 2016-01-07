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

#define OVR_CAPI_LIMITED_MOZILLA 1

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

#ifdef __cplusplus 
extern "C" {
#endif

typedef int32_t ovrResult;

typedef enum {
  ovrSuccess = 0,
  ovrSuccess_NotVisible = 1000,
  ovrSuccess_HMDFirmwareMismatch = 4100,
  ovrSuccess_TrackerFirmwareMismatch = 4101,
  ovrSuccess_ControllerFirmwareMismatch = 4104,
} ovrSuccessType;

typedef char ovrBool;
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
  ovrHmd_None      = 0,    
  ovrHmd_DK1       = 3,
  ovrHmd_DKHD      = 4,
  ovrHmd_DK2       = 6,
  ovrHmd_CB        = 8,
  ovrHmd_Other     = 9,
  ovrHmd_E3_2015   = 10,
  ovrHmd_ES06      = 11,
  ovrHmd_EnumSize = 0x7fffffff
} ovrHmdType;

typedef enum {
  ovrHmdCap_Writable_Mask     = 0x0000,
  ovrHmdCap_Service_Mask      = 0x0000,
  ovrHmdCap_DebugDevice       = 0x0010,
  ovrHmdCap_EnumSize          = 0x7fffffff
} ovrHmdCaps;

typedef enum
{
  ovrTrackingCap_Orientation      = 0x0010,
  ovrTrackingCap_MagYawCorrection = 0x0020,
  ovrTrackingCap_Position         = 0x0040,
  ovrTrackingCap_Idle             = 0x0100,
  ovrTrackingCap_EnumSize         = 0x7fffffff
} ovrTrackingCaps;

typedef struct OVR_ALIGNAS(OVR_PTR_SIZE) {
  char Reserved[8];
} ovrGraphicsLuid;

typedef enum {
  ovrEye_Left  = 0,
  ovrEye_Right = 1,
  ovrEye_Count = 2,
  ovrEye_EnumSize = 0x7fffffff
} ovrEyeType;

typedef struct OVR_ALIGNAS(OVR_PTR_SIZE) {
  ovrHmdType  Type;
  OVR_ON64(unsigned char pad0[4];)
  char ProductName[64];
  char Manufacturer[64];
  short VendorId;
  short ProductId;
  char SerialNumber[24];
  short FirmwareMajor;
  short FirmwareMinor;
  float CameraFrustumHFovInRadians;
  float CameraFrustumVFovInRadians;
  float CameraFrustumNearZInMeters;
  float CameraFrustumFarZInMeters;

  unsigned int AvailableHmdCaps;
  unsigned int DefaultHmdCaps;
  unsigned int AvailableTrackingCaps;
  unsigned int DefaultTrackingCaps;

  ovrFovPort  DefaultEyeFov[ovrEye_Count];
  ovrFovPort  MaxEyeFov[ovrEye_Count];
  ovrSizei    Resolution;
  float DisplayRefreshRate;
  OVR_ON64(unsigned char pad1[4];)
} ovrHmdDesc;

typedef struct ovrHmdStruct* ovrSession;

typedef enum {
  ovrStatus_OrientationTracked    = 0x0001,
  ovrStatus_PositionTracked       = 0x0002,
  ovrStatus_CameraPoseTracked     = 0x0004,
  ovrStatus_PositionConnected     = 0x0020,
  ovrStatus_HmdConnected          = 0x0080,
  ovrStatus_EnumSize              = 0x7fffffff
} ovrStatusBits;

typedef struct OVR_ALIGNAS(4) {
  ovrVector3f    Accelerometer;
  ovrVector3f    Gyro;
  ovrVector3f    Magnetometer;
  float          Temperature;
  float          TimeInSeconds;
} ovrSensorData;


typedef struct OVR_ALIGNAS(8) {
  ovrPoseStatef HeadPose;
  ovrPosef CameraPose;
  ovrPosef LeveledCameraPose;
  ovrPoseStatef HandPoses[2];
  ovrSensorData RawSensorData;
  unsigned int StatusFlags;
  unsigned int HandStatusFlags[2];
  uint32_t LastCameraFrameCounter;
  unsigned char pad0[4];
} ovrTrackingState;

typedef struct OVR_ALIGNAS(4) {
	ovrEyeType Eye;
	ovrFovPort Fov;
	ovrRecti DistortedViewport;
	ovrVector2f PixelsPerTanAngleAtCenter;
	ovrVector3f HmdToEyeViewOffset;
} ovrEyeRenderDesc;

typedef struct OVR_ALIGNAS(4) {
	float Projection22;
	float Projection23;
	float Projection32;
} ovrTimewarpProjectionDesc;

typedef struct OVR_ALIGNAS(4) {
	ovrVector3f HmdToEyeViewOffset[ovrEye_Count];
	float HmdSpaceToWorldScaleInMeters;
} ovrViewScaleDesc;

typedef enum {
	ovrRenderAPI_None = 0,
	ovrRenderAPI_OpenGL = 1,
	ovrRenderAPI_Android_GLES = 2,
	ovrRenderAPI_D3D11 = 5,
	ovrRenderAPI_Count = 4,
	ovrRenderAPI_EnumSize = 0x7fffffff
} ovrRenderAPIType;

typedef struct OVR_ALIGNAS(4) {
  ovrRenderAPIType API;
  ovrSizei TextureSize;
} ovrTextureHeader;

typedef struct OVR_ALIGNAS(OVR_PTR_SIZE) {
  ovrTextureHeader Header;
  OVR_ON64(unsigned char pad0[4];)
  uintptr_t PlatformData[8];
} ovrTexture;

typedef struct OVR_ALIGNAS(OVR_PTR_SIZE) {
  ovrTexture* Textures;
  int TextureCount;
  int CurrentIndex;
} ovrSwapTextureSet;


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
  ovrButton_Private = 0x00400000 | 0x00800000 | 0x01000000,
  ovrButton_EnumSize = 0x7fffffff
} ovrButton;

typedef enum {
  ovrTouch_A = ovrButton_A,
  ovrTouch_B = ovrButton_B,
  ovrTouch_RThumb = ovrButton_RThumb,
  ovrTouch_RIndexTrigger = 0x00000010,
  ovrTouch_X = ovrButton_X,
  ovrTouch_Y = ovrButton_Y,
  ovrTouch_LThumb = ovrButton_LThumb,
  ovrTouch_LIndexTrigger = 0x00001000,
  ovrTouch_RIndexPointing = 0x00000020,
  ovrTouch_RThumbUp = 0x00000040,
  ovrTouch_LIndexPointing = 0x00002000,
  ovrTouch_LThumbUp = 0x00004000,
  ovrTouch_EnumSize = 0x7fffffff
} ovrTouch;

typedef enum {
  ovrControllerType_None = 0x00,
  ovrControllerType_LTouch = 0x01,
  ovrControllerType_RTouch = 0x02,
  ovrControllerType_Touch = 0x03,
  ovrControllerType_XBox = 0x10,
  ovrControllerType_All = 0xff,
  ovrControllerType_EnumSize = 0x7fffffff
} ovrControllerType;

typedef enum {
  ovrHand_Left = 0,
  ovrHand_Right = 1,
  ovrHand_EnumSize = 0x7fffffff
} ovrHandType;

typedef struct {
  double              TimeInSeconds;
  unsigned int        ConnectedControllerTypes;
  unsigned int        Buttons;
  unsigned int        Touches;
  float               IndexTrigger[2];
  float               HandTrigger[2];
  ovrVector2f         Thumbstick[2];
} ovrInputState;

typedef enum {
  ovrInit_Debug          = 0x00000001,
  ovrInit_RequestVersion = 0x00000004,
  ovrinit_WritableBits   = 0x00ffffff,
  ovrInit_EnumSize       = 0x7fffffff
} ovrInitFlags;

typedef enum {
  ovrLogLevel_Debug = 0,
  ovrLogLevel_Info  = 1,
  ovrLogLevel_Error = 2,
  ovrLogLevel_EnumSize = 0x7fffffff
} ovrLogLevel;

typedef void (OVR_PFN *ovrLogCallback)(int level, const char* message);

typedef struct OVR_ALIGNAS(8) {
  uint32_t Flags;
  uint32_t RequestedMinorVersion;
  ovrLogCallback LogCallback;
  uintptr_t UserData;
  uint32_t ConnectionTimeoutMS;
  OVR_ON64(unsigned char pad0[4];)
} ovrInitParams;

typedef ovrResult(OVR_PFN *pfn_ovr_Initialize)(ovrInitParams const* params);
typedef void (OVR_PFN *pfn_ovr_Shutdown)();

typedef struct {
  ovrResult Result;
  char      ErrorString[512];
} ovrErrorInfo;

typedef void (OVR_PFN *pfn_ovr_GetLastErrorInfo)(ovrErrorInfo* errorInfo);
typedef const char* (OVR_PFN *pfn_ovr_GetVersionString)();
typedef int (OVR_PFN *pfn_ovr_TraceMessage)(int level, const char* message);
typedef ovrHmdDesc (OVR_PFN *pfn_ovr_GetHmdDesc)(ovrSession session);
typedef ovrResult (OVR_PFN *pfn_ovr_Create)(ovrSession*, ovrGraphicsLuid*);
typedef void (OVR_PFN *pfn_ovr_Destroy)(ovrSession session);

typedef struct {
  ovrBool HasVrFocus;
  ovrBool HmdPresent;
} ovrSessionStatus;

typedef ovrResult (OVR_PFN *pfn_ovr_GetSessionStatus)(ovrSession session, ovrSessionStatus* sessionStatus);
typedef unsigned int (OVR_PFN *pfn_ovr_GetEnabledCaps)(ovrSession session);
typedef void (OVR_PFN *pfn_ovr_SetEnabledCaps)(ovrSession session, unsigned int hmdCaps);
typedef unsigned int (OVR_PFN *pfn_ovr_GetTrackingCaps)(ovrSession session);
typedef ovrResult(OVR_PFN *pfn_ovr_ConfigureTracking)(ovrSession session, unsigned int requestedTrackingCaps, unsigned int requiredTrackingCaps);
typedef void (OVR_PFN *pfn_ovr_RecenterPose)(ovrSession session);
typedef ovrTrackingState (OVR_PFN *pfn_ovr_GetTrackingState)(ovrSession session, double absTime, ovrBool latencyMarker);
typedef ovrResult (OVR_PFN *pfn_ovr_GetInputState)(ovrSession session, unsigned int controllerTypeMask, ovrInputState* inputState);
typedef ovrResult (OVR_PFN *pfn_ovr_SetControllerVibration)(ovrSession session, unsigned int controllerTypeMask, float frequency, float amplitude);

enum {
  ovrMaxLayerCount = 32
};

typedef enum {
  ovrLayerType_Disabled       = 0,
  ovrLayerType_EyeFov         = 1,
  ovrLayerType_EyeFovDepth    = 2,
  ovrLayerType_Quad           = 3,
  ovrLayerType_EyeMatrix      = 5,
  ovrLayerType_Direct         = 6,
  ovrLayerType_EnumSize       = 0x7fffffff
} ovrLayerType;

typedef enum {
  ovrLayerFlag_HighQuality               = 0x01,
  ovrLayerFlag_TextureOriginAtBottomLeft = 0x02,
  ovrLayerFlag_HeadLocked                = 0x04
} ovrLayerFlags;

typedef struct OVR_ALIGNAS(OVR_PTR_SIZE) {
    ovrLayerType    Type;
    unsigned        Flags;
} ovrLayerHeader;

typedef struct OVR_ALIGNAS(OVR_PTR_SIZE) {
    ovrLayerHeader      Header;
    ovrSwapTextureSet*  ColorTexture[ovrEye_Count];
    ovrRecti            Viewport[ovrEye_Count];
    ovrFovPort          Fov[ovrEye_Count];
    ovrPosef            RenderPose[ovrEye_Count];
    double              SensorSampleTime;
} ovrLayerEyeFov;

typedef struct OVR_ALIGNAS(OVR_PTR_SIZE) {
  ovrLayerHeader      Header;
  ovrSwapTextureSet*  ColorTexture[ovrEye_Count];
  ovrRecti            Viewport[ovrEye_Count];
  ovrFovPort          Fov[ovrEye_Count];
  ovrPosef            RenderPose[ovrEye_Count];
  double              SensorSampleTime;
  ovrSwapTextureSet*  DepthTexture[ovrEye_Count];
  ovrTimewarpProjectionDesc ProjectionDesc;
} ovrLayerEyeFovDepth;

typedef struct OVR_ALIGNAS(OVR_PTR_SIZE) {
  ovrLayerHeader      Header;
  ovrSwapTextureSet*  ColorTexture[ovrEye_Count];
  ovrRecti            Viewport[ovrEye_Count];
  ovrPosef            RenderPose[ovrEye_Count];
  ovrMatrix4f         Matrix[ovrEye_Count];
  double              SensorSampleTime;
} ovrLayerEyeMatrix;

typedef struct OVR_ALIGNAS(OVR_PTR_SIZE) {
  ovrLayerHeader      Header;
  ovrSwapTextureSet*  ColorTexture;
  ovrRecti            Viewport;
  ovrPosef            QuadPoseCenter;
  ovrVector2f         QuadSize;
} ovrLayerQuad;

typedef struct OVR_ALIGNAS(OVR_PTR_SIZE) {
  ovrLayerHeader      Header;
  ovrSwapTextureSet*  ColorTexture[ovrEye_Count];
  ovrRecti            Viewport[ovrEye_Count];
} ovrLayerDirect;

typedef union {
  ovrLayerHeader      Header;
  ovrLayerEyeFov      EyeFov;
  ovrLayerEyeFovDepth EyeFovDepth;
  ovrLayerQuad        Quad;
  ovrLayerDirect      Direct;
} ovrLayer_Union;

typedef void (OVR_PFN *pfn_ovr_DestroySwapTextureSet)(ovrSession session, ovrSwapTextureSet* textureSet);
typedef void (OVR_PFN *pfn_ovr_DestroyMirrorTexture)(ovrSession session, ovrTexture* mirrorTexture);
typedef ovrSizei (OVR_PFN *pfn_ovr_GetFovTextureSize)(ovrSession session, ovrEyeType eye, ovrFovPort fov, float pixelsPerDisplayPixel);
typedef ovrEyeRenderDesc (OVR_PFN *pfn_ovr_GetRenderDesc)(ovrSession session, ovrEyeType eyeType, ovrFovPort fov);
typedef ovrResult (OVR_PFN *pfn_ovr_SubmitFrame)(ovrSession session, unsigned int frameIndex,
  const ovrViewScaleDesc* viewScaleDesc,
  ovrLayerHeader const * const * layerPtrList, unsigned int layerCount);
typedef double (OVR_PFN *pfn_ovr_GetPredictedDisplayTime)(ovrSession session, long long frameIndex);
typedef double (OVR_PFN *pfn_ovr_GetTimeInSeconds)();

typedef enum {
  ovrPerfHud_Off = 0,
  ovrPerfHud_LatencyTiming = 1,
  ovrPerfHud_RenderTiming = 2,
  ovrPerfHud_PerfHeadroom = 3,
  ovrPerfHud_VersionInfo = 4,
  ovrPerfHud_Count,
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

typedef void (OVR_PFN *pfn_ovr_ResetBackOfHeadTracking)(ovrSession session);
typedef void (OVR_PFN *pfn_ovr_ResetMulticameraTracking)(ovrSession session);
typedef ovrBool (OVR_PFN *pfn_ovr_GetBool)(ovrSession session, const char* propertyName, ovrBool defaultVal);
typedef ovrBool (OVR_PFN *pfn_ovr_SetBool)(ovrSession session, const char* propertyName, ovrBool value);
typedef int (OVR_PFN *pfn_ovr_GetInt)(ovrSession session, const char* propertyName, int defaultVal);
typedef ovrBool (OVR_PFN *pfn_ovr_SetInt)(ovrSession session, const char* propertyName, int value);
typedef float (OVR_PFN *pfn_ovr_GetFloat)(ovrSession session, const char* propertyName, float defaultVal);
typedef ovrBool (OVR_PFN *pfn_ovr_SetFloat)(ovrSession session, const char* propertyName, float value);
typedef unsigned int (OVR_PFN *pfn_ovr_GetFloatArray)(ovrSession session, const char* propertyName,
  float values[], unsigned int valuesCapacity);
typedef ovrBool (OVR_PFN *pfn_ovr_SetFloatArray)(ovrSession session, const char* propertyName,
  const float values[], unsigned int valuesSize);
typedef const char* (OVR_PFN *pfn_ovr_GetString)(ovrSession session, const char* propertyName,
  const char* defaultVal);
typedef ovrBool (OVR_PFN *pfn_ovr_SetString)(ovrSession session, const char* propertyName,
  const char* value);



typedef enum {
  ovrError_MemoryAllocationFailure = -1000,
  ovrError_SocketCreationFailure = -1001,
  ovrError_InvalidSession = -1002,
  ovrError_Timeout = -1003,
  ovrError_NotInitialized = -1004,
  ovrError_InvalidParameter = -1005,
  ovrError_ServiceError = -1006,
  ovrError_NoHmd = -1007,
  ovrError_AudioReservedBegin = -2000,
  ovrError_AudioDeviceNotFound = -2001,
  ovrError_AudioComError = -2002,
  ovrError_AudioReservedEnd = -2999,
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
  ovrError_InvalidBundleAdjustment = -4000,
  ovrError_USBBandwidth = -4001,
  ovrError_USBEnumeratedSpeed = -4002,
  ovrError_ImageSensorCommError = -4003,
  ovrError_GeneralTrackerFailure = -4004,
  ovrError_ExcessiveFrameTruncation = -4005,
  ovrError_ExcessiveFrameSkipping = -4006,
  ovrError_SyncDisconnected = -4007,
  ovrError_TrackerMemoryReadFailure = -4008,
  ovrError_TrackerMemoryWriteFailure = -4009,
  ovrError_TrackerFrameTimeout = -4010,
  ovrError_TrackerTruncatedFrame = -4011,
  ovrError_HMDFirmwareMismatch = -4100,
  ovrError_TrackerFirmwareMismatch = -4101,
  ovrError_BootloaderDeviceDetected = -4102,
  ovrError_TrackerCalibrationError = -4103,
  ovrError_ControllerFirmwareMismatch = -4104,
  ovrError_Incomplete = -5000,
  ovrError_Abandoned = -5001,
  ovrError_DisplayLost = -6000,
  ovrError_RuntimeException = -7000,
} ovrErrorType;


#ifdef XP_WIN
struct D3D11_TEXTURE2D_DESC;
struct ID3D11Device;
struct ID3D11Texture2D;
struct ID3D11ShaderResourceView;

typedef struct OVR_ALIGNAS(OVR_PTR_SIZE) {
    ovrTextureHeader          Header;
    OVR_ON64(uint32_t pad0;)
    ID3D11Texture2D*          pTexture;
    ID3D11ShaderResourceView* pSRView;
} ovrD3D11TextureData;

typedef union {
    ovrTexture          Texture;
    ovrD3D11TextureData D3D11;
} ovrD3D11Texture;

typedef enum {
    ovrSwapTextureSetD3D11_Typeless = 0x0001,
    ovrSwapTextureSetD3D11_EnumSize = 0x7fffffff
} ovrSwapTextureSetD3D11Flags;

typedef ovrResult (OVR_PFN *pfn_ovr_CreateSwapTextureSetD3D11)(ovrSession session, ID3D11Device* device,
                                                               const D3D11_TEXTURE2D_DESC* desc,
                                                               unsigned int miscFlags,
                                                               ovrSwapTextureSet** outTextureSet);

typedef ovrResult (OVR_PFN *pfn_ovr_CreateMirrorTextureD3D11)(ovrSession session,
                                                              ID3D11Device* device,
                                                              const D3D11_TEXTURE2D_DESC* desc,
                                                              unsigned int miscFlags,
                                                              ovrTexture** outMirrorTexture);

#endif

typedef struct {
    ovrTextureHeader Header;
    uint32_t TexId;
} ovrGLTextureData;

typedef union {
    ovrTexture       Texture;
    ovrGLTextureData OGL;
} ovrGLTexture;

typedef ovrResult (OVR_PFN *pfn_ovr_CreateSwapTextureSetGL)(ovrSession session, uint32_t format,
                                                            int width, int height,
                                                            ovrSwapTextureSet** outTextureSet);

typedef ovrResult (OVR_PFN *pfn_ovr_CreateMirrorTextureGL)(ovrSession session, uint32_t format,
                                                         int width, int height,
                                                         ovrTexture** outMirrorTexture);

#ifdef __cplusplus 
}
#endif

#endif /* mozilla_ovr_capi_dynamic_h_ */
#endif /* OVR_CAPI_h */
