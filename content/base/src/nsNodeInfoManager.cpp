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

#include "nsNodeInfoManager.h"
#include "nsNodeInfo.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsIAtom.h"
#include "nsIDocument.h"
#include "nsIPrincipal.h"
#include "nsISupportsArray.h"
#include "nsContentUtils.h"

nsNodeInfoManager* nsNodeInfoManager::gAnonymousNodeInfoManager = nsnull;
PRUint32 nsNodeInfoManager::gNodeManagerCount = 0;


nsresult NS_NewNodeInfoManager(nsINodeInfoManager** aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);

  *aResult = new nsNodeInfoManager;
  NS_ENSURE_TRUE(*aResult, NS_ERROR_OUT_OF_MEMORY);

  NS_ADDREF(*aResult);

  return NS_OK;
}


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


nsNodeInfoManager::nsNodeInfoManager()
  : mDocument(nsnull)
{
  NS_INIT_ISUPPORTS();

  if (gNodeManagerCount == 1 && gAnonymousNodeInfoManager) {
    /*
     * If we get here the global nodeinfo manager was the first one created,
     * in that case we're not holding a strong reference to the global nodeinfo
     * manager. Now we're creating one more nodeinfo manager so we'll grab
     * a strong reference to the global nodeinfo manager so that it's
     * lifetime will be longer than the lifetime of the other node managers.
     */
    NS_ADDREF(gAnonymousNodeInfoManager);
  }

  gNodeManagerCount++;

  mNodeInfoHash = PL_NewHashTable(32, GetNodeInfoInnerHashValue,
                                  NodeInfoInnerKeyCompare,
                                  PL_CompareValues, nsnull, nsnull);

#ifdef DEBUG_jst
  printf ("Creating NodeInfoManager, gcount = %d\n", gNodeManagerCount);
#endif
}


nsNodeInfoManager::~nsNodeInfoManager()
{
  gNodeManagerCount--;

  if (gNodeManagerCount == 1 && gAnonymousNodeInfoManager) {
    NS_RELEASE(gAnonymousNodeInfoManager);
  } else if (!gNodeManagerCount) {
    /*
     * Here we just make sure that we don't leave a dangling pointer to
     * the global nodeinfo manager after it's deleted.
     */
    gAnonymousNodeInfoManager = nsnull;
  }

  if (mNodeInfoHash)
    PL_HashTableDestroy(mNodeInfoHash);

#ifdef DEBUG_jst
  printf ("Removing NodeInfoManager, gcount = %d\n", gNodeManagerCount);
#endif
}


NS_IMPL_THREADSAFE_ISUPPORTS1(nsNodeInfoManager, nsINodeInfoManager);


// nsINodeInfoManager

NS_IMETHODIMP
nsNodeInfoManager::Init(nsIDocument *aDocument)
{
  NS_ENSURE_TRUE(mNodeInfoHash, NS_ERROR_OUT_OF_MEMORY);

  mDocument = aDocument;
  if (aDocument) {
    mPrincipal = nsnull;
  }

  return NS_OK;
}


NS_IMETHODIMP
nsNodeInfoManager::DropDocumentReference()
{
  if (mDocument) {
    // If the document has a uri we'll ask for it's principal. Otherwise we'll
    // consider this document 'anonymous'. We don't want to call GetPrincipal
    // on a document that doesn't have a URI since that'll give an assertion
    // that we're creating a principal without having a uri.
    // This happens in a few cases where a document is created and then
    // immediately dropped without ever getting a URI.
    nsCOMPtr<nsIURI> docUri;
    mDocument->GetDocumentURL(getter_AddRefs(docUri));
    if (docUri) {
      mDocument->GetPrincipal(getter_AddRefs(mPrincipal));
    }
  }
  mDocument = nsnull;

  return NS_OK;
}


