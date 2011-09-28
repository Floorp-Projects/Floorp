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

#ifndef nsAccDocManager_h_
#define nsAccDocManager_h_

#include "nsIDocument.h"
#include "nsIDOMEventListener.h"
#include "nsRefPtrHashtable.h"
#include "nsIWebProgress.h"
#include "nsIWebProgressListener.h"
#include "nsWeakReference.h"

class nsAccessible;
class nsDocAccessible;

//#define DEBUG_ACCDOCMGR

/**
 * Manage the document accessible life cycle.
 */
class nsAccDocManager : public nsIWebProgressListener,
                        public nsIDOMEventListener,
                        public nsSupportsWeakReference
{
public:
  virtual ~nsAccDocManager() { };

  NS_DECL_ISUPPORTS
  NS_DECL_NSIWEBPROGRESSLISTENER
  NS_DECL_NSIDOMEVENTLISTENER

  /**
   * Return document accessible for the given DOM node.
   */
  nsDocAccessible *GetDocAccessible(nsIDocument *aDocument);

  /**
   * Search through all document accessibles for an accessible with the given
   * unique id.
   */
  nsAccessible* FindAccessibleInCache(nsINode* aNode) const;

  /**
   * Return document accessible from the cache. Convenient method for testing.
   */
  inline nsDocAccessible* GetDocAccessibleFromCache(nsIDocument* aDocument) const
  {
    return mDocAccessibleCache.GetWeak(aDocument);
  }

  /**
   * Called by document accessible when it gets shutdown.
   */
  inline void NotifyOfDocumentShutdown(nsIDocument* aDocument)
  {
    mDocAccessibleCache.Remove(aDocument);
  }

protected:
  nsAccDocManager() { };

  /**
   * Initialize the manager.
   */
  PRBool Init();

  /**
   * Shutdown the manager.
   */
  void Shutdown();

private:
  nsAccDocManager(const nsAccDocManager&);
  nsAccDocManager& operator =(const nsAccDocManager&);

private:
  /**
   * Create an accessible document if it was't created and fire accessibility
   * events if needed.
   *
   * @param  aDocument       [in] loaded DOM document
   * @param  aLoadEventType  [in] specifies the event type to fire load event,
   *                           if 0 then no event is fired
   */
  void HandleDOMDocumentLoad(nsIDocument *aDocument,
                             PRUint32 aLoadEventType);

  /**
   * Add 'pagehide' and 'DOMContentLoaded' event listeners.
   */
  void AddListeners(nsIDocument *aDocument, PRBool aAddPageShowListener);

  /**
   * Create document or root accessible.
   */
  nsDocAccessible *CreateDocOrRootAccessible(nsIDocument *aDocument);

  typedef nsRefPtrHashtable<nsPtrHashKey<const nsIDocument>, nsDocAccessible>
    nsDocAccessibleHashtable;

  /**
   * Get first entry of the document accessible from cache.
   */
  static PLDHashOperator
    GetFirstEntryInDocCache(const nsIDocument* aKey,
                            nsDocAccessible* aDocAccessible,
                            void* aUserArg);

  /**
   * Clear the cache and shutdown the document accessibles.
   */
  void ClearDocCache();

  struct nsSearchAccessibleInCacheArg
  {
    nsAccessible *mAccessible;
    nsINode* mNode;
  };

  static PLDHashOperator
    SearchAccessibleInDocCache(const nsIDocument* aKey,
                               nsDocAccessible* aDocAccessible,
                               void* aUserArg);

  nsDocAccessibleHashtable mDocAccessibleCache;
};

/**
 * nsAccDocManager debugging macros.
 */
#ifdef DEBUG_ACCDOCMGR

#include "nsTraceRefcntImpl.h"

// Enable these to log accessible document loading, creation or destruction.
#define DEBUG_ACCDOCMGR_DOCLOAD
#define DEBUG_ACCDOCMGR_DOCCREATE
#define DEBUG_ACCDOCMGR_DOCDESTROY

// Common macros, do not use directly.
#define NS_LOG_ACCDOC_ADDRESS(aDocument, aDocAcc)                              \
  printf("DOM id: %p, acc id: %p", aDocument, aDocAcc);

#define NS_LOG_ACCDOC_URI(aDocument)                                           \
  nsIURI *uri = aDocument->GetDocumentURI();                                   \
  nsCAutoString spec;                                                          \
  uri->GetSpec(spec);                                                          \
  printf("uri: %s", spec);

