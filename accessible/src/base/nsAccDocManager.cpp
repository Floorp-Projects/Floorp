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
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Alexander Surkov <surkov.alexander@gmail.com> (original author)
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

#include "nsAccDocManager.h"

#include "nsAccessibilityService.h"
#include "nsAccUtils.h"
#include "nsApplicationAccessible.h"
#include "nsOuterDocAccessible.h"
#include "nsRootAccessibleWrap.h"

#include "nsCURILoader.h"
#include "nsDocShellLoadTypes.h"
#include "nsIChannel.h"
#include "nsIContentViewer.h"
#include "nsIDOMDocument.h"
#include "nsIEventListenerManager.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMWindow.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIWebNavigation.h"
#include "nsServiceManagerUtils.h"

////////////////////////////////////////////////////////////////////////////////
// nsAccDocManager
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// nsAccDocManager public

nsDocAccessible*
nsAccDocManager::GetDocAccessible(nsIDocument *aDocument)
{
  if (!aDocument)
    return nsnull;

  // Ensure CacheChildren is called before we query cache.
  nsAccessNode::GetApplicationAccessible()->EnsureChildren();

  nsDocAccessible* docAcc = mDocAccessibleCache.GetWeak(aDocument);
  if (docAcc)
    return docAcc;

  return CreateDocOrRootAccessible(aDocument);
}

nsAccessible*
nsAccDocManager::FindAccessibleInCache(nsINode* aNode) const
{
  nsSearchAccessibleInCacheArg arg;
  arg.mNode = aNode;

  mDocAccessibleCache.EnumerateRead(SearchAccessibleInDocCache,
                                    static_cast<void*>(&arg));

  return arg.mAccessible;
}

void
nsAccDocManager::ShutdownDocAccessiblesInTree(nsIDocument *aDocument)
{
  nsCOMPtr<nsISupports> container = aDocument->GetContainer();
  nsCOMPtr<nsIDocShellTreeItem> treeItem = do_QueryInterface(container);
  ShutdownDocAccessiblesInTree(treeItem, aDocument);
}


////////////////////////////////////////////////////////////////////////////////
// nsAccDocManager protected

PRBool
nsAccDocManager::Init()
{
  mDocAccessibleCache.Init(4);

  nsCOMPtr<nsIWebProgress> progress =
    do_GetService(NS_DOCUMENTLOADER_SERVICE_CONTRACTID);

  if (!progress)
    return PR_FALSE;

  progress->AddProgressListener(static_cast<nsIWebProgressListener*>(this),
                                nsIWebProgress::NOTIFY_STATE_DOCUMENT);

  return PR_TRUE;
}

void
nsAccDocManager::Shutdown()
{
  nsCOMPtr<nsIWebProgress> progress =
    do_GetService(NS_DOCUMENTLOADER_SERVICE_CONTRACTID);

  if (progress)
    progress->RemoveProgressListener(static_cast<nsIWebProgressListener*>(this));

  ClearDocCache();
}

void
nsAccDocManager::ShutdownDocAccessible(nsIDocument *aDocument)
{
  nsDocAccessible* docAccessible = mDocAccessibleCache.GetWeak(aDocument);
  if (!docAccessible)
    return;

  // We're allowed to not remove listeners when accessible document is shutdown
  // since we don't keep strong reference on chrome event target and listeners
  // are removed automatically when chrome event target goes away.

  docAccessible->Shutdown();
  mDocAccessibleCache.Remove(aDocument);
}

////////////////////////////////////////////////////////////////////////////////
// nsISupports

NS_IMPL_THREADSAFE_ISUPPORTS3(nsAccDocManager,
                              nsIWebProgressListener,
                              nsIDOMEventListener,
                              nsISupportsWeakReference)

////////////////////////////////////////////////////////////////////////////////
// nsIWebProgressListener

