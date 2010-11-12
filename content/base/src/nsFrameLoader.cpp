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
 *   Frederic Plourde <frederic.plourde@polymtl.ca>
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

#ifdef MOZ_IPC
#  include "base/basictypes.h"
#endif

#include "prenv.h"

#include "nsIDOMHTMLIFrameElement.h"
#include "nsIDOMHTMLFrameElement.h"
#include "nsIDOMWindow.h"
#include "nsIPresShell.h"
#include "nsIContent.h"
#include "nsIContentViewer.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOMWindow.h"
#include "nsPIDOMWindow.h"
#include "nsIWebNavigation.h"
#include "nsIWebProgress.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocShellTreeNode.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIDocShellLoadInfo.h"
#include "nsIBaseWindow.h"
#include "nsContentUtils.h"
#include "nsIXPConnect.h"
#include "nsIJSContextStack.h"
#include "nsUnicharUtils.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptSecurityManager.h"
#include "nsIScrollable.h"
#include "nsFrameLoader.h"
#include "nsIDOMEventTarget.h"
#include "nsIFrame.h"
#include "nsSubDocumentFrame.h"
#include "nsDOMError.h"
#include "nsGUIEvent.h"
#include "nsEventDispatcher.h"
#include "nsISHistory.h"
#include "nsISHistoryInternal.h"
#include "nsIDocShellHistory.h"
#include "nsIDOMNSHTMLDocument.h"
#include "nsIXULWindow.h"

#include "nsLayoutUtils.h"
#include "nsIView.h"
#include "nsPLDOMEvent.h"

#include "nsIURI.h"
#include "nsIURL.h"
#include "nsNetUtil.h"

#include "nsGkAtoms.h"
#include "nsINameSpaceManager.h"

#include "nsThreadUtils.h"
#include "nsIContentViewer.h"
#include "nsIView.h"

#include "nsIDOMChromeWindow.h"
#include "nsInProcessTabChildGlobal.h"
#include "mozilla/AutoRestore.h"
#include "mozilla/unused.h"

#ifdef MOZ_IPC
#include "ContentParent.h"
#include "TabParent.h"

using namespace mozilla;
using namespace mozilla::dom;
#endif

#include "jsapi.h"

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

NS_IMPL_CYCLE_COLLECTION_CLASS(nsFrameLoader)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsFrameLoader)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mDocShell)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mMessageManager)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mChildMessageManager)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsFrameLoader)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mDocShell)
  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "nsFrameLoader::mMessageManager");
  cb.NoteXPCOMChild(static_cast<nsIContentFrameMessageManager*>(tmp->mMessageManager.get()));
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mChildMessageManager)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsFrameLoader)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsFrameLoader)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsFrameLoader)
  NS_INTERFACE_MAP_ENTRY(nsIFrameLoader)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

