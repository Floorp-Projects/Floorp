/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nscore.h"
#include "nsNodeInfo.h"
#include "nsNodeInfoManager.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsIAtom.h"
#include "domstubs.h" // for SetDOMStringToNull();
#include "nsCRT.h"
#include "nsContentUtils.h"
#include "nsReadableUtils.h"

nsNodeInfo::nsNodeInfo()
  : nsINodeInfo(), mOwnerManager(nsnull)
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

NS_IMPL_ISUPPORTS1(nsNodeInfo, nsINodeInfo);


// nsINodeInfo

NS_IMETHODIMP
nsNodeInfo::GetQualifiedName(nsAString& aQualifiedName) const
{
  if (mInner.mPrefix) {
    mInner.mPrefix->ToString(aQualifiedName);

    aQualifiedName.Append(PRUnichar(':'));
  } else {
    aQualifiedName.Truncate();
  }

  const PRUnichar *name;
  mInner.mName->GetUnicode(&name);

  aQualifiedName.Append(name);

  return NS_OK;
}


NS_IMETHODIMP
nsNodeInfo::GetLocalName(nsAString& aLocalName) const
{
#ifdef STRICT_DOM_LEVEL2_LOCALNAME
  if (mInner.mNamespaceID > 0) {
    return mInner.mName->ToString(aLocalName);
  }

  SetDOMStringToNull(aLocalName);

  return NS_OK;
#else
  return mInner.mName->ToString(aLocalName);
#endif
}


NS_IMETHODIMP
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


NS_IMETHODIMP
nsNodeInfo::GetIDAttributeAtom(nsIAtom** aResult) const
{
  *aResult = mIDAttributeAtom;
  NS_IF_ADDREF(*aResult);

  return NS_OK;
}

NS_IMETHODIMP
nsNodeInfo::SetIDAttributeAtom(nsIAtom* aID)
{
  NS_ENSURE_ARG(aID);
  mIDAttributeAtom = aID;

  return NS_OK;
}



NS_IMETHODIMP
nsNodeInfo::GetNodeInfoManager(nsINodeInfoManager*& aNodeInfoManager) const
{
  aNodeInfoManager = mOwnerManager;

  NS_ADDREF(aNodeInfoManager);

  return NS_OK;
}


NS_IMETHODIMP_(PRBool)
nsNodeInfo::Equals(const nsAString& aName) const
{
  const PRUnichar *name;
  mInner.mName->GetUnicode(&name);

  return aName.Equals(name);
}


NS_IMETHODIMP_(PRBool)
nsNodeInfo::Equals(const nsAString& aName, const nsAString& aPrefix) const
{
  const PRUnichar *name;
  mInner.mName->GetUnicode(&name);

  if (!aName.Equals(name)) {
    return PR_FALSE;
  }

  if (!mInner.mPrefix) {
    return aPrefix.IsEmpty();
  }

  mInner.mPrefix->GetUnicode(&name);

  return aPrefix.Equals(name);
}


NS_IMETHODIMP_(PRBool)
nsNodeInfo::Equals(const nsAString& aName, PRInt32 aNamespaceID) const
{
  const PRUnichar *name;
  mInner.mName->GetUnicode(&name);

  return aName.Equals(name) && (mInner.mNamespaceID == aNamespaceID);
}


NS_IMETHODIMP_(PRBool)
nsNodeInfo::Equals(const nsAString& aName, const nsAString& aPrefix,
                   PRInt32 aNamespaceID) const
{
  PRUnichar nullChar = '\0';
  const PRUnichar *name, *prefix = &nullChar;
  mInner.mName->GetUnicode(&name);

  if (mInner.mPrefix) {
    mInner.mPrefix->GetUnicode(&prefix);
  }

  return ((mInner.mNamespaceID == aNamespaceID) && aName.Equals(name) &&
          aPrefix.Equals(prefix));
}


NS_IMETHODIMP_(PRBool)
nsNodeInfo::NamespaceEquals(const nsAString& aNamespaceURI) const
{
  PRInt32 nsid;
  nsContentUtils::GetNSManagerWeakRef()->GetNameSpaceID(aNamespaceURI, nsid);

  return nsINodeInfo::NamespaceEquals(nsid);
}


NS_IMETHODIMP_(PRBool)
nsNodeInfo::QualifiedNameEquals(const nsAString& aQualifiedName) const
{
  const PRUnichar *name;
  mInner.mName->GetUnicode(&name);

  if (!mInner.mPrefix) {
    return aQualifiedName.Equals(name);
  }

  nsAString::const_iterator start;
  aQualifiedName.BeginReading(start);

  nsAString::const_iterator colon(start);

  const PRUnichar *prefix;
  mInner.mPrefix->GetUnicode(&prefix);

  PRUint32 len = nsCRT::strlen(prefix);

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
  if (!Substring(start, colon).Equals(prefix)) {
    return PR_FALSE;
  }

  ++colon; // Skip the ':'

  nsAString::const_iterator end;
  aQualifiedName.EndReading(end);

  // Compare the local name to the string between the colon and the
  // end of aQualifiedName
  return Substring(colon, end).Equals(name);
}

NS_IMETHODIMP
nsNodeInfo::NameChanged(nsIAtom *aName, nsINodeInfo*& aResult)
{
  return mOwnerManager->GetNodeInfo(aName, mInner.mPrefix, mInner.mNamespaceID,
                                    aResult);
}


NS_IMETHODIMP
nsNodeInfo::PrefixChanged(nsIAtom *aPrefix, nsINodeInfo*& aResult)
{
  return mOwnerManager->GetNodeInfo(mInner.mName, aPrefix, mInner.mNamespaceID,
                                    aResult);
}

NS_IMETHODIMP
nsNodeInfo::GetDocument(nsIDocument*& aDocument) const
{
  return mOwnerManager->GetDocument(aDocument);
}

NS_IMETHODIMP
nsNodeInfo::GetDocumentPrincipal(nsIPrincipal** aPrincipal) const
{
  return mOwnerManager->GetDocumentPrincipal(aPrincipal);
}
