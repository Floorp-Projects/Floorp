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
#ifdef NECKO
#include "nsIIOService.h"
#include "nsIURL.h"
#include "nsIServiceManager.h"
#include "nsNeckoUtil.h"
static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);
#endif // NECKO
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
LocationImpl::SetURL(nsIURI* aURL)
{
  if (nsnull != mWebShell) {
#ifdef NECKO
    char* spec;
    aURL->GetSpec(&spec);
    nsAutoString s = spec;
    nsCRT::free(spec);
#else
    const char* spec;
    aURL->GetSpec(&spec);
    nsAutoString s = spec;
#endif
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
  nsIURI *uri;
  nsresult result = NS_OK;
  
  result = GetHref(href);
  if (NS_OK == result) {
#ifndef NECKO
    result = NS_NewURL(&uri, href);
#else
    result = NS_NewURI(&uri, href);
#endif // NECKO

    if (NS_OK == result) {
#ifdef NECKO
      char *ref;
      nsIURL* url;
      result = uri->QueryInterface(nsIURL::GetIID(), (void**)&url);
      if (NS_SUCCEEDED(result)) {
        result = url->GetRef(&ref);
        NS_RELEASE(url);
      }
#else
      const char *ref;
      result = uri->GetRef(&ref);
#endif
      if (result == NS_OK && (nsnull != ref) && ('\0' != *ref)) {
        aHash.SetString("#");
        aHash.Append(ref);
#ifdef NECKO
        nsCRT::free(ref);
#endif
      }
      else {
        aHash.SetLength(0);
      }
      NS_RELEASE(uri);
    }
  }

  return result;
}

NS_IMETHODIMP    
LocationImpl::SetHash(const nsString& aHash)
{
  nsAutoString href;
  nsIURI *uri;
  nsresult result = NS_OK;

  result = GetHref(href);
  if (NS_OK == result) {
#ifndef NECKO
    result = NS_NewURL(&uri, href);
    if (NS_OK == result) {
      char *buf = aHash.ToNewCString();
      uri->SetRef(buf);
      SetURL(uri);
      delete[] buf;
      NS_RELEASE(uri);      
    }
#else
    result = NS_NewURI(&uri, href);
    if (NS_FAILED(result)) return result;
    nsIURL* url;
    result = uri->QueryInterface(nsIURI::GetIID(), (void**)&url);
    NS_RELEASE(uri);
    if (NS_OK == result) {
      char *buf = aHash.ToNewCString();
      url->SetRef(buf);
      SetURL(url);
      delete[] buf;
      NS_RELEASE(url);      
    }
#endif // NECKO
  }

  return result;
}

NS_IMETHODIMP    
LocationImpl::GetHost(nsString& aHost)
{
  nsAutoString href;
  nsIURI *url;
  nsresult result = NS_OK;
  
  result = GetHref(href);
  if (NS_OK == result) {
#ifndef NECKO
    result = NS_NewURL(&url, href);
#else
    result = NS_NewURI(&url, href);
#endif // NECKO
    if (NS_OK == result) {
#ifdef NECKO
      char* host;
#else
      const char* host;
#endif
      result = url->GetHost(&host);
      if (result == NS_OK) {
        aHost.SetString(host);
#ifdef NECKO
        nsCRT::free(host);
        PRInt32 port;
        (void)url->GetPort(&port);
#else
        PRUint32 port;
        (void)url->GetHostPort(&port);
#endif
        if (-1 != port) {
          aHost.Append(":");
          aHost.Append(port, 10);
        }
      }
      NS_RELEASE(url);
    }
  }

  return result;
}

NS_IMETHODIMP    
LocationImpl::SetHost(const nsString& aHost)
{
  nsAutoString href;
  nsIURI *url;
  nsresult result = NS_OK;

  result = GetHref(href);
  if (NS_OK == result) {
#ifndef NECKO
    result = NS_NewURL(&url, href);
#else
    result = NS_NewURI(&url, href);
#endif // NECKO
    if (NS_OK == result) {
      char *buf = aHost.ToNewCString();
      url->SetHost(buf);
      SetURL(url);
      delete[] buf;
      NS_RELEASE(url);      
    }
  }

  return result;
}

NS_IMETHODIMP    
LocationImpl::GetHostname(nsString& aHostname)
{
  nsAutoString href;
  nsIURI *url;
  nsresult result = NS_OK;
  
  result = GetHref(href);
  if (NS_OK == result) {
#ifndef NECKO
    result = NS_NewURL(&url, href);
#else
    result = NS_NewURI(&url, href);
#endif // NECKO
    if (NS_OK == result) {
#ifdef NECKO
      char* host;
#else
      const char* host;
#endif
      result = url->GetHost(&host);
      if (result == NS_OK) {
        aHostname.SetString(host);
#ifdef NECKO
        nsCRT::free(host);
#endif
      }
      NS_RELEASE(url);
    }
  }

  return result;
}

NS_IMETHODIMP    
LocationImpl::SetHostname(const nsString& aHostname)
{
  nsAutoString href;
  nsIURI *url;
  nsresult result = NS_OK;

  result = GetHref(href);
  if (NS_OK == result) {
#ifndef NECKO
    result = NS_NewURL(&url, href);
#else
    result = NS_NewURI(&url, href);
#endif // NECKO
    if (NS_OK == result) {
      char *buf = aHostname.ToNewCString();
      url->SetHost(buf);
      SetURL(url);
      delete[] buf;
      NS_RELEASE(url);      
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
  nsIURI *oldUrl, *newUrl;
  nsresult result = NS_OK;

  result = GetHref(oldHref);
  if (NS_OK == result) {
#ifndef NECKO
    result = NS_NewURL(&oldUrl, oldHref);
#else
    result = NS_NewURI(&oldUrl, oldHref);
#endif // NECKO
    if (NS_OK == result) {
#ifndef NECKO
      result = NS_NewURL(&newUrl, aHref, oldUrl);
#else
      result = NS_NewURI(&newUrl, aHref, oldUrl);
#endif // NECKO
      if (NS_OK == result) {
#ifdef NECKO
        char* spec;
#else
        const char* spec;
#endif
        result = newUrl->GetSpec(&spec);
        if (NS_SUCCEEDED(result)) {
          newHref.SetString(spec);
#ifdef NECKO
          nsCRT::free(spec);
#endif
        }
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
  nsIURI *url;
  nsresult result = NS_OK;
  
  result = GetHref(href);
  if (NS_OK == result) {
#ifndef NECKO
    result = NS_NewURL(&url, href);
#else
    result = NS_NewURI(&url, href);
#endif // NECKO
    if (NS_OK == result) {
#ifdef NECKO
      char* file;
      result = url->GetPath(&file);
#else
      const char* file;
      result = url->GetFile(&file);
#endif
      if (result == NS_OK) {
        aPathname.SetString(file);
#ifdef NECKO
        nsCRT::free(file);
#endif
      }
      NS_IF_RELEASE(url);
    }
  }

  return result;
}

NS_IMETHODIMP    
LocationImpl::SetPathname(const nsString& aPathname)
{
  nsAutoString href;
  nsIURI *url;
  nsresult result = NS_OK;

  result = GetHref(href);
  if (NS_OK == result) {
#ifndef NECKO
    result = NS_NewURL(&url, href);
#else
    result = NS_NewURI(&url, href);
#endif // NECKO
    if (NS_OK == result) {
      char *buf = aPathname.ToNewCString();
#ifdef NECKO
      url->SetPath(buf);
#else
      url->SetFile(buf);
#endif
      SetURL(url);
      delete[] buf;
      NS_RELEASE(url);      
    }
  }

  return result;
}

NS_IMETHODIMP    
LocationImpl::GetPort(nsString& aPort)
{
  nsAutoString href;
  nsIURI *url;
  nsresult result = NS_OK;
  
  result = GetHref(href);
  if (NS_OK == result) {
#ifndef NECKO
    result = NS_NewURL(&url, href);
#else
    result = NS_NewURI(&url, href);
#endif // NECKO
    if (NS_OK == result) {
      aPort.SetLength(0);
#ifdef NECKO
      PRInt32 port;
      (void)url->GetPort(&port);
#else
      PRUint32 port;
      (void)url->GetHostPort(&port);
#endif
      if (-1 != port) {
        aPort.Append(port, 10);
      }
      NS_RELEASE(url);
    }
  }

  return result;
}

NS_IMETHODIMP    
LocationImpl::SetPort(const nsString& aPort)
{
  nsAutoString href;
  nsIURI *url;
  nsresult result = NS_OK;

  result = GetHref(href);
  if (NS_OK == result) {
#ifndef NECKO
    result = NS_NewURL(&url, href);
#else
    result = NS_NewURI(&url, href);
#endif // NECKO
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
#ifdef NECKO
      url->SetPort(port);
#else
      url->SetHostPort(port);
#endif
      SetURL(url);
      delete[] buf;
      NS_RELEASE(url);      
    }
  }

  return result;
}

NS_IMETHODIMP    
LocationImpl::GetProtocol(nsString& aProtocol)
{
  nsAutoString href;
  nsIURI *url;
  nsresult result = NS_OK;
  
  result = GetHref(href);
  if (NS_OK == result) {
#ifndef NECKO
    result = NS_NewURL(&url, href);
#else
    result = NS_NewURI(&url, href);
#endif // NECKO
    if (NS_OK == result) {
#ifdef NECKO
      char* protocol;
      result = url->GetScheme(&protocol);
#else
      const char* protocol;
      result = url->GetProtocol(&protocol);
#endif
      if (result == NS_OK) {
        aProtocol.SetString(protocol);
        aProtocol.Append(":");
#ifdef NECKO
        nsCRT::free(protocol);
#endif
      }
      NS_RELEASE(url);
    }
  }

  return result;
}

NS_IMETHODIMP    
LocationImpl::SetProtocol(const nsString& aProtocol)
{
  nsAutoString href;
  nsIURI *url;
  nsresult result = NS_OK;

  result = GetHref(href);
  if (NS_OK == result) {
#ifndef NECKO
    result = NS_NewURL(&url, href);
#else
    result = NS_NewURI(&url, href);
#endif // NECKO
    if (NS_OK == result) {
      char *buf = aProtocol.ToNewCString();
#ifdef NECKO
      url->SetScheme(buf);
#else
      url->SetProtocol(buf);
#endif
      SetURL(url);
      delete[] buf;
      NS_RELEASE(url);      
    }
  }

  return result;
}

NS_IMETHODIMP    
LocationImpl::GetSearch(nsString& aSearch)
{
  nsAutoString href;
  nsIURI *uri;
  nsresult result = NS_OK;

  result = GetHref(href);
  if (NS_OK == result) {
#ifndef NECKO
    result = NS_NewURL(&uri, href);
#else
    result = NS_NewURI(&uri, href);
#endif // NECKO
    if (NS_OK == result) {
#ifdef NECKO
      char *search;
      nsIURL* url;
      result = uri->QueryInterface(nsIURL::GetIID(), (void**)&url);
      if (NS_SUCCEEDED(result)) {
        result = url->GetQuery(&search);
        NS_RELEASE(url);
      }
#else
      const char *search;
      result = uri->GetSearch(&search);
#endif
      if (result == NS_OK && (nsnull != search) && ('\0' != *search)) {
        aSearch.SetString("?");
        aSearch.Append(search);
#ifdef NECKO
        nsCRT::free(search);
#endif
      }
      else {
        aSearch.SetLength(0);
      }
      NS_RELEASE(uri);
    }
  }

  return result;
}

NS_IMETHODIMP    
LocationImpl::SetSearch(const nsString& aSearch)
{
  nsAutoString href;
  nsIURI *uri;
  nsresult result = NS_OK;

  result = GetHref(href);
  if (NS_OK == result) {
#ifndef NECKO
    result = NS_NewURL(&uri, href);
#else
    result = NS_NewURI(&uri, href);
#endif // NECKO
    if (NS_OK == result) {
      char *buf = aSearch.ToNewCString();
#ifdef NECKO
      nsIURL* url;
      result = uri->QueryInterface(nsIURL::GetIID(), (void**)&url);
      if (NS_SUCCEEDED(result)) {
        result = url->SetQuery(buf);
        NS_RELEASE(url);
      }
#else
      result = uri->SetSearch(buf);
#endif
      SetURL(uri);
      delete[] buf;
      NS_RELEASE(uri);      
    }
  }

  return result;
}

NS_IMETHODIMP    
LocationImpl::Reload(JSContext *cx, jsval *argv, PRUint32 argc)
{
  nsresult result = NS_OK;

  if (nsnull != mWebShell) {
#ifdef NECKO
    result = mWebShell->Reload(nsIChannel::LOAD_NORMAL);
#else
    result = mWebShell->Reload(nsURLReload);
#endif
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

