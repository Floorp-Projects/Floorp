/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is The Waterfall Java Plugin Module
 *  
 * The Initial Developer of the Original Code is Sun Microsystems Inc
 * Portions created by Sun Microsystems Inc are Copyright (C) 2001
 * All Rights Reserved.
 * 
 * $Id: wfm6_native.c,v 1.2 2001/07/12 19:58:20 edburns%acm.org Exp $
 * 
 * Contributor(s):
 * 
 *     Nikolay N. Igotti <nikolay.igotti@Sun.Com>
 */

#include <stdlib.h>
#include <string.h>
#include <jvmp.h>
#include "sun_jvmp_mozilla_MozillaAppletPeer.h"
#include "sun_jvmp_mozilla_MozillaPeerFactory.h"
#include "sun_jvmp_mozilla_JSObject.h"
#include "sun_jvmp_mozilla_JavaScriptProtectionDomain.h"
#include "wf_moz6_common.h"

/* native methods implementation */

static jfieldID peerFID = NULL;

static jclass      g_jclass_Object = NULL;
static jclass      g_jclass_Class = NULL;
static jclass      g_jclass_Boolean = NULL;
static jclass      g_jclass_Byte = NULL;
static jclass      g_jclass_Character = NULL;
static jclass      g_jclass_Short = NULL;
static jclass      g_jclass_Integer = NULL;
static jclass      g_jclass_Long = NULL;
static jclass      g_jclass_Float = NULL;
static jclass      g_jclass_Double = NULL;
static jclass      g_jclass_Void = NULL;
static jmethodID   g_jmethod_Boolean_booleanValue = NULL;
static jmethodID   g_jmethod_Byte_byteValue = NULL;
static jmethodID   g_jmethod_Character_charValue = NULL;
static jmethodID   g_jmethod_Short_shortValue = NULL;
static jmethodID   g_jmethod_Integer_intValue = NULL;
static jmethodID   g_jmethod_Long_longValue = NULL;
static jmethodID   g_jmethod_Float_floatValue = NULL;
static jmethodID   g_jmethod_Double_doubleValue = NULL;
static jmethodID   g_jmethod_Boolean_init = NULL;
static jmethodID   g_jmethod_Byte_init = NULL;
static jmethodID   g_jmethod_Character_init = NULL;
static jmethodID   g_jmethod_Short_init = NULL;
static jmethodID   g_jmethod_Integer_init = NULL;
static jmethodID   g_jmethod_Long_init = NULL;
static jmethodID   g_jmethod_Float_init = NULL;
static jmethodID   g_jmethod_Double_init = NULL;
static jclass      g_jclass_SecureInvocation = NULL;
static jclass      g_jclass_JSObject = NULL;
static jmethodID   g_jmethod_SecureInvocation_ConstructObject = NULL;
static jmethodID   g_jmethod_SecureInvocation_CallMethod = NULL;
static jmethodID   g_jmethod_SecureInvocation_GetField = NULL;
static jmethodID   g_jmethod_SecureInvocation_SetField = NULL;

static int         g_inited = 0;

static int DoInit(JNIEnv* env);

static int CreateObject(JNIEnv*                   env,
			jobject                   constructor,
			jvalue*                   arg,
			SecurityContextWrapper*   security,
			jobject                   *result);

static int CallMethod(JNIEnv*                     env,
		      enum jvmp_jni_type          type,
		      jobject                     obj, 
		      jobject                     method, 
		      jvalue*                     arg,
		      SecurityContextWrapper*     security,
		      jvalue* result);		      

static int SetField(JNIEnv*                     env,
		    enum jvmp_jni_type          type,
                    jobject                     obj, 
                    jobject                     field, 
                    jvalue                      val, 
		    SecurityContextWrapper*     security);

static int GetField(JNIEnv*                     env,
		    enum jvmp_jni_type          type,
                    jobject                     obj, 
                    jobject                     field, 
		    SecurityContextWrapper*     security,
		    jvalue*                     result);


/* it never changes */
static BrowserSupportWrapper *browser_wrapper=NULL;
 
static void* getWrapper(JNIEnv* env, jobject jobj, jfieldID* fid) 
{
    void* wrapper;

    if (*fid == NULL) {
	*fid = (*env)->GetFieldID(env,
				  (*env)->GetObjectClass(env, jobj),
				  "m_params","J");
	if (*fid == NULL) return NULL; }
    wrapper = JLONG_TO_PTR((*env)->GetLongField(env, jobj, *fid));
    return wrapper;
}
/*
static void* getStaticWrapper(JNIEnv* env, jclass jcls, jfieldID* fid) 
{
    void* wrapper;

    if (*fid == NULL) {
	*fid = (*env)->GetStaticFieldID(env, jcls,
					"m_evaluator","J");
	if (*fid == NULL) return NULL; }
    wrapper =  JLONG_TO_PTR((*env)->GetStaticLongField(env, jcls, *fid));
    return wrapper;
    }
*/

static BrowserSupportWrapper* getBrowserWrapper(JNIEnv* env,
						jobject jobj,
						jclass  jcls) 
{
    jfieldID fid   = NULL;
    
    if (browser_wrapper) return browser_wrapper;
    if (jobj == NULL)
	{
	    fid = (*env)->GetStaticFieldID(env, jcls, "m_evaluator","J");
	    if (fid == NULL)  return NULL;
	    browser_wrapper =  (BrowserSupportWrapper*)
		JLONG_TO_PTR(((*env)->GetStaticLongField(env, jcls, fid)));
	} 
    else
	{
	    fid = (*env)->GetFieldID(env, (*env)->GetObjectClass(env, jobj), 
				     "m_params", "J");
	    if (fid == NULL) return NULL;
	    browser_wrapper =  (BrowserSupportWrapper*)
		JLONG_TO_PTR((*env)->GetLongField(env, jobj, fid));
	}    
    return browser_wrapper;
}

/*
 * Class:     sun_jvmp_mozilla_MozillaAppletPeer
 * Method:    finalizeParams
 * Signature: ()V
 */
JNIEXPORT void JNICALL 
Java_sun_jvmp_mozilla_MozillaAppletPeer_finalizeParams
(JNIEnv *env, jobject jobj)
{
    JavaObjectWrapper *wrapper;
    
    if (!(wrapper = (JavaObjectWrapper*)getWrapper(env, jobj, &peerFID))) 
	return;
    wrapper->dead = 1;
}

/* kind of obfuscated call, as we can pass m_params directly,
   but this is good start point for optimization :) */
JNIEXPORT jobjectArray JNICALL 
Java_sun_jvmp_mozilla_MozillaAppletPeer_getParams(JNIEnv* env, jobject jobj)
{
  char **keys, **vals;
  unsigned int i;
  int len;
  static jclass StringClass=NULL, StringArrayClass=NULL;
  jobjectArray jkeys, jvals, result;
  JavaObjectWrapper *wrapper;

  if (!(wrapper = (JavaObjectWrapper*)getWrapper(env, jobj, &peerFID))) 
      return NULL;
  wrapper->GetParameters(wrapper->info, &keys, &vals, &len);
  /* hopefully here runtime is inited so those classes resolved 
     without problems - maybe cache it */
  if (!StringClass)
    StringClass = (*env)->NewGlobalRef(env, 
				       (*env)->FindClass(env, "java/lang/String"));
  if (!StringArrayClass)
    StringArrayClass = (*env)->NewGlobalRef(env,
					    (*env)->FindClass(env, "[Ljava/lang/String;"));
  result = (*env)->NewObjectArray(env, 2, StringArrayClass, NULL); 
  jkeys = (*env)->NewObjectArray(env, len, StringClass, NULL);
  jvals = (*env)->NewObjectArray(env, len, StringClass, NULL);
  for (i=0; i<len; i++) 
      {
	  (*env)->SetObjectArrayElement(env, jkeys, i, 
					(*env)->NewStringUTF(env, keys[i]));
	  (*env)->SetObjectArrayElement(env, jvals, i, 
					(*env)->NewStringUTF(env, vals[i]));
      }
  (*env)->SetObjectArrayElement(env, result, 0, jkeys);
  (*env)->SetObjectArrayElement(env, result, 1, jvals);
  (*env)->DeleteLocalRef(env, jkeys);
  (*env)->DeleteLocalRef(env, jvals);
  return result;
}

