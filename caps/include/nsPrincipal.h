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
class DomainPolicy; 

class nsBasePrincipal : public nsJSPrincipals
{
public:
  nsBasePrincipal();

protected:
  virtual ~nsBasePrincipal();

public:
  NS_IMETHOD_(nsrefcnt) AddRef(void);
  NS_IMETHOD_(nsrefcnt) Release(void);
  NS_SCRIPTABLE NS_IMETHOD GetPreferences(char** prefBranch NS_OUTPARAM, char** id NS_OUTPARAM, char** subjectName NS_OUTPARAM, char** grantedList NS_OUTPARAM, char** deniedList NS_OUTPARAM, bool* isTrusted NS_OUTPARAM);
  NS_IMETHOD GetSecurityPolicy(void** aSecurityPolicy);
  NS_IMETHOD SetSecurityPolicy(void* aSecurityPolicy);
  NS_IMETHOD CanEnableCapability(const char* capability, PRInt16* _retval NS_OUTPARAM);
  NS_IMETHOD IsCapabilityEnabled(const char* capability, void* annotation, bool* _retval NS_OUTPARAM);
  NS_IMETHOD EnableCapability(const char* capability, void** annotation NS_INOUTPARAM);
  NS_IMETHOD GetHasCertificate(bool* aHasCertificate);
  NS_IMETHOD GetFingerprint(nsACString& aFingerprint);
  NS_IMETHOD GetPrettyName(nsACString& aPrettyName);
  NS_IMETHOD GetSubjectName(nsACString& aSubjectName);
  NS_IMETHOD GetCertificate(nsISupports** aCertificate);
  NS_IMETHOD GetCsp(nsIContentSecurityPolicy** aCsp);
  NS_IMETHOD SetCsp(nsIContentSecurityPolicy* aCsp);
public:

  // Call this to ensure that this principal has a subject name, a pretty name,
  // and a cert pointer.  This method will throw if there is already a
  // different subject name or if this principal has no certificate.
  nsresult EnsureCertData(const nsACString& aSubjectName,
                          const nsACString& aPrettyName,
                          nsISupports* aCert);

  enum AnnotationValue { AnnotationEnabled=1, AnnotationDisabled };

  nsresult SetCapability(const char* capability, void** annotation, 
                         AnnotationValue value);

  static const char sInvalid[];

protected:
  // Formerly an IDL method. Now just a protected helper.
  nsresult SetCanEnableCapability(const char* capability, PRInt16 canEnable);

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

#ifdef DEBUG
  virtual void dumpImpl() = 0;
#endif

  // Keep this is a pointer, even though it may slightly increase the
  // cost of keeping a certificate, this is a good tradeoff though since
  // it is very rare that we actually have a certificate.
  nsAutoPtr<Certificate> mCert;

  DomainPolicy* mSecurityPolicy;

  nsCOMPtr<nsIContentSecurityPolicy> mCSP;
  bool mTrusted;
};

class nsPrincipal : public nsBasePrincipal
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSISERIALIZABLE
  NS_IMETHOD Equals(nsIPrincipal* other, bool* _retval NS_OUTPARAM);
  NS_IMETHOD EqualsIgnoringDomain(nsIPrincipal* other, bool* _retval NS_OUTPARAM);
  NS_IMETHOD GetHashValue(PRUint32* aHashValue);
  NS_IMETHOD GetURI(nsIURI** aURI);
  NS_IMETHOD GetDomain(nsIURI** aDomain);
  NS_IMETHOD SetDomain(nsIURI* aDomain);
  NS_IMETHOD GetOrigin(char** aOrigin);
  NS_IMETHOD Subsumes(nsIPrincipal* other, bool* _retval NS_OUTPARAM);
  NS_IMETHOD SubsumesIgnoringDomain(nsIPrincipal* other, bool* _retval NS_OUTPARAM);
  NS_IMETHOD CheckMayLoad(nsIURI* uri, bool report);
  NS_IMETHOD GetExtendedOrigin(nsACString& aExtendedOrigin);
  NS_IMETHOD GetAppStatus(PRUint16* aAppStatus);
  NS_IMETHOD GetAppId(PRUint32* aAppStatus);
#ifdef DEBUG
  virtual void dumpImpl();