NS_IMETHODIMP
nsAccDocManager::OnStateChange(nsIWebProgress *aWebProgress,
                               nsIRequest *aRequest, PRUint32 aStateFlags,
                               nsresult aStatus)
{
  NS_ASSERTION(aStateFlags & STATE_IS_DOCUMENT, "Other notifications excluded");

  if (nsAccessibilityService::IsShutdown() || !aWebProgress ||
      (aStateFlags & (STATE_START | STATE_STOP)) == 0)
    return NS_OK;

  nsCOMPtr<nsIDOMWindow> DOMWindow;
  aWebProgress->GetDOMWindow(getter_AddRefs(DOMWindow));
  NS_ENSURE_STATE(DOMWindow);

  nsCOMPtr<nsIDOMDocument> DOMDocument;
  DOMWindow->GetDocument(getter_AddRefs(DOMDocument));
  NS_ENSURE_STATE(DOMDocument);

  nsCOMPtr<nsIDocument> document(do_QueryInterface(DOMDocument));

  // Document was loaded.
  if (aStateFlags & STATE_STOP) {
    NS_LOG_ACCDOCLOAD("document loaded", aWebProgress, aRequest, aStateFlags)

    // Figure out an event type to notify the document has been loaded.
    PRUint32 eventType = nsIAccessibleEvent::EVENT_DOCUMENT_LOAD_STOPPED;

    // Some XUL documents get start state and then stop state with failure
    // status when everything is ok. Fire document load complete event in this
    // case.
    if (NS_SUCCEEDED(aStatus) || !nsCoreUtils::IsContentDocument(document))
      eventType = nsIAccessibleEvent::EVENT_DOCUMENT_LOAD_COMPLETE;

    // If end consumer has been retargeted for loaded content then do not fire
    // any event because it means no new document has been loaded, for example,
    // it happens when user clicks on file link.
    if (aRequest) {
      PRUint32 loadFlags = 0;
      aRequest->GetLoadFlags(&loadFlags);
      if (loadFlags & nsIChannel::LOAD_RETARGETED_DOCUMENT_URI)
        eventType = 0;
    }

    HandleDOMDocumentLoad(document, eventType);
    return NS_OK;
  }

  // Document loading was started.
  NS_LOG_ACCDOCLOAD("start document loading", aWebProgress, aRequest,
                    aStateFlags)

  if (!IsEventTargetDocument(document))
    return NS_OK;

  nsDocAccessible* docAcc = mDocAccessibleCache.GetWeak(document);
  if (!docAcc)
    return NS_OK;

  nsCOMPtr<nsIWebNavigation> webNav(do_GetInterface(DOMWindow));
  nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(webNav));
  NS_ENSURE_STATE(docShell);

  // Fire reload and state busy events on existing document accessible while
  // event from user input flag can be calculated properly and accessible
  // is alive. When new document gets loaded then this one is destroyed.
  PRUint32 loadType;
  docShell->GetLoadType(&loadType);
  if (loadType == LOAD_RELOAD_NORMAL ||
      loadType == LOAD_RELOAD_BYPASS_CACHE ||
      loadType == LOAD_RELOAD_BYPASS_PROXY ||
      loadType == LOAD_RELOAD_BYPASS_PROXY_AND_CACHE) {

    // Fire reload event.
    nsRefPtr<AccEvent> reloadEvent =
      new AccEvent(nsIAccessibleEvent::EVENT_DOCUMENT_RELOAD, docAcc);
    nsEventShell::FireEvent(reloadEvent);
  }

  // Fire state busy change event. Use delayed event since we don't care
  // actually if event isn't delivered when the document goes away like a shot.
  nsRefPtr<AccEvent> stateEvent =
    new AccStateChangeEvent(document, nsIAccessibleStates::STATE_BUSY,
                            PR_FALSE, PR_TRUE);
  docAcc->FireDelayedAccessibleEvent(stateEvent);

  return NS_OK;
}

