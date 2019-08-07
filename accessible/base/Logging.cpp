/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Logging.h"

#include "Accessible-inl.h"
#include "AccEvent.h"
#include "DocAccessible.h"
#include "nsAccessibilityService.h"
#include "nsCoreUtils.h"
#include "OuterDocAccessible.h"

#include "nsDocShellLoadTypes.h"
#include "nsIChannel.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsISelectionController.h"
#include "nsTraceRefcnt.h"
#include "nsIWebProgress.h"
#include "prenv.h"
#include "nsIDocShellTreeItem.h"
#include "nsIURI.h"
#include "mozilla/PresShell.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/HTMLBodyElement.h"
#include "mozilla/dom/Selection.h"

using namespace mozilla;
using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// Logging helpers

static uint32_t sModules = 0;

struct ModuleRep {
  const char* mStr;
  logging::EModules mModule;
};

static ModuleRep sModuleMap[] = {{"docload", logging::eDocLoad},
                                 {"doccreate", logging::eDocCreate},
                                 {"docdestroy", logging::eDocDestroy},
                                 {"doclifecycle", logging::eDocLifeCycle},

                                 {"events", logging::eEvents},
                                 {"eventTree", logging::eEventTree},
                                 {"platforms", logging::ePlatforms},
                                 {"text", logging::eText},
                                 {"tree", logging::eTree},

                                 {"DOMEvents", logging::eDOMEvents},
                                 {"focus", logging::eFocus},
                                 {"selection", logging::eSelection},
                                 {"notifications", logging::eNotifications},

                                 {"stack", logging::eStack},
                                 {"verbose", logging::eVerbose}};

static void EnableLogging(const char* aModulesStr) {
  sModules = 0;
  if (!aModulesStr) return;

  const char* token = aModulesStr;
  while (*token != '\0') {
    size_t tokenLen = strcspn(token, ",");
    for (unsigned int idx = 0; idx < ArrayLength(sModuleMap); idx++) {
      if (strncmp(token, sModuleMap[idx].mStr, tokenLen) == 0) {
#if !defined(MOZ_PROFILING) && (!defined(DEBUG) || defined(MOZ_OPTIMIZE))
        // Stack tracing on profiling enabled or debug not optimized builds.
        if (strncmp(token, "stack", tokenLen) == 0) break;
#endif
        sModules |= sModuleMap[idx].mModule;
        printf("\n\nmodule enabled: %s\n", sModuleMap[idx].mStr);
        break;
      }
    }
    token += tokenLen;

    if (*token == ',') token++;  // skip ',' char
  }
}

static void LogDocURI(dom::Document* aDocumentNode) {
  printf("uri: %s", aDocumentNode->GetDocumentURI()->GetSpecOrDefault().get());
}

static void LogDocShellState(dom::Document* aDocumentNode) {
  printf("docshell busy: ");

  nsAutoCString docShellBusy;
  nsCOMPtr<nsIDocShell> docShell = aDocumentNode->GetDocShell();
  nsIDocShell::BusyFlags busyFlags = nsIDocShell::BUSY_FLAGS_NONE;
  docShell->GetBusyFlags(&busyFlags);
  if (busyFlags == nsIDocShell::BUSY_FLAGS_NONE) {
    printf("'none'");
  }
  if (busyFlags & nsIDocShell::BUSY_FLAGS_BUSY) {
    printf("'busy'");
  }
  if (busyFlags & nsIDocShell::BUSY_FLAGS_BEFORE_PAGE_LOAD) {
    printf(", 'before page load'");
  }
  if (busyFlags & nsIDocShell::BUSY_FLAGS_PAGE_LOADING) {
    printf(", 'page loading'");
  }
}

static void LogDocType(dom::Document* aDocumentNode) {
  if (aDocumentNode->IsActive()) {
    bool isContent = nsCoreUtils::IsContentDocument(aDocumentNode);
    printf("%s document", (isContent ? "content" : "chrome"));
  } else {
    printf("document type: [failed]");
  }
}

