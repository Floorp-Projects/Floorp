/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <cmath>

#include "GeolocationUtil.h"
#include "nsContentUtils.h"

namespace mozilla {

double CalculateDeltaInMeter(double aLat, double aLon, double aLastLat, double aLastLon)
{
  // Use spherical law of cosines to calculate difference
  // Not quite as correct as the Haversine but simpler and cheaper
  const double radsInDeg = M_PI / 180.0;
  const double rNewLat = aLat * radsInDeg;
  const double rNewLon = aLon * radsInDeg;
  const double rOldLat = aLastLat * radsInDeg;
  const double rOldLon = aLastLon * radsInDeg;
  // WGS84 equatorial radius of earth = 6378137m
  double cosDelta = (sin(rNewLat) * sin(rOldLat)) +
                    (cos(rNewLat) * cos(rOldLat) * cos(rOldLon - rNewLon));
  if (cosDelta > 1.0) {
    cosDelta = 1.0;
  } else if (cosDelta < -1.0) {
    cosDelta = -1.0;
  }
  return acos(cosDelta) * 6378137;
}


bool
DecodeGsmDefaultToString(UniquePtr<char[]> &aMessage, nsString &aWideStr)
{
  if (!aMessage || !aMessage.get()) {
    return false;
  }
  int lengthBytes = strlen(aMessage.get());
  if (lengthBytes <= 0) {
    return false;
  }
  int lengthSeptets = (lengthBytes*8)/7;
  if (lengthBytes % 7 == 0) {
    char PADDING_CHAR = 0x00;
    if(aMessage[lengthBytes-1] >> 1 == PADDING_CHAR) {
      lengthSeptets -=1;
    }
  }

  bool prevCharWasEscape = false;
  nsString languageTableToChar = NS_LITERAL_STRING(
    /**
     * National Language Identifier: 0x00
     * 6.2.1 GSM 7 bit Default Alphabet
     */
    // 01.....23.....4.....5.....6.....7.....8.....9.....A.B.....C.....D.E.....F.....
      "@\u00a3$\u00a5\u00e8\u00e9\u00f9\u00ec\u00f2\u00c7\n\u00d8\u00f8\r\u00c5\u00e5"
     //0.....12.....3.....4.....5.....6.....7.....8.....9.....A.....BC.....D.....E.....F.....
      "\u0394_\u03a6\u0393\u039b\u03a9\u03a0\u03a8\u03a3\u0398\u039e?\u00c6\u00e6\u00df\u00c9"
    // 012.34.....56789ABCDEF
      " !\"#\u00a4%&'()*+,-./"
    // 0123456789ABCDEF
      "0123456789:;<=>?"
    // 0.....123456789ABCDEF
      "\u00a1ABCDEFGHIJKLMNO"
    // 0123456789AB.....C.....D.....E.....F.....
      "PQRSTUVWXYZ\u00c4\u00d6\u00d1\u00dc\u00a7"
    // 0.....123456789ABCDEF
      "\u00bfabcdefghijklmno"
    // 0123456789AB.....C.....D.....E.....F.....
      "pqrstuvwxyz\u00e4\u00f6\u00f1\u00fc\u00e0");

  nsString shiftTableToChar = NS_LITERAL_STRING(
    /**
     * National Language Identifier: 0x00
     * 6.2.1.1 GSM 7 bit default alphabet extension table
     */
    // 0123456789A.....BCD.....EF
      "          \u000c  \ufffe  "
    // 0123456789AB.....CDEF
      "    ^      \uffff    "
    // 0123456789ABCDEF.
      "        {}     \\"
    // 0123456789ABCDEF
      "            [~] "
    // 0123456789ABCDEF
      "|               "
    // 0123456789ABCDEF
      "                "
    // 012345.....6789ABCDEF
      "     \u20ac          "
    // 0123456789ABCDEF
      "                ");

  nsString output;
  for (int i = 0; i < lengthSeptets ; i++) {
    int bitOffset = 7 * i;
    int byteOffset = bitOffset / 8;
    int shift = bitOffset % 8;
    int gsmVal;

    gsmVal = 0x7f & (aMessage[byteOffset] >> shift);
    if (shift > 1) {
      //set msb bits to 0
      gsmVal &= 0x7f >> (shift-1);
      gsmVal |= 0x7f & (aMessage[byteOffset+1] << (8-shift));
    }

    const char GSM_EXTENDED_ESCAPE = 0x1B;
    if(prevCharWasEscape) {
      if (gsmVal == GSM_EXTENDED_ESCAPE) {
        output.AppendLiteral(" ");
      } else {
        char16_t c = shiftTableToChar.CharAt(gsmVal);
        if (c == MOZ_UTF16(' ')) {
          c = languageTableToChar.CharAt(gsmVal);
        }
        output += c;
        }
        prevCharWasEscape = false;
    } else if(gsmVal == GSM_EXTENDED_ESCAPE) {
      prevCharWasEscape = true;
    } else {
      char16_t c = languageTableToChar.CharAt(gsmVal);
      output += c;
    }
  }

  aWideStr = output;
  return true;
}

nsString
DecodeNIString(const char* aMessage, GpsNiEncodingType aEncodingType)
{
  nsString wide_str;
  bool little_endian = true;
  int starthexIndex = 0, byteIndex = 0;

  // convert to byte string first
  int hexstring_length = strlen(aMessage);
  // Note: hexstring_length cannot be an odd number.
  if ((hexstring_length == 0) || (hexstring_length % 2 != 0)) {
    nsContentUtils::LogMessageToConsole(
      "DecodeNIString - Failed to decode string(length %d)", hexstring_length);
    wide_str.AssignLiteral("Failed to decode string.");
    return wide_str;
  }

  int byteArrayLen = (hexstring_length / 2);
  if (aEncodingType == GPS_ENC_SUPL_UTF8 || aEncodingType == GPS_ENC_SUPL_GSM_DEFAULT) {
    byteArrayLen += 1; //for string terminator
  } else if (aEncodingType == GPS_ENC_SUPL_UCS2) {
    byteArrayLen += 2; //for string terminator
    // Determine endianess of the data
    if (hexstring_length >= 4) {
      const char bom_be[] = "FEFF"; // Byte-Order Mark in big endian
      const char bom_le[] = "FFFE"; // Byte-Order Mark in little endian
      if (memcmp(bom_be, aMessage, sizeof(bom_be) - 1) == 0) {
        little_endian = false; // big endian detected
        starthexIndex += 4; // skip the Byte-Order Mark
      } else if (memcmp(bom_le, aMessage, sizeof(bom_le) - 1) == 0) {
        starthexIndex += 4; // skip the Byte-Order Mark
      }
    }
  }

  UniquePtr<char[]> byteArray = MakeUnique<char[]>(byteArrayLen);

  // In this loop we have to read 4 bytes at a time so as to take care of
  // endianess while converting hex string to byte array.
  // If the hexstring_length is not divisble by 4, we still take care of
  // remaining bytes outside the for loop.
  for (; starthexIndex <= hexstring_length - 4; starthexIndex += 4, byteIndex += 2) {
    char hexArray1[3] = {aMessage[starthexIndex], aMessage[starthexIndex + 1], '\0'};
    char hexArray2[3] = {aMessage[starthexIndex + 2], aMessage[starthexIndex + 3], '\0'};
    if (little_endian) {
      byteArray[byteIndex] = (char) strtol(hexArray1, nullptr, 16);
      byteArray[byteIndex + 1] = (char) strtol(hexArray2, nullptr, 16);
    } else {
      byteArray[byteIndex] = (char) strtol(hexArray2, nullptr, 16);
      byteArray[byteIndex + 1] = (char) strtol(hexArray1, nullptr, 16);
    }
  }

  if (starthexIndex <= hexstring_length - 2) {
    char hexArray[3] = {aMessage[starthexIndex], aMessage[starthexIndex + 1],'\0'};
    byteArray[byteIndex] = (char) strtol(hexArray, nullptr, 16);
  }

  // decode byte string by different encodings (support UTF8, USC2, GSM_DEFAULT)
  if (aEncodingType == GPS_ENC_SUPL_UTF8) {
    nsCString str(byteArray.get());
    wide_str.Append(NS_ConvertUTF8toUTF16(str));
  } else if (aEncodingType == GPS_ENC_SUPL_UCS2) {
    wide_str.Append((const char16_t*)byteArray.get());
  } else if (aEncodingType == GPS_ENC_SUPL_GSM_DEFAULT) {
    if (!DecodeGsmDefaultToString(byteArray, wide_str)) {
      nsContentUtils::LogMessageToConsole("DecodeNIString - Decode Failed!");
      wide_str.AssignLiteral("Failed to decode string");
    }
  } else {
    nsContentUtils::LogMessageToConsole("DecodeNIString - UnknownEncoding.");
    wide_str.AssignLiteral("Failed to decode string");
  }

  // The  |NI message|  is composed by "ni_id, text, requestor_id".
  // We need to distinguish the comma in  |NI message|  from the comma
  // in text or requestor_id. After discussion, we use escape to do so.
  wide_str.ReplaceSubstring(MOZ_UTF16("\\"), MOZ_UTF16("\\\\"));
  wide_str.ReplaceSubstring(MOZ_UTF16(","), MOZ_UTF16("\\,"));
  return wide_str;
}

} // namespace mozilla
