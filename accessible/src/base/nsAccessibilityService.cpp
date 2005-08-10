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
#include "nsCaretAccessible.h"
#include "nsCURILoader.h"
#include "nsDocAccessible.h"
#include "nsHTMLAreaAccessible.h"
#include "nsHTMLFormControlAccessibleWrap.h"
#include "nsHTMLImageAccessible.h"
#include "nsHTMLLinkAccessible.h"
#include "nsHTMLSelectAccessible.h"
#include "nsHTMLTableAccessible.h"
#include "nsHTMLTextAccessible.h"
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
#include "nsILink.h"
#include "nsINameSpaceManager.h"
#include "nsIObserverService.h"
#include "nsIPluginInstance.h"
#include "nsPresContext.h"
#include "nsIPresShell.h"
#include "nsITextContent.h"
#include "nsIWebNavigation.h"
#include "nsObjectFrame.h"
#include "nsOuterDocAccessible.h"
#include "nsRootAccessibleWrap.h"
#include "nsTextFragment.h"
#include "nsPIAccessNode.h"
#include "nsUnicharUtils.h"
#include "nsIWebProgress.h"

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
#include "nsHTMLBlockAccessible.h"
#include "nsHTMLLinkAccessibleWrap.h"
#include "nsHTMLFormControlAccessibleWrap.h"
#include "nsHTMLTableAccessibleWrap.h"
#include "nsXULFormControlAccessibleWrap.h"
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

  nsCOMPtr<nsIDocShellTreeItem> docShellTreeItem =
    nsAccessNode::GetDocShellTreeItemFor(domDocRootNode);
  NS_ASSERTION(docShellTreeItem, "No doc shell tree item for loading document");
  NS_ENSURE_TRUE(docShellTreeItem, NS_ERROR_FAILURE);
  PRInt32 contentType;
  docShellTreeItem->GetItemType(&contentType);
  if (contentType != nsIDocShellTreeItem::typeContent) {
    return NS_OK; // Not interested in chrome loading, just content
  }

  // Get the accessible for the new document.
  // If it not created yet this will create it & cache it, as well as 
  // set up event listeners so that MSAA/ATK toolkit and internal 
  // accessibility events will get fired.
  nsCOMPtr<nsIAccessible> accessible;
  GetAccessibleFor(domDocRootNode, getter_AddRefs(accessible));
  nsCOMPtr<nsPIAccessibleDocument> docAccessible =
    do_QueryInterface(accessible);
  NS_ENSURE_TRUE(docAccessible, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDocShellTreeItem> sameTypeRoot;
  docShellTreeItem->GetSameTypeRootTreeItem(getter_AddRefs(sameTypeRoot));
  PRBool isFinished = !(aStateFlags & STATE_START);
  if (sameTypeRoot != docShellTreeItem && !isFinished) {
    return NS_OK;   // A frame or iframe has begun to load new content
  }

  docAccessible->FireDocLoadingEvent(isFinished);

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
  // and STATE_STOP
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

#ifdef DEBUG
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
nsAccessibilityService::CreateHTMLBlockAccessible(nsISupports *aFrame, nsIAccessible **_retval)
{
#ifndef MOZ_ACCESSIBILITY_ATK
  *_retval = nsnull;
  return NS_ERROR_FAILURE;
#else
  nsIFrame* frame;
  nsCOMPtr<nsIDOMNode> node;
  nsCOMPtr<nsIWeakReference> weakShell;
  nsresult rv = GetInfo(aFrame, &frame, getter_AddRefs(weakShell), getter_AddRefs(node));
  if (NS_FAILED(rv))
    return rv;

  *_retval = new nsHTMLBlockAccessible(node, weakShell);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
  return NS_OK;
#endif
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

NS_IMETHODIMP
nsAccessibilityService::CreateHTMLButtonAccessibleXBL(nsIDOMNode *aNode, nsIAccessible **_retval)
{
#ifdef MOZ_XUL
  nsCOMPtr<nsIWeakReference> weakShell;
  GetShellFromNode(aNode, getter_AddRefs(weakShell));

  // reusing the HTML accessible widget and enhancing for XUL
  *_retval = new nsHTML4ButtonAccessible(aNode, weakShell);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
#else
  *_retval = nsnull;
#endif // MOZ_XUL
  return NS_OK;
}

PRBool nsAccessibilityService::GetRole(nsIContent *aContent,
                                       nsIWeakReference *aWeakShell,
                                       nsAString& aRole)
{
  return NS_CONTENT_ATTR_HAS_VALUE ==
         aContent->GetAttr(kNameSpaceID_XHTML2_Unofficial,
                           nsAccessibilityAtoms::role, aRole);
}

nsresult
nsAccessibilityService::CreateHTMLAccessibleByMarkup(nsISupports *aFrame,
                                                     nsIWeakReference *aWeakShell,
                                                     nsIDOMNode *aNode,
                                                     const nsAString& aRole,
                                                     nsIAccessible **aAccessible)
{
  // aFrame type was generic, we'll use the DOM to decide 
  // if and what kind of accessible object is needed.
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
#ifndef MOZ_ACCESSIBILITY_ATK
  else if (tag == nsAccessibilityAtoms::ul || tag == nsAccessibilityAtoms::ol) {
    *aAccessible = new nsHTMLListAccessible(aNode, aWeakShell);
  }
  else if (tag == nsAccessibilityAtoms::a) {
    *aAccessible = new nsHTMLLinkAccessible(aNode, aWeakShell, NS_STATIC_CAST(nsIFrame*, aFrame));
  }
  else if (content->HasAttr(kNameSpaceID_None, nsAccessibilityAtoms::onclick)) {
    *aAccessible = new nsLinkableAccessible(aNode, aWeakShell);
  }
  else if (tag == nsAccessibilityAtoms::li) {
    // Normally this is created by the list item frame which knows about the bullet frame
    // However, in this case the list item must have been styled using display: foo
    *aAccessible = new nsHTMLLIAccessible(aNode, aWeakShell, nsnull, EmptyString());
  }
  else if (tag == nsAccessibilityAtoms::abbr ||
           tag == nsAccessibilityAtoms::acronym ||
           tag == nsAccessibilityAtoms::blockquote ||
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
           tag == nsAccessibilityAtoms::q ||
           tag == nsAccessibilityAtoms::tbody ||
           tag == nsAccessibilityAtoms::tfoot ||
           tag == nsAccessibilityAtoms::thead ||
#else
  else if (
#endif
           content->HasAttr(kNameSpaceID_None, nsAccessibilityAtoms::tabindex) ||
           // The role from a <body> or doc element is already exposed in nsDocAccessible
           (tag != nsAccessibilityAtoms::body && content->GetParent() &&
           !aRole.IsEmpty())) {
    *aAccessible = new nsAccessibleWrap(aNode, aWeakShell);
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
nsAccessibilityService::CreateHTMLCheckboxAccessibleXBL(nsIDOMNode *aNode, nsIAccessible **_retval)
{
#ifdef MOZ_XUL
  nsCOMPtr<nsIWeakReference> weakShell;
  GetShellFromNode(aNode, getter_AddRefs(weakShell));

  // reusing the HTML accessible widget and enhancing for XUL
  *_retval = new nsHTMLCheckboxAccessible(aNode, weakShell);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
#else
  *_retval = nsnull;
#endif // MOZ_XUL
  return NS_OK;
}

NS_IMETHODIMP 
nsAccessibilityService::CreateHTMLComboboxAccessible(nsIDOMNode* aDOMNode, nsISupports* aPresShell, nsIAccessible **_retval)
{
  nsCOMPtr<nsIPresShell> presShell(do_QueryInterface(aPresShell));
  NS_ASSERTION(presShell,"Error non prescontext passed to accessible factory!!!");

  nsCOMPtr<nsIWeakReference> weakShell =
    do_GetWeakReference(presShell);

  *_retval = new nsHTMLComboboxAccessible(aDOMNode, weakShell);
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
#ifdef MOZ_ACCESSIBILITY_ATK
    PRBool hasAttribute;
    rv = domElement->HasAttribute(NS_LITERAL_STRING("usemap"), &hasAttribute);
    if (NS_SUCCEEDED(rv) && hasAttribute) {
      //There is a "use map"
      *_retval = new nsHTMLImageMapAccessible(node, weakShell);
    }
    else
#endif //MOZ_ACCESSIBILITY_ATK
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
nsAccessibilityService::CreateHTMLListboxAccessible(nsIDOMNode* aDOMNode, nsISupports* aPresContext, nsIAccessible **_retval)
{
  nsCOMPtr<nsPresContext> presContext(do_QueryInterface(aPresContext));
  NS_ASSERTION(presContext,"Error non prescontext passed to accessible factory!!!");

  nsCOMPtr<nsIWeakReference> weakShell =
    do_GetWeakReference(presContext->PresShell());

  *_retval = new nsHTMLSelectListAccessible(aDOMNode, weakShell);
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
      *aAccessible = new nsHTMLWin32ObjectAccessible(node, weakShell, pluginPort);
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
  if (frame) {
    return frame->GetAccessible(aAccessible);
  }
  return NS_ERROR_FAILURE;
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
nsAccessibilityService::CreateHTMLRadioButtonAccessibleXBL(nsIDOMNode *aNode, nsIAccessible **_retval)
{
#ifdef MOZ_XUL
  nsCOMPtr<nsIWeakReference> weakShell;
  GetShellFromNode(aNode, getter_AddRefs(weakShell));

  // reusing the HTML accessible widget and enhancing for XUL
  *_retval = new nsHTMLRadioButtonAccessible(aNode, weakShell);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
#else
  *_retval = nsnull;
#endif // MOZ_XUL
  return NS_OK;
}

NS_IMETHODIMP 
nsAccessibilityService::CreateHTMLSelectOptionAccessible(nsIDOMNode* aDOMNode, 
                                                         nsIAccessible *aParent, 
                                                         nsISupports* aPresContext, 
                                                         nsIAccessible **_retval)
{
  nsCOMPtr<nsPresContext> presContext(do_QueryInterface(aPresContext));
  NS_ASSERTION(presContext,"Error non prescontext passed to accessible factory!!!");

  nsCOMPtr<nsIWeakReference> weakShell =
    do_GetWeakReference(presContext->PresShell());

  *_retval = new nsHTMLSelectOptionAccessible(aDOMNode, weakShell);
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

  *_retval = new nsHTMLTableAccessibleWrap(node, weakShell);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
  return NS_OK;
}

NS_IMETHODIMP
nsAccessibilityService::CreateHTMLTableCaptionAccessible(nsIDOMNode *aDOMNode, nsIAccessible **_retval)
{
  NS_ENSURE_ARG_POINTER(aDOMNode);

  nsresult rv = NS_OK;

  nsCOMPtr<nsIWeakReference> weakShell;
  rv = GetShellFromNode(aDOMNode, getter_AddRefs(weakShell));
  NS_ENSURE_SUCCESS(rv, rv);

  nsHTMLTableCaptionAccessible* accTableCaption =
    new nsHTMLTableCaptionAccessible(aDOMNode, weakShell);

  NS_ENSURE_TRUE(accTableCaption, NS_ERROR_OUT_OF_MEMORY);

  *_retval = NS_STATIC_CAST(nsIAccessible *, accTableCaption);
  NS_IF_ADDREF(*_retval);

  return rv;
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

  *_retval = new nsHTMLTableCellAccessibleWrap(node, weakShell);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
  return NS_OK;
}

NS_IMETHODIMP
nsAccessibilityService::CreateHTMLTextAccessible(nsISupports *aFrame, nsIAccessible **_retval)
{
  nsIFrame* frame;
  nsCOMPtr<nsIDOMNode> node;
  nsCOMPtr<nsIWeakReference> weakShell;
  nsresult rv = GetInfo(aFrame, &frame, getter_AddRefs(weakShell), getter_AddRefs(node));
  if (NS_FAILED(rv))
    return rv;

  *_retval = nsnull;

#ifndef MOZ_ACCESSIBILITY_ATK
  *_retval = new nsHTMLTextAccessible(node, weakShell, frame);
#else
  // In ATK, we are only creating the accessible object for the text frame that is the FIRST
  //   text frame in its block.
  // A depth-first traversal from its nearest parent block frame will produce a frame sequence like
  //   TTTBTTBTT... (B for block frame, T for text frame), so every T frame which is the immediate 
  //   sibling of B frame will be the FIRST text frame.
  nsIFrame* parentFrame = nsAccessible::GetParentBlockFrame(frame);
  if (! parentFrame)
    return NS_ERROR_FAILURE; 

  nsCOMPtr<nsIPresShell> presShell(do_QueryReferent(weakShell));
  nsIFrame* childFrame = parentFrame->GetFirstChild(nsnull);
  PRInt32 index = 0;
  nsIFrame* firstTextFrame = nsnull;
  PRBool ret = nsAccessible::FindTextFrame(index, presShell->GetPresContext(),
                                           childFrame, &firstTextFrame, frame);
  if (!ret || index != 0)
    return NS_ERROR_FAILURE; 

  *_retval = new nsHTMLBlockAccessible(node, weakShell);
#endif //MOZ_ACCESSIBILITY_ATK
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

  *_retval = new nsHTMLTextFieldAccessibleWrap(node, weakShell);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
  return NS_OK;
}

NS_IMETHODIMP
nsAccessibilityService::CreateXULTextBoxAccessible(nsIDOMNode *aNode, nsIAccessible **_retval)
{
#ifdef MOZ_XUL
  nsCOMPtr<nsIWeakReference> weakShell;
  GetShellFromNode(aNode, getter_AddRefs(weakShell));

  *_retval = new nsXULTextFieldAccessibleWrap(aNode, weakShell);

  if (! *_retval)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
#else
  *_retval = nsnull;
#endif // MOZ_XUL
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

 /**
   * XUL widget creation
   *  we can't ifdef this whole block because there is no way to exclude
   *  these methods from the idl definition conditionally (MOZ_XUL).
   *  XXXjgaunt what needs to happen is all of the CreateFooAcc() methods get re-written
   *  into a single method.
   */

NS_IMETHODIMP nsAccessibilityService::CreateXULButtonAccessible(nsIDOMNode *aNode, nsIAccessible **_retval)
{
#ifdef MOZ_XUL
  nsCOMPtr<nsIWeakReference> weakShell;
  GetShellFromNode(aNode, getter_AddRefs(weakShell));

  // reusing the HTML accessible widget and enhancing for XUL
  *_retval = new nsXULButtonAccessible(aNode, weakShell);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
#else
  *_retval = nsnull;
#endif // MOZ_XUL
  return NS_OK;
}

NS_IMETHODIMP
nsAccessibilityService::CreateXULCheckboxAccessible(nsIDOMNode *aNode, nsIAccessible **_retval)
{
#ifdef MOZ_XUL
  nsCOMPtr<nsIWeakReference> weakShell;
  GetShellFromNode(aNode, getter_AddRefs(weakShell));

  // reusing the HTML accessible widget and enhancing for XUL
  *_retval = new nsXULCheckboxAccessible(aNode, weakShell);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
#else
  *_retval = nsnull;
#endif // MOZ_XUL
  return NS_OK;
}

NS_IMETHODIMP
nsAccessibilityService::CreateXULColorPickerAccessible(nsIDOMNode *aNode, nsIAccessible **_retval)
{
#ifdef MOZ_XUL
  nsCOMPtr<nsIWeakReference> weakShell;
  GetShellFromNode(aNode, getter_AddRefs(weakShell));

  *_retval = new nsXULColorPickerAccessible(aNode, weakShell);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
#else
  *_retval = nsnull;
#endif // MOZ_XUL
  return NS_OK;
}

NS_IMETHODIMP
nsAccessibilityService::CreateXULColorPickerTileAccessible(nsIDOMNode *aNode, nsIAccessible **_retval)
{
#ifdef MOZ_XUL
  nsCOMPtr<nsIWeakReference> weakShell;
  GetShellFromNode(aNode, getter_AddRefs(weakShell));

  *_retval = new nsXULColorPickerTileAccessible(aNode, weakShell);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
#else
  *_retval = nsnull;
#endif // MOZ_XUL
  return NS_OK;
}

NS_IMETHODIMP 
nsAccessibilityService::CreateXULComboboxAccessible(nsIDOMNode *aNode, nsIAccessible **_retval)
{
#ifdef MOZ_XUL
  nsCOMPtr<nsIWeakReference> weakShell;
  GetShellFromNode(aNode, getter_AddRefs(weakShell));

  *_retval = new nsXULComboboxAccessible(aNode, weakShell);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
#else
  *_retval = nsnull;
#endif // MOZ_XUL
  return NS_OK;
}

NS_IMETHODIMP 
nsAccessibilityService::CreateXULDropmarkerAccessible(nsIDOMNode *aNode, nsIAccessible **_retval)
{
#ifdef MOZ_XUL
  nsCOMPtr<nsIWeakReference> weakShell;
  GetShellFromNode(aNode, getter_AddRefs(weakShell));

  *_retval = new nsXULDropmarkerAccessible(aNode, weakShell);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
#else
  *_retval = nsnull;
#endif // MOZ_XUL
  return NS_OK;
}

NS_IMETHODIMP
nsAccessibilityService::CreateXULGroupboxAccessible(nsIDOMNode *aNode, nsIAccessible **_retval)
{
#ifdef MOZ_XUL
  nsCOMPtr<nsIWeakReference> weakShell;
  GetShellFromNode(aNode, getter_AddRefs(weakShell));

  *_retval = new nsXULGroupboxAccessible(aNode, weakShell);
  if (! *_retval)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
#else
  *_retval = nsnull;
#endif // MOZ_XUL
  return NS_OK;
}

NS_IMETHODIMP
nsAccessibilityService::CreateXULImageAccessible(nsIDOMNode *aNode, nsIAccessible **_retval)
{
#ifdef MOZ_XUL
  // Don't include nameless images in accessible tree
  *_retval = nsnull;

  nsCOMPtr<nsIDOMElement> elt(do_QueryInterface(aNode));
  if (!elt)
    return NS_ERROR_FAILURE;
  PRBool hasTextEquivalent;
  elt->HasAttribute(NS_LITERAL_STRING("tooltiptext"), &hasTextEquivalent); // Prefer value over tooltiptext
  if (!hasTextEquivalent) {
    return NS_OK;
  }

  nsCOMPtr<nsIWeakReference> weakShell;
  GetShellFromNode(aNode, getter_AddRefs(weakShell));

  *_retval = new nsHTMLImageAccessible(aNode, weakShell);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);

#else
  *_retval = nsnull;
#endif // MOZ_XUL
  return NS_OK;
}

NS_IMETHODIMP
nsAccessibilityService::CreateXULLinkAccessible(nsIDOMNode *aNode, nsIAccessible **_retval)
{
#ifdef MOZ_XUL
  nsCOMPtr<nsIWeakReference> weakShell;
  GetShellFromNode(aNode, getter_AddRefs(weakShell));

  *_retval = new nsXULLinkAccessible(aNode, weakShell);
  if (! *_retval)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
#else
  *_retval = nsnull;
#endif // MOZ_XUL
  return NS_OK;
}


NS_IMETHODIMP 
nsAccessibilityService::CreateXULListboxAccessible(nsIDOMNode *aNode, nsIAccessible **_retval)
{
#ifdef MOZ_XUL
  nsCOMPtr<nsIWeakReference> weakShell;
  GetShellFromNode(aNode, getter_AddRefs(weakShell));

  *_retval = new nsXULListboxAccessible(aNode, weakShell);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
#else
  *_retval = nsnull;
#endif // MOZ_XUL
  return NS_OK;
}

NS_IMETHODIMP 
nsAccessibilityService::CreateXULListitemAccessible(nsIDOMNode *aNode, nsIAccessible **_retval)
{
#ifdef MOZ_XUL
  nsCOMPtr<nsIWeakReference> weakShell;
  GetShellFromNode(aNode, getter_AddRefs(weakShell));

  *_retval = new nsXULListitemAccessible(aNode, weakShell);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
#else
  *_retval = nsnull;
#endif // MOZ_XUL
  return NS_OK;
}

NS_IMETHODIMP
nsAccessibilityService::CreateXULMenubarAccessible(nsIDOMNode *aNode, nsIAccessible **_retval)
{
#ifdef MOZ_XUL
  nsCOMPtr<nsIWeakReference> weakShell;
  GetShellFromNode(aNode, getter_AddRefs(weakShell));

  *_retval = new nsXULMenubarAccessible(aNode, weakShell);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
#else
  *_retval = nsnull;
#endif // MOZ_XUL
  return NS_OK;
}

NS_IMETHODIMP
nsAccessibilityService::CreateXULMenuitemAccessible(nsIDOMNode *aNode, nsIAccessible **_retval)
{
#ifdef MOZ_XUL
  nsCOMPtr<nsIWeakReference> weakShell;
  GetShellFromNode(aNode, getter_AddRefs(weakShell));

  *_retval = new nsXULMenuitemAccessibleWrap(aNode, weakShell);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
#else
  *_retval = nsnull;
#endif // MOZ_XUL
  return NS_OK;
}

NS_IMETHODIMP
nsAccessibilityService::CreateXULMenupopupAccessible(nsIDOMNode *aNode, nsIAccessible **_retval)
{
#ifdef MOZ_XUL
  nsCOMPtr<nsIWeakReference> weakShell;
  GetShellFromNode(aNode, getter_AddRefs(weakShell));

  *_retval = new nsXULMenupopupAccessible(aNode, weakShell);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
#else
  *_retval = nsnull;
#endif // MOZ_XUL
  return NS_OK;
}

NS_IMETHODIMP
nsAccessibilityService::CreateXULMenuSeparatorAccessible(nsIDOMNode *aNode, nsIAccessible **_retval)
{
#ifdef MOZ_XUL
  nsCOMPtr<nsIWeakReference> weakShell;
  GetShellFromNode(aNode, getter_AddRefs(weakShell));

  *_retval = new nsXULMenuSeparatorAccessible(aNode, weakShell);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
#else
  *_retval = nsnull;
#endif // MOZ_XUL
  return NS_OK;
}

NS_IMETHODIMP
nsAccessibilityService::CreateXULAlertAccessible(nsIDOMNode *aNode, nsIAccessible **aAccessible)
{
#ifdef MOZ_XUL
  nsCOMPtr<nsIWeakReference> weakShell;
  GetShellFromNode(aNode, getter_AddRefs(weakShell));

  *aAccessible = new nsXULAlertAccessible(aNode, weakShell);
  if (! *aAccessible)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*aAccessible);
#else
  *_retval = nsnull;
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsAccessibilityService::CreateXULProgressMeterAccessible(nsIDOMNode *aNode, nsIAccessible **_retval)
{
#ifdef MOZ_XUL
  nsCOMPtr<nsIWeakReference> weakShell;
  GetShellFromNode(aNode, getter_AddRefs(weakShell));

  *_retval = new nsXULProgressMeterAccessibleWrap(aNode, weakShell);
  if (! *_retval)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
#else
  *_retval = nsnull;
#endif // MOZ_XUL
  return NS_OK;
}

NS_IMETHODIMP 
nsAccessibilityService::CreateXULRadioButtonAccessible(nsIDOMNode *aNode, nsIAccessible **_retval)
{
#ifdef MOZ_XUL
  nsCOMPtr<nsIWeakReference> weakShell;
  GetShellFromNode(aNode, getter_AddRefs(weakShell));

  *_retval = new nsXULRadioButtonAccessible(aNode, weakShell);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
#else
  *_retval = nsnull;
#endif // MOZ_XUL
  return NS_OK;
}

NS_IMETHODIMP 
nsAccessibilityService::CreateXULRadioGroupAccessible(nsIDOMNode *aNode, nsIAccessible **_retval)
{
#ifdef MOZ_XUL
  nsCOMPtr<nsIWeakReference> weakShell;
  GetShellFromNode(aNode, getter_AddRefs(weakShell));

  *_retval = new nsXULRadioGroupAccessible(aNode, weakShell);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
#else
  *_retval = nsnull;
#endif // MOZ_XUL
  return NS_OK;
}

NS_IMETHODIMP 
nsAccessibilityService::CreateXULSelectListAccessible(nsIDOMNode *aNode, nsIAccessible **_retval)
{
#ifdef MOZ_XUL
  nsCOMPtr<nsIWeakReference> weakShell;
  GetShellFromNode(aNode, getter_AddRefs(weakShell));

  *_retval = new nsXULSelectListAccessible(aNode, weakShell);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
#else
  *_retval = nsnull;
#endif // MOZ_XUL
  return NS_OK;
}

NS_IMETHODIMP 
nsAccessibilityService::CreateXULSelectOptionAccessible(nsIDOMNode *aNode, nsIAccessible **_retval)
{
#ifdef MOZ_XUL
  nsCOMPtr<nsIWeakReference> weakShell;
  GetShellFromNode(aNode, getter_AddRefs(weakShell));

  *_retval = new nsXULSelectOptionAccessible(aNode, weakShell);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
#else
  *_retval = nsnull;
#endif // MOZ_XUL
  return NS_OK;
}

NS_IMETHODIMP 
nsAccessibilityService::CreateXULStatusBarAccessible(nsIDOMNode *aNode, nsIAccessible **_retval)
{
#ifdef MOZ_XUL
  nsCOMPtr<nsIWeakReference> weakShell;
  GetShellFromNode(aNode, getter_AddRefs(weakShell));

  *_retval = new nsXULStatusBarAccessible(aNode, weakShell);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
#else
  *_retval = nsnull;
#endif // MOZ_XUL
  return NS_OK;
}

NS_IMETHODIMP
nsAccessibilityService::CreateXULTextAccessible(nsIDOMNode *aNode, nsIAccessible **_retval)
{
#ifdef MOZ_XUL
  nsCOMPtr<nsIWeakReference> weakShell;
  GetShellFromNode(aNode, getter_AddRefs(weakShell));

  // reusing the HTML accessible widget and enhancing for XUL
  *_retval = new nsXULTextAccessible(aNode, weakShell);
  if (! *_retval)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
#else
  *_retval = nsnull;
#endif // MOZ_XUL
  return NS_OK;
}

/** The single tab in a dialog or tabbrowser/editor interface */
NS_IMETHODIMP nsAccessibilityService::CreateXULTabAccessible(nsIDOMNode *aNode, nsIAccessible **_retval)
{
#ifdef MOZ_XUL
  nsCOMPtr<nsIWeakReference> weakShell;
  GetShellFromNode(aNode, getter_AddRefs(weakShell));

  *_retval = new nsXULTabAccessible(aNode, weakShell);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
#else
  *_retval = nsnull;
#endif // MOZ_XUL
  return NS_OK;
}

/** A combination of a tabs object and a tabpanels object */
NS_IMETHODIMP nsAccessibilityService::CreateXULTabBoxAccessible(nsIDOMNode *aNode, nsIAccessible **_retval)
{
#ifdef MOZ_XUL
  nsCOMPtr<nsIWeakReference> weakShell;
  GetShellFromNode(aNode, getter_AddRefs(weakShell));

  *_retval = new nsXULTabBoxAccessible(aNode, weakShell);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
#else
  *_retval = nsnull;
#endif // MOZ_XUL
  return NS_OK;
}

/** The display area for a dialog or tabbrowser interface */
NS_IMETHODIMP nsAccessibilityService::CreateXULTabPanelsAccessible(nsIDOMNode *aNode, nsIAccessible **_retval)
{
#ifdef MOZ_XUL
  nsCOMPtr<nsIWeakReference> weakShell;
  GetShellFromNode(aNode, getter_AddRefs(weakShell));

  *_retval = new nsXULTabPanelsAccessible(aNode, weakShell);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
#else
  *_retval = nsnull;
#endif // MOZ_XUL
  return NS_OK;
}

/** The collection of tab objects, useable in the TabBox and independant of as well */
NS_IMETHODIMP nsAccessibilityService::CreateXULTabsAccessible(nsIDOMNode *aNode, nsIAccessible **_retval)
{
#ifdef MOZ_XUL
  nsCOMPtr<nsIWeakReference> weakShell;
  GetShellFromNode(aNode, getter_AddRefs(weakShell));

  *_retval = new nsXULTabsAccessible(aNode, weakShell);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
#else
  *_retval = nsnull;
#endif // MOZ_XUL
  return NS_OK;
}

NS_IMETHODIMP nsAccessibilityService::CreateXULToolbarAccessible(nsIDOMNode *aNode, nsIAccessible **_retval)
{
#ifdef MOZ_XUL
  nsCOMPtr<nsIWeakReference> weakShell;
  GetShellFromNode(aNode, getter_AddRefs(weakShell));

  *_retval = new nsXULToolbarAccessible(aNode, weakShell);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
#else
  *_retval = nsnull;
#endif // MOZ_XUL
  return NS_OK;
}

NS_IMETHODIMP nsAccessibilityService::CreateXULToolbarSeparatorAccessible(nsIDOMNode *aNode, nsIAccessible **_retval)
{
#ifdef MOZ_XUL
  nsCOMPtr<nsIWeakReference> weakShell;
  GetShellFromNode(aNode, getter_AddRefs(weakShell));

  *_retval = new nsXULToolbarSeparatorAccessible(aNode, weakShell);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
#else
  *_retval = nsnull;
#endif // MOZ_XUL
  return NS_OK;
}

NS_IMETHODIMP nsAccessibilityService::CreateXULTooltipAccessible(nsIDOMNode *aNode, nsIAccessible **_retval)
{
#ifdef MOZ_XUL
#ifndef MOZ_ACCESSIBILITY_ATK
  nsCOMPtr<nsIWeakReference> weakShell;
  GetShellFromNode(aNode, getter_AddRefs(weakShell));

  *_retval = new nsXULTooltipAccessible(aNode, weakShell);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
#else
  *_retval = nsnull;
#endif // MOZ_ACCESSIBILITY_ATK
#else
  *_retval = nsnull;
#endif // MOZ_XUL
  return NS_OK;
}

NS_IMETHODIMP nsAccessibilityService::CreateXULTreeAccessible(nsIDOMNode *aNode, nsIAccessible **_retval)
{
#ifdef MOZ_XUL
  nsCOMPtr<nsIWeakReference> weakShell;
  GetShellFromNode(aNode, getter_AddRefs(weakShell));

  *_retval = new nsXULTreeAccessibleWrap(aNode, weakShell);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
#else
  *_retval = nsnull;
#endif // MOZ_XUL
  return NS_OK;
}

NS_IMETHODIMP nsAccessibilityService::CreateXULTreeColumnsAccessible(nsIDOMNode *aNode, nsIAccessible **_retval)
{
#ifdef MOZ_XUL
  nsCOMPtr<nsIWeakReference> weakShell;
  GetShellFromNode(aNode, getter_AddRefs(weakShell));

  *_retval = new nsXULTreeColumnsAccessibleWrap(aNode, weakShell);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
#else
  *_retval = nsnull;
#endif // MOZ_XUL
  return NS_OK;
}

NS_IMETHODIMP nsAccessibilityService::CreateXULTreeColumnitemAccessible(nsIDOMNode *aNode, nsIAccessible **_retval)
{
#ifdef MOZ_XUL
  nsCOMPtr<nsIWeakReference> weakShell;
  GetShellFromNode(aNode, getter_AddRefs(weakShell));

  *_retval = new nsXULTreeColumnitemAccessible(aNode, weakShell);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
#else
  *_retval = nsnull;
#endif // MOZ_XUL
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

NS_IMETHODIMP nsAccessibilityService::GetAccessibleFor(nsIDOMNode *aNode, 
                                                       nsIAccessible **aAccessible)
{
  // It's not ideal to call this -- it will assume shell #0
  // Some of our old test scripts still use it
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
  NS_ASSERTION(aAccessibleOut && !*aAccessibleOut, "Out param should already be cleared out");
  if (!aAccessibleIn)
    return NS_ERROR_FAILURE;

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
  *aAccessible = nsnull;
  if (!aPresShell || !aWeakShell) {
    return NS_ERROR_FAILURE;
  }

  NS_ASSERTION(aNode, "GetAccessible() called with no node.");

  *aIsHidden = PR_FALSE;

#ifdef DEBUG_aleventhal
  // Please leave this in for now, it's a convenient debugging method
  nsAutoString name;
  aNode->GetLocalName(name);
  if (name.LowerCaseEqualsLiteral("ol")) 
    printf("## aaronl debugging tag name\n");

  nsAutoString attrib;
  nsCOMPtr<nsIDOMElement> element(do_QueryInterface(aNode));
  if (element) {
    element->GetAttribute(NS_LITERAL_STRING("type"), attrib);
    if (attrib.EqualsLiteral("checkbox"))
      printf("## aaronl debugging attribute\n");
  }
#endif

  // Check to see if we already have an accessible for this
  // node in the cache
  nsCOMPtr<nsIAccessNode> accessNode;
  GetCachedAccessNode(aNode, aWeakShell, getter_AddRefs(accessNode));

  nsCOMPtr<nsIAccessible> newAcc;
  if (accessNode) {
    // Retrieved from cache
    // QI might not succeed if it's a node that's not accessible
    newAcc = do_QueryInterface(accessNode);
    if (!newAcc) {
      return NS_ERROR_FAILURE;
    }
    NS_ADDREF(*aAccessible = newAcc);
    return NS_OK;
  }

  // No cache entry, so we must create the accessible
  // Check to see if hidden first
  nsCOMPtr<nsIContent> content(do_QueryInterface(aNode));
  nsCOMPtr<nsIDocument> nodeIsDoc;
  if (!content) {
    nodeIsDoc = do_QueryInterface(aNode);
    if (!nodeIsDoc) {
      return NS_ERROR_FAILURE;   // No content, and not doc node
    }
    // This happens when we're on the document node, which will not QI to an nsIContent, 
    nsCOMPtr<nsIAccessibleDocument> accessibleDoc = 
      nsAccessNode::GetDocAccessibleFor(aWeakShell);
    if (accessibleDoc) {
      newAcc = do_QueryInterface(accessibleDoc);
      NS_ASSERTION(newAcc, "nsIAccessibleDocument is not an nsIAccessible");
    }
    else {
      CreateRootAccessible(aPresShell, nodeIsDoc, getter_AddRefs(newAcc)); // Does Init() for us
      NS_ASSERTION(newAcc, "No root/doc accessible created");
    }

    *aFrameHint = aPresShell->GetRootFrame();
    NS_ADDREF(*aAccessible = newAcc );
    return NS_OK;
  }

  // We have a content node
  nsIFrame *frame = *aFrameHint;
  if (content->IsContentOfType(nsIContent::eXUL)) {
    nsCOMPtr<nsIDOMXULElement> xulElt(do_QueryInterface(aNode));
    if (xulElt) {
      xulElt->GetHidden(aIsHidden);
      if (!*aIsHidden) {
        xulElt->GetCollapsed(aIsHidden);
      }
    }
  }
  else { // Try frame hint
#ifdef DEBUG_aleventhal
    static int frameHintFailed, frameHintTried, frameHintNonexistant, frameHintFailedForText;
    ++frameHintTried;
#endif
    if (!frame || content != frame->GetContent()) {
      // Frame hint not correct, get true frame, we try to optimize away from this
      aPresShell->GetPrimaryFrameFor(content, &frame);
      if (frame) {
#ifdef DEBUG_aleventhal_
        // Frame hint debugging
        ++frameHintFailed;
        if (content->IsContentOfType(nsIContent::eTEXT)) {
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
          return NS_ERROR_FAILURE;
        }
        *aFrameHint = frame;
      }
    }

    // Check frame to see if it is hidden
    if (!frame || !frame->GetStyleVisibility()->IsVisible()) {
      *aIsHidden = PR_TRUE;
    }
  }

  if (*aIsHidden) {
    return NS_ERROR_FAILURE;
  }

  /**
   * Attempt to create an accessible based on what we know
   */
  if (content->IsContentOfType(nsIContent::eTEXT)) {
    // --- Create HTML for visible text frames ---
    if (frame->IsEmpty()) {
      *aIsHidden = true;
      return NS_ERROR_FAILURE;
    }
    frame->GetAccessible(getter_AddRefs(newAcc));
  }
  else if (!content->IsContentOfType(nsIContent::eHTML)) {
    // --- Try creating accessible non-HTML (XUL, etc.) ---
    // XUL elements may implement nsIAccessibleProvider via XBL
    // This allows them to say what kind of accessible to create
    // Non-HTML elements must have an nsIAccessibleProvider, tabindex
    // or XHTML2 role or they're not in the accessible tree.
    nsCOMPtr<nsIAccessibleProvider> accProv(do_QueryInterface(aNode));
    if (accProv) {
      accProv->GetAccessible(getter_AddRefs(newAcc));
    }
    else if (content->HasAttr(kNameSpaceID_None, nsAccessibilityAtoms::tabindex) ||
             content->HasAttr(kNameSpaceID_XHTML2_Unofficial, nsAccessibilityAtoms::role)) {
      newAcc = new nsAccessibleWrap(aNode, aWeakShell); // Create generic accessible
    }
    else {
      return NS_ERROR_FAILURE;
    }
  }
  else {
    // --- Try creating accessible for HTML ---
    nsAutoString role;
    GetRole(content, aWeakShell, role);
    if (!content->HasAttr(kNameSpaceID_None, nsAccessibilityAtoms::tabindex)) {
      // If no tabindex, check for a Presentation role, which 
      // tells us not to expose this to the accessibility hierarchy.
      if (StringEndsWith(role, NS_LITERAL_STRING(":presentation"),
                         nsCaseInsensitiveStringComparator())) {
        return NS_ERROR_FAILURE;
      }
      else {
        // If we're in table-related subcontent, check for the
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
              if (GetRole(tableContent, aWeakShell, tableRole) &&
                  StringEndsWith(tableRole, NS_LITERAL_STRING(":presentation"),
                  nsCaseInsensitiveStringComparator())) {
                // Table that we're a descendant of is presentational
                return NS_ERROR_FAILURE;
              }
              break;
            }
          }
        }
      }
    }

    frame->GetAccessible(getter_AddRefs(newAcc)); // Try using frame to do it
    if (!newAcc) {
      // Use markup (mostly tag name, perhaps attributes) to
      // decide if and what kind of accessible to create.
      CreateHTMLAccessibleByMarkup(frame, aWeakShell, aNode, role, getter_AddRefs(newAcc));
    }
  }

  return InitAccessible(newAcc, aAccessible);
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

