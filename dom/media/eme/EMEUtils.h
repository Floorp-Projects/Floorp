/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef EME_LOG_H_
#define EME_LOG_H_

#include "mozilla/Logging.h"
#include "mozilla/dom/MediaKeyStatusMapBinding.h"
#include "mozilla/dom/MediaKeySystemAccessBinding.h"
#include "nsString.h"
#include "nsTArray.h"

namespace mozilla {

namespace dom {
class ArrayBufferViewOrArrayBuffer;
}

#ifndef EME_LOG
LogModule* GetEMELog();
#  define EME_LOG(...) \
    MOZ_LOG(GetEMELog(), mozilla::LogLevel::Debug, (__VA_ARGS__))
#  define EME_LOG_ENABLED() MOZ_LOG_TEST(GetEMELog(), mozilla::LogLevel::Debug)
#endif

#ifndef EME_VERBOSE_LOG
LogModule* GetEMEVerboseLog();
#  define EME_VERBOSE_LOG(...) \
    MOZ_LOG(GetEMEVerboseLog(), mozilla::LogLevel::Debug, (__VA_ARGS__))
#else
#  ifndef EME_LOG
#    define EME_LOG(...)
#  endif

#  ifndef EME_VERBOSE_LOG
#    define EME_VERBOSE_LOG(...)
#  endif
#endif

// Helper function to extract a copy of data coming in from JS in an
// (ArrayBuffer or ArrayBufferView) IDL typed function argument.
//
// Only call this on a properly initialized ArrayBufferViewOrArrayBuffer.
void CopyArrayBufferViewOrArrayBufferData(
    const dom::ArrayBufferViewOrArrayBuffer& aBufferOrView,
    nsTArray<uint8_t>& aOutData);

nsString KeySystemToProxyName(const nsAString& aKeySystem);

bool IsClearkeyKeySystem(const nsAString& aKeySystem);

bool IsWidevineKeySystem(const nsAString& aKeySystem);

#ifdef MOZ_WMF_CDM
bool IsPlayReadyKeySystemAndSupported(const nsAString& aKeySystem);

bool IsWidevineExperimentKeySystemAndSupported(const nsAString& aKeySystem);
#endif

// Note: Primetime is now unsupported, but we leave it in the enum so
// that the telemetry enum values are not changed; doing so would break
// existing telemetry probes.
enum CDMType {
  eClearKey = 0,
  ePrimetime = 1,  // Note: Unsupported.
  eWidevine = 2,
  eUnknown = 3
};

CDMType ToCDMTypeTelemetryEnum(const nsString& aKeySystem);

const char* ToMediaKeyStatusStr(dom::MediaKeyStatus aStatus);

// Return true if given config supports hardware decryption (SL3000 or L1).
bool IsHardwareDecryptionSupported(
    const dom::MediaKeySystemConfiguration& aConfig);

}  // namespace mozilla

#endif  // EME_LOG_H_