#define NS_LOG_ACCDOC_TYPE(aDocument)                                          \
  if (aDocument->IsActive()) {                                                 \
    PRBool isContent = nsCoreUtils::IsContentDocument(aDocument);              \
    printf("%s document", (isContent ? "content" : "chrome"));                 \
  } else {                                                                     \
    printf("document type: [failed]");                                         \
  }

#define NS_LOG_ACCDOC_DOCSHELLTREE(aDocument)                                  \
  if (aDocument->IsActive()) {                                                 \
    nsCOMPtr<nsISupports> container = aDocument->GetContainer();               \
    nsCOMPtr<nsIDocShellTreeItem> treeItem(do_QueryInterface(container));      \
    nsCOMPtr<nsIDocShellTreeItem> parentTreeItem;                              \
    treeItem->GetParent(getter_AddRefs(parentTreeItem));                       \
    nsCOMPtr<nsIDocShellTreeItem> rootTreeItem;                                \
    treeItem->GetRootTreeItem(getter_AddRefs(rootTreeItem));                   \
    printf("docshell hierarchy, parent: %p, root: %p, is tab document: %s;",   \
           parentTreeItem, rootTreeItem,                                       \
           (nsCoreUtils::IsTabDocument(aDocument) ? "yes" : "no"));            \
  }

#define NS_LOG_ACCDOC_SHELLSTATE(aDocument)                                    \
  nsCAutoString docShellBusy;                                                  \
  nsCOMPtr<nsISupports> container = aDocument->GetContainer();                 \
  if (container) {                                                             \
    nsCOMPtr<nsIDocShell> docShell = do_QueryInterface(container);             \
    PRUint32 busyFlags = nsIDocShell::BUSY_FLAGS_NONE;                         \
    docShell->GetBusyFlags(&busyFlags);                                        \
    if (busyFlags == nsIDocShell::BUSY_FLAGS_NONE)                             \
      docShellBusy.AppendLiteral("'none'");                                    \
    if (busyFlags & nsIDocShell::BUSY_FLAGS_BUSY)                              \
      docShellBusy.AppendLiteral("'busy'");                                    \
    if (busyFlags & nsIDocShell::BUSY_FLAGS_BEFORE_PAGE_LOAD)                  \
      docShellBusy.AppendLiteral(", 'before page load'");                      \
    if (busyFlags & nsIDocShell::BUSY_FLAGS_PAGE_LOADING)                      \
      docShellBusy.AppendLiteral(", 'page loading'");                          \
  }                                                                            \
  else {                                                                       \
    docShellBusy.AppendLiteral("[failed]");                                    \
  }                                                                            \
  printf("docshell busy: %s", docShellBusy.get());

#define NS_LOG_ACCDOC_DOCSTATES(aDocument)                                     \
  const char *docState = 0;                                                    \
  nsIDocument::ReadyState docStateFlag = aDocument->GetReadyStateEnum();       \
  switch (docStateFlag) {                                                      \
    case nsIDocument::READYSTATE_UNINITIALIZED:                                \
     docState = "uninitialized";                                               \
      break;                                                                   \
    case nsIDocument::READYSTATE_LOADING:                                      \
      docState = "loading";                                                    \
      break;                                                                   \
    case nsIDocument::READYSTATE_INTERACTIVE:                                  \
      docState = "interactive";                                                \
      break;                                                                   \
    case nsIDocument::READYSTATE_COMPLETE:                                     \
      docState = "complete";                                                   \
      break;                                                                   \
  }                                                                            \
  printf("doc state: %s", docState);                                           \
  printf(", %sinitial", aDocument->IsInitialDocument() ? "" : "not ");         \
  printf(", %sshowing", aDocument->IsShowing() ? "" : "not ");                 \
  printf(", %svisible", aDocument->IsVisible() ? "" : "not ");                 \
  printf(", %sactive", aDocument->IsActive() ? "" : "not ");

#define NS_LOG_ACCDOC_DOCPRESSHELL(aDocument)                                  \
  nsIPresShell *ps = aDocument->GetShell();                                    \
  printf("presshell: %p", ps);                                                 \
  nsIScrollableFrame *sf = ps ?                                                \
    ps->GetRootScrollFrameAsScrollableExternal() : nsnull;                     \
  printf(", root scroll frame: %p", sf);

