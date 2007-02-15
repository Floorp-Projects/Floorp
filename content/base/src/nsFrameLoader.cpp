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

#include "nsIURI.h"
#include "nsIURL.h"
#include "nsNetUtil.h"

#include "nsGkAtoms.h"
#include "nsINameSpaceManager.h"

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

NS_IMPL_ISUPPORTS1(nsFrameLoader, nsIFrameLoader)

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
                   charset);
  }

  NS_ENSURE_SUCCESS(rv, rv);
  return LoadURI(uri);
}

NS_IMETHODIMP
nsFrameLoader::LoadURI(nsIURI* aURI)
{
  NS_PRECONDITION(aURI, "Null URI?");
  if (!aURI)
    return NS_ERROR_INVALID_POINTER;

  nsIDocument* doc = mOwnerContent->GetOwnerDoc();
  if (!doc) {
    return NS_OK;
  }

  nsresult rv = EnsureDocShell();
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDocShellLoadInfo> loadInfo;
  mDocShell->CreateLoadInfo(getter_AddRefs(loadInfo));
  NS_ENSURE_TRUE(loadInfo, NS_ERROR_FAILURE);

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
  rv = secMan->CheckLoadURIWithPrincipal(principal, aURI,
                                         nsIScriptSecurityManager::STANDARD);
  if (NS_FAILED(rv)) {
    return rv; // We're not
  }

  // Bail out if this is an infinite recursion scenario
  rv = CheckForRecursiveLoad(aURI);
  NS_ENSURE_SUCCESS(rv, rv);
  
  // We'll use our principal, not that of the document loaded inside us.  This
  // is very important; needed to prevent XSS attacks on documents loaded in
  // subframes!  But only use our principal if our docshell's type is the same
  // as the type of our ownerDocument's docshell.  Note that we could try
  // checking GetSameTypeParent() on mDocShell, but that might break if we ever
  // support docshells loaded inside disconnected nodes...
  nsCOMPtr<nsISupports> container = doc->GetContainer();
  nsCOMPtr<nsIDocShellTreeItem> parentItem = do_QueryInterface(container);
  nsCOMPtr<nsIDocShellTreeItem> ourItem = do_QueryInterface(mDocShell);
  NS_ASSERTION(ourItem, "Must have item");
  if (parentItem) {
    PRInt32 parentType;
    rv = parentItem->GetItemType(&parentType);
    PRInt32 ourType;
    nsresult rv2 = ourItem->GetItemType(&ourType);
    if (NS_SUCCEEDED(rv) && NS_SUCCEEDED(rv2) && ourType == parentType) {
      loadInfo->SetOwner(principal);
    }
  }

  nsCOMPtr<nsIURI> referrer;
  rv = principal->GetURI(getter_AddRefs(referrer));
  NS_ENSURE_SUCCESS(rv, rv);

  loadInfo->SetReferrer(referrer);

  // Kick off the load...
  rv = mDocShell->LoadURI(aURI, loadInfo, nsIWebNavigation::LOAD_FLAGS_NONE,
                          PR_FALSE);
#ifdef DEBUG
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to load the URL");
  }
#endif

  return rv;
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

