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
#include "nsIDOMEventListener.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMWindow.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIWebNavigation.h"
#include "nsIChromeEventHandler.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocShellTreeNode.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIDocShellLoadInfo.h"
#include "nsIBaseWindow.h"
#include "nsIWebShell.h"
#include "nsIWebProgressListener.h"
#include "nsIWebProgress.h"
#include "nsWeakReference.h"

#include "nsIScriptSecurityManager.h"
#include "nsICodebasePrincipal.h"

#include "nsIURI.h"
#include "nsNetUtil.h"

class nsFrameLoader : public nsIFrameLoader,
                      public nsIDOMEventListener,
                      public nsIWebProgressListener,
                      public nsSupportsWeakReference
{
public:
  nsFrameLoader();
  virtual ~nsFrameLoader();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIFrameLoader
  NS_IMETHOD Init(nsIDOMElement *aOwner);
  NS_IMETHOD LoadURI(nsIURI *aURI);
  NS_IMETHOD GetDocShell(nsIDocShell **aDocShell);
  NS_IMETHOD Destroy();

  // nsIDOMEventListener
  NS_DECL_NSIDOMEVENTLISTENER

  // nsIWebProgressListener
  NS_DECL_NSIWEBPROGRESSLISTENER

protected:
  nsresult GetPresContext(nsIPresContext **aPresContext);
  nsresult EnsureDocShell();

  nsCOMPtr<nsIDocShell> mDocShell;

  nsCOMPtr<nsIDOMElement> mOwnerElement;

  nsCOMPtr<nsIURI> mURI;
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
{
  NS_INIT_ISUPPORTS();
}

nsFrameLoader::~nsFrameLoader()
{
  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin(do_QueryInterface(mDocShell));

  if (treeOwnerAsWin) {
    treeOwnerAsWin->Destroy();
  }
}


// QueryInterface implementation for nsFrameLoader
NS_INTERFACE_MAP_BEGIN(nsFrameLoader)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY(nsIFrameLoader)
NS_INTERFACE_MAP_END


NS_IMPL_ADDREF(nsFrameLoader);
NS_IMPL_RELEASE(nsFrameLoader);

NS_IMETHODIMP
nsFrameLoader::Init(nsIDOMElement *aOwner)
{
  mOwnerElement = aOwner;

  return NS_OK;
}

NS_IMETHODIMP
nsFrameLoader::LoadURI(nsIURI *aURI)
{
  mURI = aURI;

  nsresult rv = EnsureDocShell();


  // Prevent recursion



  //    mCreatingViewer=PR_TRUE;


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

    nsCOMPtr<nsIDOMDocument> dom_doc;
    mOwnerElement->GetOwnerDocument(getter_AddRefs(dom_doc));
    nsCOMPtr<nsIDocument> doc(do_QueryInterface(dom_doc));

    if (doc) {
      doc->GetBaseURL(*getter_AddRefs(referrer));
    }
  }

  loadInfo->SetReferrer(referrer);

  // Check if we are allowed to load absURL
  rv = secMan->CheckLoadURI(referrer, mURI,
                            nsIScriptSecurityManager::STANDARD);
  if (NS_FAILED(rv)) {
    return rv; // We're not
  }

  nsCOMPtr<nsIWebProgress> webProgress(do_GetInterface(mDocShell));

  if (webProgress) {
    webProgress->AddProgressListener(this);
  }

  rv = mDocShell->LoadURI(mURI, loadInfo, nsIWebNavigation::LOAD_FLAGS_NONE);
  NS_ASSERTION(NS_SUCCEEDED(rv), "failed to load URL");

  return rv;
}

NS_IMETHODIMP
nsFrameLoader::GetDocShell(nsIDocShell **aDocShell)
{
  EnsureDocShell();

  *aDocShell = mDocShell;
  NS_IF_ADDREF(*aDocShell);

  return NS_OK;
}

NS_IMETHODIMP
nsFrameLoader::Destroy()
{
  return NS_OK;
}

NS_IMETHODIMP
nsFrameLoader::HandleEvent(nsIDOMEvent *aEvent)
{

  return NS_OK;
}