/*
 * Class:     sun_jvmp_mozilla_MozillaAppletPeer
 * Method:    nativeShowStatus
 * Signature: (Ljava/lang/String;)Z
 */
JNIEXPORT jboolean JNICALL 
Java_sun_jvmp_mozilla_MozillaAppletPeer_nativeShowStatus
  (JNIEnv * env, jobject jobj, jstring jstatus)
{
    jint res;
    char  *status;
    JavaObjectWrapper *wrapper;

    if (!(wrapper = (JavaObjectWrapper*)getWrapper(env, jobj, &peerFID))) 
      return JNI_FALSE; 
    status = (char*)(*env)->GetStringUTFChars(env, jstatus, NULL);       
    res = wrapper->ShowStatus(wrapper->info, status); 
    (*env)->ReleaseStringUTFChars(env, jstatus, status);
    return (res == 0);
}

/*
 * Class:     sun_jvmp_mozilla_MozillaAppletPeer
 * Method:    nativeShowDocument
 * Signature: (Ljava/lang/String;Ljava/lang/String;)Z
 */
JNIEXPORT jboolean JNICALL 
Java_sun_jvmp_mozilla_MozillaAppletPeer_nativeShowDocument
  (JNIEnv *env, jobject jobj, jstring jurl, jstring jtarget)
{
    int result;
    char  *url, *target;
    JavaObjectWrapper *wrapper;
    
    if (!(wrapper = (JavaObjectWrapper*)getWrapper(env, jobj, &peerFID)))
      return JNI_FALSE; 
    url = (char*)(*env)->GetStringUTFChars(env, jurl, NULL);
    target = (char*)(*env)->GetStringUTFChars(env, jtarget, NULL);    
    result = wrapper->ShowDocument(wrapper->info, url, target); 
    (*env)->ReleaseStringUTFChars(env, jurl, url);
    (*env)->ReleaseStringUTFChars(env, jtarget, target);
    return (result == 0);
}

/*
 * Class:     sun_jvmp_mozilla_MozillaAppletPeer
 * Method:    nativeReturnJObject
 * Signature: (Ljava/lang/Object;J)V
 */
JNIEXPORT void JNICALL 
Java_sun_jvmp_mozilla_MozillaAppletPeer_nativeReturnJObject
  (JNIEnv *env, jobject jobj, jobject o, jlong ptr)
{
  jobject* p;

  p = (jobject*)JLONG_TO_PTR(ptr);
  if (!p) return;
  if (o)
      *p = (*env)->NewGlobalRef(env, o);
  else
      *p = NULL;
}

/*
 * Class:     sun_jvmp_mozilla_MozillaPeerFactory
 * Method:    nativeGetProxyInfoForURL
 * Signature: (Ljava/lang/String;)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL 
Java_sun_jvmp_mozilla_MozillaPeerFactory_nativeGetProxyInfoForURL
  (JNIEnv *env, jobject jobj, jstring jurl)
{
    jstring jresult;
    jint res;
    char  *url, *result;

    if (!getBrowserWrapper(env, jobj, NULL)) return NULL; 
    url = (char*)(*env)->GetStringUTFChars(env, jurl, NULL);       
    res = browser_wrapper->GetProxyForURL(browser_wrapper->info, url, &result);
    (*env)->ReleaseStringUTFChars(env, jurl, url);
    if (res != 0) return NULL;
    jresult = (*env)->NewStringUTF(env, result);
    free(result);
    return jresult;    
}

/*
 * Class:     sun_jvmp_mozilla_MozillaPeerFactory
 * Method:    nativeHandleCall
 * Signature: (IJ)I
 */