#define NS_LOG_ACCDOC_DOCLOADGROUP(aDocument)                                  \
  nsCOMPtr<nsILoadGroup> loadGroup = aDocument->GetDocumentLoadGroup();        \
  printf("load group: %p", loadGroup);

#define NS_LOG_ACCDOC_DOCPARENT(aDocument)                                     \
  nsIDocument *parentDoc = aDocument->GetParentDocument();                     \
  printf("parent id: %p", parentDoc);                                          \
  if (parentDoc) {                                                             \
    printf("\n    parent ");                                                   \
    NS_LOG_ACCDOC_URI(parentDoc)                                               \
    printf("\n");                                                              \
  }

#define NS_LOG_ACCDOC_SHELLLOADTYPE(aDocShell)                                 \
  {                                                                            \
    printf("load type: ");                                                     \
    PRUint32 loadType;                                                         \
    docShell->GetLoadType(&loadType);                                          \
    switch (loadType) {                                                        \
      case LOAD_NORMAL:                                                        \
        printf("normal; ");                                                    \
        break;                                                                 \
      case LOAD_NORMAL_REPLACE:                                                \
        printf("normal replace; ");                                            \
        break;                                                                 \
      case LOAD_NORMAL_EXTERNAL:                                               \
        printf("normal external; ");                                           \
        break;                                                                 \
      case LOAD_HISTORY:                                                       \
        printf("history; ");                                                   \
        break;                                                                 \
      case LOAD_NORMAL_BYPASS_CACHE:                                           \
        printf("normal bypass cache; ");                                       \
        break;                                                                 \
      case LOAD_NORMAL_BYPASS_PROXY:                                           \
        printf("normal bypass proxy; ");                                       \
        break;                                                                 \
      case LOAD_NORMAL_BYPASS_PROXY_AND_CACHE:                                 \
        printf("normal bypass proxy and cache; ");                             \
        break;                                                                 \
      case LOAD_RELOAD_NORMAL:                                                 \
        printf("reload normal; ");                                             \
        break;                                                                 \
      case LOAD_RELOAD_BYPASS_CACHE:                                           \
        printf("reload bypass cache; ");                                       \
        break;                                                                 \
      case LOAD_RELOAD_BYPASS_PROXY:                                           \
        printf("reload bypass proxy; ");                                       \
        break;                                                                 \
      case LOAD_RELOAD_BYPASS_PROXY_AND_CACHE:                                 \
        printf("reload bypass proxy and cache; ");                             \
        break;                                                                 \
      case LOAD_LINK:                                                          \
        printf("link; ");                                                      \
        break;                                                                 \
      case LOAD_REFRESH:                                                       \
        printf("refresh; ");                                                   \
        break;                                                                 \
      case LOAD_RELOAD_CHARSET_CHANGE:                                         \
        printf("reload charset change; ");                                     \
        break;                                                                 \
      case LOAD_BYPASS_HISTORY:                                                \
        printf("bypass history; ");                                            \
        break;                                                                 \
      case LOAD_STOP_CONTENT:                                                  \
        printf("stop content; ");                                              \
        break;                                                                 \
      case LOAD_STOP_CONTENT_AND_REPLACE:                                      \
        printf("stop content and replace; ");                                  \
        break;                                                                 \
      case LOAD_PUSHSTATE:                                                     \
        printf("load pushstate; ");                                            \
        break;                                                                 \
      case LOAD_ERROR_PAGE:                                                    \
        printf("error page;");                                                 \
        break;                                                                 \
      default:                                                                 \
        printf("unknown");                                                     \
    }                                                                          \
  }

#define NS_LOG_ACCDOC_DOCINFO_BEGIN                                            \
  printf("  {\n");
