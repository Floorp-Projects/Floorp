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
#include "nsIDocShellLoadInfo.h"
#include "nsIWebNavigation.h"
#include "nsIURL.h"
#include "nsIIOService.h"
#include "nsIServiceManager.h"
#include "nsNetUtil.h"
#include "plstr.h"
#include "prprf.h"
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
LocationImpl::CheckURL(nsIURI* aURI, nsIDocShellLoadInfo** aLoadInfo)
{
  nsresult result;
  // Get JSContext from stack.
  nsCOMPtr<nsIJSContextStack>
    stack(do_GetService("@mozilla.org/js/xpc/ContextStack;1", &result));

  if (NS_FAILED(result))
    return NS_ERROR_FAILURE;

  JSContext *cx;

  if (NS_FAILED(stack->Peek(&cx)))
    return NS_ERROR_FAILURE;

  // Get security manager.
  nsCOMPtr<nsIScriptSecurityManager>
    secMan(do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &result));

  if (NS_FAILED(result))
    return NS_ERROR_FAILURE;

  // Check to see if URI is allowed.
  result = secMan->CheckLoadURIFromScript(cx, aURI);

  if (NS_FAILED(result))
    return result;

  // Create load info
  nsCOMPtr<nsIDocShellLoadInfo> loadInfo;
  mDocShell->CreateLoadInfo(getter_AddRefs(loadInfo));
  NS_ENSURE_TRUE(loadInfo, NS_ERROR_FAILURE);

  // Now get the principal to use when loading the URI
  nsCOMPtr<nsIPrincipal> principal;
  if (NS_FAILED(secMan->GetSubjectPrincipal(getter_AddRefs(principal))) ||
      !principal)
    return NS_ERROR_FAILURE;
  nsCOMPtr<nsISupports> owner = do_QueryInterface(principal);
  loadInfo->SetOwner(owner);

  *aLoadInfo = loadInfo.get();
  NS_ADDREF(*aLoadInfo);

  return NS_OK;
}


nsresult
LocationImpl::SetURL(nsIURI* aURI)
{
  if (mDocShell) {
    nsCOMPtr<nsIDocShellLoadInfo> loadInfo;

    if(NS_FAILED(CheckURL(aURI, getter_AddRefs(loadInfo))))
      return NS_ERROR_FAILURE;

    loadInfo->SetStopActiveDocument(PR_TRUE);

    return mDocShell->LoadURI(aURI, loadInfo,
                              nsIWebNavigation::LOAD_FLAGS_NONE);
  }

  return NS_OK;
}

NS_IMETHODIMP
LocationImpl::GetHash(nsAWritableString& aHash)
{
  nsAutoString href;
  nsresult result = NS_OK;

  result = GetHref(href);

  if (NS_SUCCEEDED(result)) {
    nsCOMPtr<nsIURI> uri;

    result = NS_NewURI(getter_AddRefs(uri), href);

    if (uri) {
      nsCOMPtr<nsIURL> url(do_QueryInterface(uri));
      nsXPIDLCString ref;

      if (url) {
        result = url->GetRef(getter_Copies(ref));
      }

      if (NS_SUCCEEDED(result) && ref && *ref) {
        aHash.Assign(NS_LITERAL_STRING("#"));
        aHash.Append(NS_ConvertASCIItoUCS2(ref));
      }
      else {
        aHash.SetLength(0);
      }
    }
  }

  return result;
}

NS_IMETHODIMP
LocationImpl::SetHash(const nsAReadableString& aHash)
{
  nsAutoString href;
  nsresult result = NS_OK;

  result = GetHref(href);

  if (NS_SUCCEEDED(result)) {
    nsCOMPtr<nsIURI> uri;

    result = NS_NewURI(getter_AddRefs(uri), href);

    if (NS_FAILED(result))
      return result;

    nsCOMPtr<nsIURL> url(do_QueryInterface(uri, &result));

    if (url) {
      url->SetRef(NS_ConvertUCS2toUTF8(aHash));

      SetURL(url);
    }
  }

  return result;
}

