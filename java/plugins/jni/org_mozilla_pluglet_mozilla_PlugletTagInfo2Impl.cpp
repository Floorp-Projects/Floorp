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
#include "nsIPluginTagInfo2.h"
#include "org_mozilla_pluglet_mozilla_PlugletTagInfo2Impl.h"

static jfieldID peerFID = NULL;
/*
 * Class:     org_mozilla_pluglet_mozilla_PlugletTagInfo2Impl
 * Method:    getAttributes
 * Signature: ()Ljava/util/Properties;
 */
JNIEXPORT jobject JNICALL Java_org_mozilla_pluglet_mozilla_PlugletTagInfo2Impl_getAttributes
    (JNIEnv *, jobject) {
    //nb Java_org_mozilla_pluglet_mozilla_PlugletTagInfo2Impl_getAttributes
    return NULL;
}

/*
 * Class:     org_mozilla_pluglet_mozilla_PlugletTagInfo2Impl
 * Method:    getAttribute
 * Signature: (Ljava/lang/String;)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_org_mozilla_pluglet_mozilla_PlugletTagInfo2Impl_getAttribute
    (JNIEnv *env, jobject jthis, jstring _name) {
    nsIPluginTagInfo2 * info = (nsIPluginTagInfo2*)env->GetLongField(jthis, peerFID);
    const char * name = NULL;
    if (!(name = env->GetStringUTFChars(_name,NULL))) {
	return NULL;
    }
    const char * result = NULL;
    if (NS_FAILED(info->GetParameter(name,&result))) {
	env->ReleaseStringUTFChars(_name,name);
	return NULL;
    }
    env->ReleaseStringUTFChars(_name,name);
    return env->NewStringUTF(result);
}

/*
 * Class:     org_mozilla_pluglet_mozilla_PlugletTagInfo2Impl
 * Method:    getTagType
 * Signature: ()Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_org_mozilla_pluglet_mozilla_PlugletTagInfo2Impl_getTagType
    (JNIEnv *env, jobject jthis) {
     nsIPluginTagInfo2 * info = (nsIPluginTagInfo2*)env->GetLongField(jthis, peerFID);
     const char * res = "Unknown";
     nsPluginTagType type;
     if (NS_FAILED(info->GetTagType(&type))) {
	 return env->NewStringUTF(res);
     }
     switch (type) {
     case nsPluginTagType_Embed: 
	 res = "EMBED"; 
	 break;
     case  nsPluginTagType_Object:
	 res = "OBJECT";
	 break;
     case nsPluginTagType_Applet:
	 res = "APPLET";
	 break;
     default:
	 res = "UNKNOWN";
	 break;
     }
     return env->NewStringUTF(res);
}

/*
 * Class:     org_mozilla_pluglet_mozilla_PlugletTagInfo2Impl
 * Method:    getTagText
 * Signature: ()Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_org_mozilla_pluglet_mozilla_PlugletTagInfo2Impl_getTagText
    (JNIEnv *env, jobject jthis) {
    nsIPluginTagInfo2 * info = (nsIPluginTagInfo2*)env->GetLongField(jthis, peerFID);
    const char *text = NULL;
    if(NS_FAILED(info->GetTagText(&text))) {
	return NULL;
    }
    return env->NewStringUTF(text);
}

/*
 * Class:     org_mozilla_pluglet_mozilla_PlugletTagInfo2Impl
 * Method:    getDocumentBase
 * Signature: ()Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_org_mozilla_pluglet_mozilla_PlugletTagInfo2Impl_getDocumentBase
    (JNIEnv *env, jobject jthis) {
    nsIPluginTagInfo2 * info = (nsIPluginTagInfo2*)env->GetLongField(jthis, peerFID);
    const char *base = NULL;
    if(NS_FAILED(info->GetDocumentBase(&base))) {
	return NULL;
    }
    return env->NewStringUTF(base);
}

/*
 * Class:     org_mozilla_pluglet_mozilla_PlugletTagInfo2Impl
 * Method:    getDocumentEncoding
 * Signature: ()Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_org_mozilla_pluglet_mozilla_PlugletTagInfo2Impl_getDocumentEncoding
    (JNIEnv *env, jobject jthis) {
    nsIPluginTagInfo2 * info = (nsIPluginTagInfo2*)env->GetLongField(jthis, peerFID);
    const char *encoding = NULL;
    if(NS_FAILED(info->GetDocumentEncoding(&encoding))) {
	return NULL;
    }
    return env->NewStringUTF(encoding);
}


/*
 * Class:     org_mozilla_pluglet_mozilla_PlugletTagInfo2Impl
 * Method:    getAlignment
 * Signature: ()Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_org_mozilla_pluglet_mozilla_PlugletTagInfo2Impl_getAlignment
    (JNIEnv *env, jobject jthis) {
    nsIPluginTagInfo2 * info = (nsIPluginTagInfo2*)env->GetLongField(jthis, peerFID);
    const char *aligment = NULL;
    if(NS_FAILED(info->GetAlignment(&aligment))) {
	return NULL;
    }
    return env->NewStringUTF(aligment);
}

/*
 * Class:     org_mozilla_pluglet_mozilla_PlugletTagInfo2Impl
 * Method:    getWidth
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_org_mozilla_pluglet_mozilla_PlugletTagInfo2Impl_getWidth
    (JNIEnv *env, jobject jthis) {
    nsIPluginTagInfo2 * info = (nsIPluginTagInfo2*)env->GetLongField(jthis, peerFID);
    PRUint32 res;
    if(NS_FAILED(info->GetWidth(&res))) {
	return 0;
    }
    return res;
}

/*
 * Class:     org_mozilla_pluglet_mozilla_PlugletTagInfo2Impl
 * Method:    getHeight
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_org_mozilla_pluglet_mozilla_PlugletTagInfo2Impl_getHeight
    (JNIEnv *env, jobject jthis) {
    nsIPluginTagInfo2 * info = (nsIPluginTagInfo2*)env->GetLongField(jthis, peerFID);
    PRUint32 res;
    if(NS_FAILED(info->GetHeight(&res))) {
	return 0;
    }
    return res;
}

/*
 * Class:     org_mozilla_pluglet_mozilla_PlugletTagInfo2Impl
 * Method:    getBorderVertSpace
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_org_mozilla_pluglet_mozilla_PlugletTagInfo2Impl_getBorderVertSpace
    (JNIEnv *env, jobject jthis) {
    nsIPluginTagInfo2 * info = (nsIPluginTagInfo2*)env->GetLongField(jthis, peerFID);
    PRUint32 res;
    if(NS_FAILED(info->GetBorderVertSpace(&res))) {
	return 0;
    }
    return res;
}

/*
 * Class:     org_mozilla_pluglet_mozilla_PlugletTagInfo2Impl
 * Method:    getBorderHorizSpace
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_org_mozilla_pluglet_mozilla_PlugletTagInfo2Impl_getBorderHorizSpace
    (JNIEnv *env, jobject jthis) {
    nsIPluginTagInfo2 * info = (nsIPluginTagInfo2*)env->GetLongField(jthis, peerFID);
    PRUint32 res;
    if(NS_FAILED(info->GetBorderHorizSpace(&res))) {
	return 0;
    }
    return res;
}

/*
 * Class:     org_mozilla_pluglet_mozilla_PlugletTagInfo2Impl
 * Method:    getUniqueID
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_org_mozilla_pluglet_mozilla_PlugletTagInfo2Impl_getUniqueID
    (JNIEnv *env, jobject jthis) {
    nsIPluginTagInfo2 * info = (nsIPluginTagInfo2*)env->GetLongField(jthis, peerFID);
    PRUint32 res;
    if(NS_FAILED(info->GetUniqueID(&res))) {
	return 0;
    }
    return res;
}

/*
 * Class:     org_mozilla_pluglet_mozilla_PlugletTagInfo2Impl
 * Method:    nativeFinalize
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_org_mozilla_pluglet_mozilla_PlugletTagInfo2Impl_nativeFinalize
    (JNIEnv *env, jobject jthis) {
    /* nb do as in java-dom  stuff */
    nsIPluginTagInfo2 * info = (nsIPluginTagInfo2*)env->GetLongField(jthis, peerFID);
    if(info) {
	NS_RELEASE(info);    
    }
}

/*
 * Class:     org_mozilla_pluglet_mozilla_PlugletTagInfo2Impl
 * Method:    nativeInitialize
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_org_mozilla_pluglet_mozilla_PlugletTagInfo2Impl_nativeInitialize
    (JNIEnv *env, jobject jthis) {
    if(!peerFID) {
	peerFID = env->GetFieldID(env->GetObjectClass(jthis),"peer","J");
	if (!peerFID) {
	    return;
	}
    }
    nsIPluginTagInfo2 * info = (nsIPluginTagInfo2*)env->GetLongField(jthis, peerFID);
    if (info) {
	info->AddRef();
    }
}

