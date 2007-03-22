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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   John Gaunt (jgaunt@netscape.com)
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

// NOTE: alphabetically ordered
#include "nsAccessibilityAtoms.h"
#include "nsAccessibilityService.h"
#include "nsCURILoader.h"
#include "nsDocAccessible.h"
#include "nsHTMLAreaAccessible.h"
#include "nsHTMLFormControlAccessibleWrap.h"
#include "nsHTMLImageAccessible.h"
#include "nsHTMLLinkAccessible.h"
#include "nsHTMLSelectAccessible.h"
#include "nsHTMLTableAccessible.h"
#include "nsHTMLTextAccessible.h"
#include "nsHyperTextAccessible.h"
#include "nsIAccessibilityService.h"
#include "nsIAccessibleProvider.h"
#include "nsIDOMDocument.h"
#include "nsIDOMHTMLAreaElement.h"
#include "nsIDOMHTMLLegendElement.h"
#include "nsIDOMHTMLObjectElement.h"
#include "nsIDOMHTMLOptGroupElement.h"
#include "nsIDOMHTMLOptionElement.h"
#include "nsIDOMWindow.h"
#include "nsIDOMXULElement.h"
#include "nsIDocShell.h"
#include "nsIFrame.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIImageFrame.h"
#include "nsILink.h"
#include "nsINameSpaceManager.h"
#include "nsIObserverService.h"
#include "nsIPluginInstance.h"
#include "nsIPresShell.h"
#include "nsISupportsUtils.h"
#include "nsIWebNavigation.h"
#include "nsObjectFrame.h"
#include "nsOuterDocAccessible.h"
#include "nsRootAccessibleWrap.h"
#include "nsTextFragment.h"
#include "nsPIAccessNode.h"
#include "nsPresContext.h"
#include "nsServiceManagerUtils.h"
#include "nsUnicharUtils.h"
#include "nsIWebProgress.h"
#include "nsNetError.h"
#include "nsDocShellLoadTypes.h"

#ifdef MOZ_XUL
#include "nsXULAlertAccessible.h"
#include "nsXULColorPickerAccessible.h"
#include "nsXULFormControlAccessible.h"
#include "nsXULMenuAccessibleWrap.h"
#include "nsXULSelectAccessible.h"
#include "nsXULTabAccessible.h"
#include "nsXULTextAccessible.h"
#include "nsXULTreeAccessibleWrap.h"
#endif

// For native window support for object/embed/applet tags
#ifdef XP_WIN
#include "nsHTMLWin32ObjectAccessible.h"
#endif

#ifdef MOZ_ACCESSIBILITY_ATK
#include "nsAppRootAccessible.h"
#endif

#ifndef DISABLE_XFORMS_HOOKS
#include "nsXFormsFormControlsAccessible.h"
#include "nsXFormsWidgetsAccessible.h"
#endif

nsAccessibilityService *nsAccessibilityService::gAccessibilityService = nsnull;

/**
  * nsAccessibilityService
  */

nsAccessibilityService::nsAccessibilityService()
{
  nsCOMPtr<nsIObserverService> observerService = 
    do_GetService("@mozilla.org/observer-service;1");
  if (!observerService)
    return;

  observerService->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, PR_FALSE);
  nsCOMPtr<nsIWebProgress> progress(do_GetService(NS_DOCUMENTLOADER_SERVICE_CONTRACTID));
  if (progress) {
    progress->AddProgressListener(NS_STATIC_CAST(nsIWebProgressListener*,this),
                                  nsIWebProgress::NOTIFY_STATE_DOCUMENT |
                                  nsIWebProgress::NOTIFY_LOCATION);
  }
  nsAccessNodeWrap::InitAccessibility();
}

nsAccessibilityService::~nsAccessibilityService()
{
  nsAccessibilityService::gAccessibilityService = nsnull;
  nsAccessNodeWrap::ShutdownAccessibility();
}

NS_IMPL_THREADSAFE_ISUPPORTS5(nsAccessibilityService, nsIAccessibilityService, nsIAccessibleRetrieval,
                              nsIObserver, nsIWebProgressListener, nsISupportsWeakReference)

// nsIObserver

NS_IMETHODIMP
nsAccessibilityService::Observe(nsISupports *aSubject, const char *aTopic,
                         const PRUnichar *aData)
{
  if (!nsCRT::strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID)) {
    nsCOMPtr<nsIObserverService> observerService = 
      do_GetService("@mozilla.org/observer-service;1");
    if (observerService) {
      observerService->RemoveObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID);
    }
    nsCOMPtr<nsIWebProgress> progress(do_GetService(NS_DOCUMENTLOADER_SERVICE_CONTRACTID));
    if (progress) {
      progress->RemoveProgressListener(NS_STATIC_CAST(nsIWebProgressListener*,this));
    }
    nsAccessNodeWrap::ShutdownAccessibility();
  }
  return NS_OK;
}

// nsIWebProgressListener
NS_IMETHODIMP nsAccessibilityService::OnStateChange(nsIWebProgress *aWebProgress,
  nsIRequest *aRequest, PRUint32 aStateFlags, nsresult aStatus)
{
  NS_ASSERTION(aStateFlags & STATE_IS_DOCUMENT, "Other notifications excluded");

  if (0 == (aStateFlags & (STATE_START | STATE_STOP))) {
    return NS_OK;
  }

  nsCOMPtr<nsIDOMWindow> domWindow;
  aWebProgress->GetDOMWindow(getter_AddRefs(domWindow));
  NS_ASSERTION(domWindow, "DOM Window for state change is null");
  NS_ENSURE_TRUE(domWindow, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDOMDocument> domDoc;
  domWindow->GetDocument(getter_AddRefs(domDoc));
  nsCOMPtr<nsIDOMNode> domDocRootNode(do_QueryInterface(domDoc));
  NS_ENSURE_TRUE(domDocRootNode, NS_ERROR_FAILURE);

  // Get the accessible for the new document.
  // If it not created yet this will create it & cache it, as well as 
  // set up event listeners so that MSAA/ATK toolkit and internal 
  // accessibility events will get fired.
  nsCOMPtr<nsIAccessible> accessible;
  GetAccessibleFor(domDocRootNode, getter_AddRefs(accessible));
  nsCOMPtr<nsPIAccessibleDocument> docAccessible =
    do_QueryInterface(accessible);
  NS_ENSURE_TRUE(docAccessible, NS_ERROR_FAILURE);

  PRUint32 eventType = 0;
  if ((aStateFlags & STATE_STOP) && NS_SUCCEEDED(aStatus)) {
    eventType = nsIAccessibleEvent::EVENT_DOCUMENT_LOAD_COMPLETE;
  } else if ((aStateFlags & STATE_STOP) && (aStatus & NS_BINDING_ABORTED)) {
    eventType = nsIAccessibleEvent::EVENT_DOCUMENT_LOAD_STOPPED;
  } else if (aStateFlags & STATE_START) {
    eventType = nsIAccessibleEvent::EVENT_DOCUMENT_LOAD_START;
    nsCOMPtr<nsIWebNavigation> webNav(do_GetInterface(domWindow));
    nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(webNav));
    NS_ENSURE_TRUE(docShell, NS_ERROR_FAILURE);
    PRUint32 loadType;
    docShell->GetLoadType(&loadType);
    if (loadType == LOAD_RELOAD_NORMAL ||
        loadType == LOAD_RELOAD_BYPASS_CACHE ||
        loadType == LOAD_RELOAD_BYPASS_PROXY ||
        loadType == LOAD_RELOAD_BYPASS_PROXY_AND_CACHE) {
      eventType = nsIAccessibleEvent::EVENT_DOCUMENT_RELOAD;
    }
  }
      
  if (eventType == 0)
    return NS_OK; //no actural event need to be fired

  docAccessible->FireDocLoadEvents(eventType);

  return NS_OK;
}

/* void onProgressChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in long aCurSelfProgress, in long aMaxSelfProgress, in long aCurTotalProgress, in long aMaxTotalProgress); */
NS_IMETHODIMP nsAccessibilityService::OnProgressChange(nsIWebProgress *aWebProgress,
  nsIRequest *aRequest, PRInt32 aCurSelfProgress, PRInt32 aMaxSelfProgress,
  PRInt32 aCurTotalProgress, PRInt32 aMaxTotalProgress)
{
  NS_NOTREACHED("notification excluded in AddProgressListener(...)");
  return NS_OK;
}

