/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_RTCCertificate_h
#define mozilla_dom_RTCCertificate_h

#include <cstdint>
#include "ScopedNSSTypes.h"
#include "certt.h"
#include "js/RootingAPI.h"
#include "keythi.h"
#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/Assertions.h"
#include "mozilla/RefPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIGlobalObject.h"
#include "nsISupports.h"
#include "nsWrapperCache.h"
#include "prtime.h"
#include "sslt.h"

class JSObject;
struct JSContext;
struct JSStructuredCloneReader;
struct JSStructuredCloneWriter;

namespace JS {
class Compartment;
}

namespace mozilla {
class DtlsIdentity;
class ErrorResult;

namespace dom {

class GlobalObject;
class ObjectOrString;
class Promise;

class RTCCertificate final : public nsISupports, public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(RTCCertificate)

  // WebIDL method that implements RTCPeerConnection.generateCertificate.
  static already_AddRefed<Promise> GenerateCertificate(
      const GlobalObject& aGlobal, const ObjectOrString& aOptions,
      ErrorResult& aRv, JS::Compartment* aCompartment = nullptr);

  explicit RTCCertificate(nsIGlobalObject* aGlobal);
  RTCCertificate(nsIGlobalObject* aGlobal, SECKEYPrivateKey* aPrivateKey,
                 CERTCertificate* aCertificate, SSLKEAType aAuthType,
                 PRTime aExpires);

  nsIGlobalObject* GetParentObject() const { return mGlobal; }
  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  // WebIDL expires attribute.  Note: JS dates are milliseconds since epoch;
  // NSPR PRTime is in microseconds since the same epoch.
  uint64_t Expires() const { return mExpires / PR_USEC_PER_MSEC; }

  // Accessors for use by PeerConnectionImpl.
  RefPtr<DtlsIdentity> CreateDtlsIdentity() const;
  const UniqueCERTCertificate& Certificate() const { return mCertificate; }

  // Structured clone methods
  bool WriteStructuredClone(JSContext* aCx,
                            JSStructuredCloneWriter* aWriter) const;
  static already_AddRefed<RTCCertificate> ReadStructuredClone(
      JSContext* aCx, nsIGlobalObject* aGlobal,
      JSStructuredCloneReader* aReader);

 private:
  ~RTCCertificate() = default;
  void operator=(const RTCCertificate&) = delete;
  RTCCertificate(const RTCCertificate&) = delete;

  bool ReadCertificate(JSStructuredCloneReader* aReader);
  bool ReadPrivateKey(JSStructuredCloneReader* aReader);
  bool WriteCertificate(JSStructuredCloneWriter* aWriter) const;
  bool WritePrivateKey(JSStructuredCloneWriter* aWriter) const;

  RefPtr<nsIGlobalObject> mGlobal;
  UniqueSECKEYPrivateKey mPrivateKey;
  UniqueCERTCertificate mCertificate;
  SSLKEAType mAuthType;
  PRTime mExpires;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_RTCCertificate_h
