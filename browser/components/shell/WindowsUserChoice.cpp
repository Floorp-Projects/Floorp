/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Generate and check the UserChoice Hash, which protects file and protocol
 * associations on Windows 10.
 *
 * NOTE: This is also used in the WDBA, so it avoids XUL and XPCOM.
 *
 * References:
 * - PS-SFTA by Danysys <https://github.com/DanysysTeam/PS-SFTA>
 *  - based on a PureBasic version by LMongrain
 *    <https://github.com/DanysysTeam/SFTA>
 * - AssocHashGen by "halfmeasuresdisabled", see bug 1225660 and
 *   <https://www.reddit.com/r/ReverseEngineering/comments/3t7q9m/assochashgen_a_reverse_engineered_version_of/>
 * - SetUserFTA changelog
 *   <https://kolbi.cz/blog/2017/10/25/setuserfta-userchoice-hash-defeated-set-file-type-associations-per-user/>
 */

#include <windows.h>
#include <appmodel.h>  // for GetPackageFamilyName
#include <sddl.h>      // for ConvertSidToStringSidW
#include <wincrypt.h>  // for CryptoAPI base64
#include <bcrypt.h>    // for CNG MD5
#include <winternl.h>  // for NT_SUCCESS()

#include "nsDebug.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/UniquePtr.h"
#include "nsWindowsHelpers.h"

#include "WindowsUserChoice.h"

using namespace mozilla;

UniquePtr<wchar_t[]> GetCurrentUserStringSid() {
  HANDLE rawProcessToken;
  if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY,
                          &rawProcessToken)) {
    return nullptr;
  }
  nsAutoHandle processToken(rawProcessToken);

  DWORD userSize = 0;
  if (!(!::GetTokenInformation(processToken.get(), TokenUser, nullptr, 0,
                               &userSize) &&
        GetLastError() == ERROR_INSUFFICIENT_BUFFER)) {
    return nullptr;
  }

  auto userBytes = MakeUnique<unsigned char[]>(userSize);
  if (!::GetTokenInformation(processToken.get(), TokenUser, userBytes.get(),
                             userSize, &userSize)) {
    return nullptr;
  }

  wchar_t* rawSid = nullptr;
  if (!::ConvertSidToStringSidW(
          reinterpret_cast<PTOKEN_USER>(userBytes.get())->User.Sid, &rawSid)) {
    return nullptr;
  }
  UniquePtr<wchar_t, LocalFreeDeleter> sid(rawSid);

  // Copy instead of passing UniquePtr<wchar_t, LocalFreeDeleter> back to
  // the caller.
  int sidLen = ::lstrlenW(sid.get()) + 1;
  auto outSid = MakeUnique<wchar_t[]>(sidLen);
  memcpy(outSid.get(), sid.get(), sidLen * sizeof(wchar_t));

  return outSid;
}

/*
 * Create the string which becomes the input to the UserChoice hash.
 *
 * @see GenerateUserChoiceHash() for parameters.
 *
 * @return The formatted string, nullptr on failure.
 *
 * NOTE: This uses the format as of Windows 10 20H2 (latest as of this writing),
 * used at least since 1803.
 * There was at least one older version, not currently supported: On Win10 RTM
 * (build 10240, aka 1507) the hash function is the same, but the timestamp and
 * User Experience string aren't included; instead (for protocols) the string
 * ends with the exe path. The changelog of SetUserFTA suggests the algorithm
 * changed in 1703, so there may be two versions: before 1703, and 1703 to now.
 */
