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
 * $Id: nsPluggableJVM.cpp,v 1.2 2001/07/12 20:32:09 edburns%acm.org Exp $
 *
 * 
 * Contributor(s): 
 *
 *   Nikolay N. Igotti <inn@sparc.spb.su>
 */

#include "nsoji2.h"
#include "nsPluggableJVM.h"
#include "nsIJavaHTMLObject.h"
#include "wf_moz6_common.h"
// just as I'm lazy to Create/Cleanup Java calls
#include "nsWFSecureEnv.h"

NS_IMPL_ISUPPORTS1(nsPluggableJVM, nsIPluggableJVM)

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIPluggableJVMIID, NS_IPLUGGABLEJVM_IID);

nsPluggableJVM::nsPluggableJVM()
{
  NS_INIT_ISUPPORTS();
  m_extID = 0;
  m_dll = nsnull;
}

nsPluggableJVM::~nsPluggableJVM()
{
  OJI_LOG("nsPluggableJVM::~nsPluggableJVM");
  if (m_jvmp_context)
    {
        if (m_ctx != nsnull && m_extID != 0) 
            (m_jvmp_context->JVMP_UnregisterExtension)(m_ctx, m_extID);
    }
}

NS_IMETHODIMP 
nsPluggableJVM::RegisterWindow(nsPluginWindow* rwin, 
			       jint *ID)
{
  JVMP_DrawingSurfaceInfo w;

  OJI_LOG("nsPluggableJVM::RegisterWindow");
  w.window =  (JPluginWindow *)rwin->window;
  w.x = rwin->x;
  w.y = rwin->y;
  w.width = rwin->width;
  w.height = rwin->height;
  if ((m_jvmp_context->JVMP_RegisterWindow)(m_ctx, &w, ID) != JNI_TRUE) 
    {
      OJI_LOG("can\'t register window");
      return NS_ERROR_FAILURE;
    };
  OJI_LOG2("Registed our native window with ID %ld", *ID);
  return NS_OK;
}

/* void UnregisterWindow (in jp_jint ID); */
NS_IMETHODIMP 
nsPluggableJVM::UnregisterWindow(jint ID)
{
  if ((m_jvmp_context->JVMP_UnregisterWindow)(m_ctx, ID) != JNI_TRUE) 
    {
      OJI_LOG("can\'t unregister window");
      return NS_ERROR_FAILURE;
    };
   return NS_OK;
}

NS_IMETHODIMP 
nsPluggableJVM::RegisterMonitor(PRMonitor* rmon, jint *pID)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void UnregisterMonitor (in jp_jint ID); */
NS_IMETHODIMP 
nsPluggableJVM::UnregisterMonitor(jint ID)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
nsPluggableJVM::GetJavaWrapper(jint obj, jobject *wrapper)
{
  struct Java_CallInfo* call;
  nsresult rv = NS_OK;
  *wrapper = NULL;
  if (!m_extID) return NS_ERROR_FAILURE;
  nsWFSecureEnv::CreateJavaCallInfo(&call, NULL);
  call->type = Java_WrapJSObject;
  call->jException = NULL;
  call->data.__wrapJSObject.jsObject = obj;
  call->data.__wrapJSObject.jstid = (jint)PR_GetCurrentThread();
  
  rv = ExtensionCall(1, PTR_TO_JLONG(call));
  if (!NS_FAILED(rv)) 
    {
      // XXX: do smth meaningful with possible Java exception
      if (call->jException) {
	DeleteLocalRef(call->jException);
	rv = NS_ERROR_FAILURE; 
      } else {
	// global ref already created
	*wrapper = call->data.__wrapJSObject.jObject; 
      }
    }
  nsWFSecureEnv::CleanupJavaCallInfo(call, NULL);
  return rv;
}

