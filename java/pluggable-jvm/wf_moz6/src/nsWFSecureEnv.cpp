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
 * $Id: nsWFSecureEnv.cpp,v 1.2 2001/07/12 20:32:09 edburns%acm.org Exp $
 *
 * 
 * Contributor(s): 
 *
 *   Nikolay N. Igotti <inn@sparc.spb.su>
 */

#include "nsDebug.h"
#include "nsWFSecureEnv.h"
#include "nsCRT.h"

// Caching of those two gives significant performance gain and is safe.
// Instead of time consuming reflection and call to Waterfall extension,
// we'll use plain JNI call.
static jclass    g_javaLangSystem      = NULL;
static jmethodID g_identityHashCodeMID =  NULL;
 
NS_IMPL_ISUPPORTS1(nsWFSecureEnv, nsISupports)

NS_METHOD 
nsWFSecureEnv::Create(nsPluggableJVM* jvm, 
		      const nsIID& aIID, 
		      void* *aInstancePtr)
{
  nsWFSecureEnv* env = new nsWFSecureEnv(jvm);
  if (env == NULL) return NS_ERROR_OUT_OF_MEMORY;
  return env->QueryInterface(aIID, aInstancePtr);
  
}

nsWFSecureEnv::nsWFSecureEnv(nsPluggableJVM* jvm)
{
  NS_INIT_ISUPPORTS();
  m_jvm = jvm;
  NS_ADDREF(m_jvm);
  m_jvm->GetWFCtx(&m_rtctx, &m_ctx);
  m_env = m_ctx->env;
  NS_ASSERTION(m_env != nsnull, 
	       "Error: env field not initialized by this Waterfall implementation.");
  if (!g_javaLangSystem) 
    g_javaLangSystem = (jclass)m_env->NewGlobalRef(m_env->FindClass("java/lang/System"));
  if (!g_identityHashCodeMID)
    g_identityHashCodeMID = m_env->GetStaticMethodID(g_javaLangSystem, 
						     "identityHashCode", 
						     "(Ljava/lang/Object;)I");
}

nsWFSecureEnv::~nsWFSecureEnv(void)
{
  OJI_LOG("nsWFSecureEnv::~nsWFSecureEnv");
  NS_RELEASE(m_jvm);
}
static int Implies(void* handle, const char* target, 
		   const char* action, int *bAllowedAccess)
{
  nsISecurityContext* ctx = (nsISecurityContext*)handle;
  if (ctx) return NS_SUCCEEDED(ctx->Implies(target, action, bAllowedAccess));
  return 0;
}

static int GetOrigin(void* handle, char *buffer, int len)
{
  nsISecurityContext* ctx = (nsISecurityContext*)handle;
  if (ctx)  return NS_SUCCEEDED(ctx->GetOrigin(buffer, len));
  return 0;
}

static int GetCertificateID(void* handle, char *buffer, int len)
{
  nsISecurityContext* ctx = (nsISecurityContext*)handle;
  if (ctx) return NS_SUCCEEDED(ctx->GetCertificateID(buffer, len));
  return 0;
}

NS_METHOD
nsWFSecureEnv::CreateJavaCallInfo(Java_CallInfo*      *pCall,
				  nsISecurityContext* ctx)
{
  Java_CallInfo* call = (Java_CallInfo*)malloc(sizeof(Java_CallInfo));
  nsCRT::memset(call, 0, sizeof(Java_CallInfo));
  if (ctx) {
    NS_ADDREF(ctx);
    SecurityContextWrapper* sec = 
      (SecurityContextWrapper*)malloc(sizeof(SecurityContextWrapper));
    sec->info = (void*)ctx;
    sec->Implies = &Implies;
    sec->GetOrigin = &GetOrigin;
    sec->GetCertificateID = &GetCertificateID;
    sec->dead = 0;
    call->security = sec;
  }
  *pCall = call;
  return NS_OK;
} 

NS_METHOD 
nsWFSecureEnv::CleanupJavaCallInfo(Java_CallInfo*      pCall,
				   nsISecurityContext* ctx)
{
  if (!pCall) return NS_OK;
  if (ctx)
    {
      free(pCall->security);
      NS_RELEASE(ctx);
    }
  free(pCall);
  return NS_OK;
}
//nsISecureEnv
NS_IMETHODIMP
nsWFSecureEnv::NewObject(/*[in]*/  jclass clazz, 
			 /*[in]*/  jmethodID methodID,
			 /*[in]*/  jvalue *args, 
			 /*[out]*/ jobject* result,
			 /*[in]*/  nsISecurityContext* ctx)
{
  //OJI_LOG("nsWFSecureEnv::NewObject");
  if (m_env == NULL || result == NULL)
        return NS_ERROR_NULL_POINTER;
  struct Java_CallInfo* call = NULL;
  nsWFSecureEnv::CreateJavaCallInfo(&call, ctx);
  call->type = Java_CreateObject;
  call->data.__createObject.clazz    = clazz;
  call->data.__createObject.methodID = methodID;
  call->data.__createObject.args = args;
  nsresult rv = m_jvm->ExtensionCall(1, PTR_TO_JLONG(call));
  if (!NS_FAILED(rv)) 
    {
      // XXX: do smth meaningful with possible Java exception
      if (call->jException) {
	m_jvm->DeleteLocalRef(call->jException);
	rv = NS_ERROR_FAILURE; 
      } else {
	*result = call->data.__createObject.result;
	rv = NS_OK;
      }
    }
  nsWFSecureEnv::CleanupJavaCallInfo(call, ctx);
  return rv;
}

