/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/EMEUtils.h"

namespace mozilla {

PRLogModuleInfo* GetEMELog() {
  static PRLogModuleInfo* log = nullptr;
  if (!log) {
    log = PR_NewLogModule("EME");
  }
  return log;
}

PRLogModuleInfo* GetEMEVerboseLog() {
  static PRLogModuleInfo* log = nullptr;
  if (!log) {
    log = PR_NewLogModule("EMEV");
  }
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

static const char16_t* sKeySystems[] = {
  MOZ_UTF16("org.w3.clearkey"),
  MOZ_UTF16("com.adobe.access"),
  MOZ_UTF16("com.adobe.primetime"),
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

} // namespace mozilla
