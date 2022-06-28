/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_PLATFORM_WMF_MFMEDIAENGINEEXTRA_H
#define DOM_MEDIA_PLATFORM_WMF_MFMEDIAENGINEEXTRA_H

#include <evr.h>
#include <mfmediaengine.h>

// Currently, we build with WINVER=0x601 (Win7), which means newer
// declarations in mfmediaengine.h will not be visible. Also, we don't
// yet have the Fall Creators Update SDK available on build machines,
// so even with updated WINVER, some of the interfaces we need would
// not be present.
// To work around this, until the build environment is updated,
// we include copies of the relevant classes/interfaces we need.
#if !defined(WINVER) || WINVER < 0x0602

EXTERN_GUID(MF_MEDIA_ENGINE_CALLBACK, 0xc60381b8, 0x83a4, 0x41f8, 0xa3, 0xd0,
            0xde, 0x05, 0x07, 0x68, 0x49, 0xa9);
EXTERN_GUID(MF_MEDIA_ENGINE_DXGI_MANAGER, 0x065702da, 0x1094, 0x486d, 0x86,
            0x17, 0xee, 0x7c, 0xc4, 0xee, 0x46, 0x48);
EXTERN_GUID(MF_MEDIA_ENGINE_EXTENSION, 0x3109fd46, 0x060d, 0x4b62, 0x8d, 0xcf,
            0xfa, 0xff, 0x81, 0x13, 0x18, 0xd2);
EXTERN_GUID(MF_MEDIA_ENGINE_PLAYBACK_HWND, 0xd988879b, 0x67c9, 0x4d92, 0xba,
            0xa7, 0x6e, 0xad, 0xd4, 0x46, 0x03, 0x9d);
EXTERN_GUID(MF_MEDIA_ENGINE_OPM_HWND, 0xa0be8ee7, 0x0572, 0x4f2c, 0xa8, 0x01,
            0x2a, 0x15, 0x1b, 0xd3, 0xe7, 0x26);
EXTERN_GUID(MF_MEDIA_ENGINE_PLAYBACK_VISUAL, 0x6debd26f, 0x6ab9, 0x4d7e, 0xb0,
            0xee, 0xc6, 0x1a, 0x73, 0xff, 0xad, 0x15);
EXTERN_GUID(MF_MEDIA_ENGINE_COREWINDOW, 0xfccae4dc, 0x0b7f, 0x41c2, 0x9f, 0x96,
            0x46, 0x59, 0x94, 0x8a, 0xcd, 0xdc);
EXTERN_GUID(MF_MEDIA_ENGINE_VIDEO_OUTPUT_FORMAT, 0x5066893c, 0x8cf9, 0x42bc,
            0x8b, 0x8a, 0x47, 0x22, 0x12, 0xe5, 0x27, 0x26);
EXTERN_GUID(MF_MEDIA_ENGINE_CONTENT_PROTECTION_FLAGS, 0xe0350223, 0x5aaf,
            0x4d76, 0xa7, 0xc3, 0x06, 0xde, 0x70, 0x89, 0x4d, 0xb4);
EXTERN_GUID(MF_MEDIA_ENGINE_CONTENT_PROTECTION_MANAGER, 0xfdd6dfaa, 0xbd85,
            0x4af3, 0x9e, 0x0f, 0xa0, 0x1d, 0x53, 0x9d, 0x87, 0x6a);
EXTERN_GUID(MF_MEDIA_ENGINE_AUDIO_ENDPOINT_ROLE, 0xd2cb93d1, 0x116a, 0x44f2,
            0x93, 0x85, 0xf7, 0xd0, 0xfd, 0xa2, 0xfb, 0x46);
EXTERN_GUID(MF_MEDIA_ENGINE_AUDIO_CATEGORY, 0xc8d4c51d, 0x350e, 0x41f2, 0xba,
            0x46, 0xfa, 0xeb, 0xbb, 0x08, 0x57, 0xf6);