NS_IMETHODIMP
LocationImpl::GetHost(nsAWritableString& aHost)
{
  nsAutoString href;
  nsresult result = NS_OK;

  result = GetHref(href);

  if (NS_SUCCEEDED(result)) {
    nsCOMPtr<nsIURI> uri;

    result = NS_NewURI(getter_AddRefs(uri), href);

    if (uri) {
      nsXPIDLCString host;

      result = uri->GetHost(getter_Copies(host));

      if (NS_SUCCEEDED(result)) {
        PRInt32 port;

        CopyASCIItoUCS2(nsLiteralCString(host), aHost);

        uri->GetPort(&port);

        if (port != -1) {
          aHost.Append(PRUnichar(':'));

          nsAutoString tmpHost;
          tmpHost.AppendInt(port);

          aHost.Append(tmpHost);
        }
      }
    }
  }

  return result;
}

NS_IMETHODIMP
LocationImpl::SetHost(const nsAReadableString& aHost)
{
  nsAutoString href;
  nsresult result = NS_OK;

  result = GetHref(href);

  if (NS_SUCCEEDED(result)) {
    nsCOMPtr<nsIURI> uri;

    result = NS_NewURI(getter_AddRefs(uri), href);

    if (uri) {
      uri->SetHost(NS_ConvertUCS2toUTF8(aHost));

      SetURL(uri);
    }
  }

  return result;
}

NS_IMETHODIMP
LocationImpl::GetHostname(nsAWritableString& aHostname)
{
  nsAutoString href;
  nsresult result = NS_OK;

  result = GetHref(href);

  if (NS_SUCCEEDED(result)) {
    nsCOMPtr<nsIURI> uri;

    result = NS_NewURI(getter_AddRefs(uri), href);

    if (uri) {
      nsXPIDLCString host;

      result = uri->GetHost(getter_Copies(host));

      if (NS_SUCCEEDED(result)) {
        CopyASCIItoUCS2(nsLiteralCString(host), aHostname);
      }
    }
  }

  return result;
}

NS_IMETHODIMP
LocationImpl::SetHostname(const nsAReadableString& aHostname)
{
  nsAutoString href;
  nsresult result = NS_OK;

  result = GetHref(href);

  if (NS_SUCCEEDED(result)) {
    nsCOMPtr<nsIURI> uri;

    result = NS_NewURI(getter_AddRefs(uri), href);

    if (uri) {
      uri->SetHost(NS_ConvertUCS2toUTF8(aHostname));
      SetURL(uri);
    }
  }

  return result;
}

NS_IMETHODIMP
LocationImpl::GetHref(nsAWritableString& aHref)
{
  nsresult result = NS_OK;

  nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(mDocShell));

  if (webNav) {
    nsCOMPtr<nsIURI> uri;

    result = webNav->GetCurrentURI(getter_AddRefs(uri));

    if (NS_SUCCEEDED(result) && uri) {
      nsXPIDLCString uriString;

      result = uri->GetSpec(getter_Copies(uriString));

      if (NS_SUCCEEDED(result))
        CopyASCIItoUCS2(nsLiteralCString(uriString), aHref);
    }
  }

  return result;
}

NS_IMETHODIMP
LocationImpl::SetHref(const nsAReadableString& aHref)
{
  nsAutoString oldHref;
  nsresult result = NS_OK;

  result = GetHref(oldHref);

  if (NS_SUCCEEDED(result)) {
    nsCOMPtr<nsIURI> oldUri;

    result = NS_NewURI(getter_AddRefs(oldUri), oldHref);

    if (oldUri) {
      result = SetHrefWithBase(aHref, oldUri, PR_FALSE);
    }
  }

  return result;
}

nsresult
LocationImpl::SetHrefWithContext(JSContext* cx, jsval val)
{
  nsCOMPtr<nsIURI> base;
  nsAutoString href;

  // Get the parameter passed in
  nsJSUtils::nsConvertJSValToString(href, cx, val);

  // Get the source of the caller
  nsresult result = GetSourceURL(cx, getter_AddRefs(base));

  if (NS_FAILED(result)) {
    return result;
  }

  return SetHrefWithBase(href, base, PR_FALSE);
}