#define NS_LOG_ACCDOC_DOCINFO_BODY(aDocument, aDocAcc)                         \
  {                                                                            \
    printf("    ");                                                            \
    NS_LOG_ACCDOC_ADDRESS(aDocument, aDocAcc)                                  \
    printf("\n    ");                                                          \
    if (aDocument) {                                                           \
      NS_LOG_ACCDOC_URI(aDocument)                                             \
      printf("\n    ");                                                        \
      NS_LOG_ACCDOC_SHELLSTATE(aDocument)                                      \
      printf("; ");                                                            \
      NS_LOG_ACCDOC_TYPE(aDocument)                                            \
      printf("\n    ");                                                        \
      NS_LOG_ACCDOC_DOCSHELLTREE(aDocument)                                    \
      printf("\n    ");                                                        \
      NS_LOG_ACCDOC_DOCSTATES(aDocument)                                       \
      printf("\n    ");                                                        \
      NS_LOG_ACCDOC_DOCPRESSHELL(aDocument)                                    \
      printf("\n    ");                                                        \
      NS_LOG_ACCDOC_DOCLOADGROUP(aDocument)                                    \
      printf(", ");                                                            \
      NS_LOG_ACCDOC_DOCPARENT(aDocument)                                       \
      printf("\n");                                                            \
    }                                                                          \
  }
#define NS_LOG_ACCDOC_DOCINFO_END                                              \
  printf("  }\n");

#define NS_LOG_ACCDOC_DOCINFO(aDocument, aDocAcc)                              \
  NS_LOG_ACCDOC_DOCINFO_BEGIN                                                  \
  NS_LOG_ACCDOC_DOCINFO_BODY(aDocument, aDocAcc)                               \
  NS_LOG_ACCDOC_DOCINFO_END

#define NS_GET_ACCDOC_EVENTTYPE(aEvent)                                        \
  nsCAutoString strEventType;                                                  \
  PRUint32 type = aEvent->GetEventType();                                      \
  if (type == nsIAccessibleEvent::EVENT_DOCUMENT_LOAD_STOPPED) {               \
    strEventType.AssignLiteral("load stopped");                                \
  } else if (type == nsIAccessibleEvent::EVENT_DOCUMENT_LOAD_COMPLETE) {       \
    strEventType.AssignLiteral("load complete");                               \
  } else if (type == nsIAccessibleEvent::EVENT_DOCUMENT_RELOAD) {              \
      strEventType.AssignLiteral("reload");                                    \
  } else if (type == nsIAccessibleEvent::EVENT_STATE_CHANGE) {                 \
    AccStateChangeEvent* event = downcast_accEvent(aEvent);                    \
    if (event->GetState() == states::BUSY) {                                   \
      strEventType.AssignLiteral("busy ");                                     \
      if (event->IsStateEnabled())                                             \
        strEventType.AppendLiteral("true");                                    \
      else                                                                     \
        strEventType.AppendLiteral("false");                                   \
    }                                                                          \
  }

#define NS_LOG_ACCDOC_ACCADDRESS(aName, aAcc)                                  \
  {                                                                            \
    nsINode* node = aAcc->GetNode();                                           \
    nsIDocument* doc = aAcc->GetDocumentNode();                                \
    nsDocAccessible *docacc = GetAccService()->GetDocAccessibleFromCache(doc); \
    printf("  " aName " accessible: %p, node: %p\n", aAcc, node);              \
    printf("  docacc for " aName " accessible: %p, node: %p\n", docacc, doc);  \
    printf("  ");                                                              \
    NS_LOG_ACCDOC_URI(doc)                                                     \
    printf("\n");                                                              \
  }

#define NS_LOG_ACCDOC_MSG(aMsg)                                                \
  printf("\n" aMsg "\n");                                                      \

#define NS_LOG_ACCDOC_TEXT(aMsg)                                               \
  printf("  " aMsg "\n");

#define NS_LOG_ACCDOC_STACK                                                    \
  printf("  stack: \n");                                                       \
  nsTraceRefcntImpl::WalkTheStack(stdout);

// Accessible document loading macros.
#ifdef DEBUG_ACCDOCMGR_DOCLOAD

