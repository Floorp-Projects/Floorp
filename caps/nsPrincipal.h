/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPrincipal_h__
#define nsPrincipal_h__

#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsJSPrincipals.h"
#include "nsTArray.h"
#include "nsAutoPtr.h"
#include "nsIProtocolHandler.h"
#include "nsNetUtil.h"
#include "nsScriptSecurityManager.h"

class nsIObjectInputStream;
class nsIObjectOutputStream;

class nsBasePrincipal : public nsJSPrincipals
{
public:
  nsBasePrincipal();

protected:
  virtual ~nsBasePrincipal();

public:
  NS_IMETHOD_(MozExternalRefCountType) AddRef(void);
  NS_IMETHOD_(MozExternalRefCountType) Release(void);
  NS_IMETHOD GetCsp(nsIContentSecurityPolicy** aCsp);
  NS_IMETHOD SetCsp(nsIContentSecurityPolicy* aCsp);
public:

  static const char sInvalid[];

protected:

#ifdef DEBUG
  virtual void dumpImpl() = 0;
#endif

  nsCOMPtr<nsIContentSecurityPolicy> mCSP;
};

class nsPrincipal final : public nsBasePrincipal
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSISERIALIZABLE
  NS_IMETHOD Equals(nsIPrincipal* other, bool* _retval) override;
  NS_IMETHOD EqualsConsideringDomain(nsIPrincipal* other, bool* _retval) override;
  NS_IMETHOD GetHashValue(uint32_t* aHashValue) override;
  NS_IMETHOD GetURI(nsIURI** aURI) override;
  NS_IMETHOD GetDomain(nsIURI** aDomain) override;
  NS_IMETHOD SetDomain(nsIURI* aDomain) override;
  NS_IMETHOD GetOrigin(char** aOrigin) override;
  NS_IMETHOD Subsumes(nsIPrincipal* other, bool* _retval) override;
  NS_IMETHOD SubsumesConsideringDomain(nsIPrincipal* other, bool* _retval) override;
  NS_IMETHOD CheckMayLoad(nsIURI* uri, bool report, bool allowIfInheritsPrincipal) override;
  NS_IMETHOD GetJarPrefix(nsACString& aJarPrefix) override;
  NS_IMETHOD GetAppStatus(uint16_t* aAppStatus) override;
  NS_IMETHOD GetAppId(uint32_t* aAppStatus) override;
  NS_IMETHOD GetIsInBrowserElement(bool* aIsInBrowserElement) override;
  NS_IMETHOD GetUnknownAppId(bool* aUnknownAppId) override;
  NS_IMETHOD GetIsNullPrincipal(bool* aIsNullPrincipal) override;
  NS_IMETHOD GetBaseDomain(nsACString& aBaseDomain) override;
  virtual bool IsOnCSSUnprefixingWhitelist() override;
#ifdef DEBUG
  virtual void dumpImpl() override;
#endif

  nsPrincipal();

  // Init() must be called before the principal is in a usable state.
  nsresult Init(nsIURI* aCodebase,
                uint32_t aAppId,
                bool aInMozBrowser);

  virtual void GetScriptLocation(nsACString& aStr) override;
  void SetURI(nsIURI* aURI);

  static bool IsPrincipalInherited(nsIURI* aURI) {
    // return true if the loadee URI has
    // the URI_INHERITS_SECURITY_CONTEXT flag set.
    bool doesInheritSecurityContext;
    nsresult rv =
    NS_URIChainHasFlags(aURI,
                        nsIProtocolHandler::URI_INHERITS_SECURITY_CONTEXT,
                        &doesInheritSecurityContext);

    if (NS_SUCCEEDED(rv) && doesInheritSecurityContext) {
      return true;
    }

    return false;
  }


  /**
   * Computes the puny-encoded origin of aURI.
   */
  static nsresult GetOriginForURI(nsIURI* aURI, char **aOrigin);

  /**
   * Called at startup to setup static data, e.g. about:config pref-observers.
   */
  static void InitializeStatics();

  nsCOMPtr<nsIURI> mDomain;
  nsCOMPtr<nsIURI> mCodebase;
  uint32_t mAppId;
  bool mInMozBrowser;
  // If mCodebaseImmutable is true, mCodebase is non-null and immutable
  bool mCodebaseImmutable;
  bool mDomainImmutable;
  bool mInitialized;
  mozilla::Maybe<bool> mIsOnCSSUnprefixingWhitelist; // Lazily-computed

protected:
  virtual ~nsPrincipal();

  /**
   * Returns the app status of the principal based on mAppId and mInMozBrowser.
   */
  uint16_t GetAppStatus();
};

class nsExpandedPrincipal : public nsIExpandedPrincipal, public nsBasePrincipal
{
public:
  explicit nsExpandedPrincipal(nsTArray< nsCOMPtr<nsIPrincipal> > &aWhiteList);

protected:
  virtual ~nsExpandedPrincipal();

public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIEXPANDEDPRINCIPAL
  NS_DECL_NSISERIALIZABLE
  NS_IMETHOD Equals(nsIPrincipal* other, bool* _retval) override;
  NS_IMETHOD EqualsConsideringDomain(nsIPrincipal* other, bool* _retval) override;
  NS_IMETHOD GetHashValue(uint32_t* aHashValue) override;
  NS_IMETHOD GetURI(nsIURI** aURI) override;
  NS_IMETHOD GetDomain(nsIURI** aDomain) override;
  NS_IMETHOD SetDomain(nsIURI* aDomain) override;
  NS_IMETHOD GetOrigin(char** aOrigin) override;
  NS_IMETHOD Subsumes(nsIPrincipal* other, bool* _retval) override;
  NS_IMETHOD SubsumesConsideringDomain(nsIPrincipal* other, bool* _retval) override;
  NS_IMETHOD CheckMayLoad(nsIURI* uri, bool report, bool allowIfInheritsPrincipal) override;
  NS_IMETHOD GetJarPrefix(nsACString& aJarPrefix) override;
  NS_IMETHOD GetAppStatus(uint16_t* aAppStatus) override;
  NS_IMETHOD GetAppId(uint32_t* aAppStatus) override;
  NS_IMETHOD GetIsInBrowserElement(bool* aIsInBrowserElement) override;
  NS_IMETHOD GetUnknownAppId(bool* aUnknownAppId) override;
  NS_IMETHOD GetIsNullPrincipal(bool* aIsNullPrincipal) override;
  NS_IMETHOD GetBaseDomain(nsACString& aBaseDomain) override;
  virtual bool IsOnCSSUnprefixingWhitelist() override;
#ifdef DEBUG
  virtual void dumpImpl() override;
#endif
  
  virtual void GetScriptLocation(nsACString &aStr) override;

private:
  nsTArray< nsCOMPtr<nsIPrincipal> > mPrincipals;
};

#define NS_PRINCIPAL_CONTRACTID "@mozilla.org/principal;1"
#define NS_PRINCIPAL_CID \
  { 0x09b7e598, 0x490d, 0x423f, \
    { 0xa8, 0xa6, 0x2e, 0x6c, 0x4e, 0xc8, 0x77, 0x50 }}

#define NS_EXPANDEDPRINCIPAL_CONTRACTID "@mozilla.org/expandedprincipal;1"
#define NS_EXPANDEDPRINCIPAL_CID \
  { 0xb33a3807, 0xb76c, 0x44e5, \
    { 0xb9, 0x9d, 0x95, 0x7e, 0xe9, 0xba, 0x6e, 0x39 }}

#endif // nsPrincipal_h__
