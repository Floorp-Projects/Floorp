/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- 
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
#include "nsIPluginTagInfo2.h"
#include "org_mozilla_pluglet_mozilla_PlugletTagInfo2Impl.h"
#include "PlugletLog.h"

static jfieldID peerFID = NULL;
static jclass DOMAccessor = NULL;
static jmethodID getNodeByHandle = NULL;

/*
 * Class:     org_mozilla_pluglet_mozilla_PlugletTagInfo2Impl
 * Method:    getAttributesArray
 * Signature: ()[[Ljava/lang/String;
 */
JNIEXPORT jobjectArray JNICALL Java_org_mozilla_pluglet_mozilla_PlugletTagInfo2Impl_getAttributesArray
    (JNIEnv *env, jobject jthis) {
    jobjectArray jnames;
    jobjectArray jvalues;
    jobjectArray result;
    const char*const* names;
    const char*const* values;
    PRUint16 nAttr;

    PR_LOG(PlugletLog::log, PR_LOG_DEBUG,
	    ("PlugletTagInfo2Impl.getAttributesArray\n"));

    nsIPluginTagInfo2 * info = (nsIPluginTagInfo2*)env->GetLongField(jthis, peerFID);
    if (!info) {
	return NULL;
    }
    if (NS_FAILED(info->GetAttributes(nAttr, names, values))) {
	return NULL;
    }
    jclass strClass = env->FindClass("java/lang/String");
    jclass strArrayClass = env->FindClass("[Ljava/lang/String;");
    if (!strClass || !strArrayClass) {
	return NULL;
    }
    result = env->NewObjectArray(2, strArrayClass, NULL); 
    jnames = env->NewObjectArray(nAttr, strClass, NULL);
    jvalues = env->NewObjectArray(nAttr, strClass, NULL);

    int i;
    for (i=0; i < nAttr; i++) {
	env->SetObjectArrayElement(jnames, i, env->NewStringUTF(names[i]));
	env->SetObjectArrayElement(jvalues, i, env->NewStringUTF(values[i]));
    }
    env->SetObjectArrayElement(result, 0, jnames);
    env->SetObjectArrayElement(result, 1, jvalues);

    env->DeleteLocalRef(jnames);
    env->DeleteLocalRef(jvalues);
    return result;
}

/*
 * Class:     org_mozilla_pluglet_mozilla_PlugletTagInfo2Impl
 * Method:    getAttribute
 * Signature: (Ljava/lang/String;)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_org_mozilla_pluglet_mozilla_PlugletTagInfo2Impl_getAttribute
    (JNIEnv *env, jobject jthis, jstring _name) {
    PR_LOG(PlugletLog::log, PR_LOG_DEBUG,
	    ("PlugletTagInfo2Impl.getAttribute: name = %s\n", _name));
    if (!_name) {
	return NULL;
    }
    nsIPluginTagInfo2 * info = (nsIPluginTagInfo2*)env->GetLongField(jthis, peerFID);
    const char * name = NULL;
    if (!(name = env->GetStringUTFChars(_name,NULL))) {
	return NULL;
    }
    const char * result = NULL;
    if (NS_FAILED(info->GetAttribute(name,&result))) {
	env->ReleaseStringUTFChars(_name,name);
	return NULL;
    }
    env->ReleaseStringUTFChars(_name,name);
    return env->NewStringUTF(result);
}


/*
 * Class:     org_mozilla_pluglet_mozilla_PlugletTagInfo2Impl
 * Method:    getDOMElement
 * Signature: ()Ljava/lang/Object;
 */
JNIEXPORT jobject JNICALL Java_org_mozilla_pluglet_mozilla_PlugletTagInfo2Impl_getDOMElement
(JNIEnv *env, jobject jthis) {
    PR_LOG(PlugletLog::log, PR_LOG_DEBUG,
           ("PlugletTagInfo2Impl.getDOMElement\n"));
    if ( !DOMAccessor ) {
        DOMAccessor = env->FindClass("org/mozilla/dom/DOMAccessor");
        if (!DOMAccessor) {
            return NULL;
        }
        DOMAccessor = (jclass) env->NewGlobalRef(DOMAccessor); // nb who is going to Delete this ref
        if (!DOMAccessor) {
            return NULL;
        }
        getNodeByHandle = env->GetStaticMethodID(DOMAccessor,"getNodeByHandle","(J)Lorg/w3c/dom/Node;");
        if (!getNodeByHandle) {
            DOMAccessor = NULL;
            return NULL;
        }
    }
    nsIPluginTagInfo2 * info = (nsIPluginTagInfo2*)env->GetLongField(jthis, peerFID);
    nsIDOMElement * elem = NULL; 
    if (!info 
        || NS_FAILED(info->GetDOMElement(&elem))) {
        return NULL;
    }
    return env->CallStaticObjectMethod(DOMAccessor,getNodeByHandle,(jlong) elem);
}


