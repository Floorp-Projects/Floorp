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
 * The Original Code is The Waterfall Java Plugin Module
 * 
 * The Initial Developer of the Original Code is Sun Microsystems Inc
 * Portions created by Sun Microsystems Inc are Copyright (C) 2001
 * All Rights Reserved.
 *
 * $Id: nsWFSecureEnv.h,v 1.1 2001/05/09 18:51:36 edburns%acm.org Exp $
 *
 * 
 * Contributor(s): 
 *
 *   Nikolay N. Igotti <inn@sparc.spb.su>
 */

#ifndef __nsWFSecureEnv_h
#define __nsWFSecureEnv_h
#include "nsoji2.h"
#include "nsISecureEnv.h"
#include "nsPluggableJVM.h"
#include "wf_moz6_common.h"

class nsWFSecureEnv : public nsISecureEnv
{
 public:
  NS_DECL_ISUPPORTS
  static NS_METHOD Create(nsPluggableJVM* jvm, 
			  const nsIID& aIID, 
			  void* *aInstancePtr);
  static NS_METHOD  CreateJavaCallInfo(Java_CallInfo*      *pCall,
				       nsISecurityContext* ctx);
  static NS_METHOD  CleanupJavaCallInfo(Java_CallInfo*      pCall,
					nsISecurityContext* ctx);  
  // here follows methods from nsISecureEnv - no IDL and 
  // cute macroses yet :(
   NS_IMETHOD NewObject(/*[in]*/  jclass clazz, 
			/*[in]*/  jmethodID methodID,
			/*[in]*/  jvalue *args, 
			/*[out]*/ jobject* result,
			/*[in]*/  nsISecurityContext* ctx = NULL);

   NS_IMETHOD CallMethod(/*[in]*/  jni_type ret_type,
			 /*[in]*/  jobject obj, 
			 /*[in]*/  jmethodID methodID,
			 /*[in]*/  jvalue *args, 
			 /*[out]*/ jvalue* result,
			 /*[in]*/  nsISecurityContext* ctx = NULL);
   
   NS_IMETHOD CallNonvirtualMethod(/*[in]*/  jni_type ret_type,
				   /*[in]*/  jobject obj, 
				   /*[in]*/  jclass clazz,
				   /*[in]*/  jmethodID methodID,
				   /*[in]*/  jvalue *args, 
				   /*[out]*/ jvalue* result,
				   /*[in]*/  nsISecurityContext* ctx = NULL);

   NS_IMETHOD GetField(/*[in]*/  jni_type field_type,
		       /*[in]*/  jobject obj, 
		       /*[in]*/  jfieldID fieldID,
		       /*[out]*/ jvalue* result,
		       /*[in]*/  nsISecurityContext* ctx = NULL);

   NS_IMETHOD SetField(/*[in]*/ jni_type field_type,
		       /*[in]*/ jobject obj, 
		       /*[in]*/ jfieldID fieldID,
		       /*[in]*/ jvalue val,
		       /*[in]*/ nsISecurityContext* ctx = NULL);
   
   NS_IMETHOD CallStaticMethod(/*[in]*/  jni_type ret_type,
                                /*[in]*/  jclass clazz,
                                /*[in]*/  jmethodID methodID,
                                /*[in]*/  jvalue *args, 
                                /*[out]*/ jvalue* result,
                                /*[in]*/  nsISecurityContext* ctx = NULL);
   
   NS_IMETHOD GetStaticField(/*[in]*/  jni_type field_type,
			     /*[in]*/  jclass clazz, 
			     /*[in]*/  jfieldID fieldID, 
			     /*[out]*/ jvalue* result,
			     /*[in]*/  nsISecurityContext* ctx = NULL);

   NS_IMETHOD SetStaticField(/*[in]*/ jni_type field_type,
			     /*[in]*/ jclass clazz, 
			     /*[in]*/ jfieldID fieldID,
			     /*[in]*/ jvalue val,
			     /*[in]*/ nsISecurityContext* ctx = NULL);
   
   NS_IMETHOD GetVersion(/*[out]*/ jint* version);
   
   NS_IMETHOD DefineClass(/*[in]*/  const char* name,
                           /*[in]*/  jobject loader,
                           /*[in]*/  const jbyte *buf,
                           /*[in]*/  jsize len,
                           /*[out]*/ jclass* clazz);

    NS_IMETHOD FindClass(/*[in]*/  const char* name, 
                         /*[out]*/ jclass* clazz);

    NS_IMETHOD GetSuperclass(/*[in]*/  jclass sub,
                             /*[out]*/ jclass* super);

    NS_IMETHOD IsAssignableFrom(/*[in]*/  jclass sub,
                                /*[in]*/  jclass super,
                                /*[out]*/ jboolean* result);

    NS_IMETHOD Throw(/*[in]*/  jthrowable obj,
                     /*[out]*/ jint* result);

    NS_IMETHOD ThrowNew(/*[in]*/  jclass clazz,
                        /*[in]*/  const char *msg,
                        /*[out]*/ jint* result);

    NS_IMETHOD ExceptionOccurred(/*[out]*/ jthrowable* result);

    NS_IMETHOD ExceptionDescribe(void);

    NS_IMETHOD ExceptionClear(void);

     NS_IMETHOD FatalError(/*[in]*/ const char* msg);

    NS_IMETHOD NewGlobalRef(/*[in]*/  jobject lobj, 
                            /*[out]*/ jobject* result);

    NS_IMETHOD DeleteGlobalRef(/*[in]*/ jobject gref);

    NS_IMETHOD DeleteLocalRef(/*[in]*/ jobject obj);