/* void onLocationChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in nsIURI location); */
NS_IMETHODIMP nsAccessibilityService::OnLocationChange(nsIWebProgress *aWebProgress,
  nsIRequest *aRequest, nsIURI *location)
{
  // If the document is already loaded, this will just check to see
  // if an anchor jump event needs to be fired.
  // If there is no accessible for the document, we will ignore
  // this and the anchor jump event will be fired via OnStateChange
  // and nsIAccessibleStates::STATE_STOP
  nsCOMPtr<nsIDOMWindow> domWindow;
  aWebProgress->GetDOMWindow(getter_AddRefs(domWindow));
  NS_ASSERTION(domWindow, "DOM Window for state change is null");
  NS_ENSURE_TRUE(domWindow, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDOMDocument> domDoc;
  domWindow->GetDocument(getter_AddRefs(domDoc));
  nsCOMPtr<nsIDOMNode> domDocRootNode(do_QueryInterface(domDoc));
  NS_ENSURE_TRUE(domDocRootNode, NS_ERROR_FAILURE);

  nsCOMPtr<nsIAccessibleDocument> accessibleDoc =
    nsAccessNode::GetDocAccessibleFor(domDocRootNode);
  nsCOMPtr<nsPIAccessibleDocument> privateAccessibleDoc =
    do_QueryInterface(accessibleDoc);
  if (!privateAccessibleDoc) {
    return NS_OK;
  }
  return privateAccessibleDoc->FireAnchorJumpEvent();
}

/* void onStatusChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in nsresult aStatus, in wstring aMessage); */
NS_IMETHODIMP nsAccessibilityService::OnStatusChange(nsIWebProgress *aWebProgress,
  nsIRequest *aRequest, nsresult aStatus, const PRUnichar *aMessage)
{
  NS_NOTREACHED("notification excluded in AddProgressListener(...)");
  return NS_OK;
}

/* void onSecurityChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in unsigned long state); */
NS_IMETHODIMP nsAccessibilityService::OnSecurityChange(nsIWebProgress *aWebProgress,
  nsIRequest *aRequest, PRUint32 state)
{
  NS_NOTREACHED("notification excluded in AddProgressListener(...)");
  return NS_OK;
}


nsresult
nsAccessibilityService::GetInfo(nsISupports* aFrame, nsIFrame** aRealFrame, nsIWeakReference** aShell, nsIDOMNode** aNode)
{
  NS_ASSERTION(aFrame,"Error -- 1st argument (aFrame) is null!!");
  *aRealFrame = NS_STATIC_CAST(nsIFrame*, aFrame);
  nsCOMPtr<nsIContent> content = (*aRealFrame)->GetContent();
  nsCOMPtr<nsIDOMNode> node(do_QueryInterface(content));
  if (!content || !node)
    return NS_ERROR_FAILURE;
  *aNode = node;
  NS_IF_ADDREF(*aNode);

  nsCOMPtr<nsIDocument> document = content->GetDocument();
  if (!document)
    return NS_ERROR_FAILURE;

#ifdef DEBUG_A11Y
  PRInt32 shells = document->GetNumberOfShells();
  NS_ASSERTION(shells > 0,"Error no shells!");
#endif

  // do_GetWR only works into a |nsCOMPtr| :-(
  nsCOMPtr<nsIWeakReference> weakShell =
    do_GetWeakReference(document->GetShellAt(0));
  NS_IF_ADDREF(*aShell = weakShell);

  return NS_OK;
}

nsresult
nsAccessibilityService::GetShellFromNode(nsIDOMNode *aNode, nsIWeakReference **aWeakShell)
{
  nsCOMPtr<nsIDOMDocument> domDoc;
  aNode->GetOwnerDocument(getter_AddRefs(domDoc));
  nsCOMPtr<nsIDocument> doc(do_QueryInterface(domDoc));
  if (!doc)
    return NS_ERROR_INVALID_ARG;

  // ---- Get the pres shell ----
  nsIPresShell *shell = doc->GetShellAt(0);
  if (!shell)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIWeakReference> weakRef(do_GetWeakReference(shell));
  
  *aWeakShell = weakRef;
  NS_IF_ADDREF(*aWeakShell);

  return NS_OK;
}

/**
  * nsIAccessibilityService methods:
  */

NS_IMETHODIMP 
nsAccessibilityService::CreateOuterDocAccessible(nsIDOMNode* aDOMNode, 
                                                 nsIAccessible **aOuterDocAccessible)
{
  NS_ENSURE_ARG_POINTER(aDOMNode);
  
  *aOuterDocAccessible = nsnull;

  nsCOMPtr<nsIWeakReference> outerWeakShell;
  GetShellFromNode(aDOMNode, getter_AddRefs(outerWeakShell));
  NS_ENSURE_TRUE(outerWeakShell, NS_ERROR_FAILURE);

  nsOuterDocAccessible *outerDocAccessible =
    new nsOuterDocAccessible(aDOMNode, outerWeakShell);
  NS_ENSURE_TRUE(outerDocAccessible, NS_ERROR_FAILURE);

  NS_ADDREF(*aOuterDocAccessible = outerDocAccessible);

  return NS_OK;
}

NS_IMETHODIMP 
nsAccessibilityService::CreateRootAccessible(nsIPresShell *aShell, 
                                             nsIDocument* aDocument, 
                                             nsIAccessible **aRootAcc)
{
  *aRootAcc = nsnull;

  nsCOMPtr<nsIDOMNode> rootNode(do_QueryInterface(aDocument));
  NS_ENSURE_TRUE(rootNode, NS_ERROR_FAILURE);

  nsIPresShell *presShell = aShell;
  if (!presShell) {
    presShell = aDocument->GetShellAt(0);
  }
  nsCOMPtr<nsIWeakReference> weakShell(do_GetWeakReference(presShell));

  nsCOMPtr<nsISupports> container = aDocument->GetContainer();
  nsCOMPtr<nsIDocShellTreeItem> docShellTreeItem =
    do_QueryInterface(container);
  NS_ENSURE_TRUE(docShellTreeItem, NS_ERROR_FAILURE);
  
  nsCOMPtr<nsIDocShellTreeItem> parentTreeItem;
  docShellTreeItem->GetParent(getter_AddRefs(parentTreeItem));

  if (parentTreeItem) {
    // We only create root accessibles for the true root, othewise create a
    // doc accessible
    *aRootAcc = new nsDocAccessibleWrap(rootNode, weakShell);
  }
  else {
    *aRootAcc = new nsRootAccessibleWrap(rootNode, weakShell);
  }
  if (!*aRootAcc)
    return NS_ERROR_OUT_OF_MEMORY;

  nsCOMPtr<nsPIAccessNode> privateAccessNode(do_QueryInterface(*aRootAcc));
  privateAccessNode->Init();

  NS_ADDREF(*aRootAcc);

  return NS_OK;
}

 /**
   * HTML widget creation
   */
NS_IMETHODIMP
nsAccessibilityService::CreateHTML4ButtonAccessible(nsISupports *aFrame, nsIAccessible **_retval)
{
  nsIFrame* frame;
  nsCOMPtr<nsIDOMNode> node;
  nsCOMPtr<nsIWeakReference> weakShell;
  nsresult rv = GetInfo(aFrame, &frame, getter_AddRefs(weakShell), getter_AddRefs(node));
  if (NS_FAILED(rv))
    return rv;

  *_retval = new nsHTML4ButtonAccessible(node, weakShell);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
  return NS_OK;
}

NS_IMETHODIMP
nsAccessibilityService::CreateHTMLAreaAccessible(nsIWeakReference *aShell, nsIDOMNode *aDOMNode, nsIAccessible *aParent, 
                                                               nsIAccessible **_retval)
{
  *_retval = new nsHTMLAreaAccessible(aDOMNode, aParent, aShell);

  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
  return NS_OK;
}

NS_IMETHODIMP
nsAccessibilityService::CreateHTMLButtonAccessible(nsISupports *aFrame, nsIAccessible **_retval)
{
  nsIFrame* frame;
  nsCOMPtr<nsIDOMNode> node;
  nsCOMPtr<nsIWeakReference> weakShell;
  nsresult rv = GetInfo(aFrame, &frame, getter_AddRefs(weakShell), getter_AddRefs(node));
  if (NS_FAILED(rv))
    return rv;

  *_retval = new nsHTMLButtonAccessible(node, weakShell);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
  return NS_OK;
}

nsresult
nsAccessibilityService::CreateHTMLAccessibleByMarkup(nsIFrame *aFrame,
                                                     nsIWeakReference *aWeakShell,
                                                     nsIDOMNode *aNode,
                                                     const nsAString& aRole,
                                                     nsIAccessible **aAccessible)
{
  // This method assumes we're in an HTML namespace.
  *aAccessible = nsnull;
  nsCOMPtr<nsIContent> content(do_QueryInterface(aNode));
  nsIAtom *tag = content->Tag();
  if (tag == nsAccessibilityAtoms::option) {
    *aAccessible = new nsHTMLSelectOptionAccessible(aNode, aWeakShell);
  }
  else if (tag == nsAccessibilityAtoms::optgroup) {
    *aAccessible = new nsHTMLSelectOptGroupAccessible(aNode, aWeakShell);
  }
  else if (tag == nsAccessibilityAtoms::ul || tag == nsAccessibilityAtoms::ol) {
    *aAccessible = new nsHTMLListAccessible(aNode, aWeakShell);
  }
  else if (tag == nsAccessibilityAtoms::a) {
    *aAccessible = new nsHTMLLinkAccessible(aNode, aWeakShell, aFrame);
  }
  else if (tag == nsAccessibilityAtoms::li && aFrame->GetType() != nsAccessibilityAtoms::blockFrame) {
    // Normally this is created by the list item frame which knows about the bullet frame
    // However, in this case the list item must have been styled using display: foo
    *aAccessible = new nsHTMLLIAccessible(aNode, aWeakShell, nsnull, EmptyString());
  }
  else if (tag == nsAccessibilityAtoms::abbr ||
           tag == nsAccessibilityAtoms::acronym ||
           tag == nsAccessibilityAtoms::blockquote ||
           tag == nsAccessibilityAtoms::caption ||
           tag == nsAccessibilityAtoms::dd ||
           tag == nsAccessibilityAtoms::dl ||
           tag == nsAccessibilityAtoms::dt ||
           tag == nsAccessibilityAtoms::form ||
           tag == nsAccessibilityAtoms::h1 ||
           tag == nsAccessibilityAtoms::h2 ||
           tag == nsAccessibilityAtoms::h3 ||
           tag == nsAccessibilityAtoms::h4 ||
           tag == nsAccessibilityAtoms::h5 ||
           tag == nsAccessibilityAtoms::h6 ||
#ifndef MOZ_ACCESSIBILITY_ATK
           tag == nsAccessibilityAtoms::tbody ||
           tag == nsAccessibilityAtoms::tfoot ||
           tag == nsAccessibilityAtoms::thead ||
#endif
           tag == nsAccessibilityAtoms::q) {
    return CreateHyperTextAccessible(aFrame, aAccessible);
  }
  NS_IF_ADDREF(*aAccessible);
  return NS_OK;
}

NS_IMETHODIMP
nsAccessibilityService::CreateHTMLLIAccessible(nsISupports *aFrame, 
                                               nsISupports *aBulletFrame,
                                               const nsAString& aBulletText,
                                               nsIAccessible **_retval)
{
  nsIFrame* frame;
  nsCOMPtr<nsIDOMNode> node;
  nsCOMPtr<nsIWeakReference> weakShell;
  nsresult rv = GetInfo(aFrame, &frame, getter_AddRefs(weakShell), getter_AddRefs(node));
  if (NS_FAILED(rv))
    return rv;
  nsIFrame *bulletFrame = NS_STATIC_CAST(nsIFrame*, aBulletFrame);
  NS_ASSERTION(bulletFrame, "bullet frame argument not a frame");

  *_retval = new nsHTMLLIAccessible(node, weakShell, bulletFrame, aBulletText);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
  return NS_OK;
}

NS_IMETHODIMP
nsAccessibilityService::CreateHyperTextAccessible(nsISupports *aFrame, nsIAccessible **aAccessible)
{
  nsIFrame* frame;
  nsCOMPtr<nsIDOMNode> node;
  nsCOMPtr<nsIWeakReference> weakShell;
  nsresult rv = GetInfo(aFrame, &frame, getter_AddRefs(weakShell), getter_AddRefs(node));
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIContent> content(do_QueryInterface(node));
  NS_ENSURE_TRUE(content, NS_ERROR_FAILURE);
  if (content->HasAttr(kNameSpaceID_None, nsAccessibilityAtoms::onclick)) {
    // nsLinkableAccessible inherits from nsHyperTextAccessible, but
    // it also includes code for dealing with the onclick
    *aAccessible = new nsLinkableAccessible(node, weakShell);
  }
  else {
    *aAccessible = new nsHyperTextAccessible(node, weakShell);
  }
  if (nsnull == *aAccessible)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*aAccessible);
  return NS_OK;
}

