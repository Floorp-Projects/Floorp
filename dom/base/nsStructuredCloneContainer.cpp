/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sw=2 et tw=80:
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 *
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Justin Lebar <justin.lebar@gmail.com>
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

#include "nsStructuredCloneContainer.h"

#include "nsCOMPtr.h"
#include "nsIDocument.h"
#include "nsIJSContextStack.h"
#include "nsIScriptContext.h"
#include "nsIVariant.h"
#include "nsServiceManagerUtils.h"
#include "nsContentUtils.h"
#include "xpcprivate.h"

NS_IMPL_ADDREF(nsStructuredCloneContainer)
NS_IMPL_RELEASE(nsStructuredCloneContainer)

NS_INTERFACE_MAP_BEGIN(nsStructuredCloneContainer)
  NS_INTERFACE_MAP_ENTRY(nsIStructuredCloneContainer)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

nsStructuredCloneContainer::nsStructuredCloneContainer()
  : mData(nsnull), mSize(0), mVersion(0)
{
}

nsStructuredCloneContainer::~nsStructuredCloneContainer()
{
  free(mData);
}

nsresult
nsStructuredCloneContainer::InitFromVariant(nsIVariant *aData, JSContext *aCx)
{
  NS_ENSURE_STATE(!mData);
  NS_ENSURE_ARG_POINTER(aData);
  NS_ENSURE_ARG_POINTER(aCx);

  // First, try to extract a jsval from the variant |aData|.  This works only
  // if the variant implements GetAsJSVal.
  jsval jsData;
  nsresult rv = aData->GetAsJSVal(&jsData);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_UNEXPECTED);

  // Make sure that we serialize in the right context.
  JSAutoRequest ar(aCx);
  JSAutoEnterCompartment ac;
  NS_ENSURE_STATE(ac.enter(aCx, JS_GetGlobalObject(aCx)));

  nsCxPusher cxPusher;
  cxPusher.Push(aCx);

  PRUint64* jsBytes = nsnull;
  bool success = JS_WriteStructuredClone(aCx, jsData, &jsBytes, &mSize,
                                           nsnull, nsnull);
  NS_ENSURE_STATE(success);
  NS_ENSURE_STATE(jsBytes);

  // Copy jsBytes into our own buffer.
  mData = (PRUint64*) malloc(mSize);
  if (!mData) {
    mSize = 0;
    mVersion = 0;

    // FIXME This should really be js::Foreground::Free, but that's not public.
    JS_free(aCx, jsBytes);

    return NS_ERROR_FAILURE;
  }
  else {
    mVersion = JS_STRUCTURED_CLONE_VERSION;
  }

  memcpy(mData, jsBytes, mSize);

  // FIXME Similarly, this should be js::Foreground::free.
  JS_free(aCx, jsBytes);
  return NS_OK;
}

nsresult
nsStructuredCloneContainer::InitFromBase64(const nsAString &aData,
                                           PRUint32 aFormatVersion,
                                           JSContext *aCx)
{
  NS_ENSURE_STATE(!mData);

  NS_ConvertUTF16toUTF8 data(aData);

  nsCAutoString binaryData;
  nsresult rv = nsXPConnect::Base64Decode(data, binaryData);
  NS_ENSURE_SUCCESS(rv, rv);

  // Copy the string's data into our own buffer.
  mData = (PRUint64*) malloc(binaryData.Length());
  NS_ENSURE_STATE(mData);
  memcpy(mData, binaryData.get(), binaryData.Length());

  mSize = binaryData.Length();
  mVersion = aFormatVersion;
  return NS_OK;
}


nsresult
nsStructuredCloneContainer::DeserializeToVariant(JSContext *aCx,
                                                 nsIVariant **aData)
{
  NS_ENSURE_STATE(mData);
  NS_ENSURE_ARG_POINTER(aData);
  *aData = nsnull;

  // Deserialize to a jsval.
  jsval jsStateObj;
  bool success = JS_ReadStructuredClone(aCx, mData, mSize, mVersion,
                                          &jsStateObj, nsnull, nsnull);
  NS_ENSURE_STATE(success);

  // Now wrap the jsval as an nsIVariant.
  nsCOMPtr<nsIVariant> varStateObj;
  nsCOMPtr<nsIXPConnect> xpconnect = do_GetService(nsIXPConnect::GetCID());
  NS_ENSURE_STATE(xpconnect);
  xpconnect->JSValToVariant(aCx, &jsStateObj, getter_AddRefs(varStateObj));
  NS_ENSURE_STATE(varStateObj);

  NS_IF_ADDREF(*aData = varStateObj);
  return NS_OK;
}

nsresult
nsStructuredCloneContainer::GetDataAsBase64(nsAString &aOut)
{
  NS_ENSURE_STATE(mData);
  aOut.Truncate();

  nsCAutoString binaryData(reinterpret_cast<char*>(mData), mSize);
  nsCAutoString base64Data;
  nsresult rv = nsXPConnect::Base64Encode(binaryData, base64Data);
  NS_ENSURE_SUCCESS(rv, rv);

  aOut.Assign(NS_ConvertASCIItoUTF16(base64Data));
  return NS_OK;
}

nsresult
nsStructuredCloneContainer::GetSerializedNBytes(PRUint64 *aSize)
{
  NS_ENSURE_STATE(mData);
  NS_ENSURE_ARG_POINTER(aSize);

  // mSize is a size_t, while aSize is a PRUint64.  We rely on an implicit cast
  // here so that we'll get a compile error if a size_t-to-uint64 cast is
  // narrowing.
  *aSize = mSize;

  return NS_OK;
}

nsresult
nsStructuredCloneContainer::GetFormatVersion(PRUint32 *aFormatVersion)
{
  NS_ENSURE_STATE(mData);
  NS_ENSURE_ARG_POINTER(aFormatVersion);
  *aFormatVersion = mVersion;
  return NS_OK;
}