JNIEXPORT jint JNICALL 
Java_sun_jvmp_mozilla_MozillaPeerFactory_nativeHandleCall
(JNIEnv * env, jobject jobj, jint arg1, jlong arg2)
{
  struct Java_CallInfo*  call   = NULL;
  static jthrowable jException  = NULL;
  call = (struct Java_CallInfo*) JLONG_TO_PTR(arg2);
  if (!call) return 0;
  if (!DoInit(env)) goto done; 
  call->jException = NULL;
  switch (call->type)
    {
    case Java_WrapJSObject:
      {
	/* maybe it has sense to lookup if we already have wrapper for this 
	   JS object? */
	static jmethodID  m              = NULL;
	jobject           jObj           = NULL;
	fprintf(stderr, "Java_WrapJSObject: %x %x\n", 
		call->data.__wrapJSObject.jstid,
		call->data.__wrapJSObject.jsObject);	
	if (!m)
	  {
	    m =  (*env)->GetMethodID(env, g_jclass_JSObject, "<init>", "(II)V");
	    if (!m) break;
	  }
        jObj = (*env)->NewObject(env, g_jclass_JSObject, m, 
				 call->data.__wrapJSObject.jstid,
				 call->data.__wrapJSObject.jsObject);
	if (!jObj) break;	
	call->data.__wrapJSObject.jObject = (*env)->NewGlobalRef(env, jObj);
	break;
      }
    case Java_UnwrapJObject:
      {
	static jfieldID  f              = NULL;
	fprintf(stderr, "Java_UnwrapJObject: %x %x\n", 
		(int)call->data.__unwrapJObject.jstid,
		(int)call->data.__unwrapJObject.jObject);
	if (!(*env)->IsInstanceOf(env, 
				  call->data.__unwrapJObject.jObject,
				  g_jclass_JSObject)) 
	  {
	    fprintf(stderr, "Java object is not JS wrapper!!!\n");
	    call->data.__unwrapJObject.jsObject = 0;
	    break;
	  }
	if (!f) 
	    {
		f = (*env)->GetFieldID(env, g_jclass_JSObject, 
				       "nativeJSObject", "I");
		if (!f) break;
	    }
	call->data.__unwrapJObject.jsObject 
	    = (*env)->GetIntField(env, call->data.__unwrapJObject.jObject, f);
	break;
      }
    case Java_CreateObject:
      {
	jobject constr, result;
	constr = (*env)->ToReflectedMethod(env, 
					   call->data.__createObject.clazz, 
					   call->data.__createObject.methodID,
					   JNI_FALSE);
	constr = (*env)->NewLocalRef(env, constr);	
	if (CreateObject(env, 
			 constr,
			 call->data.__createObject.args,
			 call->security,
			 &result))
	    call->data.__createObject.result = (*env)->NewGlobalRef(env, result);    
	(*env)->DeleteLocalRef(env, constr);	
	break;
      }
    case Java_CallMethod:	
     {
	 jclass  clazz;
	 jobject method;
	 jvalue  result;
	 clazz  =  (*env)->NewGlobalRef(env, (*env)->GetObjectClass(env, call->data.__callMethod.obj));
	 method =  (*env)->NewGlobalRef(env, (*env)->ToReflectedMethod(env, 
								       clazz, 
								       call->data.__callMethod.methodID,
								       JNI_FALSE));
	if (CallMethod(env,
		       call->data.__callMethod.type, 
		       call->data.__callMethod.obj, 
		       method, 
		       call->data.__callMethod.args, 
		       call->security, 
		       &result))
	    call->data.__callMethod.result =  result;
	(*env)->DeleteGlobalRef(env, clazz);
	(*env)->DeleteGlobalRef(env, method);
	break;
	}
    case Java_CallNonvirtualMethod:	
     {
	 jclass  clazz;
	 jobject method;
	 jvalue  result;
	 if (!((*env)->IsInstanceOf(env,
				   call->data.__callNonvirtualMethod.obj,
				   call->data.__callNonvirtualMethod.clazz)))
	   break;
	 clazz  =  (*env)->NewGlobalRef(env,
					call->data.__callNonvirtualMethod.clazz);
	 method =  (*env)->NewGlobalRef(env, 
					(*env)->ToReflectedMethod(env, 
								  clazz, 
								  call->data.__callNonvirtualMethod.methodID,
								  JNI_FALSE));
	if (CallMethod(env,
		       call->data.__callNonvirtualMethod.type, 
		       call->data.__callNonvirtualMethod.obj, 
		       method, 
		       call->data.__callNonvirtualMethod.args, 
		       call->security, 
		       &result))
	    call->data.__callNonvirtualMethod.result =  result;
	(*env)->DeleteGlobalRef(env, clazz);
	(*env)->DeleteGlobalRef(env, method);
	break;
     }
    case Java_CallStaticMethod:	
	{
	    jobject method;
	    jvalue  result;
	    method =  (*env)->NewGlobalRef(env, (*env)->ToReflectedMethod(env, 
									  call->data.__callStaticMethod.clazz, 
									  call->data.__callStaticMethod.methodID,
									  JNI_TRUE));
	    if (CallMethod(env,
			   call->data.__callMethod.type, 
			   NULL, 
			   method, 
			   call->data.__callMethod.args, 
			   call->security, 
			   &result))
		call->data.__callStaticMethod.result =  result;
	    (*env)->DeleteGlobalRef(env, method);
	    break;
	}
    case Java_SetField:
      {
	jclass  clazz;
	jobject field, jobj;
	clazz = (*env)->GetObjectClass(env, call->data.__setField.obj);
	field = (*env)->NewGlobalRef(env, (*env)->ToReflectedField(env,
								   clazz,
								   call->data.__setField.fieldID,
								   JNI_FALSE));
	jobj = (*env)->NewGlobalRef(env, call->data.__setField.obj);
	SetField(env, 
		 call->data.__setField.type, 
		 jobj, 
		 field,
		 call->data.__setField.value,
		 call->security);
	(*env)->DeleteGlobalRef(env, clazz);
	(*env)->DeleteGlobalRef(env, field);
	(*env)->DeleteGlobalRef(env, jobj);
	break;
      }
    case Java_SetStaticField:
      {
	jobject field;

	field = (*env)->NewGlobalRef(env, (*env)->ToReflectedField(env,
								   call->data.__setStaticField.clazz,
								   call->data.__setStaticField.fieldID,
								   JNI_TRUE));
	SetField(env, 
		 call->data.__setField.type, 
		 NULL, 
		 field,
		 call->data.__setStaticField.value,
		 call->security);
	(*env)->DeleteGlobalRef(env, field);
	break;
      }
    case Java_GetField:
      {
	jclass  clazz;
	jobject field, jobj;
	jvalue  result;
	clazz = (*env)->GetObjectClass(env, call->data.__getField.obj);
	field = (*env)->NewGlobalRef(env, (*env)->ToReflectedField(env,
								   clazz,
								   call->data.__getField.fieldID,
								   JNI_FALSE));
	jobj = (*env)->NewGlobalRef(env, call->data.__getField.obj);
	if (GetField(env,
		     call->data.__getField.type,
		     jobj,
		     field,
		     call->security,
		     &result)) 
	  call->data.__getField.result = result;
	(*env)->DeleteGlobalRef(env, field);
	(*env)->DeleteGlobalRef(env, jobj);
	break;
      }
    case Java_GetStaticField:
      {
	jobject field;
	jvalue  result;
	field = (*env)->NewGlobalRef(env, (*env)->ToReflectedField(env,
								   call->data.__getStaticField.clazz,
								   call->data.__getStaticField.fieldID,
								   JNI_TRUE));
	if (GetField(env,
		     call->data.__getStaticField.type,
		     NULL,
		     field,
		     call->security,
		     &result)) 
	  call->data.__getStaticField.result = result;
	(*env)->DeleteGlobalRef(env, field);
	break;
      }
    default:
      fprintf(stderr, "Unknown call type in nativeHandleCall\n");
      break;
    }
 done:
  jException = (*env)->ExceptionOccurred(env);
  if (jException)
      {
       	  fprintf(stderr, "wfm6_native.c: hmm, exception in handle call %d\n", call->type);
	  /* XXX: this behavior is kinda incorrect, but otherwise
	   * Mozilla with no LiveConnect just crashes on pages like
	   * http://javaapplets.com, when call goes in JS->Java direction.
	   * I have no complete understanding of reason of this crash, maybe 
	   * incorrect Mozilla code - I'm not sure.
	   */
#if 0	  
	  jException = (*env)->NewLocalRef(env, jException);
	  call->jException = jException;
#else
	  (*env)->ExceptionDescribe(env);
#endif
	  (*env)->ExceptionClear(env);
      }  
  return 1;
}

static void 
Initialize_JSObject_CallInfo(JNIEnv*            env, 
			     struct
			     JSObject_CallInfo* pInfo, 
			     const char*        pszUrl, 
			     jlong              wrapper,
			     jobjectArray       jCertArray, 
			     jintArray          jCertLengthArray, 
			     int                iNumOfCerts,
			     jobject            jAccessControlContext);
static void 
Clear_JSObject_CallInfo(JNIEnv* env, 
			struct JSObject_CallInfo* pInfo);

JNIEXPORT jint JNICALL 
Java_sun_jvmp_mozilla_JSObject_JSGetThreadID
(JNIEnv *env, jclass jcls, jlong param)
{
    jint tid;
    JavaObjectWrapper* wrapper = (JavaObjectWrapper*) JLONG_TO_PTR(param);
    wrapper->GetJSThread(wrapper->info, &tid);
    return tid;
}

JNIEXPORT jint JNICALL 
Java_sun_jvmp_mozilla_JSObject_JSGetNativeJSObject
(JNIEnv *env, 
 jclass jsClass, 
 jlong  param, 
 jstring url, 
 jobjectArray jCertArray, 
 jintArray jCertLengthArray, 
 jint iNumOfCerts, 
 jobject jAccessControlContext)
{
    jint   ret = 0;
    const  char* pszUrl = NULL;
    struct JSObject_CallInfo* pInfo = NULL;
    jint   jsThreadID = 0;
    JavaObjectWrapper* p = (JavaObjectWrapper*)JLONG_TO_PTR(param);

    if (getBrowserWrapper(env, NULL, jsClass) == NULL) 
	return 0;
    /*    "Java_sun_jvmp_mozilla_JSObject_JSGetNativeJSObject: url=%p param=%d %p\n", url, (int)param, browser_wrapper); */
    if (url)
      pszUrl = (*env)->GetStringUTFChars(env, url, (jboolean*)0);
    pInfo = (struct JSObject_CallInfo*)
	malloc(sizeof(struct JSObject_CallInfo));
    memset(pInfo, 0, sizeof(struct JSObject_CallInfo));
    Initialize_JSObject_CallInfo(env, pInfo, pszUrl,
				 param,
				 jCertArray, 
				 jCertLengthArray, 
				 iNumOfCerts, 
				 jAccessControlContext);

    pInfo->data.__getWindow.pPlugin = (void*)(p->plugin);
    pInfo->data.__getWindow.iNativeJSObject = 0;
    pInfo->type = JS_GetWindow;
    jsThreadID = 
	Java_sun_jvmp_mozilla_JSObject_JSGetThreadID(env, jsClass, param);

    browser_wrapper->JSCall(browser_wrapper->info, jsThreadID, &pInfo);

    if (pInfo->jException != NULL)
	{
	    jthrowable jException 
		= (jthrowable)((*env)->NewLocalRef(env, pInfo->jException));
	    (*env)->Throw(env, jException);
	    ret = 0;
	} 
    else 
        {
	    ret = pInfo->data.__getWindow.iNativeJSObject;
	}
    if (pszUrl)
	(*env)->ReleaseStringUTFChars(env, url, pszUrl);
    Clear_JSObject_CallInfo(env, pInfo);
    free(pInfo);    
    return ret;
}