static void LogDocShellTree(dom::Document* aDocumentNode) {
  if (aDocumentNode->IsActive()) {
    nsCOMPtr<nsIDocShellTreeItem> treeItem(aDocumentNode->GetDocShell());
    nsCOMPtr<nsIDocShellTreeItem> parentTreeItem;
    treeItem->GetInProcessParent(getter_AddRefs(parentTreeItem));
    nsCOMPtr<nsIDocShellTreeItem> rootTreeItem;
    treeItem->GetInProcessRootTreeItem(getter_AddRefs(rootTreeItem));
    printf("docshell hierarchy, parent: %p, root: %p, is tab document: %s;",
           static_cast<void*>(parentTreeItem), static_cast<void*>(rootTreeItem),
           (nsCoreUtils::IsTabDocument(aDocumentNode) ? "yes" : "no"));
  }
}

static void LogDocState(dom::Document* aDocumentNode) {
  const char* docState = nullptr;
  dom::Document::ReadyState docStateFlag = aDocumentNode->GetReadyStateEnum();
  switch (docStateFlag) {
    case dom::Document::READYSTATE_UNINITIALIZED:
      docState = "uninitialized";
      break;
    case dom::Document::READYSTATE_LOADING:
      docState = "loading";
      break;
    case dom::Document::READYSTATE_INTERACTIVE:
      docState = "interactive";
      break;
    case dom::Document::READYSTATE_COMPLETE:
      docState = "complete";
      break;
  }

  printf("doc state: %s", docState);
  printf(", %sinitial", aDocumentNode->IsInitialDocument() ? "" : "not ");
  printf(", %sshowing", aDocumentNode->IsShowing() ? "" : "not ");
  printf(", %svisible", aDocumentNode->IsVisible() ? "" : "not ");
  printf(", %svisible considering ancestors",
         aDocumentNode->IsVisibleConsideringAncestors() ? "" : "not ");
  printf(", %sactive", aDocumentNode->IsActive() ? "" : "not ");
  printf(", %sresource", aDocumentNode->IsResourceDoc() ? "" : "not ");

  dom::Element* rootEl = aDocumentNode->GetBodyElement();
  if (!rootEl) {
    rootEl = aDocumentNode->GetRootElement();
  }
  printf(", has %srole content", rootEl ? "" : "no ");
}

static void LogPresShell(dom::Document* aDocumentNode) {
  PresShell* presShell = aDocumentNode->GetPresShell();
  printf("presshell: %p", static_cast<void*>(presShell));

  nsIScrollableFrame* sf = nullptr;
  if (presShell) {
    printf(", is %s destroying", (presShell->IsDestroying() ? "" : "not"));
    sf = presShell->GetRootScrollFrameAsScrollable();
  }
  printf(", root scroll frame: %p", static_cast<void*>(sf));
}

static void LogDocLoadGroup(dom::Document* aDocumentNode) {
  nsCOMPtr<nsILoadGroup> loadGroup = aDocumentNode->GetDocumentLoadGroup();
  printf("load group: %p", static_cast<void*>(loadGroup));
}

static void LogDocParent(dom::Document* aDocumentNode) {
  dom::Document* parentDoc = aDocumentNode->GetInProcessParentDocument();
  printf("parent DOM document: %p", static_cast<void*>(parentDoc));
  if (parentDoc) {
    printf(", parent acc document: %p",
           static_cast<void*>(GetExistingDocAccessible(parentDoc)));
    printf("\n    parent ");
    LogDocURI(parentDoc);
    printf("\n");
  }
}

