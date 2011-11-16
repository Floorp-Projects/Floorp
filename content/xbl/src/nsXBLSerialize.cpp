/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "nsXBLSerialize.h"
#include "nsDOMScriptObjectHolder.h"
#include "nsContentUtils.h"
#include "jsxdrapi.h"

nsresult
XBL_SerializeFunction(nsIScriptContext* aContext,
                      nsIObjectOutputStream* aStream,
                      JSObject* aFunctionObject)
{
  JSContext* cx = aContext->GetNativeContext();
  JSXDRState *xdr = ::JS_XDRNewMem(cx, JSXDR_ENCODE);
  if (!xdr)
    return NS_ERROR_OUT_OF_MEMORY;
  xdr->userdata = static_cast<void*>(aStream);

  JSAutoRequest ar(cx);
  nsresult rv;
  if (!JS_XDRFunctionObject(xdr, &aFunctionObject)) {
    rv = NS_ERROR_FAILURE;
  } else {
    uint32 size;
    const char* data = reinterpret_cast<const char*>
                                       (JS_XDRMemGetData(xdr, &size));
    NS_ASSERTION(data, "no decoded JSXDRState data!");

    rv = aStream->Write32(size);
    if (NS_SUCCEEDED(rv))
      rv = aStream->WriteBytes(data, size);
  }

  JS_XDRDestroy(xdr);

  return rv;
}

// static
nsresult
XBL_DeserializeFunction(nsIScriptContext* aContext,
                        nsIObjectInputStream* aStream,
                        JSObject** aFunctionObject)
{
  *aFunctionObject = nsnull;

  PRUint32 size;
  nsresult rv = aStream->Read32(&size);
  if (NS_FAILED(rv))
    return rv;

  char* data;
  rv = aStream->ReadBytes(size, &data);
  if (NS_FAILED(rv))
    return rv;

  JSContext* cx = aContext->GetNativeContext();
  JSXDRState *xdr = JS_XDRNewMem(cx, JSXDR_DECODE);
  if (!xdr) {
    rv = NS_ERROR_OUT_OF_MEMORY;
  } else {
    xdr->userdata = static_cast<void*>(aStream);
    JSAutoRequest ar(cx);
    JS_XDRMemSetData(xdr, data, size);

    if (!JS_XDRFunctionObject(xdr, aFunctionObject)) {
      rv = NS_ERROR_FAILURE;
    }

    uint32 junk;
    data = static_cast<char*>(JS_XDRMemGetData(xdr, &junk));
    JS_XDRMemSetData(xdr, NULL, 0);
    JS_XDRDestroy(xdr);
  }

  nsMemory::Free(data);
  NS_ENSURE_SUCCESS(rv, rv);
  return rv;
}
