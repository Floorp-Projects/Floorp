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
 *   Travis Bogard <travis@netscape.com> 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

#include "nsGlobalWindow.h"
#include "nsIWebShell.h"
#include "nsIDocShell.h"
#include "nsIWebNavigation.h"
#include "nsIURL.h"
#include "nsIIOService.h"
#include "nsIURL.h"
#include "nsIServiceManager.h"
#include "nsNetUtil.h"
#include "plstr.h"
#include "prmem.h"
#include "nsCOMPtr.h"
#include "nsJSUtils.h"
#include "nsIScriptSecurityManager.h"
#include "nsICodebasePrincipal.h"
#include "nsIDOMWindow.h"
#include "nsIDOMDocument.h"
#include "nsIDocument.h"
#include "nsIJSContextStack.h"
#include "nsXPIDLString.h"
#include "nsDOMPropEnums.h"
#include "nsDOMError.h"

static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIDOMLocationIID, NS_IDOMLOCATION_IID);
static NS_DEFINE_IID(kIDOMNSLocationIID, NS_IDOMNSLOCATION_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);

LocationImpl::LocationImpl(nsIDocShell *aDocShell)
{
  NS_INIT_REFCNT();
  mScriptObject = nsnull;
  mDocShell = aDocShell; // Weak Reference
}
 
LocationImpl::~LocationImpl()
{
}

NS_IMPL_ADDREF(LocationImpl)
NS_IMPL_RELEASE(LocationImpl)

NS_INTERFACE_MAP_BEGIN(LocationImpl)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMLocation)
   NS_INTERFACE_MAP_ENTRY(nsIDOMLocation)
   NS_INTERFACE_MAP_ENTRY(nsIScriptObjectOwner)
   NS_INTERFACE_MAP_ENTRY(nsIDOMNSLocation)
   NS_INTERFACE_MAP_ENTRY(nsIJSScriptObject)
NS_INTERFACE_MAP_END

nsresult 
LocationImpl::SetScriptObject(void *aScriptObject)
{
  mScriptObject = aScriptObject;
  return NS_OK;
}

nsresult 
LocationImpl::GetScriptObject(nsIScriptContext *aContext, void** aScriptObject)
{
  NS_ENSURE_ARG_POINTER(aScriptObject);

  if (!mScriptObject) {
    nsCOMPtr<nsIScriptGlobalObject> global(do_GetInterface(mDocShell));
    NS_ENSURE_TRUE(global, NS_ERROR_FAILURE);
    NS_ENSURE_SUCCESS(NS_NewScriptLocation(aContext, 
      NS_STATIC_CAST(nsIDOMLocation*, this),global, &mScriptObject), 
      NS_ERROR_FAILURE);
  }
  *aScriptObject = mScriptObject;
  return NS_OK;
}

NS_IMETHODIMP_(void) LocationImpl::SetDocShell(nsIDocShell *aDocShell)
{
   mDocShell = aDocShell; // Weak Reference
}

nsresult 
LocationImpl::CheckURL(nsIURI* aURL, nsString &aReferrerResult)
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
  NS_WITH_SERVICE(nsIScriptSecurityManager, secMan,
                  NS_SCRIPTSECURITYMANAGER_PROGID, &result);
  if (NS_FAILED(result)) 
    return NS_ERROR_FAILURE;

  // Check to see if URI is allowed.
  if (NS_FAILED(result = secMan->CheckLoadURIFromScript(cx, aURL))) 
    return result;

  // Now get the referrer to use when loading the URI
  nsCOMPtr<nsIPrincipal> principal;
  if (NS_FAILED(secMan->GetSubjectPrincipal(getter_AddRefs(principal))))
    return NS_ERROR_FAILURE;
  nsCOMPtr<nsICodebasePrincipal> codebase = do_QueryInterface(principal);
  if (codebase) {
    nsCOMPtr<nsIURI> codebaseURI;
    if (NS_FAILED(result = codebase->GetURI(getter_AddRefs(codebaseURI))))
      return result;
    nsXPIDLCString spec;
    if (NS_FAILED(result = codebaseURI->GetSpec(getter_Copies(spec))))
      return result;
    aReferrerResult = spec;
  }

  return NS_OK;
}