NS_IMETHODIMP
nsWFSecureEnv::CallMethod(/*[in]*/  jni_type ret_type,
			  /*[in]*/  jobject obj, 
			  /*[in]*/  jmethodID methodID,
			  /*[in]*/  jvalue *args, 
			  /*[out]*/ jvalue* result,
			  /*[in]*/  nsISecurityContext* ctx)
{
  if (result == NULL || obj == NULL || methodID == NULL)
        return NS_ERROR_NULL_POINTER;
  struct Java_CallInfo* call = NULL;
  nsWFSecureEnv::CreateJavaCallInfo(&call, ctx);
  //OJI_LOG5("CallMethod: %x %x %x %x", obj, methodID, args, result);
  call->type = Java_CallMethod;
  call->data.__callMethod.type     = (enum jvmp_jni_type)ret_type; // the same structure, really
  call->data.__callMethod.obj      = obj;
  call->data.__callMethod.methodID = methodID;
  call->data.__callMethod.args     = args;
  nsresult rv = m_jvm->ExtensionCall(1, PTR_TO_JLONG(call));
  if (!NS_FAILED(rv)) 
    {
      // XXX: do smth meaningful with possible Java exception
      if (call->jException) {
	m_jvm->DeleteGlobalRef(call->jException);
	rv = NS_ERROR_FAILURE; 
      } else {
	*result = call->data.__callMethod.result;
	rv = NS_OK;
      }
    }
  nsWFSecureEnv::CleanupJavaCallInfo(call, ctx);
  return rv;
}

NS_IMETHODIMP
nsWFSecureEnv::CallNonvirtualMethod(/*[in]*/  jni_type ret_type,
				    /*[in]*/  jobject obj, 
				    /*[in]*/  jclass clazz,
				    /*[in]*/  jmethodID methodID,
				    /*[in]*/  jvalue *args, 
				    /*[out]*/ jvalue* result,
				    /*[in]*/  nsISecurityContext* ctx)
{
  if (result == NULL || obj == NULL || methodID == NULL)
        return NS_ERROR_NULL_POINTER;
  struct Java_CallInfo* call = NULL;
  nsWFSecureEnv::CreateJavaCallInfo(&call, ctx);
  //OJI_LOG5("CallMethod: %x %x %x %x", obj, methodID, args, result);
  call->type = Java_CallNonvirtualMethod;
  call->data.__callNonvirtualMethod.type     = (enum jvmp_jni_type)ret_type; // the same structure, really
  call->data.__callNonvirtualMethod.obj      = obj;
  call->data.__callNonvirtualMethod.clazz    = clazz;
  call->data.__callNonvirtualMethod.methodID = methodID;
  call->data.__callNonvirtualMethod.args     = args;
  nsresult rv = m_jvm->ExtensionCall(1, PTR_TO_JLONG(call));
  if (!NS_FAILED(rv)) 
    {
      // XXX: do smth meaningful with possible Java exception
      if (call->jException) {
	m_jvm->DeleteLocalRef(call->jException);
	rv = NS_ERROR_FAILURE; 
      } else {
	*result = call->data.__callNonvirtualMethod.result;
	rv = NS_OK;
      }
    }
  nsWFSecureEnv::CleanupJavaCallInfo(call, ctx);
  return rv;
}

NS_IMETHODIMP
nsWFSecureEnv::GetField(/*[in]*/  jni_type field_type,
			/*[in]*/  jobject obj, 
			/*[in]*/  jfieldID fieldID,
			/*[out]*/ jvalue* result,
			/*[in]*/  nsISecurityContext* ctx)
{
  //OJI_LOG("nsWFSecureEnv::GetField");
  if (m_env == NULL || obj == NULL || fieldID == NULL) 
    return NS_ERROR_NULL_POINTER;
  struct Java_CallInfo* call = NULL;
  nsWFSecureEnv::CreateJavaCallInfo(&call, ctx);
  call->type = Java_GetField;
  call->data.__getField.type = (enum jvmp_jni_type)field_type; // the same structure, really
  call->data.__getField.obj     = obj;
  call->data.__getField.fieldID = fieldID;
  nsresult rv = m_jvm->ExtensionCall(1, PTR_TO_JLONG(call));
  if (!NS_FAILED(rv)) 
    {
      // XXX: do smth meaningful with possible Java exception
      if (call->jException) {
	m_jvm->DeleteLocalRef(call->jException);
	 rv = NS_ERROR_FAILURE; 
       } else {
	 *result = call->data.__getField.result;
	 rv = NS_OK;
       }
     }
   nsWFSecureEnv::CleanupJavaCallInfo(call, ctx);
   return rv;
}

