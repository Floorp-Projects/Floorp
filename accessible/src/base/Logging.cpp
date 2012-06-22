/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Logging.h"

#include "AccEvent.h"
#include "DocAccessible.h"
#include "nsAccessibilityService.h"
#include "nsCoreUtils.h"
#include "OuterDocAccessible.h"

#include "nsDocShellLoadTypes.h"
#include "nsIChannel.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsISelectionPrivate.h"
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
    { "doclifecycle", logging::eDocLifeCycle },

    { "events", logging::eEvents },
    { "platforms", logging::ePlatforms },
    { "stack", logging::eStack },
    { "text", logging::eText },
    { "tree", logging::eTree },

    { "DOMEvents", logging::eDOMEvents },
    { "focus", logging::eFocus },
    { "selection", logging::eSelection },
    { "notifications", logging::eNotifications }
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
    case LOAD_REPLACE_BYPASS_CACHE:
      printf("replace bypass cache; ");
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

static const char* sDocLoadTitle = "DOCLOAD";
static const char* sDocCreateTitle = "DOCCREATE";
static const char* sDocDestroyTitle = "DOCDESTROY";
static const char* sDocEventTitle = "DOCEVENT";

void
logging::DocLoad(const char* aMsg, nsIWebProgress* aWebProgress,
                 nsIRequest* aRequest, PRUint32 aStateFlags)
{
  MsgBegin(sDocLoadTitle, aMsg);

  nsCOMPtr<nsIDOMWindow> DOMWindow;
  aWebProgress->GetDOMWindow(getter_AddRefs(DOMWindow));
  if (!DOMWindow) {
    MsgEnd();
    return;
  }

  nsCOMPtr<nsIDOMDocument> DOMDocument;
  DOMWindow->GetDocument(getter_AddRefs(DOMDocument));
  if (!DOMDocument) {
    MsgEnd();
    return;
  }

  nsCOMPtr<nsIDocument> documentNode(do_QueryInterface(DOMDocument));
  DocAccessible* document =
    GetAccService()->GetDocAccessibleFromCache(documentNode);

  LogDocInfo(documentNode, document);

  nsCOMPtr<nsIWebNavigation> webNav(do_GetInterface(DOMWindow));
  nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(webNav));
  printf("\n    ");
  LogShellLoadType(docShell);
  printf("\n");
  LogRequest(aRequest);
  printf("\n");
  printf("    state flags: %x", aStateFlags);
  bool isDocLoading;
  aWebProgress->GetIsLoadingDocument(&isDocLoading);
  printf(", document is %sloading\n", (isDocLoading ? "" : "not "));

  MsgEnd();
}

void
logging::DocLoad(const char* aMsg, nsIDocument* aDocumentNode)
{
  MsgBegin(sDocLoadTitle, aMsg);

  DocAccessible* document =
    GetAccService()->GetDocAccessibleFromCache(aDocumentNode);
  LogDocInfo(aDocumentNode, document);

  MsgEnd();
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
  if (strEventType.IsEmpty())
    return;

  MsgBegin(sDocEventTitle, "handled '%s' event", strEventType.get());

  nsINode* node = aEvent->GetNode();
  if (node->IsNodeOfType(nsINode::eDOCUMENT)) {
    nsIDocument* documentNode = static_cast<nsIDocument*>(node);
    DocAccessible* document = aEvent->GetDocAccessible();
    LogDocInfo(documentNode, document);
  }

  MsgEnd();
}

void
logging::DocCreate(const char* aMsg, nsIDocument* aDocumentNode,
                   DocAccessible* aDocument)
{
  DocAccessible* document = aDocument ?
    aDocument : GetAccService()->GetDocAccessibleFromCache(aDocumentNode);

  MsgBegin(sDocCreateTitle, aMsg);
  LogDocInfo(aDocumentNode, document);
  MsgEnd();
}