NS_IMETHODIMP
nsAccessibilityService::CreateHTMLCheckboxAccessible(nsISupports *aFrame, nsIAccessible **_retval)
{
  nsIFrame* frame;
  nsCOMPtr<nsIDOMNode> node;
  nsCOMPtr<nsIWeakReference> weakShell;
  nsresult rv = GetInfo(aFrame, &frame, getter_AddRefs(weakShell), getter_AddRefs(node));
  if (NS_FAILED(rv))
    return rv;

  *_retval = new nsHTMLCheckboxAccessible(node, weakShell);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
  return NS_OK;
}

NS_IMETHODIMP 
nsAccessibilityService::CreateHTMLComboboxAccessible(nsIDOMNode* aDOMNode, nsIWeakReference* aPresShell, nsIAccessible **_retval)
{
  *_retval = new nsHTMLComboboxAccessible(aDOMNode, aPresShell);
  if (! *_retval)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
  return NS_OK;
}

NS_IMETHODIMP
nsAccessibilityService::CreateHTMLImageAccessible(nsISupports *aFrame, nsIAccessible **_retval)
{
  nsIFrame* frame;
  nsCOMPtr<nsIDOMNode> node;
  nsCOMPtr<nsIWeakReference> weakShell;
  nsresult rv = GetInfo(aFrame, &frame, getter_AddRefs(weakShell), getter_AddRefs(node));
  if (NS_FAILED(rv))
    return rv;

  *_retval = nsnull;
  nsCOMPtr<nsIDOMElement> domElement(do_QueryInterface(node));
  if (domElement) {
      *_retval = new nsHTMLImageAccessible(node, weakShell);
  }

  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
  return NS_OK;
}

NS_IMETHODIMP
nsAccessibilityService::CreateHTMLGenericAccessible(nsISupports *aFrame, nsIAccessible **_retval)
{
  nsIFrame* frame;
  nsCOMPtr<nsIDOMNode> node;
  nsCOMPtr<nsIWeakReference> weakShell;
  nsresult rv = GetInfo(aFrame, &frame, getter_AddRefs(weakShell), getter_AddRefs(node));
  if (NS_FAILED(rv))
    return rv;

  *_retval = new nsAccessibleWrap(node, weakShell);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
  return NS_OK;
}

NS_IMETHODIMP
nsAccessibilityService::CreateHTMLGroupboxAccessible(nsISupports *aFrame, nsIAccessible **_retval)
{
  nsIFrame* frame;
  nsCOMPtr<nsIDOMNode> node;
  nsCOMPtr<nsIWeakReference> weakShell;
  nsresult rv = GetInfo(aFrame, &frame, getter_AddRefs(weakShell), getter_AddRefs(node));
  if (NS_FAILED(rv))
    return rv;

  *_retval = new nsHTMLGroupboxAccessible(node, weakShell);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
  return NS_OK;
}

NS_IMETHODIMP 
nsAccessibilityService::CreateHTMLListboxAccessible(nsIDOMNode* aDOMNode, nsIWeakReference* aPresShell, nsIAccessible **_retval)
{
  *_retval = new nsHTMLSelectListAccessible(aDOMNode, aPresShell);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
  return NS_OK;
}

