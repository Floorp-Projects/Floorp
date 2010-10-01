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

/*
 * Class that represents a prefix/namespace/localName triple; a single
 * nodeinfo is shared by all elements in a document that have that
 * prefix, namespace, and localName.
 */

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
#include "nsAutoPtr.h"
#include NEW_H
#include "nsFixedSizeAllocator.h"
#include "prprf.h"

static const size_t kNodeInfoPoolSizes[] = {
  sizeof(nsNodeInfo)
};

static const PRInt32 kNodeInfoPoolInitialSize = 
  (NS_SIZE_IN_HEAP(sizeof(nsNodeInfo))) * 64;

// static
nsFixedSizeAllocator* nsNodeInfo::sNodeInfoPool = nsnull;

// static
nsNodeInfo*
nsNodeInfo::Create()
{
  if (!sNodeInfoPool) {
    sNodeInfoPool = new nsFixedSizeAllocator();
    if (!sNodeInfoPool)
      return nsnull;

    nsresult rv = sNodeInfoPool->Init("NodeInfo Pool", kNodeInfoPoolSizes,
                                      1, kNodeInfoPoolInitialSize);
    if (NS_FAILED(rv)) {
      delete sNodeInfoPool;
      sNodeInfoPool = nsnull;
      return nsnull;
    }
  }

  // Create a new one
  void* place = sNodeInfoPool->Alloc(sizeof(nsNodeInfo));
  return place ? new (place) nsNodeInfo() : nsnull;
}

nsNodeInfo::nsNodeInfo()
{
}

nsNodeInfo::~nsNodeInfo()
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

NS_IMPL_CYCLE_COLLECTION_CLASS(nsNodeInfo)
NS_IMPL_CYCLE_COLLECTION_UNLINK_0(nsNodeInfo)

static const char* kNSURIs[] = {
  " ([none])",
  " (xmlns)",
  " (xml)",
  " (xhtml)",
  " (XLink)",
  " (XSLT)",
  " (XBL)",
  " (MathML)",
  " (RDF)",
  " (XUL)"
};

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INTERNAL(nsNodeInfo)
  if (NS_UNLIKELY(cb.WantDebugInfo())) {
    char name[72];
    PRUint32 nsid = tmp->NamespaceID();
    nsAtomCString localName(tmp->NameAtom());
    if (nsid < NS_ARRAY_LENGTH(kNSURIs)) {
      PR_snprintf(name, sizeof(name), "nsNodeInfo%s %s", kNSURIs[nsid],
                  localName.get());
    }
    else {
      PR_snprintf(name, sizeof(name), "nsNodeInfo %s", localName.get());
    }

    cb.DescribeNode(RefCounted, tmp->mRefCnt.get(), sizeof(nsNodeInfo), name);
  }
  else {
    cb.DescribeNode(RefCounted, tmp->mRefCnt.get(), sizeof(nsNodeInfo),
                    "nsNodeInfo");
  }

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NATIVE_MEMBER(mOwnerManager,
                                                  nsNodeInfoManager)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsNodeInfo)
NS_IMPL_CYCLE_COLLECTING_RELEASE_WITH_DESTROY(nsNodeInfo, LastRelease())
NS_INTERFACE_TABLE_HEAD(nsNodeInfo)
  NS_INTERFACE_TABLE1(nsNodeInfo, nsINodeInfo)
  NS_INTERFACE_TABLE_TO_MAP_SEGUE_CYCLE_COLLECTION(nsNodeInfo)
NS_INTERFACE_MAP_END

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
    rv = nsContentUtils::NameSpaceManager()->GetNameSpaceURI(mInner.mNamespaceID,
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
  PRInt32 nsid =
    nsContentUtils::NameSpaceManager()->GetNameSpaceID(aNamespaceURI);

  return nsINodeInfo::NamespaceEquals(nsid);
}

PRBool
nsNodeInfo::QualifiedNameEqualsInternal(const nsAString& aQualifiedName) const
{
  NS_PRECONDITION(mInner.mPrefix, "Must have prefix");
  
  nsAString::const_iterator start;
  aQualifiedName.BeginReading(start);

  nsAString::const_iterator colon(start);

  nsDependentAtomString prefix(mInner.mPrefix);

  if (prefix.Length() >= aQualifiedName.Length()) {
    return PR_FALSE;
  }

  colon.advance(prefix.Length());

  // If the character at the prefix length index is not a colon,
  // aQualifiedName is not equal to this string.
  if (*colon != ':') {
    return PR_FALSE;
  }

  // Compare the prefix to the string from the start to the colon
  if (!prefix.Equals(Substring(start, colon)))
    return PR_FALSE;

  ++colon; // Skip the ':'

  nsAString::const_iterator end;
  aQualifiedName.EndReading(end);

  // Compare the local name to the string between the colon and the
  // end of aQualifiedName
  return mInner.mName->Equals(Substring(colon, end));
}

// static
void
nsNodeInfo::ClearCache()
{
  // Clear our cache.
  delete sNodeInfoPool;
  sNodeInfoPool = nsnull;
}

void
nsNodeInfo::LastRelease()
{
  nsRefPtr<nsNodeInfoManager> kungFuDeathGrip = mOwnerManager;
  this->~nsNodeInfo();

  // The refcount balancing and destructor re-entrancy protection
  // code in Release() sets mRefCnt to 1 so we have to set it to 0
  // here to prevent leaks
  mRefCnt = 0;

  NS_ASSERTION(sNodeInfoPool, "No NodeInfoPool when deleting NodeInfo!!!");
  sNodeInfoPool->Free(this, sizeof(nsNodeInfo));
}
