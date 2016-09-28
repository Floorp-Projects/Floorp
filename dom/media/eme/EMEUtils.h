/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef EME_LOG_H_
#define EME_LOG_H_

#include "VideoUtils.h"
#include "mozilla/Logging.h"
#include "nsString.h"
#include "nsTArray.h"

namespace mozilla {

namespace dom {
class ArrayBufferViewOrArrayBuffer;
}

#ifndef EME_LOG
  LogModule* GetEMELog();
  #define EME_LOG(...) MOZ_LOG(GetEMELog(), mozilla::LogLevel::Debug, (__VA_ARGS__))
  #define EME_LOG_ENABLED() MOZ_LOG_TEST(GetEMELog(), mozilla::LogLevel::Debug)
#endif

#ifndef EME_VERBOSE_LOG
  LogModule* GetEMEVerboseLog();
  #define EME_VERBOSE_LOG(...) MOZ_LOG(GetEMEVerboseLog(), mozilla::LogLevel::Debug, (__VA_ARGS__))
#else
  #ifndef EME_LOG
    #define EME_LOG(...)
  #endif

  #ifndef EME_VERBOSE_LOG
    #define EME_VERBOSE_LOG(...)
  #endif
#endif

#define NO_CDM_VERSION -1

// Checks a keySystem string against a whitelist, and determines whether
// the keySystem is in the whitelist, and extracts the requested minimum
// CDM version.
//
// Format of EME keysystems:
// com.domain.keysystem[.minVersionAsInt]
// i.e. org.w3.clearkey, com.adobe.primetime.7
//
// Returns true if aKeySystem contains a valid keySystem which we support,
// false otherwise.
//
// On success, aOutKeySystem contains the keySystem string stripped of the
// min version number, and aOutMinCDMVersion contains the min version number
// if present. If it was not present, aOutMinCDMVersion is NO_CDM_VERSION.
bool ParseKeySystem(const nsAString& aKeySystem,
                    nsAString& aOutKeySystem,
                    int32_t& aOutMinCDMVersion);

// Helper function to extract a copy of data coming in from JS in an
// (ArrayBuffer or ArrayBufferView) IDL typed function argument.
//
// Only call this on a properly initialized ArrayBufferViewOrArrayBuffer.
void
CopyArrayBufferViewOrArrayBufferData(const dom::ArrayBufferViewOrArrayBuffer& aBufferOrView,
                                     nsTArray<uint8_t>& aOutData);

struct ArrayData {
  explicit ArrayData(const uint8_t* aData, size_t aLength)
    : mData(aData)
    , mLength(aLength)
  {
  }
  const uint8_t* mData;
  const size_t mLength;
  bool IsValid() const {
    return mData != nullptr && mLength != 0;
  }
  bool operator== (const nsTArray<uint8_t>& aOther) const {
    return mLength == aOther.Length() &&
           memcmp(mData, aOther.Elements(), mLength) == 0;
  }
};

// Helper function to extract data coming in from JS in an
// (ArrayBuffer or ArrayBufferView) IDL typed function argument.
//
// Be *very* careful with this!
//
// Only use returned ArrayData inside the lifetime of the
// ArrayBufferViewOrArrayBuffer; the ArrayData struct does not contain
// a copy of the data!
//
// And do *not* call out to anything that could call into JavaScript,
// while the ArrayData is live, as then all bets about the data not changing
// are off! No calls into JS, no calls into JS-implemented WebIDL or XPIDL,
// nothing. Beware!
//
// Only call this on a properly initialized ArrayBufferViewOrArrayBuffer.
ArrayData
GetArrayBufferViewOrArrayBufferData(const dom::ArrayBufferViewOrArrayBuffer& aBufferOrView);

nsString
KeySystemToGMPName(const nsAString& aKeySystem);

bool
IsClearkeyKeySystem(const nsAString& aKeySystem);

enum CDMType {
  eClearKey = 0,
  ePrimetime = 1,
  eWidevine = 2,
  eUnknown = 3
};

CDMType
ToCDMTypeTelemetryEnum(const nsString& aKeySystem);

} // namespace mozilla

#endif // EME_LOG_H_