NS_IMETHODIMP
nsWFSecureEnv::SetField(/*[in]*/ jni_type field_type,
			/*[in]*/ jobject obj, 
			/*[in]*/ jfieldID fieldID,
			/*[in]*/ jvalue val,
			/*[in]*/ nsISecurityContext* ctx)
{
  //OJI_LOG("nsWFSecureEnv::SetField");
   if (m_env == NULL || obj == NULL || fieldID == NULL) 
     return NS_ERROR_NULL_POINTER;
   struct Java_CallInfo* call = NULL;
   nsWFSecureEnv::CreateJavaCallInfo(&call, ctx);
   call->type = Java_SetField;
   call->data.__setField.type = (enum jvmp_jni_type)field_type; // the same structure, really
   call->data.__setField.obj     = obj;
   call->data.__setField.fieldID = fieldID;
   call->data.__setField.value   = val;
   nsresult rv = m_jvm->ExtensionCall(1, PTR_TO_JLONG(call));
   if (!NS_FAILED(rv)) 
     {
       // XXX: do smth meaningful with possible Java exception
       if (call->jException) {
	 m_jvm->DeleteLocalRef(call->jException);
	 rv = NS_ERROR_FAILURE; 
       } else
	 rv = NS_OK;
     }
   nsWFSecureEnv::CleanupJavaCallInfo(call, ctx);
   return rv;
}

NS_IMETHODIMP
nsWFSecureEnv::CallStaticMethod(/*[in]*/  jni_type ret_type,
                                /*[in]*/  jclass clazz,
                                /*[in]*/  jmethodID methodID,
                                /*[in]*/  jvalue *args, 
                                /*[out]*/ jvalue* result,
                                /*[in]*/  nsISecurityContext* ctx)
{
  if (result == NULL  || clazz == NULL || methodID == NULL)
    return NS_ERROR_NULL_POINTER;
  if (methodID ==  g_identityHashCodeMID)
    {
      //OJI_LOG("using cached identityHashCode");
      result->i = m_env->CallStaticIntMethodA(g_javaLangSystem, 
					      g_identityHashCodeMID, 
					      args);  
      return NS_OK;
    }
  struct Java_CallInfo* call = NULL;
  nsWFSecureEnv::CreateJavaCallInfo(&call, ctx);
  call->type = Java_CallStaticMethod;
  call->data.__callStaticMethod.type     = (enum jvmp_jni_type)ret_type; // the same structure, really
  call->data.__callStaticMethod.clazz    = clazz;
  call->data.__callStaticMethod.methodID = methodID;
  call->data.__callStaticMethod.args     = args;
  nsresult rv = m_jvm->ExtensionCall(1, PTR_TO_JLONG(call));
  if (!NS_FAILED(rv)) 
    {
      // XXX: do smth meaningful with possible Java exception
      if (call->jException) {
	m_jvm->DeleteLocalRef(call->jException);
	rv = NS_ERROR_FAILURE; 
      } else
	*result = call->data.__callStaticMethod.result;
    }
  nsWFSecureEnv::CleanupJavaCallInfo(call, ctx);
  return rv;
}

NS_IMETHODIMP
nsWFSecureEnv::GetStaticField(/*[in]*/  jni_type field_type,
			      /*[in]*/  jclass clazz, 
			      /*[in]*/  jfieldID fieldID, 
			      /*[out]*/ jvalue* result,
			      /*[in]*/  nsISecurityContext* ctx)
{
  //OJI_LOG4("nsWFSecureEnv::GetStaticField: type=%d class=%x fid=%x", 
  //   field_type, clazz, fieldID);
  if (m_env == NULL || clazz == NULL || fieldID == NULL) 
    return NS_ERROR_NULL_POINTER;
  struct Java_CallInfo* call = NULL;
  nsWFSecureEnv::CreateJavaCallInfo(&call, ctx);
  call->type = Java_GetStaticField;
  call->data.__getStaticField.type = (enum jvmp_jni_type)field_type; // the same structure, really
  call->data.__getStaticField.clazz     = clazz;
  call->data.__getStaticField.fieldID   = fieldID;
  nsresult rv = m_jvm->ExtensionCall(1, PTR_TO_JLONG(call));
  if (!NS_FAILED(rv)) 
    {
      // XXX: do smth meaningful with possible Java exception
      if (call->jException) {
	m_jvm->DeleteLocalRef(call->jException);
	 rv = NS_ERROR_FAILURE; 
       } else {
	 *result = call->data.__getStaticField.result;
	 rv = NS_OK;
       }
     }
   nsWFSecureEnv::CleanupJavaCallInfo(call, ctx);
   return rv;
}

