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
 * $Id: nsJavaObjectInfo.cpp,v 1.2 2001/07/12 20:32:09 edburns%acm.org Exp $
 *
 * 
 * Contributor(s): 
 *
 *   Nikolay N. Igotti <inn@sparc.spb.su>
 */

#include "nsJavaObjectInfo.h"
#include "nsCOMPtr.h"
#include "nsIWFInstanceWrapper.h"
#include "nsIPluginInstance.h"
#include "wf_moz6_common.h"
#include <stdlib.h>

static NS_DEFINE_IID(kIPluginInstanceIID, NS_IPLUGININSTANCE_IID);
static NS_DEFINE_IID(kIWFInstanceWrapperIID, NS_IWFINSTANCEWRAPPER_IID);

static int GetParameters(void* handle, char*** keys, 
			 char*** vals, int* count)
{
  nsIJavaObjectInfo* obj = (nsIJavaObjectInfo*)handle;
  return obj->GetParameters(keys, vals, (PRUint32*)count);
}

static int ShowDocument(void* handle, char* url, char* target)
{
  nsresult res;
  nsIJavaObjectInfo* obj = (nsIJavaObjectInfo*)handle;
  nsCOMPtr<nsIWFInstanceWrapper> wrapper;
  if (NS_FAILED(res = obj->GetPluginWrapper(getter_AddRefs(wrapper))))
    return res;
  return (wrapper->ShowDocument(url, target));
}

static int ShowStatus(void* handle, char* status)
{
  nsresult res;
  nsIJavaObjectInfo* obj = (nsIJavaObjectInfo*)handle;
  nsCOMPtr<nsIWFInstanceWrapper> wrapper;
  if (NS_FAILED(res = obj->GetPluginWrapper(getter_AddRefs(wrapper))))
    return res;
  return (wrapper->ShowStatus(status));
}

static int GetJSThread(void* handle, jint* jstid)
{
  nsresult res;
  nsIJavaObjectInfo* obj = (nsIJavaObjectInfo*)handle;
  nsCOMPtr<nsIWFInstanceWrapper> wrapper;
  if (NS_FAILED(res = obj->GetPluginWrapper(getter_AddRefs(wrapper))))
    return res;
  return (wrapper->GetJSThread(jstid));
}

static int WrapperAddRef(JavaObjectWrapper* me)
{
  return 1;
}

static int WrapperRelease(JavaObjectWrapper* me)
{
  //if (--me->counter > 0) return me->counter;
  nsIPluginInstance* plugin = (nsIPluginInstance*)(me->plugin);
  if (plugin) NS_RELEASE(plugin);
  free(me);
  return 0;
}



NS_IMPL_ISUPPORTS1(nsJavaObjectInfo, nsIJavaObjectInfo)

nsJavaObjectInfo::nsJavaObjectInfo(nsISupports* instance)
{
  NS_INIT_ISUPPORTS();
  m_argc = 0;
  m_size = 5;
  m_step = 3;
  m_keys = (char**) malloc(m_size*sizeof(char*));
  m_vals = (char**) malloc(m_size*sizeof(char*));
  m_docbase = NULL;
  m_instance = instance;
  // backward compatibility with plugin API - fix when new API will appear
  if (NS_FAILED(m_instance->QueryInterface(kIWFInstanceWrapperIID, 
					   (void**)&m_jpiwrapper)))
    OJI_LOG("FAILED to get nsIWFInstanceWrapper");
  nsIPluginInstance* plugin = nsnull;
  if (NS_FAILED(m_instance->QueryInterface(kIPluginInstanceIID, 
					   (void**)&plugin)))
    OJI_LOG("FAILED to get nsIPluginInstance");
  /* member initializers and constructor code */
  m_wrapper = (JavaObjectWrapper*) malloc(sizeof(JavaObjectWrapper));
  m_wrapper->info = (void*)this;
  m_wrapper->plugin = (void*)plugin;
  m_wrapper->GetParameters = &::GetParameters;
  m_wrapper->ShowDocument  = &::ShowDocument;
  m_wrapper->ShowStatus    = &::ShowStatus;
  m_wrapper->GetJSThread   = &::GetJSThread;
  m_wrapper->dead          = 0;
}

