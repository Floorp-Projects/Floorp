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
 * $Id: nsAppletHTMLObject.cpp,v 1.2 2001/07/12 20:32:07 edburns%acm.org Exp $
 *
 * 
 * Contributor(s): 
 *
 *   Nikolay N. Igotti <inn@sparc.spb.su>
 */

#include "nsAppletHTMLObject.h"
#include "nsJavaHTMLObject.h"
#include "nsJavaObjectInfo.h"
#include "nsIPluginTagInfo.h"
#include "nsIProxyObjectManager.h"
#include "nsIServiceManager.h"
#include "nsIPluginManager2.h"
#include "nsIJavaPluginInstanceProxy.h"
#include "nsIWFInstanceWrapper.h"
#include "nsIJVMPluginInstance.h"
#include "wf_moz6_common.h"
#include "nsIThread.h"

static NS_DEFINE_CID(kProxyObjectManagerCID,   NS_PROXYEVENT_MANAGER_CID);
static NS_DEFINE_CID(kPluginManagerCID,        NS_PLUGINMANAGER_CID);

static NS_DEFINE_IID(kIPluginManagerIID,       NS_IPLUGINMANAGER_IID);
static NS_DEFINE_IID(kIPluginManager2IID,      NS_IPLUGINMANAGER2_IID);
static NS_DEFINE_IID(kIPluginInstancePeerIID,  NS_IPLUGININSTANCEPEER_IID);
static NS_DEFINE_IID(kIPluginInstancePeer2IID, NS_IPLUGININSTANCEPEER2_IID);
static NS_DEFINE_IID(kIPluginTagInfoIID,       NS_IPLUGINTAGINFO_IID);
static NS_DEFINE_IID(kIPluginTagInfo2IID,      NS_IPLUGINTAGINFO2_IID);

NS_IMPL_THREADSAFE_ISUPPORTS3(nsAppletHTMLObject, 
			      nsIPluginInstance,
			      nsIWFInstanceWrapper,
			      nsIJVMPluginInstance)

nsAppletHTMLObject::nsAppletHTMLObject(nsJavaHTMLObjectFactory *factory) 
{
  NS_INIT_ISUPPORTS();
  m_factory = factory;
  m_mgr = nsnull;
  NS_ADDREF(m_factory);
  m_peer = m_peer2 = nsnull;
  m_dead = PR_FALSE;
  nsCOMPtr<nsIThread>  mainIThread;    
  nsIThread::GetMainThread(getter_AddRefs(mainIThread));  
  mainIThread->GetPRThread(&m_mainThread);
}

nsAppletHTMLObject::~nsAppletHTMLObject(void) {
  OJI_LOG("nsAppletHTMLObject::~nsAppletHTMLObject");
  if (m_factory) NS_RELEASE(m_factory);
}

