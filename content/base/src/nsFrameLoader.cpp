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

#include "nsIFrameLoader.h"
#include "nsIDOMHTMLIFrameElement.h"
#include "nsIDOMHTMLFrameElement.h"
#include "nsIDOMWindow.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOMWindow.h"
#include "nsPIDOMWindow.h"
#include "nsIWebNavigation.h"
#include "nsIChromeEventHandler.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocShellTreeNode.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIDocShellLoadInfo.h"
#include "nsIBaseWindow.h"
#include "nsIWebShell.h"
#include "nsContentUtils.h"
#include "nsUnicharUtils.h"

#include "nsIScriptSecurityManager.h"

#include "nsIURI.h"
#include "nsIURL.h"
#include "nsNetUtil.h"

#include "nsHTMLAtoms.h"
#include "nsINameSpaceManager.h"

// Bug 136580: Limit to the number of nested content frames that can have the
//             same URL. This is to stop content that is recursively loading
//             itself.  Note that "#foo" on the end of URL doesn't affect
//             whether it's considered identical, but "?foo" or ";foo" are
//             considered and compared.
#define MAX_SAME_URL_CONTENT_FRAMES 3

// Bug 8065: Limit content frame depth to some reasonable level. This
// does not count chrome frames when determining depth, nor does it
// prevent chrome recursion.  Number is fairly arbitrary, but meant to
// keep number of shells to a reasonable number on accidental recursion with a
// small (but not 1) branching factor.  With large branching factors the number
// of shells can rapidly become huge and run us out of memory.  To solve that,
// we'd need to re-institute a fixed version of bug 98158.
#define MAX_DEPTH_CONTENT_FRAMES 10


class nsFrameLoader : public nsIFrameLoader
{
public:
  nsFrameLoader();
  virtual ~nsFrameLoader();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIFrameLoader
  NS_IMETHOD Init(nsIContent *aOwner);
  NS_IMETHOD LoadFrame();
  NS_IMETHOD GetDocShell(nsIDocShell **aDocShell);
  NS_IMETHOD Destroy();

protected:
  nsresult GetPresContext(nsIPresContext **aPresContext);
  nsresult EnsureDocShell();
  void GetURL(nsAString& aURL);

  nsCOMPtr<nsIDocShell> mDocShell;

  nsIContent *mOwnerContent; // WEAK
};

nsresult
NS_NewFrameLoader(nsIFrameLoader **aFrameLoader)
{
  *aFrameLoader = new nsFrameLoader();
  NS_ENSURE_TRUE(*aFrameLoader, NS_ERROR_OUT_OF_MEMORY);

  NS_ADDREF(*aFrameLoader);

  return NS_OK;
}

nsFrameLoader::nsFrameLoader()
  : mOwnerContent(nsnull)
{
}

nsFrameLoader::~nsFrameLoader()
{
  Destroy();
}


// QueryInterface implementation for nsFrameLoader
NS_INTERFACE_MAP_BEGIN(nsFrameLoader)
  NS_INTERFACE_MAP_ENTRY(nsIFrameLoader)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END


NS_IMPL_ADDREF(nsFrameLoader)
NS_IMPL_RELEASE(nsFrameLoader)

NS_IMETHODIMP
nsFrameLoader::Init(nsIContent *aOwner)
{
  mOwnerContent = aOwner; // WEAK

  return NS_OK;
}