JNIEXPORT jobject JNICALL 
Java_sun_jvmp_mozilla_JSObject_JSObjectEval
(JNIEnv*         env, 
 jclass          jsClass, 
 jint            jsThreadID, 
 jint            nativeJSObject, 
 jlong           wrapper,
 jstring         url, 
 jobjectArray    jCertArray, 
 jintArray       jCertLengthArray, 
 jint            iNumOfCerts, 
 jstring         str, 
 jobject         jAccessControlContext)
{
    jobject ret = NULL;       
    struct JSObject_CallInfo* pInfo = NULL;
    const jchar* pszStr = NULL;
    const char*  pszUrl = NULL;
    jsize len = 0;

    if (getBrowserWrapper(env, NULL, jsClass) == NULL) 
	return NULL;
    if (url)
      pszUrl = (*env)->GetStringUTFChars(env, url, (jboolean*)0);
    pszStr = (*env)->GetStringChars(env, str, (jboolean*)0);
    len    = (*env)->GetStringLength(env, str);
    pInfo = (struct JSObject_CallInfo*)
	malloc(sizeof(struct JSObject_CallInfo));
    memset(pInfo, 0, sizeof(struct JSObject_CallInfo));
    Initialize_JSObject_CallInfo(env, pInfo, pszUrl, 
				 wrapper,
				 jCertArray, 
				 jCertLengthArray, 
				 iNumOfCerts, 
				 jAccessControlContext);    
    pInfo->data.__eval.iNativeJSObject = nativeJSObject;
    pInfo->data.__eval.jResult = NULL;
    pInfo->data.__eval.pszEval = pszStr;
    pInfo->data.__eval.iEvalLen = len;
    pInfo->type = JS_Eval;

    browser_wrapper->JSCall(browser_wrapper->info, jsThreadID, &pInfo);
    
    if (pInfo->data.__eval.jResult)
	ret = (*env)->NewLocalRef(env, pInfo->data.__eval.jResult);
    
    if (pInfo->jException != NULL)
	{
	    jthrowable jException 
		= (jthrowable)((*env)->NewLocalRef(env, pInfo->jException));
	    (*env)->Throw(env, jException);
	}
    if (pszStr)
	(*env)->ReleaseStringChars(env, str, pszStr);
    if (pszUrl)
	(*env)->ReleaseStringUTFChars(env, url, pszUrl);
    
    pInfo->data.__eval.iNativeJSObject =0;
    pInfo->data.__eval.pszEval = NULL;
    pInfo->data.__eval.iEvalLen = 0;
    
    if (pInfo->data.__eval.jResult != NULL)
	(*env)->DeleteGlobalRef(env, pInfo->data.__eval.jResult);
    pInfo->data.__eval.jResult = NULL;
    Clear_JSObject_CallInfo(env, pInfo);
    free(pInfo);
    return ret;
}

JNIEXPORT jstring JNICALL 
Java_sun_jvmp_mozilla_JSObject_JSObjectToString
(JNIEnv *env, 
 jclass jsClass, 
 jint   jsThreadID, 
 jint   nativeJSObject,
 jlong  wrapper)
{
    jstring ret = NULL;       
    struct JSObject_CallInfo* pInfo = NULL;

    if (getBrowserWrapper(env, NULL, jsClass) == NULL) 
	return NULL;
    pInfo = (struct JSObject_CallInfo*)
	malloc(sizeof(struct JSObject_CallInfo));
    memset(pInfo, 0, sizeof(struct JSObject_CallInfo));
    Initialize_JSObject_CallInfo(env, pInfo, NULL, wrapper,
				 NULL, NULL, 0, NULL);    
    pInfo->data.__toString.iNativeJSObject = nativeJSObject;
    pInfo->data.__toString.jResult = NULL;
    pInfo->type = JS_ToString;
    browser_wrapper->JSCall(browser_wrapper->info, jsThreadID, &pInfo);
    
    if (pInfo->data.__toString.jResult)
	ret = (*env)->NewLocalRef(env, pInfo->data.__toString.jResult);
    if (pInfo->jException != NULL)
	{
	    jthrowable jException 
		= (jthrowable)((*env)->NewLocalRef(env, pInfo->jException));
	    (*env)->Throw(env, jException);
	}
    pInfo->data.__toString.iNativeJSObject = 0;
    if (env != NULL && pInfo->data.__toString.jResult != NULL)
	(*env)->DeleteGlobalRef(env, pInfo->data.__toString.jResult);
    pInfo->data.__toString.jResult = NULL;
    Clear_JSObject_CallInfo(env, pInfo);
    free(pInfo);
    return ret;
}	

JNIEXPORT void JNICALL 
Java_sun_jvmp_mozilla_JSObject_JSFinalize
(JNIEnv *env, 
 jclass jsClass, 
 jint   jsThreadID, 
 jint   nativeJSObject,
 jlong  wrapper)
{
    struct JSObject_CallInfo* pInfo = NULL;

    fprintf(stderr, "Java_sun_jvmp_mozilla_JSObject_JSFinalize: %x\n", 
	    nativeJSObject);
    if (getBrowserWrapper(env, NULL, jsClass) == NULL)
	return;
    
    pInfo = (struct JSObject_CallInfo*)
	malloc(sizeof(struct JSObject_CallInfo));
    memset(pInfo, 0, sizeof(struct JSObject_CallInfo));
    Initialize_JSObject_CallInfo(env, pInfo, NULL, wrapper,
				 NULL, NULL, 0, NULL);
    pInfo->data.__finalize.iNativeJSObject = nativeJSObject;
    pInfo->type = JS_Finalize;
    browser_wrapper->JSCall(browser_wrapper->info, 
			    jsThreadID, &pInfo);
    
    if (pInfo->jException != NULL)
	{
	    jthrowable jException 
		= (jthrowable)((*env)->NewLocalRef(env, pInfo->jException));
	    (*env)->Throw(env, jException);
	}
    pInfo->data.__finalize.iNativeJSObject = 0;
    Clear_JSObject_CallInfo(env, pInfo);
    free(pInfo);
}

JNIEXPORT jobject JNICALL 
Java_sun_jvmp_mozilla_JSObject_JSObjectCall
(JNIEnv *env, 
 jclass       jsClass, 
 jint         jsThreadID, 
 jint         nativeJSObject, 
 jlong        wrapper,
 jstring      url, 
 jobjectArray jCertArray, 
 jintArray    jCertLengthArray, 
 jint         iNumOfCerts, 
 jstring      methodName, 
 jobjectArray args, 
 jobject      jAccessControlContext)
{
    struct JSObject_CallInfo* pInfo = NULL;
    jobject ret = NULL;
    const jchar* pszName = NULL;
    const char*  pszUrl = NULL;
    jsize len = 0;

    
    if (getBrowserWrapper(env, NULL, jsClass) == NULL) 
	return NULL;
    pszName = (*env)->GetStringChars(env, methodName, 
				     (jboolean*)0);
    if (url)
      pszUrl = (*env)->GetStringUTFChars(env, url, (jboolean*)0);
    len = (*env)->GetStringLength(env, methodName);    
    
    pInfo = (struct JSObject_CallInfo*)
	malloc(sizeof(struct JSObject_CallInfo));
    memset(pInfo, 0, sizeof(struct JSObject_CallInfo));
    Initialize_JSObject_CallInfo(env, pInfo, pszUrl, 
				 wrapper,
				 jCertArray, 
				 jCertLengthArray, 
				 iNumOfCerts, 
				 jAccessControlContext);

    pInfo->data.__call.iNativeJSObject = nativeJSObject;    
    pInfo->data.__call.pszMethodName = pszName;
    pInfo->data.__call.iMethodNameLen = len;
    if (args != NULL)
	pInfo->data.__call.jArgs = 
	    (jobjectArray)(*env)->NewGlobalRef(env, args);
    pInfo->data.__call.jResult = NULL; 
    pInfo->type = JS_Call;    
	
    browser_wrapper->JSCall(browser_wrapper->info, jsThreadID, &pInfo);

    if (pszName)
      (*env)->ReleaseStringChars(env, methodName, pszName);
    if (pszUrl)
      (*env)->ReleaseStringUTFChars(env, url, pszUrl);
    if (pInfo->data.__call.jResult != NULL)
      {
	ret = (*env)->NewLocalRef(env, pInfo->data.__call.jResult);
	(*env)->DeleteGlobalRef(env, pInfo->data.__call.jResult);
      }
    if (pInfo->data.__call.jArgs != NULL)
	(*env)->DeleteGlobalRef(env, pInfo->data.__call.jArgs);

    if (pInfo->jException != NULL)
	{
	    jthrowable jException 
		= (jthrowable)((*env)->NewLocalRef(env, pInfo->jException));
	    (*env)->Throw(env, jException);
	}
    pInfo->data.__call.iNativeJSObject = 0;
    Clear_JSObject_CallInfo(env, pInfo);
    free(pInfo);
    return ret;
}

