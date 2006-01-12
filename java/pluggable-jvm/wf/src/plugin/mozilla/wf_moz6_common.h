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
 * $Id: wf_moz6_common.h,v 1.3 2006/01/12 07:57:22 timeless%mozdev.org Exp $
 * 
 * Contributor(s):
 * 
 *     Nikolay N. Igotti <nikolay.igotti@Sun.Com>
 */

#ifndef __wf_moz6_common_h
#define __wf_moz6_common_h
/** Ideology behind this file:
 *  To keep generic binary compatibility, and be independent from Mozilla 
 *  build system for Mozilla extension (not very good idea, but currently
 *  it's more convenient) we need some kind of fixed API/ABI between
 *  extension and Java service in browser. This file is _only_ (beside 
 *  jvmp.h) common header between Java component and extension. It's written
 *  in pure C and has no Mozilla dependencies. So JVM service in browser fills
 *  propers callbacks and the extension, if wishes to perform some call to 
 *  Mozilla uses those callbacks (and maybe even in another direction, but
 *  it's not recommened - use JVMP_Direct*Call for such purposes).
 *  Here described structures for 
 *     - common browser configuration callbacks (show status, show document, 
 *       get proxy config, add anything you want)
 *     - LiveConnect calls and security
 *  JavaObjectWrapper is wrapper for java-driven objects on browser side, 
 *                       usually lifetime coincides with lifetime of web page.
 *                       If underlying native object is dead, proper field is 1.
 *  SecurityContextWrapper is wrapper for security context is 
 *                            LiveConnect calls, lifetime coincide with lifetime 
 *                            of LiveConnect call.
 *  BrowserSupportWrapper is wrapper for global JVM service object, 
 *                           exist always until JVM service is loaded, can be
 *                           used for global operations not related to browser
 *  NB: If any other browser will implement those callbacks and be kind enough 
 *  to follow Mozilla protocol in inline plugin handling, it will have 
 *  Java + applets + LiveConnect for free. For example MSIE code can be written
 *  in such a manner.
 **/

struct SecurityContextWrapper {
  void* info;
  int (*Implies)  (void* handle, const char* target, 
		   const char* action, int *bAllowedAccess);
  int (*GetOrigin)(void* handle, char *buffer, int len);
  int (*GetCertificateID)(void* handle, char *buffer, int len);
  int dead;
};
typedef struct SecurityContextWrapper SecurityContextWrapper;
struct JavaObjectWrapper;
typedef struct JavaObjectWrapper JavaObjectWrapper;

#define WF_APPLETPEER_CID "@sun.com/wf/mozilla/appletpeer;1"

enum JSCallType {	    /*	Possible JS call type */
	JS_Unknown = 0,
	JS_GetWindow = 1,
	JS_ToString,
	JS_Eval,
	JS_Call,
	JS_GetMember,
	JS_SetMember,
	JS_RemoveMember,
	JS_GetSlot,
	JS_SetSlot,
	JS_Finalize
    };

struct JSObject_CallInfo
{
    const char*		pszUrl;			
    jobjectArray	jCertArray;		
    jintArray		jCertLengthArray;	
    int			iNumOfCerts;		
    jobject		jAccessControlContext;	
    enum JSCallType	type;			
    jthrowable		jException;	        
    JavaObjectWrapper*  pWrapper;               
    union {
	struct __GetWindow {
	    void*    pPlugin;		
	    jint	    iNativeJSObject;
	} __getWindow;
	struct __Finalize  {
	    jint	iNativeJSObject;	
	} __finalize;
	struct __ToString  {
	    jint	iNativeJSObject;	
	    jstring	jResult;		
	} __toString;
	struct __Call  {
	    const jchar*    pszMethodName;	
	  int		    iMethodNameLen;	
	    jobjectArray    jArgs;		
 	    jint	    iNativeJSObject;	
	    jobject	    jResult;		
	} __call;
	struct __Eval  {
	    const jchar*    pszEval;		
	    int		    iEvalLen;		
	    jint	    iNativeJSObject;	
	    jobject	    jResult;		
	} __eval;
	struct __GetMember  {
	    const jchar*    pszName;		
	    int		    iNameLen;		
	    jint	    iNativeJSObject;	
	    jobject	    jResult;		
	} __getMember;
	struct __SetMember  {
	    const jchar*    pszName;		
	    int		    iNameLen;		
	    jint	    iNativeJSObject;	
	    jobject	    jValue;		
	} __setMember;
	struct __RemoveMember  {
	    const jchar*    pszName;		
	    int		    iNameLen;		
	    jint	    iNativeJSObject;	
	} __removeMember;
	struct __GetSlot  {
	    int		    iIndex;		
	    jint	    iNativeJSObject;	
	    jobject	    jResult;		
	} __getSlot;
	struct __SetSlot  {
	    int		    iIndex;		
	    jobject	    jValue;		
	    jint	    iNativeJSObject;	
	} __setSlot;
    } data;
};

