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
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Original Author: Aaron Leventhal (aaronl@netscape.com)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsAccessNode.h"
#include "nsIStringBundle.h"
#include "nsIServiceManager.h"
#include "nsAccessibilityAtoms.h"
#include "nsHashtable.h"

/* For documentation of the accessibility architecture, 
 * see http://lxr.mozilla.org/seamonkey/source/accessible/accessible-docs.html
 */

static NS_DEFINE_CID(kStringBundleServiceCID,  NS_STRINGBUNDLESERVICE_CID);

nsIStringBundle *nsAccessNode::gStringBundle = 0;
nsIStringBundle *nsAccessNode::gKeyStringBundle = 0;
nsIDOMNode *nsAccessNode::gLastFocusedNode = 0;
nsSupportsHashtable *nsAccessNode::gGlobalAccessibleDocCache = nsnull;

/*
 * Class nsAccessNode
 */

//-----------------------------------------------------
// construction 
//-----------------------------------------------------

NS_INTERFACE_MAP_BEGIN(nsAccessNode)
  NS_INTERFACE_MAP_ENTRY(nsIAccessNode)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

//NS_IMPL_QUERYINTERFACE1(nsAccessNode, nsIAccessNode)
NS_IMPL_ADDREF(nsAccessNode)
NS_IMPL_RELEASE(nsAccessNode)

nsAccessNode::nsAccessNode(nsIDOMNode *aNode): 
  mDOMNode(aNode), mRootAccessibleDoc(nsnull), mSiblingIndex(eSiblingsUninitialized)
{
}

//-----------------------------------------------------
// destruction
//-----------------------------------------------------
nsAccessNode::~nsAccessNode()
{
  if (mDOMNode) {
    Shutdown();  // Otherwise virtual Shutdown() methods could get fired twice
  }
}

NS_IMETHODIMP nsAccessNode::Init(nsIAccessibleDocument *aRootAccessibleDoc)
{
  // XXX aaronl: Temporary approach,
  // until we have hash table ready
  mRootAccessibleDoc = aRootAccessibleDoc;

  // Later we can get doc from mDOMNode, and get accessible from that

  return NS_OK;
}

NS_IMETHODIMP nsAccessNode::Shutdown()
{
  mDOMNode = nsnull;

  return NS_OK;
}

NS_IMETHODIMP nsAccessNode::GetUniqueID(void **aUniqueID)
{
  *aUniqueID = NS_STATIC_CAST(void*, mDOMNode.get());
  return NS_OK;
}

NS_IMETHODIMP nsAccessNode::GetOwnerWindow(void **aWindow)
{
  NS_ASSERTION(mRootAccessibleDoc, "No root accessible pointer back, Init() not called.");
  return mRootAccessibleDoc->GetWindow(aWindow);
}

void nsAccessNode::InitAccessibility()
{
  if (gIsAccessibilityActive) {
    return;
  }
  nsCOMPtr<nsIStringBundleService> stringBundleService(do_GetService(kStringBundleServiceCID));
  if (stringBundleService) {
    // Static variables are released in nsRootAccessible::ShutdownAll();
    stringBundleService->CreateBundle(ACCESSIBLE_BUNDLE_URL, 
                                      &gStringBundle);
    NS_IF_ADDREF(gStringBundle);
    stringBundleService->CreateBundle(PLATFORM_KEYS_BUNDLE_URL, 
                                      &gKeyStringBundle);
    NS_IF_ADDREF(gKeyStringBundle);
  }
  nsAccessibilityAtoms::AddRefAtoms();

  gGlobalAccessibleDocCache = new nsSupportsHashtable;

  gIsAccessibilityActive = PR_TRUE;
}

void nsAccessNode::ShutdownAccessibility()
{
  // Called by nsAccessibilityService::Shutdown()
  // which happens when xpcom is shutting down
  // at exit of program

  if (!gIsAccessibilityActive) {
    return;
  }
  NS_IF_RELEASE(gLastFocusedNode);
  NS_IF_RELEASE(gStringBundle);
  NS_IF_RELEASE(gKeyStringBundle);

  // And just to be safe
  gLastFocusedNode = nsnull;
  gStringBundle = gKeyStringBundle = nsnull;

  nsAccessibilityAtoms::ReleaseAtoms();

  ClearCache(gGlobalAccessibleDocCache);
  delete gGlobalAccessibleDocCache;
  gGlobalAccessibleDocCache = nsnull;

  gIsAccessibilityActive = PR_FALSE;
}

/***************** Hashtable of nsIAccessNode's *****************/

// For my hash table, I'm using a void* key, and nsISupports data. 
// However, I want a custom Shutdown() method called whenever an entry is removed.
// Should I use nsObjectHashtable instead of nsSupportsHashtable so that I can give 
// it my custom Shutdown/destroyElement method?
// Or should I use an nsSupportsHashtable and enumerate through the 
// hash myself for Shutdown() of elements?   

void nsAccessNode::PutCacheEntry(nsSupportsHashtable *aCache, void* aUniqueID, 
                                 nsIAccessNode *aAccessNode)
{
  nsVoidKey key(aUniqueID);

  NS_ASSERTION(!aCache->Exists(&key), "This cache entry shouldn't exist already");

  aCache->Put(&key, aAccessNode);
}

void nsAccessNode::GetCacheEntry(nsSupportsHashtable *aCache, void* aUniqueID, 
                                 nsIAccessNode **aAccessNode)
{
  nsVoidKey key(aUniqueID);

  *aAccessNode = NS_STATIC_CAST(nsIAccessNode*, aCache->Get(&key));  // AddRefs for us
}

PRIntn PR_CALLBACK nsAccessNode::ClearCacheEntry(nsHashKey *aKey, void *aData, 
                                                 void* aClosure)
{
  NS_STATIC_CAST(nsIAccessNode*, aData)->Shutdown();

  return kHashEnumerateNext; // Or kHashEnumerateRemove?
}

void nsAccessNode::ClearCache(nsSupportsHashtable *aCache)
{
  aCache->Enumerate(ClearCacheEntry);  
}

void nsAccessNode::GetDocumentAccessible(nsIDOMNode *aDocNode, 
                                         nsIAccessibleDocument **aDocAccessible)
{
  NS_ASSERTION(gGlobalAccessibleDocCache, "Global doc cache does not exist");

  nsCOMPtr<nsIAccessNode> docAccessNode;

  GetCacheEntry(gGlobalAccessibleDocCache, NS_STATIC_CAST(void*, aDocNode), 
    getter_AddRefs(docAccessNode));

  nsCOMPtr<nsIAccessible> docAccessible(do_QueryInterface(docAccessNode));

  NS_IF_ADDREF(*aDocAccessible = docAccessible);
}