JNIEXPORT jobject JNICALL 
Java_sun_jvmp_mozilla_JSObject_JSObjectGetMember
(JNIEnv *env, 
 jclass       jsClass, 
 jint         jsThreadID, 
 jint         nativeJSObject,
 jlong        wrapper,
 jstring      url, 
 jobjectArray jCertArray, 
 jintArray    jCertLengthArray, 
 jint         iNumOfCerts, 
 jstring      name, 
 jobject      jAccessControlContext)
{
  struct JSObject_CallInfo* pInfo = NULL;
  jobject ret = NULL;
  const jchar* pszName = NULL;
  const char*  pszUrl = NULL;
  jsize len = 0;
  
  if (getBrowserWrapper(env, NULL, jsClass) == NULL) 
    return NULL;
  pszName = (*env)->GetStringChars(env, name, 
				     (jboolean*)0);
  if (url)
    pszUrl = (*env)->GetStringUTFChars(env, url, (jboolean*)0);
  len = (*env)->GetStringLength(env, name);    
    
  pInfo = (struct JSObject_CallInfo*)
    malloc(sizeof(struct JSObject_CallInfo));
  memset(pInfo, 0, sizeof(struct JSObject_CallInfo));
  Initialize_JSObject_CallInfo(env, pInfo, pszUrl, 
			       wrapper,
			       jCertArray, 
			       jCertLengthArray, 
			       iNumOfCerts, 
			       jAccessControlContext);
  
  pInfo->data.__getMember.iNativeJSObject = nativeJSObject;
  pInfo->data.__getMember.jResult = NULL;
  pInfo->data.__getMember.pszName = pszName;
  pInfo->data.__getMember.iNameLen = len;
  pInfo->type = JS_GetMember;
 
  browser_wrapper->JSCall(browser_wrapper->info, jsThreadID, &pInfo);
  
  if (pInfo->jException != NULL)
    {
      jthrowable jException 
	= (jthrowable)((*env)->NewLocalRef(env, pInfo->jException));
      (*env)->Throw(env, jException);
    }
  if (pszName)
      (*env)->ReleaseStringChars(env, name, pszName);
  if (pszUrl)
    (*env)->ReleaseStringUTFChars(env, url, pszUrl);
  if (pInfo->data.__getMember.jResult != NULL)
    {
      ret = (*env)->NewLocalRef(env, pInfo->data.__getMember.jResult);
      (*env)->DeleteGlobalRef(env, pInfo->data.__getMember.jResult);
    }
  pInfo->data.__getMember.iNativeJSObject = 0;
  Clear_JSObject_CallInfo(env, pInfo);
  free(pInfo);
  return ret;
}

JNIEXPORT void JNICALL 
Java_sun_jvmp_mozilla_JSObject_JSObjectSetMember
(JNIEnv *env, 
 jclass jsClass, 
 jint jsThreadID, 
 jint nativeJSObject, 
 jlong     wrapper,
 jstring url, 
 jobjectArray jCertArray, 
 jintArray jCertLengthArray, 
 jint iNumOfCerts, 
 jstring name, 
 jobject value, 
 jobject jAccessControlContext)
{
   struct JSObject_CallInfo* pInfo = NULL;
   const jchar* pszName = NULL;
   const char*  pszUrl = NULL;
   jsize len = 0;
   
   if (getBrowserWrapper(env, NULL, jsClass) == NULL) 
     return;
   pszName = (*env)->GetStringChars(env, name, 
				    (jboolean*)0);
  if (url)
    pszUrl = (*env)->GetStringUTFChars(env, url, (jboolean*)0);
  len = (*env)->GetStringLength(env, name);    
  
  pInfo = (struct JSObject_CallInfo*)
    malloc(sizeof(struct JSObject_CallInfo));
  memset(pInfo, 0, sizeof(struct JSObject_CallInfo));
  Initialize_JSObject_CallInfo(env, pInfo, pszUrl, 
			       wrapper,
			       jCertArray, 
			       jCertLengthArray, 
			       iNumOfCerts, 
			       jAccessControlContext);
  pInfo->data.__setMember.iNativeJSObject = nativeJSObject;
  pInfo->data.__setMember.jValue = NULL;
  pInfo->data.__setMember.pszName = pszName;
  pInfo->data.__setMember.iNameLen = len;
  pInfo->type = JS_SetMember;
  if (value != NULL)
    pInfo->data.__setMember.jValue = (*env)->NewGlobalRef(env, value);
  
  browser_wrapper->JSCall(browser_wrapper->info, jsThreadID, &pInfo);
    
  if (pInfo->jException != NULL)
    {
      jthrowable jException 
	= (jthrowable)((*env)->NewLocalRef(env, pInfo->jException));
      (*env)->Throw(env, jException);
    }
  if (pszName)
      (*env)->ReleaseStringChars(env, name, pszName);
  if (pszUrl)
      (*env)->ReleaseStringUTFChars(env, url, pszUrl);
  if (pInfo->data.__setMember.jValue != NULL)
      (*env)->DeleteGlobalRef(env, pInfo->data.__setMember.jValue);
  pInfo->data.__setMember.iNativeJSObject = 0;
  Clear_JSObject_CallInfo(env, pInfo);
  free(pInfo);
  return;
}

JNIEXPORT void JNICALL 
Java_sun_jvmp_mozilla_JSObject_JSObjectRemoveMember
(JNIEnv *env, 
 jclass jsClass, 
 jint jsThreadID, 
 jint nativeJSObject, 
 jlong        wrapper,
 jstring url, 
 jobjectArray jCertArray, 
 jintArray jCertLengthArray, 
 jint iNumOfCerts, 
 jstring name, 
 jobject jAccessControlContext)
{
    struct JSObject_CallInfo* pInfo = NULL;
    const jchar* pszName = NULL;
    const char*  pszUrl = NULL;
    jsize len = 0;
    
    if (getBrowserWrapper(env, NULL, jsClass) == NULL) 
	return;
    pszName = (*env)->GetStringChars(env, name, 
				     (jboolean*)0);
    if (url)
	pszUrl = (*env)->GetStringUTFChars(env, url, (jboolean*)0);
    len = (*env)->GetStringLength(env, name);    
    
    pInfo = (struct JSObject_CallInfo*)
	malloc(sizeof(struct JSObject_CallInfo));
    memset(pInfo, 0, sizeof(struct JSObject_CallInfo));
    Initialize_JSObject_CallInfo(env, pInfo, pszUrl, 
				 wrapper,
				 jCertArray, 
				 jCertLengthArray, 
				 iNumOfCerts, 
				 jAccessControlContext);
    pInfo->data.__removeMember.iNativeJSObject = nativeJSObject;
    pInfo->data.__removeMember.pszName = pszName;
    pInfo->data.__removeMember.iNameLen = len;
    pInfo->type = JS_RemoveMember;

    browser_wrapper->JSCall(browser_wrapper->info, jsThreadID, &pInfo);
    
    if (pInfo->jException != NULL)
	{
	    jthrowable jException 
		= (jthrowable)((*env)->NewLocalRef(env, pInfo->jException));
	    (*env)->Throw(env, jException);
	}
    if (pszName)
	(*env)->ReleaseStringChars(env, name, pszName);
    if (pszUrl)
	(*env)->ReleaseStringUTFChars(env, url, pszUrl);
    pInfo->data.__removeMember.iNativeJSObject = 0;
    Clear_JSObject_CallInfo(env, pInfo);
    free(pInfo);
    return;
}