nsresult
LocationImpl::SetHrefWithBase(const nsAReadableString& aHref,
                              nsIURI* aBase, PRBool aReplace)
{
  nsresult result;
  nsCOMPtr<nsIURI> newUri;

  result = NS_NewURI(getter_AddRefs(newUri), aHref, aBase);

  if (newUri && mDocShell) {
    nsCOMPtr<nsIDocShellLoadInfo> loadInfo;

    nsresult rv = CheckURL(newUri, getter_AddRefs(loadInfo));

    if(NS_FAILED(rv))
      return rv;

    if (aReplace) {
      loadInfo->SetLoadType(nsIDocShellLoadInfo::loadNormalReplace);
    }

    loadInfo->SetStopActiveDocument(PR_TRUE);

    return mDocShell->LoadURI(newUri, loadInfo,
                              nsIWebNavigation::LOAD_FLAGS_NONE);
  }

  return result;
}

NS_IMETHODIMP
LocationImpl::GetPathname(nsAWritableString& aPathname)
{
  nsAutoString href;
  nsresult result = NS_OK;

  aPathname.Truncate();

  result = GetHref(href);

  if (NS_SUCCEEDED(result)) {
    nsCOMPtr<nsIURI> uri;

    result = NS_NewURI(getter_AddRefs(uri), href);

    if (uri) {
      nsCOMPtr<nsIURL> url(do_QueryInterface(uri));

      if (url) {
        nsXPIDLCString file;
        result = url->GetFilePath(getter_Copies(file));

        if (NS_SUCCEEDED(result)) {
          CopyASCIItoUCS2(nsLiteralCString(file), aPathname);
        }
      }
    }
  }

  return result;
}

NS_IMETHODIMP
LocationImpl::SetPathname(const nsAReadableString& aPathname)
{
  nsAutoString href;
  nsresult result = NS_OK;

  result = GetHref(href);

  if (NS_SUCCEEDED(result)) {
    nsCOMPtr<nsIURI> uri;

    result = NS_NewURI(getter_AddRefs(uri), href);

    if (uri) {
      uri->SetPath(NS_ConvertUCS2toUTF8(aPathname));
      SetURL(uri);
    }
  }

  return result;
}

NS_IMETHODIMP
LocationImpl::GetPort(nsAWritableString& aPort)
{
  nsAutoString href;
  nsresult result = NS_OK;

  result = GetHref(href);

  if (NS_SUCCEEDED(result)) {
    nsCOMPtr<nsIURI> uri;

    result = NS_NewURI(getter_AddRefs(uri), href);

    if (uri) {
      aPort.SetLength(0);

      PRInt32 port;
      uri->GetPort(&port);

      if (-1 != port) {
        nsAutoString portStr;
        portStr.AppendInt(port);
        aPort.Append(portStr);
      }
    }
  }

  return result;
}

NS_IMETHODIMP
LocationImpl::SetPort(const nsAReadableString& aPort)
{
  nsAutoString href;
  nsresult result = NS_OK;

  result = GetHref(href);

  if (NS_SUCCEEDED(result)) {
    nsCOMPtr<nsIURI> uri;

    result = NS_NewURI(getter_AddRefs(uri), href);

    if (uri) {
      const char *buf = NS_ConvertUCS2toUTF8(aPort).GetBuffer();
      PRInt32 port = -1;

      if (buf) {
        if (*buf == ':') {
          port = atol(buf+1);
        }
        else {
          port = atol(buf);
        }
      }
      uri->SetPort(port);
      SetURL(uri);
    }
  }

  return result;
}

NS_IMETHODIMP
LocationImpl::GetProtocol(nsAWritableString& aProtocol)
{
  nsAutoString href;
  nsresult result = NS_OK;

  result = GetHref(href);

  if (NS_SUCCEEDED(result)) {
    nsCOMPtr<nsIURI> uri;

    result = NS_NewURI(getter_AddRefs(uri), href);

    if (uri) {
      nsXPIDLCString protocol;

      result = uri->GetScheme(getter_Copies(protocol));

      if (NS_SUCCEEDED(result)) {
        aProtocol.Assign(NS_ConvertASCIItoUCS2(protocol));
        aProtocol.Append(PRUnichar(':'));
      }
    }
  }

  return result;
}

