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

#include "nsNodeInfoManager.h"
#include "nsNodeInfo.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsIAtom.h"
#include "nsIDocument.h"
#include "nsIPrincipal.h"
#include "nsIURI.h"
#include "nsArray.h"
#include "nsContentUtils.h"
#include "nsReadableUtils.h"

PRUint32 nsNodeInfoManager::gNodeManagerCount;

PLHashNumber
nsNodeInfoManager::GetNodeInfoInnerHashValue(const void *key)
{
  NS_ASSERTION(key, "Null key passed to nsNodeInfo::GetHashValue!");

  const nsINodeInfo::nsNodeInfoInner *node =
    NS_REINTERPRET_CAST(const nsINodeInfo::nsNodeInfoInner *, key);

  // Is this an acceptable hash value?
  return (PLHashNumber(NS_PTR_TO_INT32(node->mName)) & 0xffff) >> 8;
}


PRIntn
nsNodeInfoManager::NodeInfoInnerKeyCompare(const void *key1, const void *key2)
{
  NS_ASSERTION(key1 && key2, "Null key passed to NodeInfoInnerKeyCompare!");

  const nsINodeInfo::nsNodeInfoInner *node1 =
    NS_REINTERPRET_CAST(const nsINodeInfo::nsNodeInfoInner *, key1);
  const nsINodeInfo::nsNodeInfoInner *node2 =
    NS_REINTERPRET_CAST(const nsINodeInfo::nsNodeInfoInner *, key2);

  return (node1->mName == node2->mName &&
          node1->mPrefix == node2->mPrefix &&
          node1->mNamespaceID == node2->mNamespaceID);
}


nsNodeInfoManager::nsNodeInfoManager() : mDocument(nsnull)
{
  ++gNodeManagerCount;

  mNodeInfoHash = PL_NewHashTable(32, GetNodeInfoInnerHashValue,
                                  NodeInfoInnerKeyCompare,
                                  PL_CompareValues, nsnull, nsnull);

#ifdef DEBUG_jst
  printf ("Creating NodeInfoManager, gcount = %d\n", gNodeManagerCount);
#endif
}


nsNodeInfoManager::~nsNodeInfoManager()
{
  --gNodeManagerCount;

  if (mNodeInfoHash)
    PL_HashTableDestroy(mNodeInfoHash);


  if (gNodeManagerCount == 0) {
    nsNodeInfo::ClearCache();
  }

#ifdef DEBUG_jst
  printf ("Removing NodeInfoManager, gcount = %d\n", gNodeManagerCount);
#endif
}


nsrefcnt
nsNodeInfoManager::AddRef()
{
  NS_PRECONDITION(PRInt32(mRefCnt) >= 0, "illegal refcnt");

  nsrefcnt count = PR_AtomicIncrement((PRInt32*)&mRefCnt);
  NS_LOG_ADDREF(this, count, "nsNodeInfoManager", sizeof(*this));

  return count;
}

nsrefcnt
nsNodeInfoManager::Release()
{
  NS_PRECONDITION(0 != mRefCnt, "dup release");

  nsrefcnt count = PR_AtomicDecrement((PRInt32 *)&mRefCnt);
  NS_LOG_RELEASE(this, count, "nsNodeInfoManager");
  if (count == 0) {
    mRefCnt = 1; /* stabilize */
    delete this;
  }

  return count;
}

nsresult
nsNodeInfoManager::Init(nsIDocument *aDocument)
{
  NS_ENSURE_TRUE(mNodeInfoHash, NS_ERROR_OUT_OF_MEMORY);

  mDocument = aDocument;
  if (aDocument) {
    mPrincipal = nsnull;
  }

  return NS_OK;
}

void
nsNodeInfoManager::DropDocumentReference()
{
  if (mDocument) {
    // If the document has a uri we'll ask for it's principal. Otherwise we'll
    // consider this document 'anonymous'. We don't want to call GetPrincipal
    // on a document that doesn't have a URI since that'll give an assertion
    // that we're creating a principal without having a uri.
    // This happens in a few cases where a document is created and then
    // immediately dropped without ever getting a URI.
    if (mDocument->GetDocumentURI()) {
      mPrincipal = mDocument->GetPrincipal();
    }
  }
  mDocument = nsnull;
}