nsFrameLoader*
nsFrameLoader::Create(nsIContent* aOwner, PRBool aNetworkCreated)
{
  NS_ENSURE_TRUE(aOwner, nsnull);
  nsIDocument* doc = aOwner->GetOwnerDoc();
  NS_ENSURE_TRUE(doc && !doc->GetDisplayDocument() &&
                 ((!doc->IsLoadedAsData() && aOwner->GetCurrentDoc()) ||
                   doc->IsStaticDocument()),
                 nsnull);

  return new nsFrameLoader(aOwner, aNetworkCreated);
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
  if (!doc || doc->IsStaticDocument()) {
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

  if (NS_SUCCEEDED(rv)) {
    rv = LoadURI(uri);
  }
  
  if (NS_FAILED(rv)) {
    FireErrorEvent();

    return rv;
  }

  return NS_OK;
}

void
nsFrameLoader::FireErrorEvent()
{
  if (mOwnerContent) {
    nsRefPtr<nsPLDOMEvent> event =
      new nsLoadBlockingPLDOMEvent(mOwnerContent, NS_LITERAL_STRING("error"),
                                   PR_FALSE, PR_FALSE);
    event->PostDOMEvent();
  }
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
  nsresult rv = ReallyStartLoadingInternal();
  if (NS_FAILED(rv)) {
    FireErrorEvent();
  }
  
  return rv;
}

nsresult
nsFrameLoader::ReallyStartLoadingInternal()
{
  NS_ENSURE_STATE(mURIToLoad && mOwnerContent && mOwnerContent->IsInDoc());

  nsresult rv = MaybeCreateDocShell();
  if (NS_FAILED(rv)) {
    return rv;
  }

#ifdef MOZ_IPC
  if (mRemoteFrame) {
    if (!mRemoteBrowser) {
      TryRemoteBrowser();

      if (!mRemoteBrowser) {
        NS_WARNING("Couldn't create child process for iframe.");
        return NS_ERROR_FAILURE;
      }
    }

    // FIXME get error codes from child
    mRemoteBrowser->LoadURL(mURIToLoad);
    return NS_OK;
  }
#endif

  NS_ASSERTION(mDocShell,
               "MaybeCreateDocShell succeeded with a null mDocShell");

  // Just to be safe, recheck uri.
  rv = CheckURILoad(mURIToLoad);
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
  NS_ENSURE_SUCCESS(rv, rv);

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
  rv = MaybeCreateDocShell();
  if (NS_FAILED(rv)) {
    return rv;
  }
#ifdef MOZ_IPC
  if (mRemoteFrame) {
    return NS_OK;
  }
#endif
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
    nsresult rv = MaybeCreateDocShell();
    if (NS_FAILED(rv))
      return rv;
#ifdef MOZ_IPC
    if (mRemoteFrame) {
      NS_WARNING("No docshells for remote frames!");
      return NS_ERROR_NOT_AVAILABLE;
    }
#endif
    NS_ASSERTION(mDocShell,
                 "MaybeCreateDocShell succeeded, but null mDocShell");
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

/**
 * A class that automatically sets mInShow to false when it goes
 * out of scope.
 */
class NS_STACK_CLASS AutoResetInShow {
  private:
    nsFrameLoader* mFrameLoader;
    MOZILLA_DECL_USE_GUARD_OBJECT_NOTIFIER
  public:
    AutoResetInShow(nsFrameLoader* aFrameLoader MOZILLA_GUARD_OBJECT_NOTIFIER_PARAM)
      : mFrameLoader(aFrameLoader)
    {
      MOZILLA_GUARD_OBJECT_NOTIFIER_INIT;
    }
    ~AutoResetInShow() { mFrameLoader->mInShow = PR_FALSE; }
};


PRBool
nsFrameLoader::Show(PRInt32 marginWidth, PRInt32 marginHeight,
                    PRInt32 scrollbarPrefX, PRInt32 scrollbarPrefY,
                    nsSubDocumentFrame* frame)
{
  if (mInShow) {
    return PR_FALSE;
  }
  // Reset mInShow if we exit early.
  AutoResetInShow resetInShow(this);
  mInShow = PR_TRUE;

  nsresult rv = MaybeCreateDocShell();
  if (NS_FAILED(rv)) {
    return PR_FALSE;
  }

#ifdef MOZ_IPC
  if (!mRemoteFrame)
#endif
  {
    if (!mDocShell)
      return PR_FALSE;
    nsCOMPtr<nsIPresShell> presShell;
    mDocShell->GetPresShell(getter_AddRefs(presShell));
    if (presShell)
      return PR_TRUE;

    mDocShell->SetMarginWidth(marginWidth);
    mDocShell->SetMarginHeight(marginHeight);

    nsCOMPtr<nsIScrollable> sc = do_QueryInterface(mDocShell);
    if (sc) {
      sc->SetDefaultScrollbarPreferences(nsIScrollable::ScrollOrientation_X,
                                         scrollbarPrefX);
      sc->SetDefaultScrollbarPreferences(nsIScrollable::ScrollOrientation_Y,
                                         scrollbarPrefY);
    }
  }

  nsIView* view = frame->EnsureInnerView();
  if (!view)
    return PR_FALSE;

#ifdef MOZ_IPC
  if (mRemoteFrame) {
    return ShowRemoteFrame(GetSubDocumentSize(frame));
  }
#endif

  nsCOMPtr<nsIBaseWindow> baseWindow = do_QueryInterface(mDocShell);
  NS_ASSERTION(baseWindow, "Found a nsIDocShell that isn't a nsIBaseWindow.");
  nsIntSize size;
  if (!(frame->GetStateBits() & NS_FRAME_FIRST_REFLOW)) {
    // We have a useful size already; use it, since we might get no
    // more size updates.
    size = GetSubDocumentSize(frame);
  } else {
    // Pick some default size for now.  Using 10x10 because that's what the
    // code here used to do.
    size.SizeTo(10, 10);
  }
  baseWindow->InitWindow(nsnull, view->GetWidget(), 0, 0,
                         size.width, size.height);
  // This is kinda whacky, this "Create()" call doesn't really
  // create anything, one starts to wonder why this was named
  // "Create"...
  baseWindow->Create();
  baseWindow->SetVisibility(PR_TRUE);

  // Trigger editor re-initialization if midas is turned on in the
  // sub-document. This shouldn't be necessary, but given the way our
  // editor works, it is. See
  // https://bugzilla.mozilla.org/show_bug.cgi?id=284245
  nsCOMPtr<nsIPresShell> presShell;
  mDocShell->GetPresShell(getter_AddRefs(presShell));
  if (presShell) {
    nsCOMPtr<nsIDOMNSHTMLDocument> doc =
      do_QueryInterface(presShell->GetDocument());

    if (doc) {
      nsAutoString designMode;
      doc->GetDesignMode(designMode);

      if (designMode.EqualsLiteral("on")) {
        doc->SetDesignMode(NS_LITERAL_STRING("off"));
        doc->SetDesignMode(NS_LITERAL_STRING("on"));
      }
    }
  }

  mInShow = PR_FALSE;
  if (mHideCalled) {
    mHideCalled = PR_FALSE;
    Hide();
    return PR_FALSE;
  }
  return PR_TRUE;
}

#ifdef MOZ_IPC
bool
nsFrameLoader::ShowRemoteFrame(const nsIntSize& size)
{
  NS_ASSERTION(mRemoteFrame, "ShowRemote only makes sense on remote frames.");

  if (!mRemoteBrowser) {
    TryRemoteBrowser();

    if (!mRemoteBrowser) {
      NS_ERROR("Couldn't create child process.");
      return false;
    }
  }

  // FIXME/bug 589337: Show()/Hide() is pretty expensive for
  // cross-process layers; need to figure out what behavior we really
  // want here.  For now, hack.
  if (!mRemoteBrowserShown) {
    mRemoteBrowser->Show(size);
    mRemoteBrowserShown = PR_TRUE;

    EnsureMessageManager();
  } else {
    mRemoteBrowser->Move(size);
  }

  return true;
}
#endif

void
nsFrameLoader::Hide()
{
  if (mHideCalled) {
    return;
  }
  if (mInShow) {
    mHideCalled = PR_TRUE;
    return;
  }

  if (!mDocShell)
    return;

  nsCOMPtr<nsIContentViewer> contentViewer;
  mDocShell->GetContentViewer(getter_AddRefs(contentViewer));
  if (contentViewer)
    contentViewer->SetSticky(PR_FALSE);

  nsCOMPtr<nsIBaseWindow> baseWin = do_QueryInterface(mDocShell);
  NS_ASSERTION(baseWin,
               "Found an nsIDocShell which doesn't implement nsIBaseWindow.");
  baseWin->SetVisibility(PR_FALSE);
  baseWin->SetParentWidget(nsnull);
}

nsresult
nsFrameLoader::SwapWithOtherLoader(nsFrameLoader* aOther,
                                   nsRefPtr<nsFrameLoader>& aFirstToSwap,
                                   nsRefPtr<nsFrameLoader>& aSecondToSwap)
{
  NS_PRECONDITION((aFirstToSwap == this && aSecondToSwap == aOther) ||
                  (aFirstToSwap == aOther && aSecondToSwap == this),
                  "Swapping some sort of random loaders?");
  NS_ENSURE_STATE(!mInShow && !aOther->mInShow);

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

  nsIPresShell* ourShell = ourDoc->GetShell();
  nsIPresShell* otherShell = otherDoc->GetShell();
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
  
  nsIFrame* ourFrame = ourContent->GetPrimaryFrame();
  nsIFrame* otherFrame = otherContent->GetPrimaryFrame();
  if (!ourFrame || !otherFrame) {
    mInSwap = aOther->mInSwap = PR_FALSE;
    FirePageShowEvent(ourTreeItem, ourChromeEventHandler, PR_TRUE);
    FirePageShowEvent(otherTreeItem, otherChromeEventHandler, PR_TRUE);
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  nsSubDocumentFrame* ourFrameFrame = do_QueryFrame(ourFrame);
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

  nsRefPtr<nsFrameMessageManager> ourMessageManager = mMessageManager;
  nsRefPtr<nsFrameMessageManager> otherMessageManager = aOther->mMessageManager;
  // Swap pointers in child message managers.
  if (mChildMessageManager) {
    nsInProcessTabChildGlobal* tabChild =
      static_cast<nsInProcessTabChildGlobal*>(mChildMessageManager.get());
    tabChild->SetOwner(otherContent);
    tabChild->SetChromeMessageManager(otherMessageManager);
  }
  if (aOther->mChildMessageManager) {
    nsInProcessTabChildGlobal* otherTabChild =
      static_cast<nsInProcessTabChildGlobal*>(aOther->mChildMessageManager.get());
    otherTabChild->SetOwner(ourContent);
    otherTabChild->SetChromeMessageManager(ourMessageManager);
  }
  // Swap and setup things in parent message managers.
  nsFrameMessageManager* ourParentManager = mMessageManager ?
    mMessageManager->GetParentManager() : nsnull;
  nsFrameMessageManager* otherParentManager = aOther->mMessageManager ?
    aOther->mMessageManager->GetParentManager() : nsnull;
  if (mMessageManager) {
    mMessageManager->Disconnect();
    mMessageManager->SetParentManager(otherParentManager);
    mMessageManager->SetCallbackData(aOther, PR_FALSE);
  }
  if (aOther->mMessageManager) {
    aOther->mMessageManager->Disconnect();
    aOther->mMessageManager->SetParentManager(ourParentManager);
    aOther->mMessageManager->SetCallbackData(this, PR_FALSE);
  }
  mMessageManager.swap(aOther->mMessageManager);

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

  NS_ASSERTION(ourFrame == ourContent->GetPrimaryFrame() &&
               otherFrame == otherContent->GetPrimaryFrame(),
               "changed primary frame");

  ourFrameFrame->EndSwapDocShells(otherFrame);

  ourParentDocument->FlushPendingNotifications(Flush_Layout);
  otherParentDocument->FlushPendingNotifications(Flush_Layout);

  FirePageShowEvent(ourTreeItem, otherChromeEventHandler, PR_TRUE);
  FirePageShowEvent(otherTreeItem, ourChromeEventHandler, PR_TRUE);

  mInSwap = aOther->mInSwap = PR_FALSE;
  return NS_OK;
}

void
nsFrameLoader::DestroyChild()
{
#ifdef MOZ_IPC
  if (mRemoteBrowser) {
    mRemoteBrowser->SetOwnerElement(nsnull);
    // If this fails, it's most likely due to a content-process crash,
    // and auto-cleanup will kick in.  Otherwise, the child side will
    // destroy itself and send back __delete__().
    unused << mRemoteBrowser->SendDestroy();
    mRemoteBrowser = nsnull;
  }
#endif
}

NS_IMETHODIMP
nsFrameLoader::Destroy()
{
  if (mDestroyCalled) {
    return NS_OK;
  }
  mDestroyCalled = PR_TRUE;

  if (mMessageManager) {
    mMessageManager->Disconnect();
  }
  if (mChildMessageManager) {
    static_cast<nsInProcessTabChildGlobal*>(mChildMessageManager.get())->Disconnect();
  }

  nsCOMPtr<nsIDocument> doc;
  PRBool dynamicSubframeRemoval = PR_FALSE;
  if (mOwnerContent) {
    doc = mOwnerContent->GetOwnerDoc();

    if (doc) {
      dynamicSubframeRemoval = !mIsTopLevelContent && !doc->InUnlinkOrDeletion();
      doc->SetSubDocumentFor(mOwnerContent, nsnull);
    }

    mOwnerContent = nsnull;
  }
  DestroyChild();

  // Seems like this is a dynamic frame removal.
  if (dynamicSubframeRemoval) {
    nsCOMPtr<nsIDocShellHistory> dhistory = do_QueryInterface(mDocShell);
    if (dhistory) {
      dhistory->RemoveFromSessionHistory();
    }
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

#ifdef MOZ_IPC
bool
nsFrameLoader::ShouldUseRemoteProcess()
{
  // Check for *disabled* multi-process first: environment, prefs, attribute
  // Then check for *enabled* multi-process pref: attribute, prefs
  // Default is not-remote.

  if (PR_GetEnv("MOZ_DISABLE_OOP_TABS")) {
    return false;
  }

  PRBool remoteDisabled = nsContentUtils::GetBoolPref("dom.ipc.tabs.disabled",
                                                      PR_FALSE);
  if (remoteDisabled) {
    return false;
  }

  static nsIAtom* const *const remoteValues[] = {
    &nsGkAtoms::_false,
    &nsGkAtoms::_true,
    nsnull
  };

  switch (mOwnerContent->FindAttrValueIn(kNameSpaceID_None, nsGkAtoms::Remote,
                                         remoteValues, eCaseMatters)) {
  case 0:
    return false;
  case 1:
    return true;
  }

  PRBool remoteEnabled = nsContentUtils::GetBoolPref("dom.ipc.tabs.enabled",
                                                     PR_FALSE);
  return (bool) remoteEnabled;
}
#endif

nsresult
nsFrameLoader::MaybeCreateDocShell()
{
  if (mDocShell) {
    return NS_OK;
  }
#ifdef MOZ_IPC
  if (mRemoteFrame) {
    return NS_OK;
  }
#endif
  NS_ENSURE_STATE(!mDestroyCalled);

#ifdef MOZ_IPC
  if (ShouldUseRemoteProcess()) {
    mRemoteFrame = true;
    return NS_OK;
  }
#endif

  // Get our parent docshell off the document of mOwnerContent
  // XXXbz this is such a total hack.... We really need to have a
  // better setup for doing this.
  nsIDocument* doc = mOwnerContent->GetOwnerDoc();
  if (!doc || !(doc->IsStaticDocument() || mOwnerContent->IsInDoc())) {
    return NS_ERROR_UNEXPECTED;
  }

  if (doc->GetDisplayDocument()) {
    // Don't allow subframe loads in external reference documents
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsCOMPtr<nsISupports> container =
    doc->GetContainer();
  nsCOMPtr<nsIWebNavigation> parentAsWebNav = do_QueryInterface(container);

  // Create the docshell...
  mDocShell = do_CreateInstance("@mozilla.org/docshell;1");
  NS_ENSURE_TRUE(mDocShell, NS_ERROR_FAILURE);

  if (!mNetworkCreated) {
    nsCOMPtr<nsIDocShellHistory> history = do_QueryInterface(mDocShell);
    if (history) {
      history->SetCreatedDynamically(PR_TRUE);
    }
  }

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

  EnsureMessageManager();

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
  nsresult rv;

  mDepthTooGreat = PR_FALSE;
  rv = MaybeCreateDocShell();
  if (NS_FAILED(rv)) {
    return rv;
  }
#ifdef MOZ_IPC
  NS_ASSERTION(!mRemoteFrame,
               "Shouldn't call CheckForRecursiveLoad on remote frames.");
#endif
  if (!mDocShell) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIDocShellTreeItem> treeItem = do_QueryInterface(mDocShell);
  NS_ASSERTION(treeItem, "docshell must be a treeitem!");

  // Check that we're still in the docshell tree.
  nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
  treeItem->GetTreeOwner(getter_AddRefs(treeOwner));
  NS_WARN_IF_FALSE(treeOwner,
                   "Trying to load a new url to a docshell without owner!");
  NS_ENSURE_STATE(treeOwner);
  
  
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

NS_IMETHODIMP
nsFrameLoader::UpdatePositionAndSize(nsIFrame *aIFrame)
{
#ifdef MOZ_IPC
  if (mRemoteFrame) {
    if (mRemoteBrowser) {
      nsIntSize size = GetSubDocumentSize(aIFrame);
      mRemoteBrowser->Move(size);
    }
    return NS_OK;
  }
#endif
  return UpdateBaseWindowPositionAndSize(aIFrame);
}

nsresult
nsFrameLoader::UpdateBaseWindowPositionAndSize(nsIFrame *aIFrame)
{
  nsCOMPtr<nsIDocShell> docShell;
  GetDocShell(getter_AddRefs(docShell));
  nsCOMPtr<nsIBaseWindow> baseWindow(do_QueryInterface(docShell));

  // resize the sub document
  if (baseWindow) {
    PRInt32 x = 0;
    PRInt32 y = 0;

    nsWeakFrame weakFrame(aIFrame);

    baseWindow->GetPositionAndSize(&x, &y, nsnull, nsnull);

    if (!weakFrame.IsAlive()) {
      // GetPositionAndSize() killed us
      return NS_OK;
    }

    nsIntSize size = GetSubDocumentSize(aIFrame);

    baseWindow->SetPositionAndSize(x, y, size.width, size.height, PR_FALSE);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsFrameLoader::ScrollViewportTo(float aXpx, float aYpx)
{
  ViewportConfig config(mViewportConfig);
  config.mScrollOffset = nsPoint(nsPresContext::CSSPixelsToAppUnits(aXpx),
                                nsPresContext::CSSPixelsToAppUnits(aYpx));
  return UpdateViewportConfig(config);
}

NS_IMETHODIMP
nsFrameLoader::ScrollViewportBy(float aDXpx, float aDYpx)
{
  ViewportConfig config(mViewportConfig);
  config.mScrollOffset.MoveBy(nsPresContext::CSSPixelsToAppUnits(aDXpx),
                             nsPresContext::CSSPixelsToAppUnits(aDYpx));
  return UpdateViewportConfig(config);
}

NS_IMETHODIMP
nsFrameLoader::SetViewportScale(float aXScale, float aYScale)
{
  ViewportConfig config(mViewportConfig);
  config.mXScale = aXScale;
  config.mYScale = aYScale;
  return UpdateViewportConfig(config);
}

NS_IMETHODIMP
nsFrameLoader::GetViewportScrollX(float* aViewportScrollX)
{
  *aViewportScrollX =
    nsPresContext::AppUnitsToFloatCSSPixels(mViewportConfig.mScrollOffset.x);
  return NS_OK;
}

NS_IMETHODIMP
nsFrameLoader::GetViewportScrollY(float* aViewportScrollY)
{
  *aViewportScrollY =
    nsPresContext::AppUnitsToFloatCSSPixels(mViewportConfig.mScrollOffset.y);
  return NS_OK;
}

nsresult
nsFrameLoader::UpdateViewportConfig(const ViewportConfig& aNewConfig)
{
  if (aNewConfig == mViewportConfig) {
    return NS_OK;
  }
  mViewportConfig = aNewConfig;

  // Viewport changed.  Try to locate our subdoc frame and invalidate
  // it if found.
  nsIFrame* frame = GetPrimaryFrameOfOwningContent();
  if (!frame) {
    // Oops, don't have a frame right now.  That's OK; the viewport
    // config persists and will apply to the next frame we get, if we
    // ever get one.
    return NS_OK;
  }

  // XXX could be clever here and compute a smaller invalidation
  // rect
  nsRect rect = nsRect(nsPoint(0, 0), frame->GetRect().Size());
  // NB: we pass INVALIDATE_NO_THEBES_LAYERS here to keep viewport
  // semantics the same for both in-process and out-of-process
  // <browser>.  This is just a transform of the layer subtree in
  // both.
  frame->InvalidateWithFlags(rect, nsIFrame::INVALIDATE_NO_THEBES_LAYERS);

  return NS_OK;
}

nsIntSize
nsFrameLoader::GetSubDocumentSize(const nsIFrame *aIFrame)
{
  nsSize docSizeAppUnits;
  nsPresContext* presContext = aIFrame->PresContext();
  nsCOMPtr<nsIDOMHTMLFrameElement> frameElem = 
    do_QueryInterface(aIFrame->GetContent());
  if (frameElem) {
    docSizeAppUnits = aIFrame->GetSize();
  } else {
    docSizeAppUnits = aIFrame->GetContentRect().Size();
  }
  return nsIntSize(presContext->AppUnitsToDevPixels(docSizeAppUnits.width),
                   presContext->AppUnitsToDevPixels(docSizeAppUnits.height));
}

#ifdef MOZ_IPC
bool
nsFrameLoader::TryRemoteBrowser()
{
  NS_ASSERTION(!mRemoteBrowser, "TryRemoteBrowser called with a remote browser already?");

  nsIDocument* doc = mOwnerContent->GetDocument();
  if (!doc) {
    return false;
  }

  if (doc->GetDisplayDocument()) {
    // Don't allow subframe loads in external reference documents
    return false;
  }

  nsCOMPtr<nsIWebNavigation> parentAsWebNav =
    do_GetInterface(doc->GetScriptGlobalObject());

  if (!parentAsWebNav) {
    return false;
  }

  nsCOMPtr<nsIDocShellTreeItem> parentAsItem(do_QueryInterface(parentAsWebNav));

  PRInt32 parentType;
  parentAsItem->GetItemType(&parentType);

  if (parentType != nsIDocShellTreeItem::typeChrome) {
    return false;
  }

  if (!mOwnerContent->IsXUL()) {
    return false;
  }

  nsAutoString value;
  mOwnerContent->GetAttr(kNameSpaceID_None, nsGkAtoms::type, value);

  if (!value.LowerCaseEqualsLiteral("content") &&
      !StringBeginsWith(value, NS_LITERAL_STRING("content-"),
                        nsCaseInsensitiveStringComparator())) {
    return false;
  }

  PRUint32 chromeFlags = 0;
  nsCOMPtr<nsIDocShellTreeOwner> parentOwner;
  if (NS_FAILED(parentAsItem->GetTreeOwner(getter_AddRefs(parentOwner))) ||
      !parentOwner) {
    return false;
  }
  nsCOMPtr<nsIXULWindow> window(do_GetInterface(parentOwner));
  if (!window) {
    return false;
  }
  if (NS_FAILED(window->GetChromeFlags(&chromeFlags))) {
    return false;
  }

  ContentParent* parent = ContentParent::GetSingleton();
  NS_ASSERTION(parent->IsAlive(), "Process parent should be alive; something is very wrong!");
  mRemoteBrowser = parent->CreateTab(chromeFlags);
  if (mRemoteBrowser) {
    nsCOMPtr<nsIDOMElement> element = do_QueryInterface(mOwnerContent);
    mRemoteBrowser->SetOwnerElement(element);

    nsCOMPtr<nsIDocShellTreeItem> rootItem;
    parentAsItem->GetRootTreeItem(getter_AddRefs(rootItem));
    nsCOMPtr<nsIDOMWindow> rootWin = do_GetInterface(rootItem);
    nsCOMPtr<nsIDOMChromeWindow> rootChromeWin = do_QueryInterface(rootWin);
    NS_ABORT_IF_FALSE(rootChromeWin, "How did we not get a chrome window here?");

    nsCOMPtr<nsIBrowserDOMWindow> browserDOMWin;
    rootChromeWin->GetBrowserDOMWindow(getter_AddRefs(browserDOMWin));
    mRemoteBrowser->SetBrowserDOMWindow(browserDOMWin);
    
    mChildHost = parent;
  }
  return true;
}
#endif

#ifdef MOZ_IPC
mozilla::dom::PBrowserParent*
nsFrameLoader::GetRemoteBrowser()
{
  return mRemoteBrowser;
}
#endif

NS_IMETHODIMP
nsFrameLoader::ActivateRemoteFrame() {
#ifdef MOZ_IPC
  if (mRemoteBrowser) {
    mRemoteBrowser->Activate();
    return NS_OK;
  }
#endif
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsFrameLoader::SendCrossProcessMouseEvent(const nsAString& aType,
                                          float aX,
                                          float aY,
                                          PRInt32 aButton,
                                          PRInt32 aClickCount,
                                          PRInt32 aModifiers,
                                          PRBool aIgnoreRootScrollFrame)
{
#ifdef MOZ_IPC
  if (mRemoteBrowser) {
    mRemoteBrowser->SendMouseEvent(aType, aX, aY, aButton,
                                  aClickCount, aModifiers,
                                  aIgnoreRootScrollFrame);
  }
#endif
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsFrameLoader::ActivateFrameEvent(const nsAString& aType,
                                  PRBool aCapture)
{
#ifdef MOZ_IPC
  if (mRemoteBrowser) {
    return mRemoteBrowser->SendActivateFrameEvent(nsString(aType), aCapture) ?
      NS_OK : NS_ERROR_NOT_AVAILABLE;
  }
#endif
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsFrameLoader::SendCrossProcessKeyEvent(const nsAString& aType,
                                        PRInt32 aKeyCode,
                                        PRInt32 aCharCode,
                                        PRInt32 aModifiers,
                                        PRBool aPreventDefault)
{
#ifdef MOZ_IPC
  if (mRemoteBrowser) {
    mRemoteBrowser->SendKeyEvent(aType, aKeyCode, aCharCode, aModifiers,
                                aPreventDefault);
  }
#endif
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsFrameLoader::GetDelayRemoteDialogs(PRBool* aRetVal)
{
#ifdef MOZ_IPC
  *aRetVal = mDelayRemoteDialogs;
#else
  *aRetVal = PR_FALSE;
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsFrameLoader::SetDelayRemoteDialogs(PRBool aDelay)
{
#ifdef MOZ_IPC
  if (mRemoteBrowser && mDelayRemoteDialogs && !aDelay) {
    nsRefPtr<nsIRunnable> ev =
      NS_NewRunnableMethod(mRemoteBrowser,
                           &mozilla::dom::TabParent::HandleDelayedDialogs);
    NS_DispatchToCurrentThread(ev);
  }
  mDelayRemoteDialogs = aDelay;
#endif
  return NS_OK;
}

nsresult
nsFrameLoader::CreateStaticClone(nsIFrameLoader* aDest)
{
  nsFrameLoader* dest = static_cast<nsFrameLoader*>(aDest);
  dest->MaybeCreateDocShell();
  NS_ENSURE_STATE(dest->mDocShell);

  nsCOMPtr<nsIDOMDocument> dummy = do_GetInterface(dest->mDocShell);
  nsCOMPtr<nsIContentViewer> viewer;
  dest->mDocShell->GetContentViewer(getter_AddRefs(viewer));
  NS_ENSURE_STATE(viewer);

  nsCOMPtr<nsIDocShell> origDocShell;
  GetDocShell(getter_AddRefs(origDocShell));
  nsCOMPtr<nsIDOMDocument> domDoc = do_GetInterface(origDocShell);

  nsCOMPtr<nsIDocument> doc = do_QueryInterface(domDoc);
  NS_ENSURE_STATE(doc);
  nsCOMPtr<nsIDocument> clonedDoc = doc->CreateStaticClone(dest->mDocShell);
  nsCOMPtr<nsIDOMDocument> clonedDOMDoc = do_QueryInterface(clonedDoc);

  viewer->SetDOMDocument(clonedDOMDoc);
  return NS_OK;
}

bool LoadScript(void* aCallbackData, const nsAString& aURL)
{
#ifdef MOZ_IPC
  mozilla::dom::PBrowserParent* tabParent =
    static_cast<nsFrameLoader*>(aCallbackData)->GetRemoteBrowser();
  if (tabParent) {
    return tabParent->SendLoadRemoteScript(nsString(aURL));
  }
#endif
  nsFrameLoader* fl = static_cast<nsFrameLoader*>(aCallbackData);
  nsRefPtr<nsInProcessTabChildGlobal> tabChild =
    static_cast<nsInProcessTabChildGlobal*>(fl->GetTabChildGlobalAsEventTarget());
  if (tabChild) {
    tabChild->LoadFrameScript(aURL);
  }
  return true;
}

class nsAsyncMessageToChild : public nsRunnable
{
public:
  nsAsyncMessageToChild(nsFrameLoader* aFrameLoader,
                        const nsAString& aMessage, const nsAString& aJSON)
    : mFrameLoader(aFrameLoader), mMessage(aMessage), mJSON(aJSON) {}

  NS_IMETHOD Run()
  {
    nsInProcessTabChildGlobal* tabChild =
      static_cast<nsInProcessTabChildGlobal*>(mFrameLoader->mChildMessageManager.get());
    if (tabChild && tabChild->GetInnerManager()) {
      tabChild->GetInnerManager()->
        ReceiveMessage(static_cast<nsPIDOMEventTarget*>(tabChild), mMessage,
                       PR_FALSE, mJSON, nsnull, nsnull);
    }
    return NS_OK;
  }
  nsRefPtr<nsFrameLoader> mFrameLoader;
  nsString mMessage;
  nsString mJSON;
};

bool SendAsyncMessageToChild(void* aCallbackData,
                             const nsAString& aMessage,
                             const nsAString& aJSON)
{
#ifdef MOZ_IPC
  mozilla::dom::PBrowserParent* tabParent =
    static_cast<nsFrameLoader*>(aCallbackData)->GetRemoteBrowser();
  if (tabParent) {
    return tabParent->SendAsyncMessage(nsString(aMessage), nsString(aJSON));
  }
#endif
  nsRefPtr<nsIRunnable> ev =
    new nsAsyncMessageToChild(static_cast<nsFrameLoader*>(aCallbackData),
                              aMessage, aJSON);
  NS_DispatchToCurrentThread(ev);
  return true;
}

NS_IMETHODIMP
nsFrameLoader::GetMessageManager(nsIChromeFrameMessageManager** aManager)
{
  EnsureMessageManager();
  if (mMessageManager) {
    CallQueryInterface(mMessageManager, aManager);
  }
  return NS_OK;
}

nsresult
nsFrameLoader::EnsureMessageManager()
{
  NS_ENSURE_STATE(mOwnerContent);

  nsresult rv = MaybeCreateDocShell();
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (!mIsTopLevelContent
#ifdef MOZ_IPC
      && !mRemoteFrame
#endif
      ) {
    return NS_OK;
  }

  if (mMessageManager) {
#ifdef MOZ_IPC
    if (ShouldUseRemoteProcess()) {
      mMessageManager->SetCallbackData(mRemoteBrowserShown ? this : nsnull);
    }
#endif
    return NS_OK;
  }

  nsIScriptContext* sctx = mOwnerContent->GetContextForEventHandlers(&rv);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_STATE(sctx);
  JSContext* cx = static_cast<JSContext*>(sctx->GetNativeContext());
  NS_ENSURE_STATE(cx);

  nsCOMPtr<nsIDOMChromeWindow> chromeWindow =
    do_QueryInterface(mOwnerContent->GetOwnerDoc()->GetWindow());
  NS_ENSURE_STATE(chromeWindow);
  nsCOMPtr<nsIChromeFrameMessageManager> parentManager;
  chromeWindow->GetMessageManager(getter_AddRefs(parentManager));

#ifdef MOZ_IPC
  if (ShouldUseRemoteProcess()) {
    mMessageManager = new nsFrameMessageManager(PR_TRUE,
                                                nsnull,
                                                SendAsyncMessageToChild,
                                                LoadScript,
                                                mRemoteBrowserShown ? this : nsnull,
                                                static_cast<nsFrameMessageManager*>(parentManager.get()),
                                                cx);
    NS_ENSURE_TRUE(mMessageManager, NS_ERROR_OUT_OF_MEMORY);
  } else
#endif
  {

    mMessageManager = new nsFrameMessageManager(PR_TRUE,
                                                nsnull,
                                                SendAsyncMessageToChild,
                                                LoadScript,
                                                nsnull,
                                                static_cast<nsFrameMessageManager*>(parentManager.get()),
                                                cx);
    NS_ENSURE_TRUE(mMessageManager, NS_ERROR_OUT_OF_MEMORY);
    mChildMessageManager =
      new nsInProcessTabChildGlobal(mDocShell, mOwnerContent, mMessageManager);
    mMessageManager->SetCallbackData(this);
  }
  return NS_OK;
}

nsPIDOMEventTarget*
nsFrameLoader::GetTabChildGlobalAsEventTarget()
{
  return static_cast<nsInProcessTabChildGlobal*>(mChildMessageManager.get());
}
