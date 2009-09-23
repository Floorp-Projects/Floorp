// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_CRYPTO_SIGNATURE_VERIFIER_H_
#define BASE_CRYPTO_SIGNATURE_VERIFIER_H_

#include "build/build_config.h"

#if defined(OS_LINUX)
#include <cryptoht.h>
#elif defined(OS_MACOSX)
#include <Security/cssm.h>
#elif defined(OS_WIN)
#include <windows.h>
#include <wincrypt.h>
#endif

#include <vector>

#include "base/basictypes.h"

namespace base {

// The SignatureVerifier class verifies a signature using a bare public key
// (as opposed to a certificate).
class SignatureVerifier {
 public:
  SignatureVerifier();
  ~SignatureVerifier();

  // Streaming interface:

  // Initiates a signature verification operation.  This should be followed
  // by one or more VerifyUpdate calls and a VerifyFinal call.
  //
  // The signature algorithm is specified as a DER encoded ASN.1
  // AlgorithmIdentifier structure:
  //   AlgorithmIdentifier  ::=  SEQUENCE  {
  //       algorithm               OBJECT IDENTIFIER,
  //       parameters              ANY DEFINED BY algorithm OPTIONAL  }
  //
  // The signature is encoded according to the signature algorithm, but it
  // must not be further encoded in an ASN.1 BIT STRING.
  // Note: An RSA signatures is actually a big integer.  It must be in the
  // big-endian byte order.
  //
  // The public key is specified as a DER encoded ASN.1 SubjectPublicKeyInfo
  // structure, which contains not only the public key but also its type
  // (algorithm):
  //   SubjectPublicKeyInfo  ::=  SEQUENCE  {
  //       algorithm            AlgorithmIdentifier,
  //       subjectPublicKey     BIT STRING  }
  bool VerifyInit(const uint8* signature_algorithm,
                  int signature_algorithm_len,
                  const uint8* signature,
                  int signature_len,
                  const uint8* public_key_info,
                  int public_key_info_len);

  // Feeds a piece of the data to the signature verifier.
  void VerifyUpdate(const uint8* data_part, int data_part_len);

  // Concludes a signature verification operation.  Returns true if the
  // signature is valid.  Returns false if the signature is invalid or an
  // error occurred.
  bool VerifyFinal();

  // Note: we can provide a one-shot interface if there is interest:
  //   bool Verify(const uint8* data,
  //               int data_len,
  //               const uint8* signature_algorithm,
  //               int signature_algorithm_len,
  //               const uint8* signature,
  //               int signature_len,
  //               const uint8* public_key_info,
  //               int public_key_info_len);

 private:
  void Reset();

  std::vector<uint8> signature_;

#if defined(OS_LINUX)
  VFYContext* vfy_context_;
#elif defined(OS_MACOSX)
  std::vector<uint8> public_key_info_;

  CSSM_CSP_HANDLE csp_handle_;

  CSSM_CC_HANDLE sig_handle_;

  CSSM_KEY public_key_;
#elif defined(OS_WIN)
  HCRYPTPROV provider_;

  HCRYPTHASH hash_object_;

  HCRYPTKEY public_key_;
#endif
};

}  // namespace base

#endif  // BASE_CRYPTO_SIGNATURE_VERIFIER_H_
