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
 * $Id: nsJavaHTMLObjectFactory.cpp,v 1.2 2001/07/12 20:32:08 edburns%acm.org Exp $
 *
 * 
 * Contributor(s): 
 *
 *   Nikolay N. Igotti <inn@sparc.spb.su>
 */

#include "nsJavaHTMLObjectFactory.h"
#include "nsPluggableJVM.h"
#include "nsIFactory.h"
#include "nsIPluginInstance.h"
#include "nsJavaHTMLObject.h"
#include "nsAppletHTMLObject.h"
#include "nsIProxyObjectManager.h"
#include "nsIBrowserJavaSupport.h"
#include "nsIPluginInstancePeer.h"
#include "nsIPluginInstancePeer2.h"
#include "nsWFSecureEnv.h"
#include "nsWFSecurityContext.h"
#include "nsIThread.h"
#include "nsIJVMConsole.h"
#include "nsCRT.h"

static NS_DEFINE_IID(kISupportsIID,       NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIPluginIID,         NS_IPLUGIN_IID);
static NS_DEFINE_IID(kIPluginInstanceIID, NS_IPLUGININSTANCE_IID);
static NS_DEFINE_IID(kIJavaHTMLObjectIID, NS_IJAVAHTMLOBJECT_IID);
static NS_DEFINE_IID(kIFactoryIID,        NS_IFACTORY_IID);
static NS_DEFINE_IID(kIPluggableJVMIID,   NS_IPLUGGABLEJVM_IID);
static NS_DEFINE_IID(kIBrowserJavaSupportIID,  NS_IBROWSERJAVASUPPORT_IID);
static NS_DEFINE_IID(kIPluginManagerIID,  NS_IPLUGINMANAGER_IID);
static NS_DEFINE_IID(kIPluginManager2IID, NS_IPLUGINMANAGER2_IID);
static NS_DEFINE_IID(kILiveConnectIID,    NS_ILIVECONNECT_IID);
static NS_DEFINE_IID(kIPluginInstancePeerIID,  NS_IPLUGININSTANCEPEER_IID);
static NS_DEFINE_IID(kIPluginInstancePeer2IID, NS_IPLUGININSTANCEPEER2_IID);
static NS_DEFINE_IID(kIJVMPluginIID,      NS_IJVMPLUGIN_IID);
static NS_DEFINE_IID(kIServiceManagerIID, NS_ISERVICEMANAGER_IID);
static NS_DEFINE_IID(kIJVMManagerIID,     NS_IJVMMANAGER_IID);
static NS_DEFINE_IID(kIJVMConsoleIID,     NS_IJVMCONSOLE_IID);


static NS_DEFINE_CID(kCLiveConnectCID,       NS_CLIVECONNECT_CID);
static NS_DEFINE_CID(kPluginManagerCID,      NS_PLUGINMANAGER_CID);
static NS_DEFINE_CID(kProxyObjectManagerCID, NS_PROXYEVENT_MANAGER_CID);
static NS_DEFINE_CID(kCJVMManagerCID,        NS_JVMMANAGER_CID);


nsJavaHTMLObjectFactory* nsJavaHTMLObjectFactory::instance = nsnull;

NS_IMPL_THREADSAFE_ISUPPORTS5(nsJavaHTMLObjectFactory, \
                              nsIFactory,              \
                              nsIPlugin,               \
                              nsIJVMPlugin,            \
                              nsIJVMConsole,           \
                              nsIBrowserJavaSupport)

static int GetProxyForURL(void* handle, 
                          char* url, 
                          char* *proxy)
{
  nsresult res;
  nsIBrowserJavaSupport* obj = (nsIBrowserJavaSupport*)handle;
  res = obj->GetProxyForURL(url, proxy);
  return res;
}

static int JSCall(void* handle, jint jstid, struct JSObject_CallInfo** call)
{
  nsresult res;
  nsIBrowserJavaSupport* obj = (nsIBrowserJavaSupport*)handle;
  res = obj->JSCall(jstid, call);
  return res;
}
  
