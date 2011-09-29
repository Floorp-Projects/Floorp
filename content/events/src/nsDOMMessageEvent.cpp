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
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jeff Walden <jwalden+code@mit.edu> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsDOMMessageEvent.h"
#include "nsContentUtils.h"
#include "jsapi.h"

NS_IMPL_CYCLE_COLLECTION_CLASS(nsDOMMessageEvent)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(nsDOMMessageEvent, nsDOMEvent)
  if (tmp->mDataRooted) {
    tmp->UnrootData();
  }
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mSource)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsDOMMessageEvent, nsDOMEvent)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mSource)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(nsDOMMessageEvent)
  if (JSVAL_IS_GCTHING(tmp->mData)) {
    void *gcThing = JSVAL_TO_GCTHING(tmp->mData);
    NS_IMPL_CYCLE_COLLECTION_TRACE_JS_CALLBACK(gcThing, "mData")
  }
NS_IMPL_CYCLE_COLLECTION_TRACE_END

DOMCI_DATA(MessageEvent, nsDOMMessageEvent)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(nsDOMMessageEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMessageEvent)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(MessageEvent)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEvent)

NS_IMPL_ADDREF_INHERITED(nsDOMMessageEvent, nsDOMEvent)
NS_IMPL_RELEASE_INHERITED(nsDOMMessageEvent, nsDOMEvent)

nsDOMMessageEvent::nsDOMMessageEvent(nsPresContext* aPresContext,
                                     nsEvent* aEvent)
  : nsDOMEvent(aPresContext, aEvent),
    mData(JSVAL_VOID),
    mDataRooted(false)
{
}

nsDOMMessageEvent::~nsDOMMessageEvent()
{
  if (mDataRooted)
    UnrootData();
}

void
nsDOMMessageEvent::RootData()
{
  NS_ASSERTION(!mDataRooted, "...");
  NS_HOLD_JS_OBJECTS(this, nsDOMMessageEvent);
  mDataRooted = true;
}

void
nsDOMMessageEvent::UnrootData()
{
  NS_ASSERTION(mDataRooted, "...");
  NS_DROP_JS_OBJECTS(this, nsDOMMessageEvent);
  mDataRooted = false;
  mData = JSVAL_VOID;
}

NS_IMETHODIMP
nsDOMMessageEvent::GetData(JSContext* aCx, jsval* aData)
{
  *aData = mData;
  if (!JS_WrapValue(aCx, aData))
    return NS_ERROR_FAILURE;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMMessageEvent::GetOrigin(nsAString& aOrigin)
{
  aOrigin = mOrigin;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMMessageEvent::GetLastEventId(nsAString& aLastEventId)
{
  aLastEventId = mLastEventId;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMMessageEvent::GetSource(nsIDOMWindow** aSource)
{
  NS_IF_ADDREF(*aSource = mSource);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMMessageEvent::InitMessageEvent(const nsAString& aType,
                                    bool aCanBubble,
                                    bool aCancelable,
                                    const jsval& aData,
                                    const nsAString& aOrigin,
                                    const nsAString& aLastEventId,
                                    nsIDOMWindow* aSource)
{
  nsresult rv = nsDOMEvent::InitEvent(aType, aCanBubble, aCancelable);
  NS_ENSURE_SUCCESS(rv, rv);

  // Allowing double-initialization seems a little silly, but we have a test
  // for it so it might be important ...
  if (mDataRooted)
    UnrootData();
  mData = aData;
  RootData();
  mOrigin = aOrigin;
  mLastEventId = aLastEventId;
  mSource = aSource;

  return NS_OK;
}

nsresult
NS_NewDOMMessageEvent(nsIDOMEvent** aInstancePtrResult,
                      nsPresContext* aPresContext,
                      nsEvent* aEvent) 
{
  nsDOMMessageEvent* it = new nsDOMMessageEvent(aPresContext, aEvent);

  return CallQueryInterface(it, aInstancePtrResult);
}