nsresult
nsFrameLoader::GetPresContext(nsIPresContext **aPresContext)
{
  *aPresContext = nsnull;

  nsCOMPtr<nsIDOMDocument> dom_doc;
  mOwnerElement->GetOwnerDocument(getter_AddRefs(dom_doc));

  nsCOMPtr<nsIDocument> doc(do_QueryInterface(dom_doc));

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

#if 0




  // Bug 8065: Don't exceed some maximum depth in content frames
  // (MAX_DEPTH_CONTENT_FRAMES)
  PRInt32 depth = 0;
  nsCOMPtr<nsISupports> parentAsSupports;
  aPresContext->GetContainer(getter_AddRefs(parentAsSupports));

  if (parentAsSupports) {
    nsCOMPtr<nsIDocShellTreeItem> parentAsItem =
      do_QueryInterface(parentAsSupports);

    while (parentAsItem) {
      ++depth;

      if (MAX_DEPTH_CONTENT_FRAMES < depth)
        return NS_ERROR_UNEXPECTED; // Too deep, give up!  (silently?)

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
#endif

  mDocShell = do_CreateInstance("@mozilla.org/webshell;1");
  NS_ENSURE_TRUE(mDocShell, NS_ERROR_FAILURE);

#if 0
  // notify the pres shell that a docshell has been created
  nsCOMPtr<nsIPresShell> presShell;
  aPresContext->GetShell(getter_AddRefs(presShell));

  if (presShell) {
    nsCOMPtr<nsISupports> subShellAsSupports(do_QueryInterface(mDocShell));
    NS_ENSURE_TRUE(subShellAsSupports, NS_ERROR_FAILURE);

    presShell->SetSubShellFor(mContent, subShellAsSupports);

    // We need to be able to get back to the presShell to unset the
    // subshell at destruction
    mPresShellWeak = getter_AddRefs(NS_GetWeakReference(presShell));
  }
#endif

  nsCOMPtr<nsIDocShellTreeItem> docShellAsItem(do_QueryInterface(mDocShell));
  NS_ENSURE_TRUE(docShellAsItem, NS_ERROR_FAILURE);
  nsAutoString frameName;
  mOwnerElement->GetAttribute(NS_LITERAL_STRING("name"), frameName);

  if (!frameName.IsEmpty()) {
    docShellAsItem->SetName(frameName.get());
  }

  // If our container is a web-shell, inform it that it has a new
  // child. If it's not a web-shell then some things will not operate
  // properly.


  nsCOMPtr<nsIPresContext> presContext;
  GetPresContext(getter_AddRefs(presContext));











  // what if !presContext ?









  nsCOMPtr<nsISupports> container;
  presContext->GetContainer(getter_AddRefs(container));

  if (container) {
    nsCOMPtr<nsIDocShellTreeNode> parentAsNode(do_QueryInterface(container));
    if (parentAsNode) {
      nsCOMPtr<nsIDocShellTreeItem> parentAsItem =
        do_QueryInterface(parentAsNode);

      PRInt32 parentType;
      parentAsItem->GetItemType(&parentType);

      nsAutoString value, valuePiece;
      PRBool isContent;

      isContent = PR_FALSE;
      mOwnerElement->GetAttribute(NS_LITERAL_STRING("type"), value);

      if (!value.IsEmpty()) {
        // we accept "content" and "content-xxx" values.
        // at time of writing, we expect "xxx" to be "primary", but
        // someday it might be an integer expressing priority




        // string iterators!


        value.Left(valuePiece, 7);
        if (valuePiece.EqualsIgnoreCase("content") &&
           (value.Length() == 7 ||
            value.Mid(valuePiece, 7, 1) == 1 &&
            valuePiece.EqualsWithConversion("-"))) {
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
          PRBool is_primary = value.EqualsIgnoreCase("content-primary");

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
        chromeEventHandler = do_QueryInterface(mOwnerElement);
        NS_WARN_IF_FALSE(chromeEventHandler,
                         "This mContent should implement this.");
      } else {
        nsCOMPtr<nsIDocShell> parentShell(do_QueryInterface(parentAsNode));

        // Our parent shell is a content shell. Get the chrome info from
        // it and use that for our shell as well.
        parentShell->GetChromeEventHandler(getter_AddRefs(chromeEventHandler));
      }



      // Should this be in the layout code?

      mDocShell->SetChromeEventHandler(chromeEventHandler);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsFrameLoader::OnStateChange(nsIWebProgress *aWebProgress,
                             nsIRequest *aRequest,
                             PRInt32 aStateFlags, PRUint32 aStatus)
{
  if (!((~aStateFlags) & (nsIWebProgressListener::STATE_IS_DOCUMENT |
                          nsIWebProgressListener::STATE_TRANSFERRING))) {
    nsCOMPtr<nsIDOMWindow> win(do_GetInterface(mDocShell));
    nsCOMPtr<nsIDOMEventTarget> eventTarget(do_QueryInterface(win));

    if (eventTarget) {
      eventTarget->AddEventListener(NS_LITERAL_STRING("load"), this,
                                    PR_FALSE);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsFrameLoader::OnProgressChange(nsIWebProgress *aWebProgress,
                                nsIRequest *aRequest,
                                PRInt32 aCurSelfProgress,
                                PRInt32 aMaxSelfProgress,
                                PRInt32 aCurTotalProgress,
                                PRInt32 aMaxTotalProgress)
{
  return NS_OK;
}

NS_IMETHODIMP
nsFrameLoader::OnLocationChange(nsIWebProgress *aWebProgress,
                                nsIRequest *aRequest, nsIURI *location)
{
  return NS_OK;
}

NS_IMETHODIMP
nsFrameLoader::OnStatusChange(nsIWebProgress *aWebProgress,
                              nsIRequest *aRequest, nsresult aStatus,
                              const PRUnichar *aMessage)
{
  return NS_OK;
}

NS_IMETHODIMP
nsFrameLoader::OnSecurityChange(nsIWebProgress *aWebProgress,
                                nsIRequest *aRequest, PRInt32 state)
{
  return NS_OK;
}

