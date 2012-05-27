/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Logging.h"

#include "AccEvent.h"
#include "nsAccessibilityService.h"
#include "nsCoreUtils.h"
#include "DocAccessible.h"

#include "nsDocShellLoadTypes.h"
#include "nsIChannel.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsTraceRefcntImpl.h"
#include "nsIWebProgress.h"
#include "prenv.h"

using namespace mozilla;
using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// Logging helpers

static PRUint32 sModules = 0;

struct ModuleRep {
  const char* mStr;
  logging::EModules mModule;
};

static void
EnableLogging(const char* aModulesStr)
{
  sModules = 0;
  if (!aModulesStr)
    return;

  static ModuleRep modules[] = {
    { "docload", logging::eDocLoad },
    { "doccreate", logging::eDocCreate },
    { "docdestroy", logging::eDocDestroy },
    { "doclifecycle", logging::eDocLifeCycle }
  };

  const char* token = aModulesStr;
  while (*token != '\0') {
    size_t tokenLen = strcspn(token, ",");
    for (unsigned int idx = 0; idx < ArrayLength(modules); idx++) {
      if (strncmp(token, modules[idx].mStr, tokenLen) == 0) {
        sModules |= modules[idx].mModule;
        printf("\n\nmodule enabled: %s\n", modules[idx].mStr);
        break;
      }
    }
    token += tokenLen;

    if (*token == ',')
      token++; // skip ',' char
  }
}

static void
LogDocURI(nsIDocument* aDocumentNode)
{
  nsIURI* uri = aDocumentNode->GetDocumentURI();
  nsCAutoString spec;
  uri->GetSpec(spec);
  printf("uri: %s", spec.get());
}

static void
LogDocShellState(nsIDocument* aDocumentNode)
{
  printf("docshell busy: ");

  nsCAutoString docShellBusy;
  nsCOMPtr<nsISupports> container = aDocumentNode->GetContainer();
  if (container) {
    nsCOMPtr<nsIDocShell> docShell = do_QueryInterface(container);
    PRUint32 busyFlags = nsIDocShell::BUSY_FLAGS_NONE;
    docShell->GetBusyFlags(&busyFlags);
    if (busyFlags == nsIDocShell::BUSY_FLAGS_NONE)
      printf("'none'");
    if (busyFlags & nsIDocShell::BUSY_FLAGS_BUSY)
      printf("'busy'");
    if (busyFlags & nsIDocShell::BUSY_FLAGS_BEFORE_PAGE_LOAD)
      printf(", 'before page load'");
    if (busyFlags & nsIDocShell::BUSY_FLAGS_PAGE_LOADING)
      printf(", 'page loading'");
  } else {
    printf("[failed]");
  }
}

static void
LogDocType(nsIDocument* aDocumentNode)
{
  if (aDocumentNode->IsActive()) {
    bool isContent = nsCoreUtils::IsContentDocument(aDocumentNode);
    printf("%s document", (isContent ? "content" : "chrome"));
  } else {
    printf("document type: [failed]");\
  }
}

static void
LogDocShellTree(nsIDocument* aDocumentNode)
{
  if (aDocumentNode->IsActive()) {
    nsCOMPtr<nsISupports> container = aDocumentNode->GetContainer();
    nsCOMPtr<nsIDocShellTreeItem> treeItem(do_QueryInterface(container));
    nsCOMPtr<nsIDocShellTreeItem> parentTreeItem;
    treeItem->GetParent(getter_AddRefs(parentTreeItem));
    nsCOMPtr<nsIDocShellTreeItem> rootTreeItem;
    treeItem->GetRootTreeItem(getter_AddRefs(rootTreeItem));
    printf("docshell hierarchy, parent: %p, root: %p, is tab document: %s;",
           static_cast<void*>(parentTreeItem), static_cast<void*>(rootTreeItem),
           (nsCoreUtils::IsTabDocument(aDocumentNode) ? "yes" : "no"));
  }
}

