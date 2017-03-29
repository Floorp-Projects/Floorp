/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ExpandedPrincipal.h"
#include "nsIClassInfoImpl.h"

using namespace mozilla;

NS_IMPL_CLASSINFO(ExpandedPrincipal, nullptr, nsIClassInfo::MAIN_THREAD_ONLY,
                  NS_EXPANDEDPRINCIPAL_CID)
NS_IMPL_QUERY_INTERFACE_CI(ExpandedPrincipal,
                           nsIPrincipal,
                           nsIExpandedPrincipal)
NS_IMPL_CI_INTERFACE_GETTER(ExpandedPrincipal,
                            nsIPrincipal,
                            nsIExpandedPrincipal)

struct OriginComparator
{
  bool LessThan(nsIPrincipal* a, nsIPrincipal* b) const
  {
    nsAutoCString originA;
    nsresult rv = a->GetOrigin(originA);
    NS_ENSURE_SUCCESS(rv, false);
    nsAutoCString originB;
    rv = b->GetOrigin(originB);
    NS_ENSURE_SUCCESS(rv, false);
    return originA < originB;
  }

  bool Equals(nsIPrincipal* a, nsIPrincipal* b) const
  {
    nsAutoCString originA;
    nsresult rv = a->GetOrigin(originA);
    NS_ENSURE_SUCCESS(rv, false);
    nsAutoCString originB;
    rv = b->GetOrigin(originB);
    NS_ENSURE_SUCCESS(rv, false);
    return a == b;
  }
};

ExpandedPrincipal::ExpandedPrincipal(nsTArray<nsCOMPtr<nsIPrincipal>> &aWhiteList,
                                     const OriginAttributes& aAttrs)
  : BasePrincipal(eExpandedPrincipal)
{
  // We force the principals to be sorted by origin so that ExpandedPrincipal
  // origins can have a canonical form.
  OriginComparator c;
  for (size_t i = 0; i < aWhiteList.Length(); ++i) {
    mPrincipals.InsertElementSorted(aWhiteList[i], c);
  }
  mOriginAttributes = aAttrs;
}

ExpandedPrincipal::~ExpandedPrincipal()
{ }

already_AddRefed<ExpandedPrincipal>
ExpandedPrincipal::Create(nsTArray<nsCOMPtr<nsIPrincipal>>& aWhiteList,
                          const OriginAttributes& aAttrs)
{
  RefPtr<ExpandedPrincipal> ep = new ExpandedPrincipal(aWhiteList, aAttrs);
  ep->FinishInit();
  return ep.forget();
}

NS_IMETHODIMP
ExpandedPrincipal::GetDomain(nsIURI** aDomain)
{
  *aDomain = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
ExpandedPrincipal::SetDomain(nsIURI* aDomain)
{
  return NS_OK;
}

nsresult
ExpandedPrincipal::GetOriginInternal(nsACString& aOrigin)
{
  aOrigin.AssignLiteral("[Expanded Principal [");
  for (size_t i = 0; i < mPrincipals.Length(); ++i) {
    if (i != 0) {
      aOrigin.AppendLiteral(", ");
    }

    nsAutoCString subOrigin;
    nsresult rv = mPrincipals.ElementAt(i)->GetOrigin(subOrigin);
    NS_ENSURE_SUCCESS(rv, rv);
    aOrigin.Append(subOrigin);
  }

  aOrigin.Append("]]");
  return NS_OK;
}

bool
ExpandedPrincipal::SubsumesInternal(nsIPrincipal* aOther,
                                    BasePrincipal::DocumentDomainConsideration aConsideration)
{
  // If aOther is an ExpandedPrincipal too, we break it down into its component
  // nsIPrincipals, and check subsumes on each one.
  nsCOMPtr<nsIExpandedPrincipal> expanded = do_QueryInterface(aOther);
  if (expanded) {
    nsTArray< nsCOMPtr<nsIPrincipal> >* otherList;
    expanded->GetWhiteList(&otherList);
    for (uint32_t i = 0; i < otherList->Length(); ++i){
      // Use SubsumesInternal rather than Subsumes here, since OriginAttribute
      // checks are only done between non-expanded sub-principals, and we don't
      // need to incur the extra virtual call overhead.
      if (!SubsumesInternal((*otherList)[i], aConsideration)) {
        return false;
      }
    }
    return true;
  }

  // We're dealing with a regular principal. One of our principals must subsume
  // it.
  for (uint32_t i = 0; i < mPrincipals.Length(); ++i) {
    if (Cast(mPrincipals[i])->Subsumes(aOther, aConsideration)) {
      return true;
    }
  }

  return false;
}

bool
ExpandedPrincipal::MayLoadInternal(nsIURI* uri)
{
  for (uint32_t i = 0; i < mPrincipals.Length(); ++i){
    if (BasePrincipal::Cast(mPrincipals[i])->MayLoadInternal(uri)) {
      return true;
    }
  }

  return false;
}

NS_IMETHODIMP
ExpandedPrincipal::GetHashValue(uint32_t* result)
{
  MOZ_CRASH("extended principal should never be used as key in a hash map");
}

NS_IMETHODIMP
ExpandedPrincipal::GetURI(nsIURI** aURI)
{
  *aURI = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
ExpandedPrincipal::GetWhiteList(nsTArray<nsCOMPtr<nsIPrincipal> >** aWhiteList)
{
  *aWhiteList = &mPrincipals;
  return NS_OK;
}

NS_IMETHODIMP
ExpandedPrincipal::GetBaseDomain(nsACString& aBaseDomain)
{
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
ExpandedPrincipal::GetAddonId(nsAString& aAddonId)
{
  aAddonId.Truncate();
  return NS_OK;
};

bool
ExpandedPrincipal::AddonHasPermission(const nsAString& aPerm)
{
  for (size_t i = 0; i < mPrincipals.Length(); ++i) {
    if (BasePrincipal::Cast(mPrincipals[i])->AddonHasPermission(aPerm)) {
      return true;
    }
  }
  return false;
}

nsresult
ExpandedPrincipal::GetScriptLocation(nsACString& aStr)
{
  aStr.Assign("[Expanded Principal [");
  for (size_t i = 0; i < mPrincipals.Length(); ++i) {
    if (i != 0) {
      aStr.AppendLiteral(", ");
    }

    nsAutoCString spec;
    nsresult rv =
      nsJSPrincipals::get(mPrincipals.ElementAt(i))->GetScriptLocation(spec);
    NS_ENSURE_SUCCESS(rv, rv);

    aStr.Append(spec);
  }
  aStr.Append("]]");
  return NS_OK;
}

//////////////////////////////////////////
// Methods implementing nsISerializable //
//////////////////////////////////////////

NS_IMETHODIMP
ExpandedPrincipal::Read(nsIObjectInputStream* aStream)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ExpandedPrincipal::Write(nsIObjectOutputStream* aStream)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}