/**
  * We can have several cases here. 
  *  1) a text or html embedded document where the contentDocument
  *     variable in the object element holds the content
  *  2) web content that uses a plugin, which means we will
  *     have to go to the plugin to get the accessible content
  *  3) An image or imagemap, where the image frame points back to 
  *     the object element DOMNode
  */
NS_IMETHODIMP
nsAccessibilityService::CreateHTMLObjectFrameAccessible(nsObjectFrame *aFrame,
                                                        nsIAccessible **aAccessible)
{
  nsCOMPtr<nsIDOMNode> node;
  nsCOMPtr<nsIWeakReference> weakShell;
  nsIFrame *frame;
  GetInfo(NS_STATIC_CAST(nsIFrame*, aFrame), &frame, getter_AddRefs(weakShell), getter_AddRefs(node));

  *aAccessible = nsnull;
  if (!frame || frame->GetRect().IsEmpty()) {
    return NS_ERROR_FAILURE;
  }
  // 1) for object elements containing either HTML or TXT documents
  nsCOMPtr<nsIDOMDocument> domDoc;
  nsCOMPtr<nsIDOMHTMLObjectElement> obj(do_QueryInterface(node));
  if (obj)
    obj->GetContentDocument(getter_AddRefs(domDoc));
  else
    domDoc = do_QueryInterface(node);
  if (domDoc)
    return CreateOuterDocAccessible(node, aAccessible);

#ifdef XP_WIN
  // 2) for plugins
  nsCOMPtr<nsIPluginInstance> pluginInstance ;
  aFrame->GetPluginInstance(*getter_AddRefs(pluginInstance));
  if (pluginInstance) {
    HWND pluginPort = nsnull;
    aFrame->GetPluginPort(&pluginPort);
    if (pluginPort) {
      *aAccessible = new nsHTMLWin32ObjectOwnerAccessible(node, weakShell, pluginPort);
      if (*aAccessible) {
        NS_ADDREF(*aAccessible);
        return NS_OK;
      }
    }
  }
#endif

  // 3) for images and imagemaps, or anything else with a child frame
  // we have the object frame, get the image frame
  frame = aFrame->GetFirstChild(nsnull);
  if (frame)
    return frame->GetAccessible(aAccessible);

  return NS_OK;
}

NS_IMETHODIMP
nsAccessibilityService::CreateHTMLRadioButtonAccessible(nsISupports *aFrame, nsIAccessible **_retval)
{
  nsIFrame* frame;
  nsCOMPtr<nsIDOMNode> node;
  nsCOMPtr<nsIWeakReference> weakShell;
  nsresult rv = GetInfo(aFrame, &frame, getter_AddRefs(weakShell), getter_AddRefs(node));
  if (NS_FAILED(rv))
    return rv;

  *_retval = new nsHTMLRadioButtonAccessibleWrap(node, weakShell);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
  return NS_OK;
}

NS_IMETHODIMP 
nsAccessibilityService::CreateHTMLSelectOptionAccessible(nsIDOMNode* aDOMNode, 
                                                         nsIAccessible *aParent, 
                                                         nsIWeakReference* aPresShell, 
                                                         nsIAccessible **_retval)
{
  *_retval = new nsHTMLSelectOptionAccessible(aDOMNode, aPresShell);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
  return NS_OK;
}

NS_IMETHODIMP
nsAccessibilityService::CreateHTMLTableAccessible(nsISupports *aFrame, nsIAccessible **_retval)
{
  nsIFrame* frame;
  nsCOMPtr<nsIDOMNode> node;
  nsCOMPtr<nsIWeakReference> weakShell;
  nsresult rv = GetInfo(aFrame, &frame, getter_AddRefs(weakShell), getter_AddRefs(node));
  if (NS_FAILED(rv))
    return rv;

  *_retval = new nsHTMLTableAccessible(node, weakShell);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
  return NS_OK;
}

NS_IMETHODIMP
nsAccessibilityService::CreateHTMLTableHeadAccessible(nsIDOMNode *aDOMNode, nsIAccessible **_retval)
{
#ifndef MOZ_ACCESSIBILITY_ATK
  *_retval = nsnull;
  return NS_ERROR_FAILURE;
#else
  NS_ENSURE_ARG_POINTER(aDOMNode);

  nsresult rv = NS_OK;

  nsCOMPtr<nsIWeakReference> weakShell;
  rv = GetShellFromNode(aDOMNode, getter_AddRefs(weakShell));
  NS_ENSURE_SUCCESS(rv, rv);

  nsHTMLTableHeadAccessible* accTableHead =
    new nsHTMLTableHeadAccessible(aDOMNode, weakShell);

  NS_ENSURE_TRUE(accTableHead, NS_ERROR_OUT_OF_MEMORY);

  *_retval = NS_STATIC_CAST(nsIAccessible *, accTableHead);
  NS_IF_ADDREF(*_retval);

  return rv;
#endif
}

NS_IMETHODIMP
nsAccessibilityService::CreateHTMLTableCellAccessible(nsISupports *aFrame, nsIAccessible **_retval)
{
  nsIFrame* frame;
  nsCOMPtr<nsIDOMNode> node;
  nsCOMPtr<nsIWeakReference> weakShell;
  nsresult rv = GetInfo(aFrame, &frame, getter_AddRefs(weakShell), getter_AddRefs(node));
  if (NS_FAILED(rv))
    return rv;

  *_retval = new nsHTMLTableCellAccessible(node, weakShell);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
  return NS_OK;
}

NS_IMETHODIMP
nsAccessibilityService::CreateHTMLTextAccessible(nsISupports *aFrame, nsIAccessible **_retval)
{
  *_retval = nsnull;

  nsIFrame* frame;
  nsCOMPtr<nsIDOMNode> node;
  nsCOMPtr<nsIWeakReference> weakShell;
  nsresult rv = GetInfo(aFrame, &frame, getter_AddRefs(weakShell), getter_AddRefs(node));
  if (NS_FAILED(rv))
    return rv;

  // XXX Don't create ATK objects for these
  *_retval = new nsHTMLTextAccessible(node, weakShell, frame);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
  return NS_OK;
}

NS_IMETHODIMP
nsAccessibilityService::CreateHTMLTextFieldAccessible(nsISupports *aFrame, nsIAccessible **_retval)
{
  nsIFrame* frame;
  nsCOMPtr<nsIDOMNode> node;
  nsCOMPtr<nsIWeakReference> weakShell;
  nsresult rv = GetInfo(aFrame, &frame, getter_AddRefs(weakShell), getter_AddRefs(node));
  if (NS_FAILED(rv))
    return rv;

  *_retval = new nsHTMLTextFieldAccessible(node, weakShell);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
  return NS_OK;
}

NS_IMETHODIMP
nsAccessibilityService::CreateHTMLLabelAccessible(nsISupports *aFrame, nsIAccessible **_retval)
{
  nsIFrame* frame;
  nsCOMPtr<nsIDOMNode> node;
  nsCOMPtr<nsIWeakReference> weakShell;
  nsresult rv = GetInfo(aFrame, &frame, getter_AddRefs(weakShell), getter_AddRefs(node));
  if (NS_FAILED(rv))
    return rv;

  *_retval = new nsHTMLLabelAccessible(node, weakShell);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
  return NS_OK;
}

NS_IMETHODIMP
nsAccessibilityService::CreateHTMLHRAccessible(nsISupports *aFrame, nsIAccessible **_retval)
{
  nsIFrame* frame;
  nsCOMPtr<nsIDOMNode> node;
  nsCOMPtr<nsIWeakReference> weakShell;
  nsresult rv = GetInfo(aFrame, &frame, getter_AddRefs(weakShell), getter_AddRefs(node));
  if (NS_FAILED(rv))
    return rv;

  *_retval = new nsHTMLHRAccessible(node, weakShell);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
  return NS_OK;
}

