/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nscore.h"
#include "nsNodeInfo.h"
#include "nsNodeInfoManager.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsIAtom.h"
#include "nsDOMString.h"
#include "nsCRT.h"
#include "nsContentUtils.h"
#include "nsReadableUtils.h"

// static
nsNodeInfo*
nsNodeInfo::Create()
{
  if (sCachedNodeInfo) {
    // We have cached unused instances of this class, return a cached
    // instance instead of always creating a new one.
    nsNodeInfo *nodeInfo = sCachedNodeInfo;
    sCachedNodeInfo = nsnull;
    return nodeInfo;
  }

  // Create a new one
  return new nsNodeInfo();
}

nsNodeInfo::nsNodeInfo()
{
}


nsNodeInfo::~nsNodeInfo()
{
  Clear();
}

void
nsNodeInfo::Clear()
{
  if (mOwnerManager) {
    mOwnerManager->RemoveNodeInfo(this);
    NS_RELEASE(mOwnerManager);
  }

  NS_IF_RELEASE(mInner.mName);
  NS_IF_RELEASE(mInner.mPrefix);
}


nsresult
nsNodeInfo::Init(nsIAtom *aName, nsIAtom *aPrefix, PRInt32 aNamespaceID,
                 nsNodeInfoManager *aOwnerManager)
{
  NS_ENSURE_TRUE(!mInner.mName && !mInner.mPrefix && !mOwnerManager,
                 NS_ERROR_ALREADY_INITIALIZED);
  NS_ENSURE_ARG_POINTER(aName);
  NS_ENSURE_ARG_POINTER(aOwnerManager);

  mInner.mName = aName;
  NS_ADDREF(mInner.mName);

  mInner.mPrefix = aPrefix;
  NS_IF_ADDREF(mInner.mPrefix);

  mInner.mNamespaceID = aNamespaceID;

  mOwnerManager = aOwnerManager;
  NS_ADDREF(mOwnerManager);

  return NS_OK;
}


// nsISupports

NS_IMPL_ADDREF(nsNodeInfo)
NS_IMPL_RELEASE_WITH_DESTROY(nsNodeInfo, LastRelease())
NS_IMPL_QUERY_INTERFACE1(nsNodeInfo, nsINodeInfo)

// nsINodeInfo

void
nsNodeInfo::GetQualifiedName(nsAString& aQualifiedName) const
{
  if (mInner.mPrefix) {
    mInner.mPrefix->ToString(aQualifiedName);

    aQualifiedName.Append(PRUnichar(':'));
  } else {
    aQualifiedName.Truncate();
  }

  nsAutoString name;
  mInner.mName->ToString(name);

  aQualifiedName.Append(name);
}


void
nsNodeInfo::GetLocalName(nsAString& aLocalName) const
{
#ifdef STRICT_DOM_LEVEL2_LOCALNAME
  if (mInner.mNamespaceID > 0) {
    mInner.mName->ToString(aLocalName);
  } else {
    SetDOMStringToNull(aLocalName);
  }
#else
  mInner.mName->ToString(aLocalName);
#endif
}


nsresult
nsNodeInfo::GetNamespaceURI(nsAString& aNameSpaceURI) const
{
  nsresult rv = NS_OK;

  if (mInner.mNamespaceID > 0) {
    rv = nsContentUtils::GetNSManagerWeakRef()->GetNameSpaceURI(mInner.mNamespaceID,
                                                                aNameSpaceURI);
  } else {
    SetDOMStringToNull(aNameSpaceURI);
  }

  return rv;
}


PRBool
nsNodeInfo::Equals(const nsAString& aName) const
{
  return mInner.mName->Equals(aName);
}


PRBool
nsNodeInfo::Equals(const nsAString& aName, const nsAString& aPrefix) const
{
  if (!mInner.mName->Equals(aName)) {
    return PR_FALSE;
  }

  if (!mInner.mPrefix) {
    return aPrefix.IsEmpty();
  }

  return mInner.mPrefix->Equals(aPrefix);
}


PRBool
nsNodeInfo::Equals(const nsAString& aName, PRInt32 aNamespaceID) const
{
  return mInner.mNamespaceID == aNamespaceID &&
    mInner.mName->Equals(aName);
}


PRBool
nsNodeInfo::Equals(const nsAString& aName, const nsAString& aPrefix,
                   PRInt32 aNamespaceID) const
{
  if (!mInner.mNamespaceID == aNamespaceID ||
      !mInner.mName->Equals(aName))
    return PR_FALSE;

  return mInner.mPrefix ? mInner.mPrefix->Equals(aPrefix) :
    aPrefix.IsEmpty();
}


PRBool
nsNodeInfo::NamespaceEquals(const nsAString& aNamespaceURI) const
{
  PRInt32 nsid;
  nsContentUtils::GetNSManagerWeakRef()->GetNameSpaceID(aNamespaceURI, &nsid);

  return nsINodeInfo::NamespaceEquals(nsid);
}

PRBool
nsNodeInfo::QualifiedNameEquals(const nsACString& aQualifiedName) const
{
  
  if (!mInner.mPrefix)
    return mInner.mName->EqualsUTF8(aQualifiedName);

  nsACString::const_iterator start;
  aQualifiedName.BeginReading(start);

  nsACString::const_iterator colon(start);

  const char* prefix;
  mInner.mPrefix->GetUTF8String(&prefix);

  PRUint32 len = strlen(prefix);

  if (len >= aQualifiedName.Length()) {
    return PR_FALSE;
  }

  colon.advance(len);

  // If the character at the prefix length index is not a colon,
  // aQualifiedName is not equal to this string.
  if (*colon != ':') {
    return PR_FALSE;
  }

  // Compare the prefix to the string from the start to the colon
  if (!mInner.mPrefix->EqualsUTF8(Substring(start, colon)))
    return PR_FALSE;

  ++colon; // Skip the ':'

  nsACString::const_iterator end;
  aQualifiedName.EndReading(end);

  // Compare the local name to the string between the colon and the
  // end of aQualifiedName
  return mInner.mName->EqualsUTF8(Substring(colon, end));
}

// static
nsNodeInfo *nsNodeInfo::sCachedNodeInfo = nsnull;

// static
void
nsNodeInfo::ClearCache()
{
  // Clear our cache.
  delete sCachedNodeInfo;
  sCachedNodeInfo = nsnull;
}

void
nsNodeInfo::LastRelease()
{
  if (sCachedNodeInfo) {
    // No room in cache
    delete this;
    return;
  }

  // There's space in the cache for one instance. Put
  // this instance in the cache instead of deleting it.
  sCachedNodeInfo = this;

  // Clear object so that we have no references to anything external
  Clear();

  // The refcount balancing and destructor re-entrancy protection
  // code in Release() sets mRefCnt to 1 so we have to set it to 0
  // here to prevent leaks
  mRefCnt = 0;
}