NS_IMETHODIMP
nsFrameLoader::LoadFrame()
{
  NS_ENSURE_TRUE(mOwnerContent, NS_ERROR_NOT_INITIALIZED);

  nsresult rv = EnsureDocShell();
  NS_ENSURE_SUCCESS(rv, rv);

  nsIDocument* doc = mOwnerContent->GetDocument();
  if (!doc) {
    return NS_OK;
  }

  nsAutoString src;
  GetURL(src);

  src.Trim(" \t\n\r");

  if (src.IsEmpty()) {
    src.Assign(NS_LITERAL_STRING("about:blank"));
  }

  // Make an absolute URI
  nsIURI *base_uri = doc->GetBaseURI();

  const nsACString &doc_charset = doc->GetDocumentCharacterSet();

  nsCOMPtr<nsIURI> uri;
  rv = NS_NewURI(getter_AddRefs(uri), src,
                 doc_charset.IsEmpty() ? nsnull :
                 PromiseFlatCString(doc_charset).get(), base_uri);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDocShellLoadInfo> loadInfo;
  mDocShell->CreateLoadInfo(getter_AddRefs(loadInfo));
  NS_ENSURE_TRUE(loadInfo, NS_ERROR_FAILURE);

  // Check for security
  nsIScriptSecurityManager *secMan = nsContentUtils::GetSecurityManager();

  // Get referring URL
  nsCOMPtr<nsIURI> referrer;
  nsCOMPtr<nsIPrincipal> principal;
  rv = secMan->GetSubjectPrincipal(getter_AddRefs(principal));
  NS_ENSURE_SUCCESS(rv, rv);

  // If we were called from script, get the referring URL from the script

  if (principal) {
    // Pass the script principal to the docshell

    loadInfo->SetOwner(principal);
  } else {
    // We're not being called form script, tell the docshell
    // to inherit an owner from the current document.

    loadInfo->SetInheritOwner(PR_TRUE);
    principal = doc->GetPrincipal();
  }

  if (!principal) {
    return NS_ERROR_FAILURE;
  }
  
  // Check if we are allowed to load absURL
  rv = secMan->CheckLoadURIWithPrincipal(principal, uri,
                                         nsIScriptSecurityManager::STANDARD);
  if (NS_FAILED(rv)) {
    return rv; // We're not
  }

  rv = principal->GetURI(getter_AddRefs(referrer));
  NS_ENSURE_SUCCESS(rv, rv);

  loadInfo->SetReferrer(referrer);

  // Bug 136580: Check for recursive frame loading
  // pre-grab these for speed
  nsCAutoString prepath;
  nsCAutoString filepath;
  nsCAutoString query;
  nsCAutoString param;
  rv = uri->GetPrePath(prepath);
  NS_ENSURE_SUCCESS(rv,rv);
  nsCOMPtr<nsIURL> aURL(do_QueryInterface(uri, &rv)); // QI can fail
  if (NS_SUCCEEDED(rv)) {
    rv = aURL->GetFilePath(filepath);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = aURL->GetQuery(query);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = aURL->GetParam(param);
    NS_ENSURE_SUCCESS(rv,rv);
  } else {
    // Not a URL, so punt and just take the whole path.  Note that if you
    // have a self-referential-via-refs non-URL (can't happen via nsSimpleURI,
    // but could in theory with an external protocol handler) frameset it will
    // recurse down to the depth limit before stopping, but it will stop.
    rv = uri->GetPath(filepath);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  PRInt32 matchCount = 0;
  nsCOMPtr<nsIDocShellTreeItem> treeItem = do_QueryInterface(mDocShell);
  nsCOMPtr<nsIDocShellTreeItem> parentAsItem;
  treeItem->GetParent(getter_AddRefs(parentAsItem));
  while (parentAsItem) {
    // Only interested in checking for recursion in content
    PRInt32 parentType;
    parentAsItem->GetItemType(&parentType);
    if (parentType != nsIDocShellTreeItem::typeContent) {
      break; // Not content
    }
    // Check the parent URI with the URI we're loading
    nsCOMPtr<nsIWebNavigation> parentAsNav(do_QueryInterface(parentAsItem));
    if (parentAsNav) {
      // Does the URI match the one we're about to load?
      nsCOMPtr<nsIURI> parentURI;
      parentAsNav->GetCurrentURI(getter_AddRefs(parentURI));
      if (parentURI) {
        // Bug 98158/193011: We need to ignore data after the #
        // Note that this code is back-stopped by the maximum-depth checks.
        
        // Check prepath (foo://blah@bar:port/) and filepath
        // (/dir/dir/file.ext), but not # extensions.
        // There's no easy way to get the URI without extension
        // directly, so check prepath, filepath and query/param (if any)

        // Note that while in theory a CGI could return different data for
        // the same query string, the spec states that it shouldn't, so
        // we'll compare queries (and params).
        nsCAutoString parentPrePath;
        nsCAutoString parentFilePath;
        nsCAutoString parentQuery;
        nsCAutoString parentParam;
        rv = parentURI->GetPrePath(parentPrePath);
        NS_ENSURE_SUCCESS(rv,rv);
        nsCOMPtr<nsIURL> parentURL(do_QueryInterface(parentURI, &rv)); // QI can fail
        if (NS_SUCCEEDED(rv)) {
          rv = parentURL->GetFilePath(parentFilePath);
          NS_ENSURE_SUCCESS(rv,rv);
          rv = parentURL->GetQuery(parentQuery);
          NS_ENSURE_SUCCESS(rv,rv);
          rv = parentURL->GetParam(parentParam);
          NS_ENSURE_SUCCESS(rv,rv);
        } else {
          rv = uri->GetPath(filepath);
          NS_ENSURE_SUCCESS(rv,rv);
        }
        
        // filepath will often not match; test it first
        if (filepath.Equals(parentFilePath) &&
            query.Equals(parentQuery) &&
            prepath.Equals(parentPrePath) &&
            param.Equals(parentParam))
        {
          matchCount++;
          if (matchCount >= MAX_SAME_URL_CONTENT_FRAMES) {
            NS_WARNING("Too many nested content frames have the same url (recursion?) so giving up");
            return NS_ERROR_UNEXPECTED;
          }
        }
      }
    }
    nsIDocShellTreeItem* temp = parentAsItem;
    temp->GetParent(getter_AddRefs(parentAsItem));
  }
  
  // Kick off the load...
  rv = mDocShell->LoadURI(uri, loadInfo, nsIWebNavigation::LOAD_FLAGS_NONE,
                          PR_FALSE);
  NS_ASSERTION(NS_SUCCEEDED(rv), "failed to load URL");

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

nsresult
nsFrameLoader::GetPresContext(nsIPresContext **aPresContext)
{
  *aPresContext = nsnull;

  nsIDocument* doc = mOwnerContent->GetDocument();

  while (doc) {
    nsIPresShell *presShell = doc->GetShellAt(0);

    if (presShell) {
      presShell->GetPresContext(aPresContext);

      return NS_OK;
    }

    doc = doc->GetParentDocument();
  }

  return NS_OK;
}

nsresult
nsFrameLoader::EnsureDocShell()
{
  if (mDocShell) {
    return NS_OK;
  }

  nsCOMPtr<nsIPresContext> presContext;
  GetPresContext(getter_AddRefs(presContext));
  NS_ENSURE_TRUE(presContext, NS_ERROR_UNEXPECTED);

  // Bug 8065: Don't exceed some maximum depth in content frames
  // (MAX_DEPTH_CONTENT_FRAMES)
  PRInt32 depth = 0;
  nsCOMPtr<nsISupports> parentAsSupports = presContext->GetContainer();

  if (parentAsSupports) {
    nsCOMPtr<nsIDocShellTreeItem> parentAsItem =
      do_QueryInterface(parentAsSupports);

    while (parentAsItem) {
      ++depth;

      if (depth >= MAX_DEPTH_CONTENT_FRAMES) {
        NS_WARNING("Too many nested content frames so giving up");

        return NS_ERROR_UNEXPECTED; // Too deep, give up!  (silently?)
      }

      // Only count depth on content, not chrome.
      // If we wanted to limit total depth, skip the following check:
      PRInt32 parentType;
      parentAsItem->GetItemType(&parentType);

      if (nsIDocShellTreeItem::typeContent == parentType) {
        nsIDocShellTreeItem* temp = parentAsItem;
        temp->GetParent(getter_AddRefs(parentAsItem));
      } else {
        break; // we have exited content, stop counting, depth is OK!
      }
    }
  }

  // Create the docshell...
  mDocShell = do_CreateInstance("@mozilla.org/webshell;1");
  NS_ENSURE_TRUE(mDocShell, NS_ERROR_FAILURE);

  // Get the frame name and tell the docshell about it.
  nsCOMPtr<nsIDocShellTreeItem> docShellAsItem(do_QueryInterface(mDocShell));
  NS_ENSURE_TRUE(docShellAsItem, NS_ERROR_FAILURE);
  nsAutoString frameName;
  mOwnerContent->GetAttr(kNameSpaceID_None, nsHTMLAtoms::name, frameName);

  if (!frameName.IsEmpty()) {
    docShellAsItem->SetName(frameName.get());
  }

  // If our container is a web-shell, inform it that it has a new
  // child. If it's not a web-shell then some things will not operate
  // properly.

  nsCOMPtr<nsISupports> container = presContext->GetContainer();

  nsCOMPtr<nsIDocShellTreeNode> parentAsNode(do_QueryInterface(container));
  if (parentAsNode) {
    nsCOMPtr<nsIDocShellTreeItem> parentAsItem =
      do_QueryInterface(parentAsNode);

    PRInt32 parentType;
    parentAsItem->GetItemType(&parentType);

    nsAutoString value;
    PRBool isContent = PR_FALSE;

    if (mOwnerContent->IsContentOfType(nsIContent::eXUL)) {
      mOwnerContent->GetAttr(kNameSpaceID_None, nsHTMLAtoms::type, value);
    }

    // we accept "content" and "content-xxx" values.
    // at time of writing, we expect "xxx" to be "primary", but
    // someday it might be an integer expressing priority

    if (value.Length() >= 7) {
      // Lowercase the value, ContentShellAdded() further down relies
      // on it being lowercased.
      ToLowerCase(value);

      nsAutoString::const_char_iterator start, end;
      value.BeginReading(start);
      value.EndReading(end);

      nsAutoString::const_char_iterator iter(start + 7);

      isContent = Substring(start, iter).EqualsLiteral("content") &&
                  (iter == end || *iter == '-');
    }

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

    if (isContent) {
      nsCOMPtr<nsIDocShellTreeOwner> parentTreeOwner;
      parentAsItem->GetTreeOwner(getter_AddRefs(parentTreeOwner));

      if (parentTreeOwner) {
        PRBool is_primary = parentType == nsIDocShellTreeItem::typeChrome &&
                            value.EqualsLiteral("content-primary");

        parentTreeOwner->ContentShellAdded(docShellAsItem, is_primary,
                                           value.get());
      }
    }

    // connect the container...
    nsCOMPtr<nsIWebShell> webShell(do_QueryInterface(mDocShell));
    nsCOMPtr<nsIWebShellContainer> outerContainer =
      do_QueryInterface(container);

    if (outerContainer) {
      webShell->SetContainer(outerContainer);
    }

    // Make sure all shells have links back to the content element
    // in the nearest enclosing chrome shell.
    nsCOMPtr<nsIChromeEventHandler> chromeEventHandler;

    if (parentType == nsIDocShellTreeItem::typeChrome) {
      // Our parent shell is a chrome shell. It is therefore our nearest
      // enclosing chrome shell.

      chromeEventHandler = do_QueryInterface(mOwnerContent);
      NS_WARN_IF_FALSE(chromeEventHandler,
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
nsFrameLoader::GetURL(nsAString& aURI)
{
  aURI.Truncate();

  if (mOwnerContent->Tag() == nsHTMLAtoms::object) {
    mOwnerContent->GetAttr(kNameSpaceID_None, nsHTMLAtoms::data, aURI);
  } else {
    mOwnerContent->GetAttr(kNameSpaceID_None, nsHTMLAtoms::src, aURI);
  }
}