static void LogDocInfo(dom::Document* aDocumentNode, DocAccessible* aDocument) {
  printf("    DOM document: %p, acc document: %p\n    ",
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

static void LogShellLoadType(nsIDocShell* aDocShell) {
  printf("load type: ");

  uint32_t loadType = 0;
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
    case LOAD_NORMAL_ALLOW_MIXED_CONTENT:
      printf("normal allow mixed content; ");
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
    case LOAD_RELOAD_ALLOW_MIXED_CONTENT:
      printf("reload allow mixed content; ");
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

static void LogRequest(nsIRequest* aRequest) {
  if (aRequest) {
    nsAutoCString name;
    aRequest->GetName(name);
    printf("    request spec: %s\n", name.get());
    uint32_t loadFlags = 0;
    aRequest->GetLoadFlags(&loadFlags);
    printf("    request load flags: %x; ", loadFlags);
    if (loadFlags & nsIChannel::LOAD_DOCUMENT_URI) printf("document uri; ");
    if (loadFlags & nsIChannel::LOAD_RETARGETED_DOCUMENT_URI)
      printf("retargeted document uri; ");
    if (loadFlags & nsIChannel::LOAD_REPLACE) printf("replace; ");
    if (loadFlags & nsIChannel::LOAD_INITIAL_DOCUMENT_URI)
      printf("initial document uri; ");
    if (loadFlags & nsIChannel::LOAD_TARGETED) printf("targeted; ");
    if (loadFlags & nsIChannel::LOAD_CALL_CONTENT_SNIFFERS)
      printf("call content sniffers; ");
    if (loadFlags & nsIChannel::LOAD_BYPASS_URL_CLASSIFIER) {
      printf("bypass classify uri; ");
    }
  } else {
    printf("    no request");
  }
}

static void LogDocAccState(DocAccessible* aDocument) {
  printf("document acc state: ");
  if (aDocument->HasLoadState(DocAccessible::eCompletelyLoaded))
    printf("completely loaded;");
  else if (aDocument->HasLoadState(DocAccessible::eReady))
    printf("ready;");
  else if (aDocument->HasLoadState(DocAccessible::eDOMLoaded))
    printf("DOM loaded;");
  else if (aDocument->HasLoadState(DocAccessible::eTreeConstructed))
    printf("tree constructed;");
}

static void GetDocLoadEventType(AccEvent* aEvent, nsACString& aEventType) {
  uint32_t type = aEvent->GetEventType();
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
static const char* sFocusTitle = "FOCUS";

void logging::DocLoad(const char* aMsg, nsIWebProgress* aWebProgress,
                      nsIRequest* aRequest, uint32_t aStateFlags) {
  MsgBegin(sDocLoadTitle, "%s", aMsg);

  nsCOMPtr<mozIDOMWindowProxy> DOMWindow;
  aWebProgress->GetDOMWindow(getter_AddRefs(DOMWindow));
  nsPIDOMWindowOuter* window = nsPIDOMWindowOuter::From(DOMWindow);
  if (!window) {
    MsgEnd();
    return;
  }

  nsCOMPtr<dom::Document> documentNode = window->GetDoc();
  if (!documentNode) {
    MsgEnd();
    return;
  }

  DocAccessible* document = GetExistingDocAccessible(documentNode);

  LogDocInfo(documentNode, document);

  nsCOMPtr<nsIDocShell> docShell = window->GetDocShell();
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

void logging::DocLoad(const char* aMsg, dom::Document* aDocumentNode) {
  MsgBegin(sDocLoadTitle, "%s", aMsg);

  DocAccessible* document = GetExistingDocAccessible(aDocumentNode);
  LogDocInfo(aDocumentNode, document);

  MsgEnd();
}

void logging::DocCompleteLoad(DocAccessible* aDocument,
                              bool aIsLoadEventTarget) {
  MsgBegin(sDocLoadTitle, "document loaded *completely*");

  printf("    DOM document: %p, acc document: %p\n",
         static_cast<void*>(aDocument->DocumentNode()),
         static_cast<void*>(aDocument));

  printf("    ");
  LogDocURI(aDocument->DocumentNode());
  printf("\n");

  printf("    ");
  LogDocAccState(aDocument);
  printf("\n");

  printf("    document is load event target: %s\n",
         (aIsLoadEventTarget ? "true" : "false"));

  MsgEnd();
}

void logging::DocLoadEventFired(AccEvent* aEvent) {
  nsAutoCString strEventType;
  GetDocLoadEventType(aEvent, strEventType);
  if (!strEventType.IsEmpty()) printf("  fire: %s\n", strEventType.get());
}

void logging::DocLoadEventHandled(AccEvent* aEvent) {
  nsAutoCString strEventType;
  GetDocLoadEventType(aEvent, strEventType);
  if (strEventType.IsEmpty()) return;

  MsgBegin(sDocEventTitle, "handled '%s' event", strEventType.get());

  DocAccessible* document = aEvent->GetAccessible()->AsDoc();
  if (document) LogDocInfo(document->DocumentNode(), document);

  MsgEnd();
}

void logging::DocCreate(const char* aMsg, dom::Document* aDocumentNode,
                        DocAccessible* aDocument) {
  DocAccessible* document =
      aDocument ? aDocument : GetExistingDocAccessible(aDocumentNode);

  MsgBegin(sDocCreateTitle, "%s", aMsg);
  LogDocInfo(aDocumentNode, document);
  MsgEnd();
}

void logging::DocDestroy(const char* aMsg, dom::Document* aDocumentNode,
                         DocAccessible* aDocument) {
  DocAccessible* document =
      aDocument ? aDocument : GetExistingDocAccessible(aDocumentNode);

  MsgBegin(sDocDestroyTitle, "%s", aMsg);
  LogDocInfo(aDocumentNode, document);
  MsgEnd();
}

void logging::OuterDocDestroy(OuterDocAccessible* aOuterDoc) {
  MsgBegin(sDocDestroyTitle, "outerdoc shutdown");
  logging::Address("outerdoc", aOuterDoc);
  MsgEnd();
}

void logging::FocusNotificationTarget(const char* aMsg,
                                      const char* aTargetDescr,
                                      Accessible* aTarget) {
  MsgBegin(sFocusTitle, "%s", aMsg);
  AccessibleNNode(aTargetDescr, aTarget);
  MsgEnd();
}

void logging::FocusNotificationTarget(const char* aMsg,
                                      const char* aTargetDescr,
                                      nsINode* aTargetNode) {
  MsgBegin(sFocusTitle, "%s", aMsg);
  Node(aTargetDescr, aTargetNode);
  MsgEnd();
}

void logging::FocusNotificationTarget(const char* aMsg,
                                      const char* aTargetDescr,
                                      nsISupports* aTargetThing) {
  MsgBegin(sFocusTitle, "%s", aMsg);

  if (aTargetThing) {
    nsCOMPtr<nsINode> targetNode(do_QueryInterface(aTargetThing));
    if (targetNode)
      AccessibleNNode(aTargetDescr, targetNode);
    else
      printf("    %s: %p, window\n", aTargetDescr,
             static_cast<void*>(aTargetThing));
  }

  MsgEnd();
}

void logging::ActiveItemChangeCausedBy(const char* aCause,
                                       Accessible* aTarget) {
  SubMsgBegin();
  printf("    Caused by: %s\n", aCause);
  AccessibleNNode("Item", aTarget);
  SubMsgEnd();
}

void logging::ActiveWidget(Accessible* aWidget) {
  SubMsgBegin();

  AccessibleNNode("Widget", aWidget);
  printf("    Widget is active: %s, has operable items: %s\n",
         (aWidget && aWidget->IsActiveWidget() ? "true" : "false"),
         (aWidget && aWidget->AreItemsOperable() ? "true" : "false"));

  SubMsgEnd();
}

void logging::FocusDispatched(Accessible* aTarget) {
  SubMsgBegin();
  AccessibleNNode("A11y target", aTarget);
  SubMsgEnd();
}

void logging::SelChange(dom::Selection* aSelection, DocAccessible* aDocument,
                        int16_t aReason) {
  SelectionType type = aSelection->GetType();

  const char* strType = 0;
  if (type == SelectionType::eNormal)
    strType = "normal";
  else if (type == SelectionType::eSpellCheck)
    strType = "spellcheck";
  else
    strType = "unknown";

  bool isIgnored = !aDocument || !aDocument->IsContentLoaded();
  printf(
      "\nSelection changed, selection type: %s, notification %s, reason: %d\n",
      strType, (isIgnored ? "ignored" : "pending"), aReason);

  Stack();
}

void logging::TreeInfo(const char* aMsg, uint32_t aExtraFlags, ...) {
  if (IsEnabledAll(logging::eTree | aExtraFlags)) {
    va_list vl;
    va_start(vl, aExtraFlags);
    const char* descr = va_arg(vl, const char*);
    if (descr) {
      Accessible* acc = va_arg(vl, Accessible*);
      MsgBegin("TREE", "%s; doc: %p", aMsg, acc ? acc->Document() : nullptr);
      AccessibleInfo(descr, acc);
      while ((descr = va_arg(vl, const char*))) {
        AccessibleInfo(descr, va_arg(vl, Accessible*));
      }
    } else {
      MsgBegin("TREE", "%s", aMsg);
    }
    va_end(vl);
    MsgEnd();

    if (aExtraFlags & eStack) {
      Stack();
    }
  }
}

void logging::TreeInfo(const char* aMsg, uint32_t aExtraFlags,
                       const char* aMsg1, Accessible* aAcc, const char* aMsg2,
                       nsINode* aNode) {
  if (IsEnabledAll(logging::eTree | aExtraFlags)) {
    MsgBegin("TREE", "%s; doc: %p", aMsg, aAcc ? aAcc->Document() : nullptr);
    AccessibleInfo(aMsg1, aAcc);
    Accessible* acc = aAcc ? aAcc->Document()->GetAccessible(aNode) : nullptr;
    if (acc) {
      AccessibleInfo(aMsg2, acc);
    } else {
      Node(aMsg2, aNode);
    }
    MsgEnd();
  }
}

void logging::TreeInfo(const char* aMsg, uint32_t aExtraFlags,
                       Accessible* aParent) {
  if (IsEnabledAll(logging::eTree | aExtraFlags)) {
    MsgBegin("TREE", "%s; doc: %p", aMsg, aParent->Document());
    AccessibleInfo("container", aParent);
    for (uint32_t idx = 0; idx < aParent->ChildCount(); idx++) {
      AccessibleInfo("child", aParent->GetChildAt(idx));
    }
    MsgEnd();
  }
}

void logging::Tree(const char* aTitle, const char* aMsgText, Accessible* aRoot,
                   GetTreePrefix aPrefixFunc, void* aGetTreePrefixData) {
  logging::MsgBegin(aTitle, "%s", aMsgText);

  nsAutoString level;
  Accessible* root = aRoot;
  do {
    const char* prefix =
        aPrefixFunc ? aPrefixFunc(aGetTreePrefixData, root) : "";
    printf("%s", NS_ConvertUTF16toUTF8(level).get());
    logging::AccessibleInfo(prefix, root);
    if (root->FirstChild() && !root->FirstChild()->IsDoc()) {
      level.AppendLiteral(u"  ");
      root = root->FirstChild();
      continue;
    }
    int32_t idxInParent = root != aRoot && root->mParent
                              ? root->mParent->mChildren.IndexOf(root)
                              : -1;
    if (idxInParent != -1 &&
        idxInParent <
            static_cast<int32_t>(root->mParent->mChildren.Length() - 1)) {
      root = root->mParent->mChildren.ElementAt(idxInParent + 1);
      continue;
    }
    while (root != aRoot && (root = root->Parent())) {
      level.Cut(0, 2);
      int32_t idxInParent = !root->IsDoc() && root->mParent
                                ? root->mParent->mChildren.IndexOf(root)
                                : -1;
      if (idxInParent != -1 &&
          idxInParent <
              static_cast<int32_t>(root->mParent->mChildren.Length() - 1)) {
        root = root->mParent->mChildren.ElementAt(idxInParent + 1);
        break;
      }
    }
  } while (root && root != aRoot);

  logging::MsgEnd();
}

void logging::DOMTree(const char* aTitle, const char* aMsgText,
                      DocAccessible* aDocument) {
  logging::MsgBegin(aTitle, "%s", aMsgText);
  nsAutoString level;
  nsINode* root = aDocument->DocumentNode();
  do {
    printf("%s", NS_ConvertUTF16toUTF8(level).get());
    logging::Node("", root);
    if (root->GetFirstChild()) {
      level.AppendLiteral(u"  ");
      root = root->GetFirstChild();
      continue;
    }
    if (root->GetNextSibling()) {
      root = root->GetNextSibling();
      continue;
    }
    while ((root = root->GetParentNode())) {
      level.Cut(0, 2);
      if (root->GetNextSibling()) {
        root = root->GetNextSibling();
        break;
      }
    }
  } while (root);
  logging::MsgEnd();
}

void logging::MsgBegin(const char* aTitle, const char* aMsgText, ...) {
  printf("\nA11Y %s: ", aTitle);

  va_list argptr;
  va_start(argptr, aMsgText);
  vprintf(aMsgText, argptr);
  va_end(argptr);

  PRIntervalTime time = PR_IntervalNow();
  uint32_t mins = (PR_IntervalToSeconds(time) / 60) % 60;
  uint32_t secs = PR_IntervalToSeconds(time) % 60;
  uint32_t msecs = PR_IntervalToMilliseconds(time) % 1000;
  printf("; %02u:%02u.%03u", mins, secs, msecs);

  printf("\n  {\n");
}

void logging::MsgEnd() { printf("  }\n"); }

void logging::SubMsgBegin() { printf("  {\n"); }

void logging::SubMsgEnd() { printf("  }\n"); }

void logging::MsgEntry(const char* aEntryText, ...) {
  printf("    ");

  va_list argptr;
  va_start(argptr, aEntryText);
  vprintf(aEntryText, argptr);
  va_end(argptr);

  printf("\n");
}

void logging::Text(const char* aText) { printf("  %s\n", aText); }

void logging::Address(const char* aDescr, Accessible* aAcc) {
  if (!aAcc->IsDoc()) {
    printf("    %s accessible: %p, node: %p\n", aDescr,
           static_cast<void*>(aAcc), static_cast<void*>(aAcc->GetNode()));
  }

  DocAccessible* doc = aAcc->Document();
  dom::Document* docNode = doc->DocumentNode();
  printf("    document: %p, node: %p\n", static_cast<void*>(doc),
         static_cast<void*>(docNode));

  printf("    ");
  LogDocURI(docNode);
  printf("\n");
}

void logging::Node(const char* aDescr, nsINode* aNode) {
  printf("    ");

  if (!aNode) {
    printf("%s: null\n", aDescr);
    return;
  }

  if (aNode->IsDocument()) {
    printf("%s: %p, document\n", aDescr, static_cast<void*>(aNode));
    return;
  }

  nsINode* parentNode = aNode->GetParentNode();
  int32_t idxInParent = parentNode ? parentNode->ComputeIndexOf(aNode) : -1;

  if (aNode->IsText()) {
    printf("%s: %p, text node, idx in parent: %d\n", aDescr,
           static_cast<void*>(aNode), idxInParent);
    return;
  }

  if (!aNode->IsElement()) {
    printf("%s: %p, not accessible node type, idx in parent: %d\n", aDescr,
           static_cast<void*>(aNode), idxInParent);
    return;
  }

  dom::Element* elm = aNode->AsElement();

  nsAutoCString tag;
  elm->NodeInfo()->NameAtom()->ToUTF8String(tag);

  nsAtom* idAtom = elm->GetID();
  nsAutoCString id;
  if (idAtom) idAtom->ToUTF8String(id);

  printf("%s: %p, %s@id='%s', idx in parent: %d\n", aDescr,
         static_cast<void*>(elm), tag.get(), id.get(), idxInParent);
}

void logging::Document(DocAccessible* aDocument) {
  printf("    Document: %p, document node: %p\n", static_cast<void*>(aDocument),
         static_cast<void*>(aDocument->DocumentNode()));

  printf("    Document ");
  LogDocURI(aDocument->DocumentNode());
  printf("\n");
}

void logging::AccessibleInfo(const char* aDescr, Accessible* aAccessible) {
  printf("    %s: %p; ", aDescr, static_cast<void*>(aAccessible));
  if (!aAccessible) {
    printf("\n");
    return;
  }
  if (aAccessible->IsDefunct()) {
    printf("defunct\n");
    return;
  }
  if (!aAccessible->Document() || aAccessible->Document()->IsDefunct()) {
    printf("document is shutting down, no info\n");
    return;
  }

  nsAutoString role;
  GetAccService()->GetStringRole(aAccessible->Role(), role);
  printf("role: %s", NS_ConvertUTF16toUTF8(role).get());

  nsAutoString name;
  aAccessible->Name(name);
  if (!name.IsEmpty()) {
    printf(", name: '%s'", NS_ConvertUTF16toUTF8(name).get());
  }

  printf(", idx: %d", aAccessible->IndexInParent());

  nsINode* node = aAccessible->GetNode();
  if (!node) {
    printf(", node: null\n");
  } else if (node->IsDocument()) {
    printf(", document node: %p\n", static_cast<void*>(node));
  } else if (node->IsText()) {
    printf(", text node: %p\n", static_cast<void*>(node));
  } else if (node->IsElement()) {
    dom::Element* el = node->AsElement();

    nsAutoCString tag;
    el->NodeInfo()->NameAtom()->ToUTF8String(tag);

    nsAtom* idAtom = el->GetID();
    nsAutoCString id;
    if (idAtom) {
      idAtom->ToUTF8String(id);
    }

    printf(", element node: %p, %s@id='%s'\n", static_cast<void*>(el),
           tag.get(), id.get());
  }
}

void logging::AccessibleNNode(const char* aDescr, Accessible* aAccessible) {
  printf("    %s: %p; ", aDescr, static_cast<void*>(aAccessible));
  if (!aAccessible) return;

  nsAutoString role;
  GetAccService()->GetStringRole(aAccessible->Role(), role);
  nsAutoString name;
  aAccessible->Name(name);

  printf("role: %s, name: '%s';\n", NS_ConvertUTF16toUTF8(role).get(),
         NS_ConvertUTF16toUTF8(name).get());

  nsAutoCString nodeDescr(aDescr);
  nodeDescr.AppendLiteral(" node");
  Node(nodeDescr.get(), aAccessible->GetNode());

  Document(aAccessible->Document());
}

void logging::AccessibleNNode(const char* aDescr, nsINode* aNode) {
  DocAccessible* document =
      GetAccService()->GetDocAccessible(aNode->OwnerDoc());

  if (document) {
    Accessible* accessible = document->GetAccessible(aNode);
    if (accessible) {
      AccessibleNNode(aDescr, accessible);
      return;
    }
  }

  nsAutoCString nodeDescr("[not accessible] ");
  nodeDescr.Append(aDescr);
  Node(nodeDescr.get(), aNode);

  if (document) {
    Document(document);
    return;
  }

  printf("    [contained by not accessible document]:\n");
  LogDocInfo(aNode->OwnerDoc(), document);
  printf("\n");
}

void logging::DOMEvent(const char* aDescr, nsINode* aOrigTarget,
                       const nsAString& aEventType) {
  logging::MsgBegin("DOMEvents", "event '%s' %s",
                    NS_ConvertUTF16toUTF8(aEventType).get(), aDescr);
  logging::AccessibleNNode("Target", aOrigTarget);
  logging::MsgEnd();
}

void logging::Stack() {
  if (IsEnabled(eStack)) {
    printf("  stack: \n");
    nsTraceRefcnt::WalkTheStack(stdout);
  }
}

////////////////////////////////////////////////////////////////////////////////
// namespace logging:: initialization

bool logging::IsEnabled(uint32_t aModules) { return sModules & aModules; }

bool logging::IsEnabledAll(uint32_t aModules) {
  return (sModules & aModules) == aModules;
}

bool logging::IsEnabled(const nsAString& aModuleStr) {
  for (unsigned int idx = 0; idx < ArrayLength(sModuleMap); idx++) {
    if (aModuleStr.EqualsASCII(sModuleMap[idx].mStr))
      return sModules & sModuleMap[idx].mModule;
  }

  return false;
}

void logging::Enable(const nsCString& aModules) {
  EnableLogging(aModules.get());
}

void logging::CheckEnv() { EnableLogging(PR_GetEnv("A11YLOG")); }
