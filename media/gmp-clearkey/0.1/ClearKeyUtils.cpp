/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <algorithm>
#include <ctype.h>
#include <stdarg.h>
#include <stdint.h>
#include <vector>

#include "ClearKeyUtils.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/Assertions.h"
#include "mozilla/Endian.h"
#include "mozilla/NullPtr.h"
#include "openaes/oaes_lib.h"

using namespace std;

#define FOURCC(a,b,c,d) ((a << 24) + (b << 16) + (c << 8) + d)

// System ID identifying the cenc v2 pssh box format; specified at:
// https://dvcs.w3.org/hg/html-media/raw-file/tip/encrypted-media/cenc-format.html
const uint8_t kSystemID[] = {
  0x10, 0x77, 0xef, 0xec, 0xc0, 0xb2, 0x4d, 0x02,
  0xac, 0xe3, 0x3c, 0x1e, 0x52, 0xe2, 0xfb, 0x4b
};

void
CK_Log(const char* aFmt, ...)
{
  va_list ap;

  va_start(ap, aFmt);
  vprintf(aFmt, ap);
  va_end(ap);

  printf("\n");
}

static void
IncrementIV(vector<uint8_t>& aIV) {
  using mozilla::BigEndian;

  MOZ_ASSERT(aIV.size() == 16);
  BigEndian::writeUint64(&aIV[8], BigEndian::readUint64(&aIV[8]) + 1);
}

/* static */ void
ClearKeyUtils::DecryptAES(const vector<uint8_t>& aKey,
                          vector<uint8_t>& aData, vector<uint8_t>& aIV)
{
  MOZ_ASSERT(aIV.size() == CLEARKEY_KEY_LEN);
  MOZ_ASSERT(aKey.size() == CLEARKEY_KEY_LEN);

  OAES_CTX* aes = oaes_alloc();
  oaes_key_import_data(aes, &aKey[0], aKey.size());
  oaes_set_option(aes, OAES_OPTION_ECB, nullptr);

  for (size_t i = 0; i < aData.size(); i += CLEARKEY_KEY_LEN) {
    size_t encLen;
    oaes_encrypt(aes, &aIV[0], CLEARKEY_KEY_LEN, nullptr, &encLen);

    vector<uint8_t> enc(encLen);
    oaes_encrypt(aes, &aIV[0], CLEARKEY_KEY_LEN, &enc[0], &encLen);

    MOZ_ASSERT(encLen >= 2 * OAES_BLOCK_SIZE + CLEARKEY_KEY_LEN);
    size_t blockLen = std::min(aData.size() - i, CLEARKEY_KEY_LEN);
    for (size_t j = 0; j < blockLen; j++) {
      aData[i + j] ^= enc[2 * OAES_BLOCK_SIZE + j];
    }
    IncrementIV(aIV);
  }

  oaes_free(&aes);
}

/**
 * ClearKey expects all Key IDs to be base64 encoded with non-standard alphabet
 * and padding.
 */