NS_IMETHODIMP
nsAccDocManager::OnProgressChange(nsIWebProgress *aWebProgress,
                                  nsIRequest *aRequest,
                                  PRInt32 aCurSelfProgress,
                                  PRInt32 aMaxSelfProgress,
                                  PRInt32 aCurTotalProgress,
                                  PRInt32 aMaxTotalProgress)
{
  NS_NOTREACHED("notification excluded in AddProgressListener(...)");
  return NS_OK;
}

NS_IMETHODIMP
nsAccDocManager::OnLocationChange(nsIWebProgress *aWebProgress,
                                  nsIRequest *aRequest, nsIURI *aLocation)
{
  NS_NOTREACHED("notification excluded in AddProgressListener(...)");
  return NS_OK;
}

NS_IMETHODIMP
nsAccDocManager::OnStatusChange(nsIWebProgress *aWebProgress,
                                nsIRequest *aRequest, nsresult aStatus,
                                const PRUnichar *aMessage)
{
  NS_NOTREACHED("notification excluded in AddProgressListener(...)");
  return NS_OK;
}

NS_IMETHODIMP
nsAccDocManager::OnSecurityChange(nsIWebProgress *aWebProgress,
                                  nsIRequest *aRequest,
                                  PRUint32 aState)
{
  NS_NOTREACHED("notification excluded in AddProgressListener(...)");
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsIDOMEventListener

NS_IMETHODIMP
nsAccDocManager::HandleEvent(nsIDOMEvent *aEvent)
{
  nsAutoString type;
  aEvent->GetType(type);

  nsCOMPtr<nsIDOMEventTarget> target;
  aEvent->GetTarget(getter_AddRefs(target));

  nsCOMPtr<nsIDocument> document(do_QueryInterface(target));
  NS_ASSERTION(document, "pagehide or DOMContentLoaded for non document!");
  if (!document)
    return NS_OK;

  if (type.EqualsLiteral("pagehide")) {
    // 'pagehide' event is registered on every DOM document we create an
    // accessible for, process the event for the target. This document
    // accessible and all its sub document accessible are shutdown as result of
    // processing.

    NS_LOG_ACCDOCDESTROY("received 'pagehide' event", document)

    // Ignore 'pagehide' on temporary documents since we ignore them entirely in
    // accessibility.
    if (document->IsInitialDocument())
      return NS_OK;

    // Shutdown this one and sub document accessibles.
    ShutdownDocAccessiblesInTree(document);
    return NS_OK;
  }

  // XXX: handle error pages loading separately since they get neither
  // webprogress notifications nor 'pageshow' event.
  if (type.EqualsLiteral("DOMContentLoaded") &&
      nsCoreUtils::IsErrorPage(document)) {
    NS_LOG_ACCDOCLOAD2("handled 'DOMContentLoaded' event", document)
    HandleDOMDocumentLoad(document,
                          nsIAccessibleEvent::EVENT_DOCUMENT_LOAD_COMPLETE,
                          PR_TRUE);
  }

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsAccDocManager private

void
nsAccDocManager::HandleDOMDocumentLoad(nsIDocument *aDocument,
                                       PRUint32 aLoadEventType,
                                       PRBool aMarkAsLoaded)
{
  // Document accessible can be created before we were notified the DOM document
  // was loaded completely. However if it's not created yet then create it.
  nsDocAccessible* docAcc = mDocAccessibleCache.GetWeak(aDocument);
  if (!docAcc) {
    docAcc = CreateDocOrRootAccessible(aDocument);
    NS_ASSERTION(docAcc, "Can't create document accessible!");
    if (!docAcc)
      return;
  }

  if (aMarkAsLoaded)
    docAcc->MarkAsLoaded();

  // Do not fire document complete/stop events for root chrome document
  // accessibles and for frame/iframe documents because
  // a) screen readers start working on focus event in the case of root chrome
  // documents
  // b) document load event on sub documents causes screen readers to act is if
  // entire page is reloaded.
  if (!IsEventTargetDocument(aDocument))
    return;

  // Fire complete/load stopped if the load event type is given.
  if (aLoadEventType) {
    nsRefPtr<AccEvent> loadEvent = new AccEvent(aLoadEventType, aDocument);
    docAcc->FireDelayedAccessibleEvent(loadEvent);
  }

  // Fire busy state change event.
  nsRefPtr<AccEvent> stateEvent =
    new AccStateChangeEvent(aDocument, nsIAccessibleStates::STATE_BUSY,
                            PR_FALSE, PR_FALSE);
  docAcc->FireDelayedAccessibleEvent(stateEvent);
}

PRBool
nsAccDocManager::IsEventTargetDocument(nsIDocument *aDocument) const
{
  nsCOMPtr<nsISupports> container = aDocument->GetContainer();
  nsCOMPtr<nsIDocShellTreeItem> docShellTreeItem =
    do_QueryInterface(container);
  NS_ASSERTION(docShellTreeItem, "No document shell for document!");

  nsCOMPtr<nsIDocShellTreeItem> parentTreeItem;
  docShellTreeItem->GetParent(getter_AddRefs(parentTreeItem));

  // It's not a root document.
  if (parentTreeItem) {
    nsCOMPtr<nsIDocShellTreeItem> sameTypeRoot;
    docShellTreeItem->GetSameTypeRootTreeItem(getter_AddRefs(sameTypeRoot));

    // It's not a sub document, i.e. a frame or iframe.
    return (sameTypeRoot == docShellTreeItem);
  }

  // It's not chrome root document.
  PRInt32 contentType;
  docShellTreeItem->GetItemType(&contentType);
  return (contentType == nsIDocShellTreeItem::typeContent);
}

void
nsAccDocManager::AddListeners(nsIDocument *aDocument,
                              PRBool aAddDOMContentLoadedListener)
{
  nsPIDOMWindow *window = aDocument->GetWindow();
  nsPIDOMEventTarget *target = window->GetChromeEventHandler();
  nsIEventListenerManager* elm = target->GetListenerManager(PR_TRUE);
  elm->AddEventListenerByType(this, NS_LITERAL_STRING("pagehide"),
                              NS_EVENT_FLAG_CAPTURE, nsnull);

  NS_LOG_ACCDOCCREATE_TEXT("  added 'pagehide' listener")

  if (aAddDOMContentLoadedListener) {
    elm->AddEventListenerByType(this, NS_LITERAL_STRING("DOMContentLoaded"),
                                NS_EVENT_FLAG_CAPTURE, nsnull);
    NS_LOG_ACCDOCCREATE_TEXT("  added 'DOMContentLoaded' listener")
  }
}

nsDocAccessible*
nsAccDocManager::CreateDocOrRootAccessible(nsIDocument *aDocument)
{
  // Ignore temporary, hiding, resource documents and documents without
  // docshell.
  if (aDocument->IsInitialDocument() || !aDocument->IsVisible() ||
      aDocument->IsResourceDoc() || !aDocument->IsActive())
    return nsnull;

  // Ignore documents without presshell.
  nsIPresShell *presShell = aDocument->GetShell();
  if (!presShell)
    return nsnull;

  // Do not create document accessible until role content is loaded, otherwise
  // we get accessible document with wrong role.
  nsIContent *rootElm = nsCoreUtils::GetRoleContent(aDocument);
  if (!rootElm)
    return nsnull;

  PRBool isRootDoc = nsCoreUtils::IsRootDocument(aDocument);

  // Ensure the document container node is accessible, otherwise do not create
  // document accessible.
  nsAccessible *outerDocAcc = nsnull;
  if (isRootDoc) {
    outerDocAcc = nsAccessNode::GetApplicationAccessible();

  } else {
    nsIDocument* parentDoc = aDocument->GetParentDocument();
    if (!parentDoc)
      return nsnull;

    nsIContent* ownerContent = parentDoc->FindContentForSubDocument(aDocument);
    if (!ownerContent)
      return nsnull;

    // XXXaaronl: ideally we would traverse the presshell chain. Since there's
    // no easy way to do that, we cheat and use the document hierarchy.
    // GetAccessible() is bad because it doesn't support our concept of multiple
    // presshells per doc. It should be changed to use
    // GetAccessibleInWeakShell().
    outerDocAcc = GetAccService()->GetAccessible(ownerContent);
  }

  if (!outerDocAcc)
    return nsnull;

  // We only create root accessibles for the true root, otherwise create a
  // doc accessible.
  nsCOMPtr<nsIWeakReference> weakShell(do_GetWeakReference(presShell));
  nsDocAccessible *docAcc = isRootDoc ?
    new nsRootAccessibleWrap(aDocument, rootElm, weakShell) :
    new nsDocAccessibleWrap(aDocument, rootElm, weakShell);

  if (!docAcc)
    return nsnull;

  // Cache and addref document accessible.
  if (!mDocAccessibleCache.Put(aDocument, docAcc)) {
    delete docAcc;
    return nsnull;
  }

  // XXX: ideally we should initialize an accessible and then put it into tree,
  // we can't since document accessible fires reorder event on its container
  // while initialized.
  if (!outerDocAcc->AppendChild(docAcc) ||
      !GetAccService()->InitAccessible(docAcc, nsAccUtils::GetRoleMapEntry(aDocument))) {
    mDocAccessibleCache.Remove(aDocument);
    return nsnull;
  }

  NS_LOG_ACCDOCCREATE("document creation finished", aDocument)

  AddListeners(aDocument, isRootDoc);
  return docAcc;
}

void
nsAccDocManager::ShutdownDocAccessiblesInTree(nsIDocShellTreeItem *aTreeItem,
                                              nsIDocument *aDocument)
{
  nsCOMPtr<nsIDocShellTreeNode> treeNode(do_QueryInterface(aTreeItem));

  if (treeNode) {
    PRInt32 subDocumentsCount = 0;
    treeNode->GetChildCount(&subDocumentsCount);
    for (PRInt32 idx = 0; idx < subDocumentsCount; idx++) {
      nsCOMPtr<nsIDocShellTreeItem> treeItemChild;
      treeNode->GetChildAt(idx, getter_AddRefs(treeItemChild));
      NS_ASSERTION(treeItemChild, "No tree item when there should be");
      if (!treeItemChild)
        continue;

      nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(treeItemChild));
      nsCOMPtr<nsIContentViewer> contentViewer;
      docShell->GetContentViewer(getter_AddRefs(contentViewer));
      if (!contentViewer)
        continue;

      ShutdownDocAccessiblesInTree(treeItemChild, contentViewer->GetDocument());
    }
  }

  ShutdownDocAccessible(aDocument);
}

////////////////////////////////////////////////////////////////////////////////
// nsAccDocManager static

PLDHashOperator
nsAccDocManager::ClearDocCacheEntry(const nsIDocument* aKey,
                                    nsRefPtr<nsDocAccessible>& aDocAccessible,
                                    void* aUserArg)
{
  NS_ASSERTION(aDocAccessible,
               "Calling ClearDocCacheEntry with a NULL pointer!");

  if (aDocAccessible)
    aDocAccessible->Shutdown();

  return PL_DHASH_REMOVE;
}

PLDHashOperator
nsAccDocManager::SearchAccessibleInDocCache(const nsIDocument* aKey,
                                            nsDocAccessible* aDocAccessible,
                                            void* aUserArg)
{
  NS_ASSERTION(aDocAccessible,
               "No doc accessible for the object in doc accessible cache!");

  if (aDocAccessible) {
    nsSearchAccessibleInCacheArg* arg =
      static_cast<nsSearchAccessibleInCacheArg*>(aUserArg);
    arg->mAccessible = aDocAccessible->GetCachedAccessible(arg->mNode);
    if (arg->mAccessible)
      return PL_DHASH_STOP;
  }

  return PL_DHASH_NEXT;
}
