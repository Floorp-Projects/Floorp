/* 
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Sun Microsystems,
 * Inc. Portions created by Sun are
 * Copyright (C) 1999 Sun Microsystems, Inc. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include <stdlib.h>
#include "nsIInputStream.h"
#include "org_mozilla_pluglet_mozilla_PlugletInputStream.h"
#include "PlugletLog.h"

static jfieldID peerFID = NULL;
/*
 * Class:     org_mozilla_pluglet_mozilla_PlugletInputStream
 * Method:    available
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_org_mozilla_pluglet_mozilla_PlugletInputStream_available
    (JNIEnv *env, jobject jthis) {
    nsIInputStream * input = (nsIInputStream*)env->GetLongField(jthis, peerFID);
    PRUint32 res = 0;
    if(input) {
	PR_LOG(PlugletLog::log, PR_LOG_DEBUG,
		("PlugletInputStream.available: stream = %p\n", input));
	input->Available(&res);
    }
    return (jint)res;
}

/*
 * Class:     org_mozilla_pluglet_mozilla_PlugletInputStream
 * Method:    close
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_org_mozilla_pluglet_mozilla_PlugletInputStream_close
    (JNIEnv *env, jobject jthis) {
    nsIInputStream * input = (nsIInputStream*)env->GetLongField(jthis, peerFID);
    if (input) {
	PR_LOG(PlugletLog::log, PR_LOG_DEBUG,
		("PlugletInputStream.close: stream = %p\n", input));
	NS_RELEASE(input);
	env->SetLongField(jthis, peerFID,0);
    }

}


/*
 * Class:     org_mozilla_pluglet_mozilla_PlugletInputStream
 * Method:    nativeRead
 * Signature: ([BII)I
 */
JNIEXPORT jint JNICALL Java_org_mozilla_pluglet_mozilla_PlugletInputStream_nativeRead
    (JNIEnv *env, jobject jthis, jbyteArray b, jint off, jint len) {
    jint retval = -1;
    nsIInputStream * input = (nsIInputStream*)env->GetLongField(jthis, peerFID);
    if (input) {
	PR_LOG(PlugletLog::log, PR_LOG_DEBUG,
		("PlugletInputStream.nativeRead: stream = %p, off = %i, len = %i\n", input, off, len));
    }

    if (env->ExceptionOccurred()) {
	return retval;
    }
    if (!input) {
        jclass ex = env->FindClass("java/io/IOException");
	if (!ex) {
  	    return retval;
	}
	env->ThrowNew(ex,"Stream was closed");
	return retval;
    }
    jbyte * bufElems = (jbyte*) malloc(sizeof(jbyte)*len);
    if (!bufElems) {
	return retval;
	//nb throw OutOfMemory
    }
    nsresult res;
    PRUint32 actualLen;
    res = input->Read((char*)bufElems,(PRUint32)len,&actualLen);
    if (NS_FAILED(res)) {
	free(bufElems);
	return retval;
    }
    retval = actualLen;
    PR_LOG(PlugletLog::log, PR_LOG_DEBUG,
	    ("PlugletInputStream.nativeRead: %i bytes read\n", retval));

    env->SetByteArrayRegion(b,off,retval,bufElems);
    free(bufElems);
    return (len > 0 && actualLen <= 0) ? -1 : retval;
}

/*
 * Class:     org_mozilla_pluglet_mozilla_PlugletInputStream
 * Method:    nativeFinalize
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_org_mozilla_pluglet_mozilla_PlugletInputStream_nativeFinalize
    (JNIEnv *env, jobject jthis) {
    /* nb do as in java-dom  stuff */
    nsIInputStream * input = (nsIInputStream*)env->GetLongField(jthis, peerFID);
    if(input) {
	PR_LOG(PlugletLog::log, PR_LOG_DEBUG,
		("PlugletInputStream.nativeFinalize: stream = %p\n", input));
	NS_RELEASE(input);    
    }
}

/*
 * Class:     org_mozilla_pluglet_mozilla_PlugletInputStream
 * Method:    nativeInitialize
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_org_mozilla_pluglet_mozilla_PlugletInputStream_nativeInitialize
    (JNIEnv *env, jobject jthis) {
    if(!peerFID) {
	peerFID = env->GetFieldID(env->GetObjectClass(jthis),"peer","J");
	if (!peerFID) {
	    return;
	}
    }
    nsIInputStream * input = (nsIInputStream*)env->GetLongField(jthis, peerFID);

    if (input) {
	PR_LOG(PlugletLog::log, PR_LOG_DEBUG,
		("PlugletInputStream.nativeInitialize: stream = %p\n", input));
	input->AddRef();
    }
}