static bool
EncodeBase64Web(vector<uint8_t> aBinary, string& aEncoded)
{
  const char sAlphabet[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
  const uint8_t sMask = 0x3f;

  aEncoded.resize((aBinary.size() * 8 + 5) / 6);

  // Pad binary data in case there's rubbish past the last byte.
  aBinary.push_back(0);

  // Number of bytes not consumed in the previous character
  uint32_t shift = 0;

  auto out = aEncoded.begin();
  auto data = aBinary.begin();
  for (string::size_type i = 0; i < aEncoded.length(); i++) {
    if (shift) {
      out[i] = (*data << (6 - shift)) & sMask;
      data++;
    } else {
      out[i] = 0;
    }

    out[i] += (*data >> (shift + 2)) & sMask;
    shift = (shift + 2) % 8;

    // Cast idx to size_t before using it as an array-index,
    // to pacify clang 'Wchar-subscripts' warning:
    size_t idx = static_cast<size_t>(out[i]);
    MOZ_ASSERT(idx < MOZ_ARRAY_LENGTH(sAlphabet),
               "out of bounds index for 'sAlphabet'");
    out[i] = sAlphabet[idx];
  }

  return true;
}

/* static */ void
ClearKeyUtils::ParseInitData(const uint8_t* aInitData, uint32_t aInitDataSize,
                             vector<KeyId>& aOutKeys)
{
  using mozilla::BigEndian;

  uint32_t size = 0;
  for (uint32_t offset = 0; offset + sizeof(uint32_t) < aInitDataSize; offset += size) {
    const uint8_t* data = aInitData + offset;
    size = BigEndian::readUint32(data); data += sizeof(uint32_t);

    CK_LOGD("Looking for pssh at offset %u", offset);

    if (size + offset > aInitDataSize) {
      CK_LOGE("Box size %u overflows init data buffer", size);
      return;
    }

    if (size < 36) {
      // Too small to be a cenc2 pssh box
      continue;
    }

    uint32_t box = BigEndian::readUint32(data); data += sizeof(uint32_t);
    if (box != FOURCC('p','s','s','h')) {
      CK_LOGE("ClearKey CDM passed non-pssh initData");
      return;
    }

    uint32_t head = BigEndian::readUint32(data); data += sizeof(uint32_t);
    CK_LOGD("Got version %u pssh box, length %u", head & 0xff, size);

    if ((head >> 24) != 1) {
      // Ignore pssh boxes with wrong version
      CK_LOGD("Ignoring pssh box with wrong version");
      continue;
    }

    if (memcmp(kSystemID, data, sizeof(kSystemID))) {
      // Ignore pssh boxes with wrong system ID
      continue;
    }
    data += sizeof(kSystemID);

    uint32_t kidCount = BigEndian::readUint32(data); data += sizeof(uint32_t);
    if (data + kidCount * CLEARKEY_KEY_LEN > aInitData + aInitDataSize) {
      CK_LOGE("pssh key IDs overflow init data buffer");
      return;
    }

    for (uint32_t i = 0; i < kidCount; i++) {
      aOutKeys.push_back(KeyId(data, data + CLEARKEY_KEY_LEN));
      data += CLEARKEY_KEY_LEN;
    }
  }
}

/* static */ void
ClearKeyUtils::MakeKeyRequest(const vector<KeyId>& aKeyIDs,
                              string& aOutRequest,
                              GMPSessionType aSessionType)
{
  MOZ_ASSERT(aKeyIDs.size() && aOutRequest.empty());

  aOutRequest.append("{ \"kids\":[");
  for (size_t i = 0; i < aKeyIDs.size(); i++) {
    if (i) {
      aOutRequest.append(",");
    }
    aOutRequest.append("\"");

    string base64key;
    EncodeBase64Web(aKeyIDs[i], base64key);
    aOutRequest.append(base64key);

    aOutRequest.append("\"");
  }
  aOutRequest.append("], \"type\":");

  aOutRequest.append("\"");
  aOutRequest.append(SessionTypeToString(aSessionType));
  aOutRequest.append("\"}");
}

#define EXPECT_SYMBOL(CTX, X) do { \
  if (GetNextSymbol(CTX) != (X)) { \
    CK_LOGE("Unexpected symbol in JWK parser"); \
    return false; \
  } \
} while (false)

struct ParserContext {
  const uint8_t* mIter;
  const uint8_t* mEnd;
};

static uint8_t
PeekSymbol(ParserContext& aCtx)
{
  for (; aCtx.mIter < aCtx.mEnd; (aCtx.mIter)++) {
    if (!isspace(*aCtx.mIter)) {
      return *aCtx.mIter;
    }
  }

  return 0;
}

static uint8_t
GetNextSymbol(ParserContext& aCtx)
{
  uint8_t sym = PeekSymbol(aCtx);
  aCtx.mIter++;
  return sym;
}

static bool SkipToken(ParserContext& aCtx);

static bool
SkipString(ParserContext& aCtx)
{
  EXPECT_SYMBOL(aCtx, '"');
  for (uint8_t sym = GetNextSymbol(aCtx); sym; sym = GetNextSymbol(aCtx)) {
    if (sym == '\\') {
      sym = GetNextSymbol(aCtx);
    } else if (sym == '"') {
      return true;
    }
  }

  return false;
}

/**
 * Skip whole object and values it contains.
 */
static bool
SkipObject(ParserContext& aCtx)
{
  EXPECT_SYMBOL(aCtx, '{');

  if (PeekSymbol(aCtx) == '}') {
    GetNextSymbol(aCtx);
    return true;
  }

  while (true) {
    if (!SkipString(aCtx)) return false;
    EXPECT_SYMBOL(aCtx, ':');
    if (!SkipToken(aCtx)) return false;

    if (PeekSymbol(aCtx) == '}') {
      GetNextSymbol(aCtx);
      return true;
    }
    EXPECT_SYMBOL(aCtx, ',');
  }

  return false;
}