NS_IMETHODIMP
nsNodeInfoManager::GetNodeInfo(nsIAtom *aName, nsIAtom *aPrefix,
                               PRInt32 aNamespaceID, nsINodeInfo*& aNodeInfo)
{
  NS_ENSURE_ARG_POINTER(aName);

  nsINodeInfo::nsNodeInfoInner tmpKey(aName, aPrefix, aNamespaceID);

  void *node = PL_HashTableLookup(mNodeInfoHash, &tmpKey);

  if (node) {
    aNodeInfo = NS_STATIC_CAST(nsINodeInfo *, node);

    NS_ADDREF(aNodeInfo);

    return NS_OK;
  }

  nsNodeInfo *newNodeInfo = new nsNodeInfo();
  NS_ENSURE_TRUE(newNodeInfo, NS_ERROR_OUT_OF_MEMORY);

  NS_ADDREF(newNodeInfo);

  nsresult rv = newNodeInfo->Init(aName, aPrefix, aNamespaceID, this);
  NS_ENSURE_SUCCESS(rv, rv);

  PLHashEntry *he;
  he = PL_HashTableAdd(mNodeInfoHash, &newNodeInfo->mInner, newNodeInfo);
  NS_ENSURE_TRUE(he, NS_ERROR_OUT_OF_MEMORY);

  aNodeInfo = newNodeInfo;

  return NS_OK;
}


NS_IMETHODIMP
nsNodeInfoManager::GetNodeInfo(const nsAString& aName, nsIAtom *aPrefix,
                               PRInt32 aNamespaceID, nsINodeInfo*& aNodeInfo)
{
  NS_ENSURE_ARG(!aName.IsEmpty());

  nsCOMPtr<nsIAtom> name(dont_AddRef(NS_NewAtom(aName)));
  NS_ENSURE_TRUE(name, NS_ERROR_OUT_OF_MEMORY);

  return GetNodeInfo(name, aPrefix, aNamespaceID, aNodeInfo);
}


NS_IMETHODIMP
nsNodeInfoManager::GetNodeInfo(const nsAString& aName,
                               const nsAString& aPrefix, PRInt32 aNamespaceID,
                               nsINodeInfo*& aNodeInfo)
{
  NS_ENSURE_ARG(!aName.IsEmpty());

  nsCOMPtr<nsIAtom> name(dont_AddRef(NS_NewAtom(aName)));
  NS_ENSURE_TRUE(name, NS_ERROR_OUT_OF_MEMORY);

  nsCOMPtr<nsIAtom> prefix;

  if (!aPrefix.IsEmpty()) {
    prefix = dont_AddRef(NS_NewAtom(aPrefix));
    NS_ENSURE_TRUE(prefix, NS_ERROR_OUT_OF_MEMORY);
  }

  return GetNodeInfo(name, prefix, aNamespaceID, aNodeInfo);
}


NS_IMETHODIMP
nsNodeInfoManager::GetNodeInfo(const nsAString& aName,
                               const nsAString& aPrefix,
                               const nsAString& aNamespaceURI,
                               nsINodeInfo*& aNodeInfo)
{
  NS_ENSURE_ARG(!aName.IsEmpty());

  nsCOMPtr<nsIAtom> name(dont_AddRef(NS_NewAtom(aName)));
  NS_ENSURE_TRUE(name, NS_ERROR_OUT_OF_MEMORY);

  nsCOMPtr<nsIAtom> prefix;

  if (!aPrefix.IsEmpty()) {
    prefix = dont_AddRef(NS_NewAtom(aPrefix));
    NS_ENSURE_TRUE(prefix, NS_ERROR_OUT_OF_MEMORY);
  }

  PRInt32 nsid;
  nsresult rv = nsContentUtils::GetNSManagerWeakRef()->RegisterNameSpace(aNamespaceURI, nsid);
  NS_ENSURE_SUCCESS(rv, rv);

  return GetNodeInfo(name, prefix, nsid, aNodeInfo);
}


