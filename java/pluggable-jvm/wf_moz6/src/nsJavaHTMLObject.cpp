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
 * $Id: nsJavaHTMLObject.cpp,v 1.1 2001/05/10 18:12:42 edburns%acm.org Exp $
 *
 * 
 * Contributor(s): 
 *
 *   Nikolay N. Igotti <inn@sparc.spb.su>
 */

#include "nsJavaHTMLObjectFactory.h"
#include "nsJavaHTMLObject.h"

NS_IMPL_ISUPPORTS1(nsJavaHTMLObject, nsIJavaHTMLObject)

nsJavaHTMLObject::nsJavaHTMLObject(nsJavaHTMLObjectFactory* f)
{
  NS_INIT_ISUPPORTS();
  m_factory = f;
  NS_ADDREF(f);
  m_id = 0;
  m_jvm = NULL;
  m_info = NULL;
}

nsJavaHTMLObject::~nsJavaHTMLObject()
{
  OJI_LOG("nsJavaHTMLObject::~nsJavaHTMLObject");
  if (m_factory) NS_RELEASE(m_factory);
}

/* [noscript] void SetWindow (in jp_jint ID); */
NS_IMETHODIMP nsJavaHTMLObject::SetWindow(jint ID)
{
  OJI_LOG2("nsJavaHTMLObject::SetWindow(%ld)", ID);
  if (m_id == 0)
    {
      OJI_LOG("SetWindow() for not inited obj!");
      return NS_ERROR_FAILURE;
    }
  nsresult res;
  if (ID == 0) return NS_OK;
  // this is part of generic policy to minimize time occupied in main thread
  // if this event will be proceed when user leaves page - nothing wrong, window
  // will be just unregistered and Java-side SetWindow() will fail
  // also it prevents possible deadlocks, when Java calls back to browser and 
  // browser main thread waits here and can't perform call
  if (NS_FAILED(res = PostEvent((jint)PE_SETWINDOW, (jlong)ID)))
    return res;
  return NS_OK;
}

/* void Initialize (in nsISupports peer, in nsIPluggableJVM jvm); */
NS_IMETHODIMP 
nsJavaHTMLObject::Initialize(nsISupports *peer, 
			     nsIPluggableJVM *jvm)
{
  nsresult res;
  m_instance_peer = peer;
  NS_ADDREF(m_instance_peer);
  m_jvm = jvm;
  NS_ADDREF(m_jvm);
  m_id = 0;
  if (NS_FAILED(res = m_jvm->CreateJavaPeer(this, 
					    WF_APPLETPEER_CID, 
					    &m_id)))
    return res;
  if (m_id == 0) return NS_ERROR_FAILURE;
  OJI_LOG2("nsJavaHTMLObject::Initialize: got id %d", (int)m_id);
  return NS_OK;
}

/* void Start (); */
NS_IMETHODIMP 
nsJavaHTMLObject::Start()
{
  nsresult res;
  if (NS_FAILED(res = PostEvent((jint)PE_START, (jlong)m_id)))
    return res;
  return NS_OK;
}

/* void Stop (); */
NS_IMETHODIMP 
nsJavaHTMLObject::Stop()
{
  nsresult res;
  if (NS_FAILED(res = PostEvent((jint)PE_STOP, (jlong)m_id)))
    return res;
  return NS_OK;
}

/* void Destroy (); */
NS_IMETHODIMP 
nsJavaHTMLObject::Destroy()
{
  nsresult res;
  if (!m_jvm) return NS_ERROR_FAILURE;
  if (NS_FAILED(res 
		= SendEvent((jint)PE_DESTROY, (jlong)m_id)))
    return res;
  if (NS_FAILED(res 
		= m_jvm->DestroyJavaPeer(m_id)))
    return res;
  //if (ID) m_jvm->UnregisterWindow(ID);
  if (m_instance_peer) NS_RELEASE(m_instance_peer);
  if (m_info) NS_RELEASE(m_info);
  return NS_OK;
}

/* void GetPeer (out nsISupports peer); */
NS_IMETHODIMP 
nsJavaHTMLObject::GetPeer(nsISupports **peer)
{
  *peer = m_instance_peer;
  NS_ADDREF(m_instance_peer);
  return NS_OK;
}

NS_IMETHODIMP 
nsJavaHTMLObject::SetJavaObjectInfo(nsIJavaObjectInfo* info)
{
  if (m_info) NS_RELEASE(m_info);
  m_info = info;
  NS_ADDREF(m_info);
  nsresult res;
  JavaObjectWrapper* wrapper;  
  if (NS_FAILED(res = m_info->GetWrapper(&wrapper)))
    return res;
  if (NS_FAILED(res = PostEvent((jint)PE_NEWPARAMS, PTR_TO_JLONG(wrapper))))
    return res;
  return NS_OK;
}

NS_IMETHODIMP 
nsJavaHTMLObject::GetJavaObjectInfo(nsIJavaObjectInfo* *info)
{
  *info = m_info;
  NS_ADDREF(m_info);
  return NS_OK;
}

NS_IMETHODIMP 
nsJavaHTMLObject::GetJavaPeer(jobject *pObj)
{
  nsresult res;
  OJI_LOG("nsJavaHTMLObject::GetJavaPeer");
  if (!pObj) return NS_ERROR_NULL_POINTER;
  *pObj = NULL;
  // both ways should be OK, but event seems to work better in generic case
  // as synchronized with other applet events
#if 0
  if (NS_FAILED(res = m_jvm->JavaPeerCall(m_id, 
					  PE_GETPEER, 
					  PTR_TO_JLONG(pObj))))
					  return res;
#else
  if (NS_FAILED(res = SendEvent(PE_GETPEER, 
				PTR_TO_JLONG(pObj)))) 
				return res;
#endif
  return NS_OK;
}

NS_IMETHODIMP 
nsJavaHTMLObject::SetType(jint type)
{
  nsresult res;
  if (NS_FAILED(res = SendEvent(PE_SETTYPE, (jlong)type)))
      return res;
  return NS_OK;
}


NS_IMETHODIMP 
nsJavaHTMLObject::GetDOMElement(nsIDOMElement* *result)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
nsJavaHTMLObject::PostEvent(jint id, jlong data)
{
  nsresult res;
  if (!m_jvm || !m_id) return NS_ERROR_FAILURE;
  if (NS_FAILED(res = m_jvm->PostEventToJavaPeer(m_id, id, data)))
    return res;
  return NS_OK;
}

NS_IMETHODIMP 
nsJavaHTMLObject::SendEvent(jint id, jlong data)
{
  nsresult res;
  if (!m_jvm || !m_id) return NS_ERROR_FAILURE;
  if (NS_FAILED(res = m_jvm->SendEventToJavaPeer(m_id, id, data, nsnull)))
     return res;
   return NS_OK;
}