nsresult 
LocationImpl::SetURL(nsIURI* aURL)
{
  if (mDocShell) {
    char* spec;
    aURL->GetSpec(&spec);
    nsAutoString s = spec;
    nsCRT::free(spec);

    nsAutoString referrer;
    if (NS_FAILED(CheckURL(aURL, referrer)))
      return NS_ERROR_FAILURE;

    nsCOMPtr<nsIWebShell> webShell(do_QueryInterface(mDocShell));
    return webShell->LoadURL(s.GetUnicode(), nsnull, PR_TRUE, 
                             nsIChannel::LOAD_NORMAL, 0, nsnull, 
                             referrer.Length() > 0
                             ? referrer.GetUnicode()
                             : nsnull);
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
      result = uri->QueryInterface(NS_GET_IID(nsIURL), (void**)&url);
      if (NS_SUCCEEDED(result)) {
        result = url->GetRef(&ref);
        NS_RELEASE(url);
      }
      if (result == NS_OK && (nsnull != ref) && ('\0' != *ref)) {
        aHash.Assign("#");
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
    result = uri->QueryInterface(NS_GET_IID(nsIURI), (void**)&url);
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
        aHost.Assign(host);
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
        aHostname.Assign(host);
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
  nsresult result = NS_OK;

  if (mDocShell) {
    nsCOMPtr<nsIURI> uri;
    result = mDocShell->GetCurrentURI(getter_AddRefs(uri));
    if (NS_SUCCEEDED(result)) {
        nsXPIDLCString uriString;
        result = uri->GetSpec(getter_Copies(uriString));
        if (NS_SUCCEEDED(result))
            aHref = uriString;
    }
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
      newHref.Assign(spec);
      nsCRT::free(spec);
    }
  }

  if ((NS_OK == result) && (mDocShell)) {

    nsAutoString referrer;
    if (NS_FAILED(CheckURL(newUrl, referrer)))
      return NS_ERROR_FAILURE;

    // Load new URI.
    nsCOMPtr<nsIWebShell> webShell(do_QueryInterface(mDocShell));
    result = webShell->LoadURL(newHref.GetUnicode(), nsnull, aReplace, 
                               nsIChannel::LOAD_NORMAL, 0, nsnull, 
                               referrer.Length() > 0
                               ? referrer.GetUnicode()
                               : nsnull);
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
        aPathname.Assign(file);
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
        aProtocol.Assign(protocol);
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
      result = uri->QueryInterface(NS_GET_IID(nsIURL), (void**)&url);
      if (NS_SUCCEEDED(result)) {
        result = url->GetQuery(&search);
        NS_RELEASE(url);
      }
      if (result == NS_OK && (nsnull != search) && ('\0' != *search)) {
        aSearch.Assign("?");
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
      result = uri->QueryInterface(NS_GET_IID(nsIURL), (void**)&url);
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
   nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(mDocShell));
   NS_ENSURE_TRUE(webNav, NS_ERROR_FAILURE);

   NS_ENSURE_SUCCESS(webNav->Reload(nsIWebNavigation::reloadNormal), 
      NS_ERROR_FAILURE);

   return NS_OK;
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

  nsresult result = NS_ERROR_FAILURE;
      
  // We need to use the dynamically scoped global and assume that the 
  // current JSContext is a DOM context with a nsIScriptGlobalObject so
  // that we can get the url of the caller.
  // XXX This will fail on non-DOM contexts :(

  nsCOMPtr<nsIScriptGlobalObject> nativeGlob;
  nsJSUtils::nsGetDynamicScriptGlobal(cx, getter_AddRefs(nativeGlob));
  if (nativeGlob) {
    nsCOMPtr<nsIDOMWindow> window = do_QueryInterface(nativeGlob);

    if (window) {
      nsCOMPtr<nsIDOMDocument> domDoc;

      result = window->GetDocument(getter_AddRefs(domDoc));
      if (NS_SUCCEEDED(result)) {
        nsCOMPtr<nsIDocument> doc = do_QueryInterface(domDoc);

        if (doc) {
          result = doc->GetBaseURL(*sourceURL);
        }

        if (!*sourceURL) {
          *sourceURL = doc->GetDocumentURL();
        }
      }
    }
  }

  return result;
}

PRBool    
LocationImpl::AddProperty(JSContext *aContext, JSObject *aObj, jsval aID, jsval *aVp)
{
  return JS_TRUE;
}

PRBool    
LocationImpl::DeleteProperty(JSContext *aContext, JSObject *aObj, jsval aID, jsval *aVp)
{
  return JS_TRUE;
}

static nsresult
CheckHrefAccess(JSContext *aContext, JSObject *aObj, PRBool isWrite) 
{
  nsresult rv;
  NS_WITH_SERVICE(nsIScriptSecurityManager, secMan,
                  NS_SCRIPTSECURITYMANAGER_PROGID, &rv);
  if (NS_FAILED(rv))
    rv = NS_ERROR_DOM_SECMAN_ERR;
  else
    rv = secMan->CheckScriptAccess(aContext, aObj, NS_DOM_PROP_LOCATION_HREF,
                                   isWrite);
  if (NS_FAILED(rv)) {
    nsJSUtils::nsReportError(aContext, aObj, rv);
    return rv;
  }
  return NS_OK;
}

PRBool    
LocationImpl::GetProperty(JSContext *aContext, JSObject *aObj, jsval aID, jsval *aVp)
{
  PRBool result = PR_TRUE;

  if (JSVAL_IS_STRING(aID)) {
    char* cString = JS_GetStringBytes(JS_ValueToString(aContext, aID));
    if (PL_strcmp("href", cString) == 0) {
      nsAutoString href;

    if (NS_SUCCEEDED(CheckHrefAccess(aContext, aObj, PR_FALSE)) &&
        NS_SUCCEEDED(GetHref(href))) 
    {
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
LocationImpl::SetProperty(JSContext *aContext, JSObject *aObj, jsval aID, jsval *aVp)
{
  nsresult result = NS_OK;

  if (JSVAL_IS_STRING(aID)) {
    char* cString = JS_GetStringBytes(JS_ValueToString(aContext, aID));
    
    if (PL_strcmp("href", cString) == 0) {
      nsIURI* base;
      nsAutoString href;
      
      if (NS_FAILED(CheckHrefAccess(aContext, aObj, PR_TRUE)))
        return PR_FALSE;

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
LocationImpl::EnumerateProperty(JSContext *aContext, JSObject *aObj)
{
  return JS_TRUE;
}

PRBool    
LocationImpl::Resolve(JSContext *aContext, JSObject *aObj, jsval aID)
{
  return JS_TRUE;
}

PRBool    
LocationImpl::Convert(JSContext *aContext, JSObject *aObj, jsval aID)
{
  return JS_TRUE;
}

void      
LocationImpl::Finalize(JSContext *aContext, JSObject *aObj)
{
}