NS_IMETHODIMP
nsWFSecureEnv::SetStaticField(/*[in]*/ jni_type field_type,
			      /*[in]*/ jclass clazz, 
			      /*[in]*/ jfieldID fieldID,
			      /*[in]*/ jvalue val,
			      /*[in]*/ nsISecurityContext* ctx)
{
  //OJI_LOG("nsWFSecureEnv::SetStaticField");
  if (m_env == NULL || clazz == NULL || fieldID == NULL) 
     return NS_ERROR_NULL_POINTER;
   struct Java_CallInfo* call = NULL;
   nsWFSecureEnv::CreateJavaCallInfo(&call, ctx);
   call->type = Java_SetStaticField;
   call->data.__setStaticField.type = (enum jvmp_jni_type)field_type; // the same structure, really
   call->data.__setStaticField.clazz     = clazz;
   call->data.__setStaticField.fieldID   = fieldID;
   call->data.__setStaticField.value     = val;
   nsresult rv = m_jvm->ExtensionCall(1, PTR_TO_JLONG(call));
   if (!NS_FAILED(rv)) 
     {
       // XXX: do smth meaningful with possible Java exception
       if (call->jException) {
	 m_jvm->DeleteLocalRef(call->jException);
	 rv = NS_ERROR_FAILURE; 
       } else
	 rv = NS_OK;
     }
   nsWFSecureEnv::CleanupJavaCallInfo(call, ctx);
   return rv;
}   

NS_IMETHODIMP nsWFSecureEnv::GetVersion(/*[out]*/ jint* version) 
{
    if (m_env == NULL || version == NULL)
        return NS_ERROR_NULL_POINTER;
    
    *version = m_env->GetVersion();

    return NS_OK;
}

NS_IMETHODIMP nsWFSecureEnv::DefineClass(/*[in]*/  const char* name,
                                       /*[in]*/  jobject loader,
                                       /*[in]*/  const jbyte *buf,
                                       /*[in]*/  jsize len,
                                       /*[out]*/ jclass* clazz) 
{
  //OJI_LOG("nsWFSecureEnv::DefineClass");
  
    if (m_env == NULL || clazz == NULL)
        return NS_ERROR_NULL_POINTER;
    
    *clazz = m_env->DefineClass(name, loader, buf, len);

    return NS_OK;
}
                                       

NS_IMETHODIMP nsWFSecureEnv::FindClass(/*[in]*/  const char* name, 
                                     /*[out]*/ jclass* clazz) 
{
   
    if (m_env == NULL || clazz == NULL)
        return NS_ERROR_NULL_POINTER;
    
    *clazz = m_env->FindClass(name);
    //OJI_LOG3("nsWFSecureEnv::FindClass for %s is %x", name, *clazz);  
    return NS_OK;
}


NS_IMETHODIMP nsWFSecureEnv::GetSuperclass(/*[in]*/  jclass sub,
                                         /*[out]*/ jclass* super) 
{
    if (m_env == NULL || super == NULL)
        return NS_ERROR_NULL_POINTER;
    
    *super = m_env->GetSuperclass(sub);
    //OJI_LOG3("nsWFSecureEnv::GetSuperclass: %x->%x", sub, *super);
  
    return NS_OK;
}


NS_IMETHODIMP nsWFSecureEnv::IsAssignableFrom(/*[in]*/  jclass sub,
					      /*[in]*/  jclass super,
					      /*[out]*/ jboolean* result) 
{
    if (m_env == NULL || result == NULL)
        return NS_ERROR_NULL_POINTER;
    
    *result = m_env->IsAssignableFrom(sub, super);

    return NS_OK;
}


NS_IMETHODIMP nsWFSecureEnv::Throw(/*[in]*/  jthrowable obj,
                                 /*[out]*/ jint* result) 
{
    if (m_env == NULL || result == NULL)
        return NS_ERROR_NULL_POINTER;
    
    //OJI_LOG("nsWFSecureEnv::Throw");
    *result = m_env->Throw(obj);

    return NS_OK;
}


NS_IMETHODIMP nsWFSecureEnv::ThrowNew(/*[in]*/  jclass clazz,
                                    /*[in]*/  const char *msg,
                                    /*[out]*/ jint* result) 
{
    if (m_env == NULL || result == NULL)
        return NS_ERROR_NULL_POINTER;
    
    *result = m_env->ThrowNew(clazz, msg);

    return NS_OK;
}


NS_IMETHODIMP nsWFSecureEnv::ExceptionOccurred(/*[out]*/ jthrowable* result)
{
    if (m_env == NULL || result == NULL)
        return NS_ERROR_NULL_POINTER;
    
    *result = m_env->ExceptionOccurred();

    return NS_OK;
}


NS_IMETHODIMP nsWFSecureEnv::ExceptionDescribe(void)
{
    if (m_env == NULL)
        return NS_ERROR_NULL_POINTER;
    
    m_env->ExceptionDescribe();

    return NS_OK;
}


