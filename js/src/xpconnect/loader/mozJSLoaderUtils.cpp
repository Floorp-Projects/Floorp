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
 * The Original Code is mozilla.org
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010-2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Michael Wu <mwu@mozilla.com>
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

#include "nsAutoPtr.h"
#include "nsScriptLoader.h"

#include "jsapi.h"
#include "jsxdrapi.h"

#include "mozilla/scache/StartupCache.h"
#include "mozilla/scache/StartupCacheUtils.h"

using namespace mozilla::scache;

static nsresult
ReadScriptFromStream(JSContext *cx, nsIObjectInputStream *stream,
                     JSObject **scriptObj)
{
    *scriptObj = nsnull;

    PRUint32 size;
    nsresult rv = stream->Read32(&size);
    NS_ENSURE_SUCCESS(rv, rv);

    char *data;
    rv = stream->ReadBytes(size, &data);
    NS_ENSURE_SUCCESS(rv, rv);

    JSXDRState *xdr = JS_XDRNewMem(cx, JSXDR_DECODE);
    NS_ENSURE_TRUE(xdr, NS_ERROR_OUT_OF_MEMORY);

    xdr->userdata = stream;
    JS_XDRMemSetData(xdr, data, size);

    if (!JS_XDRScriptObject(xdr, scriptObj)) {
        rv = NS_ERROR_FAILURE;
    }

    // Update data in case ::JS_XDRScript called back into C++ code to
    // read an XPCOM object.
    //
    // In that case, the serialization process must have flushed a run
    // of counted bytes containing JS data at the point where the XPCOM
    // object starts, after which an encoding C++ callback from the JS
    // XDR code must have written the XPCOM object directly into the
    // nsIObjectOutputStream.
    //
    // The deserialization process will XDR-decode counted bytes up to
    // but not including the XPCOM object, then call back into C++ to
    // read the object, then read more counted bytes and hand them off
    // to the JSXDRState, so more JS data can be decoded.
    //
    // This interleaving of JS XDR data and XPCOM object data may occur
    // several times beneath the call to ::JS_XDRScript, above.  At the
    // end of the day, we need to free (via nsMemory) the data owned by
    // the JSXDRState.  So we steal it back, nulling xdr's buffer so it
    // doesn't get passed to ::JS_free by ::JS_XDRDestroy.

    uint32 length;
    data = static_cast<char*>(JS_XDRMemGetData(xdr, &length));
    JS_XDRMemSetData(xdr, nsnull, 0);
    JS_XDRDestroy(xdr);

    // If data is null now, it must have been freed while deserializing an
    // XPCOM object (e.g., a principal) beneath ::JS_XDRScript.
    nsMemory::Free(data);

    return rv;
}

static nsresult
WriteScriptToStream(JSContext *cx, JSObject *scriptObj,
                    nsIObjectOutputStream *stream)
{
    JSXDRState *xdr = JS_XDRNewMem(cx, JSXDR_ENCODE);
    NS_ENSURE_TRUE(xdr, NS_ERROR_OUT_OF_MEMORY);

    xdr->userdata = stream;
    nsresult rv = NS_OK;

    if (JS_XDRScriptObject(xdr, &scriptObj)) {
        // Get the encoded JSXDRState data and write it.  The JSXDRState owns
        // this buffer memory and will free it beneath ::JS_XDRDestroy.
        //
        // If an XPCOM object needs to be written in the midst of the JS XDR
        // encoding process, the C++ code called back from the JS engine (e.g.,
        // nsEncodeJSPrincipals in caps/src/nsJSPrincipals.cpp) will flush data
        // from the JSXDRState to aStream, then write the object, then return
        // to JS XDR code with xdr reset so new JS data is encoded at the front
        // of the xdr's data buffer.
        //
        // However many XPCOM objects are interleaved with JS XDR data in the
        // stream, when control returns here from ::JS_XDRScript, we'll have
        // one last buffer of data to write to aStream.

        uint32 size;
        const char* data = reinterpret_cast<const char*>
                                           (JS_XDRMemGetData(xdr, &size));
        NS_ASSERTION(data, "no decoded JSXDRState data!");

        rv = stream->Write32(size);
        if (NS_SUCCEEDED(rv)) {
            rv = stream->WriteBytes(data, size);
        }
    } else {
        rv = NS_ERROR_FAILURE; // likely to be a principals serialization error
    }

    JS_XDRDestroy(xdr);
    return rv;
}

nsresult
ReadCachedScript(StartupCache* cache, nsACString &uri, JSContext *cx, JSObject **scriptObj)
{
    nsresult rv;

    nsAutoArrayPtr<char> buf;
    PRUint32 len;
    rv = cache->GetBuffer(PromiseFlatCString(uri).get(), getter_Transfers(buf),
                          &len);
    if (NS_FAILED(rv)) {
        return rv; // don't warn since NOT_AVAILABLE is an ok error
    }

    nsCOMPtr<nsIObjectInputStream> ois;
    rv = NewObjectInputStreamFromBuffer(buf, len, getter_AddRefs(ois));
    NS_ENSURE_SUCCESS(rv, rv);
    buf.forget();

    return ReadScriptFromStream(cx, ois, scriptObj);
}

nsresult
WriteCachedScript(StartupCache* cache, nsACString &uri, JSContext *cx, JSObject *scriptObj)
{
    nsresult rv;

    nsCOMPtr<nsIObjectOutputStream> oos;
    nsCOMPtr<nsIStorageStream> storageStream;
    rv = NewObjectOutputWrappedStorageStream(getter_AddRefs(oos),
                                             getter_AddRefs(storageStream),
                                             true);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = WriteScriptToStream(cx, scriptObj, oos);
    oos->Close();
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoArrayPtr<char> buf;
    PRUint32 len;
    rv = NewBufferFromStorageStream(storageStream, getter_Transfers(buf), &len);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = cache->PutBuffer(PromiseFlatCString(uri).get(), buf, len);
    return rv;
}
