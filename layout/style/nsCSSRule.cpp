/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#include "nsCSSRule.h"
#include "nsCRT.h"
#include "nsIArena.h"
#include "nsICSSStyleSheet.h"

void* nsCSSRule::operator new(size_t size)
{
  nsCSSRule* rv = (nsCSSRule*) ::operator new(size);
#ifdef NS_DEBUG
  if (nsnull != rv) {
    nsCRT::memset(rv, 0xEE, size);
  }
#endif
  rv->mInHeap = 1;
  return (void*) rv;
}

void* nsCSSRule::operator new(size_t size, nsIArena* aArena)
{
  nsCSSRule* rv = (nsCSSRule*) aArena->Alloc(PRInt32(size));
#ifdef NS_DEBUG
  if (nsnull != rv) {
    nsCRT::memset(rv, 0xEE, size);
  }
#endif
  rv->mInHeap = 0;
  return (void*) rv;
}

void nsCSSRule::operator delete(void* ptr)
{
  nsCSSRule* rule = (nsCSSRule*) ptr;
  if (nsnull != rule) {
    if (rule->mInHeap) {
      ::operator delete(ptr);
    }
  }
}

nsCSSRule::nsCSSRule(void)
  : mSheet(nsnull),
    mParentRule(nsnull)
{
  NS_INIT_REFCNT();
}

nsCSSRule::nsCSSRule(const nsCSSRule& aCopy)
  : mSheet(aCopy.mSheet),
    mParentRule(aCopy.mParentRule)
{
  NS_INIT_REFCNT();
}


nsCSSRule::~nsCSSRule(void)
{
}

NS_IMPL_ADDREF(nsCSSRule)
NS_IMPL_RELEASE(nsCSSRule)

NS_IMETHODIMP
nsCSSRule::GetStyleSheet(nsIStyleSheet*& aSheet) const
{
  NS_IF_ADDREF(mSheet);
  aSheet = mSheet;
  return NS_OK;
}

NS_IMETHODIMP
nsCSSRule::SetStyleSheet(nsICSSStyleSheet* aSheet)
{
  // We don't reference count this up reference. The style sheet
  // will tell us when it's going away or when we're detached from
  // it.
  mSheet = aSheet;
  return NS_OK;
}

NS_IMETHODIMP
nsCSSRule::SetParentRule(nsICSSGroupRule* aRule)
{
  // We don't reference count this up reference. The group rule
  // will tell us when it's going away or when we're detached from
  // it.
  mParentRule = aRule;
  return NS_OK;
}

NS_IMETHODIMP
nsCSSRule::GetStrength(PRInt32& aStrength) const
{
  aStrength = 0;
  return NS_OK;
}

NS_IMETHODIMP
nsCSSRule::MapRuleInfoInto(nsRuleData* aRuleData)
{
  return NS_OK;
}