nsresult
nsNodeInfoManager::GetNodeInfo(nsIAtom *aName, nsIAtom *aPrefix,
                               PRInt32 aNamespaceID, nsINodeInfo** aNodeInfo)
{
  NS_ENSURE_ARG_POINTER(aName);
  NS_ASSERTION(!aName->Equals(EmptyString()),
               "Don't pass an empty string to GetNodeInfo, fix caller.");

  nsINodeInfo::nsNodeInfoInner tmpKey(aName, aPrefix, aNamespaceID);

  void *node = PL_HashTableLookup(mNodeInfoHash, &tmpKey);

  if (node) {
    *aNodeInfo = NS_STATIC_CAST(nsINodeInfo *, node);

    NS_ADDREF(*aNodeInfo);

    return NS_OK;
  }

  nsNodeInfo *newNodeInfo = nsNodeInfo::Create();
  NS_ENSURE_TRUE(newNodeInfo, NS_ERROR_OUT_OF_MEMORY);

  NS_ADDREF(newNodeInfo);

  nsresult rv = newNodeInfo->Init(aName, aPrefix, aNamespaceID, this);
  NS_ENSURE_SUCCESS(rv, rv);

  PLHashEntry *he;
  he = PL_HashTableAdd(mNodeInfoHash, &newNodeInfo->mInner, newNodeInfo);
  NS_ENSURE_TRUE(he, NS_ERROR_OUT_OF_MEMORY);

  *aNodeInfo = newNodeInfo;

  return NS_OK;
}


nsresult
nsNodeInfoManager::GetNodeInfo(const nsAString& aName, nsIAtom *aPrefix,
                               PRInt32 aNamespaceID, nsINodeInfo** aNodeInfo)
{
  nsCOMPtr<nsIAtom> name = do_GetAtom(aName);
  return nsNodeInfoManager::GetNodeInfo(name, aPrefix, aNamespaceID,
                                        aNodeInfo);
}


nsresult
nsNodeInfoManager::GetNodeInfo(const nsAString& aQualifiedName,
                               const nsAString& aNamespaceURI,
                               nsINodeInfo** aNodeInfo)
{
  NS_ENSURE_ARG(!aQualifiedName.IsEmpty());

  nsAString::const_iterator start, end;
  aQualifiedName.BeginReading(start);
  aQualifiedName.EndReading(end);

  nsCOMPtr<nsIAtom> prefixAtom;

  nsAString::const_iterator iter(start);

  if (FindCharInReadable(':', iter, end)) {
    prefixAtom = do_GetAtom(Substring(start, iter));
    NS_ENSURE_TRUE(prefixAtom, NS_ERROR_OUT_OF_MEMORY);

    start = ++iter; // step over the ':'

    if (iter == end) {
      // No data after the ':'.

      return NS_ERROR_INVALID_ARG;
    }
  }

  nsCOMPtr<nsIAtom> nameAtom = do_GetAtom(Substring(start, end));
  NS_ENSURE_TRUE(nameAtom, NS_ERROR_OUT_OF_MEMORY);

  PRInt32 nsid = kNameSpaceID_None;

  if (!aNamespaceURI.IsEmpty()) {
    nsresult rv = nsContentUtils::GetNSManagerWeakRef()->
      RegisterNameSpace(aNamespaceURI, nsid);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return GetNodeInfo(nameAtom, prefixAtom, nsid, aNodeInfo);
}

nsIPrincipal*
nsNodeInfoManager::GetDocumentPrincipal()
{
  NS_ASSERTION(!mDocument || !mPrincipal,
               "how'd we end up with both a document and a principal?");

  if (mDocument) {
    // If the document has a uri we'll ask for it's principal. Otherwise we'll
    // consider this document 'anonymous'
    if (!mDocument->GetDocumentURI()) {
      return nsnull;
    }

    return mDocument->GetPrincipal();
  }

  return mPrincipal;
}

void
nsNodeInfoManager::SetDocumentPrincipal(nsIPrincipal *aPrincipal)
{
  NS_ASSERTION(!mDocument,
               "Don't set a principal, we already have a document.");
  if (!mDocument) {
    mPrincipal = aPrincipal;
  }
}

void
nsNodeInfoManager::RemoveNodeInfo(nsNodeInfo *aNodeInfo)
{
  NS_WARN_IF_FALSE(aNodeInfo, "Trying to remove null nodeinfo from manager!");

  if (aNodeInfo) {
#ifdef DEBUG
    PRBool ret =
#endif
    PL_HashTableRemove(mNodeInfoHash, &aNodeInfo->mInner);

    NS_WARN_IF_FALSE(ret, "Can't find nsINodeInfo to remove!!!");
  }
}
