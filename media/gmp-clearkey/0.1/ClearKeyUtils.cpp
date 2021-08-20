/*
 * Copyright 2015, Mozilla Foundation and contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "ClearKeyUtils.h"

#include <assert.h>
#include <ctype.h>
#include <memory.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <algorithm>
#include <cctype>
#include <memory>
#include <sstream>
#include <vector>

#include "pk11pub.h"
#include "prerror.h"
#include "secmodt.h"

#include "ArrayUtils.h"
#include "BigEndian.h"
#include "ClearKeyBase64.h"
#include "mozilla/Sprintf.h"
#include "psshparser/PsshParser.h"

using namespace cdm;
using std::string;
using std::stringstream;
using std::vector;

struct DeleteHelper {
  void operator()(PK11Context* value) { PK11_DestroyContext(value, true); }
  void operator()(PK11SlotInfo* value) { PK11_FreeSlot(value); }
  void operator()(PK11SymKey* value) { PK11_FreeSymKey(value); }
};

template <class T>
struct MaybeDeleteHelper {
  void operator()(T* ptr) {
    if (ptr) {
      DeleteHelper del;
      del(ptr);
    }
  }
};

void CK_Log(const char* aFmt, ...) {
  FILE* out = stdout;

  if (getenv("CLEARKEY_LOG_FILE")) {
    out = fopen(getenv("CLEARKEY_LOG_FILE"), "a");
  }

  va_list ap;

  va_start(ap, aFmt);
  const size_t len = 1024;
  char buf[len];
  VsprintfLiteral(buf, aFmt, ap);
  va_end(ap);

  fprintf(out, "%s\n", buf);
  fflush(out);

  if (out != stdout) {
    fclose(out);
  }
}

static bool PrintableAsString(const uint8_t* aBytes, uint32_t aLength) {
  return std::all_of(aBytes, aBytes + aLength,
                     [](uint8_t c) { return isprint(c) == 1; });
}

void CK_LogArray(const char* prepend, const uint8_t* aData,
                 const uint32_t aDataSize) {
  // If the data is valid ascii, use that. Otherwise print the hex
  string data = PrintableAsString(aData, aDataSize)
                    ? string(aData, aData + aDataSize)
                    : ClearKeyUtils::ToHexString(aData, aDataSize);

  CK_LOGD("%s%s", prepend, data.c_str());
}

/* static */
bool ClearKeyUtils::DecryptCbcs(const vector<uint8_t>& aKey,
                                const vector<uint8_t>& aIV,
                                mozilla::Span<uint8_t> aSubsample,
                                uint32_t aCryptByteBlock,
                                uint32_t aSkipByteBlock) {
  assert(aKey.size() == CENC_KEY_LEN);
  assert(aIV.size() == CENC_KEY_LEN);
  assert(aCryptByteBlock <= 0xFF);
  assert(aSkipByteBlock <= 0xFF);

  std::unique_ptr<PK11SlotInfo, MaybeDeleteHelper<PK11SlotInfo>> slot(
      PK11_GetInternalKeySlot());

  if (!slot.get()) {
    CK_LOGE("Failed to get internal PK11 slot");
    return false;
  }

  SECItem keyItem = {siBuffer, (unsigned char*)&aKey[0], CENC_KEY_LEN};
  SECItem ivItem = {siBuffer, (unsigned char*)&aIV[0], CENC_KEY_LEN};

  std::unique_ptr<PK11SymKey, MaybeDeleteHelper<PK11SymKey>> key(
      PK11_ImportSymKey(slot.get(), CKM_AES_CBC, PK11_OriginUnwrap, CKA_DECRYPT,
                        &keyItem, nullptr));

  if (!key.get()) {
    CK_LOGE("Failed to import sym key");
    return false;
  }

  std::unique_ptr<PK11Context, MaybeDeleteHelper<PK11Context>> ctx(
      PK11_CreateContextBySymKey(CKM_AES_CBC, CKA_DECRYPT, key.get(), &ivItem));

  uint8_t* encryptedSubsample = &aSubsample[0];
  const uint32_t BLOCK_SIZE = 16;
  const uint32_t skipBytes = aSkipByteBlock * BLOCK_SIZE;
  const uint32_t totalBlocks = aSubsample.Length() / BLOCK_SIZE;
  uint32_t blocksProcessed = 0;

  while (blocksProcessed < totalBlocks) {
    uint32_t blocksToDecrypt = aCryptByteBlock <= totalBlocks - blocksProcessed
                                   ? aCryptByteBlock
                                   : totalBlocks - blocksProcessed;
    uint32_t bytesToDecrypt = blocksToDecrypt * BLOCK_SIZE;
    int outLen;
    SECStatus rv;
    rv = PK11_CipherOp(ctx.get(), encryptedSubsample, &outLen, bytesToDecrypt,
                       encryptedSubsample, bytesToDecrypt);
    if (rv != SECSuccess) {
      CK_LOGE("PK11_CipherOp() failed");
      return false;
    }

    encryptedSubsample += skipBytes + bytesToDecrypt;
    blocksProcessed += aSkipByteBlock + blocksToDecrypt;
  }

  return true;
}

