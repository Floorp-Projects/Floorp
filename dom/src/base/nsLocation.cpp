/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Travis Bogard <travis@netscape.com>
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsGlobalWindow.h"
#include "nsIWebShell.h"
#include "nsIDocShell.h"
#include "nsIDocShellLoadInfo.h"
#include "nsIWebNavigation.h"
#include "nsCDefaultURIFixup.h"
#include "nsIURIFixup.h"
#include "nsIURL.h"
#include "nsIIOService.h"
#include "nsIServiceManager.h"
#include "nsNetUtil.h"
#include "plstr.h"
#include "prprf.h"
#include "prmem.h"
#include "nsCOMPtr.h"
#include "nsEscape.h"
#include "nsJSUtils.h"
#include "nsIScriptSecurityManager.h"
#include "nsICodebasePrincipal.h"
#include "nsIDOMWindow.h"
#include "nsIDOMDocument.h"
#include "nsIDocument.h"
#include "nsIJSContextStack.h"
#include "nsXPIDLString.h"
#include "nsDOMError.h"
#include "nsDOMClassInfo.h"
#include "nsCRT.h"


static nsresult GetDocumentCharacterSetForURI(const nsAString& aHref, nsACString& aCharset)
{
  aCharset.Truncate();

  nsresult rv;

  nsCOMPtr<nsIJSContextStack> stack(do_GetService("@mozilla.org/js/xpc/ContextStack;1", &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  JSContext *cx;

  rv = stack->Peek(&cx);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIScriptGlobalObject> nativeGlob;
  nsJSUtils::GetDynamicScriptGlobal(cx, getter_AddRefs(nativeGlob));
  NS_ENSURE_TRUE(nativeGlob, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDOMWindow> window = do_QueryInterface(nativeGlob);
  NS_ENSURE_TRUE(window, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDOMDocument> domDoc;
  rv = window->GetDocument(getter_AddRefs(domDoc));
  NS_ENSURE_SUCCESS(rv, rv);
  if (!domDoc)
    return NS_OK;

  nsCOMPtr<nsIDocument> doc(do_QueryInterface(domDoc));
  NS_ENSURE_TRUE(doc, NS_ERROR_FAILURE);

  nsAutoString charset;
  rv = doc->GetDocumentCharacterSet(charset);
  NS_ENSURE_SUCCESS(rv, rv);

  CopyUCS2toASCII(charset, aCharset);

  return rv;
}

LocationImpl::LocationImpl(nsIDocShell *aDocShell)
{
  NS_INIT_ISUPPORTS();
  mDocShell = aDocShell; // Weak Reference
}

LocationImpl::~LocationImpl()
{
}


// QueryInterface implementation for LocationImpl
NS_INTERFACE_MAP_BEGIN(LocationImpl)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNSLocation)
  NS_INTERFACE_MAP_ENTRY(nsIDOMLocation)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMLocation)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(Location)
NS_INTERFACE_MAP_END


NS_IMPL_ADDREF(LocationImpl)
NS_IMPL_RELEASE(LocationImpl)

NS_IMETHODIMP_(void) LocationImpl::SetDocShell(nsIDocShell *aDocShell)
{
   mDocShell = aDocShell; // Weak Reference
}

nsresult
LocationImpl::CheckURL(nsIURI* aURI, nsIDocShellLoadInfo** aLoadInfo)
{
  *aLoadInfo = nsnull;

  nsresult result;
  // Get JSContext from stack.
  nsCOMPtr<nsIJSContextStack>
    stack(do_GetService("@mozilla.org/js/xpc/ContextStack;1", &result));

  if (NS_FAILED(result))
    return NS_ERROR_FAILURE;

  JSContext *cx;

  if (NS_FAILED(stack->Peek(&cx)))
    return NS_ERROR_FAILURE;

  if (!cx) {
    // No cx means that there's no JS running, or at least no JS that
    // was run through code that properly pushed a context onto the
    // context stack (as all code that runs JS off of web pages
    // does). Going further from here will crash, so lets not do
    // that...

    return NS_OK;
  }

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

  // now set the referrer on the loadinfo
  nsCOMPtr<nsIURI> sourceURI;
  GetSourceURL(cx, getter_AddRefs(sourceURI));
  if (sourceURI) {
    loadInfo->SetReferrer(sourceURI);
  }
  
  *aLoadInfo = loadInfo.get();
  NS_ADDREF(*aLoadInfo);

  return NS_OK;
}

nsresult
LocationImpl::GetURI(nsIURI** aURI)
{
  *aURI = nsnull;

  nsresult rv;

  nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(mDocShell, &rv));
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsIURI> uri;
  rv = webNav->GetCurrentURI(getter_AddRefs(uri));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIURIFixup> urifixup(do_GetService(NS_URIFIXUP_CONTRACTID, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  return urifixup->CreateExposableURI(uri, aURI);
}

nsresult
LocationImpl::GetWritableURI(nsIURI** aURI)
{
  *aURI = nsnull;

  nsCOMPtr<nsIURI> uri;

  nsresult rv = GetURI(getter_AddRefs(uri));
  if (NS_FAILED(rv) || !uri) {
    return rv;
  }

  return uri->Clone(aURI);
}

nsresult
LocationImpl::SetURI(nsIURI* aURI)
{
  if (mDocShell) {
    nsCOMPtr<nsIDocShellLoadInfo> loadInfo;
    nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(mDocShell));

    if(NS_FAILED(CheckURL(aURI, getter_AddRefs(loadInfo))))
      return NS_ERROR_FAILURE;

    webNav->Stop(nsIWebNavigation::STOP_CONTENT);
    return mDocShell->LoadURI(aURI, loadInfo,
                              nsIWebNavigation::LOAD_FLAGS_NONE, PR_TRUE);
  }

  return NS_OK;
}

NS_IMETHODIMP
LocationImpl::GetHash(nsAString& aHash)
{
  aHash.SetLength(0);

  nsCOMPtr<nsIURI> uri;
  nsresult result = NS_OK;

  result = GetURI(getter_AddRefs(uri));

  nsCOMPtr<nsIURL> url(do_QueryInterface(uri));

  if (url) {
    nsCAutoString ref;

    result = url->GetRef(ref);
    // XXX danger... this may result in non-ASCII octets!
    NS_UnescapeURL(ref);

    if (NS_SUCCEEDED(result) && !ref.IsEmpty()) {
      aHash.Assign(NS_LITERAL_STRING("#") + NS_ConvertASCIItoUCS2(ref));
    }
  }

  return result;
}

NS_IMETHODIMP
LocationImpl::SetHash(const nsAString& aHash)
{
  nsCOMPtr<nsIURI> uri;
  nsresult result = NS_OK;

  result = GetWritableURI(getter_AddRefs(uri));

  nsCOMPtr<nsIURL> url(do_QueryInterface(uri));

  if (url) {
    url->SetRef(NS_ConvertUCS2toUTF8(aHash));

    if (mDocShell) {
      nsCOMPtr<nsIDocShellLoadInfo> loadInfo;

      if (NS_SUCCEEDED(CheckURL(url, getter_AddRefs(loadInfo))))
        // We're not calling nsIWebNavigation->Stop, we don't want to
        // stop the load when we're just scrolling to a named anchor
        // in the document. See bug 114975.
        mDocShell->LoadURI(url, loadInfo,
                           nsIWebNavigation::LOAD_FLAGS_NONE, PR_TRUE);
    }
  }

  return result;
}

NS_IMETHODIMP
LocationImpl::GetHost(nsAString& aHost)
{
  aHost.SetLength(0);

  nsCOMPtr<nsIURI> uri;
  nsresult result;

  result = GetURI(getter_AddRefs(uri));

  if (uri) {
    nsCAutoString hostport;

    result = uri->GetHostPort(hostport);

    if (NS_SUCCEEDED(result)) {
      aHost = NS_ConvertUTF8toUCS2(hostport);
    }
  }

  return result;
}

NS_IMETHODIMP
LocationImpl::SetHost(const nsAString& aHost)
{
  nsCOMPtr<nsIURI> uri;
  nsresult result;

  result = GetWritableURI(getter_AddRefs(uri));

  if (uri) {
    uri->SetHostPort(NS_ConvertUCS2toUTF8(aHost));
    SetURI(uri);
  }

  return result;
}

NS_IMETHODIMP
LocationImpl::GetHostname(nsAString& aHostname)
{
  aHostname.SetLength(0);

  nsCOMPtr<nsIURI> uri;
  nsresult result;

  result = GetURI(getter_AddRefs(uri));

  if (uri) {
    nsCAutoString host;

    result = uri->GetHost(host);

    if (NS_SUCCEEDED(result)) {
      aHostname = NS_ConvertUTF8toUCS2(host);
    }
  }

  return result;
}

NS_IMETHODIMP
LocationImpl::SetHostname(const nsAString& aHostname)
{
  nsCOMPtr<nsIURI> uri;
  nsresult result;

  result = GetWritableURI(getter_AddRefs(uri));

  if (uri) {
    uri->SetHost(NS_ConvertUCS2toUTF8(aHostname));
    SetURI(uri);
  }

  return result;
}

NS_IMETHODIMP
LocationImpl::GetHref(nsAString& aHref)
{
  aHref.SetLength(0);

  nsCOMPtr<nsIURI> uri;
  nsresult result;

  result = GetURI(getter_AddRefs(uri));

  if (uri) {
    nsCAutoString uriString;

    result = uri->GetSpec(uriString);

    if (NS_SUCCEEDED(result)) {
      aHref = NS_ConvertUTF8toUCS2(uriString);
    }
  }

  return result;
}

NS_IMETHODIMP
LocationImpl::SetHref(const nsAString& aHref)
{
  nsAutoString oldHref;
  nsresult rv = NS_OK;

  // Get JSContext from stack.
  nsCOMPtr<nsIJSContextStack>
    stack(do_GetService("@mozilla.org/js/xpc/ContextStack;1", &rv));

  if (NS_FAILED(rv))
    return NS_ERROR_FAILURE;

  JSContext *cx;

  if (NS_FAILED(stack->Peek(&cx)))
    return NS_ERROR_FAILURE;

  if (cx) {
    rv = SetHrefWithContext(cx, aHref, PR_FALSE);
  } else {
    rv = GetHref(oldHref);

    if (NS_SUCCEEDED(rv)) {
      nsCOMPtr<nsIURI> oldUri;

      rv = NS_NewURI(getter_AddRefs(oldUri), oldHref);

      if (oldUri) {
        rv = SetHrefWithBase(aHref, oldUri, PR_FALSE);
      }
    }
  }

  return rv;
}

nsresult
LocationImpl::SetHrefWithContext(JSContext* cx, const nsAString& aHref,
                                 PRBool aReplace)
{
  nsCOMPtr<nsIURI> base;

  // Get the source of the caller
  nsresult result = GetSourceBaseURL(cx, getter_AddRefs(base));

  if (NS_FAILED(result)) {
    return result;
  }

  return SetHrefWithBase(aHref, base, aReplace);
}

nsresult
LocationImpl::SetHrefWithBase(const nsAString& aHref,
                              nsIURI* aBase, PRBool aReplace)
{
  nsresult result;
  nsCOMPtr<nsIURI> newUri;

  nsCAutoString docCharset;
  if (NS_SUCCEEDED(GetDocumentCharacterSetForURI(aHref, docCharset)))
    result = NS_NewURI(getter_AddRefs(newUri), aHref, docCharset.get(), aBase);
  else
    result = NS_NewURI(getter_AddRefs(newUri), aHref, nsnull, aBase);

  if (newUri && mDocShell) {
    nsCOMPtr<nsIDocShellLoadInfo> loadInfo;
    nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(mDocShell));

    nsresult rv = CheckURL(newUri, getter_AddRefs(loadInfo));

    if(NS_FAILED(rv))
      return rv;
     
    /* Check with the scriptContext if it is currently processing a script tag.
     * If so, this must be a <script> tag with a location.href in it.
     * we want to do a replace load, in such a situation. 
     * In other cases, for example if a event handler or a JS timer
     * had a location.href in it, we want to do a normal load,
     * so that the new url will be appended to Session History.
     * This solution is tricky. Hopefully it isn't going to bite
     * anywhere else. This is part of solution for bug # 39938, 72197
     * 
     */
    PRBool inScriptTag=PR_FALSE;
    // Get JSContext from stack.
    nsCOMPtr<nsIJSContextStack> stack(do_GetService("@mozilla.org/js/xpc/ContextStack;1", &result));

    if (stack) {
      JSContext *cx;

      result = stack->Peek(&cx);
      if (cx) {
        nsIScriptContext* scriptCX = (nsIScriptContext*)JS_GetContextPrivate(cx);
       
        if (scriptCX) {
          scriptCX->GetProcessingScriptTag(&inScriptTag);
        }  
      } //cx
    }  // stack

    if (aReplace ||  inScriptTag) {
      loadInfo->SetLoadType(nsIDocShellLoadInfo::loadNormalReplace);
    }

    webNav->Stop(nsIWebNavigation::STOP_CONTENT);
    return mDocShell->LoadURI(newUri, loadInfo,
                              nsIWebNavigation::LOAD_FLAGS_NONE, PR_TRUE);
  }

  return result;
}

NS_IMETHODIMP
LocationImpl::GetPathname(nsAString& aPathname)
{
  aPathname.SetLength(0);

  nsCOMPtr<nsIURI> uri;
  nsresult result = NS_OK;

  result = GetURI(getter_AddRefs(uri));

  nsCOMPtr<nsIURL> url(do_QueryInterface(uri));
  if (url) {
    nsCAutoString file;

    result = url->GetFilePath(file);

    if (NS_SUCCEEDED(result)) {
      aPathname = NS_ConvertUTF8toUCS2(file);
    }
  }

  return result;
}

NS_IMETHODIMP
LocationImpl::SetPathname(const nsAString& aPathname)
{
  nsCOMPtr<nsIURI> uri;
  nsresult result = NS_OK;

  result = GetWritableURI(getter_AddRefs(uri));

  if (uri) {
    uri->SetPath(NS_ConvertUCS2toUTF8(aPathname));
    SetURI(uri);
  }

  return result;
}

NS_IMETHODIMP
LocationImpl::GetPort(nsAString& aPort)
{
  aPort.SetLength(0);

  nsCOMPtr<nsIURI> uri;
  nsresult result = NS_OK;

  result = GetURI(getter_AddRefs(uri));

  if (uri) {
    PRInt32 port;
    uri->GetPort(&port);

    if (-1 != port) {
      nsAutoString portStr;
      portStr.AppendInt(port);
      aPort.Append(portStr);
    }
  }

  return result;
}

NS_IMETHODIMP
LocationImpl::SetPort(const nsAString& aPort)
{
  nsCOMPtr<nsIURI> uri;
  nsresult result = NS_OK;

  result = GetWritableURI(getter_AddRefs(uri));

  if (uri) {
    // perhaps use nsReadingIterators at some point?
    NS_ConvertUCS2toUTF8 portStr(aPort);
    const char *buf = portStr.get();
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
    SetURI(uri);
  }

  return result;
}

NS_IMETHODIMP
LocationImpl::GetProtocol(nsAString& aProtocol)
{
  aProtocol.SetLength(0);

  nsCOMPtr<nsIURI> uri;
  nsresult result = NS_OK;

  result = GetURI(getter_AddRefs(uri));

  if (uri) {
    nsCAutoString protocol;

    result = uri->GetScheme(protocol);

    if (NS_SUCCEEDED(result)) {
      aProtocol.Assign(NS_ConvertASCIItoUCS2(protocol));
      aProtocol.Append(PRUnichar(':'));
    }
  }

  return result;
}

NS_IMETHODIMP
LocationImpl::SetProtocol(const nsAString& aProtocol)
{
  nsCOMPtr<nsIURI> uri;
  nsresult result = NS_OK;

  result = GetWritableURI(getter_AddRefs(uri));

  if (uri) {
    uri->SetScheme(NS_ConvertUCS2toUTF8(aProtocol));
    SetURI(uri);
  }

  return result;
}

NS_IMETHODIMP
LocationImpl::GetSearch(nsAString& aSearch)
{
  aSearch.SetLength(0);

  nsCOMPtr<nsIURI> uri;
  nsresult result = NS_OK;

  result = GetURI(getter_AddRefs(uri));

  nsCOMPtr<nsIURL> url(do_QueryInterface(uri));

  if (url) {
    nsCAutoString search;

    result = url->GetQuery(search);

    if (NS_SUCCEEDED(result) && !search.IsEmpty()) {
      aSearch.Assign(NS_LITERAL_STRING("?") + NS_ConvertUTF8toUCS2(search));
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
LocationImpl::SetSearch(const nsAString& aSearch)
{
  nsCOMPtr<nsIURI> uri;
  nsresult result = NS_OK;

  result = GetWritableURI(getter_AddRefs(uri));

  nsCOMPtr<nsIURL> url(do_QueryInterface(uri));
  if (url) {
    result = url->SetQuery(NS_ConvertUCS2toUTF8(aSearch));
    SetURI(uri);
  }

  return result;
}

NS_IMETHODIMP
LocationImpl::Reload(PRBool aForceget)
{
  nsresult rv;
  nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(mDocShell));

  if (webNav) {
    PRUint32 reloadFlags = nsIWebNavigation::LOAD_FLAGS_NONE;

    if (aForceget) {
      reloadFlags = nsIWebNavigation::LOAD_FLAGS_BYPASS_CACHE | 
                    nsIWebNavigation::LOAD_FLAGS_BYPASS_PROXY;
    }
    rv = webNav->Reload(reloadFlags);
  } else {
    NS_ASSERTION(0, "nsIWebNavigation interface is not available!");
    rv = NS_ERROR_FAILURE;
  }

  return rv;
}

NS_IMETHODIMP
LocationImpl::Reload()
{
  nsresult rv;
  nsCOMPtr<nsIXPConnect> xpc(do_GetService(nsIXPConnect::GetCID(), &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIXPCNativeCallContext> ncc;

  rv = xpc->GetCurrentNativeCallContext(getter_AddRefs(ncc));
  NS_ENSURE_SUCCESS(rv, rv);

  if (!ncc)
    return NS_ERROR_NOT_AVAILABLE;

  PRBool force_get = PR_FALSE;

  PRUint32 argc;

  ncc->GetArgc(&argc);

  if (argc > 0) {
    jsval *argv = nsnull;

    ncc->GetArgvPtr(&argv);
    NS_ENSURE_TRUE(argv, NS_ERROR_UNEXPECTED);

    JSContext *cx = nsnull;

    rv = ncc->GetJSContext(&cx);
    NS_ENSURE_SUCCESS(rv, rv);

    JS_ValueToBoolean(cx, argv[0], &force_get);
  }

  return Reload(force_get);
}

NS_IMETHODIMP
LocationImpl::Replace(const nsAString& aUrl)
{
  nsresult rv = NS_OK;

  // Get JSContext from stack.
  nsCOMPtr<nsIJSContextStack>
  stack(do_GetService("@mozilla.org/js/xpc/ContextStack;1"));

  if (stack) {
    JSContext *cx;

    rv = stack->Peek(&cx);
    NS_ENSURE_SUCCESS(rv, rv);
    if (cx) {
      return SetHrefWithContext(cx, aUrl, PR_TRUE);
    }
  }

  nsAutoString oldHref;

  rv = GetHref(oldHref);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIURI> oldUri;

  rv = NS_NewURI(getter_AddRefs(oldUri), oldHref);
  NS_ENSURE_SUCCESS(rv, rv);

  return SetHrefWithBase(aUrl, oldUri, PR_TRUE);
}

NS_IMETHODIMP
LocationImpl::Assign(const nsAString& aUrl)
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
LocationImpl::ToString(nsAString& aReturn)
{
  return GetHref(aReturn);
}

nsresult
LocationImpl::GetSourceDocument(JSContext* cx, nsIDocument** aDocument)
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
  nsJSUtils::GetDynamicScriptGlobal(cx, getter_AddRefs(nativeGlob));

  if (nativeGlob) {
    nsCOMPtr<nsIDOMWindow> window = do_QueryInterface(nativeGlob, &result);

    if (window) {
      nsCOMPtr<nsIDOMDocument> domDoc;
      result = window->GetDocument(getter_AddRefs(domDoc));
      if (domDoc) {
        return CallQueryInterface(domDoc, aDocument);
      }
    }
  } else {
    *aDocument = nsnull;
  }
  return result;
}

nsresult
LocationImpl::GetSourceBaseURL(JSContext* cx, nsIURI** sourceURL)
{
  nsCOMPtr<nsIDocument> doc;
  nsresult rv = GetSourceDocument(cx, getter_AddRefs(doc));
  if (doc) {
    rv = doc->GetBaseURL(*sourceURL);

    if (!*sourceURL) {
      doc->GetDocumentURL(sourceURL);
    }
  } else {
    *sourceURL = nsnull;
  }
  return rv;
}

nsresult
LocationImpl::GetSourceURL(JSContext* cx, nsIURI** sourceURL)
{
  nsCOMPtr<nsIDocument> doc;
  nsresult rv = GetSourceDocument(cx, getter_AddRefs(doc));
  if (doc) {
    doc->GetDocumentURL(sourceURL);
  } else {
    *sourceURL = nsnull;
  }

  return rv;
}