static void
LogDocState(nsIDocument* aDocumentNode)
{
  const char* docState = nsnull;
  nsIDocument::ReadyState docStateFlag = aDocumentNode->GetReadyStateEnum();
  switch (docStateFlag) {
    case nsIDocument::READYSTATE_UNINITIALIZED:
     docState = "uninitialized";
      break;
    case nsIDocument::READYSTATE_LOADING:
      docState = "loading";
      break;
    case nsIDocument::READYSTATE_INTERACTIVE:
      docState = "interactive";
      break;
    case nsIDocument::READYSTATE_COMPLETE:
      docState = "complete";
      break;
  }

  printf("doc state: %s", docState);
  printf(", %sinitial", aDocumentNode->IsInitialDocument() ? "" : "not ");
  printf(", %sshowing", aDocumentNode->IsShowing() ? "" : "not ");
  printf(", %svisible", aDocumentNode->IsVisible() ? "" : "not ");
  printf(", %sactive", aDocumentNode->IsActive() ? "" : "not ");
}

static void
LogPresShell(nsIDocument* aDocumentNode)
{
  nsIPresShell* ps = aDocumentNode->GetShell();
  printf("presshell: %p", static_cast<void*>(ps));
  nsIScrollableFrame *sf = ps ?
    ps->GetRootScrollFrameAsScrollableExternal() : nsnull;
  printf(", root scroll frame: %p", static_cast<void*>(sf));
}

static void
LogDocLoadGroup(nsIDocument* aDocumentNode)
{
  nsCOMPtr<nsILoadGroup> loadGroup = aDocumentNode->GetDocumentLoadGroup();
  printf("load group: %p", static_cast<void*>(loadGroup));
}

static void
LogDocParent(nsIDocument* aDocumentNode)
{
  nsIDocument* parentDoc = aDocumentNode->GetParentDocument();
  printf("parent id: %p", static_cast<void*>(parentDoc));
  if (parentDoc) {
    printf("\n    parent ");
    LogDocURI(parentDoc);
    printf("\n");
  }
}

static void
LogDocInfo(nsIDocument* aDocumentNode, DocAccessible* aDocument)
{
  printf("  {\n");

  printf("    DOM id: %p, acc id: %p\n    ",
         static_cast<void*>(aDocumentNode), static_cast<void*>(aDocument));

  // log document info
  if (aDocumentNode) {
    LogDocURI(aDocumentNode);
    printf("\n    ");
    LogDocShellState(aDocumentNode);
    printf("; ");
    LogDocType(aDocumentNode);
    printf("\n    ");
    LogDocShellTree(aDocumentNode);
    printf("\n    ");
    LogDocState(aDocumentNode);
    printf("\n    ");
    LogPresShell(aDocumentNode);
    printf("\n    ");
    LogDocLoadGroup(aDocumentNode);
    printf(", ");
    LogDocParent(aDocumentNode);
    printf("\n");
  }

  printf("  }\n");
}

static void
LogShellLoadType(nsIDocShell* aDocShell)
{
  printf("load type: ");

  PRUint32 loadType = 0;
  aDocShell->GetLoadType(&loadType);
  switch (loadType) {
    case LOAD_NORMAL:
      printf("normal; ");
      break;
    case LOAD_NORMAL_REPLACE:
      printf("normal replace; ");
      break;
    case LOAD_NORMAL_EXTERNAL:
      printf("normal external; ");
      break;
    case LOAD_HISTORY:
      printf("history; ");
      break;
    case LOAD_NORMAL_BYPASS_CACHE:
      printf("normal bypass cache; ");
      break;
    case LOAD_NORMAL_BYPASS_PROXY:
      printf("normal bypass proxy; ");
      break;
    case LOAD_NORMAL_BYPASS_PROXY_AND_CACHE:
      printf("normal bypass proxy and cache; ");
      break;
    case LOAD_RELOAD_NORMAL:
      printf("reload normal; ");
      break;
    case LOAD_RELOAD_BYPASS_CACHE:
      printf("reload bypass cache; ");
      break;
    case LOAD_RELOAD_BYPASS_PROXY:
      printf("reload bypass proxy; ");
      break;
    case LOAD_RELOAD_BYPASS_PROXY_AND_CACHE:
      printf("reload bypass proxy and cache; ");
      break;
    case LOAD_LINK:
      printf("link; ");
      break;
    case LOAD_REFRESH:
      printf("refresh; ");
      break;
    case LOAD_RELOAD_CHARSET_CHANGE:
      printf("reload charset change; ");
      break;
    case LOAD_BYPASS_HISTORY:
      printf("bypass history; ");
      break;
    case LOAD_STOP_CONTENT:
      printf("stop content; ");
      break;
    case LOAD_STOP_CONTENT_AND_REPLACE:
      printf("stop content and replace; ");
      break;
    case LOAD_PUSHSTATE:
      printf("load pushstate; ");
      break;
    case LOAD_ERROR_PAGE:
      printf("error page;");
      break;
    default:
      printf("unknown");
  }
}