#define NS_LOG_ACCDOCLOAD_REQUEST(aRequest)                                    \
  if (aRequest) {                                                              \
    nsCAutoString name;                                                        \
    aRequest->GetName(name);                                                   \
    printf("    request spec: %s\n", name.get());                              \
    PRUint32 loadFlags = 0;                                                    \
    aRequest->GetLoadFlags(&loadFlags);                                        \
    printf("    request load flags: %x; ", loadFlags);                         \
    if (loadFlags & nsIChannel::LOAD_DOCUMENT_URI)                             \
      printf("document uri; ");                                                \
    if (loadFlags & nsIChannel::LOAD_RETARGETED_DOCUMENT_URI)                  \
      printf("retargeted document uri; ");                                     \
    if (loadFlags & nsIChannel::LOAD_REPLACE)                                  \
      printf("replace; ");                                                     \
    if (loadFlags & nsIChannel::LOAD_INITIAL_DOCUMENT_URI)                     \
      printf("initial document uri; ");                                        \
    if (loadFlags & nsIChannel::LOAD_TARGETED)                                 \
      printf("targeted; ");                                                    \
    if (loadFlags & nsIChannel::LOAD_CALL_CONTENT_SNIFFERS)                    \
      printf("call content sniffers; ");                                       \
    if (loadFlags & nsIChannel::LOAD_CLASSIFY_URI)                             \
      printf("classify uri; ");                                                \
  } else {                                                                     \
    printf("    no request");                                                  \
  }

#define NS_LOG_ACCDOCLOAD(aMsg, aWebProgress, aRequest, aStateFlags)           \
  {                                                                            \
    NS_LOG_ACCDOC_MSG("A11Y DOCLOAD: " aMsg);                                  \
                                                                               \
    nsCOMPtr<nsIDOMWindow> DOMWindow;                                          \
    aWebProgress->GetDOMWindow(getter_AddRefs(DOMWindow));                     \
    if (DOMWindow) {                                                           \
      nsCOMPtr<nsIDOMDocument> DOMDocument;                                    \
      DOMWindow->GetDocument(getter_AddRefs(DOMDocument));                     \
      if (DOMDocument) {                                                       \
        nsCOMPtr<nsIDocument> document(do_QueryInterface(DOMDocument));        \
        nsDocAccessible *docAcc =                                              \
          GetAccService()->GetDocAccessibleFromCache(document);                \
        NS_LOG_ACCDOC_DOCINFO(document, docAcc)                                \
                                                                               \
        printf("  {\n");                                                       \
        nsCOMPtr<nsIWebNavigation> webNav(do_GetInterface(DOMWindow));         \
        nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(webNav));             \
        printf("    ");                                                        \
        NS_LOG_ACCDOC_SHELLLOADTYPE(docShell)                                  \
        printf("\n");                                                          \
        NS_LOG_ACCDOCLOAD_REQUEST(aRequest)                                    \
        printf("\n");                                                          \
        printf("    state flags: %x", aStateFlags);                            \
        PRBool isDocLoading;                                                   \
        aWebProgress->GetIsLoadingDocument(&isDocLoading);                     \
        printf(", document is %sloading\n", (isDocLoading ? "" : "not "));     \
        printf("  }\n");                                                       \
      }                                                                        \
    }                                                                          \
  }

#define NS_LOG_ACCDOCLOAD2(aMsg, aDocument)                                    \
  {                                                                            \
    NS_LOG_ACCDOC_MSG("A11Y DOCLOAD: " aMsg);                                  \
    nsDocAccessible *docAcc =                                                  \
      GetAccService()->GetDocAccessibleFromCache(aDocument);                   \
    NS_LOG_ACCDOC_DOCINFO(aDocument, docAcc)                                   \
  }

#define NS_LOG_ACCDOCLOAD_FIREEVENT(aEvent)                                    \
  {                                                                            \
    NS_GET_ACCDOC_EVENTTYPE(aEvent)                                            \
    if (!strEventType.IsEmpty())                                               \
      printf("  fire: %s\n", strEventType.get());                              \
  }

#define NS_LOG_ACCDOCLOAD_HANDLEEVENT(aEvent)                                  \
  {                                                                            \
    NS_GET_ACCDOC_EVENTTYPE(aEvent)                                            \
    nsCOMPtr<nsIDocument> doc(do_QueryInterface(aEvent->GetNode()));           \
    if (doc && !strEventType.IsEmpty()) {                                      \
      printf("\nA11Y DOCEVENT: handled '%s' event ", strEventType.get());      \
      nsDocAccessible *docAcc = aEvent->GetDocAccessible();                    \
      NS_LOG_ACCDOC_DOCINFO(doc, docAcc)                                       \
      printf("\n");                                                            \
    }                                                                          \
  }

#define NS_LOG_ACCDOCLOAD_TEXT(aMsg)                                           \
    NS_LOG_ACCDOC_TEXT(aMsg)

#endif // DEBUG_ACCDOCMGR_DOCLOAD

