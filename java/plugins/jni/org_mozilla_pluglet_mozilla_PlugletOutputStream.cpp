/* 
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Initial Developer of the Original Code is Sun Microsystems,
 * Inc. Portions created by Sun are Copyright (C) 1999 Sun Microsystems,
 * Inc. All Rights Reserved. 
 */
#include "nsIOutputStream.h"
#include "org_mozilla_pluglet_mozilla_PlugletOutputstream.h"

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
    env->GetByteArrayRegion(b,off,len,buf);
    PRUint32 tmp;
    output->Write(buf,len,&tmp);
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
	NS_RELEASE(output);
	env->SetLongField(jthis, peerFID,0);
    }
}