NS_IMETHODIMP 
nsPluggableJVM::UnwrapJavaWrapper(jobject jobj, jint *jsobj)
{
  struct Java_CallInfo* call;
  nsresult rv = NS_OK;
  if (jsobj == NULL || !m_extID) return NS_ERROR_FAILURE;
  if (jobj == NULL) 
    { 
      *jsobj = 0;
      return NS_OK;
    }
  *jsobj = 0;
  nsWFSecureEnv::CreateJavaCallInfo(&call, NULL);
  call->type = Java_UnwrapJObject;
  call->jException = NULL;
  call->data.__unwrapJObject.jObject = jobj;
  call->data.__unwrapJObject.jstid = (jint)PR_GetCurrentThread();
  
  rv = ExtensionCall(1, PTR_TO_JLONG(call));
  if (!NS_FAILED(rv)) 
    {
      // XXX: do smth meaningful with possible Java exception
      if (call->jException) {
	DeleteLocalRef(call->jException);
	rv = NS_ERROR_FAILURE; 
      } else {
	*jsobj = call->data.__unwrapJObject.jsObject; 
      }
    }
  nsWFSecureEnv::CleanupJavaCallInfo(call, NULL);
  return rv;
}


NS_IMETHODIMP 
nsPluggableJVM::GetJNIEnv(JNIEnv* *env)
{
  JVMP_CallingContext* ctx;
  int res;

  if (!m_jvmp_context) return NS_ERROR_FAILURE;
  res = (m_jvmp_context->JVMP_GetCallingContext)(&ctx);
  if (res != JNI_TRUE) return NS_ERROR_FAILURE;  
  *env = ctx->env;
  return NS_OK;;
}


static int newPrincipalsFromStrings(jbyte** *resprin, 
                                    jint* *reslen, 
                                    jint num, ...)
{
    jint*  prins_len; 
    jbyte* *prins;
    char* str;
    jint i, len;
    va_list ap;

    prins_len = (jint*)malloc(num*sizeof(jint));
    if (!prins_len) return 0;
    prins = (jbyte**)malloc(num*sizeof(jbyte**));
    if (!prins) return 0;
    va_start(ap, num);
    for (i=0; i<num; i++) 
	{
	    str =  va_arg(ap, char*);
	    len = str ? strlen(str) : 0;
	    prins[i] = (jbyte*)malloc(len);
	    if (!prins[i]) return 0;
	    prins_len[i] = len;
	    /* not copy last 0 */
	    memcpy(prins[i], str, len);
	}
    va_end(ap);
    *resprin = prins;
    *reslen = prins_len;
    return 1;
}
static int deletePrincipals(jbyte** prins, jint* plen, jint len)
{
    int i;
    for (i=0; i<len; i++) free(prins[i]);
    free(prins);
    free(plen);
    return 1;
}



NS_IMETHODIMP 
nsPluggableJVM::StartupJVM(const char *classPath, 
			   nsPluggableJVMStatus *status,
			   jlong sup)
{
  JavaVMInitArgs vm_args;
  jint res;
  OJI_LOG("nsPluggableJVM::StartupJVM");
  *status = nsPluggableJVMStatus_Failed;
  if (sup == nsnull) return NS_ERROR_FAILURE;
  const char* plugin_home = getenv("WFHOME");
  if (plugin_home == NULL) {
    OJI_LOG("Env variable JAVA_PLUGIN_HOME not set");
    return NS_ERROR_FAILURE;
  }
  if (NS_FAILED(loadPluginDLL(plugin_home))) 
    return NS_ERROR_FAILURE;
  memset(&vm_args, 0, sizeof(vm_args));
  vm_args.version = JNI_VERSION_1_2;
  // XXX: add classpath here
  if (NS_FAILED(initJVM(&m_ctx, &vm_args))) 
    return NS_ERROR_FAILURE;
  // initialize capability mechanism 
  JVMP_SecurityCap cap;
  JVMP_FORBID_ALL_CAPS(cap);
  JVMP_ALLOW_CAP(cap, JVMP_CAP_SYS_SYSTEM);
  JVMP_ALLOW_CAP(cap, JVMP_CAP_SYS_SYSEVENT)
  JVMP_ALLOW_CAP(cap, JVMP_CAP_SYS_EVENT);
  JVMP_ALLOW_CAP(cap, JVMP_CAP_SYS_CALL);
  jbyte** prins;
  jint    pnum = 2;
  jint*   plen;
  if (!newPrincipalsFromStrings(&prins, &plen, 
				pnum, "CAPS", "MOZILLA"))
    return JNI_FALSE;
  if ((m_jvmp_context->JVMP_EnableCapabilities)
      (m_ctx, &cap, pnum, plen, prins) != JNI_TRUE) 
    {
      OJI_LOG("Can\'t enable caps");
      return JNI_FALSE;
    };
  deletePrincipals(prins, plen, pnum);
  SetConsoleVisibility(PR_TRUE);
  char* ext_dll = PR_GetLibraryName(plugin_home, LIBEXT);
  OJI_LOG2("JVMP_RegisterExtension: %s", ext_dll);
  res = (m_jvmp_context->JVMP_RegisterExtension)(m_ctx, 
						 ext_dll,
						 &m_extID,
						 sup);
  PR_Free(ext_dll);
  if (res != JNI_TRUE || m_extID == 0) return NS_ERROR_FAILURE;  
  OJI_LOG("JVMP_RegisterExtension OK");
  *status = nsPluggableJVMStatus_Running;
  return NS_OK;
}

