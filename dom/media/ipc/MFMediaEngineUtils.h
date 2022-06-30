/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_IPC_MFMEDIAENGINEUTILS_H_
#define DOM_MEDIA_IPC_MFMEDIAENGINEUTILS_H_

#include "MFMediaEngineExtra.h"
#include "ipc/EnumSerializer.h"
#include "mozilla/Logging.h"

namespace mozilla {

inline LazyLogModule gMFMediaEngineLog{"MFMediaEngine"};

// https://docs.microsoft.com/en-us/windows/win32/api/mfmediaengine/ne-mfmediaengine-mf_media_engine_event
using MFMediaEngineEvent = MF_MEDIA_ENGINE_EVENT;

// https://docs.microsoft.com/en-us/windows/win32/api/mfmediaengine/ne-mfmediaengine-mf_media_engine_err
using MFMediaEngineError = MF_MEDIA_ENGINE_ERR;

#define LOG_AND_WARNING(msg, ...)                                \
  do {                                                           \
    NS_WARNING(nsPrintfCString(msg, rv).get());                  \
    MOZ_LOG(gMFMediaEngineLog, LogLevel::Debug,                  \
            ("%s:%d, " msg, __FILE__, __LINE__, ##__VA_ARGS__)); \
  } while (false)

#ifndef RETURN_IF_FAILED
#  define RETURN_IF_FAILED(x)                           \
    do {                                                \
      HRESULT rv = x;                                   \
      if (MOZ_UNLIKELY(FAILED(rv))) {                   \
        LOG_AND_WARNING("(" #x ") failed, rv=%lx", rv); \
        return rv;                                      \
      }                                                 \
    } while (false)
#endif

#ifndef RETURN_VOID_IF_FAILED
#  define RETURN_VOID_IF_FAILED(x)                      \
    do {                                                \
      HRESULT rv = x;                                   \
      if (MOZ_UNLIKELY(FAILED(rv))) {                   \
        LOG_AND_WARNING("(" #x ") failed, rv=%lx", rv); \
        return;                                         \
      }                                                 \
    } while (false)
#endif

const char* MediaEventTypeToStr(MediaEventType aType);
const char* GUIDToStr(GUID aGUID);
const char* MFVideoRotationFormatToStr(MFVideoRotationFormat aFormat);
const char* MFVideoTransferFunctionToStr(MFVideoTransferFunction aFunc);
const char* MFVideoPrimariesToStr(MFVideoPrimaries aPrimaries);

}  // namespace mozilla

#endif  // DOM_MEDIA_IPC_MFMEDIAENGINECHILD_H_
