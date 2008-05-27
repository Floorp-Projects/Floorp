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

NS_IMPL_CYCLE_COLLECTION_CLASS(nsDOMMessageEvent)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(nsDOMMessageEvent, nsDOMEvent)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mSource)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsDOMMessageEvent, nsDOMEvent)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mSource)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(nsDOMMessageEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMessageEvent)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(MessageEvent)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEvent)

NS_IMPL_ADDREF_INHERITED(nsDOMMessageEvent, nsDOMEvent)
NS_IMPL_RELEASE_INHERITED(nsDOMMessageEvent, nsDOMEvent)

NS_IMETHODIMP
nsDOMMessageEvent::GetData(nsAString& aData)
{
  aData = mData;
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
                                    PRBool aCanBubble,
                                    PRBool aCancelable,
                                    const nsAString& aData,
                                    const nsAString& aOrigin,
                                    const nsAString& aLastEventId,
                                    nsIDOMWindow* aSource)
{
  nsresult rv = nsDOMEvent::InitEvent(aType, aCanBubble, aCancelable);
  NS_ENSURE_SUCCESS(rv, rv);

  mData = aData;
  mOrigin = aOrigin;
  mLastEventId = aLastEventId;
  mSource = aSource;

  return NS_OK;
}

NS_IMETHODIMP
nsDOMMessageEvent::InitMessageEventNS(const nsAString& aNamespaceURI,
                                      const nsAString& aType,
                                      PRBool aCanBubble,
                                      PRBool aCancelable,
                                      const nsAString& aData,
                                      const nsAString& aOrigin,
                                      const nsAString& aLastEventId,
                                      nsIDOMWindow* aSource)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
NS_NewDOMMessageEvent(nsIDOMEvent** aInstancePtrResult,
                      nsPresContext* aPresContext,
                      nsEvent* aEvent) 
{
  nsDOMMessageEvent* it = new nsDOMMessageEvent(aPresContext, aEvent);
  if (nsnull == it)
    return NS_ERROR_OUT_OF_MEMORY;

  return CallQueryInterface(it, aInstancePtrResult);
}