/* void ShutdownJVM (in PRBool fullShutdown, out nsJVMStatus status); */
NS_IMETHODIMP 
nsPluggableJVM::ShutdownJVM(PRBool fullShutdown, nsPluggableJVMStatus *status)
{
  if ((m_jvmp_context->JVMP_UnregisterExtension(m_ctx, m_extID) != JNI_TRUE))
    return NS_ERROR_FAILURE;
  m_extID = 0;
  if ((m_jvmp_context->JVMP_StopJVM(m_ctx) != JNI_TRUE))
    return NS_ERROR_FAILURE;
  m_jvmp_context = NULL;
  return NS_OK;
}

nsresult
nsPluggableJVM::loadPluginDLL(const char* plugin_home)
{
  char* plugin_dll = PR_GetLibraryName(plugin_home, LIBJVMP);
  JVMP_GetPlugin_t fJVMP_GetPlugin;
  char errorMsg[1024] = "<unknown; can't get error from NSPR>";

  OJI_LOG2("Loading JVMP from %s", plugin_dll);
  m_dll = PR_LoadLibrary(plugin_dll);
  PR_Free(plugin_dll);
  if (!m_dll) 
    {
      if (PR_GetErrorTextLength() < (int) sizeof(errorMsg))
	PR_GetErrorText(errorMsg);
      OJI_LOG2("Cannot load plugin DLL: %s", errorMsg);
      return NS_ERROR_FAILURE;
    }
  fJVMP_GetPlugin = 
    (JVMP_GetPlugin_t) PR_FindSymbol(m_dll, "JVMP_GetPlugin");
  if (!fJVMP_GetPlugin)
    {
      if (PR_GetErrorTextLength() < (int) sizeof(errorMsg))
	PR_GetErrorText(errorMsg);
      OJI_LOG2("Cannot resolve JVMP_GetPlugin: %s", errorMsg);
      return NS_ERROR_FAILURE;
    }
  (*fJVMP_GetPlugin)(&m_jvmp_context);
  return NS_OK;
}

nsresult
nsPluggableJVM::initJVM(JVMP_CallingContext* *penv, 
			JavaVMInitArgs* vm_args)
{
  jint res = (m_jvmp_context->JVMP_GetRunningJVM)(penv, vm_args, JNI_TRUE);
  if (res == JNI_TRUE) 
    {
      OJI_LOG("OK GOT JVM!!!!");
      return NS_OK;
    }
  else 
    {
      OJI_LOG("BAD - NO JVM!!!!");
      return NS_ERROR_FAILURE;
    }
  return NS_OK;
}

NS_IMETHODIMP 
nsPluggableJVM::CreateJavaPeer(nsIJavaHTMLObject *obj, 
			       const char*        cid, 
			       jint              *pid)
{
  if ((m_jvmp_context->JVMP_CreatePeer)(m_ctx, cid, 0, pid))
    return NS_OK;
  return NS_ERROR_FAILURE;
}