EXTERN_GUID(MF_MEDIA_ENGINE_STREAM_CONTAINS_ALPHA_CHANNEL, 0x5cbfaf44, 0xd2b2,
            0x4cfb, 0x80, 0xa7, 0xd4, 0x29, 0xc7, 0x4c, 0x78, 0x9d);
EXTERN_GUID(MF_MEDIA_ENGINE_BROWSER_COMPATIBILITY_MODE, 0x4e0212e2, 0xe18f,
            0x41e1, 0x95, 0xe5, 0xc0, 0xe7, 0xe9, 0x23, 0x5b, 0xc3);
EXTERN_GUID(MF_MEDIA_ENGINE_BROWSER_COMPATIBILITY_MODE_IE9, 0x052c2d39, 0x40c0,
            0x4188, 0xab, 0x86, 0xf8, 0x28, 0x27, 0x3b, 0x75, 0x22);
EXTERN_GUID(MF_MEDIA_ENGINE_BROWSER_COMPATIBILITY_MODE_IE10, 0x11a47afd, 0x6589,
            0x4124, 0xb3, 0x12, 0x61, 0x58, 0xec, 0x51, 0x7f, 0xc3);
EXTERN_GUID(MF_MEDIA_ENGINE_BROWSER_COMPATIBILITY_MODE_IE11, 0x1cf1315f, 0xce3f,
            0x4035, 0x93, 0x91, 0x16, 0x14, 0x2f, 0x77, 0x51, 0x89);
EXTERN_GUID(MF_MEDIA_ENGINE_BROWSER_COMPATIBILITY_MODE_IE_EDGE, 0xa6f3e465,
            0x3aca, 0x442c, 0xa3, 0xf0, 0xad, 0x6d, 0xda, 0xd8, 0x39, 0xae);
EXTERN_GUID(MF_MEDIA_ENGINE_COMPATIBILITY_MODE, 0x3ef26ad4, 0xdc54, 0x45de,
            0xb9, 0xaf, 0x76, 0xc8, 0xc6, 0x6b, 0xfa, 0x8e);
EXTERN_GUID(MF_MEDIA_ENGINE_COMPATIBILITY_MODE_WWA_EDGE, 0x15b29098, 0x9f01,
            0x4e4d, 0xb6, 0x5a, 0xc0, 0x6c, 0x6c, 0x89, 0xda, 0x2a);
EXTERN_GUID(MF_MEDIA_ENGINE_COMPATIBILITY_MODE_WIN10, 0x5b25e089, 0x6ca7,
            0x4139, 0xa2, 0xcb, 0xfc, 0xaa, 0xb3, 0x95, 0x52, 0xa3);
EXTERN_GUID(MF_MEDIA_ENGINE_SOURCE_RESOLVER_CONFIG_STORE, 0x0ac0c497, 0xb3c4,
            0x48c9, 0x9c, 0xde, 0xbb, 0x8c, 0xa2, 0x44, 0x2c, 0xa3);
EXTERN_GUID(MF_MEDIA_ENGINE_TRACK_ID, 0x65bea312, 0x4043, 0x4815, 0x8e, 0xab,
            0x44, 0xdc, 0xe2, 0xef, 0x8f, 0x2a);
EXTERN_GUID(MF_MEDIA_ENGINE_TELEMETRY_APPLICATION_ID, 0x1e7b273b, 0xa7e4,
            0x402a, 0x8f, 0x51, 0xc4, 0x8e, 0x88, 0xa2, 0xca, 0xbc);
EXTERN_GUID(MF_MEDIA_ENGINE_SYNCHRONOUS_CLOSE, 0xc3c2e12f, 0x7e0e, 0x4e43, 0xb9,
            0x1c, 0xdc, 0x99, 0x2c, 0xcd, 0xfa, 0x5e);
EXTERN_GUID(MF_MEDIA_ENGINE_MEDIA_PLAYER_MODE, 0x3ddd8d45, 0x5aa1, 0x4112, 0x82,
            0xe5, 0x36, 0xf6, 0xa2, 0x19, 0x7e, 0x6e);
EXTERN_GUID(CLSID_MFMediaEngineClassFactory, 0xb44392da, 0x499b, 0x446b, 0xa4,
            0xcb, 0x0, 0x5f, 0xea, 0xd0, 0xe6, 0xd5);