#endif

  nsPrincipal();

  // Either Init() or InitFromPersistent() must be called before
  // the principal is in a usable state.
  nsresult Init(const nsACString& aCertFingerprint,
                const nsACString& aSubjectName,
                const nsACString& aPrettyName,
                nsISupports* aCert,
                nsIURI* aCodebase,
                PRUint32 aAppId,
                bool aInMozBrowser);
  nsresult InitFromPersistent(const char* aPrefName,
                              const nsCString& aFingerprint,
                              const nsCString& aSubjectName,
                              const nsACString& aPrettyName,
                              const char* aGrantedList,
                              const char* aDeniedList,
                              nsISupports* aCert,
                              bool aIsCert,
                              bool aTrusted,
                              PRUint32 aAppId,
                              bool aInMozBrowser);

  virtual void GetScriptLocation(nsACString& aStr) MOZ_OVERRIDE;
  void SetURI(nsIURI* aURI);

  /**
   * Computes the puny-encoded origin of aURI.
   */
  static nsresult GetOriginForURI(nsIURI* aURI, char **aOrigin);

  nsCOMPtr<nsIURI> mDomain;
  nsCOMPtr<nsIURI> mCodebase;
  PRUint32 mAppId;
  bool mInMozBrowser;
  // If mCodebaseImmutable is true, mCodebase is non-null and immutable
  bool mCodebaseImmutable;
  bool mDomainImmutable;
  bool mInitialized;

protected:
  virtual ~nsPrincipal();

  /**
   * Returns the app status of the principal based on mAppId and mInMozBrowser.
   */
  PRUint16 GetAppStatus();
};

class nsExpandedPrincipal : public nsIExpandedPrincipal, public nsBasePrincipal
{
public:
  nsExpandedPrincipal(nsTArray< nsCOMPtr<nsIPrincipal> > &aWhiteList);

protected:
  virtual ~nsExpandedPrincipal();

public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIEXPANDEDPRINCIPAL
  NS_DECL_NSISERIALIZABLE
  NS_IMETHOD Equals(nsIPrincipal* other, bool* _retval NS_OUTPARAM);
  NS_IMETHOD EqualsIgnoringDomain(nsIPrincipal* other, bool* _retval NS_OUTPARAM);
  NS_IMETHOD GetHashValue(PRUint32* aHashValue);
  NS_IMETHOD GetURI(nsIURI** aURI);
  NS_IMETHOD GetDomain(nsIURI** aDomain);
  NS_IMETHOD SetDomain(nsIURI* aDomain);
  NS_IMETHOD GetOrigin(char** aOrigin);
  NS_IMETHOD Subsumes(nsIPrincipal* other, bool* _retval NS_OUTPARAM);
  NS_IMETHOD SubsumesIgnoringDomain(nsIPrincipal* other, bool* _retval NS_OUTPARAM);
  NS_IMETHOD CheckMayLoad(nsIURI* uri, bool report);
  NS_IMETHOD GetExtendedOrigin(nsACString& aExtendedOrigin);
  NS_IMETHOD GetAppStatus(PRUint16* aAppStatus);
  NS_IMETHOD GetAppId(PRUint32* aAppStatus);
#ifdef DEBUG
  virtual void dumpImpl();
#endif
  
  virtual void GetScriptLocation(nsACString &aStr) MOZ_OVERRIDE;

private:
  nsTArray< nsCOMPtr<nsIPrincipal> > mPrincipals;
};

#define NS_PRINCIPAL_CLASSNAME  "principal"
#define NS_PRINCIPAL_CONTRACTID "@mozilla.org/principal;1"
#define NS_PRINCIPAL_CID \
  { 0x36102b6b, 0x7b62, 0x451a, \
    { 0xa1, 0xc8, 0xa0, 0xd4, 0x56, 0xc9, 0x2d, 0xc5 }}

#define NS_EXPANDEDPRINCIPAL_CLASSNAME  "expandedprincipal"
#define NS_EXPANDEDPRINCIPAL_CONTRACTID "@mozilla.org/expandedprincipal;1"
#define NS_EXPANDEDPRINCIPAL_CID \
  { 0xb33a3807, 0xb76c, 0x44e5, \
    { 0xb9, 0x9d, 0x95, 0x7e, 0xe9, 0xba, 0x6e, 0x39 }}

#endif // nsPrincipal_h__