NS_IMETHODIMP
nsAccessibilityService::CreateHTMLBRAccessible(nsISupports *aFrame, nsIAccessible **_retval)
{
  nsIFrame* frame;
  nsCOMPtr<nsIDOMNode> node;
  nsCOMPtr<nsIWeakReference> weakShell;
  nsresult rv = GetInfo(aFrame, &frame, getter_AddRefs(weakShell), getter_AddRefs(node));
  if (NS_FAILED(rv))
    return rv;

  *_retval = new nsHTMLBRAccessible(node, weakShell);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
  return NS_OK;
}

NS_IMETHODIMP nsAccessibilityService::GetCachedAccessible(nsIDOMNode *aNode, 
                                                          nsIWeakReference *aWeakShell,
                                                          nsIAccessible **aAccessible)
{
  nsCOMPtr<nsIAccessNode> accessNode;
  nsresult rv = GetCachedAccessNode(aNode, aWeakShell, getter_AddRefs(accessNode));
  nsCOMPtr<nsIAccessible> accessible(do_QueryInterface(accessNode));
  NS_IF_ADDREF(*aAccessible = accessible);
  return rv;
}

NS_IMETHODIMP nsAccessibilityService::GetCachedAccessNode(nsIDOMNode *aNode, 
                                                          nsIWeakReference *aWeakShell,
                                                          nsIAccessNode **aAccessNode)
{
  nsCOMPtr<nsIAccessibleDocument> accessibleDoc =
    nsAccessNode::GetDocAccessibleFor(aWeakShell);

  if (!accessibleDoc) {
    *aAccessNode = nsnull;
    return NS_ERROR_FAILURE;
  }

  return accessibleDoc->GetCachedAccessNode(NS_STATIC_CAST(void*, aNode), aAccessNode);
}

/**
  * GetAccessibleFor - get an nsIAccessible from a DOM node
  */

NS_IMETHODIMP
nsAccessibilityService::GetAccessibleFor(nsIDOMNode *aNode,
                                         nsIAccessible **aAccessible)
{
  // We use presentation shell #0 because we assume that is presentation of
  // given node window.
  nsCOMPtr<nsIContent> content(do_QueryInterface(aNode));
  nsCOMPtr<nsIDocument> doc;
  if (content) {
    doc = content->GetDocument();
  }
  else {// Could be document node
    doc = do_QueryInterface(aNode);
  }
  if (!doc)
    return NS_ERROR_FAILURE;

  nsIPresShell *presShell = doc->GetShellAt(0);
  return GetAccessibleInShell(aNode, presShell, aAccessible);
}

NS_IMETHODIMP
nsAccessibilityService::GetAttachedAccessibleFor(nsIDOMNode *aNode,
                                                 nsIAccessible **aAccessible)
{
  NS_ENSURE_ARG(aNode);
  NS_ENSURE_ARG_POINTER(aAccessible);

  *aAccessible = nsnull;

  nsCOMPtr<nsIDOMNode> relevantNode;
  nsresult rv = GetRelevantContentNodeFor(aNode, getter_AddRefs(relevantNode));
  NS_ENSURE_SUCCESS(rv, rv);

  if (relevantNode != aNode)
    return NS_OK;

  return GetAccessibleFor(aNode, aAccessible);
}

NS_IMETHODIMP nsAccessibilityService::GetAccessibleInWindow(nsIDOMNode *aNode, 
                                                            nsIDOMWindow *aWin,
                                                            nsIAccessible **aAccessible)
{
  nsCOMPtr<nsIWebNavigation> webNav(do_GetInterface(aWin));
  nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(webNav));
  if (!docShell)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIPresShell> presShell;
  docShell->GetPresShell(getter_AddRefs(presShell));
  return GetAccessibleInShell(aNode, presShell, aAccessible);
}

NS_IMETHODIMP nsAccessibilityService::GetAccessibleInShell(nsIDOMNode *aNode, 
                                                           nsIPresShell *aPresShell,
                                                           nsIAccessible **aAccessible) 
{
  nsCOMPtr<nsIWeakReference> weakShell(do_GetWeakReference(aPresShell));
  nsIFrame *outFrameUnused = NULL;
  PRBool isHiddenUnused = false;
  return GetAccessible(aNode, aPresShell, weakShell, 
                       &outFrameUnused, &isHiddenUnused, aAccessible);
}

NS_IMETHODIMP nsAccessibilityService::GetAccessibleInWeakShell(nsIDOMNode *aNode, 
                                                               nsIWeakReference *aWeakShell,
                                                               nsIAccessible **aAccessible) 
{
  nsCOMPtr<nsIPresShell> presShell(do_QueryReferent(aWeakShell));
  nsIFrame *outFrameUnused = NULL;
  PRBool isHiddenUnused = false;
  return GetAccessible(aNode, presShell, aWeakShell, 
                       &outFrameUnused, &isHiddenUnused, aAccessible);
}

nsresult nsAccessibilityService::InitAccessible(nsIAccessible *aAccessibleIn,
                                                nsIAccessible **aAccessibleOut)
{
  NS_ENSURE_TRUE(aAccessibleIn, NS_ERROR_FAILURE);
  NS_ASSERTION(aAccessibleOut && !*aAccessibleOut, "Out param should already be cleared out");

  nsCOMPtr<nsPIAccessNode> privateAccessNode = do_QueryInterface(aAccessibleIn);
  NS_ASSERTION(privateAccessNode, "All accessibles must support nsPIAccessNode");
  nsresult rv = privateAccessNode->Init(); // Add to cache, etc.
  if (NS_SUCCEEDED(rv)) {
    NS_ADDREF(*aAccessibleOut = aAccessibleIn);
  }
  return rv;
}