nsJavaHTMLObjectFactory::nsJavaHTMLObjectFactory()  {
  NS_INIT_ISUPPORTS();
  m_mgr2 = nsnull;
  m_jvm = new nsPluggableJVM(); 
  NS_ADDREF(m_jvm);
  nsCOMPtr<nsIThread>  mainIThread;    
  nsIThread::GetMainThread(getter_AddRefs(mainIThread));  
  mainIThread->GetPRThread(&m_mainThread);
  instance = this;
  m_jsjRecursion = 0;
  m_wrapper = nsnull;
}

nsJavaHTMLObjectFactory::~nsJavaHTMLObjectFactory() {
  OJI_LOG("nsJavaHTMLObjectFactory::~nsJavaHTMLObjectFactory");
  NS_RELEASE(m_jvm);
  if (m_wrapper) free(m_wrapper);
}


NS_METHOD
nsJavaHTMLObjectFactory::Create(nsISupports* outer,
                                const nsIID& aIID, 
                                void* *aInstancePtr)
{
  //OJI_LOG2("nsJavaHTMLObjectFactory::Create with %p", outer);
  //OJI_LOG2("it is %s", aIID.ToString());
  if (!aInstancePtr)
    return NS_ERROR_INVALID_POINTER;
  *aInstancePtr = nsnull;
  nsresult rv = NS_OK;

  if (!instance) 
    { 
      instance = new nsJavaHTMLObjectFactory();
      // XXX: instance never will be freed, need to be rewritten            
      NS_ADDREF(instance);
      rv = instance->Initialize();  
      if (NS_FAILED(rv)) return rv;
    }

  if (aIID.Equals(kIPluginIID) || aIID.Equals(kIFactoryIID))
    {
      return instance->QueryInterface(aIID, aInstancePtr);
    }

  if (aIID.Equals(kIPluginInstanceIID) || aIID.Equals(kIJavaHTMLObjectIID)) 
    {
      return instance->CreateInstance(outer, 
                                      aIID, 
                                      aInstancePtr);
    }  
  OJI_LOG2("nsJavaHTMLObjectFactory::Create: no matching ifaces for %s", 
	   aIID.ToString()); 
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP 
nsJavaHTMLObjectFactory::CreateInstance(nsISupports *aOuter, 
                                        const nsIID & aIID,
                                        void **result) 
{
  *result = NULL;
  if (aOuter != NULL) 
    {
      OJI_LOG("Aggregation of nsJavaHTMLObjectFactory failed");
      return NS_ERROR_NO_AGGREGATION;
    }
  // m_env is set if JVM is started up
  if (!m_jvm || !m_env) return NS_ERROR_FAILURE;
  // XXX: remove me when it'll be called by Mozilla
  //if (NS_FAILED(Initialize())) return NS_ERROR_FAILURE; 
  if (aIID.Equals(kIJavaHTMLObjectIID)) 
    {
      OJI_LOG("NEW API - creating IJavaHTMLObject");
      nsJavaHTMLObject* instance = new nsJavaHTMLObject(this);
      *result = (nsIJavaHTMLObject*) instance;
      NS_ADDREF(instance);
      return NS_OK;
    }
  // it's current Mozilla API where it ask us to create PluginInstance
  // give it a wrapper
  if (aIID.Equals(kIPluginInstanceIID)) 
    {
      OJI_LOG("OLD API - creating IPluginInstance");
      nsAppletHTMLObject* instance = new nsAppletHTMLObject(this);
      *result = (nsIPluginInstance *) instance;
      NS_ADDREF(instance);
      return NS_OK;
    }
  if (aIID.Equals(kIPluginIID)) 
    {
      OJI_LOG("request for factory");
      return QueryInterface(kIPluginIID, result);
    }
  return NS_ERROR_NO_INTERFACE;
}

NS_IMETHODIMP
nsJavaHTMLObjectFactory::Initialize(void)
{
  //OJI_LOG2("nsJavaHTMLObjectFactory::Initialize: %p", this);
  static int inited = 0;
  if (inited) return NS_OK;
  nsresult res;
  nsPluggableJVMStatus status;  
  res = nsServiceManager::GetService(kPluginManagerCID, 
				     kIPluginManagerIID,
				     getter_AddRefs(m_mgr));  
  if (NS_FAILED(res)) return res;
  res = nsServiceManager::GetService(kCJVMManagerCID, 
				     kIJVMManagerIID,
				     getter_AddRefs(m_jvmMgr));  
  if (NS_FAILED(res)) return res;
  m_mgr2 = do_QueryInterface(m_mgr);
  if (m_mgr2 == nsnull) return NS_ERROR_FAILURE;

  m_wrapper = (BrowserSupportWrapper*) malloc(sizeof(BrowserSupportWrapper));
  m_wrapper->info = (void*)(nsIBrowserJavaSupport*)this;
  m_wrapper->GetProxyForURL = &::GetProxyForURL;
  m_wrapper->JSCall = &::JSCall;

  res = m_jvm->StartupJVM("", &status, PTR_TO_JLONG(m_wrapper));
  if (NS_FAILED(res) || status != nsPluggableJVMStatus_Running) 
    return NS_ERROR_FAILURE;
  m_jvm->GetJNIEnv(&m_env);
  inited = 1;
  OJI_LOG("nsJavaHTMLObjectFactory::Initialize done");  
  return NS_OK;
}


NS_IMETHODIMP
nsJavaHTMLObjectFactory::CreatePluginInstance(nsISupports *aOuter,
					      REFNSIID aIID,
					      const char * aMimeType,
					      void **result) 
{
  return CreateInstance(aOuter, aIID, result);
}

NS_IMETHODIMP 
nsJavaHTMLObjectFactory::GetMIMEDescription(const char **resDesc) 
{
  *resDesc = 
    "application/x-java-vm::Java(tm) Plug-in"
    ";application/x-java-applet::Java(tm) Plug-in"
    ";application/x-java-applet:version=1.1::Java(tm) Plug-in"
    ";application/x-java-applet:version=1.1.1::Java(tm) Plug-in"
    ";application/x-java-applet:version=1.1.2::Java(tm) Plug-in"
    ";application/x-java-applet:version=1.1.3::Java(tm) Plug-in"
    ";application/x-java-applet:version=1.2::Java(tm) Plug-in"
    ";application/x-java-applet:version=1.2.1::Java(tm) Plug-in"
    ";application/x-java-applet:version=1.2.2::Java(tm) Plug-in"
    ";application/x-java-applet:version=1.3::Java(tm) Plug-in"
    ";application/x-java-bean::Java(tm) Plug-in"
    ";application/x-java-bean:version=1.1::Java(tm) Plug-in"
    ";application/x-java-bean:version=1.1.1::Java(tm) Plug-in"
    ";application/x-java-bean:version=1.1.2::Java(tm) Plug-in"
    ";application/x-java-bean:version=1.1.3::Java(tm) Plug-in"
    ";application/x-java-bean:version=1.2::Java(tm) Plug-in"
    ";application/x-java-bean:version=1.2.1::Java(tm) Plug-in"
    ";application/x-java-bean:version=1.2.2::Java(tm) Plug-in"
    ";application/x-java-bean:version=1.3::Java(tm) Plug-in"
    "";
    return NS_OK;
}

NS_IMETHODIMP nsJavaHTMLObjectFactory::LockFactory(PRBool aLock) 
{
  return NS_OK;
}

NS_IMETHODIMP
nsJavaHTMLObjectFactory::Shutdown(void) 
{
  OJI_LOG("nsJavaHTMLObjectFactory::Shutdown");
  return NS_OK;
}



NS_IMETHODIMP
nsJavaHTMLObjectFactory::GetValue(nsPluginVariable variable, 
				  void *value) 
{
    nsresult err = NS_OK;
    return err;
}

NS_IMETHODIMP
nsJavaHTMLObjectFactory::GetJVM(nsIPluggableJVM* *jvm)
{
  if ((jvm != NULL) && (m_jvm != NULL)) 
    {
      *jvm = m_jvm;
      return NS_OK;
    }
  return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP
nsJavaHTMLObjectFactory::Show()
{
  if (m_jvm) return m_jvm->SetConsoleVisibility(PR_TRUE); 
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsJavaHTMLObjectFactory::Hide()
{
  if (m_jvm) return m_jvm->SetConsoleVisibility(PR_FALSE); 
  return NS_ERROR_FAILURE;
}
  
NS_IMETHODIMP
nsJavaHTMLObjectFactory::IsVisible(PRBool *result)
{
  if (m_jvm) return m_jvm->GetConsoleVisibility(result); 
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsJavaHTMLObjectFactory::Print(const char* msg, 
			       const char* encodingName)
{
  if (m_jvm) return m_jvm->PrintToConsole(msg, encodingName); 
  return NS_ERROR_FAILURE;
}

/* HERE INTERTHREAD PROXY STUFF FOLLOWS */
// utility proxy class
class nsJavaPluginProxy : public nsIJavaPluginProxy 
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIJAVAPLUGINPROXY

  nsJavaPluginProxy();
  virtual ~nsJavaPluginProxy();
  NS_IMETHOD Init(nsJavaHTMLObjectFactory* owner);
protected:
  nsJavaHTMLObjectFactory* m_owner;
};

NS_IMPL_THREADSAFE_ISUPPORTS1(nsJavaPluginProxy, 
			      nsIJavaPluginProxy);

nsJavaPluginProxy::nsJavaPluginProxy()
{
  NS_INIT_ISUPPORTS();
}

NS_IMETHODIMP
nsJavaPluginProxy::Init(nsJavaHTMLObjectFactory* owner)
{
  m_owner = owner;
  NS_ADDREF(owner);
  return NS_OK;
}

nsJavaPluginProxy::~nsJavaPluginProxy()
{
  NS_RELEASE(m_owner);
}

NS_IMETHODIMP
nsJavaPluginProxy::GetProxyForURL(const char* url, 
				  char* *result)
{
  return m_owner->doGetProxyForURL(url, result);
}

NS_IMETHODIMP
nsJavaPluginProxy::JSCall(jint jstid, struct JSObject_CallInfo** call)
{
  return m_owner->doJSCall(jstid, call);
}

/* END OF INTERTHREAD STUFF */


NS_IMETHODIMP 
nsJavaHTMLObjectFactory::GetProxyForURL(const char *url, char **proxy)
{
  nsresult rv = NS_OK;
  if (PR_GetCurrentThread() == m_mainThread) 
    return doGetProxyForURL(url, proxy);
  nsCOMPtr<nsJavaPluginProxy> inst = 
    new nsJavaPluginProxy();
  inst->Init(this);
  nsCOMPtr<nsIJavaPluginProxy> instProxy;
  NS_WITH_SERVICE(nsIProxyObjectManager, proxyObjectManager,
		  kProxyObjectManagerCID, &rv);
  rv = proxyObjectManager->
    GetProxyForObject(NS_UI_THREAD_EVENTQ,
		      NS_GET_IID(nsIJavaPluginProxy),
		      NS_STATIC_CAST(nsISupports*, inst),
		      PROXY_SYNC | PROXY_ALWAYS,
		      getter_AddRefs(instProxy));
  if (NS_FAILED(rv)) return rv;
  rv = instProxy->GetProxyForURL(url, proxy);
  return rv;
}

NS_IMETHODIMP 
nsJavaHTMLObjectFactory::JSCall(jint jstid, struct JSObject_CallInfo** call)
{
  nsresult rv = NS_OK;
  // TODO: I dunno why, but if not proxify it, some calls like GetWindow
  // just fails, maybe due timing issues. I have to investigate it in the 
  // future.
  // if this thread is already main thread - don't forward anything  
  //if (PR_GetCurrentThread() == m_mainThread) return doJSCall(jstid, call);
  
  nsCOMPtr<nsJavaPluginProxy> inst = new nsJavaPluginProxy();
  inst->Init(this);
  nsCOMPtr<nsIJavaPluginProxy> instProxy;
  NS_WITH_SERVICE(nsIProxyObjectManager, proxyObjectManager,
		  kProxyObjectManagerCID, &rv);
  rv = proxyObjectManager->
    GetProxyForObject(NS_UI_THREAD_EVENTQ,
		      NS_GET_IID(nsIJavaPluginProxy),
		      NS_STATIC_CAST(nsISupports*, inst),
		      PROXY_SYNC | PROXY_ALWAYS,
		      getter_AddRefs(instProxy));
  if (NS_FAILED(rv)) return rv;
  rv = instProxy->JSCall(jstid, call);
  return rv;
}


NS_IMETHODIMP
nsJavaHTMLObjectFactory::doGetProxyForURL(const char* url,
					  char* *proxy)
{ 
  if (!m_mgr2) 
    {
      *proxy = nsCRT::strdup(""); // falls back to direct connection 
      return NS_OK;
    }
  return m_mgr2->FindProxyForURL(url, proxy);
}

NS_IMETHODIMP
nsJavaHTMLObjectFactory::doJSCall(jint jstid, struct JSObject_CallInfo** call)
{
  // placed here to avoid lifetime pain in switch
  nsCOMPtr<nsIPluginInstance>  plugin   = nsnull;
  nsCOMPtr<nsISecurityContext> pContext = nsnull;
  nsresult rv = NS_OK;
  

  if ((*call)->pWrapper && (*call)->pWrapper->dead) 
    {
      OJI_LOG("JSObject dead before JS call can be performed.");
      return NS_ERROR_FAILURE;
    }
  NS_WITH_SERVICE(nsIJVMManager, pJVMManager, kCJVMManagerCID, &rv);
  if (NS_FAILED(rv))
    {
      OJI_LOG2("cannot get JVMManager service: %x", rv);
      return rv;
    }

  if (m_liveConnect == nsnull && NS_FAILED(rv = initLiveConnect()))
    {
      OJI_LOG2("Cannot init Live Connect: %x", rv);
      return rv;
    }

  JNIEnv* pProxyJNIEnv = nsnull;
  if (NS_FAILED(rv = pJVMManager->CreateProxyJNI(NULL, &pProxyJNIEnv)))
    {
       OJI_LOG2("Cannot create proxy JNI: %x", rv);
       return rv;
    }

  if (NS_FAILED(rv=nsWFSecurityContext::Create(NULL, m_jvm, 
					       (*call)->jAccessControlContext,
					       (*call)->pszUrl,
					       kISupportsIID,
					       getter_AddRefs(pContext))))
    {
      OJI_LOG2("Cannot create proper security context: %x", rv);
      return rv;
    }
  m_jsjRecursion++;
  if (m_jsjRecursion > 1) OJI_LOG2("incremented depth=%d!!!",
				   (int)m_jsjRecursion);

  switch ((*call)->type)
    {
    case JS_GetWindow:
      OJI_LOG2("JS_GetWindow: %p",  (*call)->data.__getWindow.pPlugin);
      plugin = do_QueryInterface((nsISupports*)((*call)->data.__getWindow.pPlugin));
      if (plugin == nsnull)
	{
	  OJI_LOG("Bug in internals - pPlugin field is incorrect");
	  break;
	}
      if (NS_FAILED(m_liveConnect->GetWindow(pProxyJNIEnv,
					     plugin,
					     NULL,
					     0,
					     (nsISupports*)pContext,
					     (jsobject*) &((*call)->data.__getWindow.iNativeJSObject))))
	OJI_LOG("Liveconnect::GetWindow failed");
      break;
    case JS_Eval:
      {
	jobject ret = NULL;
	OJI_LOG2("JS_Eval: script len=%d",  (*call)->data.__eval.iEvalLen);
	if (NS_FAILED(m_liveConnect->Eval(pProxyJNIEnv, 
					  (jsobject)(*call)->data.__eval.iNativeJSObject,
					  (*call)->data.__eval.pszEval, 
					  (*call)->data.__eval.iEvalLen, 
					  NULL,
					  0,
					  (nsISupports*)pContext, 
					  &ret)))
	  {
	    OJI_LOG("Liveconnect::Eval failed");
	  }
	else
	  {
	    if (ret != NULL)
	      (*call)->data.__eval.jResult = m_env->NewGlobalRef(ret);      
	  }
	break;
      }
    case JS_ToString:
      {
	jstring ret = NULL;
	m_liveConnect->ToString(pProxyJNIEnv, 
				(jsobject)(*call)->data.__toString.iNativeJSObject, 
				&ret);
	if (ret != NULL)
	  (*call)->data.__toString.jResult = (jstring)(m_env->NewGlobalRef(ret));
	break;
      }

    case JS_Finalize:
      OJI_LOG2("JS_Finalize: %x", 
		 (int)(*call)->data.__finalize.iNativeJSObject);
      m_liveConnect->FinalizeJSObject(pProxyJNIEnv, 
				      (jsobject)(*call)->data.__finalize.iNativeJSObject);
      break;

    case JS_Call:
      {
	jobject ret = NULL;      
	OJI_LOG2("JS_Call: obj=%x",  
		 (int)(*call)->data.__call.iNativeJSObject);
	if (NS_FAILED(m_liveConnect->Call(pProxyJNIEnv, 
					  (jsobject)
					  (*call)->data.__call.iNativeJSObject, 
					  (*call)->data.__call.pszMethodName, 
					  (*call)->data.__call.iMethodNameLen, 
					  (*call)->data.__call.jArgs,
					  /* (void**)&pPrincipal, */ NULL,
					  /* principalLen, */ 0,
					  (nsISupports*)pContext, 
					  &ret)))
	  {
	    OJI_LOG("Call failed");
	    break;
	  }	
	if (ret != NULL)
	  (*call)->data.__call.jResult = m_env->NewGlobalRef(ret);      
      }
      break;

    case JS_GetMember:
      {
	jobject ret = NULL;
	m_liveConnect->GetMember(pProxyJNIEnv, 
				 (jsobject)(*call)->data.__getMember.iNativeJSObject, 
				 (*call)->data.__getMember.pszName, 
				 (*call)->data.__getMember.iNameLen, 
				 /* (void**)&pPrincipal, */ NULL,
				 /* principalLen, */ 0,
				 (nsISupports*)pContext, 
				 &ret);      
	if (ret != NULL)
	  (*call)->data.__getMember.jResult = m_env->NewGlobalRef(ret);
	break;
      }
    case JS_SetMember:
      m_liveConnect->SetMember(pProxyJNIEnv, 
			       (jsobject)(*call)->data.__setMember.iNativeJSObject, 
			       (*call)->data.__setMember.pszName, 
			       (*call)->data.__setMember.iNameLen, 
			       (*call)->data.__setMember.jValue,
			       /* (void**)&pPrincipal, */ NULL,
			       /* principalLen, */ 0,
			       (nsISupports*)pContext);
      break;


    case JS_RemoveMember:
      m_liveConnect->RemoveMember(pProxyJNIEnv, 
				  (jsobject)(*call)->data.__removeMember.iNativeJSObject,
				  (*call)->data.__removeMember.pszName, 
				  (*call)->data.__removeMember.iNameLen, 
				  /* (void**)&pPrincipal, */ NULL,
				  /* principalLen, */ 0,
				  (nsISupports*)pContext);
      break;
      
     case JS_GetSlot:
       {
	 jobject ret = NULL;       
	 m_liveConnect->GetSlot(pProxyJNIEnv, 
				(jsobject)(*call)->data.__getSlot.iNativeJSObject,
				(*call)->data.__getSlot.iIndex,
				/* (void**)&pPrincipal, */ NULL,
				/* principalLen, */ 0,
				(nsISupports*)pContext,
				&ret);
       
	 if (ret != NULL)
	   (*call)->data.__getSlot.jResult = m_env->NewGlobalRef(ret);
	 break;
       }
    case JS_SetSlot:
      m_liveConnect->SetSlot(pProxyJNIEnv, 
			     (jsobject)(*call)->data.__setSlot.iNativeJSObject,
			     (*call)->data.__setSlot.iIndex,
			     (*call)->data.__setSlot.jValue,
			     /* (void**)&pPrincipal, */ NULL,
			     /* principalLen, */ 0,
			     (nsISupports*)pContext);
      break;

    default:
      OJI_LOG2("UNKNOWN CALL: %d", (*call)->type);
    }
  jthrowable jException = m_env->ExceptionOccurred();

  if (jException != NULL)
    {
      OJI_LOG("exception in doJSCall");
      m_env->ExceptionDescribe();
      // Clean up exception
      m_env->ExceptionClear();
      
      // Store the exception in JSObject_CallInfo
      (*call)->jException = 
	NS_STATIC_CAST(jthrowable, m_env->NewGlobalRef(jException));
      // Release local ref
      m_env->DeleteLocalRef(jException);
    }
  m_jsjRecursion--;
  return NS_OK;
}



NS_IMETHODIMP 
nsJavaHTMLObjectFactory::AddToClassPath(const char* dirPath)
{
  OJI_LOG("nsJavaHTMLObjectFactory::AddToClassPath");
  return NS_OK;
}
NS_IMETHODIMP
nsJavaHTMLObjectFactory::RemoveFromClassPath(const char* dirPath)
{
  OJI_LOG("nsJavaHTMLObjectFactory::RemoveFromClassPath");
  return NS_OK;
}
NS_IMETHODIMP 
nsJavaHTMLObjectFactory::GetClassPath(const char* *result)
{
  OJI_LOG("nsJavaHTMLObjectFactory::GetClassPath");
  return NS_OK;
}
NS_IMETHODIMP 
nsJavaHTMLObjectFactory::GetJavaWrapper(JNIEnv*  jenv, 
					jint     jsobj, 
					jobject *jobj)
{
  OJI_LOG2("GetJavaWrapper for %x", (int)jsobj);
  if (!m_jvm || !jobj) return NS_ERROR_FAILURE;
  *jobj = NULL;
  return m_jvm->GetJavaWrapper(jsobj, jobj);
}

NS_IMETHODIMP
nsJavaHTMLObjectFactory::CreateSecureEnv(JNIEnv*       proxyEnv, 
					 nsISecureEnv* *outSecureEnv)
{
  return nsWFSecureEnv::Create(m_jvm, 
			       kISupportsIID, (void**)outSecureEnv);
}
NS_IMETHODIMP
nsJavaHTMLObjectFactory::SpendTime(PRUint32 timeMillis)
{
  OJI_LOG("nsJavaHTMLObjectFactory::SpendTime");
  return NS_OK;
}
NS_IMETHODIMP
nsJavaHTMLObjectFactory::UnwrapJavaWrapper(JNIEnv* jenv, 
					   jobject jobj, 
					   jint*   jsobj)
{
  OJI_LOG2("nsJavaHTMLObjectFactory::UnwrapJavaWrapper for %x", (int)jobj);
  if (!m_jvm || !jsobj) return NS_ERROR_FAILURE;
  *jsobj = 0;
  return m_jvm->UnwrapJavaWrapper(jobj, jsobj);
}


NS_IMETHODIMP
nsJavaHTMLObjectFactory::initLiveConnect()
{
  nsresult res;
  res = nsServiceManager::GetService(kCLiveConnectCID,
				     kILiveConnectIID,
				     getter_AddRefs(m_liveConnect));
  if (NS_FAILED(res))
  {
    OJI_LOG2("cannot get LiveConnect service from browser: %x", res);
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}