    NS_IMETHOD IsSameObject(/*[in]*/  jobject obj1,
                            /*[in]*/  jobject obj2,
                            /*[out]*/ jboolean* result);

    NS_IMETHOD AllocObject(/*[in]*/  jclass clazz,
                           /*[out]*/ jobject* result);

    NS_IMETHOD GetObjectClass(/*[in]*/  jobject obj,
                              /*[out]*/ jclass* result);

    NS_IMETHOD IsInstanceOf(/*[in]*/  jobject obj,
                            /*[in]*/  jclass clazz,
                            /*[out]*/ jboolean* result);

    NS_IMETHOD GetMethodID(/*[in]*/  jclass clazz, 
                           /*[in]*/  const char* name,
                           /*[in]*/  const char* sig,
                           /*[out]*/ jmethodID* id);

    NS_IMETHOD GetFieldID(/*[in]*/  jclass clazz, 
                          /*[in]*/  const char* name,
                          /*[in]*/  const char* sig,
                          /*[out]*/ jfieldID* id);

    NS_IMETHOD GetStaticMethodID(/*[in]*/  jclass clazz, 
                                 /*[in]*/  const char* name,
                                 /*[in]*/  const char* sig,
                                 /*[out]*/ jmethodID* id);

    NS_IMETHOD GetStaticFieldID(/*[in]*/  jclass clazz, 
                                /*[in]*/  const char* name,
                                /*[in]*/  const char* sig,
                                /*[out]*/ jfieldID* id);

    NS_IMETHOD NewString(/*[in]*/  const jchar* unicode,
                         /*[in]*/  jsize len,
                         /*[out]*/ jstring* result);

     NS_IMETHOD GetStringLength(/*[in]*/  jstring str,
                               /*[out]*/ jsize* result);
    
    NS_IMETHOD GetStringChars(/*[in]*/  jstring str,
                              /*[in]*/  jboolean *isCopy,
                              /*[out]*/ const jchar** result);

    NS_IMETHOD ReleaseStringChars(/*[in]*/  jstring str,
                                  /*[in]*/  const jchar *chars);

    NS_IMETHOD NewStringUTF(/*[in]*/  const char *utf,
                            /*[out]*/ jstring* result);

    NS_IMETHOD GetStringUTFLength(/*[in]*/  jstring str,
                                  /*[out]*/ jsize* result);
    
    NS_IMETHOD GetStringUTFChars(/*[in]*/  jstring str,
                                 /*[in]*/  jboolean *isCopy,
                                 /*[out]*/ const char** result);

    NS_IMETHOD ReleaseStringUTFChars(/*[in]*/  jstring str,
                                     /*[in]*/  const char *chars);

    NS_IMETHOD GetArrayLength(/*[in]*/  jarray array,
			      /*[out]*/ jsize* result);

    NS_IMETHOD GetObjectArrayElement(/*[in]*/  jobjectArray array,
                                     /*[in]*/  jsize index,
                                     /*[out]*/ jobject* result);

    NS_IMETHOD SetObjectArrayElement(/*[in]*/  jobjectArray array,
                                     /*[in]*/  jsize index,
                                     /*[in]*/  jobject val);

    NS_IMETHOD NewObjectArray(/*[in]*/  jsize len,
                                              /*[in]*/  jclass clazz,
                              /*[in]*/  jobject init,
                              /*[out]*/ jobjectArray* result);

    NS_IMETHOD NewArray(/*[in]*/ jni_type element_type,
                        /*[in]*/  jsize len,
                        /*[out]*/ jarray* result);

    NS_IMETHOD GetArrayElements(/*[in]*/  jni_type element_type,
                                /*[in]*/  jarray array,
				 /*[in]*/  jboolean *isCopy,
                                /*[out]*/ void* result);

    NS_IMETHOD ReleaseArrayElements(/*[in]*/ jni_type element_type,
                                    /*[in]*/ jarray array,
                                    /*[in]*/ void *elems,
                                    /*[in]*/ jint mode);

    NS_IMETHOD GetArrayRegion(/*[in]*/  jni_type element_type,
                              /*[in]*/  jarray array,
                              /*[in]*/  jsize start,
                              /*[in]*/  jsize len,
                              /*[out]*/ void* buf);

    NS_IMETHOD SetArrayRegion(/*[in]*/  jni_type element_type,
                              /*[in]*/  jarray array,
                              /*[in]*/  jsize start,
                              /*[in]*/  jsize len,
                              /*[in]*/  void* buf);

    NS_IMETHOD RegisterNatives(/*[in]*/  jclass clazz,
                               /*[in]*/  const JNINativeMethod *methods,
                               /*[in]*/  jint nMethods,
			       /*[out]*/ jint* result);
    
    NS_IMETHOD UnregisterNatives(/*[in]*/  jclass clazz,
                                 /*[out]*/ jint* result);

    NS_IMETHOD MonitorEnter(/*[in]*/  jobject obj,
                            /*[out]*/ jint* result);

    NS_IMETHOD MonitorExit(/*[in]*/  jobject obj,
                           /*[out]*/ jint* result);

    NS_IMETHOD GetJavaVM(/*[in]*/  JavaVM **vm,
                         /*[out]*/ jint* result);
                         
    nsWFSecureEnv(nsPluggableJVM* jvm);
    virtual ~nsWFSecureEnv(void);
 protected:
    nsPluggableJVM* m_jvm;
    // as for now all LiveConnect calls comes from the main thread,
    // it's correct to use one JNI env and cache it here.
    // if LiveConnect will be truly mutlithreaded - it should be rewritten
    JNIEnv*               m_env;
    JVMP_RuntimeContext*  m_rtctx;    
    JVMP_CallingContext*  m_ctx;    
};

#endif //ifndef __nsWFSecureEnv_h