NS_IMETHODIMP
nsNodeInfoManager::GetNodeInfo(const nsAString& aQualifiedName,
                               const nsAString& aNamespaceURI,
                               nsINodeInfo*& aNodeInfo)
{
  NS_ENSURE_ARG(!aQualifiedName.IsEmpty());

  nsAutoString name(aQualifiedName);
  nsAutoString prefix;
  PRInt32 nsoffset = name.FindChar(':');
  if (-1 != nsoffset) {
    name.Left(prefix, nsoffset);
    name.Cut(0, nsoffset+1);
  }

  nsCOMPtr<nsIAtom> nameAtom(dont_AddRef(NS_NewAtom(name)));
  NS_ENSURE_TRUE(nameAtom, NS_ERROR_OUT_OF_MEMORY);

  nsCOMPtr<nsIAtom> prefixAtom;

  if (!prefix.IsEmpty()) {
    prefixAtom = dont_AddRef(NS_NewAtom(prefix));
    NS_ENSURE_TRUE(prefixAtom, NS_ERROR_OUT_OF_MEMORY);
  }

  PRInt32 nsid = kNameSpaceID_None;

  if (!aNamespaceURI.IsEmpty()) {
    nsresult rv = nsContentUtils::GetNSManagerWeakRef()->RegisterNameSpace(aNamespaceURI, nsid);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return GetNodeInfo(nameAtom, prefixAtom, nsid, aNodeInfo);
}

NS_IMETHODIMP
nsNodeInfoManager::GetDocument(nsIDocument*& aDocument)
{
  aDocument = mDocument;

  NS_IF_ADDREF(aDocument);

  return NS_OK;
}

NS_IMETHODIMP
nsNodeInfoManager::GetDocumentPrincipal(nsIPrincipal** aPrincipal)
{
  NS_ENSURE_ARG_POINTER(aPrincipal);
  NS_ASSERTION(!mDocument || !mPrincipal,
               "how'd we end up with both a document and a principal?");

  if (mDocument) {
    // If the document has a uri we'll ask for it's principal. Otherwise we'll
    // consider this document 'anonymous'
    nsCOMPtr<nsIURI> docUri;
    mDocument->GetDocumentURL(getter_AddRefs(docUri));
    if (!docUri) {
      *aPrincipal = nsnull;
      return NS_OK;
    }
    return mDocument->GetPrincipal(aPrincipal);
  }
  *aPrincipal = mPrincipal;
  NS_IF_ADDREF(*aPrincipal);
  return NS_OK;
}

NS_IMETHODIMP
nsNodeInfoManager::SetDocumentPrincipal(nsIPrincipal* aPrincipal)
{
  NS_ENSURE_FALSE(mDocument, NS_ERROR_UNEXPECTED);
  mPrincipal = aPrincipal;
  return NS_OK;
}

NS_IMETHODIMP
nsNodeInfoManager::GetNodeInfoArray(nsISupportsArray** aArray)
{
  *aArray = nsnull;

  nsCOMPtr<nsISupportsArray> array;
  nsresult rv = NS_NewISupportsArray(getter_AddRefs(array));
  NS_ENSURE_SUCCESS(rv, rv);

  PL_HashTableEnumerateEntries(mNodeInfoHash,
                               GetNodeInfoArrayEnumerator,
                               array);
  PRUint32 n;
  array->Count(&n);
  NS_ENSURE_TRUE(n == mNodeInfoHash->nentries, NS_ERROR_OUT_OF_MEMORY);

  *aArray = array;
  NS_ADDREF(*aArray);

  return NS_OK;
}

//static
PRIntn
nsNodeInfoManager::GetNodeInfoArrayEnumerator(PLHashEntry* he, PRIntn i,
                                              void* arg)
{
  NS_ASSERTION(arg, "missing array");
  nsISupportsArray* array = (nsISupportsArray*)arg;

  nsresult rv = array->AppendElement((nsINodeInfo*)he->value);
  if (NS_FAILED(rv)) {
    return HT_ENUMERATE_STOP;
  }

  return HT_ENUMERATE_NEXT;
}

void
nsNodeInfoManager::RemoveNodeInfo(nsNodeInfo *aNodeInfo)
{
  NS_WARN_IF_FALSE(aNodeInfo, "Trying to remove null nodeinfo from manager!");

  if (aNodeInfo) {
    PRBool ret = PL_HashTableRemove(mNodeInfoHash, &aNodeInfo->mInner);

    NS_WARN_IF_FALSE(ret, "Can't find nsINodeInfo to remove!!!");
  }
}


nsresult
nsNodeInfoManager::GetAnonymousManager(nsINodeInfoManager*& aNodeInfoManager)
{
  if (!gAnonymousNodeInfoManager) {
    gAnonymousNodeInfoManager = new nsNodeInfoManager;

    if (!gAnonymousNodeInfoManager)
      return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(gAnonymousNodeInfoManager);
  }

  aNodeInfoManager = gAnonymousNodeInfoManager;

  /*
   * If the only nodeinfo manager is the global one we don't hold a ref
   * since the global nodeinfo manager should be destroyed when it's released,
   * even if it's the last one arround.
   */
  if (gNodeManagerCount > 1) {
    NS_ADDREF(aNodeInfoManager);
  }

  return NS_OK;
}

