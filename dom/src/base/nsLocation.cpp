/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsGlobalWindow.h"
#include "nsIWebShell.h"
#include "nsIURL.h"
#include "plstr.h"
#include "prmem.h"

static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIDOMLocationIID, NS_IDOMLOCATION_IID);

LocationImpl::LocationImpl(nsIWebShell *aWebShell)
{
  NS_INIT_REFCNT();
  mScriptObject = nsnull;
  mWebShell = aWebShell;
}
 
LocationImpl::~LocationImpl()
{
}

NS_IMPL_ADDREF(LocationImpl)
NS_IMPL_RELEASE(LocationImpl)

nsresult 
LocationImpl::QueryInterface(const nsIID& aIID,
                              void** aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null pointer");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kIScriptObjectOwnerIID)) {
    *aInstancePtrResult = (void*) ((nsIScriptObjectOwner*)this);
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kIDOMLocationIID)) {
    *aInstancePtrResult = (void*) ((nsIDOMLocation*)this);
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtrResult = (void*)(nsISupports*)(nsIScriptObjectOwner*)this;
    AddRef();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

nsresult 
LocationImpl::SetScriptObject(void *aScriptObject)
{
  mScriptObject = aScriptObject;
  return NS_OK;
}

nsresult 
LocationImpl::GetScriptObject(nsIScriptContext *aContext, void** aScriptObject)
{
  NS_PRECONDITION(nsnull != aScriptObject, "null arg");
  nsresult res = NS_OK;
  if (nsnull == mScriptObject) {
    nsIScriptGlobalObject *global = aContext->GetGlobalObject();
    res = NS_NewScriptLocation(aContext, (nsISupports *)(nsIDOMLocation *)this, global, &mScriptObject);
    NS_IF_RELEASE(global);
  }
  
  *aScriptObject = mScriptObject;
  return res;
}

NS_IMETHODIMP_(void)       
LocationImpl::SetWebShell(nsIWebShell *aWebShell)
{
  //mWebShell isn't refcnt'd here.  GlobalWindow calls SetWebShell(nsnull) 
  // when it's told that the WebShell is going to be deleted.
  mWebShell = aWebShell;
}

nsresult 
LocationImpl::SetURL(nsIURL* aURL)
{
  if (nsnull != mWebShell) {
    const char* spec;
    aURL->GetSpec(&spec);
    nsAutoString s = spec;
    return mWebShell->LoadURL(s.GetUnicode(), nsnull, PR_TRUE);
  }
  else {
    return NS_OK;
  }
}

NS_IMETHODIMP    
LocationImpl::GetHash(nsString& aHash)
{
  nsAutoString href;
  nsIURL *url;
  nsresult result = NS_OK;
  const char *ref;
  
  result = GetHref(href);
  if (NS_OK == result) {
    result = NS_NewURL(&url, href);
    if (NS_OK == result) {
      result = url->GetRef(&ref);
      if (result == NS_OK && (nsnull != ref) && ('\0' != *ref)) {
        aHash.SetString("#");
        aHash.Append(ref);
      }
      else {
        aHash.SetLength(0);
      }
      NS_IF_RELEASE(url);
    }
  }

  return result;
}

NS_IMETHODIMP    
LocationImpl::SetHash(const nsString& aHash)
{
  nsAutoString href;
  nsIURL *url;
  nsresult result = NS_OK;

  result = GetHref(href);
  if (NS_OK == result) {
    result = NS_NewURL(&url, href);
    if (NS_OK == result) {
      char *buf = aHash.ToNewCString();
      url->SetRef(buf);
      SetURL(url);
      delete[] buf;
      NS_IF_RELEASE(url);      
    }
  }

  return result;
}

NS_IMETHODIMP    
LocationImpl::GetHost(nsString& aHost)
{
  nsAutoString href;
  nsIURL *url;
  nsresult result = NS_OK;
  
  result = GetHref(href);
  if (NS_OK == result) {
    result = NS_NewURL(&url, href);
    if (NS_OK == result) {
      const char* host;
      result = url->GetHost(&host);
      if (result == NS_OK) {
        aHost.SetString(host);
        PRUint32 port;
        (void)url->GetHostPort(&port);
        if (-1 != port) {
          aHost.Append(":");
          aHost.Append(port, 10);
        }
      }
      NS_IF_RELEASE(url);
    }
  }

  return result;
}

NS_IMETHODIMP    
LocationImpl::SetHost(const nsString& aHost)
{
  nsAutoString href;
  nsIURL *url;
  nsresult result = NS_OK;

  result = GetHref(href);
  if (NS_OK == result) {
    result = NS_NewURL(&url, href);
    if (NS_OK == result) {
      char *buf = aHost.ToNewCString();
      url->SetHost(buf);
      SetURL(url);
      delete[] buf;
      NS_IF_RELEASE(url);      
    }
  }

  return result;
}

NS_IMETHODIMP    
LocationImpl::GetHostname(nsString& aHostname)
{
  nsAutoString href;
  nsIURL *url;
  nsresult result = NS_OK;
  
  result = GetHref(href);
  if (NS_OK == result) {
    result = NS_NewURL(&url, href);
    if (NS_OK == result) {
      const char* host;
      result = url->GetHost(&host);
      if (result == NS_OK)
        aHostname.SetString(host);
      NS_IF_RELEASE(url);
    }
  }

  return result;
}

NS_IMETHODIMP    
LocationImpl::SetHostname(const nsString& aHostname)
{
  nsAutoString href;
  nsIURL *url;
  nsresult result = NS_OK;

  result = GetHref(href);
  if (NS_OK == result) {
    result = NS_NewURL(&url, href);
    if (NS_OK == result) {
      char *buf = aHostname.ToNewCString();
      url->SetHost(buf);
      SetURL(url);
      delete[] buf;
      NS_IF_RELEASE(url);      
    }
  }

  return result;
}

NS_IMETHODIMP    
LocationImpl::GetHref(nsString& aHref)
{
  PRInt32 index;
  nsresult result = NS_OK;

  if (nsnull != mWebShell) {
    const PRUnichar *href;
    mWebShell->GetHistoryIndex(index);
    result = mWebShell->GetURL(index, &href);
    aHref = href;
  }

  return result;
}

NS_IMETHODIMP    
LocationImpl::SetHref(const nsString& aHref)
{
  nsAutoString oldHref, newHref;
  nsIURL *oldUrl, *newUrl;
  nsresult result = NS_OK;

  result = GetHref(oldHref);
  if (NS_OK == result) {
    result = NS_NewURL(&oldUrl, oldHref);
    if (NS_OK == result) {
      result = NS_NewURL(&newUrl, aHref, oldUrl);
      if (NS_OK == result) {
        const char* spec;
        result = newUrl->GetSpec(&spec);
        newHref.SetString(spec);
        NS_RELEASE(newUrl);
      }
      NS_RELEASE(oldUrl);
    }
  }

  if ((NS_OK == result) && (nsnull != mWebShell)) {
    return mWebShell->LoadURL(newHref.GetUnicode(), nsnull, PR_TRUE);
  }

  return result;
}

NS_IMETHODIMP    
LocationImpl::GetPathname(nsString& aPathname)
{
  nsAutoString href;
  nsIURL *url;
  nsresult result = NS_OK;
  
  result = GetHref(href);
  if (NS_OK == result) {
    result = NS_NewURL(&url, href);
    if (NS_OK == result) {
      const char* file;
      result = url->GetFile(&file);
      if (result == NS_OK)
        aPathname.SetString(file);
      NS_IF_RELEASE(url);
    }
  }

  return result;
}

NS_IMETHODIMP    
LocationImpl::SetPathname(const nsString& aPathname)
{
  nsAutoString href;
  nsIURL *url;
  nsresult result = NS_OK;

  result = GetHref(href);
  if (NS_OK == result) {
    result = NS_NewURL(&url, href);
    if (NS_OK == result) {
      char *buf = aPathname.ToNewCString();
      url->SetFile(buf);
      SetURL(url);
      delete[] buf;
      NS_IF_RELEASE(url);      
    }
  }

  return result;
}

NS_IMETHODIMP    
LocationImpl::GetPort(nsString& aPort)
{
  nsAutoString href;
  nsIURL *url;
  nsresult result = NS_OK;
  
  result = GetHref(href);
  if (NS_OK == result) {
    result = NS_NewURL(&url, href);
    if (NS_OK == result) {
      aPort.SetLength(0);
      PRUint32 port;
      (void)url->GetHostPort(&port);
      if (-1 != port) {
        aPort.Append(port, 10);
      }
      NS_IF_RELEASE(url);
    }
  }

  return result;
}

NS_IMETHODIMP    
LocationImpl::SetPort(const nsString& aPort)
{
  nsAutoString href;
  nsIURL *url;
  nsresult result = NS_OK;

  result = GetHref(href);
  if (NS_OK == result) {
    result = NS_NewURL(&url, href);
    if (NS_OK == result) {
      char *buf = aPort.ToNewCString();
      PRInt32 port = -1;

      if (buf) {
        if (*buf == ':') {
          port = atol(buf+1);
        }
        else {
          port = atol(buf);
        }
      }
      url->SetHostPort(port);
      SetURL(url);
      delete[] buf;
      NS_IF_RELEASE(url);      
    }
  }

  return result;
}

NS_IMETHODIMP    
LocationImpl::GetProtocol(nsString& aProtocol)
{
  nsAutoString href;
  nsIURL *url;
  nsresult result = NS_OK;
  
  result = GetHref(href);
  if (NS_OK == result) {
    result = NS_NewURL(&url, href);
    if (NS_OK == result) {
      const char* protocol;
      result = url->GetProtocol(&protocol);
      if (result == NS_OK) {
        aProtocol.SetString(protocol);
        aProtocol.Append(":");
      }
      NS_IF_RELEASE(url);
    }
  }

  return result;
}

NS_IMETHODIMP    
LocationImpl::SetProtocol(const nsString& aProtocol)
{
  nsAutoString href;
  nsIURL *url;
  nsresult result = NS_OK;

  result = GetHref(href);
  if (NS_OK == result) {
    result = NS_NewURL(&url, href);
    if (NS_OK == result) {
      char *buf = aProtocol.ToNewCString();
      url->SetProtocol(buf);
      SetURL(url);
      delete[] buf;
      NS_IF_RELEASE(url);      
    }
  }

  return result;
}

NS_IMETHODIMP    
LocationImpl::GetSearch(nsString& aSearch)
{
  nsAutoString href;
  nsIURL *url;
  nsresult result = NS_OK;

  result = GetHref(href);
  if (NS_OK == result) {
    result = NS_NewURL(&url, href);
    if (NS_OK == result) {
      const char *search;
      result = url->GetSearch(&search);
      if (result == NS_OK && (nsnull != search) && ('\0' != *search)) {
        aSearch.SetString("?");
        aSearch.Append(search);
      }
      else {
        aSearch.SetLength(0);
      }
      NS_IF_RELEASE(url);
    }
  }

  return result;
}

NS_IMETHODIMP    
LocationImpl::SetSearch(const nsString& aSearch)
{
  nsAutoString href;
  nsIURL *url;
  nsresult result = NS_OK;

  result = GetHref(href);
  if (NS_OK == result) {
    result = NS_NewURL(&url, href);
    if (NS_OK == result) {
      char *buf = aSearch.ToNewCString();
      url->SetSearch(buf);
      SetURL(url);
      delete[] buf;
      NS_IF_RELEASE(url);      
    }
  }

  return result;
}

NS_IMETHODIMP    
LocationImpl::Reload(JSContext *cx, jsval *argv, PRUint32 argc)
{
  nsresult result = NS_OK;

  if (nsnull != mWebShell) {
    result = mWebShell->Reload(nsURLReload);
  }

  return result;
}

NS_IMETHODIMP    
LocationImpl::Replace(const nsString& aUrl)
{
  if (nsnull != mWebShell) {
    return mWebShell->LoadURL(aUrl.GetUnicode(), nsnull, PR_FALSE);
  }

  return NS_OK;
}

NS_IMETHODIMP    
LocationImpl::ToString(nsString& aReturn)
{
  return GetHref(aReturn);
}

