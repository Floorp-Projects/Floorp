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

#include "nsIScriptSecurityManager.h"
#include "nsICodebasePrincipal.h"

#include "nsIURI.h"
#include "nsNetUtil.h"

#include "nsHTMLAtoms.h"
#include "nsINameSpaceManager.h"

// Bug 98158: Limit to the number of total docShells in one page.
#define MAX_NUMBER_DOCSHELLS 100

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

  PRInt32    GetDocShellChildCount(nsIDocShellTreeNode *aParentNode);

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
  NS_INIT_ISUPPORTS();
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


NS_IMPL_ADDREF(nsFrameLoader);
NS_IMPL_RELEASE(nsFrameLoader);

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

  nsCOMPtr<nsIDocument> doc;
  mOwnerContent->GetDocument(*getter_AddRefs(doc));
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
  nsCOMPtr<nsIURI> base_uri;
  doc->GetBaseURL(*getter_AddRefs(base_uri));

  nsAutoString doc_charset;
  doc->GetDocumentCharacterSet(doc_charset);

  nsCOMPtr<nsIURI> uri;
  rv = NS_NewURI(getter_AddRefs(uri), src,
                 doc_charset.IsEmpty() ? nsnull :
                 NS_ConvertUCS2toUTF8(doc_charset).get(), base_uri);
  NS_ENSURE_SUCCESS(rv, rv);

  // Check for security
  nsCOMPtr<nsIScriptSecurityManager> secMan =
    do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDocShellLoadInfo> loadInfo;
  mDocShell->CreateLoadInfo(getter_AddRefs(loadInfo));
  NS_ENSURE_TRUE(loadInfo, NS_ERROR_FAILURE);

  // Get referring URL
  nsCOMPtr<nsIURI> referrer;
  nsCOMPtr<nsIPrincipal> principal;
  rv = secMan->GetSubjectPrincipal(getter_AddRefs(principal));
  NS_ENSURE_SUCCESS(rv, rv);

  // If we were called from script, get the referring URL from the script

  if (principal) {
    nsCOMPtr<nsICodebasePrincipal> codebase(do_QueryInterface(principal));

    if (codebase) {
      rv = codebase->GetURI(getter_AddRefs(referrer));
      NS_ENSURE_SUCCESS(rv, rv);
    }

    // Pass the script principal to the docshell

    loadInfo->SetOwner(principal);
  }

  if (!referrer) {
    // We're not being called form script, tell the docshell
    // to inherit an owner from the current document.

    loadInfo->SetInheritOwner(PR_TRUE);

    referrer = base_uri;
  }

  loadInfo->SetReferrer(referrer);

  // Check if we are allowed to load absURL
  rv = secMan->CheckLoadURI(referrer, uri,
                            nsIScriptSecurityManager::STANDARD);
  if (NS_FAILED(rv)) {
    return rv; // We're not
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

  nsresult rv = EnsureDocShell();
  NS_ENSURE_SUCCESS(rv, rv);

  *aDocShell = mDocShell;
  NS_IF_ADDREF(*aDocShell);

  return NS_OK;
}

NS_IMETHODIMP
nsFrameLoader::Destroy()
{
  if (mOwnerContent) {
    nsCOMPtr<nsIDocument> doc;

    mOwnerContent->GetDocument(*getter_AddRefs(doc));

    if (doc) {
      doc->SetSubDocumentFor(mOwnerContent, nsnull);
    }

    mOwnerContent = nsnull;
  }

  nsCOMPtr<nsIBaseWindow> base_win(do_QueryInterface(mDocShell));

  if (base_win) {
    base_win->Destroy();
  }

  mDocShell = nsnull;

  return NS_OK;
}

/**
 *  Count the total number of docshell children in each page.
 */
PRInt32
nsFrameLoader::GetDocShellChildCount(nsIDocShellTreeNode* aParentNode)
{
  PRInt32 retval = 1;

  PRInt32 childCount;
  PRInt32 i;
  aParentNode->GetChildCount(&childCount);
  for(i=0;i<childCount;i++)
  {
    nsCOMPtr<nsIDocShellTreeItem> child;
    aParentNode->GetChildAt(i,getter_AddRefs(child));
    nsCOMPtr<nsIDocShellTreeNode> childAsNode(do_QueryInterface(child));
    retval += GetDocShellChildCount(childAsNode);
  }
    
  return retval;
}

nsresult
nsFrameLoader::GetPresContext(nsIPresContext **aPresContext)
{
  *aPresContext = nsnull;

  nsCOMPtr<nsIDocument> doc;
  mOwnerContent->GetDocument(*getter_AddRefs(doc));

  while (doc) {
    nsCOMPtr<nsIPresShell> presShell;
    doc->GetShellAt(0, getter_AddRefs(presShell));

    if (presShell) {
      presShell->GetPresContext(aPresContext);

      return NS_OK;
    }

    nsCOMPtr<nsIDocument> parent;
    doc->GetParentDocument(getter_AddRefs(parent));

    doc = parent;
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

  nsCOMPtr<nsISupports> parentAsSupports;
  presContext->GetContainer(getter_AddRefs(parentAsSupports));

  // bug98158:count the children under the root docshell.
  // if the total number of children under the root docshell
  // beyond the limit,return a error.
  if (parentAsSupports) {
    nsCOMPtr<nsIDocShellTreeItem> parentAsItem = do_QueryInterface(parentAsSupports);

    nsCOMPtr<nsIDocShellTreeItem> root;
    parentAsItem->GetSameTypeRootTreeItem(getter_AddRefs(root));

    nsCOMPtr<nsIDocShellTreeNode> rootNode(do_QueryInterface(root));

    PRInt32  childrenCount;
    childrenCount = GetDocShellChildCount(rootNode);

    if(childrenCount >= MAX_NUMBER_DOCSHELLS) {
      NS_WARNING("Too many docshell (recursion?) so giving up");
      return NS_ERROR_FAILURE;
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

  nsCOMPtr<nsISupports> container;
  presContext->GetContainer(getter_AddRefs(container));

  nsCOMPtr<nsIDocShellTreeNode> parentAsNode(do_QueryInterface(container));
  if (parentAsNode) {
    nsCOMPtr<nsIDocShellTreeItem> parentAsItem =
      do_QueryInterface(parentAsNode);

    PRInt32 parentType;
    parentAsItem->GetItemType(&parentType);

    nsAutoString value;
    PRBool isContent;

    isContent = PR_FALSE;

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

      nsAutoString::const_iterator start, end;
      value.BeginReading(start);
      value.EndReading(end);

      nsAutoString::const_iterator iter(start);
      iter.advance(7);

      const nsAString& valuePiece = Substring(start, iter);

      if (valuePiece.Equals(NS_LITERAL_STRING("content")) &&
          (iter == end || *iter == '-')) {
        isContent = PR_TRUE;
      }
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

      if(parentTreeOwner) {
        PRBool is_primary = parentType == nsIDocShellTreeItem::typeChrome &&
                            value.Equals(NS_LITERAL_STRING("content-primary"));

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

  nsCOMPtr<nsIDOMWindow> win(do_GetInterface(mDocShell));
  nsCOMPtr<nsPIDOMWindow> win_private(do_QueryInterface(win));
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

  nsCOMPtr<nsIAtom> type;
  mOwnerContent->GetTag(*getter_AddRefs(type));

  if (type == nsHTMLAtoms::object) {
    mOwnerContent->GetAttr(kNameSpaceID_None, nsHTMLAtoms::data, aURI);
  } else {
    mOwnerContent->GetAttr(kNameSpaceID_None, nsHTMLAtoms::src, aURI);
  }
}

