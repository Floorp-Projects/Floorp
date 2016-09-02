/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPrincipal_h__
#define nsPrincipal_h__

#include "nsCOMPtr.h"
#include "nsJSPrincipals.h"
#include "nsTArray.h"
#include "nsIContentSecurityPolicy.h"
#include "nsIProtocolHandler.h"
#include "nsNetUtil.h"
#include "nsScriptSecurityManager.h"
#include "mozilla/BasePrincipal.h"

class nsPrincipal final : public mozilla::BasePrincipal
{
public:
  NS_DECL_NSISERIALIZABLE
  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr) override;
  NS_IMETHOD GetHashValue(uint32_t* aHashValue) override;
  NS_IMETHOD GetURI(nsIURI** aURI) override;
  NS_IMETHOD GetDomain(nsIURI** aDomain) override;
  NS_IMETHOD SetDomain(nsIURI* aDomain) override;
  NS_IMETHOD GetBaseDomain(nsACString& aBaseDomain) override;
  virtual bool IsOnCSSUnprefixingWhitelist() override;
  bool IsCodebasePrincipal() const override { return true; }
  nsresult GetOriginInternal(nsACString& aOrigin) override;

  nsPrincipal();

  // Init() must be called before the principal is in a usable state.
  nsresult Init(nsIURI* aCodebase, const mozilla::PrincipalOriginAttributes& aOriginAttributes);

  virtual void GetScriptLocation(nsACString& aStr) override;
  void SetURI(nsIURI* aURI);

  /**
   * Computes the puny-encoded origin of aURI.
   */
  static nsresult GetOriginForURI(nsIURI* aURI, nsACString& aOrigin);

  /**
   * Called at startup to setup static data, e.g. about:config pref-observers.
   */
  static void InitializeStatics();

  PrincipalKind Kind() override { return eCodebasePrincipal; }

  nsCOMPtr<nsIURI> mDomain;
  nsCOMPtr<nsIURI> mCodebase;
  // If mCodebaseImmutable is true, mCodebase is non-null and immutable
  bool mCodebaseImmutable;
  bool mDomainImmutable;
  bool mInitialized;
  mozilla::Maybe<bool> mIsOnCSSUnprefixingWhitelist; // Lazily-computed

protected:
  virtual ~nsPrincipal();

  bool SubsumesInternal(nsIPrincipal* aOther, DocumentDomainConsideration aConsideration) override;
  bool MayLoadInternal(nsIURI* aURI) override;
};

class nsExpandedPrincipal : public nsIExpandedPrincipal, public mozilla::BasePrincipal
{
public:
  explicit nsExpandedPrincipal(nsTArray< nsCOMPtr<nsIPrincipal> > &aWhiteList);

  NS_DECL_NSIEXPANDEDPRINCIPAL
  NS_DECL_NSISERIALIZABLE
  NS_IMETHOD_(MozExternalRefCountType) AddRef() override { return nsJSPrincipals::AddRef(); };
  NS_IMETHOD_(MozExternalRefCountType) Release() override { return nsJSPrincipals::Release(); };
  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr) override;
  NS_IMETHOD GetHashValue(uint32_t* aHashValue) override;
  NS_IMETHOD GetURI(nsIURI** aURI) override;
  NS_IMETHOD GetDomain(nsIURI** aDomain) override;
  NS_IMETHOD SetDomain(nsIURI* aDomain) override;
  NS_IMETHOD GetBaseDomain(nsACString& aBaseDomain) override;
  virtual bool AddonHasPermission(const nsAString& aPerm) override;
  virtual bool IsOnCSSUnprefixingWhitelist() override;
  virtual void GetScriptLocation(nsACString &aStr) override;
  nsresult GetOriginInternal(nsACString& aOrigin) override;

  PrincipalKind Kind() override { return eExpandedPrincipal; }

protected:
  virtual ~nsExpandedPrincipal();

  bool SubsumesInternal(nsIPrincipal* aOther, DocumentDomainConsideration aConsideration) override;
  bool MayLoadInternal(nsIURI* aURI) override;

private:
  nsTArray< nsCOMPtr<nsIPrincipal> > mPrincipals;
};

#define NS_PRINCIPAL_CONTRACTID "@mozilla.org/principal;1"
#define NS_PRINCIPAL_CID \
{ 0x653e0e4d, 0x3ee4, 0x45fa, \
  { 0xb2, 0x72, 0x97, 0xc2, 0x0b, 0xc0, 0x1e, 0xb8 } }

#define NS_EXPANDEDPRINCIPAL_CONTRACTID "@mozilla.org/expandedprincipal;1"
#define NS_EXPANDEDPRINCIPAL_CID \
{ 0xe8ee88b0, 0x5571, 0x4086, \
  { 0xa4, 0x5b, 0x39, 0xa7, 0x16, 0x90, 0x6b, 0xdb } }

#endif // nsPrincipal_h__