/* void PostEventToJavaPeer (in jp_jint obj_id, in jp_jint evt_id, in jp_jint evt_data); */
NS_IMETHODIMP 
nsPluggableJVM::PostEventToJavaPeer(jint obj_id, jint evt_id, 
				    jlong evt_data)
{
  if ((m_jvmp_context->JVMP_PostEvent)(m_ctx, obj_id, 
			  evt_id, evt_data, JVMP_PRIORITY_NORMAL) == JNI_FALSE)
    return NS_ERROR_FAILURE;
  return NS_OK;
}

NS_IMETHODIMP 
nsPluggableJVM::JavaPeerCall(jint obj_id, 
			     jint data1, 
			     jlong data2)
{
  if ((m_jvmp_context->JVMP_DirectPeerCall)(m_ctx, obj_id, 
					    data1, data2) == JNI_FALSE)
    return NS_ERROR_FAILURE;
  return NS_OK;
}


/* void SendEventToJavaPeer (in jp_jint obj_id, in jp_jint evt_id, in jp_jint evt_data); */
NS_IMETHODIMP 
nsPluggableJVM::SendEventToJavaPeer(jint obj_id, jint evt_id, 
				    jlong evt_data, jint* result)
{  
  jint r = (m_jvmp_context->JVMP_SendEvent)(m_ctx, obj_id, 
					    evt_id, evt_data, JVMP_PRIORITY_NORMAL);
  if (r == JNI_FALSE) 
    {
       return NS_ERROR_FAILURE;
    }
  if (result) *result = r;
  return NS_OK;
}

/* void DestroyJavaPeer (in jp_jint id); */
NS_IMETHODIMP 
nsPluggableJVM::DestroyJavaPeer(jint id)
{
   if ((m_jvmp_context->JVMP_DestroyPeer)(m_ctx, id))
     return NS_OK;
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP 
nsPluggableJVM::ExtensionCall(jint arg1, jlong arg2)
{
  if (m_extID == 0) return NS_ERROR_FAILURE;
  if ((m_jvmp_context->JVMP_DirectExtCall)(m_ctx, m_extID, arg1, arg2))
     return NS_OK;
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP 
nsPluggableJVM::DeleteGlobalRef(jobject jobj)
{
  m_ctx->env->DeleteGlobalRef(jobj);
  return NS_OK;
}

NS_IMETHODIMP 
nsPluggableJVM::DeleteLocalRef(jobject jobj)
{
  m_ctx->env->DeleteLocalRef(jobj);
  return NS_OK;
}


NS_IMETHODIMP 
nsPluggableJVM::GetWFCtx(JVMP_RuntimeContext* *rt_ctx, 
			 JVMP_CallingContext* *c_ctx)
{
  if (!m_jvmp_context) return NS_ERROR_FAILURE;
  *rt_ctx = m_jvmp_context;
  if ((m_jvmp_context->JVMP_GetCallingContext)(c_ctx) != JNI_TRUE)
    {
      OJI_LOG2("JVMP_GetCallingContext failed: %ld", (*c_ctx)->err);
      return NS_ERROR_FAILURE;
    }
  return NS_OK;
}

NS_IMETHODIMP 
nsPluggableJVM::SetConsoleVisibility(PRBool visibility)
{
  
  //OJI_LOG2("nsPluggableJVM::SetConsoleVisibility: %d", visibility);
  jint rv = (m_jvmp_context->JVMP_PostSysEvent)(m_ctx, 
						JVMP_CMD_CTL_CONSOLE, 
						(jlong) (visibility ? 1 : 0),
						JVMP_PRIORITY_NORMAL);
  return (rv == JNI_TRUE ? NS_OK : NS_ERROR_FAILURE);
}

NS_IMETHODIMP 
nsPluggableJVM::GetConsoleVisibility(PRBool *pVisibility)
{
  // this call uses hacky feature of JVMP_Send*Event(): 
  // returned value can be used to return value from event handler
  jint rv = (m_jvmp_context->JVMP_SendSysEvent)(m_ctx,
						JVMP_CMD_CTL_CONSOLE, 
						(jlong)2,
						JVMP_PRIORITY_NORMAL);
  *pVisibility = (rv == 2 ? PR_TRUE : PR_FALSE);
  return NS_OK;
}

NS_IMETHODIMP 
nsPluggableJVM::PrintToConsole(const char* msg, 
			       const char* encoding)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}
