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
 * $Id: nsWFSecurityContext.cpp,v 1.1 2001/05/10 18:12:45 edburns%acm.org Exp $
 *
 * 
 * Contributor(s): 
 *
 *   Nikolay N. Igotti <inn@sparc.spb.su>
 */

#include "nsWFSecurityContext.h"
#include "plstr.h"
#include "nsCRT.h"

NS_DEFINE_IID(kISecurityContextIID, NS_ISECURITYCONTEXT_IID);
NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

////////////////////////////////////////////////////////////////////////////
// from nsISupports and AggregatedQueryInterface:

// Thes macro expands to the aggregated query interface scheme.

NS_IMPL_AGGREGATED(nsWFSecurityContext);

NS_METHOD
nsWFSecurityContext::AggregatedQueryInterface(const nsIID& aIID, 
					      void** aInstancePtr)
{
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtr = GetInner();
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kISecurityContextIID)) {
    *aInstancePtr = (nsISecurityContext *)this;
    AddRef();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}



nsWFSecurityContext::nsWFSecurityContext(nsISupports *aOuter, 
					 nsPluggableJVM* pIJavaVM, 
					 jobject ctx,
					 const char* url): 
  m_jvm(pIJavaVM), context(NULL), m_url(NULL), m_olen(0)
{
    NS_INIT_AGGREGATED(aOuter);

    JNIEnv* env = nsnull;
    m_jvm->GetJNIEnv(&env);
    if (env) context = env->NewGlobalRef(ctx);
    if (url) { 
      m_url = nsCRT::strdup(url);
      char* s = m_url;
      int   slashes=0;
  
      while(s && *s)
	{
	  if (*s++ == '/') slashes++;	 
	  if (slashes == 3)  break;	
	  m_olen++;
	}
    }
    //fprintf(stderr, "url=%s\n", url);
}



nsWFSecurityContext::~nsWFSecurityContext()  
{
    JNIEnv* env = nsnull;
    m_jvm->GetJNIEnv(&env);

    if (env && context)
        env->DeleteGlobalRef(context);
    if (m_url) nsCRT::free(m_url);
}


NS_METHOD
nsWFSecurityContext::Create(nsISupports* outer, 
			    nsPluggableJVM* pIJavaVM,
			    jobject ctx, 
			    const char* url,
			    const nsIID& aIID, 
			    void* *aInstancePtr)
{
    if (outer && !aIID.Equals(kISupportsIID))
      return NS_NOINTERFACE;
    nsWFSecurityContext* context = 
      new nsWFSecurityContext(outer, pIJavaVM, ctx, url);
    if (context == NULL)
      return NS_ERROR_OUT_OF_MEMORY;
    context->AddRef();
    *aInstancePtr = context->GetInner();
    *aInstancePtr = (outer != NULL) ? 
      (void *)context->GetInner(): (void *)context;
    return NS_OK;
}

// XXX: not the best implementation, but this method never got called AFAIK,
// maybe beside LiveConnect with signed scripts, but it's not implemented 
// anyway.
NS_IMETHODIMP 
nsWFSecurityContext::Implies(const char* target, 
			     const char* action, 
			     PRBool* bActionAllowed)
{    
    OJI_LOG3("nsWFSecurityContext::Implies: %s %s",target, action);
    if (target == NULL || bActionAllowed == NULL)
        return NS_ERROR_NULL_POINTER;
    *bActionAllowed = PR_FALSE;
    return NS_OK;
}
   

NS_IMETHODIMP
nsWFSecurityContext::GetOrigin(char* buf, int len)
{
  int total =  m_olen > len ? len : m_olen;
  nsCRT::memcpy(buf, m_url, total);
  buf[total] = '\0';
  return NS_OK;
}

NS_IMETHODIMP
nsWFSecurityContext::GetCertificateID(char* buf, int len)
{
  //::PL_strncpy(buf, "", len);
  buf[0] = 0;
  return NS_OK;
}

