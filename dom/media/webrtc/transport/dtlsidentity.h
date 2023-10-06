/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef dtls_identity_h__
#define dtls_identity_h__

#include <utility>
#include <vector>

#include "ScopedNSSTypes.h"
#include "m_cpp_utils.h"
#include "mozilla/RefPtr.h"
#include "nsISupportsImpl.h"
#include "nsString.h"
#include "nsTArray.h"
#include "sslt.h"

// All code in this module requires NSS to be live.
// Callers must initialize NSS and implement the nsNSSShutdownObject
// protocol.
namespace mozilla {

class DtlsDigest {
 public:
  const static size_t kMaxDtlsDigestLength = HASH_LENGTH_MAX;
  DtlsDigest() = default;
  explicit DtlsDigest(const nsACString& algorithm) : algorithm_(algorithm) {}

  DtlsDigest(const nsACString& algorithm, const std::vector<uint8_t>& value)
      : algorithm_(algorithm), value_(value) {
    MOZ_ASSERT(value.size() <= kMaxDtlsDigestLength);
  }
  ~DtlsDigest() = default;

  bool operator!=(const DtlsDigest& rhs) const { return !operator==(rhs); }

  bool operator==(const DtlsDigest& rhs) const {
    if (algorithm_ != rhs.algorithm_) {
      return false;
    }

    return value_ == rhs.value_;
  }

  nsCString algorithm_;
  std::vector<uint8_t> value_;
};

typedef std::vector<DtlsDigest> DtlsDigestList;

class DtlsIdentity final {
 public:
  // This constructor takes ownership of privkey and cert.
  DtlsIdentity(UniqueSECKEYPrivateKey privkey, UniqueCERTCertificate cert,
               SSLKEAType authType)
      : private_key_(std::move(privkey)),
        cert_(std::move(cert)),
        auth_type_(authType) {}

  // Allows serialization/deserialization; cannot write IPC serialization code
  // directly for DtlsIdentity, since IPC-able types need to be constructable
  // on the stack.
  nsresult Serialize(nsTArray<uint8_t>* aKeyDer, nsTArray<uint8_t>* aCertDer);
  static RefPtr<DtlsIdentity> Deserialize(const nsTArray<uint8_t>& aKeyDer,
                                          const nsTArray<uint8_t>& aCertDer,
                                          SSLKEAType authType);

  // This is only for use in tests, or for external linkage.  It makes a (bad)
  // instance of this class.
  static RefPtr<DtlsIdentity> Generate();

  // These don't create copies or transfer ownership. If you want these to live
  // on, make a copy.
  const UniqueCERTCertificate& cert() const { return cert_; }
  const UniqueSECKEYPrivateKey& privkey() const { return private_key_; }
  // Note: this uses SSLKEAType because that is what the libssl API requires.
  // This is a giant confusing mess, but libssl indexes certificates based on a
  // key exchange type, not authentication type (as you might have reasonably
  // expected).
  SSLKEAType auth_type() const { return auth_type_; }

  nsresult ComputeFingerprint(DtlsDigest* digest) const;
  static nsresult ComputeFingerprint(const UniqueCERTCertificate& cert,
                                     DtlsDigest* digest);

  static constexpr nsLiteralCString DEFAULT_HASH_ALGORITHM = "sha-256"_ns;
  enum { HASH_ALGORITHM_MAX_LENGTH = 64 };

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(DtlsIdentity)

 private:
  ~DtlsIdentity() = default;
  DISALLOW_COPY_ASSIGN(DtlsIdentity);

  UniqueSECKEYPrivateKey private_key_;
  UniqueCERTCertificate cert_;
  SSLKEAType auth_type_;
};
}  // namespace mozilla
#endif
