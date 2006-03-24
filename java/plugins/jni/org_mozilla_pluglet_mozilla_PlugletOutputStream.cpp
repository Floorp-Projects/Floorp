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
#include "nsIOutputStream.h"
#include "org_mozilla_pluglet_mozilla_PlugletOutputStream.h"
#include "PlugletLog.h"

static jfieldID peerFID = NULL;
/*
 * Class:     org_mozilla_pluglet_mozilla_PlugletOutputStream
 * Method:    flush
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_org_mozilla_pluglet_mozilla_PlugletOutputStream_flush
    (JNIEnv *env, jobject jthis) {
     nsIOutputStream * output = NULL;
     output = (nsIOutputStream*)env->GetLongField(jthis, peerFID);
     if(!output) {
	 return;
     }
     PR_LOG(PlugletLog::log, PR_LOG_DEBUG,
	    ("PlugletOutputStream.flush: stream = %p\n", output));
     output->Flush();
}

/*
 * Class:     org_mozilla_pluglet_mozilla_PlugletOutputStream
 * Method:    nativeWrite
 * Signature: ([BII)V
 */
JNIEXPORT void JNICALL Java_org_mozilla_pluglet_mozilla_PlugletOutputStream_nativeWrite
    (JNIEnv *env, jobject jthis, jbyteArray b, jint off, jint len) {
    jbyte *buf = NULL;
    if(!(buf = (jbyte*)malloc(sizeof(jbyte)*len))) {
	return; //nb throw OutOfMemoryException
    }
    nsIOutputStream * output = NULL;
    output = (nsIOutputStream*)env->GetLongField(jthis, peerFID);
    if(!output) {
	return;
    }
    PR_LOG(PlugletLog::log, PR_LOG_DEBUG,
	    ("PlugletOutputStream.nativeWrite: stream = %p, off = %i, len = %i\n", output, off, len));
    env->GetByteArrayRegion(b,off,len,buf);
    PRUint32 tmp;
    output->Write((const char *)buf,len,&tmp);
    PR_LOG(PlugletLog::log, PR_LOG_DEBUG,
	    ("PlugletOutputStream.nativeWrite: %i bytes written\n", tmp));
    free(buf);
}

/*
 * Class:     org_mozilla_pluglet_mozilla_PlugletOutputStream
 * Method:    nativeFinalize
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_org_mozilla_pluglet_mozilla_PlugletOutputStream_nativeFinalize
    (JNIEnv *env, jobject jthis) {
    /* nb do as in java-dom  stuff */
    nsIOutputStream * output = (nsIOutputStream*)env->GetLongField(jthis, peerFID);
    if(output) {
	PR_LOG(PlugletLog::log, PR_LOG_DEBUG,
		("PlugletOutputStream.nativeFinalize: stream = %p\n", output));
	NS_RELEASE(output);    
    }
}

/*
 * Class:     org_mozilla_pluglet_mozilla_PlugletOutputStream
 * Method:    nativeInitialize
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_org_mozilla_pluglet_mozilla_PlugletOutputStream_nativeInitialize
    (JNIEnv *env, jobject jthis) {
    if(!peerFID) {
	peerFID = env->GetFieldID(env->GetObjectClass(jthis),"peer","J");
	if (!peerFID) {
	    return;
	}
    }
    nsIOutputStream * output = (nsIOutputStream*)env->GetLongField(jthis, peerFID);
    if (output) {
	PR_LOG(PlugletLog::log, PR_LOG_DEBUG,
		("PlugletOutputStream.nativeInitialize: stream = %p\n", output));
	output->AddRef();
    }
}

/*
 * Class:     org_mozilla_pluglet_mozilla_PlugletOutputStream
 * Method:    close
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_org_mozilla_pluglet_mozilla_PlugletOutputStream_close
    (JNIEnv *env, jobject jthis) {
    nsIOutputStream * output = (nsIOutputStream*)env->GetLongField(jthis, peerFID);
    if (output) {
	PR_LOG(PlugletLog::log, PR_LOG_DEBUG,
		("PlugletOutputStream.close: stream = %p\n", output));
	NS_RELEASE(output);
	env->SetLongField(jthis, peerFID,0);
    }
}


