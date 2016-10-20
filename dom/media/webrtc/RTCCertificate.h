/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_RTCCertificate_h
#define mozilla_dom_RTCCertificate_h

#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"
#include "nsIGlobalObject.h"
#include "nsNSSShutDown.h"
#include "prtime.h"
#include "sslt.h"
#include "ScopedNSSTypes.h"

#include "mozilla/ErrorResult.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/RefPtr.h"
#include "mozilla/dom/CryptoKey.h"
#include "mtransport/dtlsidentity.h"
#include "js/StructuredClone.h"
#include "js/TypeDecls.h"

namespace mozilla {
namespace dom {

class ObjectOrString;

class RTCCertificate final
    : public nsISupports,
      public nsWrapperCache,
      public nsNSSShutDownObject
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(RTCCertificate)

  // WebIDL method that implements RTCPeerConnection.generateCertificate.
  static already_AddRefed<Promise> GenerateCertificate(
      const GlobalObject& global, const ObjectOrString& keygenAlgorithm,
      ErrorResult& aRv, JSCompartment* aCompartment = nullptr);

  explicit RTCCertificate(nsIGlobalObject* aGlobal);
  RTCCertificate(nsIGlobalObject* aGlobal, SECKEYPrivateKey* aPrivateKey,
                 CERTCertificate* aCertificate, SSLKEAType aAuthType,
                 PRTime aExpires);

  nsIGlobalObject* GetParentObject() const { return mGlobal; }
  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  // WebIDL expires attribute.  Note: JS dates are milliseconds since epoch;
  // NSPR PRTime is in microseconds since the same epoch.
  uint64_t Expires() const
  {
    return mExpires / PR_USEC_PER_MSEC;
  }

  // Accessors for use by PeerConnectionImpl.
  RefPtr<DtlsIdentity> CreateDtlsIdentity() const;
  const UniqueCERTCertificate& Certificate() const { return mCertificate; }

  // For nsNSSShutDownObject
  virtual void virtualDestroyNSSReference() override;
  void destructorSafeDestroyNSSReference();

  // Structured clone methods
  bool WriteStructuredClone(JSStructuredCloneWriter* aWriter) const;
  bool ReadStructuredClone(JSStructuredCloneReader* aReader);

private:
  ~RTCCertificate();
  void operator=(const RTCCertificate&) = delete;
  RTCCertificate(const RTCCertificate&) = delete;

  bool ReadCertificate(JSStructuredCloneReader* aReader,
                       const nsNSSShutDownPreventionLock& /*lockproof*/);
  bool ReadPrivateKey(JSStructuredCloneReader* aReader,
                      const nsNSSShutDownPreventionLock& aLockProof);
  bool WriteCertificate(JSStructuredCloneWriter* aWriter,
                        const nsNSSShutDownPreventionLock& /*lockproof*/) const;
  bool WritePrivateKey(JSStructuredCloneWriter* aWriter,
                       const nsNSSShutDownPreventionLock& aLockProof) const;

  RefPtr<nsIGlobalObject> mGlobal;
  UniqueSECKEYPrivateKey mPrivateKey;
  UniqueCERTCertificate mCertificate;
  SSLKEAType mAuthType;
  PRTime mExpires;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_RTCCertificate_h