// Accessible document creation macros.
#ifdef DEBUG_ACCDOCMGR_DOCCREATE
#define NS_LOG_ACCDOCCREATE_FOR(aMsg, aDocument, aDocAcc)                      \
  NS_LOG_ACCDOC_MSG("A11Y DOCCREATE: " aMsg);                                  \
  NS_LOG_ACCDOC_DOCINFO(aDocument, aDocAcc)

#define NS_LOG_ACCDOCCREATE(aMsg, aDocument)                                   \
  {                                                                            \
    nsDocAccessible *docAcc =                                                  \
      GetAccService()->GetDocAccessibleFromCache(aDocument);                   \
    NS_LOG_ACCDOCCREATE_FOR(aMsg, aDocument, docAcc)                           \
  }

#define NS_LOG_ACCDOCCREATE_ACCADDRESS(aName, aAcc)                            \
  NS_LOG_ACCDOC_ACCADDRESS(aName, aAcc)

#define NS_LOG_ACCDOCCREATE_TEXT(aMsg)                                         \
  NS_LOG_ACCDOC_TEXT(aMsg)

#define NS_LOG_ACCDOCCREATE_STACK                                              \
  NS_LOG_ACCDOC_STACK

#endif // DEBUG_ACCDOCMGR_DOCCREATE

// Accessible document destruction macros.
#ifdef DEBUG_ACCDOCMGR_DOCDESTROY
#define NS_LOG_ACCDOCDESTROY_FOR(aMsg, aDocument, aDocAcc)                     \
  NS_LOG_ACCDOC_MSG("A11Y DOCDESTROY: " aMsg);                                 \
  NS_LOG_ACCDOC_DOCINFO(aDocument, aDocAcc)

#define NS_LOG_ACCDOCDESTROY(aMsg, aDocument)                                  \
  {                                                                            \
    nsDocAccessible* docAcc =                                                  \
      GetAccService()->GetDocAccessibleFromCache(aDocument);                   \
    NS_LOG_ACCDOCDESTROY_FOR(aMsg, aDocument, docAcc)                          \
  }

#define NS_LOG_ACCDOCDESTROY_ACCADDRESS(aName, aAcc)                           \
  NS_LOG_ACCDOC_ACCADDRESS(aName, aAcc)

#define NS_LOG_ACCDOCDESTROY_MSG(aMsg)                                         \
  NS_LOG_ACCDOC_MSG(aMsg)

#define NS_LOG_ACCDOCDESTROY_TEXT(aMsg)                                        \
  NS_LOG_ACCDOC_TEXT(aMsg)

#endif // DEBUG_ACCDOCMGR_DOCDESTROY

#endif // DEBUG_ACCDOCMGR

#ifndef DEBUG_ACCDOCMGR_DOCLOAD
#define NS_LOG_ACCDOCLOAD(aMsg, aWebProgress, aRequest, aStateFlags)
#define NS_LOG_ACCDOCLOAD2(aMsg, aDocument)
#define NS_LOG_ACCDOCLOAD_EVENT(aMsg, aEvent)
#define NS_LOG_ACCDOCLOAD_FIREEVENT(aEvent)
#define NS_LOG_ACCDOCLOAD_HANDLEEVENT(aEvent)
#define NS_LOG_ACCDOCLOAD_TEXT(aMsg)
#endif

#ifndef DEBUG_ACCDOCMGR_DOCCREATE
#define NS_LOG_ACCDOCCREATE_FOR(aMsg, aDocument, aDocAcc)
#define NS_LOG_ACCDOCCREATE(aMsg, aDocument)
#define NS_LOG_ACCDOCCREATE_ACCADDRESS(aName, aAcc)
#define NS_LOG_ACCDOCCREATE_TEXT(aMsg)
#define NS_LOG_ACCDOCCREATE_STACK
#endif

#ifndef DEBUG_ACCDOCMGR_DOCDESTROY
#define NS_LOG_ACCDOCDESTROY_FOR(aMsg, aDocument, aDocAcc)
#define NS_LOG_ACCDOCDESTROY(aMsg, aDocument)
#define NS_LOG_ACCDOCDESTROY_MSG(aMsg)
#define NS_LOG_ACCDOCDESTROY_ACCADDRESS(aName, aAcc)
#define NS_LOG_ACCDOCDESTROY_TEXT(aMsg)
#endif

#endif // nsAccDocManager_h_