EXTERN_GUID(MF_MT_VIDEO_ROTATION, 0xc380465d, 0x2271, 0x428c, 0x9b, 0x83, 0xec,
            0xea, 0x3b, 0x4a, 0x85, 0xc1);

typedef enum _MFVideoRotationFormat {
  MFVideoRotationFormat_0 = 0,
  MFVideoRotationFormat_90 = 90,
  MFVideoRotationFormat_180 = 180,
  MFVideoRotationFormat_270 = 270
} MFVideoRotationFormat;

typedef enum MF_MEDIA_ENGINE_EVENT {
  MF_MEDIA_ENGINE_EVENT_LOADSTART = 1,
  MF_MEDIA_ENGINE_EVENT_PROGRESS = 2,
  MF_MEDIA_ENGINE_EVENT_SUSPEND = 3,
  MF_MEDIA_ENGINE_EVENT_ABORT = 4,
  MF_MEDIA_ENGINE_EVENT_ERROR = 5,
  MF_MEDIA_ENGINE_EVENT_EMPTIED = 6,
  MF_MEDIA_ENGINE_EVENT_STALLED = 7,
  MF_MEDIA_ENGINE_EVENT_PLAY = 8,
  MF_MEDIA_ENGINE_EVENT_PAUSE = 9,
  MF_MEDIA_ENGINE_EVENT_LOADEDMETADATA = 10,
  MF_MEDIA_ENGINE_EVENT_LOADEDDATA = 11,
  MF_MEDIA_ENGINE_EVENT_WAITING = 12,
  MF_MEDIA_ENGINE_EVENT_PLAYING = 13,
  MF_MEDIA_ENGINE_EVENT_CANPLAY = 14,
  MF_MEDIA_ENGINE_EVENT_CANPLAYTHROUGH = 15,
  MF_MEDIA_ENGINE_EVENT_SEEKING = 16,
  MF_MEDIA_ENGINE_EVENT_SEEKED = 17,
  MF_MEDIA_ENGINE_EVENT_TIMEUPDATE = 18,
  MF_MEDIA_ENGINE_EVENT_ENDED = 19,
  MF_MEDIA_ENGINE_EVENT_RATECHANGE = 20,
  MF_MEDIA_ENGINE_EVENT_DURATIONCHANGE = 21,
  MF_MEDIA_ENGINE_EVENT_VOLUMECHANGE = 22,
  MF_MEDIA_ENGINE_EVENT_FORMATCHANGE = 1000,
  MF_MEDIA_ENGINE_EVENT_PURGEQUEUEDEVENTS = 1001,
  MF_MEDIA_ENGINE_EVENT_TIMELINE_MARKER = 1002,
  MF_MEDIA_ENGINE_EVENT_BALANCECHANGE = 1003,
  MF_MEDIA_ENGINE_EVENT_DOWNLOADCOMPLETE = 1004,
  MF_MEDIA_ENGINE_EVENT_BUFFERINGSTARTED = 1005,
  MF_MEDIA_ENGINE_EVENT_BUFFERINGENDED = 1006,
  MF_MEDIA_ENGINE_EVENT_FRAMESTEPCOMPLETED = 1007,
  MF_MEDIA_ENGINE_EVENT_NOTIFYSTABLESTATE = 1008,
  MF_MEDIA_ENGINE_EVENT_FIRSTFRAMEREADY = 1009,
  MF_MEDIA_ENGINE_EVENT_TRACKSCHANGE = 1010,
  MF_MEDIA_ENGINE_EVENT_OPMINFO = 1011,
  MF_MEDIA_ENGINE_EVENT_RESOURCELOST = 1012,
  MF_MEDIA_ENGINE_EVENT_DELAYLOADEVENT_CHANGED = 1013,
  MF_MEDIA_ENGINE_EVENT_STREAMRENDERINGERROR = 1014,
  MF_MEDIA_ENGINE_EVENT_SUPPORTEDRATES_CHANGED = 1015,
  MF_MEDIA_ENGINE_EVENT_AUDIOENDPOINTCHANGE = 1016
} MF_MEDIA_ENGINE_EVENT;