nsJavaObjectInfo::~nsJavaObjectInfo()
{
  OJI_LOG("nsJavaObjectInfo::~nsJavaObjectInfo");
  for (PRUint32 i=0; i < m_argc; i++) 
    {
      if (m_keys[i]) free(m_keys[i]);
      if (m_vals[i]) free(m_vals[i]);
    }
  free(m_keys);
  free(m_vals);
  if (m_jpiwrapper) NS_RELEASE(m_jpiwrapper);
  WrapperRelease(m_wrapper);
  // if m_wrapper is hold by someone else, like Java native method
  // set field in wrapper that it cannot be used anymore and should be released
  //m_wrapper->dead = 1;
  //m_wrapper->Release(m_wrapper);
}

/* void addParameter (in string key, in string value); */
NS_IMETHODIMP 
nsJavaObjectInfo::AddParameter(const char *key, const char *value)
{
  OJI_LOG3("adding {%s} => {%s}", key, value);
  if (m_argc >= m_size) 
    {
      //OJI_LOG3("Enlarge  vector from %d to %d", m_size, m_size+m_step);
      m_size += m_step;
      m_keys = (char**) realloc(m_keys, m_size*sizeof(char*));
      m_vals = (char**) realloc(m_vals, m_size*sizeof(char*));
    }
  m_keys[m_argc] = strdup(key);
  m_vals[m_argc] = strdup(value);
  m_argc++; 
  return NS_OK;
}

/* void getParameters ([array, size_is (count)] out string keys, [array, size_is
 (count)] out string values, [retval] out PRUint32 count); */
NS_IMETHODIMP
nsJavaObjectInfo::GetParameters(char ***keys, char ***values, 
				PRUint32 *count)
{
  *keys = m_keys;
  *values = m_vals;
  *count = m_argc;
  return NS_OK;
}

/* attribute string docBase; */
NS_IMETHODIMP 
nsJavaObjectInfo::GetDocBase(char * *aDocBase)
{
  *aDocBase = m_docbase;
  return NS_OK;
}

NS_IMETHODIMP 
nsJavaObjectInfo::SetDocBase(const char * aDocBase)
{
  free(m_docbase);
  m_docbase = strdup(aDocBase);
  AddParameter("docbase", aDocBase);
  return NS_OK;
}

/* attribute string encoding; */
NS_IMETHODIMP 
nsJavaObjectInfo::GetEncoding(char * *aEncoding)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP 
nsJavaObjectInfo::SetEncoding(const char * aEncoding)
{
  AddParameter("encoding", aEncoding);
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute string alignment; */
NS_IMETHODIMP 
nsJavaObjectInfo::GetAlignment(char * *aAlignment)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP 
nsJavaObjectInfo::SetAlignment(const char * aAlignment)
{
  AddParameter("align", aAlignment);
  return NS_ERROR_NOT_IMPLEMENTED;
}

// ask peer
/* attribute PRUint32 width; */
NS_IMETHODIMP 
nsJavaObjectInfo::GetWidth(PRUint32 *aWidth)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP 
nsJavaObjectInfo::SetWidth(PRUint32 aWidth)
{
  //char buf[10];
  //snprintf(buf, 9, "%d", aWidth);
  //AddParameter("width", buf);
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute PRUint32 height; */
NS_IMETHODIMP 
nsJavaObjectInfo::GetHeight(PRUint32 *aHeight)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP 
nsJavaObjectInfo::SetHeight(PRUint32 aHeight)
{
  //char buf[10];
  //snprintf(buf, 9, "%d", aHeight);
  //AddParameter("height", buf);
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
nsJavaObjectInfo::SetType(nsJavaObjectType type)
{
  m_type = type;
  return NS_OK;
}


NS_IMETHODIMP 
nsJavaObjectInfo::GetType(nsJavaObjectType* type)
{
  *type = m_type;
  return NS_OK;
}

NS_IMETHODIMP
nsJavaObjectInfo::GetOwner(nsIJavaHTMLObject **owner)
{
  *owner = m_owner;
  NS_ADDREF(m_owner);
  return NS_OK;
}


NS_IMETHODIMP
nsJavaObjectInfo::GetWrapper(JavaObjectWrapper* *res) 
{
  *res = m_wrapper;
  return NS_OK;
}

NS_IMETHODIMP 
nsJavaObjectInfo::GetPluginWrapper(nsIWFInstanceWrapper* *wrapper)
{
  *wrapper = m_jpiwrapper;
  NS_ADDREF(m_jpiwrapper);
  return NS_OK;
}