/**
 * Skip array value and the values it contains.
 */
static bool
SkipArray(ParserContext& aCtx)
{
  EXPECT_SYMBOL(aCtx, '[');

  if (PeekSymbol(aCtx) == ']') {
    GetNextSymbol(aCtx);
    return true;
  }

  while (SkipToken(aCtx)) {
    if (PeekSymbol(aCtx) == ']') {
      GetNextSymbol(aCtx);
      return true;
    }
    EXPECT_SYMBOL(aCtx, ',');
  }

  return false;
}

/**
 * Skip unquoted literals like numbers, |true|, and |null|.
 * (XXX and anything else that matches /([:alnum:]|[+-.])+/)
 */
static bool
SkipLiteral(ParserContext& aCtx)
{
  for (; aCtx.mIter < aCtx.mEnd; aCtx.mIter++) {
    if (!isalnum(*aCtx.mIter) &&
        *aCtx.mIter != '.' && *aCtx.mIter != '-' && *aCtx.mIter != '+') {
      return true;
    }
  }

  return false;
}

static bool
SkipToken(ParserContext& aCtx)
{
  uint8_t startSym = PeekSymbol(aCtx);
  if (startSym == '"') {
    CK_LOGD("JWK parser skipping string");
    return SkipString(aCtx);
  } else if (startSym == '{') {
    CK_LOGD("JWK parser skipping object");
    return SkipObject(aCtx);
  } else if (startSym == '[') {
    CK_LOGD("JWK parser skipping array");
    return SkipArray(aCtx);
  } else {
    CK_LOGD("JWK parser skipping literal");
    return SkipLiteral(aCtx);
  }

  return false;
}

static bool
GetNextLabel(ParserContext& aCtx, string& aOutLabel)
{
  EXPECT_SYMBOL(aCtx, '"');

  const uint8_t* start = aCtx.mIter;
  for (uint8_t sym = GetNextSymbol(aCtx); sym; sym = GetNextSymbol(aCtx)) {
    if (sym == '\\') {
      GetNextSymbol(aCtx);
      continue;
    }

    if (sym == '"') {
      aOutLabel.assign(start, aCtx.mIter - 1);
      return true;
    }
  }

  return false;
}

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
    } else if (aStr[i] >= 'a' && aStr[i] <= 'z') {
      aStr[i] -= 'a' - 26;
    } else if (aStr[i] >= '0' && aStr[i] <= '9') {
      aStr[i] -= '0' - 52;
    } else if (aStr[i] == '-' || aStr[i] == '+') {
      aStr[i] = 62;
    } else if (aStr[i] == '_' || aStr[i] == '/') {
      aStr[i] = 63;
    } else {
      // Truncate '=' padding at the end of the aString.
      if (aStr[i] != '=') {
        return false;
      }
      aStr[i] = '\0';
      aStr.resize(i);
      break;
    }
  }

  return true;
}

static bool
DecodeBase64(string& aEncoded, vector<uint8_t>& aOutDecoded)
{
  if (!Decode6Bit(aEncoded)) {
    return false;
  }

  // The number of bytes we haven't yet filled in the current byte, mod 8.
  int shift = 0;

  aOutDecoded.resize(aEncoded.length() * 6 / 8);
  aOutDecoded.reserve(aEncoded.length() * 6 / 8 + 1);
  auto out = aOutDecoded.begin();
  for (size_t i = 0; i < aEncoded.length(); i++) {
    if (!shift) {
      *out = aEncoded[i] << 2;
    } else {
      *out |= aEncoded[i] >> (6 - shift);
      *(++out) = aEncoded[i] << (shift + 2);
    }
    shift = (shift + 2) % 8;
  }

  return true;
}

static bool
DecodeKey(string& aEncoded, Key& aOutDecoded)
{
  return DecodeBase64(aEncoded, aOutDecoded) &&
    // Key should be 128 bits long.
    aOutDecoded.size() == CLEARKEY_KEY_LEN;
}