void
logging::DocDestroy(const char* aMsg, nsIDocument* aDocumentNode,
                    DocAccessible* aDocument)
{
  DocAccessible* document = aDocument ?
    aDocument : GetAccService()->GetDocAccessibleFromCache(aDocumentNode);

  MsgBegin(sDocDestroyTitle, aMsg);
  LogDocInfo(aDocumentNode, document);
  MsgEnd();
}

void
logging::OuterDocDestroy(OuterDocAccessible* aOuterDoc)
{
  MsgBegin(sDocDestroyTitle, "outerdoc shutdown");
  logging::Address("outerdoc", aOuterDoc);
  MsgEnd();
}

void
logging::SelChange(nsISelection* aSelection, DocAccessible* aDocument)
{
  nsCOMPtr<nsISelectionPrivate> privSel(do_QueryInterface(aSelection));

  PRInt16 type = 0;
  privSel->GetType(&type);

  const char* strType = 0;
  if (type == nsISelectionController::SELECTION_NORMAL)
    strType = "normal";
  else if (type == nsISelectionController::SELECTION_SPELLCHECK)
    strType = "spellcheck";
  else
    strType = "unknown";

  bool isIgnored = !aDocument || !aDocument->IsContentLoaded();
  printf("\nSelection changed, selection type: %s, notification %s\n",
         strType, (isIgnored ? "ignored" : "pending"));
}

void
logging::MsgBegin(const char* aTitle, const char* aMsgText, ...)
{
  printf("\nA11Y %s: ", aTitle);

  va_list argptr;
  va_start(argptr, aMsgText);
  vprintf(aMsgText, argptr);
  va_end(argptr);

  printf("\n  {\n");
}

void
logging::MsgEnd()
{
  printf("  }\n");
}

void
logging::MsgEntry(const char* aEntryText, ...)
{
  printf("    ");

  va_list argptr;
  va_start(argptr, aEntryText);
  vprintf(aEntryText, argptr);
  va_end(argptr);

  printf("\n");
}

void
logging::Text(const char* aText)
{
  printf("  %s\n", aText);
}

void
logging::Address(const char* aDescr, Accessible* aAcc)
{
  if (!aAcc->IsDoc()) {
    printf("    %s accessible: %p, node: %p\n", aDescr,
           static_cast<void*>(aAcc), static_cast<void*>(aAcc->GetNode()));
  }

  DocAccessible* doc = aAcc->Document();
  nsIDocument* docNode = aAcc->GetDocumentNode();
  printf("    document: %p, node: %p\n",
         static_cast<void*>(doc), static_cast<void*>(docNode));

  printf("    ");
  LogDocURI(docNode);
  printf("\n");
}

void
logging::Node(const char* aDescr, nsINode* aNode)
{
  printf("    ");

  if (!aNode) {
    printf("%s: null\n", aDescr);
    return;
  }

  if (aNode->IsNodeOfType(nsINode::eDOCUMENT)) {
    printf("%s: %p, document\n", aDescr, static_cast<void*>(aNode));
    return;
  }

  nsINode* parentNode = aNode->GetNodeParent();
  PRInt32 idxInParent = parentNode ? parentNode->IndexOf(aNode) : - 1;

  if (aNode->IsNodeOfType(nsINode::eTEXT)) {
    printf("%s: %p, text node, idx in parent: %d\n",
           aDescr, static_cast<void*>(aNode), idxInParent);
    return;
  }

  if (!aNode->IsElement()) {
    printf("%s: %p, not accessible node type, idx in parent: %d\n",
           aDescr, static_cast<void*>(aNode), idxInParent);
    return;
  }

  dom::Element* elm = aNode->AsElement();

  nsCAutoString tag;
  elm->Tag()->ToUTF8String(tag);

  nsIAtom* idAtom = elm->GetID();
  nsCAutoString id;
  if (idAtom)
    idAtom->ToUTF8String(id);

  printf("%s: %p, %s@id='%s', idx in parent: %d\n",
         aDescr, static_cast<void*>(elm), tag.get(), id.get(), idxInParent);
}

void
logging::Stack()
{
  if (IsEnabled(eStack)) {
    printf("  stack: \n");
    nsTraceRefcntImpl::WalkTheStack(stdout);
  }
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