NS_IMETHODIMP
nsAppletHTMLObject::Initialize(nsIPluginInstancePeer *peer)
{
  nsresult res;
  OJI_LOG("nsAppletHTMLObject::Initialize");
  m_peer = peer;
  NS_ADDREF(m_peer);
  peer->QueryInterface(kIPluginInstancePeer2IID,
                       (void **)&m_peer2);
  m_jobject = new nsJavaHTMLObject(m_factory);
  NS_ADDREF(m_jobject);
  res = nsServiceManager::GetService(kPluginManagerCID, 
				     kIPluginManagerIID,
				     getter_AddRefs(m_mgr));
  if (NS_FAILED(res)) return res;
  m_id = 0;
  if (NS_FAILED(res = m_factory->GetJVM(&m_jvm)))          return res;
  if (NS_FAILED(res = m_jvm->GetJNIEnv(&m_env)))           return res;
  if (NS_FAILED(res = m_jobject->Initialize(peer, m_jvm))) return res;
  // find out applet information from our peer and explain it 
  // to nsJavaHTMLObject
  // in new API nsJavaHTMLObject should get everything needed
  // from peer
  nsJavaObjectInfo* info = 
    new nsJavaObjectInfo((nsIWFInstanceWrapper*)this);
  NS_ADDREF(info);
  nsIPluginTagInfo *tagInfo;
  if (NS_SUCCEEDED(res = peer->QueryInterface(kIPluginTagInfoIID,
					      (void **)&tagInfo)))
    {
      PRUint16 argc = 0, i;
      char **keys, **vals;
      tagInfo->GetAttributes(argc, (const char* const* &)keys, 
			     (const char* const* &) vals);
      for (i=0; i<argc; i++) 
	info->AddParameter(keys[i], vals[i]);
      NS_RELEASE(tagInfo);
    }
  
  nsIPluginTagInfo2 *extTagInfo;
  if (NS_SUCCEEDED(res = peer->QueryInterface(kIPluginTagInfo2IID, 
					      (void **)&extTagInfo)))
    {
      nsPluginTagType tagType = nsPluginTagType_Unknown;
      if (NS_SUCCEEDED(extTagInfo->GetTagType(&tagType)))
        {
          if (tagType == nsPluginTagType_Applet || 
              tagType == nsPluginTagType_Object) 
	    {
	      PRUint16 argc = 0, i;
	      char **keys, **vals;
	      extTagInfo->GetParameters(argc, 
					(const char*const*&)keys,
					(const char*const*&) vals); 
	      for (i=0; i<argc; i++) 
		info->AddParameter(keys[i], vals[i]);
	    }
	  nsJavaObjectType type = mapType(tagType);
	  info->SetType(type);
	  if (NS_FAILED(res = m_jobject->SetType(type)))
	    return res;
        }
      const char *docbase = NULL;
      if (NS_FAILED(res = extTagInfo->GetDocumentBase(&docbase)))
	{
	  OJI_LOG("Failed to obtain doc base");
	}
      if (docbase != NULL) 
	{
	  char *slash = strrchr((char*)docbase, '/');
	  if (slash != NULL) *(slash+1) = 0;
	  OJI_LOG2("real docbase is %s", docbase);
	  info->SetDocBase(docbase);
        }            
      NS_RELEASE(extTagInfo);
    }
  // really it's crucial to call SetType() _before_ passing Java object info
  // otherwise parameters will be passed to wrapper - and silently ignored
  res = m_jobject->SetJavaObjectInfo(info);
  NS_RELEASE(info);
  return res;
}

NS_IMETHODIMP
nsAppletHTMLObject::GetPeer(nsIPluginInstancePeer* *resultingPeer) 
{
  //OJI_LOG("nsAppletHTMLObject::GetPeer");
  if (m_dead) return NS_ERROR_FAILURE;
  *resultingPeer = m_peer;
  NS_ADDREF(m_peer);
  return NS_OK;  
}

NS_IMETHODIMP 
nsAppletHTMLObject::Start(void)
{
  OJI_LOG("nsAppletHTMLObject::Start");
  m_dead = PR_FALSE;
  return m_jobject->Start();
}

