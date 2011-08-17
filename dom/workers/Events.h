/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
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
 * The Original Code is Web Workers.
 *
 * The Initial Developer of the Original Code is
 *   The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ben Turner <bent.mozilla@gmail.com> (Original Author)
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

#ifndef mozilla_dom_workers_events_h__
#define mozilla_dom_workers_events_h__

#include "Workers.h"

#include "jspubtd.h"
#include "nsTArray.h"
#include "nsCOMPtr.h"

class JSAutoStructuredCloneBuffer;

BEGIN_WORKERS_NAMESPACE

namespace events {

bool
InitClasses(JSContext* aCx, JSObject* aGlobal, bool aMainRuntime);

JSObject*
CreateGenericEvent(JSContext* aCx, JSString* aType, bool aBubbles,
                   bool aCancelable, bool aMainRuntime);

JSObject*
CreateMessageEvent(JSContext* aCx, JSAutoStructuredCloneBuffer& aData,
                   nsTArray<nsCOMPtr<nsISupports> >& aClonedObjects,
                   bool aMainRuntime);

JSObject*
CreateErrorEvent(JSContext* aCx, JSString* aMessage, JSString* aFilename,
                 uint32 aLineNumber, bool aMainRuntime);

JSObject*
CreateProgressEvent(JSContext* aCx, JSString* aType, bool aLengthComputable,
                    jsdouble aLoaded, jsdouble aTotal);

bool
IsSupportedEventClass(JSContext* aCx, JSObject* aEvent);

bool
SetEventTarget(JSContext* aCx, JSObject* aEvent, JSObject* aTarget);

bool
EventWasCanceled(JSContext* aCx, JSObject* aEvent);

bool
DispatchEventToTarget(JSContext* aCx, JSObject* aTarget, JSObject* aEvent,
                      bool* aPreventDefaultCalled);

} // namespace events

END_WORKERS_NAMESPACE

#endif /* mozilla_dom_workers_events_h__ */
