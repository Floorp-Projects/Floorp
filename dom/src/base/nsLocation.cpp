/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsGlobalWindow.h"
#include "nsIWebShell.h"
#include "nsIURL.h"
#include "nsIIOService.h"
#include "nsIURL.h"
#include "nsIServiceManager.h"
#include "nsNeckoUtil.h"
static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);
#include "plstr.h"
#include "prmem.h"
#include "nsCOMPtr.h"
#include "nsJSUtils.h"
#include "nsIScriptSecurityManager.h"
#include "nsIScriptContextOwner.h"
#include "nsIJSContextStack.h"

static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIDOMLocationIID, NS_IDOMLOCATION_IID);
static NS_DEFINE_IID(kIDOMNSLocationIID, NS_IDOMNSLOCATION_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);

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
  if (aIID.Equals(kIDOMNSLocationIID)) {
    *aInstancePtrResult = (void*) ((nsIDOMNSLocation*)this);
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kIJSScriptObjectIID)) {
    *aInstancePtrResult = (void*)(nsISupports*)(nsIJSScriptObject*)this;
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
    nsCOMPtr<nsIScriptContextOwner> owner = do_QueryInterface(mWebShell);
    if (!owner) 
      return NS_ERROR_NO_INTERFACE;
    nsCOMPtr<nsIScriptGlobalObject> global;
    if (NS_FAILED(res = owner->GetScriptGlobalObject(getter_AddRefs(global))))
      return res;
    res = NS_NewScriptLocation(aContext, (nsISupports *)(nsIDOMLocation *)this, global, &mScriptObject);
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
LocationImpl::CheckURL(nsIURI* aURL)
{
  nsresult result;
  // Get JSContext from stack.
  NS_WITH_SERVICE(nsIJSContextStack, stack, "nsThreadJSContextStack", 
                  &result);
  if (NS_FAILED(result))
    return NS_ERROR_FAILURE;
  JSContext *cx;
  if (NS_FAILED(stack->Peek(&cx)))
    return NS_ERROR_FAILURE;

  // Get security manager.
  nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
  nsCOMPtr<nsIScriptSecurityManager> secMan;
  if (NS_FAILED(scriptCX->GetSecurityManager(getter_AddRefs(secMan))))
    return NS_ERROR_FAILURE;

  // Check to see if URI is allowed.
  PRBool ok = PR_FALSE;
  if (NS_FAILED(secMan->CheckURI(scriptCX, aURL, &ok)) || !ok) 
    return NS_ERROR_FAILURE;

  return NS_OK;
}


nsresult 
LocationImpl::SetURL(nsIURI* aURL)
{
  if (nsnull != mWebShell) {
    char* spec;
    aURL->GetSpec(&spec);
    nsAutoString s = spec;
    nsCRT::free(spec);

    if (NS_FAILED(CheckURL(aURL)))
      return NS_ERROR_FAILURE;

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
    result = NS_NewURI(&uri, href);

    if (NS_OK == result) {
      char *ref;
      nsIURL* url;
      result = uri->QueryInterface(nsIURL::GetIID(), (void**)&url);
      if (NS_SUCCEEDED(result)) {
        result = url->GetRef(&ref);
        NS_RELEASE(url);
      }
      if (result == NS_OK && (nsnull != ref) && ('\0' != *ref)) {
        aHash.SetString("#");
        aHash.Append(ref);
        nsCRT::free(ref);
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
    result = NS_NewURI(&uri, href);
    if (NS_FAILED(result)) return result;
    nsIURL* url;
    result = uri->QueryInterface(nsIURI::GetIID(), (void**)&url);
    NS_RELEASE(uri);
    if (NS_OK == result) {
      char *buf = aHash.ToNewCString();
      url->SetRef(buf);
      SetURL(url);
      nsCRT::free(buf);
      NS_RELEASE(url);      
    }
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
    result = NS_NewURI(&url, href);
    if (NS_OK == result) {
      char* host;
      result = url->GetHost(&host);
      if (result == NS_OK) {
        aHost.SetString(host);
        nsCRT::free(host);
        PRInt32 port;
        (void)url->GetPort(&port);
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
    result = NS_NewURI(&url, href);
    if (NS_OK == result) {
      char *buf = aHost.ToNewCString();
      url->SetHost(buf);
      SetURL(url);
      nsCRT::free(buf);
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
    result = NS_NewURI(&url, href);
    if (NS_OK == result) {
      char* host;
      result = url->GetHost(&host);
      if (result == NS_OK) {
        aHostname.SetString(host);
        nsCRT::free(host);
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
    result = NS_NewURI(&url, href);
    if (NS_OK == result) {
      char *buf = aHostname.ToNewCString();
      url->SetHost(buf);
      SetURL(url);
      nsCRT::free(buf);
      NS_RELEASE(url);      
    }
  }

  return result;
}

NS_IMETHODIMP    
LocationImpl::GetHref(nsString& aHref)
{
//  PRInt32 index;
  nsresult result = NS_OK;

  if (nsnull != mWebShell) {
    const PRUnichar *href;
    /* no need to use session history to get the url for the
     * current document. Fix until webshell's generic session history
     * is restored. S'd work even otherwise
     */
    //mWebShell->GetHistoryIndex(index);
    result = mWebShell->GetURL (&href);
    aHref = href;
  }

  return result;
}

NS_IMETHODIMP    
LocationImpl::SetHref(const nsString& aHref)
{
  nsAutoString oldHref;
  nsIURI *oldUrl;
  nsresult result = NS_OK;

  result = GetHref(oldHref);
  if (NS_OK == result) {
    result = NS_NewURI(&oldUrl, oldHref);
    if (NS_OK == result) {
      result = SetHrefWithBase(aHref, oldUrl, PR_TRUE);
      NS_RELEASE(oldUrl);
    }
  }

  return result;
}

nsresult
LocationImpl::SetHrefWithBase(const nsString& aHref, 
                              nsIURI* aBase,
                              PRBool aReplace)
{
  nsresult result;
  nsCOMPtr<nsIURI> newUrl;
  nsAutoString newHref;

  result = NS_NewURI(getter_AddRefs(newUrl), aHref, aBase);
  if (NS_OK == result) {
    char* spec;
    result = newUrl->GetSpec(&spec);
    if (NS_SUCCEEDED(result)) {
      newHref.SetString(spec);
      nsCRT::free(spec);
    }
  }

  if ((NS_OK == result) && (nsnull != mWebShell)) {

    if (NS_FAILED(CheckURL(newUrl)))
      return NS_ERROR_FAILURE;

    // Load new URI.
    result = mWebShell->LoadURL(newHref.GetUnicode(), nsnull, aReplace);
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
    result = NS_NewURI(&url, href);
    if (NS_OK == result) {
      char* file;
      result = url->GetPath(&file);
      if (result == NS_OK) {
        aPathname.SetString(file);
        nsCRT::free(file);
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
    result = NS_NewURI(&url, href);
    if (NS_OK == result) {
      char *buf = aPathname.ToNewCString();
      url->SetPath(buf);
      SetURL(url);
      nsCRT::free(buf);
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
    result = NS_NewURI(&url, href);
    if (NS_OK == result) {
      aPort.SetLength(0);
      PRInt32 port;
      (void)url->GetPort(&port);
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
    result = NS_NewURI(&url, href);
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
      url->SetPort(port);
      SetURL(url);
      nsCRT::free(buf);
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
    result = NS_NewURI(&url, href);
    if (NS_OK == result) {
      char* protocol;
      result = url->GetScheme(&protocol);
      if (result == NS_OK) {
        aProtocol.SetString(protocol);
        aProtocol.Append(":");
        nsCRT::free(protocol);
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
    result = NS_NewURI(&url, href);
    if (NS_OK == result) {
      char *buf = aProtocol.ToNewCString();
      url->SetScheme(buf);
      SetURL(url);
      nsCRT::free(buf);
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
    result = NS_NewURI(&uri, href);
    if (NS_OK == result) {
      char *search;
      nsIURL* url;
      result = uri->QueryInterface(nsIURL::GetIID(), (void**)&url);
      if (NS_SUCCEEDED(result)) {
        result = url->GetQuery(&search);
        NS_RELEASE(url);
      }
      if (result == NS_OK && (nsnull != search) && ('\0' != *search)) {
        aSearch.SetString("?");
        aSearch.Append(search);
        nsCRT::free(search);
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
    result = NS_NewURI(&uri, href);
    if (NS_OK == result) {
      char *buf = aSearch.ToNewCString();
      nsIURL* url;
      result = uri->QueryInterface(nsIURL::GetIID(), (void**)&url);
      if (NS_SUCCEEDED(result)) {
        result = url->SetQuery(buf);
        NS_RELEASE(url);
      }
      SetURL(uri);
      nsCRT::free(buf);
      NS_RELEASE(uri);      
    }
  }

  return result;
}

NS_IMETHODIMP    
LocationImpl::Reload(PRBool aForceget)
{
  nsresult result = NS_OK;

  if (nsnull != mWebShell) {
    result = mWebShell->Reload(nsIChannel::LOAD_NORMAL);
  }

  return result;
}

NS_IMETHODIMP    
LocationImpl::Replace(const nsString& aUrl)
{
  nsAutoString oldHref;
  nsIURI *oldUrl;
  nsresult result = NS_OK;

  result = GetHref(oldHref);
  if (NS_OK == result) {
    result = NS_NewURI(&oldUrl, oldHref);
    if (NS_OK == result) {
      result = SetHrefWithBase(aUrl, oldUrl, PR_FALSE);
      NS_RELEASE(oldUrl);
    }
  }

  return result;
}

NS_IMETHODIMP    
LocationImpl::Reload(JSContext *cx, jsval *argv, PRUint32 argc)
{
  // XXX Security manager needs to be called
  JSBool force = JS_FALSE;

  if (argc > 0) {
    JS_ValueToBoolean(cx, argv[0], &force);
  }

  return Reload(force ? PR_TRUE : PR_FALSE);
}

NS_IMETHODIMP    
LocationImpl::Replace(JSContext *cx, jsval *argv, PRUint32 argc)
{
  // XXX Security manager needs to be called
  nsresult result = NS_OK;

  if (argc > 0) {
    nsIURI* base;
    nsAutoString href;

    // Get the parameter passed in
    nsJSUtils::nsConvertJSValToString(href, cx, argv[0]);
    
    // Get the source of the caller
    result = GetSourceURL(cx, &base);
    
    if (NS_SUCCEEDED(result)) {
      result = SetHrefWithBase(href, base, PR_FALSE);
      NS_RELEASE(base);
    }
  }
  
  return result;
}

NS_IMETHODIMP    
LocationImpl::ToString(nsString& aReturn)
{
  return GetHref(aReturn);
}

nsresult
LocationImpl::GetSourceURL(JSContext* cx,
                           nsIURI** sourceURL)
{
  // XXX Code duplicated from nsHTMLDocument
  // XXX Tom said this reminded him of the "Six Degrees of
  // Kevin Bacon" game. We try to get from here to there using
  // whatever connections possible. The problem is that this
  // could break if any of the connections along the way change.
  // I wish there were a better way.
  nsresult result = NS_OK;
  nsIScriptContext* context = (nsIScriptContext*)JS_GetContextPrivate(cx);
  
  if (nsnull != context) {
    nsCOMPtr<nsIScriptGlobalObject> global;

    global = dont_AddRef(context->GetGlobalObject());
    if (global) {
      nsCOMPtr<nsIWebShell> webShell;
      
      global->GetWebShell(getter_AddRefs(webShell));
      if (webShell) {
        const PRUnichar* url;

        // XXX Ughh - incorrect ownership rules for url?
        webShell->GetURL(&url);
        result = NS_NewURI(sourceURL, url);
      }
    }
  }

  return result;
}

PRBool    
LocationImpl::AddProperty(JSContext *aContext, jsval aID, jsval *aVp)
{
  return JS_TRUE;
}

PRBool    
LocationImpl::DeleteProperty(JSContext *aContext, jsval aID, jsval *aVp)
{
  return JS_TRUE;
}

PRBool    
LocationImpl::GetProperty(JSContext *aContext, jsval aID, jsval *aVp)
{
  PRBool result = PR_TRUE;

  // XXX Security manager needs to be called
  if (JSVAL_IS_STRING(aID)) {
    char* cString = JS_GetStringBytes(JS_ValueToString(aContext, aID));
    if (PL_strcmp("href", cString) == 0) {
      nsAutoString href;

      if (NS_SUCCEEDED(GetHref(href))) {
        const PRUnichar* bytes = href.GetUnicode();
        JSString* str = JS_NewUCStringCopyZ(aContext, (const jschar*)bytes);
        if (str) {
          *aVp = STRING_TO_JSVAL(str);
        }
        else {
          result = PR_FALSE;
        }
      }
      else {
        result = PR_FALSE;
      }
    }
  }
  return result;
}

PRBool    
LocationImpl::SetProperty(JSContext *aContext, jsval aID, jsval *aVp)
{
  nsresult result = NS_OK;

  // XXX Security manager needs to be called
  if (JSVAL_IS_STRING(aID)) {
    char* cString = JS_GetStringBytes(JS_ValueToString(aContext, aID));
    
    if (PL_strcmp("href", cString) == 0) {
      nsIURI* base;
      nsAutoString href;
      
      // Get the parameter passed in
      nsJSUtils::nsConvertJSValToString(href, aContext, *aVp);
      
      // Get the source of the caller
      result = GetSourceURL(aContext, &base);
      
      if (NS_SUCCEEDED(result)) {
        result = SetHrefWithBase(href, base, PR_TRUE);
        NS_RELEASE(base);
      }
    }
  }

  return NS_SUCCEEDED(result);
}

PRBool    
LocationImpl::EnumerateProperty(JSContext *aContext)
{
  return JS_TRUE;
}

PRBool    
LocationImpl::Resolve(JSContext *aContext, jsval aID)
{
  return JS_TRUE;
}

PRBool    
LocationImpl::Convert(JSContext *aContext, jsval aID)
{
  return JS_TRUE;
}

void      
LocationImpl::Finalize(JSContext *aContext)
{
}