JNIEXPORT void JNICALL 
Java_sun_jvmp_mozilla_JSObject_JSObjectSetSlot
(JNIEnv *env, 
 jclass jsClass, 
 jint jsThreadID, 
 jint nativeJSObject,
 jlong        wrapper,
 jstring url, 
 jobjectArray jCertArray, 
 jintArray jCertLengthArray, 
 jint iNumOfCerts, 
 jint index, 
 jobject value, 
 jobject jAccessControlContext)
{
   struct JSObject_CallInfo* pInfo = NULL;
   const char*  pszUrl = NULL;
   
   if (getBrowserWrapper(env, NULL, jsClass) == NULL) 
     return;
   if (url)
     pszUrl = (*env)->GetStringUTFChars(env, url, (jboolean*)0);
   
   pInfo = (struct JSObject_CallInfo*)
     malloc(sizeof(struct JSObject_CallInfo));
   memset(pInfo, 0, sizeof(struct JSObject_CallInfo));
   Initialize_JSObject_CallInfo(env, pInfo, pszUrl,
				wrapper,
				jCertArray, 
				jCertLengthArray, 
				iNumOfCerts, 
				jAccessControlContext);
   pInfo->data.__setSlot.iNativeJSObject = nativeJSObject;
   pInfo->data.__setSlot.jValue = NULL;
   pInfo->data.__setSlot.iIndex = index;
   pInfo->type = JS_SetSlot;
   if (value != NULL)
     pInfo->data.__setSlot.jValue = (*env)->NewGlobalRef(env, value);
   
  browser_wrapper->JSCall(browser_wrapper->info, jsThreadID, &pInfo);
  
  if (pInfo->jException != NULL)
    {
      jthrowable jException 
	= (jthrowable)((*env)->NewLocalRef(env, pInfo->jException));
      (*env)->Throw(env, jException);
    }
  if (pszUrl)
    (*env)->ReleaseStringUTFChars(env, url, pszUrl);
  if (pInfo->data.__setSlot.jValue != NULL)
    (*env)->DeleteGlobalRef(env, pInfo->data.__setSlot.jValue);
  pInfo->data.__setSlot.iNativeJSObject = 0;
  Clear_JSObject_CallInfo(env, pInfo);
  free(pInfo);
  return;   
}

JNIEXPORT jobject JNICALL 
Java_sun_jvmp_mozilla_JSObject_JSObjectGetSlot
(JNIEnv *env, 
 jclass jsClass, 
 jint   jsThreadID,
 jint nativeJSObject,
 jlong  wrapper,
 jstring url, 
 jobjectArray jCertArray, 
 jintArray jCertLengthArray, 
 jint iNumOfCerts, 
 jint index, 
 jobject jAccessControlContext)
{
  struct JSObject_CallInfo* pInfo = NULL;
  jobject ret = NULL;
  const char*  pszUrl = NULL;
  
  if (getBrowserWrapper(env, NULL, jsClass) == NULL) 
    return NULL;
  if (url)
    pszUrl = (*env)->GetStringUTFChars(env, url, (jboolean*)0);
  pInfo = (struct JSObject_CallInfo*)
    malloc(sizeof(struct JSObject_CallInfo));
  memset(pInfo, 0, sizeof(struct JSObject_CallInfo));
  Initialize_JSObject_CallInfo(env, pInfo, pszUrl,
			       wrapper,
			       jCertArray, 
			       jCertLengthArray, 
			       iNumOfCerts, 
			       jAccessControlContext);
  
  pInfo->data.__getSlot.iNativeJSObject = nativeJSObject;
  pInfo->data.__getSlot.jResult = NULL;
  pInfo->data.__getSlot.iIndex = index;
  pInfo->type = JS_GetSlot;
 
  browser_wrapper->JSCall(browser_wrapper->info, jsThreadID, &pInfo);
  
   if (pInfo->data.__getSlot.jResult)
    ret = (*env)->NewLocalRef(env, pInfo->data.__getSlot.jResult);    
  if (pInfo->jException != NULL)
    {
      jthrowable jException 
	= (jthrowable)((*env)->NewLocalRef(env, pInfo->jException));
      (*env)->Throw(env, jException);
    }

  if (pszUrl)
    (*env)->ReleaseStringUTFChars(env, url, pszUrl);
  if (pInfo->data.__getSlot.jResult != NULL)
      (*env)->DeleteGlobalRef(env, pInfo->data.__getSlot.jResult);
  pInfo->data.__getSlot.iNativeJSObject = 0;
  Clear_JSObject_CallInfo(env, pInfo);
  free(pInfo);
  return ret;    
}     

JNIEXPORT void JNICALL 
Java_sun_jvmp_mozilla_JavaScriptProtectionDomain_finalize
(JNIEnv  *env, 
 jobject jobj, 
 jlong ctx)
{
  /* do nothing here yet */
  /* fprintf(stderr, "JavaScriptProtectionDomain_finalize\n"); */
}

JNIEXPORT jstring JNICALL 
Java_sun_jvmp_mozilla_JavaScriptProtectionDomain_getCodeBase
(JNIEnv *env, 
 jclass clazz, 
 jlong  rawctx)
{
#define ORIGIN_LEN 512
  char                    origin[ORIGIN_LEN+1];
  SecurityContextWrapper* ctx;
  
  ctx = JLONG_TO_PTR(rawctx);
  if (!ctx || !ctx->GetOrigin(ctx->info, origin, ORIGIN_LEN))
      return NULL;
  return (*env)->NewStringUTF(env, origin);
}


JNIEXPORT jobjectArray JNICALL 
Java_sun_jvmp_mozilla_JavaScriptProtectionDomain_getRawCerts
(JNIEnv *env, 
 jclass clazz, 
 jlong  rawctx)
{
  return NULL; 
}


JNIEXPORT jboolean JNICALL 
Java_sun_jvmp_mozilla_JavaScriptProtectionDomain_implies
(JNIEnv*    env, 
 jobject    jobj, 
 jlong      rawctx, 
 jstring    target, 
 jstring    action)
{
  const char* pTarget = NULL;
  const char* pAction = NULL;
  jboolean    ret     = JNI_FALSE;
  int         res     = 0;
  SecurityContextWrapper* ctx;  
  
  ctx = JLONG_TO_PTR(rawctx);  
  if (!target || !ctx) return ret;
  pTarget = (*env)->GetStringUTFChars(env, target, (jboolean*)0);
  if (action) pAction = (*env)->GetStringUTFChars(env, action, (jboolean*)0);
  if (!ctx->Implies(ctx->info, pTarget, pAction, &res)) ret = JNI_TRUE;
  ret = res ? JNI_TRUE : JNI_FALSE;
  if (pTarget)
    (*env)->ReleaseStringUTFChars(env, target, pTarget);
  if (pAction)
    (*env)->ReleaseStringUTFChars(env, action, pAction);
  return ret;
}