/* static */
bool ClearKeyUtils::DecryptAES(const vector<uint8_t>& aKey,
                               vector<uint8_t>& aData, vector<uint8_t>& aIV) {
  assert(aIV.size() == CENC_KEY_LEN);
  assert(aKey.size() == CENC_KEY_LEN);

  PK11SlotInfo* slot = PK11_GetInternalKeySlot();
  if (!slot) {
    CK_LOGE("Failed to get internal PK11 slot");
    return false;
  }

  SECItem keyItem = {siBuffer, (unsigned char*)&aKey[0], CENC_KEY_LEN};
  PK11SymKey* key = PK11_ImportSymKey(slot, CKM_AES_CTR, PK11_OriginUnwrap,
                                      CKA_ENCRYPT, &keyItem, nullptr);
  PK11_FreeSlot(slot);
  if (!key) {
    CK_LOGE("Failed to import sym key");
    return false;
  }

  CK_AES_CTR_PARAMS params;
  params.ulCounterBits = 32;
  memcpy(&params.cb, &aIV[0], CENC_KEY_LEN);
  SECItem paramItem = {siBuffer, (unsigned char*)&params,
                       sizeof(CK_AES_CTR_PARAMS)};

  unsigned int outLen = 0;
  auto rv = PK11_Decrypt(key, CKM_AES_CTR, &paramItem, &aData[0], &outLen,
                         aData.size(), &aData[0], aData.size());

  aData.resize(outLen);
  PK11_FreeSymKey(key);

  if (rv != SECSuccess) {
    CK_LOGE("PK11_Decrypt() failed");
    return false;
  }

  return true;
}

/**
 * ClearKey expects all Key IDs to be base64 encoded with non-standard alphabet
 * and padding.
 */
static bool EncodeBase64Web(vector<uint8_t> aBinary, string& aEncoded) {
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

    // out of bounds index for 'sAlphabet'
    assert(idx < MOZ_ARRAY_LENGTH(sAlphabet));
    out[i] = sAlphabet[idx];
  }

  return true;
}

/* static */
void ClearKeyUtils::MakeKeyRequest(const vector<KeyId>& aKeyIDs,
                                   string& aOutRequest,
                                   SessionType aSessionType) {
  assert(aKeyIDs.size() && aOutRequest.empty());

  aOutRequest.append("{\"kids\":[");
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
  aOutRequest.append("],\"type\":");

  aOutRequest.append("\"");
  aOutRequest.append(SessionTypeToString(aSessionType));
  aOutRequest.append("\"}");
}

#define EXPECT_SYMBOL(CTX, X)                     \
  do {                                            \
    if (GetNextSymbol(CTX) != (X)) {              \
      CK_LOGE("Unexpected symbol in JWK parser"); \
      return false;                               \
    }                                             \
  } while (false)

struct ParserContext {
  const uint8_t* mIter;
  const uint8_t* mEnd;
};

static uint8_t PeekSymbol(ParserContext& aCtx) {
  for (; aCtx.mIter < aCtx.mEnd; (aCtx.mIter)++) {
    if (!isspace(*aCtx.mIter)) {
      return *aCtx.mIter;
    }
  }

  return 0;
}

static uint8_t GetNextSymbol(ParserContext& aCtx) {
  uint8_t sym = PeekSymbol(aCtx);
  aCtx.mIter++;
  return sym;
}

static bool SkipToken(ParserContext& aCtx);

static bool SkipString(ParserContext& aCtx) {
  EXPECT_SYMBOL(aCtx, '"');
  for (uint8_t sym = GetNextSymbol(aCtx); sym; sym = GetNextSymbol(aCtx)) {
    if (sym == '\\') {
      sym = GetNextSymbol(aCtx);
      if (!sym) {
        return false;
      }
    } else if (sym == '"') {
      return true;
    }
  }

  return false;
}