typedef enum MF_MEDIA_ENGINE_PROTECTION_FLAGS {
  MF_MEDIA_ENGINE_ENABLE_PROTECTED_CONTENT = 1,
  MF_MEDIA_ENGINE_USE_PMP_FOR_ALL_CONTENT = 2,
  MF_MEDIA_ENGINE_USE_UNPROTECTED_PMP = 4
} MF_MEDIA_ENGINE_PROTECTION_FLAGS;

typedef enum MF_MEDIA_ENGINE_CREATEFLAGS {
  MF_MEDIA_ENGINE_AUDIOONLY = 0x1,
  MF_MEDIA_ENGINE_WAITFORSTABLE_STATE = 0x2,
  MF_MEDIA_ENGINE_FORCEMUTE = 0x4,
  MF_MEDIA_ENGINE_REAL_TIME_MODE = 0x8,
  MF_MEDIA_ENGINE_DISABLE_LOCAL_PLUGINS = 0x10,
  MF_MEDIA_ENGINE_CREATEFLAGS_MASK = 0x1f
} MF_MEDIA_ENGINE_CREATEFLAGS;

typedef enum MF_MEDIA_ENGINE_S3D_PACKING_MODE {
  MF_MEDIA_ENGINE_S3D_PACKING_MODE_NONE = 0,
  MF_MEDIA_ENGINE_S3D_PACKING_MODE_SIDE_BY_SIDE = 1,
  MF_MEDIA_ENGINE_S3D_PACKING_MODE_TOP_BOTTOM = 2
} MF_MEDIA_ENGINE_S3D_PACKING_MODE;

typedef enum MF_MEDIA_ENGINE_STATISTIC {
  MF_MEDIA_ENGINE_STATISTIC_FRAMES_RENDERED = 0,
  MF_MEDIA_ENGINE_STATISTIC_FRAMES_DROPPED = 1,
  MF_MEDIA_ENGINE_STATISTIC_BYTES_DOWNLOADED = 2,
  MF_MEDIA_ENGINE_STATISTIC_BUFFER_PROGRESS = 3,
  MF_MEDIA_ENGINE_STATISTIC_FRAMES_PER_SECOND = 4,
  MF_MEDIA_ENGINE_STATISTIC_PLAYBACK_JITTER = 5,
  MF_MEDIA_ENGINE_STATISTIC_FRAMES_CORRUPTED = 6,
  MF_MEDIA_ENGINE_STATISTIC_TOTAL_FRAME_DELAY = 7
} MF_MEDIA_ENGINE_STATISTIC;

typedef enum MF_MEDIA_ENGINE_SEEK_MODE {
  MF_MEDIA_ENGINE_SEEK_MODE_NORMAL = 0,
  MF_MEDIA_ENGINE_SEEK_MODE_APPROXIMATE = 1
} MF_MEDIA_ENGINE_SEEK_MODE;

typedef enum MF_MEDIA_ENGINE_ERR {
  MF_MEDIA_ENGINE_ERR_NOERROR = 0,
  MF_MEDIA_ENGINE_ERR_ABORTED = 1,
  MF_MEDIA_ENGINE_ERR_NETWORK = 2,
  MF_MEDIA_ENGINE_ERR_DECODE = 3,
  MF_MEDIA_ENGINE_ERR_SRC_NOT_SUPPORTED = 4,
  MF_MEDIA_ENGINE_ERR_ENCRYPTED = 5
} MF_MEDIA_ENGINE_ERR;

typedef enum MF_MEDIA_ENGINE_NETWORK {
  MF_MEDIA_ENGINE_NETWORK_EMPTY = 0,
  MF_MEDIA_ENGINE_NETWORK_IDLE = 1,
  MF_MEDIA_ENGINE_NETWORK_LOADING = 2,
  MF_MEDIA_ENGINE_NETWORK_NO_SOURCE = 3
} MF_MEDIA_ENGINE_NETWORK;

