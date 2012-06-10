/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPrincipal_h__
#define nsPrincipal_h__

#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsVoidArray.h"
#include "nsHashtable.h"
#include "nsJSPrincipals.h"
#include "nsTArray.h"
#include "nsAutoPtr.h"

class nsIObjectInputStream;
class nsIObjectOutputStream;

class nsPrincipal : public nsJSPrincipals
{
public:
  nsPrincipal();

protected:
  virtual ~nsPrincipal();

public:
  // Our refcount is managed by nsJSPrincipals.  Use this macro to avoid
  // an extra refcount member.
  NS_DECL_ISUPPORTS_INHERITED
public:

  NS_DECL_NSIPRINCIPAL
  NS_DECL_NSISERIALIZABLE

  // Either Init() or InitFromPersistent() must be called before
  // the principal is in a usable state.
  nsresult Init(const nsACString& aCertFingerprint,
                const nsACString& aSubjectName,
                const nsACString& aPrettyName,
                nsISupports* aCert,
                nsIURI *aCodebase);
  nsresult InitFromPersistent(const char* aPrefName,
                              const nsCString& aFingerprint,
                              const nsCString& aSubjectName,
                              const nsACString& aPrettyName,
                              const char* aGrantedList,
                              const char* aDeniedList,
                              nsISupports* aCert,
                              bool aIsCert,
                              bool aTrusted);

  // Call this to ensure that this principal has a subject name, a pretty name,
  // and a cert pointer.  This method will throw if there is already a
  // different subject name or if this principal has no certificate.
  nsresult EnsureCertData(const nsACString& aSubjectName,
                          const nsACString& aPrettyName,
                          nsISupports* aCert);

  enum AnnotationValue { AnnotationEnabled=1, AnnotationDisabled };

  void SetURI(nsIURI *aURI);
  nsresult SetCapability(const char *capability, void **annotation, 
                         AnnotationValue value);

  static const char sInvalid[];

  virtual void GetScriptLocation(nsACString &aStr) MOZ_OVERRIDE;

#ifdef DEBUG
  virtual void dumpImpl() MOZ_OVERRIDE;
#endif 

protected:
  // Formerly an IDL method. Now just a protected helper.
  nsresult SetCanEnableCapability(const char *capability, PRInt16 canEnable);

  nsTArray< nsAutoPtr<nsHashtable> > mAnnotations;
  nsHashtable* mCapabilities;
  nsCString mPrefName;
  static PRInt32 sCapabilitiesOrdinal;

  // XXXcaa This is a semi-hack.  The best solution here is to keep
  // a reference to an interface here, except there is no interface
  // that we can use yet.
  struct Certificate
  {
    Certificate(const nsACString& aFingerprint, const nsACString& aSubjectName,
                const nsACString& aPrettyName, nsISupports* aCert)
      : fingerprint(aFingerprint),
        subjectName(aSubjectName),
        prettyName(aPrettyName),
        cert(aCert)
    {
    }
    nsCString fingerprint;
    nsCString subjectName;
    nsCString prettyName;
    nsCOMPtr<nsISupports> cert;
  };

  nsresult SetCertificate(const nsACString& aFingerprint,
                          const nsACString& aSubjectName,
                          const nsACString& aPrettyName,
                          nsISupports* aCert);

  // Checks whether this principal's certificate equals aOther's.
  bool CertificateEquals(nsIPrincipal *aOther);

  // Keep this is a pointer, even though it may slightly increase the
  // cost of keeping a certificate, this is a good tradeoff though since
  // it is very rare that we actually have a certificate.
  nsAutoPtr<Certificate> mCert;

  DomainPolicy* mSecurityPolicy;

  nsCOMPtr<nsIContentSecurityPolicy> mCSP;
  nsCOMPtr<nsIURI> mCodebase;
  nsCOMPtr<nsIURI> mDomain;
  bool mTrusted;
  bool mInitialized;
  // If mCodebaseImmutable is true, mCodebase is non-null and immutable
  bool mCodebaseImmutable;
  bool mDomainImmutable;
};


#define NS_PRINCIPAL_CLASSNAME  "principal"
#define NS_PRINCIPAL_CONTRACTID "@mozilla.org/principal;1"
#define NS_PRINCIPAL_CID \
  { 0x36102b6b, 0x7b62, 0x451a, \
    { 0xa1, 0xc8, 0xa0, 0xd4, 0x56, 0xc9, 0x2d, 0xc5 }}


#endif // nsPrincipal_h__