NS_IMETHODIMP nsAccessibilityService::GetAccessible(nsIDOMNode *aNode,
                                                    nsIPresShell *aPresShell,
                                                    nsIWeakReference *aWeakShell,
                                                    nsIFrame **aFrameHint,
                                                    PRBool *aIsHidden,
                                                    nsIAccessible **aAccessible)
{
  NS_ENSURE_ARG_POINTER(aAccessible);
  NS_ENSURE_ARG_POINTER(aFrameHint);
  *aAccessible = nsnull;
  if (!aPresShell || !aWeakShell) {
    return NS_ERROR_FAILURE;
  }

  NS_ASSERTION(aNode, "GetAccessible() called with no node.");

  *aIsHidden = PR_FALSE;

#ifdef DEBUG_A11Y
  // Please leave this in for now, it's a convenient debugging method
  nsAutoString name;
  aNode->GetLocalName(name);
  if (name.LowerCaseEqualsLiteral("h1")) 
    printf("## aaronl debugging tag name\n");

  nsAutoString attrib;
  nsCOMPtr<nsIDOMElement> element(do_QueryInterface(aNode));
  if (element) {
    element->GetAttribute(NS_LITERAL_STRING("type"), attrib);
    if (attrib.EqualsLiteral("statusbarpanel"))
      printf("## aaronl debugging attribute\n");
  }
#endif

  nsCOMPtr<nsIContent> content(do_QueryInterface(aNode));
  if (content && content->Tag() == nsAccessibilityAtoms::map) {
    // Don't walk into maps, they take up no space.
    // The nsHTMLAreaAccessible's they contain are attached as
    // children of the appropriate nsHTMLImageAccessible.
    *aIsHidden = PR_TRUE;
    return NS_OK;
  }

  // Check to see if we already have an accessible for this
  // node in the cache
  nsCOMPtr<nsIAccessNode> accessNode;
  GetCachedAccessNode(aNode, aWeakShell, getter_AddRefs(accessNode));

  nsCOMPtr<nsIAccessible> newAcc;
  if (accessNode) {
    // Retrieved from cache
    // QI might not succeed if it's a node that's not accessible
    newAcc = do_QueryInterface(accessNode);
    NS_IF_ADDREF(*aAccessible = newAcc);
    return NS_OK;
  }

  // No cache entry, so we must create the accessible
  // Check to see if hidden first
  nsCOMPtr<nsIDocument> nodeIsDoc;
  if (!content) {
    // This happens when we're on the document node, which will not QI to an
    // nsIContent.
    nodeIsDoc = do_QueryInterface(aNode);
    NS_ENSURE_TRUE(nodeIsDoc, NS_ERROR_FAILURE); // No content, and not doc node

    nsCOMPtr<nsIAccessibleDocument> accessibleDoc =
      nsAccessNode::GetDocAccessibleFor(aWeakShell);
    if (accessibleDoc) {
      newAcc = do_QueryInterface(accessibleDoc);
      NS_ASSERTION(newAcc, "nsIAccessibleDocument is not an nsIAccessible");
    }
    else {
      CreateRootAccessible(aPresShell, nodeIsDoc, getter_AddRefs(newAcc)); // Does Init() for us
      NS_WARN_IF_FALSE(newAcc, "No root/doc accessible created");
    }

    *aFrameHint = aPresShell->GetRootFrame();
    NS_IF_ADDREF(*aAccessible = newAcc);
    return NS_OK;
  }

  NS_ASSERTION(content->GetDocument(), "Creating accessible for node with no document");
  if (!content->GetDocument()) {
    return NS_ERROR_FAILURE;
  }
  NS_ASSERTION(content->GetDocument() == aPresShell->GetDocument(), "Creating accessible for wrong pres shell");
  if (content->GetDocument() != aPresShell->GetDocument()) {
    return NS_ERROR_FAILURE;
  }

  // We have a content node
  nsIFrame *frame = *aFrameHint;
#ifdef DEBUG_A11Y
  static int frameHintFailed, frameHintTried, frameHintNonexistant, frameHintFailedForText;
  ++frameHintTried;
#endif
  if (!frame || content != frame->GetContent()) {
    // Frame hint not correct, get true frame, we try to optimize away from this
    frame = aPresShell->GetPrimaryFrameFor(content);
    if (frame) {
#ifdef DEBUG_A11Y_FRAME_OPTIMIZATION
      // Frame hint debugging
      ++frameHintFailed;
      if (content->IsNodeOfType(nsINode::eTEXT)) {
        ++frameHintFailedForText;
      }
      frameHintNonexistant += !*aFrameHint;
      printf("Frame hint failures: %d / %d . Text fails = %d. No hint fails = %d \n", frameHintFailed, frameHintTried, frameHintFailedForText, frameHintNonexistant);
      if (frameHintTried >= 354) {
        printf("* "); // Aaron's break point
      }
#endif
      if (frame->GetContent() != content) {
        // Not the main content for this frame!
        // For example, this happens because <area> elements return the
        // image frame as their primary frame. The main content for the 
        // image frame is the image content.

        // Check if frame is an image frame, and content is <area>
        nsIImageFrame *imageFrame;
        CallQueryInterface(frame, &imageFrame);
        nsCOMPtr<nsIDOMHTMLAreaElement> areaElmt = do_QueryInterface(content);
        if (imageFrame && areaElmt) {
          nsCOMPtr<nsIAccessible> imageAcc;
          CreateHTMLImageAccessible(frame, getter_AddRefs(imageAcc));
          if (imageAcc) {
            // cache children
            PRInt32 childCount;
            imageAcc->GetChildCount(&childCount);
            // area accessible should be in cache now
            return GetCachedAccessible(aNode, aWeakShell, aAccessible);
          }
        }

        return NS_OK;
      }
      *aFrameHint = frame;
    }
  }

  // Check frame to see if it is hidden
  if (!frame || !frame->GetStyleVisibility()->IsVisible()) {
    *aIsHidden = PR_TRUE;
  }

  if (*aIsHidden)
    return NS_OK;

  /**
   * Attempt to create an accessible based on what we know
   */
  if (content->IsNodeOfType(nsINode::eTEXT)) {
    // --- Create HTML for visible text frames ---
    if (frame->IsEmpty()) {
      *aIsHidden = PR_TRUE;
      return NS_OK;
    }
    frame->GetAccessible(getter_AddRefs(newAcc));
    return InitAccessible(newAcc, aAccessible);
  }

  nsAutoString role;
  if (nsAccessNode::GetRoleAttribute(content, role) &&
      StringEndsWith(role, NS_LITERAL_STRING(":presentation")) &&
      !content->IsFocusable()) {
    // Only create accessible for role=":presentation" if it is focusable --
    // in that case we need an accessible in case it gets focused, we
    // don't want focus ever to be 'lost'
    return NS_OK;
  }

  // Elements may implement nsIAccessibleProvider via XBL. This allows them to
  // say what kind of accessible to create.
  nsresult rv = GetAccessibleByType(aNode, getter_AddRefs(newAcc));
  NS_ENSURE_SUCCESS(rv, rv);

  if (!newAcc && !content->IsNodeOfType(nsINode::eHTML)) {
    if (content->GetNameSpaceID() == kNameSpaceID_SVG &&
             content->Tag() == nsAccessibilityAtoms::svg) {
      newAcc = new nsEnumRoleAccessible(aNode, aWeakShell,
                                        nsIAccessibleRole::ROLE_DIAGRAM);
    }
    else if (content->GetNameSpaceID() == kNameSpaceID_MathML &&
             content->Tag() == nsAccessibilityAtoms::math) {
      newAcc = new nsEnumRoleAccessible(aNode, aWeakShell,
                                        nsIAccessibleRole::ROLE_EQUATION);
    }
  } else if (!newAcc) {  // HTML accessibles
    // Prefer to use markup (mostly tag name, perhaps attributes) to
    // decide if and what kind of accessible to create.
    CreateHTMLAccessibleByMarkup(frame, aWeakShell, aNode, role, getter_AddRefs(newAcc));

    PRBool tryFrame = (newAcc == nsnull);
    if (!content->IsFocusable()) { 
      // If we're in unfocusable table-related subcontent, check for the
      // Presentation role on the containing table
      nsIAtom *tag = content->Tag();
      if (tag == nsAccessibilityAtoms::td ||
          tag == nsAccessibilityAtoms::th ||
          tag == nsAccessibilityAtoms::tr ||
          tag == nsAccessibilityAtoms::tbody ||
          tag == nsAccessibilityAtoms::tfoot ||
          tag == nsAccessibilityAtoms::thead) {
        nsIContent *tableContent = content;
        nsAutoString tableRole;
        while ((tableContent = tableContent->GetParent()) != nsnull) {
          if (tableContent->Tag() == nsAccessibilityAtoms::table) {
            nsIFrame *tableFrame = aPresShell->GetPrimaryFrameFor(tableContent);
            if (!tableFrame || tableFrame->GetType() != nsAccessibilityAtoms::tableOuterFrame ||
                nsAccessNode::HasRoleAttribute(tableContent)) {
              // Table that we're a descendant of is not styled as a table,
              // and has no table accessible for an ancestor, or
              // table that we're a descendant of is presentational
              tryFrame = PR_FALSE;
            }

            break;
          }
        }
      }
    }

    if (tryFrame) {
      frame->GetAccessible(getter_AddRefs(newAcc)); // Try using frame to do it
    }
  }

  // If no accessible, see if we need to create a generic accessible because
  // of some property that makes this object interesting
  // We don't do this for <body>, <html>, <window>, <dialog> etc. which 
  // correspond to the doc accessible and will be created in any case
  if (!newAcc && content->Tag() != nsAccessibilityAtoms::body && content->GetParent() && 
      (content->IsFocusable() ||
       content->HasAttr(kNameSpaceID_None, nsAccessibilityAtoms::onclick) ||
       content->HasAttr(kNameSpaceID_WAIProperties, nsAccessibilityAtoms::describedby) ||
       content->HasAttr(kNameSpaceID_WAIProperties, nsAccessibilityAtoms::labelledby) ||
       content->HasAttr(kNameSpaceID_WAIProperties, nsAccessibilityAtoms::required) ||
       content->HasAttr(kNameSpaceID_WAIProperties, nsAccessibilityAtoms::invalid) ||
       !role.IsEmpty())) {
    // This content is focusable or has an interesting dynamic content accessibility property.
    // If it's interesting we need it in the accessibility hierarchy so that events or
    // other accessibles can point to it, or so that it can hold a state, etc.
    if (content->IsNodeOfType(nsINode::eHTML)) {
      // Interesting HTML container which may have selectable text and/or embedded objects
      CreateHyperTextAccessible(frame, getter_AddRefs(newAcc));
    }
    else {  // XUL, SVG, MathML etc.
      // Interesting generic non-HTML container
      newAcc = new nsAccessibleWrap(aNode, aWeakShell);
    }
  }

  return InitAccessible(newAcc, aAccessible);
}

