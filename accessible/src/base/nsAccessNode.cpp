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
#include "nsAccessibilityAtoms.h"
#include "nsHashtable.h"
#include "nsIAccessibilityService.h"
#include "nsIDocument.h"
#include "nsIFrame.h"
#include "nsIPref.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsIServiceManager.h"
#include "nsIStringBundle.h"

/* For documentation of the accessibility architecture, 
 * see http://lxr.mozilla.org/seamonkey/source/accessible/accessible-docs.html
 */

static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);
static NS_DEFINE_CID(kStringBundleServiceCID,  NS_STRINGBUNDLESERVICE_CID);

nsIStringBundle *nsAccessNode::gStringBundle = 0;
nsIStringBundle *nsAccessNode::gKeyStringBundle = 0;
nsIDOMNode *nsAccessNode::gLastFocusedNode = 0;
PRBool nsAccessNode::gIsAccessibilityActive = PR_FALSE;
PRBool nsAccessNode::gIsCacheDisabled = PR_FALSE;

nsInterfaceHashtable<nsVoidHashKey, nsIAccessNode> *nsAccessNode::gGlobalDocAccessibleCache = nsnull;

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

NS_IMPL_ADDREF(nsAccessNode)
NS_IMPL_RELEASE(nsAccessNode)

nsAccessNode::nsAccessNode(nsIDOMNode *aNode, nsIWeakReference* aShell): 
  mDOMNode(aNode), mWeakShell(aShell), mRefCnt(0),
  mAccChildCount(eChildCountUninitialized)
{
#ifdef DEBUG
  mIsInitialized = PR_FALSE;
#endif
}

//-----------------------------------------------------
// destruction
//-----------------------------------------------------
nsAccessNode::~nsAccessNode()
{
  if (mWeakShell) {
    Shutdown();  // Otherwise virtual Shutdown() methods could get fired twice
  }
}

NS_IMETHODIMP nsAccessNode::Init()
{
  // We have to put this here, instead of constructor, otherwise
  // we don't have the virtual GetUniqueID() method for the hash key.
  // We need that for accessibles that don't have DOM nodes

  NS_ASSERTION(!mIsInitialized, "Initialized twice!");
  nsCOMPtr<nsIAccessibleDocument> docAccessible(GetDocAccessible());
  if (!docAccessible) {
    // No doc accessible yet for this node's document. 
    // There was probably an accessible event fired before the 
    // current document was ever asked for by the assistive technology.
    // Create a doc accessible so we can cache this node
    nsCOMPtr<nsIPresShell> presShell(do_QueryReferent(mWeakShell));
    if (presShell) {
      nsCOMPtr<nsIDocument> doc;
      presShell->GetDocument(getter_AddRefs(doc));
      nsCOMPtr<nsIDOMNode> docNode(do_QueryInterface(doc));
      if (docNode) {
        nsCOMPtr<nsIAccessibilityService> accService = 
          do_GetService("@mozilla.org/accessibilityService;1");
        NS_ASSERTION(accService, "No accessibility service");
        if (accService) {
          nsCOMPtr<nsIAccessible> accessible;
          accService->GetAccessibleInShell(docNode, presShell, 
            getter_AddRefs(accessible));
          docAccessible = do_QueryInterface(accessible);
        }
      }
    }
    NS_ASSERTION(docAccessible, "Cannot cache new nsAccessNode");
    if (!docAccessible) {
      return NS_ERROR_FAILURE;
    }
  }
  void* uniqueID;
  GetUniqueID(&uniqueID);
  docAccessible->CacheAccessNode(uniqueID, this);
#ifdef DEBUG
  mIsInitialized = PR_TRUE;
#endif

  return NS_OK;
}


NS_IMETHODIMP nsAccessNode::Shutdown()
{
  mDOMNode = nsnull;
  mWeakShell = nsnull;

  return NS_OK;
}

NS_IMETHODIMP nsAccessNode::GetUniqueID(void **aUniqueID)
{
  *aUniqueID = NS_STATIC_CAST(void*, mDOMNode);
  return NS_OK;
}

NS_IMETHODIMP nsAccessNode::GetOwnerWindow(void **aWindow)
{
  nsCOMPtr<nsIAccessibleDocument> docAccessible(GetDocAccessible());
  NS_ASSERTION(docAccessible, "No root accessible pointer back, Init() not called.");
  return docAccessible->GetWindow(aWindow);
}