typedef enum MF_MEDIA_ENGINE_READY {
  MF_MEDIA_ENGINE_READY_HAVE_NOTHING = 0,
  MF_MEDIA_ENGINE_READY_HAVE_METADATA = 1,
  MF_MEDIA_ENGINE_READY_HAVE_CURRENT_DATA = 2,
  MF_MEDIA_ENGINE_READY_HAVE_FUTURE_DATA = 3,
  MF_MEDIA_ENGINE_READY_HAVE_ENOUGH_DATA = 4
} MF_MEDIA_ENGINE_READY;

typedef enum MF_MEDIA_ENGINE_CANPLAY {
  MF_MEDIA_ENGINE_CANPLAY_NOT_SUPPORTED = 0,
  MF_MEDIA_ENGINE_CANPLAY_MAYBE = 1,
  MF_MEDIA_ENGINE_CANPLAY_PROBABLY = 2
} MF_MEDIA_ENGINE_CANPLAY;

typedef enum MF_MEDIA_ENGINE_PRELOAD {
  MF_MEDIA_ENGINE_PRELOAD_MISSING = 0,
  MF_MEDIA_ENGINE_PRELOAD_EMPTY = 1,
  MF_MEDIA_ENGINE_PRELOAD_NONE = 2,
  MF_MEDIA_ENGINE_PRELOAD_METADATA = 3,
  MF_MEDIA_ENGINE_PRELOAD_AUTOMATIC = 4
} MF_MEDIA_ENGINE_PRELOAD;

#  ifndef __IMFMediaEngineNotify_INTERFACE_DEFINED__
#    define __IMFMediaEngineNotify_INTERFACE_DEFINED__

/* interface IMFMediaEngineNotify */
/* [local][unique][uuid][object] */

EXTERN_C const IID IID_IMFMediaEngineNotify;
MIDL_INTERFACE("fee7c112-e776-42b5-9bbf-0048524e2bd5")
IMFMediaEngineNotify : public IUnknown {
 public:
  virtual HRESULT STDMETHODCALLTYPE EventNotify(
      /* [annotation][in] */
      _In_ DWORD event,
      /* [annotation][in] */
      _In_ DWORD_PTR param1,
      /* [annotation][in] */
      _In_ DWORD param2) = 0;
};

#  endif /* __IMFMediaEngineNotify_INTERFACE_DEFINED__ */

#  ifndef __IMFMediaEngineExtension_INTERFACE_DEFINED__
#    define __IMFMediaEngineExtension_INTERFACE_DEFINED__

/* interface IMFMediaEngineExtension */
/* [local][unique][uuid][object] */
EXTERN_C const IID IID_IMFMediaEngineExtension;
MIDL_INTERFACE("2f69d622-20b5-41e9-afdf-89ced1dda04e")
IMFMediaEngineExtension : public IUnknown {
 public:
  virtual HRESULT STDMETHODCALLTYPE CanPlayType(
      /* [annotation][in] */
      _In_ BOOL AudioOnly,
      /* [annotation][in] */
      _In_ BSTR MimeType,
      /* [annotation][out] */
      _Out_ MF_MEDIA_ENGINE_CANPLAY * pAnswer) = 0;

  virtual HRESULT STDMETHODCALLTYPE BeginCreateObject(
      /* [annotation][in] */
      _In_ BSTR bstrURL,
      /* [annotation][in] */
      _In_opt_ IMFByteStream * pByteStream,
      /* [annotation][in] */
      _In_ MF_OBJECT_TYPE type,
      /* [annotation][out] */
      _Outptr_ IUnknown * *ppIUnknownCancelCookie,
      /* [annotation][in] */
      _In_ IMFAsyncCallback * pCallback,
      /* [annotation][in] */
      _In_opt_ IUnknown * punkState) = 0;

  virtual HRESULT STDMETHODCALLTYPE CancelObjectCreation(
      /* [annotation][in] */
      _In_ IUnknown * pIUnknownCancelCookie) = 0;

  virtual HRESULT STDMETHODCALLTYPE EndCreateObject(
      /* [annotation][in] */
      _In_ IMFAsyncResult * pResult,
      /* [annotation][out] */
      _Outptr_ IUnknown * *ppObject) = 0;
};