NS_IMETHODIMP
nsFrameLoader::Destroy()
{
  if (mOwnerContent) {
    nsCOMPtr<nsIDocument> doc = mOwnerContent->GetDocument();

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
      nsCOMPtr<nsIDocShellTreeOwner_MOZILLA_1_8_BRANCH> owner2 =
        do_QueryInterface(owner);
      if (owner2) {
        owner2->ContentShellRemoved(ourItem);
      }
    }
  }
  
  // Let our window know that we are gone
  nsCOMPtr<nsPIDOMWindow> win_private(do_GetInterface(mDocShell));
  if (win_private) {
    win_private->SetFrameElementInternal(nsnull);
  }
  
  nsCOMPtr<nsIBaseWindow> base_win(do_QueryInterface(mDocShell));

  if (base_win) {
    base_win->Destroy();
  }

  mDocShell = nsnull;
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

  // Get our parent docshell off the document of mOwnerContent
  // XXXbz this is such a total hack.... We really need to have a
  // better setup for doing this.
  nsIDocument* doc = mOwnerContent->GetDocument();
  if (!doc) {
    return NS_ERROR_UNEXPECTED;
  }

  nsCOMPtr<nsIWebNavigation> parentAsWebNav =
    do_GetInterface(doc->GetScriptGlobalObject());

  // Create the docshell...
  mDocShell = do_CreateInstance("@mozilla.org/webshell;1");
  NS_ENSURE_TRUE(mDocShell, NS_ERROR_FAILURE);

  // Get the frame name and tell the docshell about it.
  nsCOMPtr<nsIDocShellTreeItem> docShellAsItem(do_QueryInterface(mDocShell));
  NS_ENSURE_TRUE(docShellAsItem, NS_ERROR_FAILURE);
  nsAutoString frameName;

  PRInt32 namespaceID = mOwnerContent->GetNameSpaceID();
  if (namespaceID == kNameSpaceID_XHTML) {
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

    nsAutoString value;
    PRBool isContent = PR_FALSE;

    if (mOwnerContent->IsNodeOfType(nsINode::eXUL)) {
      mOwnerContent->GetAttr(kNameSpaceID_None, nsGkAtoms::type, value);
    }

    // we accept "content" and "content-xxx" values.
    // at time of writing, we expect "xxx" to be "primary" or "targetable", but
    // someday it might be an integer expressing priority or something else.

    isContent = value.LowerCaseEqualsLiteral("content") ||
      StringBeginsWith(value, NS_LITERAL_STRING("content-"),
                       nsCaseInsensitiveStringComparator());

    if (isContent) {
      // The web shell's type is content.

      docShellAsItem->SetItemType(nsIDocShellTreeItem::typeContent);
    } else {
      // Inherit our type from our parent webshell.  If it is
      // chrome, we'll be chrome.  If it is content, we'll be
      // content.

      docShellAsItem->SetItemType(parentType);
    }

    parentAsNode->AddChild(docShellAsItem);

    if (parentType == nsIDocShellTreeItem::typeChrome && isContent) {
      mIsTopLevelContent = PR_TRUE;
      
      // XXXbz why is this in content code, exactly?  We should handle
      // this some other way.....  Not sure how yet.
      nsCOMPtr<nsIDocShellTreeOwner> parentTreeOwner;
      parentAsItem->GetTreeOwner(getter_AddRefs(parentTreeOwner));
      nsCOMPtr<nsIDocShellTreeOwner_MOZILLA_1_8_BRANCH> owner2 =
        do_QueryInterface(parentTreeOwner);

      PRBool is_primary = value.LowerCaseEqualsLiteral("content-primary");

      if (owner2) {
        PRBool is_targetable = is_primary ||
          value.LowerCaseEqualsLiteral("content-targetable");
        owner2->ContentShellAdded2(docShellAsItem, is_primary, is_targetable,
                                   value);
      } else if (parentTreeOwner) {
        parentTreeOwner->ContentShellAdded(docShellAsItem, is_primary,
                                           value.get());
      }
    }

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
  NS_ENSURE_TRUE(win_private, NS_ERROR_UNEXPECTED);

  win_private->SetFrameElementInternal(frame_element);

  nsCOMPtr<nsIBaseWindow> base_win(do_QueryInterface(mDocShell));
  NS_ENSURE_TRUE(base_win, NS_ERROR_UNEXPECTED);

  // This is kinda whacky, this call doesn't really create anything,
  // but it must be called to make sure things are properly
  // initialized

  base_win->Create();

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
  
  NS_PRECONDITION(mDocShell, "Must have docshell here");
  
  nsCOMPtr<nsIDocShellTreeItem> treeItem = do_QueryInterface(mDocShell);
  NS_ASSERTION(treeItem, "docshell must be a treeitem!");
  
  PRInt32 ourType;
  nsresult rv = treeItem->GetItemType(&ourType);
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
