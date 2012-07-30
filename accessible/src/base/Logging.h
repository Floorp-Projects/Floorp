/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_logs_h__
#define mozilla_a11y_logs_h__

#include "nscore.h"
#include "nsAString.h"

class AccEvent;
class Accessible;
class DocAccessible;

class nsIDocument;
class nsINode;
class nsIRequest;
class nsISelection;
class nsIWebProgress;

namespace mozilla {
namespace a11y {

class OuterDocAccessible;

namespace logging {

enum EModules {
  eDocLoad = 1 << 0,
  eDocCreate = 1 << 1,
  eDocDestroy = 1 << 2,
  eDocLifeCycle = eDocLoad | eDocCreate | eDocDestroy,

  eEvents = 1 << 3,
  ePlatforms = 1 << 4,
  eStack = 1 << 5,
  eText = 1 << 6,
  eTree = 1 << 7,

  eDOMEvents = 1 << 8,
  eFocus = 1 << 9,
  eSelection = 1 << 10,
  eNotifications = eDOMEvents | eSelection | eFocus
};

/**
 * Return true if any of the given modules is logged.
 */
bool IsEnabled(PRUint32 aModules);

/**
 * Log the document loading progress.
 */
void DocLoad(const char* aMsg, nsIWebProgress* aWebProgress,
             nsIRequest* aRequest, PRUint32 aStateFlags);
void DocLoad(const char* aMsg, nsIDocument* aDocumentNode);

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
void DocCreate(const char* aMsg, nsIDocument* aDocumentNode,
               DocAccessible* aDocument = nullptr);

/**
 * Log the document was destroyed.
 */
void DocDestroy(const char* aMsg, nsIDocument* aDocumentNode,
                DocAccessible* aDocument = nullptr);

/**
 * Log the outer document was destroyed.
 */
void OuterDocDestroy(OuterDocAccessible* OuterDoc);

/**
 * Log the selection change.
 */
void SelChange(nsISelection* aSelection, DocAccessible* aDocument);

/**
 * Log the message ('title: text' format) on new line. Print the start and end
 * boundaries of the message body designated by '{' and '}' (2 spaces indent for
 * body).
 */
void MsgBegin(const char* aTitle, const char* aMsgText, ...);
void MsgEnd();

/**
 * Log the entry into message body (4 spaces indent).
 */
void MsgEntry(const char* aEntryText, ...);

/**
 * Log the text, two spaces offset is used.
 */
void Text(const char* aText);

/**
 * Log the accessible object address as message entry (4 spaces indent).
 */
void Address(const char* aDescr, Accessible* aAcc);

/**
 * Log the DOM node info as message entry.
 */
void Node(const char* aDescr, nsINode* aNode);

/**
 * Log the call stack, two spaces offset is used.
 */
void Stack();

/**
 * Enable logging of the specified modules, all other modules aren't logged.
 */
void Enable(const nsAFlatCString& aModules);

/**
 * Enable logging of modules specified by A11YLOG environment variable,
 * all other modules aren't logged.
 */
void CheckEnv();

} // namespace logs
} // namespace a11y
} // namespace mozilla

#endif

