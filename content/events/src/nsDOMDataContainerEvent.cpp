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
 *   Alexander Surkov <surkov.alexander@gmail.com> (original author)
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

#include "nsDOMDataContainerEvent.h"

#include "nsContentUtils.h"

nsDOMDataContainerEvent::nsDOMDataContainerEvent(nsPresContext *aPresContext,
                                                 nsEvent *aEvent)
  : nsDOMEvent(aPresContext, aEvent)
{
  mData.Init();
}

NS_IMPL_CYCLE_COLLECTION_CLASS(nsDOMDataContainerEvent)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(nsDOMDataContainerEvent,
                                                nsDOMEvent)
  if (tmp->mData.IsInitialized())
    tmp->mData.Clear();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsDOMDataContainerEvent,
                                                  nsDOMEvent)
  tmp->mData.EnumerateRead(TraverseEntry, &cb);
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_ADDREF_INHERITED(nsDOMDataContainerEvent, nsDOMEvent)
NS_IMPL_RELEASE_INHERITED(nsDOMDataContainerEvent, nsDOMEvent)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(nsDOMDataContainerEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMDataContainerEvent)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(DataContainerEvent)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEvent)

NS_IMETHODIMP
nsDOMDataContainerEvent::GetData(const nsAString& aKey, nsIVariant **aData)
{
  NS_ENSURE_ARG_POINTER(aData);

  NS_ENSURE_STATE(mData.IsInitialized());

  mData.Get(aKey, aData);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMDataContainerEvent::SetData(const nsAString& aKey, nsIVariant *aData)
{
  NS_ENSURE_ARG(aData);

  // Make sure this event isn't already being dispatched.
  NS_ENSURE_STATE(!(NS_IS_EVENT_IN_DISPATCH(mEvent) ||
                   (mEvent->flags & NS_EVENT_FLAG_STOP_DISPATCH_IMMEDIATELY)));

  NS_ENSURE_STATE(mData.IsInitialized());
  return mData.Put(aKey, aData) ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

nsresult
NS_NewDOMDataContainerEvent(nsIDOMEvent** aInstancePtrResult,
                   nsPresContext* aPresContext,
                   nsEvent* aEvent)
{
  nsDOMDataContainerEvent* it =
    new nsDOMDataContainerEvent(aPresContext, aEvent);
  NS_ENSURE_TRUE(it, NS_ERROR_OUT_OF_MEMORY);

  return CallQueryInterface(it, aInstancePtrResult);
}

PLDHashOperator
nsDOMDataContainerEvent::TraverseEntry(const nsAString& aKey,
                                       nsIVariant *aDataItem,
                                       void* aUserArg)
{
  nsCycleCollectionTraversalCallback *cb =
    static_cast<nsCycleCollectionTraversalCallback*>(aUserArg);
  cb->NoteXPCOMChild(aDataItem);

  return PL_DHASH_NEXT;
}