NS_IMETHODIMP
nsAccessibilityService::GetRelevantContentNodeFor(nsIDOMNode *aNode,
                                                  nsIDOMNode **aRelevantNode)
{
  // The method returns node that is relevant for attached accessible check.
  // Sometimes element that is XBL widget hasn't accessible children in
  // anonymous content. This method check whether given node can be accessible
  // by looking through all nested bindings that given node is anonymous for. If
  // there is XBL widget that deniedes to be accessible for given node then the
  // method returns that XBL widget otherwise it returns given node.

  // For example, the algorithm allows us to handle following cases:
  // 1. xul:dialog element has buttons (like 'ok' and 'cancel') in anonymous
  // content. When node is dialog's button then we dialog's button since button
  // of xul:dialog is accessible anonymous node.
  // 2. xul:texbox has html:input in anonymous content. When given node is
  // html:input elmement then we return xul:textbox since xul:textbox doesn't
  // allow accessible nodes in anonymous content.
  // 3. xforms:input that is hosted in xul document contains xul:textbox
  // element. When given node is html:input or xul:textbox then we return
  // xforms:input element since xforms:input hasn't accessible anonymous
  // children.

  NS_ENSURE_ARG(aNode);
  NS_ENSURE_ARG_POINTER(aRelevantNode);

  nsresult rv;
  nsCOMPtr<nsIContent> content(do_QueryInterface(aNode));
  if (content) {
    // Build stack of binding parents so we can walk it in reverse.
    nsIContent *bindingParent;
    nsCOMArray<nsIContent> bindingsStack;

    for (bindingParent = content->GetBindingParent(); bindingParent != nsnull &&
         bindingParent != bindingParent->GetBindingParent();
         bindingParent = bindingParent->GetBindingParent()) {
      bindingsStack.AppendObject(bindingParent);
    }

    PRInt32 bindingsCount = bindingsStack.Count();
    for (PRInt32 index = bindingsCount - 1; index >= 0 ; index--) {
      bindingParent = bindingsStack[index];
      nsCOMPtr<nsIDOMNode> bindingNode(do_QueryInterface(bindingParent));
      if (bindingNode) {
        // Try to get an accessible by type since XBL widget can be accessible
        // only if it implements nsIAccessibleProvider interface.
        nsCOMPtr<nsIAccessible> accessible;
        rv = GetAccessibleByType(bindingNode, getter_AddRefs(accessible));

        if (NS_SUCCEEDED(rv)) {
          nsCOMPtr<nsPIAccessible> paccessible(do_QueryInterface(accessible));
          if (paccessible) {
            PRBool allowsAnonChildren = PR_FALSE;
            paccessible->GetAllowsAnonChildAccessibles(&allowsAnonChildren);
            if (!allowsAnonChildren) {
              NS_ADDREF(*aRelevantNode = bindingNode);
              return NS_OK;
            }
          }
        }
      }
    }
  }

  NS_ADDREF(*aRelevantNode = aNode);
  return NS_OK;
}

nsresult nsAccessibilityService::GetAccessibleByType(nsIDOMNode *aNode,
                                                     nsIAccessible **aAccessible)
{
  NS_ENSURE_ARG(aNode);
  NS_ENSURE_ARG_POINTER(aAccessible);

  *aAccessible = nsnull;

  nsCOMPtr<nsIAccessibleProvider> node(do_QueryInterface(aNode));
  if (!node)
    return NS_OK;

  PRInt32 type;
  nsresult rv = node->GetAccessibleType(&type);
  NS_ENSURE_SUCCESS(rv, rv);

  if (type == nsIAccessibleProvider::OuterDoc)
    return CreateOuterDocAccessible(aNode, aAccessible);

  nsCOMPtr<nsIWeakReference> weakShell;
  GetShellFromNode(aNode, getter_AddRefs(weakShell));

  switch (type)
  {
#ifdef MOZ_XUL
    // XUL controls
    case nsIAccessibleProvider::XULAlert:
      *aAccessible = new nsXULAlertAccessible(aNode, weakShell);
      break;
    case nsIAccessibleProvider::XULButton:
      *aAccessible = new nsXULButtonAccessible(aNode, weakShell);
      break;
    case nsIAccessibleProvider::XULCheckbox:
      *aAccessible = new nsXULCheckboxAccessible(aNode, weakShell);
      break;
    case nsIAccessibleProvider::XULColorPicker:
      *aAccessible = new nsXULColorPickerAccessible(aNode, weakShell);
      break;
    case nsIAccessibleProvider::XULColorPickerTile:
      *aAccessible = new nsXULColorPickerTileAccessible(aNode, weakShell);
      break;
    case nsIAccessibleProvider::XULCombobox:
      *aAccessible = new nsXULComboboxAccessible(aNode, weakShell);
      break;
    case nsIAccessibleProvider::XULDropmarker:
      *aAccessible = new nsXULDropmarkerAccessible(aNode, weakShell);
      break;
    case nsIAccessibleProvider::XULGroupbox:
      *aAccessible = new nsXULGroupboxAccessible(aNode, weakShell);
      break;
    case nsIAccessibleProvider::XULImage:
    {
      // Don't include nameless images in accessible tree
      nsCOMPtr<nsIDOMElement> elt(do_QueryInterface(aNode));
      if (!elt)
        return NS_ERROR_FAILURE;

      PRBool hasTextEquivalent;
      // Prefer value over tooltiptext
      elt->HasAttribute(NS_LITERAL_STRING("tooltiptext"), &hasTextEquivalent);
      if (!hasTextEquivalent)
        return NS_OK;

      *aAccessible = new nsHTMLImageAccessible(aNode, weakShell);
      break;
    }
    case nsIAccessibleProvider::XULLink:
      *aAccessible = new nsXULLinkAccessible(aNode, weakShell);
      break;
    case nsIAccessibleProvider::XULListbox:
      *aAccessible = new nsXULListboxAccessible(aNode, weakShell);
      break;
    case nsIAccessibleProvider::XULListitem:
      *aAccessible = new nsXULListitemAccessible(aNode, weakShell);
      break;
    case nsIAccessibleProvider::XULMenubar:
      *aAccessible = new nsXULMenubarAccessible(aNode, weakShell);
      break;
    case nsIAccessibleProvider::XULMenuitem:
      *aAccessible = new nsXULMenuitemAccessibleWrap(aNode, weakShell);
      break;
    case nsIAccessibleProvider::XULMenupopup:
    {
#ifdef MOZ_ACCESSIBILITY_ATK
      // ATK considers this node to be redundant when within menubars, and it makes menu
      // navigation with assistive technologies more difficult
      // XXX In the future we will should this for consistency across the nsIAccessible
      // implementations on each platform for a consistent scripting environment, but
      // then strip out redundant accessibles in the nsAccessibleWrap class for each platform.
      nsCOMPtr<nsIContent> content = do_QueryInterface(aNode);
      if (content) {
        nsIContent *parent = content->GetParent();
        if (parent && parent->NodeInfo()->Equals(nsAccessibilityAtoms::menu, kNameSpaceID_XUL)) {
          return NS_OK;
        }
      }
#endif
      *aAccessible = new nsXULMenupopupAccessible(aNode, weakShell);
      break;
    }
    case nsIAccessibleProvider::XULMenuSeparator:
      *aAccessible = new nsXULMenuSeparatorAccessible(aNode, weakShell);
      break;
    case nsIAccessibleProvider::XULProgressMeter:
      *aAccessible = new nsXULProgressMeterAccessible(aNode, weakShell);
      break;
    case nsIAccessibleProvider::XULStatusBar:
      *aAccessible = new nsXULStatusBarAccessible(aNode, weakShell);
      break;
    case nsIAccessibleProvider::XULRadioButton:
      *aAccessible = new nsXULRadioButtonAccessible(aNode, weakShell);
      break;
    case nsIAccessibleProvider::XULRadioGroup:
      *aAccessible = new nsXULRadioGroupAccessible(aNode, weakShell);
      break;
    case nsIAccessibleProvider::XULTab:
      *aAccessible = new nsXULTabAccessible(aNode, weakShell);
      break;
    case nsIAccessibleProvider::XULTabBox:
      *aAccessible = new nsXULTabBoxAccessible(aNode, weakShell);
      break;
    case nsIAccessibleProvider::XULTabPanels:
      *aAccessible = new nsXULTabPanelsAccessible(aNode, weakShell);
      break;
    case nsIAccessibleProvider::XULTabs:
      *aAccessible = new nsXULTabsAccessible(aNode, weakShell);
      break;
    case nsIAccessibleProvider::XULText:
      *aAccessible = new nsXULTextAccessible(aNode, weakShell);
      break;
    case nsIAccessibleProvider::XULTextBox:
      *aAccessible = new nsXULTextFieldAccessible(aNode, weakShell);
      break;
    case nsIAccessibleProvider::XULTree:
      *aAccessible = new nsXULTreeAccessibleWrap(aNode, weakShell);
      break;
    case nsIAccessibleProvider::XULTreeColumns:
      *aAccessible = new nsXULTreeColumnsAccessibleWrap(aNode, weakShell);
      break;
    case nsIAccessibleProvider::XULTreeColumnitem:
      *aAccessible = new nsXULTreeColumnitemAccessible(aNode, weakShell);
      break;
    case nsIAccessibleProvider::XULToolbar:
      *aAccessible = new nsXULToolbarAccessible(aNode, weakShell);
      break;
    case nsIAccessibleProvider::XULToolbarSeparator:
      *aAccessible = new nsXULToolbarSeparatorAccessible(aNode, weakShell);
      break;
    case nsIAccessibleProvider::XULTooltip:
      *aAccessible = new nsXULTooltipAccessible(aNode, weakShell);
      break;
#endif // MOZ_XUL

#ifndef DISABLE_XFORMS_HOOKS
    // XForms elements
    case nsIAccessibleProvider::XFormsContainer:
      *aAccessible = new nsXFormsContainerAccessible(aNode, weakShell);
      break;

    case nsIAccessibleProvider::XFormsLabel:
      *aAccessible = new nsXFormsLabelAccessible(aNode, weakShell);
      break;
    case nsIAccessibleProvider::XFormsOuput:
      *aAccessible = new nsXFormsOutputAccessible(aNode, weakShell);
      break;
    case nsIAccessibleProvider::XFormsTrigger:
      *aAccessible = new nsXFormsTriggerAccessible(aNode, weakShell);
      break;
    case nsIAccessibleProvider::XFormsInput:
      *aAccessible = new nsXFormsInputAccessible(aNode, weakShell);
      break;
    case nsIAccessibleProvider::XFormsInputBoolean:
      *aAccessible = new nsXFormsInputBooleanAccessible(aNode, weakShell);
      break;
    case nsIAccessibleProvider::XFormsInputDate:
      *aAccessible = new nsXFormsInputDateAccessible(aNode, weakShell);
      break;
    case nsIAccessibleProvider::XFormsSecret:
      *aAccessible = new nsXFormsSecretAccessible(aNode, weakShell);
      break;
    case nsIAccessibleProvider::XFormsSliderRange:
      *aAccessible = new nsXFormsRangeAccessible(aNode, weakShell);
      break;
    case nsIAccessibleProvider::XFormsSelect:
      *aAccessible = new nsXFormsSelectAccessible(aNode, weakShell);
      break;
    case nsIAccessibleProvider::XFormsChoices:
      *aAccessible = new nsXFormsChoicesAccessible(aNode, weakShell);
      break;
    case nsIAccessibleProvider::XFormsSelectFull:
      *aAccessible = new nsXFormsSelectFullAccessible(aNode, weakShell);
      break;
    case nsIAccessibleProvider::XFormsItemCheckgroup:
      *aAccessible = new nsXFormsItemCheckgroupAccessible(aNode, weakShell);
      break;
    case nsIAccessibleProvider::XFormsItemRadiogroup:
      *aAccessible = new nsXFormsItemRadiogroupAccessible(aNode, weakShell);
      break;
    case nsIAccessibleProvider::XFormsSelectCombobox:
      *aAccessible = new nsXFormsSelectComboboxAccessible(aNode, weakShell);
      break;
    case nsIAccessibleProvider::XFormsItemCombobox:
      *aAccessible = new nsXFormsItemComboboxAccessible(aNode, weakShell);
      break;

    case nsIAccessibleProvider::XFormsDropmarkerWidget:
      *aAccessible = new nsXFormsDropmarkerWidgetAccessible(aNode, weakShell);
      break;
    case nsIAccessibleProvider::XFormsCalendarWidget:
      *aAccessible = new nsXFormsCalendarWidgetAccessible(aNode, weakShell);
      break;
    case nsIAccessibleProvider::XFormsComboboxPopupWidget:
      *aAccessible = new nsXFormsComboboxPopupWidgetAccessible(aNode, weakShell);
      break;
#endif

    default:
      return NS_ERROR_FAILURE;
  }

  if (!*aAccessible)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*aAccessible);
  return NS_OK;
}