/*
 * Class:     org_mozilla_pluglet_mozilla_PlugletTagInfo2Impl
 * Method:    getParametersArray
 * Signature: ()[[Ljava/lang/String;
 */
JNIEXPORT jobjectArray JNICALL Java_org_mozilla_pluglet_mozilla_PlugletTagInfo2Impl_getParametersArray
    (JNIEnv *env, jobject jthis) {
    jobjectArray jnames;
    jobjectArray jvalues;
    jobjectArray result;
    const char * const * names;
    const char * const * values;
    PRUint16 nParam;

    PR_LOG(PlugletLog::log, PR_LOG_DEBUG,
	    ("PlugletTagInfo2Impl.getParameters\n"));

    nsIPluginTagInfo2 * info = (nsIPluginTagInfo2*)env->GetLongField(jthis, peerFID);
    if (!info) {
	return NULL;
    }
    if (NS_FAILED(info->GetParameters(nParam, names, values))) {
	return NULL;
    }
    jclass strClass = env->FindClass("java/lang/String");
    jclass strArrayClass = env->FindClass("[Ljava/lang/String;");
    if (!strClass || !strArrayClass) {
	return NULL;
    }
    result = env->NewObjectArray(2, strArrayClass, NULL); 
    jnames = env->NewObjectArray(nParam, strClass, NULL);
    jvalues = env->NewObjectArray(nParam, strClass, NULL);

    int i;
    for (i=0; i < nParam; i++) {
	env->SetObjectArrayElement(jnames, i, env->NewStringUTF(names[i]));
	env->SetObjectArrayElement(jvalues, i, env->NewStringUTF(values[i]));
    }
    env->SetObjectArrayElement(result, 0, jnames);
    env->SetObjectArrayElement(result, 1, jvalues);

    env->DeleteLocalRef(jnames);
    env->DeleteLocalRef(jvalues);
    return result;
}

/*
 * Class:     org_mozilla_pluglet_mozilla_PlugletTagInfo2Impl
 * Method:    getParameter
 * Signature: (Ljava/lang/String;)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_org_mozilla_pluglet_mozilla_PlugletTagInfo2Impl_getParameter
    (JNIEnv *env, jobject jthis, jstring _name) {
    PR_LOG(PlugletLog::log, PR_LOG_DEBUG,
	    ("PlugletTagInfo2Impl.getParameter: name = %s\n", _name));
    if (!_name) {
	return NULL;
    }
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
    PR_LOG(PlugletLog::log, PR_LOG_DEBUG,
	    ("PlugletTagInfo2Impl.getAttribute: value = %s\n", result));
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
     PR_LOG(PlugletLog::log, PR_LOG_DEBUG,
	    ("PlugletTagInfo2Impl.getTagType: tag is %s\n", res));
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
    PR_LOG(PlugletLog::log, PR_LOG_DEBUG,
	    ("PlugletTagInfo2Impl.getTagText: text = %s\n", text));
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
    PR_LOG(PlugletLog::log, PR_LOG_DEBUG,
	    ("PlugletTagInfo2Impl.getDocumentBase: base = %s\n", base));
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
    PR_LOG(PlugletLog::log, PR_LOG_DEBUG,
	    ("PlugletTagInfo2Impl.getDocumentEncoding: encoding = %s\n", encoding));
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
    PR_LOG(PlugletLog::log, PR_LOG_DEBUG,
	    ("PlugletTagInfo2Impl.getAlignment: align = %s\n", aligment));
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
    PR_LOG(PlugletLog::log, PR_LOG_DEBUG,
	    ("PlugletTagInfo2Impl.getWidth: width = %i\n", res));
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
    PR_LOG(PlugletLog::log, PR_LOG_DEBUG,
	    ("PlugletTagInfo2Impl.getHeight: width = %i\n", res));
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
    PR_LOG(PlugletLog::log, PR_LOG_DEBUG,
	    ("PlugletTagInfo2Impl.getBorderVertSpace: res = %i\n", res));
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
    PR_LOG(PlugletLog::log, PR_LOG_DEBUG,
	    ("PlugletTagInfo2Impl.getBorderHorizSpace: res = %i\n", res));
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
    PR_LOG(PlugletLog::log, PR_LOG_DEBUG,
	    ("PlugletTagInfo2Impl.getUniqueID: id = %i\n", res));
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
	PR_LOG(PlugletLog::log, PR_LOG_DEBUG,
		("PlugletTagInfo2Impl.nativeFinalize: info = %p\n", info));
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
	PR_LOG(PlugletLog::log, PR_LOG_DEBUG,
		("PlugletTagInfo2Impl.nativeInitialize: info = %p\n", info));
	info->AddRef();
    }
}