static UniquePtr<wchar_t[]> FormatUserChoiceString(const wchar_t* aExt,
                                                   const wchar_t* aUserSid,
                                                   const wchar_t* aProgId,
                                                   SYSTEMTIME aTimestamp) {
  aTimestamp.wSecond = 0;
  aTimestamp.wMilliseconds = 0;

  FILETIME fileTime = {0};
  if (!::SystemTimeToFileTime(&aTimestamp, &fileTime)) {
    return nullptr;
  }

  // This string is built into Windows as part of the UserChoice hash algorithm.
  // It might vary across Windows SKUs (e.g. Windows 10 vs. Windows Server), or
  // across builds of the same SKU, but this is the only currently known
  // version. There isn't any known way of deriving it, so we assume this
  // constant value. If we are wrong, we will not be able to generate correct
  // UserChoice hashes.
  const wchar_t* userExperience =
      L"User Choice set via Windows User Experience "
      L"{D18B6DD5-6124-4341-9318-804003BAFA0B}";

  const wchar_t* userChoiceFmt =
      L"%s%s%s"
      L"%08lx"
      L"%08lx"
      L"%s";
  int userChoiceLen = _scwprintf(userChoiceFmt, aExt, aUserSid, aProgId,
                                 fileTime.dwHighDateTime,
                                 fileTime.dwLowDateTime, userExperience);
  userChoiceLen += 1;  // _scwprintf does not include the terminator

  auto userChoice = MakeUnique<wchar_t[]>(userChoiceLen);
  _snwprintf_s(userChoice.get(), userChoiceLen, _TRUNCATE, userChoiceFmt, aExt,
               aUserSid, aProgId, fileTime.dwHighDateTime,
               fileTime.dwLowDateTime, userExperience);

  ::CharLowerW(userChoice.get());

  return userChoice;
}

// @return The MD5 hash of the input, nullptr on failure.
static UniquePtr<DWORD[]> CNG_MD5(const unsigned char* bytes, ULONG bytesLen) {
  constexpr ULONG MD5_BYTES = 16;
  constexpr ULONG MD5_DWORDS = MD5_BYTES / sizeof(DWORD);
  UniquePtr<DWORD[]> hash;

  BCRYPT_ALG_HANDLE hAlg = nullptr;
  if (NT_SUCCESS(::BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_MD5_ALGORITHM,
                                               nullptr, 0))) {
    BCRYPT_HASH_HANDLE hHash = nullptr;
    // As of Windows 7 the hash handle will manage its own object buffer when
    // pbHashObject is nullptr and cbHashObject is 0.
    if (NT_SUCCESS(
            ::BCryptCreateHash(hAlg, &hHash, nullptr, 0, nullptr, 0, 0))) {
      // BCryptHashData promises not to modify pbInput.
      if (NT_SUCCESS(::BCryptHashData(hHash, const_cast<unsigned char*>(bytes),
                                      bytesLen, 0))) {
        hash = MakeUnique<DWORD[]>(MD5_DWORDS);
        if (!NT_SUCCESS(::BCryptFinishHash(
                hHash, reinterpret_cast<unsigned char*>(hash.get()),
                MD5_DWORDS * sizeof(DWORD), 0))) {
          hash.reset();
        }
      }
      ::BCryptDestroyHash(hHash);
    }
    ::BCryptCloseAlgorithmProvider(hAlg, 0);
  }

  return hash;
}

// @return The input bytes encoded as base64, nullptr on failure.
static UniquePtr<wchar_t[]> CryptoAPI_Base64Encode(const unsigned char* bytes,
                                                   DWORD bytesLen) {
  DWORD base64Len = 0;
  if (!::CryptBinaryToStringW(bytes, bytesLen,
                              CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF,
                              nullptr, &base64Len)) {
    return nullptr;
  }
  auto base64 = MakeUnique<wchar_t[]>(base64Len);
  if (!::CryptBinaryToStringW(bytes, bytesLen,
                              CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF,
                              base64.get(), &base64Len)) {
    return nullptr;
  }

  return base64;
}

static inline DWORD WordSwap(DWORD v) { return (v >> 16) | (v << 16); }

/*
 * Generate the UserChoice Hash.
 *
 * This implementation is based on the references listed above.
 * It is organized to show the logic as clearly as possible, but at some
 * point the reasoning is just "this is how it works".
 *
 * @param inputString   A null-terminated string to hash.
 *
 * @return The base64-encoded hash, or nullptr on failure.
 */
