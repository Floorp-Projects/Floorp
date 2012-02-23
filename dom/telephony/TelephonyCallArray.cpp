/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=40: */
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
 * The Original Code is Telephony.
 *
 * The Initial Developer of the Original Code is
 *   The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2012
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

#include "TelephonyCallArray.h"

// For NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER...
#include "nsINode.h"

#include "dombindings.h"
#include "nsDOMClassInfo.h"

#include "Telephony.h"
#include "TelephonyCall.h"

USING_TELEPHONY_NAMESPACE

TelephonyCallArray::TelephonyCallArray(Telephony* aTelephony)
: mTelephony(aTelephony)
{
  SetIsProxy();
}

TelephonyCallArray::~TelephonyCallArray()
{ }

// static
already_AddRefed<TelephonyCallArray>
TelephonyCallArray::Create(Telephony* aTelephony)
{
  nsRefPtr<TelephonyCallArray> array = new TelephonyCallArray(aTelephony);
  return array.forget();
}

NS_IMPL_CYCLE_COLLECTION_CLASS(TelephonyCallArray)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(TelephonyCallArray)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mTelephony");
  cb.NoteXPCOMChild(tmp->mTelephony->ToISupports());
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(TelephonyCallArray)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(TelephonyCallArray)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mTelephony)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(TelephonyCallArray)
NS_IMPL_CYCLE_COLLECTING_RELEASE(TelephonyCallArray)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(TelephonyCallArray)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(TelephonyCallArray)
  NS_INTERFACE_MAP_ENTRY(nsIDOMTelephonyCallArray)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

DOMCI_DATA(TelephonyCallArray, TelephonyCallArray)

NS_IMETHODIMP
TelephonyCallArray::GetLength(PRUint32* aLength)
{
  NS_ASSERTION(aLength, "Null pointer!");
  *aLength = mTelephony->Calls().Length();
  return NS_OK;
}

nsIDOMTelephonyCall*
TelephonyCallArray::GetCallAt(PRUint32 aIndex)
{
  return mTelephony->Calls().SafeElementAt(aIndex);
}

nsISupports*
TelephonyCallArray::GetParentObject() const
{
  return mTelephony->ToISupports();
}

JSObject*
TelephonyCallArray::WrapObject(JSContext* aCx, XPCWrappedNativeScope* aScope,
                               bool* aTriedToWrap)
{
  return mozilla::dom::binding::TelephonyCallArray::create(aCx, aScope, this,
                                                           aTriedToWrap);
}
