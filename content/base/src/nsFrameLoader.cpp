/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Johnny Stenback <jst@netscape.com> (original author)
 *   Boris Zbarsky <bzbarsky@mit.edu>
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
 * Class for managing loading of a subframe (creation of the docshell,
 * handling of loads in it, recursion-checking).
 */

#include "nsIDOMHTMLIFrameElement.h"
#include "nsIDOMHTMLFrameElement.h"
#include "nsIDOMWindow.h"
#include "nsPresContext.h"
#include "nsIPresShell.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOMWindow.h"
#include "nsPIDOMWindow.h"
#include "nsIWebNavigation.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocShellTreeNode.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIDocShellLoadInfo.h"
#include "nsIBaseWindow.h"
#include "nsContentUtils.h"
#include "nsUnicharUtils.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptSecurityManager.h"
#include "nsFrameLoader.h"
#include "nsIDOMEventTarget.h"
#include "nsIFrame.h"
#include "nsIFrameFrame.h"
#include "nsDOMError.h"
#include "nsPresShellIterator.h"
#include "nsGUIEvent.h"
#include "nsEventDispatcher.h"
#include "nsISHistory.h"
#include "nsISHistoryInternal.h"

#include "nsIURI.h"
#include "nsIURL.h"
#include "nsNetUtil.h"

#include "nsGkAtoms.h"
#include "nsINameSpaceManager.h"

#include "nsThreadUtils.h"

class nsAsyncDocShellDestroyer : public nsRunnable
{
public:
  nsAsyncDocShellDestroyer(nsIDocShell* aDocShell)
    : mDocShell(aDocShell)
  {
  }

  NS_IMETHOD Run()
  {
    nsCOMPtr<nsIBaseWindow> base_win(do_QueryInterface(mDocShell));
    if (base_win) {
      base_win->Destroy();
    }
    return NS_OK;
  }
  nsRefPtr<nsIDocShell> mDocShell;
};

// Bug 136580: Limit to the number of nested content frames that can have the
//             same URL. This is to stop content that is recursively loading
//             itself.  Note that "#foo" on the end of URL doesn't affect
//             whether it's considered identical, but "?foo" or ";foo" are
//             considered and compared.
// Bug 228829: Limit this to 1, like IE does.
#define MAX_SAME_URL_CONTENT_FRAMES 1

// Bug 8065: Limit content frame depth to some reasonable level. This
// does not count chrome frames when determining depth, nor does it
// prevent chrome recursion.  Number is fairly arbitrary, but meant to
// keep number of shells to a reasonable number on accidental recursion with a
// small (but not 1) branching factor.  With large branching factors the number
// of shells can rapidly become huge and run us out of memory.  To solve that,
// we'd need to re-institute a fixed version of bug 98158.
#define MAX_DEPTH_CONTENT_FRAMES 10

NS_IMPL_CYCLE_COLLECTION_1(nsFrameLoader, mDocShell)

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsFrameLoader)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsFrameLoader)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsFrameLoader)
  NS_INTERFACE_MAP_ENTRY(nsIFrameLoader)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

nsFrameLoader*
nsFrameLoader::Create(nsIContent* aOwner)
{
  NS_ENSURE_TRUE(aOwner, nsnull);
  nsIDocument* doc = aOwner->GetCurrentDoc();
  NS_ENSURE_TRUE(doc && !doc->GetDisplayDocument() &&
                 !doc->IsLoadedAsData(), nsnull);

  return new nsFrameLoader(aOwner);
}

NS_IMETHODIMP
nsFrameLoader::LoadFrame()
{
  NS_ENSURE_TRUE(mOwnerContent, NS_ERROR_NOT_INITIALIZED);

  nsAutoString src;
  GetURL(src);

  src.Trim(" \t\n\r");

  if (src.IsEmpty()) {
    src.AssignLiteral("about:blank");
  }

  nsIDocument* doc = mOwnerContent->GetOwnerDoc();
  if (!doc) {
    return NS_OK;
  }

  nsCOMPtr<nsIURI> base_uri = mOwnerContent->GetBaseURI();
  const nsAFlatCString &doc_charset = doc->GetDocumentCharacterSet();
  const char *charset = doc_charset.IsEmpty() ? nsnull : doc_charset.get();

  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), src, charset, base_uri);

  // If the URI was malformed, try to recover by loading about:blank.
  if (rv == NS_ERROR_MALFORMED_URI) {
    rv = NS_NewURI(getter_AddRefs(uri), NS_LITERAL_STRING("about:blank"),
                   charset, base_uri);
  }

  NS_ENSURE_SUCCESS(rv, rv);
  return LoadURI(uri);
}