static UniquePtr<wchar_t[]> HashString(const wchar_t* inputString) {
  auto inputBytes = reinterpret_cast<const unsigned char*>(inputString);
  int inputByteCount = (::lstrlenW(inputString) + 1) * sizeof(wchar_t);

  constexpr size_t DWORDS_PER_BLOCK = 2;
  constexpr size_t BLOCK_SIZE = sizeof(DWORD) * DWORDS_PER_BLOCK;
  // Incomplete blocks are ignored.
  int blockCount = inputByteCount / BLOCK_SIZE;

  if (blockCount == 0) {
    return nullptr;
  }

  // Compute an MD5 hash. md5[0] and md5[1] will be used as constant multipliers
  // in the scramble below.
  auto md5 = CNG_MD5(inputBytes, inputByteCount);
  if (!md5) {
    return nullptr;
  }

  // The following loop effectively computes two checksums, scrambled like a
  // hash after every DWORD is added.

  // Constant multipliers for the scramble, one set for each DWORD in a block.
  const DWORD C0s[DWORDS_PER_BLOCK][5] = {
      {md5[0] | 1, 0xCF98B111uL, 0x87085B9FuL, 0x12CEB96DuL, 0x257E1D83uL},
      {md5[1] | 1, 0xA27416F5uL, 0xD38396FFuL, 0x7C932B89uL, 0xBFA49F69uL}};
  const DWORD C1s[DWORDS_PER_BLOCK][5] = {
      {md5[0] | 1, 0xEF0569FBuL, 0x689B6B9FuL, 0x79F8A395uL, 0xC3EFEA97uL},
      {md5[1] | 1, 0xC31713DBuL, 0xDDCD1F0FuL, 0x59C3AF2DuL, 0x35BD1EC9uL}};

  // The checksums.
  DWORD h0 = 0;
  DWORD h1 = 0;
  // Accumulated total of the checksum after each DWORD.
  DWORD h0Acc = 0;
  DWORD h1Acc = 0;

  for (int i = 0; i < blockCount; ++i) {
    for (size_t j = 0; j < DWORDS_PER_BLOCK; ++j) {
      const DWORD* C0 = C0s[j];
      const DWORD* C1 = C1s[j];

      DWORD input;
      memcpy(&input, &inputBytes[(i * DWORDS_PER_BLOCK + j) * sizeof(DWORD)],
             sizeof(DWORD));

      h0 += input;
      // Scramble 0
      h0 *= C0[0];
      h0 = WordSwap(h0) * C0[1];
      h0 = WordSwap(h0) * C0[2];
      h0 = WordSwap(h0) * C0[3];
      h0 = WordSwap(h0) * C0[4];
      h0Acc += h0;

      h1 += input;
      // Scramble 1
      h1 = WordSwap(h1) * C1[1] + h1 * C1[0];
      h1 = (h1 >> 16) * C1[2] + h1 * C1[3];
      h1 = WordSwap(h1) * C1[4] + h1;
      h1Acc += h1;
    }
  }

  DWORD hash[2] = {h0 ^ h1, h0Acc ^ h1Acc};

  return CryptoAPI_Base64Encode(reinterpret_cast<const unsigned char*>(hash),
                                sizeof(hash));
}

UniquePtr<wchar_t[]> GetAssociationKeyPath(const wchar_t* aExt) {
  const wchar_t* keyPathFmt;
  if (aExt[0] == L'.') {
    keyPathFmt =
        L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\%s";
  } else {
    keyPathFmt =
        L"SOFTWARE\\Microsoft\\Windows\\Shell\\Associations\\"
        L"UrlAssociations\\%s";
  }

  int keyPathLen = _scwprintf(keyPathFmt, aExt);
  keyPathLen += 1;  // _scwprintf does not include the terminator

  auto keyPath = MakeUnique<wchar_t[]>(keyPathLen);
  _snwprintf_s(keyPath.get(), keyPathLen, _TRUNCATE, keyPathFmt, aExt);

  return keyPath;
}

UniquePtr<wchar_t[]> GenerateUserChoiceHash(const wchar_t* aExt,
                                            const wchar_t* aUserSid,
                                            const wchar_t* aProgId,
                                            SYSTEMTIME aTimestamp) {
  auto userChoice = FormatUserChoiceString(aExt, aUserSid, aProgId, aTimestamp);
  if (!userChoice) {
    return nullptr;
  }
  return HashString(userChoice.get());
}