static void
LogRequest(nsIRequest* aRequest)
{
  if (aRequest) {
    nsCAutoString name;
    aRequest->GetName(name);
    printf("    request spec: %s\n", name.get());
    PRUint32 loadFlags = 0;
    aRequest->GetLoadFlags(&loadFlags);
    printf("    request load flags: %x; ", loadFlags);
    if (loadFlags & nsIChannel::LOAD_DOCUMENT_URI)
      printf("document uri; ");
    if (loadFlags & nsIChannel::LOAD_RETARGETED_DOCUMENT_URI)
      printf("retargeted document uri; ");
    if (loadFlags & nsIChannel::LOAD_REPLACE)
      printf("replace; ");
    if (loadFlags & nsIChannel::LOAD_INITIAL_DOCUMENT_URI)
      printf("initial document uri; ");
    if (loadFlags & nsIChannel::LOAD_TARGETED)
      printf("targeted; ");
    if (loadFlags & nsIChannel::LOAD_CALL_CONTENT_SNIFFERS)
      printf("call content sniffers; ");
    if (loadFlags & nsIChannel::LOAD_CLASSIFY_URI)
      printf("classify uri; ");
  } else {
    printf("    no request");
  }
}

static void
GetDocLoadEventType(AccEvent* aEvent, nsACString& aEventType)
{
  PRUint32 type = aEvent->GetEventType();
  if (type == nsIAccessibleEvent::EVENT_DOCUMENT_LOAD_STOPPED) {
    aEventType.AssignLiteral("load stopped");
  } else if (type == nsIAccessibleEvent::EVENT_DOCUMENT_LOAD_COMPLETE) {
    aEventType.AssignLiteral("load complete");
  } else if (type == nsIAccessibleEvent::EVENT_DOCUMENT_RELOAD) {
    aEventType.AssignLiteral("reload");
  } else if (type == nsIAccessibleEvent::EVENT_STATE_CHANGE) {
    AccStateChangeEvent* event = downcast_accEvent(aEvent);
    if (event->GetState() == states::BUSY) {
      aEventType.AssignLiteral("busy ");
      if (event->IsStateEnabled())
        aEventType.AppendLiteral("true");
      else
        aEventType.AppendLiteral("false");
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// namespace logging:: document life cycle logging methods

void
logging::DocLoad(const char* aMsg, nsIWebProgress* aWebProgress,
                 nsIRequest* aRequest, PRUint32 aStateFlags)
{
  printf("\nA11Y DOCLOAD: %s\n", aMsg);

  nsCOMPtr<nsIDOMWindow> DOMWindow;
  aWebProgress->GetDOMWindow(getter_AddRefs(DOMWindow));
  if (!DOMWindow)
    return;

  nsCOMPtr<nsIDOMDocument> DOMDocument;
  DOMWindow->GetDocument(getter_AddRefs(DOMDocument));
  if (!DOMDocument)
    return;

  nsCOMPtr<nsIDocument> documentNode(do_QueryInterface(DOMDocument));
  DocAccessible* document =
    GetAccService()->GetDocAccessibleFromCache(documentNode);

  LogDocInfo(documentNode, document);

  printf("  {\n");
  nsCOMPtr<nsIWebNavigation> webNav(do_GetInterface(DOMWindow));
  nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(webNav));
  printf("    ");
  LogShellLoadType(docShell);
  printf("\n");
  LogRequest(aRequest);
  printf("\n");
  printf("    state flags: %x", aStateFlags);
  bool isDocLoading;
  aWebProgress->GetIsLoadingDocument(&isDocLoading);
  printf(", document is %sloading\n", (isDocLoading ? "" : "not "));
  printf("  }\n");
}

void
logging::DocLoad(const char* aMsg, nsIDocument* aDocumentNode)
{
  printf("\nA11Y DOCLOAD: %s\n", aMsg);

  DocAccessible* document =
    GetAccService()->GetDocAccessibleFromCache(aDocumentNode);
  LogDocInfo(aDocumentNode, document);
}

void
logging::DocLoadEventFired(AccEvent* aEvent)
{
  nsCAutoString strEventType;
  GetDocLoadEventType(aEvent, strEventType);
  if (!strEventType.IsEmpty())
    printf("  fire: %s\n", strEventType.get());
}

void
logging::DocLoadEventHandled(AccEvent* aEvent)
{
  nsCAutoString strEventType;
  GetDocLoadEventType(aEvent, strEventType);
  if (!strEventType.IsEmpty()) {
    printf("\nA11Y DOCEVENT: handled '%s' event ", strEventType.get());

    nsINode* node = aEvent->GetNode();
    if (node->IsNodeOfType(nsINode::eDOCUMENT)) {
      nsIDocument* documentNode = static_cast<nsIDocument*>(node);
      DocAccessible* document = aEvent->GetDocAccessible();
      LogDocInfo(documentNode, document);
    }

    printf("\n");
  }
}

void
logging::DocCreate(const char* aMsg, nsIDocument* aDocumentNode,
                   DocAccessible* aDocument)
{
  DocAccessible* document = aDocument ?
    aDocument : GetAccService()->GetDocAccessibleFromCache(aDocumentNode);

  printf("\nA11Y DOCCREATE: %s\n", aMsg);
  LogDocInfo(aDocumentNode, document);
}

void
logging::DocDestroy(const char* aMsg, nsIDocument* aDocumentNode,
                    DocAccessible* aDocument)
{
  DocAccessible* document = aDocument ?
    aDocument : GetAccService()->GetDocAccessibleFromCache(aDocumentNode);

  printf("\nA11Y DOCDESTROY: %s\n", aMsg);
  LogDocInfo(aDocumentNode, document);
}

void
logging::Address(const char* aDescr, nsAccessible* aAcc)
{
  nsINode* node = aAcc->GetNode();
  nsIDocument* docNode = aAcc->GetDocumentNode();
  DocAccessible* doc = GetAccService()->GetDocAccessibleFromCache(docNode);
  printf("  %s accessible: %p, node: %p\n", aDescr,
         static_cast<void*>(aAcc), static_cast<void*>(node));
  printf("  docacc for %s accessible: %p, node: %p\n", aDescr,
         static_cast<void*>(doc), static_cast<void*>(docNode));
  printf("  ");
  LogDocURI(docNode);
  printf("\n");
}

void
logging::Msg(const char* aMsg)
{
  printf("\n%s\n", aMsg);
}

void
logging::Text(const char* aText)
{
  printf("  %s\n", aText);
}

void
logging::Stack()
{
  printf("  stack: \n");
  nsTraceRefcntImpl::WalkTheStack(stdout);
}

////////////////////////////////////////////////////////////////////////////////
// namespace logging:: initialization

bool
logging::IsEnabled(PRUint32 aModules)
{
  return sModules & aModules;
}

void
logging::Enable(const nsAFlatCString& aModules)
{
  EnableLogging(aModules.get());
}


void
logging::CheckEnv()
{
  EnableLogging(PR_GetEnv("A11YLOG"));
}