NS_IMETHODIMP nsAccessibilityService::AddNativeRootAccessible(void * aAtkAccessible,  nsIAccessible **aRootAccessible)
{
#ifdef MOZ_ACCESSIBILITY_ATK
  nsNativeRootAccessibleWrap* rootAccWrap =
    new nsNativeRootAccessibleWrap((AtkObject*)aAtkAccessible);

  *aRootAccessible = NS_STATIC_CAST(nsIAccessible*, rootAccWrap);
  NS_ADDREF(*aRootAccessible);

  nsAppRootAccessible *appRoot = nsAppRootAccessible::Create();
  appRoot->AddRootAccessible(*aRootAccessible);

  return NS_OK;
#else
  return NS_ERROR_NOT_IMPLEMENTED;
#endif
}

NS_IMETHODIMP nsAccessibilityService::RemoveNativeRootAccessible(nsIAccessible * aRootAccessible)
{
#ifdef MOZ_ACCESSIBILITY_ATK
  void* atkAccessible;
  aRootAccessible->GetNativeInterface(&atkAccessible);

  nsAppRootAccessible *appRoot = nsAppRootAccessible::Create();
  appRoot->RemoveRootAccessible(aRootAccessible);

  return NS_OK;
#else
  return NS_ERROR_NOT_IMPLEMENTED;
#endif
}

// Called from layout when the frame tree owned by a node changes significantly
NS_IMETHODIMP nsAccessibilityService::InvalidateSubtreeFor(nsIPresShell *aShell,
                                                           nsIContent *aChangeContent,
                                                           PRUint32 aEvent)
{
  NS_ASSERTION(aEvent == nsIAccessibleEvent::EVENT_REORDER ||
               aEvent == nsIAccessibleEvent::EVENT_SHOW ||
               aEvent == nsIAccessibleEvent::EVENT_HIDE,
               "Incorrect aEvent passed in");

  nsCOMPtr<nsIWeakReference> weakShell(do_GetWeakReference(aShell));
  NS_ASSERTION(aShell, "No pres shell in call to InvalidateSubtreeFor");
  nsCOMPtr<nsIAccessibleDocument> accessibleDoc =
    nsAccessNode::GetDocAccessibleFor(weakShell);
  nsCOMPtr<nsPIAccessibleDocument> privateAccessibleDoc =
    do_QueryInterface(accessibleDoc);
  if (!privateAccessibleDoc) {
    return NS_OK;
  }
  return privateAccessibleDoc->InvalidateCacheSubtree(aChangeContent, aEvent);
}

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////

nsresult 
nsAccessibilityService::GetAccessibilityService(nsIAccessibilityService** aResult)
{
  NS_PRECONDITION(aResult != nsnull, "null ptr");
  if (! aResult)
      return NS_ERROR_NULL_POINTER;

  *aResult = nsnull;
  if (!nsAccessibilityService::gAccessibilityService) {
    gAccessibilityService = new nsAccessibilityService();
    if (!gAccessibilityService ) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }
  *aResult = nsAccessibilityService::gAccessibilityService;
  NS_ADDREF(*aResult);
  return NS_OK;
}

nsresult
NS_GetAccessibilityService(nsIAccessibilityService** aResult)
{
  return nsAccessibilityService::GetAccessibilityService(aResult);
}

