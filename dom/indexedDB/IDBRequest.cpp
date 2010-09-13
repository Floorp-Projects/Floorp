/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
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
 * The Original Code is Indexed Database.
 *
 * The Initial Developer of the Original Code is
 * The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Shawn Wilsher <me@shawnwilsher.com>
 *   Ben Turner <bent.mozilla@gmail.com>
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

#include "IDBRequest.h"

#include "nsIScriptContext.h"
#include "nsIVariant.h"

#include "nsComponentManagerUtils.h"
#include "nsDOMClassInfo.h"
#include "nsDOMJSUtils.h"
#include "nsPIDOMWindow.h"
#include "nsStringGlue.h"
#include "nsThreadUtils.h"

#include "IDBEvents.h"

USING_INDEXEDDB_NAMESPACE

// static
already_AddRefed<IDBRequest>
IDBRequest::Create(nsISupports* aSource,
                   nsIScriptContext* aScriptContext,
                   nsPIDOMWindow* aOwner)
{
  if (!aScriptContext || !aOwner) {
    NS_ERROR("Null context and owner!");
    return nsnull;
  }

  nsRefPtr<IDBRequest> request(new IDBRequest());

  request->mSource = aSource;
  request->mScriptContext = aScriptContext;
  request->mOwner = aOwner;

  return request.forget();
}

NS_IMETHODIMP
IDBRequest::GetReadyState(PRUint16* aReadyState)
{
  *aReadyState = mReadyState;
  return NS_OK;
}

NS_IMETHODIMP
IDBRequest::SetOnsuccess(nsIDOMEventListener* aSuccessListener)
{
  return RemoveAddEventListener(NS_LITERAL_STRING(SUCCESS_EVT_STR),
                                mOnSuccessListener, aSuccessListener);
}

NS_IMETHODIMP
IDBRequest::GetOnsuccess(nsIDOMEventListener** aSuccessListener)
{
  return GetInnerEventListener(mOnSuccessListener, aSuccessListener);
}

NS_IMETHODIMP
IDBRequest::SetOnerror(nsIDOMEventListener* aErrorListener)
{
  return RemoveAddEventListener(NS_LITERAL_STRING(ERROR_EVT_STR),
                                mOnErrorListener, aErrorListener);
}

NS_IMETHODIMP
IDBRequest::GetOnerror(nsIDOMEventListener** aErrorListener)
{
  return GetInnerEventListener(mOnErrorListener, aErrorListener);
}

NS_IMPL_CYCLE_COLLECTION_CLASS(IDBRequest)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(IDBRequest,
                                                  nsDOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mOnSuccessListener)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mOnErrorListener)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mSource)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(IDBRequest,
                                                nsDOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mOnSuccessListener)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mOnErrorListener)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mSource)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(IDBRequest)
  NS_INTERFACE_MAP_ENTRY(nsIIDBRequest)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(IDBRequest)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(IDBRequest, nsDOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(IDBRequest, nsDOMEventTargetHelper)

DOMCI_DATA(IDBRequest, IDBRequest)
