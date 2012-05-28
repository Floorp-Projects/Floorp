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
class nsAccessible;
class DocAccessible;
class nsIDocument;
class nsIRequest;
class nsIWebProgress;

namespace mozilla {
namespace a11y {
namespace logging {

enum EModules {
  eDocLoad = 1 << 0,
  eDocCreate = 1 << 1,
  eDocDestroy = 1 << 2,
  eDocLifeCycle = eDocLoad | eDocCreate | eDocDestroy
};

/**
 * Return true if the given module is logged.
 */
bool IsEnabled(PRUint32 aModule);

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
               DocAccessible* aDocument = nsnull);

/**
 * Log the document was destroyed.
 */
void DocDestroy(const char* aMsg, nsIDocument* aDocumentNode,
                DocAccessible* aDocument = nsnull);

/**
 * Log the message, a piece of text on own line, no offset.
 */
void Msg(const char* aMsg);

/**
 * Log the text, two spaces offset is used.
 */
void Text(const char* aText);

/**
 * Log the accesisble object address, two spaces offset is used.
 */
void Address(const char* aDescr, nsAccessible* aAcc);

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