static void 
Initialize_JSObject_CallInfo(JNIEnv*            env, 
			     struct 
			     JSObject_CallInfo* pInfo, 
			     const char*        pszUrl,
			     jlong              wrapper,
			     jobjectArray       jCertArray, 
			     jintArray          jCertLengthArray, 
			     int                iNumOfCerts,
			     jobject            jAccessControlContext)
{    
    pInfo->pszUrl = pszUrl;
    pInfo->jCertArray = NULL;
    pInfo->jCertLengthArray = NULL;
    pInfo->iNumOfCerts = iNumOfCerts;
    pInfo->jAccessControlContext = NULL;
    pInfo->pWrapper = (JavaObjectWrapper*)JLONG_TO_PTR(wrapper);
    pInfo->type = JS_Unknown;
    pInfo->jException = NULL;
    if (jCertArray != NULL)
	pInfo->jCertArray = 
	    (jobjectArray)((*env)->NewGlobalRef(env, jCertArray));
    if (jCertLengthArray != NULL)
	pInfo->jCertLengthArray = 
	    (jintArray)((*env)->NewGlobalRef(env, jCertLengthArray));    
    if (jAccessControlContext != NULL)
	pInfo->jAccessControlContext = 
	    (*env)->NewGlobalRef(env, jAccessControlContext);
}


static void Clear_JSObject_CallInfo(JNIEnv* env, 
				    struct JSObject_CallInfo* pInfo)
{
    pInfo->pszUrl = NULL;
    pInfo->iNumOfCerts = 0;
    pInfo->type = JS_Unknown;
    
    if (pInfo->jCertArray != NULL)
	(*env)->DeleteGlobalRef(env, (jobject)(pInfo->jCertArray));

    if (pInfo->jCertLengthArray != NULL)
	(*env)->DeleteGlobalRef(env, (jobject)(pInfo->jCertLengthArray));

    if (pInfo->jAccessControlContext != NULL)
	(*env)->DeleteGlobalRef(env, pInfo->jAccessControlContext);
    
    if (pInfo->jException != NULL)
	(*env)->DeleteGlobalRef(env, pInfo->jException);

    pInfo->jCertArray = NULL;
    pInfo->jCertLengthArray = NULL;
    pInfo->jAccessControlContext = NULL;
    pInfo->jException = NULL;    
}
static int DoInit(JNIEnv* env)
{
    if (g_inited) return 1;
    
    g_jclass_Object         = 
       (jclass) (*env)->NewGlobalRef(env, (*env)->FindClass(env, "java/lang/Object")); 
    g_jclass_Class          = 
       (jclass) (*env)->NewGlobalRef(env, (*env)->FindClass(env, "java/lang/Class"));    
    g_jclass_Boolean        = 
       (jclass) (*env)->NewGlobalRef(env, (*env)->FindClass(env, "java/lang/Boolean"));
    g_jclass_Byte           = 
       (jclass) (*env)->NewGlobalRef(env, (*env)->FindClass(env, "java/lang/Byte"));
    g_jclass_Character      = 
       (jclass) (*env)->NewGlobalRef(env, (*env)->FindClass(env, "java/lang/Character"));
    g_jclass_Short          = 
       (jclass) (*env)->NewGlobalRef(env, (*env)->FindClass(env, "java/lang/Short"));
    g_jclass_Integer        = 
       (jclass) (*env)->NewGlobalRef(env, (*env)->FindClass(env, "java/lang/Integer"));
    g_jclass_Long           = 
       (jclass) (*env)->NewGlobalRef(env, (*env)->FindClass(env, "java/lang/Long"));
    g_jclass_Float          = 
       (jclass) (*env)->NewGlobalRef(env, (*env)->FindClass(env, "java/lang/Float"));
    g_jclass_Double         = 
       (jclass) (*env)->NewGlobalRef(env, (*env)->FindClass(env, "java/lang/Double"));
    g_jclass_Void           = 
       (jclass) (*env)->NewGlobalRef(env, (*env)->FindClass(env, "java/lang/Void"));

    g_jmethod_Boolean_booleanValue  = 
       (*env)->GetMethodID(env, g_jclass_Boolean, "booleanValue", "()Z");
    g_jmethod_Byte_byteValue        = 
       (*env)->GetMethodID(env, g_jclass_Byte, "byteValue", "()B");
    g_jmethod_Character_charValue   = 
       (*env)->GetMethodID(env, g_jclass_Character, "charValue", "()C");
    g_jmethod_Short_shortValue      = 
       (*env)->GetMethodID(env, g_jclass_Short, "shortValue", "()S");
    g_jmethod_Integer_intValue      = 
       (*env)->GetMethodID(env, g_jclass_Integer, "intValue", "()I");
    g_jmethod_Long_longValue        = 
       (*env)->GetMethodID(env, g_jclass_Long, "longValue", "()J");
    g_jmethod_Float_floatValue      = 
       (*env)->GetMethodID(env, g_jclass_Float, "floatValue", "()F");
    g_jmethod_Double_doubleValue    = 
       (*env)->GetMethodID(env, g_jclass_Double, "doubleValue", "()D");

    g_jmethod_Boolean_init      = 
       (*env)->GetMethodID(env, g_jclass_Boolean, "<init>", "(Z)V");
    g_jmethod_Byte_init         = 
       (*env)->GetMethodID(env, g_jclass_Byte, "<init>", "(B)V");
    g_jmethod_Character_init    = 
       (*env)->GetMethodID(env, g_jclass_Character, "<init>", "(C)V");
    g_jmethod_Short_init        = 
       (*env)->GetMethodID(env, g_jclass_Short, "<init>", "(S)V");
    g_jmethod_Integer_init      = 
       (*env)->GetMethodID(env, g_jclass_Integer, "<init>", "(I)V");
    g_jmethod_Long_init         = 
       (*env)->GetMethodID(env, g_jclass_Long, "<init>", "(J)V");
    g_jmethod_Float_init        = 
       (*env)->GetMethodID(env, g_jclass_Float, "<init>", "(F)V");
    g_jmethod_Double_init       = 
       (*env)->GetMethodID(env, g_jclass_Double, "<init>", "(D)V");
     
    g_jclass_SecureInvocation = 
	(jclass) (*env)->NewGlobalRef(env, 
				      (*env)->FindClass(env, "sun/jvmp/mozilla/SecureInvocation"));
    if (!g_jclass_SecureInvocation) return 0;
    g_jmethod_SecureInvocation_ConstructObject = 
	(*env)->GetStaticMethodID(env, g_jclass_SecureInvocation,
				  "ConstructObject",
				  "(Ljava/lang/reflect/Constructor;[Ljava/lang/Object;J)Ljava/lang/Object;");
    if (!g_jmethod_SecureInvocation_ConstructObject) return 0;
    g_jmethod_SecureInvocation_CallMethod = 
	(*env)->GetStaticMethodID(env, g_jclass_SecureInvocation,
				  "CallMethod",
				  "(Ljava/lang/Object;Ljava/lang/reflect/Method;[Ljava/lang/Object;J)Ljava/lang/Object;");
    if (!g_jmethod_SecureInvocation_CallMethod) return 0;
    g_jmethod_SecureInvocation_GetField = 
	(*env)->GetStaticMethodID(env, g_jclass_SecureInvocation,
				  "GetField",
				  "(Ljava/lang/Object;Ljava/lang/reflect/Field;J)Ljava/lang/Object;");
    if (!g_jmethod_SecureInvocation_GetField) return 0;
    g_jmethod_SecureInvocation_SetField = 
	(*env)->GetStaticMethodID(env, g_jclass_SecureInvocation,
				  "SetField",
				  "(Ljava/lang/Object;Ljava/lang/reflect/Field;Ljava/lang/Object;J)V");
    if (!g_jmethod_SecureInvocation_SetField) return 0; 
    g_jclass_JSObject = 
	(jclass) (*env)->NewGlobalRef(env, 
				      (*env)->FindClass(env, "sun/jvmp/mozilla/JSObject"));
    if (!g_jclass_JSObject) return 0;
    g_inited = 1;
    return 1;
}
/* kinda hacky, but makes code smaller */
#ifdef XP_WIN32
#define strcasecmp stricmp
#endif
static int ConvertJValueToJava(JNIEnv*   env,
			       jvalue    val, 
			       jclass    type, 
                               jobject*  result)
{
  static  jmethodID m = NULL;
  jstring           name;
  const char*       szName;
  
  if (!m)
    {
      m = (*env)->GetMethodID(env, 
			      g_jclass_Class, 
			      "getName", 
			      "()Ljava/lang/String;");
    }
  name = (jstring) (*env)->CallObjectMethod(env, type, m);
  szName = (*env)->GetStringUTFChars(env, name, JNI_FALSE);
  if (strcasecmp(szName, "boolean") == 0)  
    {
      *result = (*env)->NewObject(env,
				  g_jclass_Boolean, 
				  g_jmethod_Boolean_init, 
				  val.z);
    }
  else if (strcasecmp(szName, "byte") == 0)
    {
      *result = (*env)->NewObject(env,
				  g_jclass_Byte, 
				  g_jmethod_Byte_init, 
				  val.b);
    }
  else if (strcasecmp(szName, "char") == 0)
    {
      *result = (*env)->NewObject(env,
				  g_jclass_Character, 
				  g_jmethod_Character_init, 
				  val.c);
    }
  else if (strcasecmp(szName, "short") == 0)
    {
      *result = (*env)->NewObject(env,
				  g_jclass_Short,
				  g_jmethod_Short_init, 
				  val.s);
    }
  else if (strcasecmp(szName, "int") == 0)
    {
      *result = (*env)->NewObject(env,
				  g_jclass_Integer, 
				  g_jmethod_Integer_init, 
				  val.i);
    }
  else if (strcasecmp(szName, "long") == 0)
    {
      *result = (*env)->NewObject(env,
				  g_jclass_Long, 
				  g_jmethod_Long_init, 
				  val.j);
    }
  else if (strcasecmp(szName, "float") == 0)
    {
      *result = (*env)->NewObject(env,
				  g_jclass_Float, 
				  g_jmethod_Float_init, 
				  val.f);
    }
  else if (strcasecmp(szName, "double") == 0)
    {
      *result = (*env)->NewObject(env,
				  g_jclass_Double, 
				  g_jmethod_Double_init, 
				  val.d);
    }
  else /* Object */
    {
      *result = val.l;
    }
  
  (*env)->ReleaseStringUTFChars(env, name, szName);
  
  return 1;
}