#  endif /* __IMFMediaEngineExtension_INTERFACE_DEFINED__ */

#  ifndef __IMFMediaEngineClassFactory_INTERFACE_DEFINED__
#    define __IMFMediaEngineClassFactory_INTERFACE_DEFINED__

/* interface IMFMediaEngineClassFactory */
/* [local][unique][uuid][object] */

EXTERN_C const IID IID_IMFMediaEngineClassFactory;

MIDL_INTERFACE("4D645ACE-26AA-4688-9BE1-DF3516990B93")
IMFMediaEngineClassFactory : public IUnknown {
 public:
  virtual HRESULT STDMETHODCALLTYPE CreateInstance(
      /* [annotation][in] */
      _In_ DWORD dwFlags,
      /* [annotation][in] */
      _In_ IMFAttributes * pAttr,
      /* [annotation][out] */
      _Outptr_ IMFMediaEngine * *ppPlayer) = 0;

  virtual HRESULT STDMETHODCALLTYPE CreateTimeRange(
      /* [annotation][out] */
      _Outptr_ IMFMediaTimeRange * *ppTimeRange) = 0;

  virtual HRESULT STDMETHODCALLTYPE CreateError(
      /* [annotation][out] */
      _Outptr_ IMFMediaError * *ppError) = 0;
};

#  endif /* __IMFMediaEngineClassFactory_INTERFACE_DEFINED__ */

#  ifndef __IMFMediaEngine_INTERFACE_DEFINED__
#    define __IMFMediaEngine_INTERFACE_DEFINED__

/* interface IMFMediaEngine */
/* [local][unique][uuid][object] */