/**
 * Skip whole object and values it contains.
 */
static bool SkipObject(ParserContext& aCtx) {
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
static bool SkipArray(ParserContext& aCtx) {
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
static bool SkipLiteral(ParserContext& aCtx) {
  for (; aCtx.mIter < aCtx.mEnd; aCtx.mIter++) {
    if (!isalnum(*aCtx.mIter) && *aCtx.mIter != '.' && *aCtx.mIter != '-' &&
        *aCtx.mIter != '+') {
      return true;
    }
  }

  return false;
}

static bool SkipToken(ParserContext& aCtx) {
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

static bool GetNextLabel(ParserContext& aCtx, string& aOutLabel) {
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

static bool ParseKeyObject(ParserContext& aCtx, KeyIdPair& aOutKey) {
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

  return !key.empty() && !keyId.empty() &&
         DecodeBase64(keyId, aOutKey.mKeyId) &&
         DecodeBase64(key, aOutKey.mKey) && GetNextSymbol(aCtx) == '}';
}

static bool ParseKeys(ParserContext& aCtx, vector<KeyIdPair>& aOutKeys) {
  // Consume start of array.
  EXPECT_SYMBOL(aCtx, '[');

  while (true) {
    KeyIdPair key;
    if (!ParseKeyObject(aCtx, key)) {
      CK_LOGE("Failed to parse key object");
      return false;
    }

    assert(!key.mKey.empty() && !key.mKeyId.empty());
    aOutKeys.push_back(key);

    uint8_t sym = PeekSymbol(aCtx);
    if (!sym || sym == ']') {
      break;
    }

    EXPECT_SYMBOL(aCtx, ',');
  }

  return GetNextSymbol(aCtx) == ']';
}

/* static */
bool ClearKeyUtils::ParseJWK(const uint8_t* aKeyData, uint32_t aKeyDataSize,
                             vector<KeyIdPair>& aOutKeys,
                             SessionType aSessionType) {
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

static bool ParseKeyIds(ParserContext& aCtx, vector<KeyId>& aOutKeyIds) {
  // Consume start of array.
  EXPECT_SYMBOL(aCtx, '[');

  while (true) {
    string label;
    vector<uint8_t> keyId;
    if (!GetNextLabel(aCtx, label) || !DecodeBase64(label, keyId)) {
      return false;
    }
    if (!keyId.empty() && keyId.size() <= kMaxKeyIdsLength) {
      aOutKeyIds.push_back(keyId);
    }

    uint8_t sym = PeekSymbol(aCtx);
    if (!sym || sym == ']') {
      break;
    }

    EXPECT_SYMBOL(aCtx, ',');
  }

  return GetNextSymbol(aCtx) == ']';
}

/* static */
bool ClearKeyUtils::ParseKeyIdsInitData(const uint8_t* aInitData,
                                        uint32_t aInitDataSize,
                                        vector<KeyId>& aOutKeyIds) {
  ParserContext ctx;
  ctx.mIter = aInitData;
  ctx.mEnd = aInitData + aInitDataSize;

  // Consume '{' from start of object.
  EXPECT_SYMBOL(ctx, '{');

  while (true) {
    string label;
    // Consume member kids.
    if (!GetNextLabel(ctx, label)) return false;
    EXPECT_SYMBOL(ctx, ':');

    if (label == "kids") {
      // Parse "kids" array.
      if (!ParseKeyIds(ctx, aOutKeyIds) || aOutKeyIds.empty()) {
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

/* static */ const char* ClearKeyUtils::SessionTypeToString(
    SessionType aSessionType) {
  switch (aSessionType) {
    case SessionType::kTemporary:
      return "temporary";
    case SessionType::kPersistentLicense:
      return "persistent-license";
    default: {
      // We don't support any other license types.
      assert(false);
      return "invalid";
    }
  }
}

/* static */
bool ClearKeyUtils::IsValidSessionId(const char* aBuff, uint32_t aLength) {
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

string ClearKeyUtils::ToHexString(const uint8_t* aBytes, uint32_t aLength) {
  stringstream ss;
  ss << std::showbase << std::uppercase << std::hex;
  for (uint32_t i = 0; i < aLength; ++i) {
    ss << std::hex << static_cast<uint32_t>(aBytes[i]);
    ss << " ";
  }

  return ss.str();
}