static bool
ParseKeyObject(ParserContext& aCtx, KeyIdPair& aOutKey)
{
  EXPECT_SYMBOL(aCtx, '{');

  // Reject empty objects as invalid licenses.
  if (PeekSymbol(aCtx) == '}') {
    GetNextSymbol(aCtx);
    return false;
  }

  string keyId;
  string key;

  while (true) {
    string label;
    string value;

    if (!GetNextLabel(aCtx, label)) {
      return false;
    }

    EXPECT_SYMBOL(aCtx, ':');
    if (label == "kty") {
      if (!GetNextLabel(aCtx, value)) return false;
      // By spec, type must be "oct".
      if (value != "oct") return false;
    } else if (label == "alg") {
      if (!GetNextLabel(aCtx, value)) return false;
      // By spec, alg must be "A128KW".
      if (value != "A128KW") return false;
    } else if (label == "k" && PeekSymbol(aCtx) == '"') {
      // if this isn't a string we will fall through to the SkipToken() path.
      if (!GetNextLabel(aCtx, key)) return false;
    } else if (label == "kid" && PeekSymbol(aCtx) == '"') {
      if (!GetNextLabel(aCtx, keyId)) return false;
    } else {
      if (!SkipToken(aCtx)) return false;
    }

    uint8_t sym = PeekSymbol(aCtx);
    if (!sym || sym == '}') {
      break;
    }
    EXPECT_SYMBOL(aCtx, ',');
  }

  return !key.empty() &&
         !keyId.empty() &&
         DecodeBase64(keyId, aOutKey.mKeyId) &&
         DecodeKey(key, aOutKey.mKey) &&
         GetNextSymbol(aCtx) == '}';
}

static bool
ParseKeys(ParserContext& aCtx, vector<KeyIdPair>& aOutKeys)
{
  // Consume start of array.
  EXPECT_SYMBOL(aCtx, '[');

  while (true) {
    KeyIdPair key;
    if (!ParseKeyObject(aCtx, key)) {
      CK_LOGE("Failed to parse key object");
      return false;
    }

    MOZ_ASSERT(!key.mKey.empty() && !key.mKeyId.empty());
    aOutKeys.push_back(key);

    uint8_t sym = PeekSymbol(aCtx);
    if (!sym || sym == ']') {
      break;
    }

    EXPECT_SYMBOL(aCtx, ',');
  }

  return GetNextSymbol(aCtx) == ']';
}

/* static */ bool
ClearKeyUtils::ParseJWK(const uint8_t* aKeyData, uint32_t aKeyDataSize,
                        vector<KeyIdPair>& aOutKeys,
                        GMPSessionType aSessionType)
{
  ParserContext ctx;
  ctx.mIter = aKeyData;
  ctx.mEnd = aKeyData + aKeyDataSize;

  // Consume '{' from start of object.
  EXPECT_SYMBOL(ctx, '{');

  while (true) {
    string label;
    // Consume member key.
    if (!GetNextLabel(ctx, label)) return false;
    EXPECT_SYMBOL(ctx, ':');

    if (label == "keys") {
      // Parse "keys" array.
      if (!ParseKeys(ctx, aOutKeys)) return false;
    } else if (label == "type") {
      // Consume type string.
      string type;
      if (!GetNextLabel(ctx, type)) return false;
      if (type != SessionTypeToString(aSessionType)) {
        return false;
      }
    } else {
      SkipToken(ctx);
    }

    // Check for end of object.
    if (PeekSymbol(ctx) == '}') {
      break;
    }

    // Consume ',' between object members.
    EXPECT_SYMBOL(ctx, ',');
  }

  // Consume '}' from end of object.
  EXPECT_SYMBOL(ctx, '}');

  return true;
}

/* static */ const char*
ClearKeyUtils::SessionTypeToString(GMPSessionType aSessionType)
{
  switch (aSessionType) {
    case kGMPTemporySession: return "temporary";
    case kGMPPersistentSession: return "persistent";
    default: {
      MOZ_ASSERT(false, "Should not reach here.");
      return "invalid";
    }
  }
}

/* static */ bool
ClearKeyUtils::IsValidSessionId(const char* aBuff, uint32_t aLength)
{
  if (aLength > 10) {
    // 10 is the max number of characters in UINT32_MAX when
    // represented as a string; ClearKey session ids are integers.
    return false;
  }
  for (uint32_t i = 0; i < aLength; i++) {
    if (!isdigit(aBuff[i])) {
      return false;
    }
  }
  return true;
}
