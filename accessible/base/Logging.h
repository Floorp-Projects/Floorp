/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_logs_h__
#define mozilla_a11y_logs_h__

#include "nscore.h"
#include "nsStringFwd.h"
#include "mozilla/Attributes.h"

class nsINode;
class nsIRequest;
class nsISupports;
class nsIWebProgress;

namespace mozilla {

namespace dom {
class Document;
class Selection;
}  // namespace dom

namespace a11y {

class AccEvent;
class LocalAccessible;
class DocAccessible;
class OuterDocAccessible;

namespace logging {

enum EModules {
  eDocLoad = 1 << 0,
  eDocCreate = 1 << 1,
  eDocDestroy = 1 << 2,
  eDocLifeCycle = eDocLoad | eDocCreate | eDocDestroy,

  eEvents = 1 << 3,
  eEventTree = 1 << 4,
  ePlatforms = 1 << 5,
  eText = 1 << 6,
  eTree = 1 << 7,
  eTreeSize = 1 << 8,

  eDOMEvents = 1 << 9,
  eFocus = 1 << 10,
  eSelection = 1 << 11,
  eNotifications = eDOMEvents | eSelection | eFocus,

  // extras
  eStack = 1 << 12,
  eVerbose = 1 << 13
};

/**
 * Return true if any of the given modules is logged.
 */
bool IsEnabled(uint32_t aModules);

/**
 * Return true if all of the given modules are logged.
 */
bool IsEnabledAll(uint32_t aModules);

/**
 * Return true if the given module is logged.
 */
bool IsEnabled(const nsAString& aModules);

/**
 * Log the document loading progress.
 */
void DocLoad(const char* aMsg, nsIWebProgress* aWebProgress,
             nsIRequest* aRequest, uint32_t aStateFlags);
void DocLoad(const char* aMsg, dom::Document* aDocumentNode);
void DocCompleteLoad(DocAccessible* aDocument, bool aIsLoadEventTarget);

/**
 * Log that document load event was fired.
 */
void DocLoadEventFired(AccEvent* aEvent);

/**
 * Log that document laod event was handled.
 */
void DocLoadEventHandled(AccEvent* aEvent);

/**
 * Log the document was created.
 */
void DocCreate(const char* aMsg, dom::Document* aDocumentNode,
               DocAccessible* aDocument = nullptr);

/**
 * Log the document was destroyed.
 */
void DocDestroy(const char* aMsg, dom::Document* aDocumentNode,
                DocAccessible* aDocument = nullptr);

/**
 * Log the outer document was destroyed.
 */
void OuterDocDestroy(OuterDocAccessible* OuterDoc);

/**
 * Log the focus notification target.
 */
void FocusNotificationTarget(const char* aMsg, const char* aTargetDescr,
                             LocalAccessible* aTarget);
void FocusNotificationTarget(const char* aMsg, const char* aTargetDescr,
                             nsINode* aTargetNode);
void FocusNotificationTarget(const char* aMsg, const char* aTargetDescr,
                             nsISupports* aTargetThing);

/**
 * Log a cause of active item descendant change (submessage).
 */
void ActiveItemChangeCausedBy(const char* aMsg, LocalAccessible* aTarget);

/**
 * Log the active widget (submessage).
 */
void ActiveWidget(LocalAccessible* aWidget);

/**
 * Log the focus event was dispatched (submessage).
 */
void FocusDispatched(LocalAccessible* aTarget);

/**
 * Log the selection change.
 */
void SelChange(dom::Selection* aSelection, DocAccessible* aDocument,
               int16_t aReason);

/**
 * Log the given accessible elements info.
 */
void TreeInfo(const char* aMsg, uint32_t aExtraFlags, ...);
void TreeInfo(const char* aMsg, uint32_t aExtraFlags, const char* aMsg1,
              LocalAccessible* aAcc, const char* aMsg2, nsINode* aNode);
void TreeInfo(const char* aMsg, uint32_t aExtraFlags, LocalAccessible* aParent);

/**
 * Log the accessible/DOM tree.
 */
typedef const char* (*GetTreePrefix)(void* aData, LocalAccessible*);
void Tree(const char* aTitle, const char* aMsgText, LocalAccessible* aRoot,
          GetTreePrefix aPrefixFunc = nullptr,
          void* aGetTreePrefixData = nullptr);
void DOMTree(const char* aTitle, const char* aMsgText, DocAccessible* aDoc);

/**
 * Log the tree size in bytes.
 */
void TreeSize(const char* aTitle, const char* aMsgText, LocalAccessible* aRoot);

/**
 * Log the message ('title: text' format) on new line. Print the start and end
 * boundaries of the message body designated by '{' and '}' (2 spaces indent for
 * body).
 */
void MsgBegin(const char* aTitle, const char* aMsgText, ...)
    MOZ_FORMAT_PRINTF(2, 3);
void MsgEnd();

/**
 * Print start and end boundaries of the message body designated by '{' and '}'
 * (2 spaces indent for body).
 */
void SubMsgBegin();
void SubMsgEnd();

/**
 * Log the entry into message body (4 spaces indent).
 */
void MsgEntry(const char* aEntryText, ...) MOZ_FORMAT_PRINTF(1, 2);

/**
 * Log the text, two spaces offset is used.
 */
void Text(const char* aText);

/**
 * Log the accessible object address as message entry (4 spaces indent).
 */
void Address(const char* aDescr, LocalAccessible* aAcc);

/**
 * Log the DOM node info as message entry.
 */
void Node(const char* aDescr, nsINode* aNode);

/**
 * Log the document accessible info as message entry.
 */
void Document(DocAccessible* aDocument);

/**
 * Log the accessible and its DOM node as a message entry.
 */
void AccessibleInfo(const char* aDescr, LocalAccessible* aAccessible);
void AccessibleNNode(const char* aDescr, LocalAccessible* aAccessible);
void AccessibleNNode(const char* aDescr, nsINode* aNode);

/**
 * Log the DOM event.
 */
void DOMEvent(const char* aDescr, nsINode* aOrigTarget,
              const nsAString& aEventType);

/**
 * Log the call stack, two spaces offset is used.
 */
void Stack();

/**
 * Enable logging of the specified modules, all other modules aren't logged.
 */
void Enable(const nsCString& aModules);

/**
 * Enable logging of modules specified by A11YLOG environment variable,
 * all other modules aren't logged.
 */
void CheckEnv();

}  // namespace logging

}  // namespace a11y
}  // namespace mozilla

#endif