static int ConvertJavaToJValue(JNIEnv*            env,
			       enum jvmp_jni_type type, 
			       jobject            obj, 
			       jvalue             *result)
{
    if (obj == NULL)
        return 0;

    switch(type)
    {
        case jvmp_jboolean_type: {
            result->z = (*env)->CallBooleanMethod(env, obj, g_jmethod_Boolean_booleanValue);            
            break;
        }
         case jvmp_jbyte_type:    {
            result->b = (*env)->CallByteMethod(env, obj, g_jmethod_Byte_byteValue);
            break;
        }
        case jvmp_jchar_type:    {
            result->c = (*env)->CallCharMethod(env, obj, g_jmethod_Character_charValue);            
            break;
        }
        case jvmp_jshort_type:   {
            result->s = (*env)->CallShortMethod(env, obj, g_jmethod_Short_shortValue);            
            break;
        }
        case jvmp_jint_type:     {
            result->i = (*env)->CallIntMethod(env, obj, g_jmethod_Integer_intValue);            
            break;
        }
        case jvmp_jlong_type:   {
            result->j = (*env)->CallLongMethod(env, obj, g_jmethod_Long_longValue);           
            break;
        }
        case jvmp_jfloat_type:  {
            result->f = (*env)->CallFloatMethod(env, obj, g_jmethod_Float_floatValue);            
            break;
        }
        case jvmp_jdouble_type: {
            result->d = (*env)->CallDoubleMethod(env, obj, g_jmethod_Double_doubleValue);           
            break;
        }
        case jvmp_jobject_type:
        {
            result->l = (*env)->NewGlobalRef(env, obj);
            break;
        }
        case jvmp_jvoid_type:
        {
            break;
        }
        default:
        {
            return 0;
        }
    }
   return 1;
}


static int ConvertJValueArrayToJavaArray(JNIEnv*       env,
					 jobject       method,
					 jvalue*       obj, 
					 jobjectArray* result)
{
  jclass    clazz, type;
  jmethodID m = NULL;
  jobjectArray params;
  int len = 0, i;
  jobject val = NULL;

  if (obj == NULL)
    {
      if (result != NULL)
	*result = NULL;
      return 1;
    }

  clazz = (*env)->GetObjectClass(env, method);

  m = (*env)->GetMethodID(env, clazz, 
			  "getParameterTypes", 
			  "()[Ljava/lang/Class;");

  params = (jobjectArray)(*env)->CallObjectMethod(env, 
						  method, 
						  m);        
  if (params)
    {
      len = (*env)->GetArrayLength(env, params);
      *result = (*env)->NewObjectArray(env, len, g_jclass_Object, NULL);
    }

  for (i=0; i < len; i++)
    {
      type = (jclass) (*env)->NewLocalRef(env, (*env)->GetObjectArrayElement(env, params, i));
      ConvertJValueToJava(env, obj[i], type, &val);
      (*env)->SetObjectArrayElement(env, *result, i, val);
      (*env)->DeleteLocalRef(env, type);
    }
  return 1;
}

static int CreateObject(JNIEnv*                   env,
			jobject                   constructor,
			jvalue*                   arg,
			SecurityContextWrapper*   security,
			jobject                   *result)
{
  jobjectArray jarg = NULL;

  if (!(ConvertJValueArrayToJavaArray(env, constructor, arg, &jarg)))
    return 0;
  *result = (*env)->CallStaticObjectMethod(env,
					   g_jclass_SecureInvocation,
					   g_jmethod_SecureInvocation_ConstructObject,
					   constructor, 
					   jarg, 
					   JLONG_TO_PTR(security));
  return 1;
}

static int CallMethod(JNIEnv*                   env, 
		      enum jvmp_jni_type        type,
		      jobject                   obj, 
		      jobject                   method, 
		      jvalue*                   arg,
		      SecurityContextWrapper*   security,
		      jvalue*                   result)
{
    jobjectArray jarg;
    jobject ret;
    
    if (!(ConvertJValueArrayToJavaArray(env, method, arg, &jarg))) return 0;
    ret = (*env)->CallStaticObjectMethod(env, g_jclass_SecureInvocation,
					 g_jmethod_SecureInvocation_CallMethod,
					 obj, method, jarg, PTR_TO_JLONG(security));
    if ((*env)->ExceptionOccurred(env))
	{
	    (*env)->ExceptionClear(env);
	    return 0;
	}
    return ConvertJavaToJValue(env, type, ret, result);
}

static int SetField(JNIEnv*                     env,
		    enum jvmp_jni_type          type,
                    jobject                     obj, 
                    jobject                     field, 
                    jvalue                      val, 
		    SecurityContextWrapper*     security)
{  
  jclass    fieldClazz, jtype;
  jmethodID m;
  jobject   jval=NULL;

  fieldClazz = (*env)->GetObjectClass(env, field);
  m = (*env)->GetMethodID(env,
			  fieldClazz,
			  "getType", 
			  "()Ljava/lang/Class;");

  jtype = (jclass) (*env)->CallObjectMethod(env, field, m);
  if ((ConvertJValueToJava(env, val, jtype, &jval)))
    {
      (*env)->CallStaticVoidMethod(env,
				   g_jclass_SecureInvocation,
				   g_jmethod_SecureInvocation_SetField, 
				   obj, 
				   field, 
				   jval, 
				   PTR_TO_JLONG(security));
      return 1;
    }
  return 0;
}


static int GetField(JNIEnv*                     env,
		    enum jvmp_jni_type          type,
                    jobject                     obj, 
                    jobject                     field, 
		    SecurityContextWrapper*     security,
		    jvalue                      *result)
{
  jobject ret = NULL;

  ret = (*env)->CallStaticObjectMethod(env,
				       g_jclass_SecureInvocation,
				       g_jmethod_SecureInvocation_GetField,
				       obj, 
				       field, 
				       PTR_TO_JLONG(security));
  return ConvertJavaToJValue(env, type, ret, result);
}
