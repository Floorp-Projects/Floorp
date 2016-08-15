/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/EMEUtils.h"
#include "mozilla/dom/UnionTypes.h"

namespace mozilla {

LogModule* GetEMELog() {
  static LazyLogModule log("EME");
  return log;
}

LogModule* GetEMEVerboseLog() {
  static LazyLogModule log("EMEV");
  return log;
}

static bool
ContainsOnlyDigits(const nsAString& aString)
{
  nsAString::const_iterator iter, end;
  aString.BeginReading(iter);
  aString.EndReading(end);
  while (iter != end) {
    char16_t ch = *iter;
    if (ch < '0' || ch > '9') {
      return false;
    }
    iter++;
  }
  return true;
}

static bool
ParseKeySystem(const nsAString& aExpectedKeySystem,
               const nsAString& aInputKeySystem,
               int32_t& aOutCDMVersion)
{
  if (!StringBeginsWith(aInputKeySystem, aExpectedKeySystem)) {
    return false;
  }

  if (aInputKeySystem.Length() > aExpectedKeySystem.Length() + 8) {
    // Allow up to 8 bytes for the ".version" field. 8 bytes should
    // be enough for any versioning scheme...
    NS_WARNING("Input KeySystem including was suspiciously long");
    return false;
  }

  const char16_t* versionStart = aInputKeySystem.BeginReading() + aExpectedKeySystem.Length();
  const char16_t* end = aInputKeySystem.EndReading();
  if (versionStart == end) {
    // No version supplied with keysystem.
    aOutCDMVersion = NO_CDM_VERSION;
    return true;
  }
  if (*versionStart != '.') {
    // version not in correct format.
    NS_WARNING("EME keySystem version string not prefixed by '.'");
    return false;
  }
  versionStart++;
  const nsAutoString versionStr(Substring(versionStart, end));
  if (!ContainsOnlyDigits(versionStr)) {
    NS_WARNING("Non-digit character in EME keySystem string's version suffix");
    return false;
  }
  nsresult rv;
  int32_t version = versionStr.ToInteger(&rv);
  if (NS_FAILED(rv) || version < 0) {
    NS_WARNING("Invalid version in EME keySystem string");
    return false;
  }
  aOutCDMVersion = version;

  return true;
}

static const char16_t *const sKeySystems[] = {
  u"org.w3.clearkey",
  u"com.adobe.primetime",
  u"com.widevine.alpha",
};

bool
ParseKeySystem(const nsAString& aInputKeySystem,
               nsAString& aOutKeySystem,
               int32_t& aOutCDMVersion)
{
  for (const char16_t* keySystem : sKeySystems) {
    int32_t minCDMVersion = NO_CDM_VERSION;
    if (ParseKeySystem(nsDependentString(keySystem),
                       aInputKeySystem,
                       minCDMVersion)) {
      aOutKeySystem = keySystem;
      aOutCDMVersion = minCDMVersion;
      return true;
    }
  }
  return false;
}

ArrayData
GetArrayBufferViewOrArrayBufferData(const dom::ArrayBufferViewOrArrayBuffer& aBufferOrView)
{
  MOZ_ASSERT(aBufferOrView.IsArrayBuffer() || aBufferOrView.IsArrayBufferView());
  if (aBufferOrView.IsArrayBuffer()) {
    const dom::ArrayBuffer& buffer = aBufferOrView.GetAsArrayBuffer();
    buffer.ComputeLengthAndData();
    return ArrayData(buffer.Data(), buffer.Length());
  } else if (aBufferOrView.IsArrayBufferView()) {
    const dom::ArrayBufferView& bufferview = aBufferOrView.GetAsArrayBufferView();
    bufferview.ComputeLengthAndData();
    return ArrayData(bufferview.Data(), bufferview.Length());
  }
  return ArrayData(nullptr, 0);
}

void
CopyArrayBufferViewOrArrayBufferData(const dom::ArrayBufferViewOrArrayBuffer& aBufferOrView,
                                     nsTArray<uint8_t>& aOutData)
{
  ArrayData data = GetArrayBufferViewOrArrayBufferData(aBufferOrView);
  aOutData.Clear();
  if (!data.IsValid()) {
    return;
  }
  aOutData.AppendElements(data.mData, data.mLength);
}

nsString
KeySystemToGMPName(const nsAString& aKeySystem)
{
  if (!CompareUTF8toUTF16(kEMEKeySystemPrimetime, aKeySystem)) {
    return NS_LITERAL_STRING("gmp-eme-adobe");
  }
  if (!CompareUTF8toUTF16(kEMEKeySystemClearkey, aKeySystem)) {
    return NS_LITERAL_STRING("gmp-clearkey");
  }
  if (!CompareUTF8toUTF16(kEMEKeySystemWidevine, aKeySystem)) {
    return NS_LITERAL_STRING("gmp-widevinecdm");
  }
  MOZ_ASSERT(false, "We should only call this for known GMPs");
  return EmptyString();
}

bool
IsClearkeyKeySystem(const nsAString& aKeySystem)
{
  return !CompareUTF8toUTF16(kEMEKeySystemClearkey, aKeySystem);
}

} // namespace mozilla