/*
 * NOTE: The passed-in current user SID is used here, instead of getting the SID
 * for the owner of the key. We are assuming that this key in HKCU is owned by
 * the current user, since we want to replace that key ourselves. If the key is
 * owned by someone else, then this check will fail; this is ok because we would
 * likely not want to replace that other user's key anyway.
 */
CheckUserChoiceHashResult CheckUserChoiceHash(const wchar_t* aExt,
                                              const wchar_t* aUserSid) {
  auto keyPath = GetAssociationKeyPath(aExt);
  if (!keyPath) {
    return CheckUserChoiceHashResult::ERR_OTHER;
  }

  HKEY rawAssocKey;
  if (::RegOpenKeyExW(HKEY_CURRENT_USER, keyPath.get(), 0, KEY_READ,
                      &rawAssocKey) != ERROR_SUCCESS) {
    return CheckUserChoiceHashResult::ERR_OTHER;
  }
  nsAutoRegKey assocKey(rawAssocKey);

  FILETIME lastWriteFileTime;
  {
    HKEY rawUserChoiceKey;
    if (::RegOpenKeyExW(assocKey.get(), L"UserChoice", 0, KEY_READ,
                        &rawUserChoiceKey) != ERROR_SUCCESS) {
      return CheckUserChoiceHashResult::ERR_OTHER;
    }
    nsAutoRegKey userChoiceKey(rawUserChoiceKey);

    if (::RegQueryInfoKeyW(userChoiceKey.get(), nullptr, nullptr, nullptr,
                           nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                           nullptr, &lastWriteFileTime) != ERROR_SUCCESS) {
      return CheckUserChoiceHashResult::ERR_OTHER;
    }
  }

  SYSTEMTIME lastWriteSystemTime;
  if (!::FileTimeToSystemTime(&lastWriteFileTime, &lastWriteSystemTime)) {
    return CheckUserChoiceHashResult::ERR_OTHER;
  }

  // Read ProgId
  DWORD dataSizeBytes = 0;
  if (::RegGetValueW(assocKey.get(), L"UserChoice", L"ProgId", RRF_RT_REG_SZ,
                     nullptr, nullptr, &dataSizeBytes) != ERROR_SUCCESS) {
    return CheckUserChoiceHashResult::ERR_OTHER;
  }
  // +1 in case dataSizeBytes was odd, +1 to ensure termination
  DWORD dataSizeChars = (dataSizeBytes / sizeof(wchar_t)) + 2;
  UniquePtr<wchar_t[]> progId(new wchar_t[dataSizeChars]());
  if (::RegGetValueW(assocKey.get(), L"UserChoice", L"ProgId", RRF_RT_REG_SZ,
                     nullptr, progId.get(), &dataSizeBytes) != ERROR_SUCCESS) {
    return CheckUserChoiceHashResult::ERR_OTHER;
  }

  // Read Hash
  dataSizeBytes = 0;
  if (::RegGetValueW(assocKey.get(), L"UserChoice", L"Hash", RRF_RT_REG_SZ,
                     nullptr, nullptr, &dataSizeBytes) != ERROR_SUCCESS) {
    return CheckUserChoiceHashResult::ERR_OTHER;
  }
  dataSizeChars = (dataSizeBytes / sizeof(wchar_t)) + 2;
  UniquePtr<wchar_t[]> storedHash(new wchar_t[dataSizeChars]());
  if (::RegGetValueW(assocKey.get(), L"UserChoice", L"Hash", RRF_RT_REG_SZ,
                     nullptr, storedHash.get(),
                     &dataSizeBytes) != ERROR_SUCCESS) {
    return CheckUserChoiceHashResult::ERR_OTHER;
  }

  auto computedHash =
      GenerateUserChoiceHash(aExt, aUserSid, progId.get(), lastWriteSystemTime);
  if (!computedHash) {
    return CheckUserChoiceHashResult::ERR_OTHER;
  }

  if (::CompareStringOrdinal(computedHash.get(), -1, storedHash.get(), -1,
                             FALSE) != CSTR_EQUAL) {
    return CheckUserChoiceHashResult::ERR_MISMATCH;
  }

  return CheckUserChoiceHashResult::OK_V1;
}