NS_IMETHODIMP nsWFSecureEnv::ExceptionClear(void)
{
    if (m_env == NULL)
        return NS_ERROR_NULL_POINTER;
    
    m_env->ExceptionClear();

    return NS_OK;
}


NS_IMETHODIMP nsWFSecureEnv::FatalError(/*[in]*/ const char* msg)
{
    if (m_env == NULL)
        return NS_ERROR_NULL_POINTER;
    
    OJI_LOG2("Fatal error: %s", msg);
    m_env->FatalError(msg);

    return NS_OK;
}


NS_IMETHODIMP nsWFSecureEnv::NewGlobalRef(/*[in]*/  jobject lobj, 
                                        /*[out]*/ jobject* result) 
{
    if (m_env == NULL || result == NULL)
        return NS_ERROR_NULL_POINTER;
    
    *result = m_env->NewGlobalRef(lobj);

    return NS_OK;
}


NS_IMETHODIMP nsWFSecureEnv::DeleteGlobalRef(/*[in]*/ jobject gref) 
{
    if (m_env == NULL)
        return NS_ERROR_NULL_POINTER;
    
    m_env->DeleteGlobalRef(gref);

    return NS_OK;
}


NS_IMETHODIMP nsWFSecureEnv::DeleteLocalRef(/*[in]*/ jobject obj)
{
    if (m_env == NULL)
        return NS_ERROR_NULL_POINTER;
    
    m_env->DeleteLocalRef(obj);

    return NS_OK;
}


NS_IMETHODIMP nsWFSecureEnv::IsSameObject(/*[in]*/  jobject obj1,
					  /*[in]*/  jobject obj2,
					  /*[out]*/ jboolean* result) 
{
    if (m_env == NULL || result == NULL)
        return NS_ERROR_NULL_POINTER;
    
    *result = m_env->IsSameObject(obj1, obj2);

    return NS_OK;
}


NS_IMETHODIMP nsWFSecureEnv::AllocObject(/*[in]*/  jclass clazz,
					 /*[out]*/ jobject* result) 
{
    if (m_env == NULL || result == NULL)
        return NS_ERROR_NULL_POINTER;
    
    *result = m_env->AllocObject(clazz);

    return NS_OK;
}


NS_IMETHODIMP nsWFSecureEnv::GetObjectClass(/*[in]*/  jobject obj,
					    /*[out]*/ jclass* result) 
{
    if (m_env == NULL || result == NULL)
        return NS_ERROR_NULL_POINTER;
    
    *result = m_env->GetObjectClass(obj);
    //OJI_LOG("nsWFSecureEnv::GetObjectClass");
    return NS_OK;
}


NS_IMETHODIMP nsWFSecureEnv::IsInstanceOf(/*[in]*/  jobject obj,
					  /*[in]*/  jclass clazz,
					  /*[out]*/ jboolean* result) 
{
    if (m_env == NULL || result == NULL)
        return NS_ERROR_NULL_POINTER;
    
    *result = m_env->IsInstanceOf(obj, clazz);

    return NS_OK;
}


NS_IMETHODIMP nsWFSecureEnv::GetMethodID(/*[in]*/  jclass clazz, 
					 /*[in]*/  const char* name,
					 /*[in]*/  const char* sig,
					 /*[out]*/ jmethodID* result) 
{
    if (m_env == NULL || result == NULL)
        return NS_ERROR_NULL_POINTER;
    
    *result = m_env->GetMethodID(clazz, name, sig);
    //OJI_LOG5("nsWFSecureEnv::GetMethodID: class=%x method=%s sig=%s r=%x",  clazz, name, sig, *result);
    return NS_OK;
}


NS_IMETHODIMP nsWFSecureEnv::GetFieldID(/*[in]*/  jclass clazz, 
					/*[in]*/  const char* name,
					/*[in]*/  const char* sig,
					/*[out]*/ jfieldID* result) 
{
    if (m_env == NULL || result == NULL)
        return NS_ERROR_NULL_POINTER;
    
    *result = m_env->GetFieldID(clazz, name, sig);
    //OJI_LOG5("nsWFSecureEnv::GetFieldID: class=%x method=%s sig=%s r=%x",
    //clazz, name, sig, *result);
    return NS_OK;
}


NS_IMETHODIMP nsWFSecureEnv::GetStaticMethodID(/*[in]*/  jclass clazz, 
                                             /*[in]*/  const char* name,
                                             /*[in]*/  const char* sig,
                                             /*[out]*/ jmethodID* result)
{
    if (m_env == NULL || result == NULL)
        return NS_ERROR_NULL_POINTER;
    
    *result = m_env->GetStaticMethodID(clazz, name, sig);
    //OJI_LOG5("nsWFSecureEnv::GetStaticMethodID: class=%x method=%s sig=%s r=%x",
    //clazz, name, sig, *result);
    return NS_OK;
}