NS_IMETHODIMP
LocationImpl::SetProtocol(const nsAReadableString& aProtocol)
{
  nsAutoString href;
  nsresult result = NS_OK;

  result = GetHref(href);

  if (NS_SUCCEEDED(result)) {
    nsCOMPtr<nsIURI> uri;

    result = NS_NewURI(getter_AddRefs(uri), href);

    if (uri) {
      uri->SetScheme(NS_ConvertUCS2toUTF8(aProtocol));
      SetURL(uri);
    }
  }

  return result;
}

NS_IMETHODIMP
LocationImpl::GetSearch(nsAWritableString& aSearch)
{
  nsAutoString href;
  nsresult result = NS_OK;

  result = GetHref(href);

  if (NS_SUCCEEDED(result)) {
    nsCOMPtr<nsIURI> uri;

    result = NS_NewURI(getter_AddRefs(uri), href);

    if (uri) {
      nsXPIDLCString search;
      nsCOMPtr<nsIURL> url(do_QueryInterface(uri));

      if (url) {
        result = url->GetEscapedQuery(getter_Copies(search));
      }

      if (NS_SUCCEEDED(result) && search && *search) {
        aSearch.Assign(NS_LITERAL_STRING("?"));
        aSearch.Append(NS_ConvertASCIItoUCS2(search));
      }
      else {
        aSearch.SetLength(0);
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
LocationImpl::SetSearch(const nsAReadableString& aSearch)
{
  nsAutoString href;
  nsresult result = NS_OK;

  result = GetHref(href);

  if (NS_SUCCEEDED(result)) {
    nsCOMPtr<nsIURI> uri;

    result = NS_NewURI(getter_AddRefs(uri), href);

    if (uri) {
      nsCOMPtr<nsIURL> url(do_QueryInterface(uri, &result));

      if (url) {
        result = url->SetQuery(NS_ConvertUCS2toUTF8(aSearch));

        SetURL(uri);
      }
    }
  }

  return result;
}

NS_IMETHODIMP
LocationImpl::Reload(PRBool aForceget)
{
  nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(mDocShell));
  NS_ENSURE_TRUE(webNav, NS_ERROR_FAILURE);

  NS_ENSURE_SUCCESS(webNav->Reload(nsIWebNavigation::LOAD_FLAGS_NONE),
                    NS_ERROR_FAILURE);

  return NS_OK;
}

NS_IMETHODIMP
LocationImpl::Replace(const nsAReadableString& aUrl)
{
  nsAutoString oldHref;
  nsresult result = NS_OK;

  result = GetHref(oldHref);

  if (NS_SUCCEEDED(result)) {
    nsCOMPtr<nsIURI> oldUri;

    result = NS_NewURI(getter_AddRefs(oldUri), oldHref);

    if (oldUri) {
      result = SetHrefWithBase(aUrl, oldUri, PR_TRUE);
    }
  }

  return result;
}

NS_IMETHODIMP
LocationImpl::Assign(const nsAReadableString& aUrl)
{
  nsAutoString oldHref;
  nsresult result = NS_OK;

  result = GetHref(oldHref);

  if (NS_SUCCEEDED(result)) {
    nsCOMPtr<nsIURI> oldUri;

    result = NS_NewURI(getter_AddRefs(oldUri), oldHref);

    if (oldUri) {
      result = SetHrefWithBase(aUrl, oldUri, PR_FALSE);
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
    nsCOMPtr<nsIURI> base;
    nsAutoString href;

    // Get the parameter passed in
    nsJSUtils::nsConvertJSValToString(href, cx, argv[0]);

    // Get the source of the caller
    result = GetSourceURL(cx, getter_AddRefs(base));

    if (NS_SUCCEEDED(result)) {
      result = SetHrefWithBase(href, base, PR_TRUE);
    }
  }

  return result;
}

NS_IMETHODIMP
LocationImpl::ToString(nsAWritableString& aReturn)
{
  return GetHref(aReturn);
}

nsresult
LocationImpl::GetSourceURL(JSContext* cx,
                           nsIURI** sourceURI)
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
        nsCOMPtr<nsIDocument> doc(do_QueryInterface(domDoc));

        if (doc) {
          result = doc->GetBaseURL(*sourceURI);
        }

        if (!*sourceURI) {
          *sourceURI = doc->GetDocumentURL();
        }
      }
    }
  }

  return result;
}

