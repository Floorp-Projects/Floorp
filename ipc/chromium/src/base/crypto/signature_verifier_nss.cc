// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/crypto/signature_verifier.h"

#include <cryptohi.h>
#include <keyhi.h>
#include <stdlib.h>

#include "base/logging.h"
#include "base/nss_init.h"

namespace base {

SignatureVerifier::SignatureVerifier() : vfy_context_(NULL) {
  EnsureNSSInit();
}

SignatureVerifier::~SignatureVerifier() {
  Reset();
}

bool SignatureVerifier::VerifyInit(const uint8* signature_algorithm,
                                   int signature_algorithm_len,
                                   const uint8* signature,
                                   int signature_len,
                                   const uint8* public_key_info,
                                   int public_key_info_len) {
  signature_.assign(signature, signature + signature_len);

  CERTSubjectPublicKeyInfo* spki = NULL;
  SECItem spki_der;
  spki_der.type = siBuffer;
  spki_der.data = const_cast<uint8*>(public_key_info);
  spki_der.len = public_key_info_len;
  spki = SECKEY_DecodeDERSubjectPublicKeyInfo(&spki_der);
  if (!spki)
    return false;
  SECKEYPublicKey* public_key = SECKEY_ExtractPublicKey(spki);
  SECKEY_DestroySubjectPublicKeyInfo(spki);  // Done with spki.
  if (!public_key)
    return false;

  PLArenaPool* arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
  if (!arena) {
    SECKEY_DestroyPublicKey(public_key);
    return false;
  }

  SECItem sig_alg_der;
  sig_alg_der.type = siBuffer;
  sig_alg_der.data = const_cast<uint8*>(signature_algorithm);
  sig_alg_der.len = signature_algorithm_len;
  SECAlgorithmID sig_alg_id;
  SECStatus rv;
  rv = SEC_QuickDERDecodeItem(arena, &sig_alg_id, SECOID_AlgorithmIDTemplate,
                              &sig_alg_der);
  if (rv != SECSuccess) {
    SECKEY_DestroyPublicKey(public_key);
    PORT_FreeArena(arena, PR_TRUE);
    return false;
  }

  SECItem sig;
  sig.type = siBuffer;
  sig.data = const_cast<uint8*>(signature);
  sig.len = signature_len;
  SECOidTag hash_alg_tag;
  vfy_context_ = VFY_CreateContextWithAlgorithmID(public_key, &sig,
                                                  &sig_alg_id, &hash_alg_tag,
                                                  NULL);
  SECKEY_DestroyPublicKey(public_key);  // Done with public_key.
  PORT_FreeArena(arena, PR_TRUE);  // Done with sig_alg_id.
  if (!vfy_context_) {
    // A corrupted RSA signature could be detected without the data, so
    // VFY_CreateContextWithAlgorithmID may fail with SEC_ERROR_BAD_SIGNATURE
    // (-8182).
    return false;
  }

  rv = VFY_Begin(vfy_context_);
  if (rv != SECSuccess) {
    NOTREACHED();
    return false;
  }
  return true;
}

void SignatureVerifier::VerifyUpdate(const uint8* data_part,
                                     int data_part_len) {
  SECStatus rv = VFY_Update(vfy_context_, data_part, data_part_len);
  DCHECK(rv == SECSuccess);
}

bool SignatureVerifier::VerifyFinal() {
  SECStatus rv = VFY_End(vfy_context_);
  Reset();

  // If signature verification fails, the error code is
  // SEC_ERROR_BAD_SIGNATURE (-8182).
  return (rv == SECSuccess);
}

void SignatureVerifier::Reset() {
  if (vfy_context_) {
    VFY_DestroyContext(vfy_context_, PR_TRUE);
    vfy_context_ = NULL;
  }
  signature_.clear();
}

}  // namespace base