bool CheckBrowserUserChoiceHashes() {
  auto userSid = GetCurrentUserStringSid();
  if (!userSid) {
    return false;
  }

  const wchar_t* exts[] = {L"https", L"http", L".html", L".htm"};

  for (size_t i = 0; i < ArrayLength(exts); ++i) {
    switch (CheckUserChoiceHash(exts[i], userSid.get())) {
      case CheckUserChoiceHashResult::OK_V1:
        break;
      case CheckUserChoiceHashResult::ERR_MISMATCH:
      case CheckUserChoiceHashResult::ERR_OTHER:
        return false;
    }
  }

  return true;
}

UniquePtr<wchar_t[]> FormatProgID(const wchar_t* aProgIDBase,
                                  const wchar_t* aAumi) {
  const wchar_t* progIDFmt = L"%s-%s";
  int progIDLen = _scwprintf(progIDFmt, aProgIDBase, aAumi);
  progIDLen += 1;  // _scwprintf does not include the terminator

  auto progID = MakeUnique<wchar_t[]>(progIDLen);
  _snwprintf_s(progID.get(), progIDLen, _TRUNCATE, progIDFmt, aProgIDBase,
               aAumi);

  return progID;
}

bool CheckProgIDExists(const wchar_t* aProgID) {
  HKEY key;
  if (::RegOpenKeyExW(HKEY_CLASSES_ROOT, aProgID, 0, KEY_READ, &key) !=
      ERROR_SUCCESS) {
    return false;
  }
  ::RegCloseKey(key);
  return true;
}

nsresult GetMsixProgId(const wchar_t* assoc, UniquePtr<wchar_t[]>& aProgId) {
  // Retrieve the registry path to the package from registry path:
  // clang-format off
  // HKEY_CLASSES_ROOT\Local Settings\Software\Microsoft\Windows\CurrentVersion\AppModel\Repository\Packages\[Package Full Name]\App\Capabilities\[FileAssociations | URLAssociations]\[File | URL]
  // clang-format on

  UINT32 pfnLen = 0;
  LONG rv = GetCurrentPackageFullName(&pfnLen, nullptr);
  NS_ENSURE_TRUE(rv != APPMODEL_ERROR_NO_PACKAGE, NS_ERROR_FAILURE);

  auto pfn = mozilla::MakeUnique<wchar_t[]>(pfnLen);
  rv = GetCurrentPackageFullName(&pfnLen, pfn.get());
  NS_ENSURE_TRUE(rv == ERROR_SUCCESS, NS_ERROR_FAILURE);

  const wchar_t* assocSuffix;
  if (assoc[0] == L'.') {
    // File association.
    assocSuffix = LR"(App\Capabilities\FileAssociations)";
  } else {
    // URL association.
    assocSuffix = LR"(App\Capabilities\URLAssociations)";
  }

  const wchar_t* assocPathFmt =
      LR"(Local Settings\Software\Microsoft\Windows\CurrentVersion\AppModel\Repository\Packages\%s\%s)";
  int assocPathLen = _scwprintf(assocPathFmt, pfn.get(), assocSuffix);
  assocPathLen += 1;  // _scwprintf does not include the terminator

  auto assocPath = MakeUnique<wchar_t[]>(assocPathLen);
  _snwprintf_s(assocPath.get(), assocPathLen, _TRUNCATE, assocPathFmt,
               pfn.get(), assocSuffix);

  LSTATUS ls;

  // Retrieve the package association's ProgID, always in the form `AppX[32 hash
  // characters]`.
  const size_t appxProgIdLen = 37;
  auto progId = MakeUnique<wchar_t[]>(appxProgIdLen);
  DWORD progIdLen = appxProgIdLen * sizeof(wchar_t);
  ls = ::RegGetValueW(HKEY_CLASSES_ROOT, assocPath.get(), assoc, RRF_RT_REG_SZ,
                      nullptr, (LPBYTE)progId.get(), &progIdLen);
  if (ls != ERROR_SUCCESS) {
    return NS_ERROR_WDBA_NO_PROGID;
  }

  aProgId.swap(progId);

  return NS_OK;
}