PRBool
LocationImpl::AddProperty(JSContext *aContext, JSObject *aObj, jsval aID,
                          jsval *aVp)
{
  return JS_TRUE;
}

PRBool
LocationImpl::DeleteProperty(JSContext *aContext, JSObject *aObj, jsval aID,
                             jsval *aVp)
{
  return JS_TRUE;
}

static nsresult
CheckHrefAccess(JSContext *aContext, JSObject *aObj, PRBool isWrite)
{
  nsresult rv;

  nsCOMPtr<nsIScriptSecurityManager>
    secMan(do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv));

  if (NS_FAILED(rv)) {
    rv = NS_ERROR_DOM_SECMAN_ERR;
  } else {
    rv = secMan->CheckScriptAccess(aContext, aObj, NS_DOM_PROP_LOCATION_HREF,
                                   isWrite);
  }

  if (NS_FAILED(rv)) {
    nsJSUtils::nsReportError(aContext, aObj, rv);
  }

  return rv;
}

PRBool
LocationImpl::GetProperty(JSContext *aContext, JSObject *aObj, jsval aID,
                          jsval *aVp)
{
  PRBool result = PR_TRUE;

  if (JSVAL_IS_STRING(aID)) {
    const PRUnichar *prop = NS_REINTERPRET_CAST(const PRUnichar *,
      JS_GetStringChars(JS_ValueToString(aContext, aID)));

    if (NS_LITERAL_STRING("href").Equals(prop)) {
      nsAutoString href;

      if (NS_SUCCEEDED(CheckHrefAccess(aContext, aObj, PR_FALSE)) &&
          NS_SUCCEEDED(GetHref(href))) {
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
LocationImpl::SetProperty(JSContext *aContext, JSObject *aObj, jsval aID,
                          jsval *aVp)
{
  nsresult result = NS_OK;

  if (JSVAL_IS_STRING(aID)) {
    const PRUnichar *prop = NS_REINTERPRET_CAST(const PRUnichar *,
      JS_GetStringChars(JS_ValueToString(aContext, aID)));

    if (NS_LITERAL_STRING("href").Equals(prop)) {
      nsCOMPtr<nsIURI> base;
      nsAutoString href;

      if (NS_FAILED(CheckHrefAccess(aContext, aObj, PR_TRUE)))
        return PR_FALSE;

      // Get the parameter passed in
      nsJSUtils::nsConvertJSValToString(href, aContext, *aVp);

      // Get the source of the caller
      result = GetSourceURL(aContext, getter_AddRefs(base));

      if (NS_SUCCEEDED(result)) {
        result = SetHrefWithBase(href, base, PR_FALSE);
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
LocationImpl::Resolve(JSContext *aContext, JSObject *aObj, jsval aID,
                      PRBool* aDidDefineProperty)
{
  *aDidDefineProperty = PR_FALSE;

  if (JSVAL_IS_STRING(aID)) {
    JSString *str;

    str = JSVAL_TO_STRING(aID);

    const jschar *chars = ::JS_GetStringChars(str);
    const PRUnichar *unichars = NS_REINTERPRET_CAST(const PRUnichar*, chars);

    if (NS_LITERAL_STRING("href").Equals(unichars)) {
      // properties defined as 'noscript' in the IDL needs to be defined
      // lazily here so that unqualified lookups of such properties work
      ::JS_DefineUCProperty(aContext, (JSObject *)mScriptObject,
                            chars, ::JS_GetStringLength(str),
                            JSVAL_VOID, nsnull, nsnull, 0);

      *aDidDefineProperty = PR_TRUE;
    }
  }

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


