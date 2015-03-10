/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ClearKeyBase64.h"

#include <algorithm>

#include "mozilla/ArrayUtils.h"

using namespace std;

/**
* Take a base64-encoded string, convert (in-place) each character to its
* corresponding value in the [0x00, 0x3f] range, and truncate any padding.
*/
static bool
Decode6Bit(string& aStr)
{
  for (size_t i = 0; i < aStr.length(); i++) {
    if (aStr[i] >= 'A' && aStr[i] <= 'Z') {
      aStr[i] -= 'A';
    }
    else if (aStr[i] >= 'a' && aStr[i] <= 'z') {
      aStr[i] -= 'a' - 26;
    }
    else if (aStr[i] >= '0' && aStr[i] <= '9') {
      aStr[i] -= '0' - 52;
    }
    else if (aStr[i] == '-' || aStr[i] == '+') {
      aStr[i] = 62;
    }
    else if (aStr[i] == '_' || aStr[i] == '/') {
      aStr[i] = 63;
    }
    else {
      // Truncate '=' padding at the end of the aString.
      if (aStr[i] != '=') {
        aStr.erase(i, string::npos);
        return false;
      }
      aStr[i] = '\0';
      aStr.resize(i);
      break;
    }
  }

  return true;
}

bool
DecodeBase64KeyOrId(const string& aEncoded, vector<uint8_t>& aOutDecoded)
{
  string encoded = aEncoded;
  if (!Decode6Bit(encoded) ||
    encoded.size() != 22) { // Can't decode to 16 byte CENC key or keyId.
    return false;
  }

  // The number of bytes we haven't yet filled in the current byte, mod 8.
  int shift = 0;

  aOutDecoded.resize(16);
  vector<uint8_t>::iterator out = aOutDecoded.begin();
  for (size_t i = 0; i < encoded.length(); i++) {
    if (!shift) {
      *out = encoded[i] << 2;
    }
    else {
      *out |= encoded[i] >> (6 - shift);
      out++;
      if (out == aOutDecoded.end()) {
        // Hit last 6bit octed in encoded, which is padding and can be ignored.
        break;
      }
      *out = encoded[i] << (shift + 2);
    }
    shift = (shift + 2) % 8;
  }

  return true;
}