NS_IMETHODIMP
nsFrameLoader::LoadURI(nsIURI* aURI)
{
  if (!aURI)
    return NS_ERROR_INVALID_POINTER;
  NS_ENSURE_STATE(!mDestroyCalled && mOwnerContent);

  nsCOMPtr<nsIDocument> doc = mOwnerContent->GetOwnerDoc();
  if (!doc) {
    return NS_OK;
  }

  nsresult rv = CheckURILoad(aURI);
  NS_ENSURE_SUCCESS(rv, rv);

  mURIToLoad = aURI;
  rv = doc->InitializeFrameLoader(this);
  if (NS_FAILED(rv)) {
    mURIToLoad = nsnull;
  }
  return rv;
}

nsresult
nsFrameLoader::ReallyStartLoading()
{
  NS_ENSURE_STATE(mURIToLoad && mOwnerContent && mOwnerContent->IsInDoc());
  // Just to be safe, recheck uri.
  nsresult rv = CheckURILoad(mURIToLoad);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = EnsureDocShell();
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDocShellLoadInfo> loadInfo;
  mDocShell->CreateLoadInfo(getter_AddRefs(loadInfo));
  NS_ENSURE_TRUE(loadInfo, NS_ERROR_FAILURE);

  // We'll use our principal, not that of the document loaded inside us.  This
  // is very important; needed to prevent XSS attacks on documents loaded in
  // subframes!
  loadInfo->SetOwner(mOwnerContent->NodePrincipal());

  nsCOMPtr<nsIURI> referrer;
  rv = mOwnerContent->NodePrincipal()->GetURI(getter_AddRefs(referrer));
  NS_ENSURE_SUCCESS(rv, rv);

  loadInfo->SetReferrer(referrer);

  // Kick off the load...
  PRBool tmpState = mNeedsAsyncDestroy;
  mNeedsAsyncDestroy = PR_TRUE;
  rv = mDocShell->LoadURI(mURIToLoad, loadInfo,
                          nsIWebNavigation::LOAD_FLAGS_NONE, PR_FALSE);
  mNeedsAsyncDestroy = tmpState;
  mURIToLoad = nsnull;
#ifdef DEBUG
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to load the URL");
  }
#endif
  return NS_OK;
}

nsresult
nsFrameLoader::CheckURILoad(nsIURI* aURI)
{
  // Check for security.  The fun part is trying to figure out what principals
  // to use.  The way I figure it, if we're doing a LoadFrame() accidentally
  // (eg someone created a frame/iframe node, we're being parsed, XUL iframes
  // are being reframed, etc.) then we definitely want to use the node
  // principal of mOwnerContent for security checks.  If, on the other hand,
  // someone's setting the src on our owner content, or created it via script,
  // or whatever, then they can clearly access it... and we should still use
  // the principal of mOwnerContent.  I don't think that leads to privilege
  // escalation, and it's reasonably guaranteed to not lead to XSS issues
  // (since caller can already access mOwnerContent in this case).  So just use
  // the principal of mOwnerContent no matter what.  If script wants to run
  // things with its own permissions, which differ from those of mOwnerContent
  // (which means the script is privileged in some way) it should set
  // window.location instead.
  nsIScriptSecurityManager *secMan = nsContentUtils::GetSecurityManager();

  // Get our principal
  nsIPrincipal* principal = mOwnerContent->NodePrincipal();

  // Check if we are allowed to load absURL
  nsresult rv =
    secMan->CheckLoadURIWithPrincipal(principal, aURI,
                                      nsIScriptSecurityManager::STANDARD);
  if (NS_FAILED(rv)) {
    return rv; // We're not
  }

  // Bail out if this is an infinite recursion scenario
  return CheckForRecursiveLoad(aURI);
}

