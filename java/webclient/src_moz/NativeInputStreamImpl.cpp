/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is RaptorCanvas.
 *
 * The Initial Developer of the Original Code is Kirk Baker and
 * Ian Wilkinson. Portions created by Kirk Baker and Ian Wilkinson are
 * Copyright (C) 1999 Kirk Baker and Ian Wilkinson. All
 * Rights Reserved.
 *
 * Contributor(s): Kirk Baker <kbaker@eb.com>
 *               Ian Wilkinson <iw@ennoble.com>
 *               Mark Lin <mark.lin@eng.sun.com>
 *               Mark Goddard
 *               Ed Burns <edburns@acm.org>
 *               Ashutosh Kulkarni <ashuk@eng.sun.com>
 *               Kyle Yuan <kyle.yuan@sun.com>
 */

/*
 * NativeInputStreamImpl.cpp
 */

#include "org_mozilla_webclient_impl_wrapper_0005fnative_NativeInputStream.h"

#include <nsIInputStream.h>
#include "ns_util.h"

JNIEXPORT jint JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_NativeInputStream_nativeRead
(JNIEnv *env, jobject obj, jint nativeInputStream, jbyteArray buf, jint off, 
 jint len) 
{
    nsIInputStream *stream = (nsIInputStream *) nativeInputStream;
    nsresult rv = NS_OK;
    PRUint32 ret;
    
    char *cbuf = new char[len];
    jbyte *jbytes = new jbyte[len];
    int i = 0;
    // initialize the array
    for (i = 0; i < len; i++) {
        cbuf[i] = 0;
        jbytes[i] = 0;
    }
    // call the mozilla method
    if (NS_SUCCEEDED(rv = stream->Read(cbuf, len, &ret))) {
        // copy the chars to jbytes
        for (i = 0; i < ret; i++) {
            jbytes[i] = (jbyte) cbuf[i];
        }
        // copy the jbytes to the return back to java
        env->SetByteArrayRegion(buf, off, ret, jbytes);
    }
    else {
        ::util_ThrowExceptionToJava(env, "java/io/IOException", 
                                    "can't Read from native Stream");
    }

    return (jint) ret;
}

/*
 * Class:     org_mozilla_webclient_impl_wrapper_0005fnative_NativeInputStream
 * Method:    nativeAvailable
 * Signature: (I)I
 */
JNIEXPORT jint JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_NativeInputStream_nativeAvailable
(JNIEnv *env, jobject obj, jint nativeInputStream) 
{
    nsIInputStream *stream = (nsIInputStream *) nativeInputStream;
    nsresult rv = NS_OK;
    PRUint32 result = -1;

    if (NS_FAILED(rv = stream->Available(&result))) {
        ::util_ThrowExceptionToJava(env, "java/io/IOException", 
                                    "can't get Available from native Stream");
    }
        
    return (jint) result;
}

JNIEXPORT void JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_NativeInputStream_nativeClose
(JNIEnv *env, jobject obj, jint nativeInputStream)
{
    nsIInputStream *stream = (nsIInputStream *) nativeInputStream;
    nsresult rv = NS_OK;
    PRUint32 result = -1;

    if (NS_SUCCEEDED(rv = stream->Close())) {
        stream->Release();
    }
    else {
        ::util_ThrowExceptionToJava(env, "java/io/IOException", 
                                    "can't get Available from native Stream");
    }
        
    return;
}