void nsAccessNode::InitXPAccessibility()
{
  if (gIsAccessibilityActive) {
    return;
  }
  nsCOMPtr<nsIStringBundleService> stringBundleService(do_GetService(kStringBundleServiceCID));
  if (stringBundleService) {
    // Static variables are released in ShutdownAllXPAccessibility();
    stringBundleService->CreateBundle(ACCESSIBLE_BUNDLE_URL, 
                                      &gStringBundle);
    NS_IF_ADDREF(gStringBundle);
    stringBundleService->CreateBundle(PLATFORM_KEYS_BUNDLE_URL, 
                                      &gKeyStringBundle);
    NS_IF_ADDREF(gKeyStringBundle);
  }
  nsAccessibilityAtoms::AddRefAtoms();

  gGlobalDocAccessibleCache = new nsInterfaceHashtable<nsVoidHashKey, nsIAccessNode>;
  gGlobalDocAccessibleCache->Init(4); // Initialize for 4 entries

  nsCOMPtr<nsIPref> prefService(do_GetService(kPrefCID));
  if (prefService) {
    prefService->GetBoolPref("accessibility.disablecache", &gIsCacheDisabled);
  }

  gIsAccessibilityActive = PR_TRUE;
}

void nsAccessNode::ShutdownXPAccessibility()
{
  // Called by nsAccessibilityService::Shutdown()
  // which happens when xpcom is shutting down
  // at exit of program

  if (!gIsAccessibilityActive) {
    return;
  }
  NS_IF_RELEASE(gStringBundle);
  NS_IF_RELEASE(gKeyStringBundle);
  NS_IF_RELEASE(gLastFocusedNode);

  ClearCache(*gGlobalDocAccessibleCache);
  delete gGlobalDocAccessibleCache;
  gGlobalDocAccessibleCache = nsnull;

  gIsAccessibilityActive = PR_FALSE;
}

already_AddRefed<nsIPresShell> nsAccessNode::GetPresShell()
{
  nsCOMPtr<nsIPresShell> presShell(do_QueryReferent(mWeakShell));
  if (!presShell) {
    if (mWeakShell) {
      // If our pres shell has died, but we're still holding onto
      // a weak reference, our accessibles are no longer relevant
      // and should be shut down
      Shutdown();
    }
    return nsnull;
  }
  nsIPresShell *resultShell = presShell;
  NS_IF_ADDREF(resultShell);
  return resultShell;
}

already_AddRefed<nsIPresContext> nsAccessNode::GetPresContext()
{
  nsCOMPtr<nsIPresShell> presShell(GetPresShell());
  if (!presShell) {
    return nsnull;
  }
  nsIPresContext *presContext;
  presShell->GetPresContext(&presContext);  // Addref'd
  return presContext;
}

already_AddRefed<nsIAccessibleDocument> nsAccessNode::GetDocAccessible()
{
  nsIAccessibleDocument *docAccessible;
  GetDocAccessibleFor(mWeakShell, &docAccessible); // Addref'd
  return docAccessible;
}

nsIFrame* nsAccessNode::GetFrame()
{
  nsCOMPtr<nsIPresShell> shell(do_QueryReferent(mWeakShell));
  if (!shell) 
    return nsnull;  

  nsIFrame* frame = nsnull;
  nsCOMPtr<nsIContent> content(do_QueryInterface(mDOMNode));
  shell->GetPrimaryFrameFor(content, &frame);
  return frame;
}

/***************** Hashtable of nsIAccessNode's *****************/

void nsAccessNode::GetDocAccessibleFor(nsIWeakReference *aPresShell, 
                                       nsIAccessibleDocument **aDocAccessible)
{
  *aDocAccessible = nsnull;
  NS_ASSERTION(gGlobalDocAccessibleCache, "Global doc cache does not exist");

  nsCOMPtr<nsIAccessNode> accessNode;
  gGlobalDocAccessibleCache->Get(NS_STATIC_CAST(void*, aPresShell), getter_AddRefs(accessNode));
  if (accessNode) {
    accessNode->QueryInterface(NS_GET_IID(nsIAccessibleDocument), (void**)aDocAccessible); // addrefs
  }
}
 
void nsAccessNode::PutCacheEntry(nsInterfaceHashtable<nsVoidHashKey, nsIAccessNode> &aCache, 
                                 void* aUniqueID, 
                                 nsIAccessNode *aAccessNode)
{
#ifdef DEBUG
  nsCOMPtr<nsIAccessNode> oldAccessNode;
  GetCacheEntry(aCache, aUniqueID, getter_AddRefs(oldAccessNode));
  NS_ASSERTION(!oldAccessNode, "This cache entry shouldn't exist already");
#endif
  aCache.Put(aUniqueID, aAccessNode);
}

void nsAccessNode::GetCacheEntry(nsInterfaceHashtable<nsVoidHashKey, nsIAccessNode> &aCache, 
                                 void* aUniqueID, 
                                 nsIAccessNode **aAccessNode)
{
  aCache.Get(aUniqueID, aAccessNode);  // AddRefs for us
}

PLDHashOperator nsAccessNode::ClearCacheEntry(const void* aKey, nsCOMPtr<nsIAccessNode>& aAccessNode, void* aUserArg)
{
  aAccessNode->Shutdown();

  return PL_DHASH_REMOVE;
}

void nsAccessNode::ClearCache(nsInterfaceHashtable<nsVoidHashKey, nsIAccessNode> &aCache)
{
  aCache.Enumerate(ClearCacheEntry, nsnull);  
}


