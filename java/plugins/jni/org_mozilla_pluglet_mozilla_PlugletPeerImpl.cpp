/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- 
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

#include "nsIPluginInstancePeer.h"
#include "nsIPluginTagInfo2.h"
#include "PlugletOutputStream.h"
#include "PlugletTagInfo2.h"
#include "org_mozilla_pluglet_mozilla_PlugletPeerImpl.h"

static jfieldID peerFID = NULL;
/*
 * Class:     org_mozilla_pluglet_mozilla_PlugletPeerImpl
 * Method:    getMIMEType
 * Signature: ()Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_org_mozilla_pluglet_mozilla_PlugletPeerImpl_getMIMEType
    (JNIEnv *env, jobject jthis) {
    nsIPluginInstancePeer * instancePeer = (nsIPluginInstancePeer*)env->GetLongField(jthis, peerFID);
    if (!instancePeer) {
	return NULL;
    }
    nsMIMEType mime;
    if(NS_FAILED(instancePeer->GetMIMEType(&mime))) {
	return NULL;
    }
    return env->NewStringUTF((char *)mime);
}

/*
 * Class:     org_mozilla_pluglet_mozilla_PlugletPeerImpl
 * Method:    getMode
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_org_mozilla_pluglet_mozilla_PlugletPeerImpl_getMode
    (JNIEnv *, jobject) {
    return 1; //nb
}

/*
 * Class:     org_mozilla_pluglet_mozilla_PlugletPeerImpl
 * Method:    getValue
 * Signature: (I)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_org_mozilla_pluglet_mozilla_PlugletPeerImpl_getValue
    (JNIEnv *, jobject, jint) {
    return NULL; //nb
}

/*
 * Class:     org_mozilla_pluglet_mozilla_PlugletPeerImpl
 * Method:    newStream
 * Signature: (Ljava/lang/String;Ljava/lang/String;)Ljava/io/OutputStream;
 */
JNIEXPORT jobject JNICALL Java_org_mozilla_pluglet_mozilla_PlugletPeerImpl_newStream
    (JNIEnv *env, jobject jthis, jstring _type, jstring _target) {
    nsMIMEType mimeType = NULL;
    if ( _type == NULL
         || _target == NULL) {
        return NULL; //nb should throw NullPointerException
    }
    if(!(mimeType = (nsMIMEType)env->GetStringUTFChars(_type,NULL))) {
	return NULL;
    }
    const char * target = NULL;
    if(!(target = env->GetStringUTFChars(_target,NULL))) {
	env->ReleaseStringUTFChars(_type,(char *)mimeType);
	return NULL;
    }
    nsIPluginInstancePeer * instancePeer = NULL;
    if(!(instancePeer = (nsIPluginInstancePeer*)env->GetLongField(jthis, peerFID))) {
	env->ReleaseStringUTFChars(_type,(char *)mimeType);
	env->ReleaseStringUTFChars(_target,target);
	return NULL;
    }
    nsIOutputStream *output = NULL;
    if (NS_FAILED(instancePeer->NewStream(mimeType,target,&output))) {
    	env->ReleaseStringUTFChars(_type,(char *)mimeType);
	env->ReleaseStringUTFChars(_target,target);
	return NULL;
    }
    env->ReleaseStringUTFChars(_type,(char *)mimeType);
    env->ReleaseStringUTFChars(_target,target);
    return PlugletOutputStream::GetJObject(env,output);
}

/*
 * Class:     org_mozilla_pluglet_mozilla_PlugletPeerImpl
 * Method:    showStatus
 * Signature: (Ljava/lang/String;)V
 */
JNIEXPORT void JNICALL Java_org_mozilla_pluglet_mozilla_PlugletPeerImpl_showStatus
    (JNIEnv *env, jobject jthis, jstring _msg) {
    nsIPluginInstancePeer * instancePeer = (nsIPluginInstancePeer*)env->GetLongField(jthis, peerFID);
    if (!instancePeer
        || !_msg) {
	return;
    }
    const char *msg = NULL;
    if(!(msg = env->GetStringUTFChars(_msg,NULL))) {
	return;
    }
    instancePeer->ShowStatus(msg);
    env->ReleaseStringUTFChars(_msg,msg);
}

/*
 * Class:     org_mozilla_pluglet_mozilla_PlugletPeerImpl
 * Method:    setWindowSize
 * Signature: (II)V
 */
JNIEXPORT void JNICALL Java_org_mozilla_pluglet_mozilla_PlugletPeerImpl_setWindowSize
    (JNIEnv *env, jobject jthis, jint width, jint height) {
    nsIPluginInstancePeer * instancePeer = (nsIPluginInstancePeer*)env->GetLongField(jthis, peerFID);
    if (!instancePeer) {
	return;
    }
    instancePeer->SetWindowSize(width,height);
    //nb add exception throwing
}



static NS_DEFINE_IID(kIPluginTagInfo2,NS_IPLUGINTAGINFO2_IID);
/*
 * Class:     org_mozilla_pluglet_mozilla_PlugletPeerImpl
 * Method:    getTagInfo
 * Signature: ()Lorg/mozilla/pluglet/mozilla/PlugletTagInfo;
 */
JNIEXPORT jobject JNICALL Java_org_mozilla_pluglet_mozilla_PlugletPeerImpl_getTagInfo
    (JNIEnv *env, jobject jthis) {
     nsIPluginInstancePeer * instancePeer = (nsIPluginInstancePeer*)env->GetLongField(jthis, peerFID);
     nsIPluginTagInfo2 * info = NULL;
     if(NS_FAILED(instancePeer->QueryInterface(kIPluginTagInfo2,(void**)&info))) {
	 return NULL;
     }
     return PlugletTagInfo2::GetJObject(env,info);
}

/*
 * Class:     org_mozilla_pluglet_mozilla_PlugletPeerImpl
 * Method:    nativeFinalize
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_org_mozilla_pluglet_mozilla_PlugletPeerImpl_nativeFinalize
    (JNIEnv *env, jobject jthis) {
    nsIPluginInstancePeer * instancePeer = (nsIPluginInstancePeer*)env->GetLongField(jthis, peerFID);
    if (instancePeer) {
	/*nb do as in java-dom  stuff */
	NS_RELEASE(instancePeer);
    }
}

/*
 * Class:     org_mozilla_pluglet_mozilla_PlugletPeerImpl
 * Method:    nativeInitialize
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_org_mozilla_pluglet_mozilla_PlugletPeerImpl_nativeInitialize
    (JNIEnv *env, jobject jthis) {
    if(!peerFID) {
	peerFID = env->GetFieldID(env->GetObjectClass(jthis),"peer","J");
	if (!peerFID) {
	    return;
	}
    }
    nsIPluginInstancePeer * instancePeer = (nsIPluginInstancePeer*)env->GetLongField(jthis, peerFID);
    if (instancePeer) {
	instancePeer->AddRef();
    }
}