enum JavaCallType {
  Java_Unknown = 0,
  Java_WrapJSObject,
  Java_CreateObject,
  Java_CallMethod,
  Java_CallStaticMethod,
  Java_CallNonvirtualMethod,
  Java_SetField,
  Java_SetStaticField,
  Java_GetField,
  Java_GetStaticField,
  Java_UnwrapJObject
};

/* just a copy of enum jni_type from nsISecureEnv, as I have to get 
   it from somewhere - values are the same, so can convert flawlessly */
enum jvmp_jni_type 
{
    jvmp_jobject_type = 0,
    jvmp_jboolean_type,
    jvmp_jbyte_type,
    jvmp_jchar_type,
    jvmp_jshort_type,
    jvmp_jint_type,
    jvmp_jlong_type,
    jvmp_jfloat_type,
    jvmp_jdouble_type,
    jvmp_jvoid_type
};

struct Java_CallInfo
{
  enum JavaCallType       type;
  jthrowable              jException;
  SecurityContextWrapper* security;
  union {
    struct __WrapJSObject {
      jint     jsObject;
      jint     jstid;
      jobject  jObject ;
    } __wrapJSObject;    
    struct __CreateObject {
      jclass                  clazz;
      jmethodID               methodID;
      jvalue                  *args;
      jobject                 result;      
    } __createObject;
    struct __CallMethod {
      enum jvmp_jni_type      type;
      jobject                 obj;
      jmethodID               methodID;
      jvalue                  *args;
      jvalue                  result;
    } __callMethod;
    struct __CallStaticMethod {
      enum jvmp_jni_type      type;
      jclass                  clazz;
      jmethodID               methodID;
      jvalue                  *args;
      jvalue                  result;
    } __callStaticMethod;
    struct __CallNonvirtualMethod {
      enum jvmp_jni_type      type;
      jobject                 obj;
      jclass                  clazz;
      jmethodID               methodID;
      jvalue                  *args;
      jvalue                  result;
    } __callNonvirtualMethod;
    struct __GetField {
      enum jvmp_jni_type      type;
      jobject                 obj;
      jfieldID                fieldID;
      jvalue                  result;
    } __getField;   
    struct __GetStaticField {
      enum jvmp_jni_type      type;
      jclass                  clazz;
      jfieldID                fieldID;
      jvalue                  result;
    } __getStaticField;   
    struct __SetField {
      enum jvmp_jni_type      type;
      jobject                 obj;
      jfieldID                fieldID;
      jvalue                  value;
    } __setField;    
    struct __SetStaticField {
      enum jvmp_jni_type      type;
      jclass                  clazz;
      jfieldID                fieldID;
      jvalue                  value;      
    } __setStaticField;
    struct __UnwrapJObject {
      jobject  jObject;
      jint     jstid;
      jint     jsObject ;
    } __unwrapJObject;
  } data;
};


struct JavaObjectWrapper
{  
  void* info;
  void* plugin;
  int (*GetParameters)(void* handle,   char ***keys, 
		       char ***values, int *count);
  int (*ShowDocument)(void* handle, char* url, char* target);
  int (*ShowStatus)(void* handle, char* status);
  int (*GetJSThread)(void* handle, jint* jstid);
  int dead;
};

struct BrowserSupportWrapper
{
  void* info;
  int (*GetProxyForURL)(void* handle, char* url, char** proxy);
  int (*JSCall)(void* handle, jint jstid, 
		struct JSObject_CallInfo** jscall);
};
typedef struct BrowserSupportWrapper BrowserSupportWrapper;

#endif