NS_IMETHODIMP nsWFSecureEnv::GetStaticFieldID(/*[in]*/  jclass clazz, 
                                            /*[in]*/  const char* name,
                                            /*[in]*/  const char* sig,
                                            /*[out]*/ jfieldID* result) 
{
    if (m_env == NULL || result == NULL)
        return NS_ERROR_NULL_POINTER;
        
    *result = m_env->GetStaticFieldID(clazz, name, sig);
    //OJI_LOG5("nsWFSecureEnv::GetStaticFieldID: clazz=%x name=%s sig=%s r=%x",
    //clazz, name, sig, *result);
    return NS_OK;
}


NS_IMETHODIMP nsWFSecureEnv::NewString(/*[in]*/  const jchar* unicode,
                                     /*[in]*/  jsize len,
                                     /*[out]*/ jstring* result) 
{
    if (m_env == NULL || result == NULL)
        return NS_ERROR_NULL_POINTER;
    
    *result = m_env->NewString(unicode, len);

    return NS_OK;
}


NS_IMETHODIMP nsWFSecureEnv::GetStringLength(/*[in]*/  jstring str,
                                           /*[out]*/ jsize* result) 
{
    if (m_env == NULL || result == NULL)
        return NS_ERROR_NULL_POINTER;
    
    *result = m_env->GetStringLength(str);

    return NS_OK;
}
    

NS_IMETHODIMP nsWFSecureEnv::GetStringChars(/*[in]*/  jstring str,
                                          /*[in]*/  jboolean *isCopy,
                                          /*[out]*/ const jchar** result) 
{
    if (m_env == NULL || result == NULL)
        return NS_ERROR_NULL_POINTER;
    
    *result = m_env->GetStringChars(str, isCopy);

    return NS_OK;
}


NS_IMETHODIMP nsWFSecureEnv::ReleaseStringChars(/*[in]*/  jstring str,
                                              /*[in]*/  const jchar *chars) 
{
    if (m_env == NULL)
        return NS_ERROR_NULL_POINTER;
    
    m_env->ReleaseStringChars(str, chars);

    return NS_OK;
}


NS_IMETHODIMP nsWFSecureEnv::NewStringUTF(/*[in]*/  const char *utf,
                                        /*[out]*/ jstring* result) 
{
    if (m_env == NULL || result == NULL)
        return NS_ERROR_NULL_POINTER;
    
    *result = m_env->NewStringUTF(utf);

    return NS_OK;
}


NS_IMETHODIMP nsWFSecureEnv::GetStringUTFLength(/*[in]*/  jstring str,
                                              /*[out]*/ jsize* result) 
{
    if (m_env == NULL || result == NULL)
        return NS_ERROR_NULL_POINTER;
    
    *result = m_env->GetStringUTFLength(str);

    return NS_OK;
}

    
NS_IMETHODIMP nsWFSecureEnv::GetStringUTFChars(/*[in]*/  jstring str,
                                             /*[in]*/  jboolean *isCopy,
                                             /*[out]*/ const char** result) 
{
    if (m_env == NULL || result == NULL)
        return NS_ERROR_NULL_POINTER;
    
    *result = m_env->GetStringUTFChars(str, isCopy);

    return NS_OK;
}


NS_IMETHODIMP nsWFSecureEnv::ReleaseStringUTFChars(/*[in]*/  jstring str,
                                                 /*[in]*/  const char *chars) 
{
    if (m_env == NULL)
        return NS_ERROR_NULL_POINTER;
    
    m_env->ReleaseStringUTFChars(str, chars);

    return NS_OK;
}


NS_IMETHODIMP nsWFSecureEnv::GetArrayLength(/*[in]*/  jarray array,
                                          /*[out]*/ jsize* result) 
{
    if (m_env == NULL || result == NULL)
        return NS_ERROR_NULL_POINTER;
    
    *result = m_env->GetArrayLength(array);

    return NS_OK;
}


NS_IMETHODIMP nsWFSecureEnv::GetObjectArrayElement(/*[in]*/  jobjectArray array,
                                                 /*[in]*/  jsize index,
                                                 /*[out]*/ jobject* result)
{
    if (m_env == NULL || result == NULL)
        return NS_ERROR_NULL_POINTER;

    *result = m_env->GetObjectArrayElement(array, index);

    return NS_OK;
}


NS_IMETHODIMP nsWFSecureEnv::SetObjectArrayElement(/*[in]*/  jobjectArray array,
                                                 /*[in]*/  jsize index,
                                                 /*[in]*/  jobject val) 
{
    if (m_env == NULL)
        return NS_ERROR_NULL_POINTER;

    m_env->SetObjectArrayElement(array, index, val);

    return NS_OK;
}



NS_IMETHODIMP nsWFSecureEnv::NewObjectArray(/*[in]*/  jsize len,
    					                  /*[in]*/  jclass clazz,
                                          /*[in]*/  jobject init,
                                          /*[out]*/ jobjectArray* result)
{
    if (m_env == NULL || result == NULL)
        return NS_ERROR_NULL_POINTER;

    *result = m_env->NewObjectArray(len, clazz, init);
            
    return NS_OK;
}


