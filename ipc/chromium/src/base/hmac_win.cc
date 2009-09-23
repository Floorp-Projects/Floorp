// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/hmac.h"

#include <windows.h>
#include <wincrypt.h>

#include <algorithm>
#include <vector>

#include "base/logging.h"

namespace base {

struct HMACPlatformData {
  // Windows Crypt API resources.
  HCRYPTPROV provider_;
  HCRYPTHASH hash_;
  HCRYPTKEY hkey_;
};

HMAC::HMAC(HashAlgorithm hash_alg)
    : hash_alg_(hash_alg), plat_(new HMACPlatformData()) {
  // Only SHA-1 digest is supported now.
  DCHECK(hash_alg_ == SHA1);
}

bool HMAC::Init(const unsigned char *key, int key_length) {
  if (plat_->provider_ || plat_->hkey_) {
    // Init must not be called more than once on the same HMAC object.
    NOTREACHED();
    return false;
  }

  if (!CryptAcquireContext(&plat_->provider_, NULL, NULL,
                           PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
    NOTREACHED();
    plat_->provider_ = NULL;
    return false;
  }

  // This code doesn't work on Win2k because PLAINTEXTKEYBLOB and
  // CRYPT_IPSEC_HMAC_KEY are not supported on Windows 2000.  PLAINTEXTKEYBLOB
  // allows the import of an unencrypted key.  For Win2k support, a cubmbersome
  // exponent-of-one key procedure must be used:
  //     http://support.microsoft.com/kb/228786/en-us
  // CRYPT_IPSEC_HMAC_KEY allows keys longer than 16 bytes.

  struct KeyBlob {
    BLOBHEADER header;
    DWORD key_size;
    BYTE key_data[1];
  };
  size_t key_blob_size = std::max(offsetof(KeyBlob, key_data) + key_length,
                                  sizeof(KeyBlob));
  std::vector<BYTE> key_blob_storage = std::vector<BYTE>(key_blob_size);
  KeyBlob* key_blob = reinterpret_cast<KeyBlob*>(&key_blob_storage[0]);
  key_blob->header.bType = PLAINTEXTKEYBLOB;
  key_blob->header.bVersion = CUR_BLOB_VERSION;
  key_blob->header.reserved = 0;
  key_blob->header.aiKeyAlg = CALG_RC2;
  key_blob->key_size = key_length;
  memcpy(key_blob->key_data, key, key_length);

  if (!CryptImportKey(plat_->provider_, &key_blob_storage[0],
                      key_blob_storage.size(), 0, CRYPT_IPSEC_HMAC_KEY,
                      &plat_->hkey_)) {
    NOTREACHED();
    plat_->hkey_ = NULL;
    return false;
  }

  // Destroy the copy of the key.
  SecureZeroMemory(key_blob->key_data, key_length);

  return true;
}

HMAC::~HMAC() {
  BOOL ok;
  if (plat_->hkey_) {
    ok = CryptDestroyKey(plat_->hkey_);
    DCHECK(ok);
  }
  if (plat_->hash_) {
    ok = CryptDestroyHash(plat_->hash_);
    DCHECK(ok);
  }
  if (plat_->provider_) {
    ok = CryptReleaseContext(plat_->provider_, 0);
    DCHECK(ok);
  }
}

bool HMAC::Sign(const std::string& data,
                unsigned char* digest,
                int digest_length) {
  if (!plat_->provider_ || !plat_->hkey_)
    return false;

  if (hash_alg_ != SHA1) {
    NOTREACHED();
    return false;
  }

  if (!CryptCreateHash(
          plat_->provider_, CALG_HMAC, plat_->hkey_, 0, &plat_->hash_))
    return false;

  HMAC_INFO hmac_info;
  memset(&hmac_info, 0, sizeof(hmac_info));
  hmac_info.HashAlgid = CALG_SHA1;
  if (!CryptSetHashParam(plat_->hash_, HP_HMAC_INFO,
                         reinterpret_cast<BYTE*>(&hmac_info), 0))
    return false;

  if (!CryptHashData(plat_->hash_,
                     reinterpret_cast<const BYTE*>(data.data()),
                     static_cast<DWORD>(data.size()), 0))
    return false;

  DWORD sha1_size = digest_length;
  if (!CryptGetHashParam(plat_->hash_, HP_HASHVAL, digest, &sha1_size, 0))
    return false;

  return true;
}

}  // namespace base
