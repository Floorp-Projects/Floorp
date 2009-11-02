/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 *   Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mark Steele <mwsteele@gmail.com>
 *   Vladimir Vukicevic <vladimir@pobox.com>
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

#ifndef WEBGLARRAYS_H_
#define WEBGLARRAYS_H_

#include <stdarg.h>

#include "nsTArray.h"
#include "nsDataHashtable.h"
#include "nsRefPtrHashtable.h"
#include "nsHashKeys.h"

#include "nsICanvasRenderingContextWebGL.h"
#include "nsICanvasRenderingContextInternal.h"
#include "nsIJSNativeInitializer.h"

#include "SimpleBuffer.h"

namespace mozilla {

//
// array wrapper classes
//

// XXX refactor buffer stuff
class WebGLArrayBuffer :
    public nsICanvasArrayBuffer,
    public nsIJSNativeInitializer,
    public SimpleBuffer
{
public:

    WebGLArrayBuffer() { }
    WebGLArrayBuffer(PRUint32 length);

    NS_DECL_ISUPPORTS
    NS_DECL_NSICANVASARRAYBUFFER

    NS_IMETHOD Initialize(nsISupports* aOwner,
                          JSContext* aCx,
                          JSObject* aObj,
                          PRUint32 aArgc,
                          jsval* aArgv);
};

class WebGLFloatArray :
    public nsICanvasFloatArray,
    public nsIJSNativeInitializer
{
public:
    WebGLFloatArray() :
        mOffset(0), mLength(0) { }

    WebGLFloatArray(PRUint32 length);
    WebGLFloatArray(WebGLArrayBuffer *buffer, PRUint32 offset, PRUint32 length);
    WebGLFloatArray(JSContext *cx, JSObject *arrayObj, jsuint arrayLen);

    NS_DECL_ISUPPORTS
    NS_DECL_NSICANVASARRAY
    NS_DECL_NSICANVASFLOATARRAY

    NS_IMETHOD Initialize(nsISupports* aOwner,
                          JSContext* aCx,
                          JSObject* aObj,
                          PRUint32 aArgc,
                          jsval* aArgv);

    void Set(PRUint32 index, float value);

protected:
    nsRefPtr<WebGLArrayBuffer> mBuffer;
    PRUint32 mOffset;
    PRUint32 mLength;
    PRUint32 mSize;
    PRUint32 mElementSize;
    PRUint32 mCount;
};

class WebGLByteArray :
    public nsICanvasByteArray,
    public nsIJSNativeInitializer
{
public:
    WebGLByteArray() :
        mOffset(0), mLength(0) { }

    WebGLByteArray(PRUint32 length);
    WebGLByteArray(WebGLArrayBuffer *buffer, PRUint32 offset, PRUint32 length);
    WebGLByteArray(JSContext *cx, JSObject *arrayObj, jsuint arrayLen);

    NS_DECL_ISUPPORTS
    NS_DECL_NSICANVASARRAY
    NS_DECL_NSICANVASBYTEARRAY

    NS_IMETHOD Initialize(nsISupports* aOwner,
                          JSContext* aCx,
                          JSObject* aObj,
                          PRUint32 aArgc,
                          jsval* aArgv);

    void Set(PRUint32 index, char value);

protected:
    nsRefPtr<WebGLArrayBuffer> mBuffer;
    PRUint32 mOffset;
    PRUint32 mLength;
    PRUint32 mSize;
    PRUint32 mElementSize;
    PRUint32 mCount;
};

class WebGLUnsignedByteArray :
    public nsICanvasUnsignedByteArray,
    public nsIJSNativeInitializer
{
public:
    WebGLUnsignedByteArray() :
        mOffset(0), mLength(0) { }

    WebGLUnsignedByteArray(PRUint32 length);
    WebGLUnsignedByteArray(WebGLArrayBuffer *buffer, PRUint32 offset, PRUint32 length);
    WebGLUnsignedByteArray(JSContext *cx, JSObject *arrayObj, jsuint arrayLen);

    NS_DECL_ISUPPORTS
    NS_DECL_NSICANVASARRAY
    NS_DECL_NSICANVASUNSIGNEDBYTEARRAY

    NS_IMETHOD Initialize(nsISupports* aOwner,
                          JSContext* aCx,
                          JSObject* aObj,
                          PRUint32 aArgc,
                          jsval* aArgv);

    void Set(PRUint32 index, unsigned char value);

protected:
    nsRefPtr<WebGLArrayBuffer> mBuffer;
    PRUint32 mOffset;
    PRUint32 mLength;
    PRUint32 mSize;
    PRUint32 mElementSize;
    PRUint32 mCount;
};

class WebGLShortArray :
    public nsICanvasShortArray,
    public nsIJSNativeInitializer
{
public:
    WebGLShortArray() :
        mOffset(0), mLength(0) { }

    WebGLShortArray(PRUint32 length);
    WebGLShortArray(WebGLArrayBuffer *buffer, PRUint32 offset, PRUint32 length);
    WebGLShortArray(JSContext *cx, JSObject *arrayObj, jsuint arrayLen);

    NS_DECL_ISUPPORTS
    NS_DECL_NSICANVASARRAY
    NS_DECL_NSICANVASSHORTARRAY

    NS_IMETHOD Initialize(nsISupports* aOwner,
                          JSContext* aCx,
                          JSObject* aObj,
                          PRUint32 aArgc,
                          jsval* aArgv);

    void Set(PRUint32 index, short value);

protected:
    nsRefPtr<WebGLArrayBuffer> mBuffer;
    PRUint32 mOffset;
    PRUint32 mLength;
    PRUint32 mSize;
    PRUint32 mElementSize;
    PRUint32 mCount;
};

class WebGLUnsignedShortArray :
    public nsICanvasUnsignedShortArray,
    public nsIJSNativeInitializer
{
public:
    WebGLUnsignedShortArray() :
        mOffset(0), mLength(0) { }

    WebGLUnsignedShortArray(PRUint32 length);
    WebGLUnsignedShortArray(WebGLArrayBuffer *buffer, PRUint32 offset, PRUint32 length);
    WebGLUnsignedShortArray(JSContext *cx, JSObject *arrayObj, jsuint arrayLen);

    NS_DECL_ISUPPORTS
    NS_DECL_NSICANVASARRAY
    NS_DECL_NSICANVASUNSIGNEDSHORTARRAY

    NS_IMETHOD Initialize(nsISupports* aOwner,
                          JSContext* aCx,
                          JSObject* aObj,
                          PRUint32 aArgc,
                          jsval* aArgv);

    void Set(PRUint32 index, unsigned short value);

protected:
    nsRefPtr<WebGLArrayBuffer> mBuffer;
    PRUint32 mOffset;
    PRUint32 mLength;
    PRUint32 mSize;
    PRUint32 mElementSize;
    PRUint32 mCount;
};

class WebGLIntArray :
    public nsICanvasIntArray,
    public nsIJSNativeInitializer
{
public:
    WebGLIntArray() :
        mOffset(0), mLength(0) { }

    WebGLIntArray(PRUint32 length);
    WebGLIntArray(WebGLArrayBuffer *buffer, PRUint32 offset, PRUint32 length);
    WebGLIntArray(JSContext *cx, JSObject *arrayObj, jsuint arrayLen);

    NS_DECL_ISUPPORTS
    NS_DECL_NSICANVASARRAY
    NS_DECL_NSICANVASINTARRAY

    NS_IMETHOD Initialize(nsISupports* aOwner,
                          JSContext* aCx,
                          JSObject* aObj,
                          PRUint32 aArgc,
                          jsval* aArgv);

    void Set(PRUint32 index, int value);

protected:
    nsRefPtr<WebGLArrayBuffer> mBuffer;
    PRUint32 mOffset;
    PRUint32 mLength;
    PRUint32 mSize;
    PRUint32 mElementSize;
    PRUint32 mCount;
};

class WebGLUnsignedIntArray :
    public nsICanvasUnsignedIntArray,
    public nsIJSNativeInitializer
{
public:
    WebGLUnsignedIntArray() :
        mOffset(0), mLength(0) { }

    WebGLUnsignedIntArray(PRUint32 length);
    WebGLUnsignedIntArray(WebGLArrayBuffer *buffer, PRUint32 offset, PRUint32 length);
    WebGLUnsignedIntArray(JSContext *cx, JSObject *arrayObj, jsuint arrayLen);

    NS_DECL_ISUPPORTS
    NS_DECL_NSICANVASARRAY
    NS_DECL_NSICANVASUNSIGNEDINTARRAY

    NS_IMETHOD Initialize(nsISupports* aOwner,
                          JSContext* aCx,
                          JSObject* aObj,
                          PRUint32 aArgc,
                          jsval* aArgv);

    void Set(PRUint32 index, unsigned int value);

protected:
    nsRefPtr<WebGLArrayBuffer> mBuffer;
    PRUint32 mOffset;
    PRUint32 mLength;
    PRUint32 mSize;
    PRUint32 mElementSize;
    PRUint32 mCount;
};

} /* namespace mozilla */


#endif /* WEBGLARRAYS_H_ */
