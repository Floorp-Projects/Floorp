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


nsNodeInfoManager::nsNodeInfoManager()
  : mDocument(nsnull)
{
  NS_INIT_REFCNT();

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

  mNodeInfoHash = PL_NewHashTable(32, nsNodeInfoInner::GetHashValue,
                                  nsNodeInfoInner::KeyCompare,
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


NS_IMPL_THREADSAFE_ISUPPORTS(nsNodeInfoManager,
                             NS_GET_IID(nsINodeInfoManager));


// nsINodeInfoManager

NS_IMETHODIMP
nsNodeInfoManager::Init(nsIDocument *aDocument,
                        nsINameSpaceManager *aNameSpaceManager)
{
  NS_ENSURE_ARG_POINTER(aNameSpaceManager);
  NS_ENSURE_TRUE(mNodeInfoHash, NS_ERROR_OUT_OF_MEMORY);

  mDocument = aDocument;
  mNameSpaceManager = aNameSpaceManager;

  return NS_OK;
}


NS_IMETHODIMP
nsNodeInfoManager::DropDocumentReference()
{
  mDocument = nsnull;

  return NS_OK;
}


NS_IMETHODIMP
nsNodeInfoManager::GetNodeInfo(nsIAtom *aName, nsIAtom *aPrefix,
                               PRInt32 aNamespaceID, nsINodeInfo*& aNodeInfo)
{
  NS_ENSURE_ARG_POINTER(aName);

  nsNodeInfoInner tmpKey(aName, aPrefix, aNamespaceID);

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
nsNodeInfoManager::GetNodeInfo(const nsAReadableString& aName, nsIAtom *aPrefix,
                               PRInt32 aNamespaceID, nsINodeInfo*& aNodeInfo)
{
  NS_ENSURE_ARG(aName.Length());

  nsCOMPtr<nsIAtom> name(dont_AddRef(NS_NewAtom(aName)));
  NS_ENSURE_TRUE(name, NS_ERROR_OUT_OF_MEMORY);

  return GetNodeInfo(name, aPrefix, aNamespaceID, aNodeInfo);
}


NS_IMETHODIMP
nsNodeInfoManager::GetNodeInfo(const nsAReadableString& aName, const nsAReadableString& aPrefix,
                               PRInt32 aNamespaceID, nsINodeInfo*& aNodeInfo)
{
  NS_ENSURE_ARG(aName.Length());

  nsCOMPtr<nsIAtom> name(dont_AddRef(NS_NewAtom(aName)));
  NS_ENSURE_TRUE(name, NS_ERROR_OUT_OF_MEMORY);

  nsCOMPtr<nsIAtom> prefix;

  if (aPrefix.Length()) {
    prefix = dont_AddRef(NS_NewAtom(aPrefix));
    NS_ENSURE_TRUE(prefix, NS_ERROR_OUT_OF_MEMORY);
  }

  return GetNodeInfo(name, prefix, aNamespaceID, aNodeInfo);
}


NS_IMETHODIMP
nsNodeInfoManager::GetNodeInfo(const nsAReadableString& aName, const nsAReadableString& aPrefix,
                               const nsAReadableString& aNamespaceURI,
                               nsINodeInfo*& aNodeInfo)
{
  NS_ENSURE_ARG(aName.Length());

  nsCOMPtr<nsIAtom> name(dont_AddRef(NS_NewAtom(aName)));
  NS_ENSURE_TRUE(name, NS_ERROR_OUT_OF_MEMORY);

  nsCOMPtr<nsIAtom> prefix;

  if (aPrefix.Length()) {
    prefix = dont_AddRef(NS_NewAtom(aPrefix));
    NS_ENSURE_TRUE(prefix, NS_ERROR_OUT_OF_MEMORY);
  }

  if (!mNameSpaceManager) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  PRInt32 nsid;
  nsresult rv = mNameSpaceManager->RegisterNameSpace(aNamespaceURI, nsid);
  NS_ENSURE_SUCCESS(rv, rv);

  return GetNodeInfo(name, prefix, nsid, aNodeInfo);
}


NS_IMETHODIMP
nsNodeInfoManager::GetNodeInfo(const nsAReadableString& aQualifiedName,
                               const nsAReadableString& aNamespaceURI,
                               nsINodeInfo*& aNodeInfo)
{
  NS_ENSURE_ARG(aQualifiedName.Length());

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

  if (prefix.Length()) {
    prefixAtom = dont_AddRef(NS_NewAtom(prefix));
    NS_ENSURE_TRUE(prefixAtom, NS_ERROR_OUT_OF_MEMORY);
  }

  PRInt32 nsid = kNameSpaceID_None;

  if (aNamespaceURI.Length()) {
    NS_ENSURE_TRUE(mNameSpaceManager, NS_ERROR_NOT_INITIALIZED);

    nsresult rv = mNameSpaceManager->RegisterNameSpace(aNamespaceURI, nsid);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return GetNodeInfo(nameAtom, prefixAtom, nsid, aNodeInfo);
}


NS_IMETHODIMP
nsNodeInfoManager::GetNamespaceManager(nsINameSpaceManager*& aNameSpaceManager)
{
  NS_ENSURE_TRUE(mNameSpaceManager, NS_ERROR_NOT_INITIALIZED);

  aNameSpaceManager = mNameSpaceManager;
  NS_ADDREF(aNameSpaceManager);

  return NS_OK;
}


NS_IMETHODIMP
nsNodeInfoManager::GetDocument(nsIDocument*& aDocument)
{
  aDocument = mDocument;

  NS_IF_ADDREF(aDocument);

  return NS_OK;
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

    nsresult rv = NS_NewNameSpaceManager(getter_AddRefs(gAnonymousNodeInfoManager->mNameSpaceManager));

    if (NS_FAILED(rv)) {
      NS_RELEASE(gAnonymousNodeInfoManager);

      return rv;
    }
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