NS_IMETHODIMP
nsAppletHTMLObject::Stop(void)
{
  OJI_LOG("nsAppletHTMLObject::Stop");
  if (m_jobject) m_jobject->Stop();
  if (m_id && m_jvm) m_jvm->UnregisterWindow(m_id);
  m_id = 0;
  m_dead = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP
nsAppletHTMLObject::Destroy(void)
{
  OJI_LOG("nsAppletHTMLObject::Destroy");
  m_dead = PR_TRUE;
  if (m_jobject)
    {
      m_jobject->Destroy();
      NS_RELEASE(m_jobject);
    }
  if (m_peer)    NS_RELEASE(m_peer);
  if (m_peer2)   NS_RELEASE(m_peer2);
  if (m_jvm)     NS_RELEASE(m_jvm); 
  //OJI_LOG2("mRefCnt=%d",mRefCnt);
  return NS_OK;
}
 
// XXX: for now, this code can leak window descriptors,
// as not unregisters old window, if new one has come 
// with another native handle.
// SetWindow() with size changing hopefully works well
NS_IMETHODIMP
nsAppletHTMLObject::SetWindow(nsPluginWindow* window)
{
  OJI_LOG2("nsAppletHTMLObject::SetWindow(%p)", 
	   window != nsnull ? (void*)window->window : 0x0);
  nsresult res = NS_OK;
  
  if (m_jvm == NULL) 
    {
      OJI_LOG("m_jvm is NULL");
      return NS_ERROR_FAILURE;
    }

  if (window != NULL) 
    {
      jint new_id;
      res = m_jvm->RegisterWindow(window, &new_id);
      if (NS_FAILED(res)) return res;
      //if (m_id != 0 && new_id != m_id) m_jvm->UnregisterWindow(m_id);
      m_id = new_id;
    }
  else
    {
      if (m_id != 0) m_jvm->UnregisterWindow(m_id);
      m_id = 0;
    }
  return m_jobject->SetWindow(m_id);
}

NS_IMETHODIMP
nsAppletHTMLObject::NewStream(nsIPluginStreamListener** listener)
{
  OJI_LOG("nsAppletHTMLObject::NewStream");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsAppletHTMLObject::Print(nsPluginPrint* platformPrint)
{
  OJI_LOG("nsAppletHTMLObject::Print");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsAppletHTMLObject::GetValue(nsPluginInstanceVariable variable, void *value)
{
  OJI_LOG2("nsAppletHTMLObject::GetValue: %d", variable);
  if (variable == nsPluginInstanceVariable_DoCacheBool) 
    {
      //OJI_LOG("SET NO PLUGIN CACHING");
      *(PRBool*)value = PR_FALSE;
      return NS_OK;
    }
  return NS_OK;
}

NS_IMETHODIMP
nsAppletHTMLObject::HandleEvent(nsPluginEvent* event, PRBool* handled)
{
  OJI_LOG("nsAppletHTMLObject::HandleEvent");
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsJavaObjectType 
nsAppletHTMLObject::mapType(nsPluginTagType pluginType)
{
  switch (pluginType)
    {
    case nsPluginTagType_Applet: 
      return nsJavaObjectType_Applet;
    case nsPluginTagType_Object:
      return nsJavaObjectType_Object;
    case nsPluginTagType_Embed:
      return nsJavaObjectType_Embed;
    default:
      return nsJavaObjectType_Unknown;
    }
}
/* HERE INTERTHREAD PROXY STUFF FOLLOWS */
// utility proxy class
class nsJavaPluginInstanceProxy : public nsIJavaPluginInstanceProxy 
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIJAVAPLUGININSTANCEPROXY

  nsJavaPluginInstanceProxy();
  virtual ~nsJavaPluginInstanceProxy();
  NS_IMETHOD Init(nsAppletHTMLObject* owner);
protected:
  nsAppletHTMLObject* m_owner;
};

NS_IMPL_THREADSAFE_ISUPPORTS1(nsJavaPluginInstanceProxy, 
			      nsIJavaPluginInstanceProxy);

nsJavaPluginInstanceProxy::nsJavaPluginInstanceProxy()
{
  NS_INIT_ISUPPORTS();
}

NS_IMETHODIMP
nsJavaPluginInstanceProxy::Init(nsAppletHTMLObject* owner)
{
  m_owner = owner;
  NS_ADDREF(owner);
  return NS_OK;
}

nsJavaPluginInstanceProxy::~nsJavaPluginInstanceProxy()
{
  NS_RELEASE(m_owner);
}

NS_IMETHODIMP
nsJavaPluginInstanceProxy::ShowDocument(const char* url, 
					const char* target)
{
  return m_owner->doShowDocument(url, target);
}

NS_IMETHODIMP
nsJavaPluginInstanceProxy::ShowStatus(const char* status)
{
  return m_owner->doShowStatus(status);
}

NS_IMETHODIMP
nsJavaPluginInstanceProxy::GetJSThread(jint* jstid)
{
  return m_owner->doGetJSThread(jstid);
}

// Here follows thread-safe versions of methods of plugin API
NS_IMETHODIMP
nsAppletHTMLObject::ShowStatus(const char* status)
{
  nsresult rv = NS_OK;
  if (m_dead) return NS_ERROR_FAILURE;
  if (PR_GetCurrentThread() == m_mainThread) return doShowStatus(status);
  nsCOMPtr<nsJavaPluginInstanceProxy> inst = 
    new nsJavaPluginInstanceProxy();
  inst->Init(this);
  nsCOMPtr<nsIJavaPluginInstanceProxy> instProxy;
  NS_WITH_SERVICE(nsIProxyObjectManager, proxyObjectManager,
		  kProxyObjectManagerCID, &rv);
  rv = proxyObjectManager->
    GetProxyForObject(NS_UI_THREAD_EVENTQ,
		      NS_GET_IID(nsIJavaPluginInstanceProxy),
		      NS_STATIC_CAST(nsISupports*, inst),
		      PROXY_ASYNC | PROXY_ALWAYS,
		      getter_AddRefs(instProxy));
  if (NS_FAILED(rv)) return rv;
  //OJI_LOG(">>>>>>>>>>>>>>>>>>>");
  rv = instProxy->ShowStatus(status);
  //OJI_LOG2("ShowStatus(%s) done ", status);
  return rv;
}


NS_IMETHODIMP
nsAppletHTMLObject::ShowDocument(const char* url,
				 const char* target)
{
  nsresult rv = NS_OK;
  if (m_dead) return NS_ERROR_FAILURE;
  if (PR_GetCurrentThread() == m_mainThread) 
    return doShowDocument(url, target);
  nsCOMPtr<nsJavaPluginInstanceProxy> inst = 
    new nsJavaPluginInstanceProxy();
  inst->Init(this);
  nsCOMPtr<nsIJavaPluginInstanceProxy> instProxy;
  NS_WITH_SERVICE(nsIProxyObjectManager, proxyObjectManager,
		  kProxyObjectManagerCID, &rv);
  rv = proxyObjectManager->
    GetProxyForObject(NS_UI_THREAD_EVENTQ,
		      NS_GET_IID(nsIJavaPluginInstanceProxy),
		      NS_STATIC_CAST(nsISupports*, inst),
		      PROXY_SYNC | PROXY_ALWAYS,
		      getter_AddRefs(instProxy));
  if (NS_FAILED(rv)) return rv;
  rv = instProxy->ShowDocument(url, target);
  return rv;
}

NS_IMETHODIMP
nsAppletHTMLObject::GetJSThread(jint* jstid)
{
  PRUint32 jsThreadID = 0;
  if (m_dead) return NS_ERROR_FAILURE;  
  if (NS_SUCCEEDED(m_peer2->GetJSThread(&jsThreadID)))
    *jstid = (jint)(jsThreadID);
  return NS_OK;
  /*
  nsresult rv = NS_OK;
  if (m_dead) return NS_ERROR_FAILURE;
  nsCOMPtr<nsJavaPluginInstanceProxy> inst = 
    new nsJavaPluginInstanceProxy();
  inst->Init(this);
  nsCOMPtr<nsIJavaPluginInstanceProxy> instProxy;
  NS_WITH_SERVICE(nsIProxyObjectManager, proxyObjectManager,
		  kProxyObjectManagerCID, &rv);
  rv = proxyObjectManager->
    GetProxyForObject(NS_UI_THREAD_EVENTQ,
		      NS_GET_IID(nsIJavaPluginInstanceProxy),
		      NS_STATIC_CAST(nsISupports*, inst),
		      PROXY_SYNC | PROXY_ALWAYS,
		      getter_AddRefs(instProxy));
  if (NS_FAILED(rv)) return rv;
  rv = instProxy->GetJSThread(jstid);
  return rv; */
}

//  Here follows thread-unsafe versions of methods of plugin API
NS_IMETHODIMP
nsAppletHTMLObject::doShowStatus(const char* status)
{
  if (m_dead) return NS_ERROR_FAILURE;
  return m_peer->ShowStatus(status);
}


NS_IMETHODIMP
nsAppletHTMLObject::doShowDocument(const char* url,
				   const char* target)
{
  if (m_dead) return NS_ERROR_FAILURE;
  return m_mgr->GetURL((nsIPluginInstance*)this, url, target, 
		       NULL, NULL, NULL, PR_FALSE);
}

NS_IMETHODIMP
nsAppletHTMLObject::doGetJSThread(jint* jstid)
{
  PRUint32 jsThreadID = 0;
  if (m_dead) return NS_ERROR_FAILURE;  
  if (NS_SUCCEEDED(m_peer2->GetJSThread(&jsThreadID)))
    *jstid = (jint)(jsThreadID);
  return NS_OK;
}

NS_IMETHODIMP
nsAppletHTMLObject::GetJavaObject(jobject *result)
{
  if (!result || !m_jobject) return NS_ERROR_FAILURE;
  return m_jobject->GetJavaPeer(result);
}

NS_IMETHODIMP
nsAppletHTMLObject::GetText(const char* *result)
{
  OJI_LOG("nsAppletHTMLObject::GetText");
  return NS_OK;
}