NS_IMETHODIMP nsWFSecureEnv::NewArray(/*[in]*/  jni_type element_type,
				      /*[in]*/  jsize len,
				      /*[out]*/ jarray* result) 
{
    if (m_env == NULL || result == NULL)
        return NS_ERROR_NULL_POINTER;

    switch (element_type)
    {
        case jboolean_type: {
            *result = m_env->NewBooleanArray(len);
            break;
        }
    
        case jbyte_type:    {
            *result = m_env->NewByteArray(len);
            break;
        }

        case jchar_type:    {
            *result = m_env->NewCharArray(len);
            break;
        }

        case jshort_type:   {
            *result = m_env->NewShortArray(len);
            break;
        }

        case jint_type:     {
            *result = m_env->NewIntArray(len);
            break;
        }

        case jlong_type:{
            *result = m_env->NewLongArray(len);
            break;
        }

        case jfloat_type:{
            *result = m_env->NewFloatArray(len);
            break;
        }

        case jdouble_type:{
            *result = m_env->NewDoubleArray(len);
            break;
        }

        default:
            return NS_ERROR_FAILURE;
    }

    return NS_OK;
}


NS_IMETHODIMP nsWFSecureEnv::GetArrayElements(/*[in]*/  jni_type element_type,
                                            /*[in]*/  jarray array,
                                            /*[in]*/  jboolean *isCopy,
                                            /*[out]*/ void* result)
{
    if (m_env == NULL || result == NULL)
        return NS_ERROR_NULL_POINTER;

    switch (element_type)
    {
        case jboolean_type: {
            jboolean** ret = (jboolean**) result;
            *ret = m_env->GetBooleanArrayElements((jbooleanArray)array, isCopy);
            break;
        }
    
        case jbyte_type:    {
            jbyte** ret = (jbyte**) result;
            *ret = m_env->GetByteArrayElements((jbyteArray)array, isCopy);
            break;
        }

        case jchar_type:    {
            jchar** ret = (jchar**) result;
            *ret = m_env->GetCharArrayElements((jcharArray)array, isCopy);
            break;
        }

        case jshort_type:   {
            jshort** ret = (jshort**) result;
            *ret = m_env->GetShortArrayElements((jshortArray)array, isCopy);
            break;
        }

        case jint_type:     {
            jint** ret = (jint**) result;
            *ret = m_env->GetIntArrayElements((jintArray)array, isCopy);
            break;
        }

        case jlong_type:{
            jlong** ret = (jlong**) result;
            *ret = m_env->GetLongArrayElements((jlongArray)array, isCopy);
            break;
        }

        case jfloat_type:{
            jfloat** ret = (jfloat**) result;
            *ret = m_env->GetFloatArrayElements((jfloatArray)array, isCopy);
            break;
        }

        case jdouble_type:{
            jdouble** ret = (jdouble**) result;
            *ret = m_env->GetDoubleArrayElements((jdoubleArray)array, isCopy);
            break;
        }

        default:
            return NS_ERROR_FAILURE;
    }

    return NS_OK;
}


NS_IMETHODIMP nsWFSecureEnv::ReleaseArrayElements(/*[in]*/ jni_type element_type,
                                                /*[in]*/ jarray array,
                                                /*[in]*/ void *elems,
                                                /*[in]*/ jint mode) 
{
    if (m_env == NULL)
        return NS_ERROR_NULL_POINTER;

    switch (element_type)
    {
        case jboolean_type: {
            m_env->ReleaseBooleanArrayElements((jbooleanArray)array, (jboolean*)elems, mode);
            break;
        }
    
        case jbyte_type:    {
            m_env->ReleaseByteArrayElements((jbyteArray)array, (jbyte*)elems, mode);
            break;
        }

        case jchar_type:    {
            m_env->ReleaseCharArrayElements((jcharArray)array, (jchar*)elems, mode);
            break;
        }

        case jshort_type:   {
            m_env->ReleaseShortArrayElements((jshortArray)array, (jshort*)elems, mode);
            break;
        }

        case jint_type:     {
            m_env->ReleaseIntArrayElements((jintArray)array, (jint*)elems, mode);
            break;
        }

        case jlong_type:{
            m_env->ReleaseLongArrayElements((jlongArray)array, (jlong*)elems, mode);
            break;
        }

        case jfloat_type:{
            m_env->ReleaseFloatArrayElements((jfloatArray)array, (jfloat*)elems, mode);
            break;
        }

        case jdouble_type:{
            m_env->ReleaseDoubleArrayElements((jdoubleArray)array, (jdouble*)elems, mode);
            break;
        }

        default:
            return NS_ERROR_FAILURE;
    }

    return NS_OK;
}

