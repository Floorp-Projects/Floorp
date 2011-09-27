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

#ifndef mozilla_dom_workers_listenermanager_h__
#define mozilla_dom_workers_listenermanager_h__

#include "Workers.h"

#include "jsapi.h"
#include "prclist.h"

BEGIN_WORKERS_NAMESPACE

namespace events {

// XXX Current impl doesn't divide into phases.
// XXX Current impl doesn't handle event target chains.
class ListenerManager
{
public:
  enum Phase
  {
    All = 0,
    Capturing,
    Onfoo,
    Bubbling
  };

private:
  PRCList mCollectionHead;

public:
  ListenerManager()
  {
    PR_INIT_CLIST(&mCollectionHead);
  }

#ifdef DEBUG
  ~ListenerManager();
#endif

  void
  Trace(JSTracer* aTrc)
  {
    if (!PR_CLIST_IS_EMPTY(&mCollectionHead)) {
      TraceInternal(aTrc);
    }
  }

  void
  Finalize(JSContext* aCx)
  {
    if (!PR_CLIST_IS_EMPTY(&mCollectionHead)) {
      FinalizeInternal(aCx);
    }
  }

  bool
  AddEventListener(JSContext* aCx, JSString* aType, jsval aListener,
                   bool aCapturing, bool aWantsUntrusted)
  {
    return Add(aCx, aType, aListener, aCapturing ? Capturing : Bubbling,
               aWantsUntrusted);
  }

  bool
  SetEventListener(JSContext* aCx, JSString* aType, jsval aListener);

  bool
  RemoveEventListener(JSContext* aCx, JSString* aType, jsval aListener,
                      bool aCapturing)
  {
    if (PR_CLIST_IS_EMPTY(&mCollectionHead)) {
      return true;
    }
    return Remove(aCx, aType, aListener, aCapturing ? Capturing : Bubbling,
                  true);
  }

  bool
  GetEventListener(JSContext* aCx, JSString* aType, jsval* aListener);

  bool
  DispatchEvent(JSContext* aCx, JSObject* aTarget, JSObject* aEvent,
                bool* aPreventDefaultCalled);

  bool
  HasListeners()
  {
    return !PR_CLIST_IS_EMPTY(&mCollectionHead);
  }

  bool
  HasListenersForType(JSContext* aCx, JSString* aType)
  {
    return HasListeners() && HasListenersForTypeInternal(aCx, aType);
  }

  bool
  HasListenersForTypeInternal(JSContext* aCx, JSString* aType);

private:
  void
  TraceInternal(JSTracer* aTrc);

  void
  FinalizeInternal(JSContext* aCx);

  bool
  Add(JSContext* aCx, JSString* aType, jsval aListener, Phase aPhase,
      bool aWantsUntrusted);

  bool
  Remove(JSContext* aCx, JSString* aType, jsval aListener, Phase aPhase,
         bool aClearEmpty);
};

} // namespace events

END_WORKERS_NAMESPACE

#endif /* mozilla_dom_workers_listenermanager_h__ */