EXTERN_C const IID IID_IMFMediaEngine;
MIDL_INTERFACE("98a1b0bb-03eb-4935-ae7c-93c1fa0e1c93")
IMFMediaEngine : public IUnknown {
 public:
  virtual HRESULT STDMETHODCALLTYPE GetError(
      /* [annotation][out] */
      _Outptr_ IMFMediaError * *ppError) = 0;

  virtual HRESULT STDMETHODCALLTYPE SetErrorCode(
      /* [annotation][in] */
      _In_ MF_MEDIA_ENGINE_ERR error) = 0;

  virtual HRESULT STDMETHODCALLTYPE SetSourceElements(
      /* [annotation][in] */
      _In_ IMFMediaEngineSrcElements * pSrcElements) = 0;

  virtual HRESULT STDMETHODCALLTYPE SetSource(
      /* [annotation][in] */
      _In_ BSTR pUrl) = 0;

  virtual HRESULT STDMETHODCALLTYPE GetCurrentSource(
      /* [annotation][out] */
      _Out_ BSTR * ppUrl) = 0;

  virtual USHORT STDMETHODCALLTYPE GetNetworkState(void) = 0;

  virtual MF_MEDIA_ENGINE_PRELOAD STDMETHODCALLTYPE GetPreload(void) = 0;

  virtual HRESULT STDMETHODCALLTYPE SetPreload(
      /* [annotation][in] */
      _In_ MF_MEDIA_ENGINE_PRELOAD Preload) = 0;

  virtual HRESULT STDMETHODCALLTYPE GetBuffered(
      /* [annotation][out] */
      _Outptr_ IMFMediaTimeRange * *ppBuffered) = 0;

  virtual HRESULT STDMETHODCALLTYPE Load(void) = 0;

  virtual HRESULT STDMETHODCALLTYPE CanPlayType(
      /* [annotation][in] */
      _In_ BSTR type,
      /* [annotation][out] */
      _Out_ MF_MEDIA_ENGINE_CANPLAY * pAnswer) = 0;

  virtual USHORT STDMETHODCALLTYPE GetReadyState(void) = 0;

  virtual BOOL STDMETHODCALLTYPE IsSeeking(void) = 0;

  virtual double STDMETHODCALLTYPE GetCurrentTime(void) = 0;

  virtual HRESULT STDMETHODCALLTYPE SetCurrentTime(
      /* [annotation][in] */
      _In_ double seekTime) = 0;

  virtual double STDMETHODCALLTYPE GetStartTime(void) = 0;

  virtual double STDMETHODCALLTYPE GetDuration(void) = 0;

  virtual BOOL STDMETHODCALLTYPE IsPaused(void) = 0;

  virtual double STDMETHODCALLTYPE GetDefaultPlaybackRate(void) = 0;

  virtual HRESULT STDMETHODCALLTYPE SetDefaultPlaybackRate(
      /* [annotation][in] */
      _In_ double Rate) = 0;

  virtual double STDMETHODCALLTYPE GetPlaybackRate(void) = 0;

  virtual HRESULT STDMETHODCALLTYPE SetPlaybackRate(
      /* [annotation][in] */
      _In_ double Rate) = 0;

  virtual HRESULT STDMETHODCALLTYPE GetPlayed(
      /* [annotation][out] */
      _Outptr_ IMFMediaTimeRange * *ppPlayed) = 0;

  virtual HRESULT STDMETHODCALLTYPE GetSeekable(
      /* [annotation][out] */
      _Outptr_ IMFMediaTimeRange * *ppSeekable) = 0;

  virtual BOOL STDMETHODCALLTYPE IsEnded(void) = 0;

  virtual BOOL STDMETHODCALLTYPE GetAutoPlay(void) = 0;

  virtual HRESULT STDMETHODCALLTYPE SetAutoPlay(
      /* [annotation][in] */
      _In_ BOOL AutoPlay) = 0;

  virtual BOOL STDMETHODCALLTYPE GetLoop(void) = 0;

  virtual HRESULT STDMETHODCALLTYPE SetLoop(
      /* [annotation][in] */
      _In_ BOOL Loop) = 0;

  virtual HRESULT STDMETHODCALLTYPE Play(void) = 0;

  virtual HRESULT STDMETHODCALLTYPE Pause(void) = 0;

  virtual BOOL STDMETHODCALLTYPE GetMuted(void) = 0;

  virtual HRESULT STDMETHODCALLTYPE SetMuted(
      /* [annotation][in] */
      _In_ BOOL Muted) = 0;

  virtual double STDMETHODCALLTYPE GetVolume(void) = 0;

  virtual HRESULT STDMETHODCALLTYPE SetVolume(
      /* [annotation][in] */
      _In_ double Volume) = 0;

  virtual BOOL STDMETHODCALLTYPE HasVideo(void) = 0;

  virtual BOOL STDMETHODCALLTYPE HasAudio(void) = 0;

  virtual HRESULT STDMETHODCALLTYPE GetNativeVideoSize(
      /* [annotation][out] */
      _Out_opt_ DWORD * cx,
      /* [annotation][out] */
      _Out_opt_ DWORD * cy) = 0;

  virtual HRESULT STDMETHODCALLTYPE GetVideoAspectRatio(
      /* [annotation][out] */
      _Out_opt_ DWORD * cx,
      /* [annotation][out] */
      _Out_opt_ DWORD * cy) = 0;

  virtual HRESULT STDMETHODCALLTYPE Shutdown(void) = 0;

  virtual HRESULT STDMETHODCALLTYPE TransferVideoFrame(
      /* [annotation][in] */
      _In_ IUnknown * pDstSurf,
      /* [annotation][in] */
      _In_opt_ const MFVideoNormalizedRect* pSrc,
      /* [annotation][in] */
      _In_ const RECT* pDst,
      /* [annotation][in] */
      _In_opt_ const MFARGB* pBorderClr) = 0;

  virtual HRESULT STDMETHODCALLTYPE OnVideoStreamTick(
      /* [annotation][out] */
      _Out_ LONGLONG * pPts) = 0;
};
#  endif /* __IMFMediaEngine_INTERFACE_DEFINED__ */

#endif  // extra class copy from mfmediaengine.h
#endif  // DOM_MEDIA_PLATFORM_WMF_MFMEDIAENGINENOTIFY_H
