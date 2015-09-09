/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DaemonSocketPDUHelpers.h"
#include <limits>

#ifdef CHROMIUM_LOG
#undef CHROMIUM_LOG
#endif

#if defined(MOZ_WIDGET_GONK)
#include <android/log.h>
#define CHROMIUM_LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "I/O", args);
#else
#include <stdio.h>
#define IODEBUG true
#define CHROMIUM_LOG(args...) if (IODEBUG) printf(args);
#endif

namespace mozilla {
namespace ipc {
namespace DaemonSocketPDUHelpers {

//
// Conversion
//

nsresult
Convert(bool aIn, uint8_t& aOut)
{
  static const uint8_t sValue[] = {
    [false] = 0x00,
    [true] = 0x01
  };
  if (NS_WARN_IF(aIn >= MOZ_ARRAY_LENGTH(sValue))) {
    aOut = 0;
    return NS_ERROR_ILLEGAL_VALUE;
  }
  aOut = sValue[aIn];
  return NS_OK;
}

nsresult
Convert(bool aIn, int32_t& aOut)
{
  uint8_t out;
  nsresult rv = Convert(aIn, out);
  if (NS_FAILED(rv)) {
    out = 0; // silence compiler warning
    return rv;
  }
  aOut = static_cast<int32_t>(out);
  return NS_OK;
}

nsresult
Convert(int aIn, uint8_t& aOut)
{
  if (NS_WARN_IF(aIn < std::numeric_limits<uint8_t>::min()) ||
      NS_WARN_IF(aIn > std::numeric_limits<uint8_t>::max())) {
    aOut = 0; // silences compiler warning
    return NS_ERROR_ILLEGAL_VALUE;
  }
  aOut = static_cast<uint8_t>(aIn);
  return NS_OK;
}

nsresult
Convert(int aIn, int16_t& aOut)
{
  if (NS_WARN_IF(aIn < std::numeric_limits<int16_t>::min()) ||
      NS_WARN_IF(aIn > std::numeric_limits<int16_t>::max())) {
    aOut = 0; // silences compiler warning
    return NS_ERROR_ILLEGAL_VALUE;
  }
  aOut = static_cast<int16_t>(aIn);
  return NS_OK;
}

nsresult
Convert(int aIn, int32_t& aOut)
{
  if (NS_WARN_IF(aIn < std::numeric_limits<int32_t>::min()) ||
      NS_WARN_IF(aIn > std::numeric_limits<int32_t>::max())) {
    aOut = 0; // silences compiler warning
    return NS_ERROR_ILLEGAL_VALUE;
  }
  aOut = static_cast<int32_t>(aIn);
  return NS_OK;
}

nsresult
Convert(uint8_t aIn, bool& aOut)
{
  static const bool sBool[] = {
    [0x00] = false,
    [0x01] = true
  };
  if (NS_WARN_IF(aIn >= MOZ_ARRAY_LENGTH(sBool))) {
    return NS_ERROR_ILLEGAL_VALUE;
  }
  aOut = sBool[aIn];
  return NS_OK;
}

nsresult
Convert(uint8_t aIn, char& aOut)
{
  aOut = static_cast<char>(aIn);
  return NS_OK;
}

nsresult
Convert(uint8_t aIn, int& aOut)
{
  aOut = static_cast<int>(aIn);
  return NS_OK;
}

nsresult
Convert(uint8_t aIn, unsigned long& aOut)
{
  aOut = static_cast<unsigned long>(aIn);
  return NS_OK;
}

nsresult
Convert(uint32_t aIn, int& aOut)
{
  aOut = static_cast<int>(aIn);
  return NS_OK;
}

nsresult
Convert(uint32_t aIn, uint8_t& aOut)
{
  if (NS_WARN_IF(aIn < std::numeric_limits<uint8_t>::min()) ||
      NS_WARN_IF(aIn > std::numeric_limits<uint8_t>::max())) {
    aOut = 0; // silences compiler warning
    return NS_ERROR_ILLEGAL_VALUE;
  }
  aOut = static_cast<uint8_t>(aIn);
  return NS_OK;
}

nsresult
Convert(size_t aIn, uint16_t& aOut)
{
  if (NS_WARN_IF(aIn >= (1ul << 16))) {
    aOut = 0; // silences compiler warning
    return NS_ERROR_ILLEGAL_VALUE;
  }
  aOut = static_cast<uint16_t>(aIn);
  return NS_OK;
}

//
// Packing
//

nsresult
PackPDU(bool aIn, DaemonSocketPDU& aPDU)
{
  return PackPDU(PackConversion<bool, uint8_t>(aIn), aPDU);
}

nsresult
PackPDU(const DaemonSocketPDUHeader& aIn, DaemonSocketPDU& aPDU)
{
  nsresult rv = PackPDU(aIn.mService, aPDU);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = PackPDU(aIn.mOpcode, aPDU);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = PackPDU(aIn.mLength, aPDU);
  if (NS_FAILED(rv)) {
    return rv;
  }
  return NS_OK;
}

//
// Unpacking
//

nsresult
UnpackPDU(DaemonSocketPDU& aPDU, bool& aOut)
{
  return UnpackPDU(aPDU, UnpackConversion<uint8_t, bool>(aOut));
}

nsresult
UnpackPDU(DaemonSocketPDU& aPDU, char& aOut)
{
  return UnpackPDU(aPDU, UnpackConversion<uint8_t, char>(aOut));
}

nsresult
UnpackPDU(DaemonSocketPDU& aPDU, nsDependentCString& aOut)
{
  // We get a pointer to the first character in the PDU, a length
  // of 1 ensures we consume the \0 byte. With 'str' pointing to
  // the string in the PDU, we can copy the actual bytes.

  const char* str = reinterpret_cast<const char*>(aPDU.Consume(1));
  if (NS_WARN_IF(!str)) {
    return NS_ERROR_ILLEGAL_VALUE; // end of PDU
  }

  const char* end = static_cast<char*>(memchr(str, '\0', aPDU.GetSize() + 1));
  if (NS_WARN_IF(!end)) {
    return NS_ERROR_ILLEGAL_VALUE; // no string terminator
  }

  ptrdiff_t len = end - str;

  const uint8_t* rest = aPDU.Consume(len);
  if (NS_WARN_IF(!rest)) {
    // We couldn't consume bytes that should have been there.
    return NS_ERROR_ILLEGAL_VALUE;
  }

  aOut.Rebind(str, len);

  return NS_OK;
}

nsresult
UnpackPDU(DaemonSocketPDU& aPDU, const UnpackCString0& aOut)
{
  nsDependentCString cstring;

  nsresult rv = UnpackPDU(aPDU, cstring);
  if (NS_FAILED(rv)) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  aOut.mString->AssignASCII(cstring.get(), cstring.Length());

  return NS_OK;
}

nsresult
UnpackPDU(DaemonSocketPDU& aPDU, const UnpackString0& aOut)
{
  nsDependentCString cstring;

  nsresult rv = UnpackPDU(aPDU, cstring);
  if (NS_FAILED(rv)) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  *aOut.mString = NS_ConvertUTF8toUTF16(cstring);

  return NS_OK;
}

//
// Init operators
//

void
PDUInitOp::WarnAboutTrailingData() const
{
  size_t size = mPDU->GetSize();

  if (MOZ_LIKELY(!size)) {
    return;
  }

  uint8_t service, opcode;
  uint16_t payloadSize;
  mPDU->GetHeader(service, opcode, payloadSize);

  CHROMIUM_LOG(
    "Unpacked PDU of type (%x,%x) still contains %zu Bytes of data.",
    service, opcode, size);
}

} // namespace DaemonSocketPDUHelpers
} // namespace ipc
} // namespace mozilla
