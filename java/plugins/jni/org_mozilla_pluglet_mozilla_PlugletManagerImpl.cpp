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
#include "nsIPluginManager.h"
#include "org_mozilla_pluglet_mozilla_PlugletManagerImpl.h"


/*
 * Class:     org_mozilla_pluglet_mozilla_PlugletManagerImpl
 * Method:    getValue
 * Signature: (I)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_org_mozilla_pluglet_mozilla_PlugletManagerImpl_getValue
    (JNIEnv *, jobject, jint) {
    //nb
    return NULL;
}

/*
 * Class:     org_mozilla_pluglet_mozilla_PlugletManagerImpl
 * Method:    reloadPluglets
 * Signature: (Z)V
 */
JNIEXPORT void JNICALL Java_org_mozilla_pluglet_mozilla_PlugletManagerImpl_reloadPluglets
    (JNIEnv *, jobject, jboolean) {
    //nb
}

/*
 * Class:     org_mozilla_pluglet_mozilla_PlugletManagerImpl
 * Method:    userAgent
 * Signature: ()Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_org_mozilla_pluglet_mozilla_PlugletManagerImpl_userAgent
    (JNIEnv *, jobject) {
    //nb
    return NULL;
}    

/*
 * Class:     org_mozilla_pluglet_mozilla_PlugletManagerImpl
 * Method:    getURL
 * Signature: (Lorg/mozilla/pluglet/PlugletInstance;Ljava/net/URL;Ljava/lang/String;Lorg/mozilla/pluglet/PlugletStreamListener;Ljava/lang/String;Ljava/net/URL;Z)V
 */
JNIEXPORT void JNICALL Java_org_mozilla_pluglet_mozilla_PlugletManagerImpl_getURL
    (JNIEnv *, jobject, jobject, jobject, jstring, jobject, jstring, jobject, jboolean) {
    //nb
}

/*
 * Class:     org_mozilla_pluglet_mozilla_PlugletManagerImpl
 * Method:    postURL
 * Signature: (Lorg/mozilla/pluglet/PlugletInstance;Ljava/net/URL;I[BZLjava/lang/String;Lorg/mozilla/pluglet/PlugletStreamListener;Ljava/lang/String;Ljava/net/URL;ZI[B)V
 */
JNIEXPORT void JNICALL Java_org_mozilla_pluglet_mozilla_PlugletManagerImpl_postURL
    (JNIEnv *, jobject, jobject, jobject, jint, jbyteArray, jboolean, jstring, jobject, jstring, jobject, jboolean, jint, jbyteArray) {
    //nb
}

/*
 * Class:     org_mozilla_pluglet_mozilla_PlugletManagerImpl
 * Method:    nativeFinalize
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_org_mozilla_pluglet_mozilla_PlugletManagerImpl_nativeFinalize
    (JNIEnv *, jobject) {
    //nb
}

/*
 * Class:     org_mozilla_pluglet_mozilla_PlugletManagerImpl
 * Method:    nativeInitialize
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_org_mozilla_pluglet_mozilla_PlugletManagerImpl_nativeInitialize
    (JNIEnv *, jobject) {
    //nb
}