NS_IMETHODIMP
nsFrameLoader::GetDocShell(nsIDocShell **aDocShell)
{
  *aDocShell = nsnull;

  // If we have an owner, make sure we have a docshell and return
  // that. If not, we're most likely in the middle of being torn down,
  // then we just return null.
  if (mOwnerContent) {
    nsresult rv = EnsureDocShell();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  *aDocShell = mDocShell;
  NS_IF_ADDREF(*aDocShell);

  return NS_OK;
}

void
nsFrameLoader::Finalize()
{
  nsCOMPtr<nsIBaseWindow> base_win(do_QueryInterface(mDocShell));
  if (base_win) {
    base_win->Destroy();
  }
  mDocShell = nsnull;
}

static void
FirePageHideEvent(nsIDocShellTreeItem* aItem,
                  nsIDOMEventTarget* aChromeEventHandler)
{
  nsCOMPtr<nsIDOMDocument> doc = do_GetInterface(aItem);
  nsCOMPtr<nsIDocument> internalDoc = do_QueryInterface(doc);
  NS_ASSERTION(internalDoc, "What happened here?");
  internalDoc->OnPageHide(PR_TRUE, aChromeEventHandler);

  PRInt32 childCount = 0;
  aItem->GetChildCount(&childCount);
  nsAutoTArray<nsCOMPtr<nsIDocShellTreeItem>, 8> kids;
  kids.AppendElements(childCount);
  for (PRInt32 i = 0; i < childCount; ++i) {
    aItem->GetChildAt(i, getter_AddRefs(kids[i]));
  }

  for (PRUint32 i = 0; i < kids.Length(); ++i) {
    if (kids[i]) {
      FirePageHideEvent(kids[i], aChromeEventHandler);
    }
  }
}

// The pageshow event is fired for a given document only if IsShowing() returns
// the same thing as aFireIfShowing.  This gives us a way to fire pageshow only
// on documents that are still loading or only on documents that are already
// loaded.
static void
FirePageShowEvent(nsIDocShellTreeItem* aItem,
                  nsIDOMEventTarget* aChromeEventHandler,
                  PRBool aFireIfShowing)
{
  PRInt32 childCount = 0;
  aItem->GetChildCount(&childCount);
  nsAutoTArray<nsCOMPtr<nsIDocShellTreeItem>, 8> kids;
  kids.AppendElements(childCount);
  for (PRInt32 i = 0; i < childCount; ++i) {
    aItem->GetChildAt(i, getter_AddRefs(kids[i]));
  }

  for (PRUint32 i = 0; i < kids.Length(); ++i) {
    if (kids[i]) {
      FirePageShowEvent(kids[i], aChromeEventHandler, aFireIfShowing);
    }
  }

  nsCOMPtr<nsIDOMDocument> doc = do_GetInterface(aItem);
  nsCOMPtr<nsIDocument> internalDoc = do_QueryInterface(doc);
  NS_ASSERTION(internalDoc, "What happened here?");
  if (internalDoc->IsShowing() == aFireIfShowing) {
    internalDoc->OnPageShow(PR_TRUE, aChromeEventHandler);
  }
}

static void
SetTreeOwnerAndChromeEventHandlerOnDocshellTree(nsIDocShellTreeItem* aItem,
                                                nsIDocShellTreeOwner* aOwner,
                                                nsIDOMEventTarget* aHandler)
{
  NS_PRECONDITION(aItem, "Must have item");

  aItem->SetTreeOwner(aOwner);
  nsCOMPtr<nsIDocShell> shell(do_QueryInterface(aItem));
  shell->SetChromeEventHandler(aHandler);

  PRInt32 childCount = 0;
  aItem->GetChildCount(&childCount);
  for (PRInt32 i = 0; i < childCount; ++i) {
    nsCOMPtr<nsIDocShellTreeItem> item;
    aItem->GetChildAt(i, getter_AddRefs(item));
    SetTreeOwnerAndChromeEventHandlerOnDocshellTree(item, aOwner, aHandler);
  }
}

/**
 * Set the type of the treeitem and hook it up to the treeowner.
 * @param aItem the treeitem we're wrking working with
 * @param aOwningContent the content node that owns aItem
 * @param aTreeOwner the relevant treeowner; might be null
 * @param aParentType the nsIDocShellTreeItem::GetType of our parent docshell
 * @param aParentNode if non-null, the docshell we should be added as a child to
 *
 * @return whether aItem is top-level content
 */
static PRBool
AddTreeItemToTreeOwner(nsIDocShellTreeItem* aItem, nsIContent* aOwningContent,
                       nsIDocShellTreeOwner* aOwner, PRInt32 aParentType,
                       nsIDocShellTreeNode* aParentNode)
{
  NS_PRECONDITION(aItem, "Must have docshell treeitem");
  NS_PRECONDITION(aOwningContent, "Must have owning content");
  
  nsAutoString value;
  PRBool isContent = PR_FALSE;

  if (aOwningContent->IsXUL()) {
      aOwningContent->GetAttr(kNameSpaceID_None, nsGkAtoms::type, value);
  }

  // we accept "content" and "content-xxx" values.
  // at time of writing, we expect "xxx" to be "primary" or "targetable", but
  // someday it might be an integer expressing priority or something else.

  isContent = value.LowerCaseEqualsLiteral("content") ||
    StringBeginsWith(value, NS_LITERAL_STRING("content-"),
                     nsCaseInsensitiveStringComparator());

  if (isContent) {
    // The web shell's type is content.

    aItem->SetItemType(nsIDocShellTreeItem::typeContent);
  } else {
    // Inherit our type from our parent docshell.  If it is
    // chrome, we'll be chrome.  If it is content, we'll be
    // content.

    aItem->SetItemType(aParentType);
  }

  // Now that we have our type set, add ourselves to the parent, as needed.
  if (aParentNode) {
    aParentNode->AddChild(aItem);
  }

  PRBool retval = PR_FALSE;
  if (aParentType == nsIDocShellTreeItem::typeChrome && isContent) {
    retval = PR_TRUE;

    PRBool is_primary = value.LowerCaseEqualsLiteral("content-primary");

    if (aOwner) {
      PRBool is_targetable = is_primary ||
        value.LowerCaseEqualsLiteral("content-targetable");
      aOwner->ContentShellAdded(aItem, is_primary, is_targetable, value);
    }
  }

  return retval;
}

static PRBool
AllDescendantsOfType(nsIDocShellTreeItem* aParentItem, PRInt32 aType)
{
  PRInt32 childCount = 0;
  aParentItem->GetChildCount(&childCount);

  for (PRInt32 i = 0; i < childCount; ++i) {
    nsCOMPtr<nsIDocShellTreeItem> kid;
    aParentItem->GetChildAt(i, getter_AddRefs(kid));

    PRInt32 kidType;
    kid->GetItemType(&kidType);
    if (kidType != aType || !AllDescendantsOfType(kid, aType)) {
      return PR_FALSE;
    }
  }

  return PR_TRUE;
}

nsresult
nsFrameLoader::SwapWithOtherLoader(nsFrameLoader* aOther,
                                   nsRefPtr<nsFrameLoader>& aFirstToSwap,
                                   nsRefPtr<nsFrameLoader>& aSecondToSwap)
{
  NS_PRECONDITION((aFirstToSwap == this && aSecondToSwap == aOther) ||
                  (aFirstToSwap == aOther && aSecondToSwap == this),
                  "Swapping some sort of random loaders?");

  nsIContent* ourContent = mOwnerContent;
  nsIContent* otherContent = aOther->mOwnerContent;

  if (!ourContent || !otherContent) {
    // Can't handle this
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  // Make sure there are no same-origin issues
  PRBool equal;
  nsresult rv =
    ourContent->NodePrincipal()->Equals(otherContent->NodePrincipal(), &equal);
  if (NS_FAILED(rv) || !equal) {
    // Security problems loom.  Just bail on it all
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  nsCOMPtr<nsIDocShell> ourDochell = GetExistingDocShell();
  nsCOMPtr<nsIDocShell> otherDocshell = aOther->GetExistingDocShell();
  if (!ourDochell || !otherDocshell) {
    // How odd
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  // To avoid having to mess with session history, avoid swapping
  // frameloaders that don't correspond to root same-type docshells,
  // unless both roots have session history disabled.
  nsCOMPtr<nsIDocShellTreeItem> ourTreeItem = do_QueryInterface(ourDochell);
  nsCOMPtr<nsIDocShellTreeItem> otherTreeItem =
    do_QueryInterface(otherDocshell);
  nsCOMPtr<nsIDocShellTreeItem> ourRootTreeItem, otherRootTreeItem;
  ourTreeItem->GetSameTypeRootTreeItem(getter_AddRefs(ourRootTreeItem));
  otherTreeItem->GetSameTypeRootTreeItem(getter_AddRefs(otherRootTreeItem));
  nsCOMPtr<nsIWebNavigation> ourRootWebnav =
    do_QueryInterface(ourRootTreeItem);
  nsCOMPtr<nsIWebNavigation> otherRootWebnav =
    do_QueryInterface(otherRootTreeItem);

  if (!ourRootWebnav || !otherRootWebnav) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  nsCOMPtr<nsISHistory> ourHistory;
  nsCOMPtr<nsISHistory> otherHistory;
  ourRootWebnav->GetSessionHistory(getter_AddRefs(ourHistory));
  otherRootWebnav->GetSessionHistory(getter_AddRefs(otherHistory));

  if ((ourRootTreeItem != ourTreeItem || otherRootTreeItem != otherTreeItem) &&
      (ourHistory || otherHistory)) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  // Also make sure that the two docshells are the same type. Otherwise
  // swapping is certainly not safe.
  PRInt32 ourType = nsIDocShellTreeItem::typeChrome;
  PRInt32 otherType = nsIDocShellTreeItem::typeChrome;
  ourTreeItem->GetItemType(&ourType);
  otherTreeItem->GetItemType(&otherType);
  if (ourType != otherType) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  // One more twist here.  Setting up the right treeowners in a heterogeneous
  // tree is a bit of a pain.  So make sure that if ourType is not
  // nsIDocShellTreeItem::typeContent then all of our descendants are the same
  // type as us.
  if (ourType != nsIDocShellTreeItem::typeContent &&
      (!AllDescendantsOfType(ourTreeItem, ourType) ||
       !AllDescendantsOfType(otherTreeItem, otherType))) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  
  // Save off the tree owners, frame elements, chrome event handlers, and
  // docshell and document parents before doing anything else.
  nsCOMPtr<nsIDocShellTreeOwner> ourOwner, otherOwner;
  ourTreeItem->GetTreeOwner(getter_AddRefs(ourOwner));
  otherTreeItem->GetTreeOwner(getter_AddRefs(otherOwner));
  // Note: it's OK to have null treeowners.

  nsCOMPtr<nsIDocShellTreeItem> ourParentItem, otherParentItem;
  ourTreeItem->GetParent(getter_AddRefs(ourParentItem));
  otherTreeItem->GetParent(getter_AddRefs(otherParentItem));
  if (!ourParentItem || !otherParentItem) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  // Make sure our parents are the same type too
  PRInt32 ourParentType = nsIDocShellTreeItem::typeContent;
  PRInt32 otherParentType = nsIDocShellTreeItem::typeContent;
  ourParentItem->GetItemType(&ourParentType);
  otherParentItem->GetItemType(&otherParentType);
  if (ourParentType != otherParentType) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  nsCOMPtr<nsPIDOMWindow> ourWindow = do_GetInterface(ourDochell);
  nsCOMPtr<nsPIDOMWindow> otherWindow = do_GetInterface(otherDocshell);

  nsCOMPtr<nsIDOMElement> ourFrameElement =
    ourWindow->GetFrameElementInternal();
  nsCOMPtr<nsIDOMElement> otherFrameElement =
    otherWindow->GetFrameElementInternal();

  nsCOMPtr<nsIDOMEventTarget> ourChromeEventHandler =
    do_QueryInterface(ourWindow->GetChromeEventHandler());
  nsCOMPtr<nsIDOMEventTarget> otherChromeEventHandler =
    do_QueryInterface(otherWindow->GetChromeEventHandler());

  NS_ASSERTION(SameCOMIdentity(ourFrameElement, ourContent) &&
               SameCOMIdentity(otherFrameElement, otherContent) &&
               SameCOMIdentity(ourChromeEventHandler, ourContent) &&
               SameCOMIdentity(otherChromeEventHandler, otherContent),
               "How did that happen, exactly?");

  nsCOMPtr<nsIDocument> ourChildDocument =
    do_QueryInterface(ourWindow->GetExtantDocument());
  nsCOMPtr<nsIDocument> otherChildDocument =
    do_QueryInterface(otherWindow->GetExtantDocument());
  if (!ourChildDocument || !otherChildDocument) {
    // This shouldn't be happening
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  nsCOMPtr<nsIDocument> ourParentDocument =
    ourChildDocument->GetParentDocument();
  nsCOMPtr<nsIDocument> otherParentDocument =
    otherChildDocument->GetParentDocument();

  // Make sure to swap docshells between the two frames.
  nsIDocument* ourDoc = ourContent->GetCurrentDoc();
  nsIDocument* otherDoc = otherContent->GetCurrentDoc();
  if (!ourDoc || !otherDoc) {
    // Again, how odd, given that we had docshells
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  NS_ASSERTION(ourDoc == ourParentDocument, "Unexpected parent document");
  NS_ASSERTION(otherDoc == otherParentDocument, "Unexpected parent document");

  nsPresShellIterator iter1(ourDoc);
  nsPresShellIterator iter2(otherDoc);
  if (iter1.HasMoreThanOneShell() || iter2.HasMoreThanOneShell()) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  nsIPresShell* ourShell = ourDoc->GetPrimaryShell();
  nsIPresShell* otherShell = otherDoc->GetPrimaryShell();
  if (!ourShell || !otherShell) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  if (mInSwap || aOther->mInSwap) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  mInSwap = aOther->mInSwap = PR_TRUE;

  // Fire pageshow events on still-loading pages, and then fire pagehide
  // events.  Note that we do NOT fire these in the normal way, but just fire
  // them on the chrome event handlers.
  FirePageShowEvent(ourTreeItem, ourChromeEventHandler, PR_FALSE);
  FirePageShowEvent(otherTreeItem, otherChromeEventHandler, PR_FALSE);
  FirePageHideEvent(ourTreeItem, ourChromeEventHandler);
  FirePageHideEvent(otherTreeItem, otherChromeEventHandler);
  
  nsIFrame* ourFrame = ourShell->GetPrimaryFrameFor(ourContent);
  nsIFrame* otherFrame = otherShell->GetPrimaryFrameFor(otherContent);
  if (!ourFrame || !otherFrame) {
    mInSwap = aOther->mInSwap = PR_FALSE;
    FirePageShowEvent(ourTreeItem, ourChromeEventHandler, PR_TRUE);
    FirePageShowEvent(otherTreeItem, otherChromeEventHandler, PR_TRUE);
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  nsIFrameFrame* ourFrameFrame = do_QueryFrame(ourFrame);
  if (!ourFrameFrame) {
    mInSwap = aOther->mInSwap = PR_FALSE;
    FirePageShowEvent(ourTreeItem, ourChromeEventHandler, PR_TRUE);
    FirePageShowEvent(otherTreeItem, otherChromeEventHandler, PR_TRUE);
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  // OK.  First begin to swap the docshells in the two nsIFrames
  rv = ourFrameFrame->BeginSwapDocShells(otherFrame);
  if (NS_FAILED(rv)) {
    mInSwap = aOther->mInSwap = PR_FALSE;
    FirePageShowEvent(ourTreeItem, ourChromeEventHandler, PR_TRUE);
    FirePageShowEvent(otherTreeItem, otherChromeEventHandler, PR_TRUE);
    return rv;
  }

  // Now move the docshells to the right docshell trees.  Note that this
  // resets their treeowners to null.
  ourParentItem->RemoveChild(ourTreeItem);
  otherParentItem->RemoveChild(otherTreeItem);
  if (ourType == nsIDocShellTreeItem::typeContent) {
    ourOwner->ContentShellRemoved(ourTreeItem);
    otherOwner->ContentShellRemoved(otherTreeItem);
  }
  
  ourParentItem->AddChild(otherTreeItem);
  otherParentItem->AddChild(ourTreeItem);

  // Restore the correct treeowners
  SetTreeOwnerAndChromeEventHandlerOnDocshellTree(ourTreeItem, otherOwner,
                                                  otherChromeEventHandler);
  SetTreeOwnerAndChromeEventHandlerOnDocshellTree(otherTreeItem, ourOwner,
                                                  ourChromeEventHandler);

  AddTreeItemToTreeOwner(ourTreeItem, otherContent, otherOwner,
                         otherParentType, nsnull);
  AddTreeItemToTreeOwner(otherTreeItem, ourContent, ourOwner, ourParentType,
                         nsnull);

  // SetSubDocumentFor nulls out parent documents on the old child doc if a
  // new non-null document is passed in, so just go ahead and remove both
  // kids before reinserting in the parent subdoc maps, to avoid
  // complications.
  ourParentDocument->SetSubDocumentFor(ourContent, nsnull);
  otherParentDocument->SetSubDocumentFor(otherContent, nsnull);
  ourParentDocument->SetSubDocumentFor(ourContent, otherChildDocument);
  otherParentDocument->SetSubDocumentFor(otherContent, ourChildDocument);

  ourWindow->SetFrameElementInternal(otherFrameElement);
  otherWindow->SetFrameElementInternal(ourFrameElement);

  mOwnerContent = otherContent;
  aOther->mOwnerContent = ourContent;

  aFirstToSwap.swap(aSecondToSwap);

  // Drop any cached content viewers in the two session histories.
  nsCOMPtr<nsISHistoryInternal> ourInternalHistory =
    do_QueryInterface(ourHistory);
  nsCOMPtr<nsISHistoryInternal> otherInternalHistory =
    do_QueryInterface(otherHistory);
  if (ourInternalHistory) {
    ourInternalHistory->EvictAllContentViewers();
  }
  if (otherInternalHistory) {
    otherInternalHistory->EvictAllContentViewers();
  }

  // We shouldn't have changed frames, but be really careful about it
  if (ourFrame == ourShell->GetPrimaryFrameFor(ourContent) &&
      otherFrame == otherShell->GetPrimaryFrameFor(otherContent)) {
    ourFrameFrame->EndSwapDocShells(otherFrame);
  }

  ourParentDocument->FlushPendingNotifications(Flush_Layout);
  otherParentDocument->FlushPendingNotifications(Flush_Layout);
  
  FirePageShowEvent(ourTreeItem, otherChromeEventHandler, PR_TRUE);
  FirePageShowEvent(otherTreeItem, ourChromeEventHandler, PR_TRUE);

  mInSwap = aOther->mInSwap = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsFrameLoader::Destroy()
{
  if (mDestroyCalled) {
    return NS_OK;
  }
  mDestroyCalled = PR_TRUE;

  nsCOMPtr<nsIDocument> doc;
  if (mOwnerContent) {
    doc = mOwnerContent->GetOwnerDoc();

    if (doc) {
      doc->SetSubDocumentFor(mOwnerContent, nsnull);
    }

    mOwnerContent = nsnull;
  }

  // Let the tree owner know we're gone.
  if (mIsTopLevelContent) {
    nsCOMPtr<nsIDocShellTreeItem> ourItem = do_QueryInterface(mDocShell);
    if (ourItem) {
      nsCOMPtr<nsIDocShellTreeItem> parentItem;
      ourItem->GetParent(getter_AddRefs(parentItem));
      nsCOMPtr<nsIDocShellTreeOwner> owner = do_GetInterface(parentItem);
      if (owner) {
        owner->ContentShellRemoved(ourItem);
      }
    }
  }
  
  // Let our window know that we are gone
  nsCOMPtr<nsPIDOMWindow> win_private(do_GetInterface(mDocShell));
  if (win_private) {
    win_private->SetFrameElementInternal(nsnull);
  }

  if ((mNeedsAsyncDestroy || !doc ||
       NS_FAILED(doc->FinalizeFrameLoader(this))) && mDocShell) {
    nsCOMPtr<nsIRunnable> event = new nsAsyncDocShellDestroyer(mDocShell);
    NS_ENSURE_TRUE(event, NS_ERROR_OUT_OF_MEMORY);
    NS_DispatchToCurrentThread(event);

    // Let go of our docshell now that the async destroyer holds on to
    // the docshell.

    mDocShell = nsnull;
  }

  // NOTE: 'this' may very well be gone by now.

  return NS_OK;
}

NS_IMETHODIMP
nsFrameLoader::GetDepthTooGreat(PRBool* aDepthTooGreat)
{
  *aDepthTooGreat = mDepthTooGreat;
  return NS_OK;
}

nsresult
nsFrameLoader::EnsureDocShell()
{
  if (mDocShell) {
    return NS_OK;
  }
  NS_ENSURE_STATE(!mDestroyCalled);

  // Get our parent docshell off the document of mOwnerContent
  // XXXbz this is such a total hack.... We really need to have a
  // better setup for doing this.
  nsIDocument* doc = mOwnerContent->GetDocument();
  if (!doc) {
    return NS_ERROR_UNEXPECTED;
  }

  if (doc->GetDisplayDocument()) {
    // Don't allow subframe loads in external reference documents
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsCOMPtr<nsIWebNavigation> parentAsWebNav =
    do_GetInterface(doc->GetScriptGlobalObject());

  // Create the docshell...
  mDocShell = do_CreateInstance("@mozilla.org/docshell;1");
  NS_ENSURE_TRUE(mDocShell, NS_ERROR_FAILURE);

  // Get the frame name and tell the docshell about it.
  nsCOMPtr<nsIDocShellTreeItem> docShellAsItem(do_QueryInterface(mDocShell));
  NS_ENSURE_TRUE(docShellAsItem, NS_ERROR_FAILURE);
  nsAutoString frameName;

  PRInt32 namespaceID = mOwnerContent->GetNameSpaceID();
  if (namespaceID == kNameSpaceID_XHTML && !mOwnerContent->IsInHTMLDocument()) {
    mOwnerContent->GetAttr(kNameSpaceID_None, nsGkAtoms::id, frameName);
  } else {
    mOwnerContent->GetAttr(kNameSpaceID_None, nsGkAtoms::name, frameName);
    // XXX if no NAME then use ID, after a transition period this will be
    // changed so that XUL only uses ID too (bug 254284).
    if (frameName.IsEmpty() && namespaceID == kNameSpaceID_XUL) {
      mOwnerContent->GetAttr(kNameSpaceID_None, nsGkAtoms::id, frameName);
    }
  }

  if (!frameName.IsEmpty()) {
    docShellAsItem->SetName(frameName.get());
  }

  // If our container is a web-shell, inform it that it has a new
  // child. If it's not a web-shell then some things will not operate
  // properly.

  nsCOMPtr<nsIDocShellTreeNode> parentAsNode(do_QueryInterface(parentAsWebNav));
  if (parentAsNode) {
    // Note: This logic duplicates a lot of logic in
    // nsSubDocumentFrame::AttributeChanged.  We should fix that.

    nsCOMPtr<nsIDocShellTreeItem> parentAsItem =
      do_QueryInterface(parentAsNode);

    PRInt32 parentType;
    parentAsItem->GetItemType(&parentType);

    // XXXbz why is this in content code, exactly?  We should handle
    // this some other way.....  Not sure how yet.
    nsCOMPtr<nsIDocShellTreeOwner> parentTreeOwner;
    parentAsItem->GetTreeOwner(getter_AddRefs(parentTreeOwner));
    mIsTopLevelContent =
      AddTreeItemToTreeOwner(docShellAsItem, mOwnerContent, parentTreeOwner,
                             parentType, parentAsNode);

    // Make sure all shells have links back to the content element
    // in the nearest enclosing chrome shell.
    nsCOMPtr<nsIDOMEventTarget> chromeEventHandler;

    if (parentType == nsIDocShellTreeItem::typeChrome) {
      // Our parent shell is a chrome shell. It is therefore our nearest
      // enclosing chrome shell.

      chromeEventHandler = do_QueryInterface(mOwnerContent);
      NS_ASSERTION(chromeEventHandler,
                   "This mContent should implement this.");
    } else {
      nsCOMPtr<nsIDocShell> parentShell(do_QueryInterface(parentAsNode));

      // Our parent shell is a content shell. Get the chrome event
      // handler from it and use that for our shell as well.

      parentShell->GetChromeEventHandler(getter_AddRefs(chromeEventHandler));
    }

    mDocShell->SetChromeEventHandler(chromeEventHandler);
  }

  // This is nasty, this code (the do_GetInterface(mDocShell) below)
  // *must* come *after* the above call to
  // mDocShell->SetChromeEventHandler() for the global window to get
  // the right chrome event handler.

  // Tell the window about the frame that hosts it.
  nsCOMPtr<nsIDOMElement> frame_element(do_QueryInterface(mOwnerContent));
  NS_ASSERTION(frame_element, "frame loader owner element not a DOM element!");

  nsCOMPtr<nsPIDOMWindow> win_private(do_GetInterface(mDocShell));
  nsCOMPtr<nsIBaseWindow> base_win(do_QueryInterface(mDocShell));
  if (win_private) {
    win_private->SetFrameElementInternal(frame_element);
  }

  // This is kinda whacky, this call doesn't really create anything,
  // but it must be called to make sure things are properly
  // initialized.
  if (NS_FAILED(base_win->Create()) || !win_private) {
    // Do not call Destroy() here. See bug 472312.
    NS_WARNING("Something wrong when creating the docshell for a frameloader!");
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

void
nsFrameLoader::GetURL(nsString& aURI)
{
  aURI.Truncate();

  if (mOwnerContent->Tag() == nsGkAtoms::object) {
    mOwnerContent->GetAttr(kNameSpaceID_None, nsGkAtoms::data, aURI);
  } else {
    mOwnerContent->GetAttr(kNameSpaceID_None, nsGkAtoms::src, aURI);
  }
}

nsresult
nsFrameLoader::CheckForRecursiveLoad(nsIURI* aURI)
{
  mDepthTooGreat = PR_FALSE;
  nsresult rv = EnsureDocShell();
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDocShellTreeItem> treeItem = do_QueryInterface(mDocShell);
  NS_ASSERTION(treeItem, "docshell must be a treeitem!");
  
  PRInt32 ourType;
  rv = treeItem->GetItemType(&ourType);
  if (NS_SUCCEEDED(rv) && ourType != nsIDocShellTreeItem::typeContent) {
    // No need to do recursion-protection here XXXbz why not??  Do we really
    // trust people not to screw up with non-content docshells?
    return NS_OK;
  }

  // Bug 8065: Don't exceed some maximum depth in content frames
  // (MAX_DEPTH_CONTENT_FRAMES)
  nsCOMPtr<nsIDocShellTreeItem> parentAsItem;
  treeItem->GetSameTypeParent(getter_AddRefs(parentAsItem));
  PRInt32 depth = 0;
  while (parentAsItem) {
    ++depth;
    
    if (depth >= MAX_DEPTH_CONTENT_FRAMES) {
      mDepthTooGreat = PR_TRUE;
      NS_WARNING("Too many nested content frames so giving up");

      return NS_ERROR_UNEXPECTED; // Too deep, give up!  (silently?)
    }

    nsCOMPtr<nsIDocShellTreeItem> temp;
    temp.swap(parentAsItem);
    temp->GetSameTypeParent(getter_AddRefs(parentAsItem));
  }
  
  // Bug 136580: Check for recursive frame loading
  // pre-grab these for speed
  nsCOMPtr<nsIURI> cloneURI;
  rv = aURI->Clone(getter_AddRefs(cloneURI));
  NS_ENSURE_SUCCESS(rv, rv);
  
  // Bug 98158/193011: We need to ignore data after the #
  nsCOMPtr<nsIURL> cloneURL(do_QueryInterface(cloneURI)); // QI can fail
  if (cloneURL) {
    rv = cloneURL->SetRef(EmptyCString());
    NS_ENSURE_SUCCESS(rv,rv);
  }

  PRInt32 matchCount = 0;
  treeItem->GetSameTypeParent(getter_AddRefs(parentAsItem));
  while (parentAsItem) {
    // Check the parent URI with the URI we're loading
    nsCOMPtr<nsIWebNavigation> parentAsNav(do_QueryInterface(parentAsItem));
    if (parentAsNav) {
      // Does the URI match the one we're about to load?
      nsCOMPtr<nsIURI> parentURI;
      parentAsNav->GetCurrentURI(getter_AddRefs(parentURI));
      if (parentURI) {
        nsCOMPtr<nsIURI> parentClone;
        rv = parentURI->Clone(getter_AddRefs(parentClone));
        NS_ENSURE_SUCCESS(rv, rv);
        nsCOMPtr<nsIURL> parentURL(do_QueryInterface(parentClone));
        if (parentURL) {
          rv = parentURL->SetRef(EmptyCString());
          NS_ENSURE_SUCCESS(rv,rv);
        }

        PRBool equal;
        rv = cloneURI->Equals(parentClone, &equal);
        NS_ENSURE_SUCCESS(rv, rv);
        
        if (equal) {
          matchCount++;
          if (matchCount >= MAX_SAME_URL_CONTENT_FRAMES) {
            NS_WARNING("Too many nested content frames have the same url (recursion?) so giving up");
            return NS_ERROR_UNEXPECTED;
          }
        }
      }
    }
    nsCOMPtr<nsIDocShellTreeItem> temp;
    temp.swap(parentAsItem);
    temp->GetSameTypeParent(getter_AddRefs(parentAsItem));
  }

  return NS_OK;
}