NS_IMETHODIMP nsWFSecureEnv::GetArrayRegion(/*[in]*/  jni_type element_type,
                                          /*[in]*/  jarray array,
                                          /*[in]*/  jsize start,
                                          /*[in]*/  jsize len,
                                          /*[out]*/ void* buf)
{
    if (m_env == NULL || buf == NULL)
        return NS_ERROR_NULL_POINTER;

    switch (element_type)
    {
        case jboolean_type: {
            m_env->GetBooleanArrayRegion((jbooleanArray)array, start, len, (jboolean*)buf);
            break;
        }
    
        case jbyte_type:    {
            m_env->GetByteArrayRegion((jbyteArray)array, start, len, (jbyte*)buf);
            break;
        }

        case jchar_type:    {
            m_env->GetCharArrayRegion((jcharArray)array, start, len, (jchar*)buf);
            break;
        }

        case jshort_type:   {
            m_env->GetShortArrayRegion((jshortArray)array, start, len, (jshort*)buf);
            break;
        }

        case jint_type:     {
            m_env->GetIntArrayRegion((jintArray)array, start, len, (jint*)buf);
            break;
        }

        case jlong_type:{
            m_env->GetLongArrayRegion((jlongArray)array, start, len, (jlong*)buf);
            break;
        }

        case jfloat_type:{
            m_env->GetFloatArrayRegion((jfloatArray)array, start, len, (jfloat*)buf);
            break;
        }

        case jdouble_type:{
            m_env->GetDoubleArrayRegion((jdoubleArray)array, start, len, (jdouble*)buf);
            break;
        }

        default:
            return NS_ERROR_FAILURE;
    }

    return NS_OK;
}


NS_IMETHODIMP nsWFSecureEnv::SetArrayRegion(/*[in]*/  jni_type element_type,
                                          /*[in]*/  jarray array,
                                          /*[in]*/  jsize start,
                                          /*[in]*/  jsize len,
                                          /*[in]*/  void* buf) 
{
    if (m_env == NULL || buf == NULL)
        return NS_ERROR_NULL_POINTER;

    switch (element_type)
    {
        case jboolean_type: {
            m_env->SetBooleanArrayRegion((jbooleanArray)array, start, len, (jboolean*)buf);
            break;
        }
    
        case jbyte_type:    {
            m_env->SetByteArrayRegion((jbyteArray)array, start, len, (jbyte*)buf);
            break;
        }

        case jchar_type:    {
            m_env->SetCharArrayRegion((jcharArray)array, start, len, (jchar*)buf);
            break;
        }

        case jshort_type:   {
            m_env->SetShortArrayRegion((jshortArray)array, start, len, (jshort*)buf);
            break;
        }

        case jint_type:     {
            m_env->SetIntArrayRegion((jintArray)array, start, len, (jint*)buf);
            break;
        }

        case jlong_type:{
            m_env->SetLongArrayRegion((jlongArray)array, start, len, (jlong*)buf);
            break;
        }

        case jfloat_type:{
            m_env->SetFloatArrayRegion((jfloatArray)array, start, len, (jfloat*)buf);
            break;
        }

        case jdouble_type:{
            m_env->SetDoubleArrayRegion((jdoubleArray)array, start, len, (jdouble*)buf);
            break;
        }

        default:
            return NS_ERROR_FAILURE;
    }

    return NS_OK;
}


NS_IMETHODIMP nsWFSecureEnv::RegisterNatives(/*[in]*/  jclass clazz,
                                           /*[in]*/  const JNINativeMethod *methods,
                                           /*[in]*/  jint nMethods,
                                           /*[out]*/ jint* result) 
{
    if (m_env == NULL || result == NULL)
        return NS_ERROR_NULL_POINTER;

    *result = m_env->RegisterNatives(clazz, methods, nMethods);

    return NS_OK;
}


NS_IMETHODIMP nsWFSecureEnv::UnregisterNatives(/*[in]*/  jclass clazz,
                                             /*[out]*/ jint* result) 
{
    if (m_env == NULL || result == NULL)
        return NS_ERROR_NULL_POINTER;

    *result = m_env->UnregisterNatives(clazz);

    return NS_OK;
}


NS_IMETHODIMP nsWFSecureEnv::MonitorEnter(/*[in]*/  jobject obj,
					  /*[out]*/ jint* result) 
{
    if (m_env == NULL || result == NULL)
        return NS_ERROR_NULL_POINTER;

    *result = m_env->MonitorEnter(obj);

    return NS_OK;
}


NS_IMETHODIMP nsWFSecureEnv::MonitorExit(/*[in]*/  jobject obj,
					 /*[out]*/ jint* result)
{
    if (m_env == NULL || result == NULL)
        return NS_ERROR_NULL_POINTER;

    *result = m_env->MonitorExit(obj);

    return NS_OK;
}

NS_IMETHODIMP nsWFSecureEnv::GetJavaVM(/*[in]*/  JavaVM **vm,
				       /*[out]*/ jint* result)
{
    if (m_env == NULL || result == NULL)
        return NS_ERROR_NULL_POINTER;

    *result = m_env->GetJavaVM(vm);

    return NS_OK;
}

