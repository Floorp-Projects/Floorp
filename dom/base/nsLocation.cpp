/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsLocation.h"
#include "nsIScriptSecurityManager.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsIScriptContext.h"
#include "nsIDocShell.h"
#include "nsIDocShellLoadInfo.h"
#include "nsIWebNavigation.h"
#include "nsCDefaultURIFixup.h"
#include "nsIURIFixup.h"
#include "nsIURL.h"
#include "nsIJARURI.h"
#include "nsNetUtil.h"
#include "nsCOMPtr.h"
#include "nsEscape.h"
#include "nsIDOMWindow.h"
#include "nsIDocument.h"
#include "nsIPresShell.h"
#include "nsPresContext.h"
#include "nsError.h"
#include "nsDOMClassInfoID.h"
#include "nsReadableUtils.h"
#include "nsITextToSubURI.h"
#include "nsJSUtils.h"
#include "nsContentUtils.h"
#include "nsGlobalWindow.h"
#include "mozilla/Likely.h"
#include "nsCycleCollectionParticipant.h"
#include "nsNullPrincipal.h"
#include "ScriptSettings.h"
#include "mozilla/dom/LocationBinding.h"

using namespace mozilla;
using namespace mozilla::dom;

static nsresult
GetDocumentCharacterSetForURI(const nsAString& aHref, nsACString& aCharset)
{
  aCharset.Truncate();

  if (nsIDocument* doc = GetEntryDocument()) {
    aCharset = doc->GetDocumentCharacterSet();
  }

  return NS_OK;
}

nsLocation::nsLocation(nsPIDOMWindow* aWindow, nsIDocShell *aDocShell)
  : mInnerWindow(aWindow)
{
  MOZ_ASSERT(aDocShell);
  MOZ_ASSERT(mInnerWindow->IsInnerWindow());
  SetIsDOMBinding();

  mDocShell = do_GetWeakReference(aDocShell);
}

nsLocation::~nsLocation()
{
  RemoveURLSearchParams();
}

// QueryInterface implementation for nsLocation
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsLocation)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsIDOMLocation)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMLocation)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_CLASS(nsLocation)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsLocation)
  tmp->RemoveURLSearchParams();

  NS_IMPL_CYCLE_COLLECTION_UNLINK(mInnerWindow);
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsLocation)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSearchParams)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mInnerWindow)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_WRAPPERCACHE(nsLocation)

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsLocation)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsLocation)

void
nsLocation::SetDocShell(nsIDocShell *aDocShell)
{
   mDocShell = do_GetWeakReference(aDocShell);
}

nsIDocShell *
nsLocation::GetDocShell()
{
  nsCOMPtr<nsIDocShell> docshell(do_QueryReferent(mDocShell));
  return docshell;
}

nsresult
nsLocation::CheckURL(nsIURI* aURI, nsIDocShellLoadInfo** aLoadInfo)
{
  *aLoadInfo = nullptr;

  nsCOMPtr<nsIDocShell> docShell(do_QueryReferent(mDocShell));
  NS_ENSURE_TRUE(docShell, NS_ERROR_NOT_AVAILABLE);

  nsCOMPtr<nsISupports> owner;
  nsCOMPtr<nsIURI> sourceURI;

  if (JSContext *cx = nsContentUtils::GetCurrentJSContext()) {
    // No cx means that there's no JS running, or at least no JS that
    // was run through code that properly pushed a context onto the
    // context stack (as all code that runs JS off of web pages
    // does). We won't bother with security checks in this case, but
    // we need to create the loadinfo etc.

    // Get security manager.
    nsIScriptSecurityManager* ssm = nsContentUtils::GetSecurityManager();
    NS_ENSURE_STATE(ssm);

    // Check to see if URI is allowed.
    nsresult rv = ssm->CheckLoadURIFromScript(cx, aURI);
    NS_ENSURE_SUCCESS(rv, rv);

    // Make the load's referrer reflect changes to the document's URI caused by
    // push/replaceState, if possible.  First, get the document corresponding to
    // fp.  If the document's original URI (i.e. its URI before
    // push/replaceState) matches the principal's URI, use the document's
    // current URI as the referrer.  If they don't match, use the principal's
    // URI.

    nsCOMPtr<nsIDocument> doc;
    nsCOMPtr<nsIURI> docOriginalURI, docCurrentURI, principalURI;
    nsCOMPtr<nsPIDOMWindow> incumbent =
      do_QueryInterface(mozilla::dom::GetIncumbentGlobal());
    if (incumbent) {
      doc = incumbent->GetDoc();
    }
    if (doc) {
      docOriginalURI = doc->GetOriginalURI();
      docCurrentURI = doc->GetDocumentURI();
      rv = doc->NodePrincipal()->GetURI(getter_AddRefs(principalURI));
      NS_ENSURE_SUCCESS(rv, rv);
    }

    bool urisEqual = false;
    if (docOriginalURI && docCurrentURI && principalURI) {
      principalURI->Equals(docOriginalURI, &urisEqual);
    }

    if (urisEqual) {
      sourceURI = docCurrentURI;
    }
    else {
      // Use principalURI as long as it is not an nsNullPrincipalURI.
      // We could add a method such as GetReferrerURI to principals to make this
      // cleaner, but given that we need to start using Source Browsing Context
      // for referrer (see Bug 960639) this may be wasted effort at this stage.
      if (principalURI) {
        bool isNullPrincipalScheme;
        rv = principalURI->SchemeIs(NS_NULLPRINCIPAL_SCHEME,
                                    &isNullPrincipalScheme);
        if (NS_SUCCEEDED(rv) && !isNullPrincipalScheme) {
          sourceURI = principalURI;
        }
      }
    }

    owner = nsContentUtils::SubjectPrincipal();
  }

  // Create load info
  nsCOMPtr<nsIDocShellLoadInfo> loadInfo;
  docShell->CreateLoadInfo(getter_AddRefs(loadInfo));
  NS_ENSURE_TRUE(loadInfo, NS_ERROR_FAILURE);

  loadInfo->SetOwner(owner);

  if (sourceURI) {
    loadInfo->SetReferrer(sourceURI);
  }

  loadInfo.swap(*aLoadInfo);

  return NS_OK;
}

nsresult
nsLocation::GetURI(nsIURI** aURI, bool aGetInnermostURI)
{
  *aURI = nullptr;

  nsresult rv;
  nsCOMPtr<nsIDocShell> docShell(do_QueryReferent(mDocShell));
  nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(docShell, &rv));
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsIURI> uri;
  rv = webNav->GetCurrentURI(getter_AddRefs(uri));
  NS_ENSURE_SUCCESS(rv, rv);

  // It is valid for docshell to return a null URI. Don't try to fixup
  // if this happens.
  if (!uri) {
    return NS_OK;
  }

  if (aGetInnermostURI) {
    nsCOMPtr<nsIJARURI> jarURI(do_QueryInterface(uri));
    while (jarURI) {
      jarURI->GetJARFile(getter_AddRefs(uri));
      jarURI = do_QueryInterface(uri);
    }
  }

  NS_ASSERTION(uri, "nsJARURI screwed up?");

  nsCOMPtr<nsIURIFixup> urifixup(do_GetService(NS_URIFIXUP_CONTRACTID, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  return urifixup->CreateExposableURI(uri, aURI);
}

nsresult
nsLocation::GetWritableURI(nsIURI** aURI)
{
  *aURI = nullptr;

  nsCOMPtr<nsIURI> uri;

  nsresult rv = GetURI(getter_AddRefs(uri));
  if (NS_FAILED(rv) || !uri) {
    return rv;
  }

  return uri->Clone(aURI);
}

nsresult
nsLocation::SetURI(nsIURI* aURI, bool aReplace)
{
  nsCOMPtr<nsIDocShell> docShell(do_QueryReferent(mDocShell));
  if (docShell) {
    nsCOMPtr<nsIDocShellLoadInfo> loadInfo;
    nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(docShell));

    if(NS_FAILED(CheckURL(aURI, getter_AddRefs(loadInfo))))
      return NS_ERROR_FAILURE;

    if (aReplace) {
      loadInfo->SetLoadType(nsIDocShellLoadInfo::loadStopContentAndReplace);
    } else {
      loadInfo->SetLoadType(nsIDocShellLoadInfo::loadStopContent);
    }

    // Get the incumbent script's browsing context to set as source.
    nsCOMPtr<nsPIDOMWindow> sourceWindow =
      do_QueryInterface(mozilla::dom::GetIncumbentGlobal());
    if (sourceWindow) {
      loadInfo->SetSourceDocShell(sourceWindow->GetDocShell());
    }

    return docShell->LoadURI(aURI, loadInfo,
                             nsIWebNavigation::LOAD_FLAGS_NONE, true);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsLocation::GetHash(nsAString& aHash)
{
  if (!CallerSubsumes())
    return NS_ERROR_DOM_SECURITY_ERR;

  aHash.SetLength(0);

  nsCOMPtr<nsIURI> uri;
  nsresult rv = GetURI(getter_AddRefs(uri));
  if (NS_FAILED(rv) || !uri) {
    return rv;
  }

  nsAutoCString ref;
  nsAutoString unicodeRef;

  rv = uri->GetRef(ref);
  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsITextToSubURI> textToSubURI(
        do_GetService(NS_ITEXTTOSUBURI_CONTRACTID, &rv));

    if (NS_SUCCEEDED(rv)) {
      nsAutoCString charset;
      uri->GetOriginCharset(charset);
        
      rv = textToSubURI->UnEscapeURIForUI(charset, ref, unicodeRef);
    }
      
    if (NS_FAILED(rv)) {
      // Oh, well.  No intl here!
      NS_UnescapeURL(ref);
      CopyASCIItoUTF16(ref, unicodeRef);
      rv = NS_OK;
    }
  }

  if (NS_SUCCEEDED(rv) && !unicodeRef.IsEmpty()) {
    aHash.Assign(char16_t('#'));
    aHash.Append(unicodeRef);
  }

  if (aHash == mCachedHash) {
    // Work around ShareThis stupidly polling location.hash every
    // 5ms all the time by handing out the same exact string buffer
    // we handed out last time.
    aHash = mCachedHash;
  } else {
    mCachedHash = aHash;
  }

  return rv;
}

NS_IMETHODIMP
nsLocation::SetHash(const nsAString& aHash)
{
  nsCOMPtr<nsIURI> uri;
  nsresult rv = GetWritableURI(getter_AddRefs(uri));
  if (NS_FAILED(rv) || !uri) {
    return rv;
  }

  NS_ConvertUTF16toUTF8 hash(aHash);
  if (hash.IsEmpty() || hash.First() != char16_t('#')) {
    hash.Insert(char16_t('#'), 0);
  }
  rv = uri->SetRef(hash);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return SetURI(uri);
}

NS_IMETHODIMP
nsLocation::GetHost(nsAString& aHost)
{
  if (!CallerSubsumes())
    return NS_ERROR_DOM_SECURITY_ERR;

  aHost.Truncate();

  nsCOMPtr<nsIURI> uri;
  nsresult result;

  result = GetURI(getter_AddRefs(uri), true);

  if (uri) {
    nsAutoCString hostport;

    result = uri->GetHostPort(hostport);

    if (NS_SUCCEEDED(result)) {
      AppendUTF8toUTF16(hostport, aHost);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsLocation::SetHost(const nsAString& aHost)
{
  if (!CallerSubsumes())
    return NS_ERROR_DOM_SECURITY_ERR;

  nsCOMPtr<nsIURI> uri;
  nsresult rv = GetWritableURI(getter_AddRefs(uri));
  if (NS_WARN_IF(NS_FAILED(rv) || !uri)) {
    return rv;
  }

  rv = uri->SetHostPort(NS_ConvertUTF16toUTF8(aHost));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return SetURI(uri);
}

NS_IMETHODIMP
nsLocation::GetHostname(nsAString& aHostname)
{
  if (!CallerSubsumes())
    return NS_ERROR_DOM_SECURITY_ERR;

  aHostname.Truncate();

  nsCOMPtr<nsIURI> uri;
  nsresult result;

  result = GetURI(getter_AddRefs(uri), true);

  if (uri) {
    nsAutoCString host;

    result = uri->GetHost(host);

    if (NS_SUCCEEDED(result)) {
      AppendUTF8toUTF16(host, aHostname);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsLocation::SetHostname(const nsAString& aHostname)
{
  if (!CallerSubsumes())
    return NS_ERROR_DOM_SECURITY_ERR;

  nsCOMPtr<nsIURI> uri;
  nsresult rv = GetWritableURI(getter_AddRefs(uri));
  if (NS_WARN_IF(NS_FAILED(rv) || !uri)) {
    return rv;
  }

  rv = uri->SetHost(NS_ConvertUTF16toUTF8(aHostname));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return SetURI(uri);
}

NS_IMETHODIMP
nsLocation::GetHref(nsAString& aHref)
{
  if (!CallerSubsumes())
    return NS_ERROR_DOM_SECURITY_ERR;

  aHref.Truncate();

  nsCOMPtr<nsIURI> uri;
  nsresult result;

  result = GetURI(getter_AddRefs(uri));

  if (uri) {
    nsAutoCString uriString;

    result = uri->GetSpec(uriString);

    if (NS_SUCCEEDED(result)) {
      AppendUTF8toUTF16(uriString, aHref);
    }
  }

  return result;
}

NS_IMETHODIMP
nsLocation::SetHref(const nsAString& aHref)
{
  nsAutoString oldHref;
  nsresult rv = NS_OK;

  JSContext *cx = nsContentUtils::GetCurrentJSContext();
  if (cx) {
    rv = SetHrefWithContext(cx, aHref, false);
  } else {
    rv = GetHref(oldHref);

    if (NS_SUCCEEDED(rv)) {
      nsCOMPtr<nsIURI> oldUri;

      rv = NS_NewURI(getter_AddRefs(oldUri), oldHref);

      if (oldUri) {
        rv = SetHrefWithBase(aHref, oldUri, false);
      }
    }
  }

  return rv;
}

nsresult
nsLocation::SetHrefWithContext(JSContext* cx, const nsAString& aHref,
                               bool aReplace)
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
nsLocation::SetHrefWithBase(const nsAString& aHref, nsIURI* aBase,
                            bool aReplace)
{
  nsresult result;
  nsCOMPtr<nsIURI> newUri;

  nsCOMPtr<nsIDocShell> docShell(do_QueryReferent(mDocShell));

  nsAutoCString docCharset;
  if (NS_SUCCEEDED(GetDocumentCharacterSetForURI(aHref, docCharset)))
    result = NS_NewURI(getter_AddRefs(newUri), aHref, docCharset.get(), aBase);
  else
    result = NS_NewURI(getter_AddRefs(newUri), aHref, nullptr, aBase);

  if (newUri) {
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
    bool inScriptTag = false;
    nsIScriptContext* scriptContext = nullptr;
    nsCOMPtr<nsPIDOMWindow> win = do_QueryInterface(GetEntryGlobal());
    if (win) {
      scriptContext = static_cast<nsGlobalWindow*>(win.get())->GetContextInternal();
    }

    if (scriptContext) {
      if (scriptContext->GetProcessingScriptTag()) {
        // Now check to make sure that the script is running in our window,
        // since we only want to replace if the location is set by a
        // <script> tag in the same window.  See bug 178729.
        nsCOMPtr<nsIScriptGlobalObject> ourGlobal =
          docShell ? docShell->GetScriptGlobalObject() : nullptr;
        inScriptTag = (ourGlobal == scriptContext->GetGlobalObject());
      }
    }

    return SetURI(newUri, aReplace || inScriptTag);
  }

  return result;
}

NS_IMETHODIMP
nsLocation::GetOrigin(nsAString& aOrigin)
{
  if (!CallerSubsumes())
    return NS_ERROR_DOM_SECURITY_ERR;

  aOrigin.Truncate();

  nsCOMPtr<nsIURI> uri;
  nsresult rv = GetURI(getter_AddRefs(uri), true);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(uri, NS_OK);

  nsAutoString origin;
  rv = nsContentUtils::GetUTFOrigin(uri, origin);
  NS_ENSURE_SUCCESS(rv, rv);

  aOrigin = origin;
  return NS_OK;
}

NS_IMETHODIMP
nsLocation::GetPathname(nsAString& aPathname)
{
  if (!CallerSubsumes())
    return NS_ERROR_DOM_SECURITY_ERR;

  aPathname.Truncate();

  nsCOMPtr<nsIURI> uri;
  nsresult result = NS_OK;

  result = GetURI(getter_AddRefs(uri));

  nsCOMPtr<nsIURL> url(do_QueryInterface(uri));
  if (url) {
    nsAutoCString file;

    result = url->GetFilePath(file);

    if (NS_SUCCEEDED(result)) {
      AppendUTF8toUTF16(file, aPathname);
    }
  }

  return result;
}

NS_IMETHODIMP
nsLocation::SetPathname(const nsAString& aPathname)
{
  if (!CallerSubsumes())
    return NS_ERROR_DOM_SECURITY_ERR;

  nsCOMPtr<nsIURI> uri;
  nsresult rv = GetWritableURI(getter_AddRefs(uri));
  if (NS_WARN_IF(NS_FAILED(rv) || !uri)) {
    return rv;
  }

  rv = uri->SetPath(NS_ConvertUTF16toUTF8(aPathname));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return SetURI(uri);
}

NS_IMETHODIMP
nsLocation::GetPort(nsAString& aPort)
{
  if (!CallerSubsumes())
    return NS_ERROR_DOM_SECURITY_ERR;

  aPort.SetLength(0);

  nsCOMPtr<nsIURI> uri;
  nsresult result = NS_OK;

  result = GetURI(getter_AddRefs(uri), true);

  if (uri) {
    int32_t port;
    result = uri->GetPort(&port);

    if (NS_SUCCEEDED(result) && -1 != port) {
      nsAutoString portStr;
      portStr.AppendInt(port);
      aPort.Append(portStr);
    }

    // Don't propagate this exception to caller
    result = NS_OK;
  }

  return result;
}

NS_IMETHODIMP
nsLocation::SetPort(const nsAString& aPort)
{
  if (!CallerSubsumes())
    return NS_ERROR_DOM_SECURITY_ERR;

  nsCOMPtr<nsIURI> uri;
  nsresult rv = GetWritableURI(getter_AddRefs(uri));
  if (NS_WARN_IF(NS_FAILED(rv) || !uri)) {
    return rv;
  }

  // perhaps use nsReadingIterators at some point?
  NS_ConvertUTF16toUTF8 portStr(aPort);
  const char *buf = portStr.get();
  int32_t port = -1;

  if (!portStr.IsEmpty() && buf) {
    if (*buf == ':') {
      port = atol(buf+1);
    }
    else {
      port = atol(buf);
    }
  }

  rv = uri->SetPort(port);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return SetURI(uri);
}

NS_IMETHODIMP
nsLocation::GetProtocol(nsAString& aProtocol)
{
  if (!CallerSubsumes())
    return NS_ERROR_DOM_SECURITY_ERR;

  aProtocol.SetLength(0);

  nsCOMPtr<nsIURI> uri;
  nsresult result = NS_OK;

  result = GetURI(getter_AddRefs(uri));

  if (uri) {
    nsAutoCString protocol;

    result = uri->GetScheme(protocol);

    if (NS_SUCCEEDED(result)) {
      CopyASCIItoUTF16(protocol, aProtocol);
      aProtocol.Append(char16_t(':'));
    }
  }

  return result;
}

NS_IMETHODIMP
nsLocation::SetProtocol(const nsAString& aProtocol)
{
  if (!CallerSubsumes())
    return NS_ERROR_DOM_SECURITY_ERR;

  nsCOMPtr<nsIURI> uri;
  nsresult rv = GetWritableURI(getter_AddRefs(uri));
  if (NS_WARN_IF(NS_FAILED(rv) || !uri)) {
    return rv;
  }

  rv = uri->SetScheme(NS_ConvertUTF16toUTF8(aProtocol));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return SetURI(uri);
}

void
nsLocation::GetUsername(nsAString& aUsername, ErrorResult& aError)
{
  if (!CallerSubsumes()) {
    aError.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  aUsername.Truncate();
  nsCOMPtr<nsIURI> uri;
  nsresult result = GetURI(getter_AddRefs(uri));
  if (uri) {
    nsAutoCString username;
    result = uri->GetUsername(username);
    if (NS_SUCCEEDED(result)) {
      CopyUTF8toUTF16(username, aUsername);
    }
  }
}

void
nsLocation::SetUsername(const nsAString& aUsername, ErrorResult& aError)
{
  if (!CallerSubsumes()) {
    aError.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  nsCOMPtr<nsIURI> uri;
  nsresult rv = GetWritableURI(getter_AddRefs(uri));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aError.Throw(rv);
    return;
  }

  if (!uri) {
    return;
  }

  rv = uri->SetUsername(NS_ConvertUTF16toUTF8(aUsername));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aError.Throw(rv);
    return;
  }

  rv = SetURI(uri);
}

void
nsLocation::GetPassword(nsAString& aPassword, ErrorResult& aError)
{
  if (!CallerSubsumes()) {
    aError.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  aPassword.Truncate();
  nsCOMPtr<nsIURI> uri;
  nsresult result = GetURI(getter_AddRefs(uri));
  if (uri) {
    nsAutoCString password;
    result = uri->GetPassword(password);
    if (NS_SUCCEEDED(result)) {
      CopyUTF8toUTF16(password, aPassword);
    }
  }
}

void
nsLocation::SetPassword(const nsAString& aPassword, ErrorResult& aError)
{
  if (!CallerSubsumes()) {
    aError.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  nsCOMPtr<nsIURI> uri;
  nsresult rv = GetWritableURI(getter_AddRefs(uri));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aError.Throw(rv);
    return;
  }

  if (!uri) {
    return;
  }

  rv = uri->SetPassword(NS_ConvertUTF16toUTF8(aPassword));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aError.Throw(rv);
    return;
  }

  rv = SetURI(uri);
}

NS_IMETHODIMP
nsLocation::GetSearch(nsAString& aSearch)
{
  if (!CallerSubsumes())
    return NS_ERROR_DOM_SECURITY_ERR;

  aSearch.SetLength(0);

  nsCOMPtr<nsIURI> uri;
  nsresult result = NS_OK;

  result = GetURI(getter_AddRefs(uri));

  nsCOMPtr<nsIURL> url(do_QueryInterface(uri));

  if (url) {
    nsAutoCString search;

    result = url->GetQuery(search);

    if (NS_SUCCEEDED(result) && !search.IsEmpty()) {
      aSearch.Assign(char16_t('?'));
      AppendUTF8toUTF16(search, aSearch);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsLocation::SetSearch(const nsAString& aSearch)
{
  nsresult rv = SetSearchInternal(aSearch);
  if (NS_FAILED(rv)) {
    return rv;
  }

  return NS_OK;
}

nsresult
nsLocation::SetSearchInternal(const nsAString& aSearch)
{
  if (!CallerSubsumes())
    return NS_ERROR_DOM_SECURITY_ERR;

  nsCOMPtr<nsIURI> uri;
  nsresult rv = GetWritableURI(getter_AddRefs(uri));

  nsCOMPtr<nsIURL> url(do_QueryInterface(uri));
  if (NS_WARN_IF(NS_FAILED(rv) || !url)) {
    return rv;
  }

  rv = url->SetQuery(NS_ConvertUTF16toUTF8(aSearch));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return SetURI(uri);
}

NS_IMETHODIMP
nsLocation::Reload(bool aForceget)
{
  if (!CallerSubsumes())
    return NS_ERROR_DOM_SECURITY_ERR;

  nsresult rv;
  nsCOMPtr<nsIDocShell> docShell(do_QueryReferent(mDocShell));
  nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(docShell));
  nsCOMPtr<nsPIDOMWindow> window = docShell ? docShell->GetWindow() : nullptr;

  if (window && window->IsHandlingResizeEvent()) {
    // location.reload() was called on a window that is handling a
    // resize event. Sites do this since Netscape 4.x needed it, but
    // we don't, and it's a horrible experience for nothing. In stead
    // of reloading the page, just clear style data and reflow the
    // page since some sites may use this trick to work around gecko
    // reflow bugs, and this should have the same effect.

    nsCOMPtr<nsIDocument> doc = window->GetExtantDoc();

    nsIPresShell *shell;
    nsPresContext *pcx;
    if (doc && (shell = doc->GetShell()) && (pcx = shell->GetPresContext())) {
      pcx->RebuildAllStyleData(NS_STYLE_HINT_REFLOW);
    }

    return NS_OK;
  }

  if (webNav) {
    uint32_t reloadFlags = nsIWebNavigation::LOAD_FLAGS_NONE;

    if (aForceget) {
      reloadFlags = nsIWebNavigation::LOAD_FLAGS_BYPASS_CACHE | 
                    nsIWebNavigation::LOAD_FLAGS_BYPASS_PROXY;
    }
    rv = webNav->Reload(reloadFlags);
    if (rv == NS_BINDING_ABORTED) {
      // This happens when we attempt to reload a POST result and the user says
      // no at the "do you want to reload?" prompt.  Don't propagate this one
      // back to callers.
      rv = NS_OK;
    }
  } else {
    rv = NS_ERROR_FAILURE;
  }

  return rv;
}

NS_IMETHODIMP
nsLocation::Replace(const nsAString& aUrl)
{
  nsresult rv = NS_OK;
  if (JSContext *cx = nsContentUtils::GetCurrentJSContext()) {
    return SetHrefWithContext(cx, aUrl, true);
  }

  nsAutoString oldHref;

  rv = GetHref(oldHref);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIURI> oldUri;

  rv = NS_NewURI(getter_AddRefs(oldUri), oldHref);
  NS_ENSURE_SUCCESS(rv, rv);

  return SetHrefWithBase(aUrl, oldUri, true);
}

NS_IMETHODIMP
nsLocation::Assign(const nsAString& aUrl)
{
  if (!CallerSubsumes())
    return NS_ERROR_DOM_SECURITY_ERR;

  nsAutoString oldHref;
  nsresult result = NS_OK;

  result = GetHref(oldHref);

  if (NS_SUCCEEDED(result)) {
    nsCOMPtr<nsIURI> oldUri;

    result = NS_NewURI(getter_AddRefs(oldUri), oldHref);

    if (oldUri) {
      result = SetHrefWithBase(aUrl, oldUri, false);
    }
  }

  return result;
}

NS_IMETHODIMP
nsLocation::ToString(nsAString& aReturn)
{
  // NB: GetHref checks CallerSubsumes().
  return GetHref(aReturn);
}

NS_IMETHODIMP
nsLocation::ValueOf(nsIDOMLocation** aReturn)
{
  nsCOMPtr<nsIDOMLocation> loc(this);
  loc.forget(aReturn);
  return NS_OK;
}

nsresult
nsLocation::GetSourceBaseURL(JSContext* cx, nsIURI** sourceURL)
{
  *sourceURL = nullptr;
  nsIDocument* doc = GetEntryDocument();
  // If there's no entry document, we either have no Script Entry Point or one
  // that isn't a DOM Window.  This doesn't generally happen with the DOM,
  // but can sometimes happen with extension code in certain IPC configurations.
  // If this happens, try falling back on the current document associated with
  // the docshell. If that fails, just return null and hope that the caller passed
  // an absolute URI.
  if (!doc && GetDocShell()) {
    nsCOMPtr<nsPIDOMWindow> docShellWin = do_QueryInterface(GetDocShell()->GetScriptGlobalObject());
    if (docShellWin) {
      doc = docShellWin->GetDoc();
    }
  }
  NS_ENSURE_TRUE(doc, NS_OK);
  *sourceURL = doc->GetBaseURI().take();
  return NS_OK;
}

bool
nsLocation::CallerSubsumes()
{
  // Get the principal associated with the location object.  Note that this is
  // the principal of the page which will actually be navigated, not the
  // principal of the Location object itself.  This is why we need this check
  // even though we only allow limited cross-origin access to Location objects
  // in general.
  nsCOMPtr<nsIDOMWindow> outer = mInnerWindow->GetOuterWindow();
  if (MOZ_UNLIKELY(!outer))
    return false;
  nsCOMPtr<nsIScriptObjectPrincipal> sop = do_QueryInterface(outer);
  bool subsumes = false;
  nsresult rv = nsContentUtils::SubjectPrincipal()->SubsumesConsideringDomain(sop->GetPrincipal(), &subsumes);
  NS_ENSURE_SUCCESS(rv, false);
  return subsumes;
}

JSObject*
nsLocation::WrapObject(JSContext* aCx)
{
  return LocationBinding::Wrap(aCx, this);
}

URLSearchParams*
nsLocation::GetDocShellSearchParams()
{
  nsCOMPtr<nsIDocShell> docShell = GetDocShell();
  if (!docShell) {
    return nullptr;
  }

  return docShell->GetURLSearchParams();
}

URLSearchParams*
nsLocation::SearchParams()
{
  if (!mSearchParams) {
    // We must register this object to the URLSearchParams of the docshell in
    // order to receive updates.
    nsRefPtr<URLSearchParams> searchParams = GetDocShellSearchParams();
    if (searchParams) {
      searchParams->AddObserver(this);
    }

    mSearchParams = new URLSearchParams();
    mSearchParams->AddObserver(this);
    UpdateURLSearchParams();
  }

  return mSearchParams;
}

void
nsLocation::SetSearchParams(URLSearchParams& aSearchParams)
{
  if (mSearchParams) {
    mSearchParams->RemoveObserver(this);
  }

  // the observer will be cleared using the cycle collector.
  mSearchParams = &aSearchParams;
  mSearchParams->AddObserver(this);

  nsAutoString search;
  mSearchParams->Serialize(search);
  SetSearchInternal(search);

  // We don't need to inform the docShell about this new SearchParams because
  // setting the new value the docShell will refresh its value automatically.
}

void
nsLocation::URLSearchParamsUpdated(URLSearchParams* aSearchParams)
{
  MOZ_ASSERT(mSearchParams);

  // This change comes from content.
  if (aSearchParams == mSearchParams) {
    nsAutoString search;
    mSearchParams->Serialize(search);
    SetSearchInternal(search);
    return;
  }

  // This change comes from the docShell.
#ifdef DEBUG
  {
    nsRefPtr<URLSearchParams> searchParams = GetDocShellSearchParams();
    MOZ_ASSERT(searchParams);
    MOZ_ASSERT(aSearchParams == searchParams);
  }
#endif

  nsAutoString search;
  aSearchParams->Serialize(search);
  mSearchParams->ParseInput(NS_ConvertUTF16toUTF8(search), this);
}

void
nsLocation::UpdateURLSearchParams()
{
  if (!mSearchParams) {
    return;
  }

  nsAutoCString search;

  nsCOMPtr<nsIURI> uri;
  nsresult rv = GetURI(getter_AddRefs(uri));
  if (NS_WARN_IF(NS_FAILED(rv)) || NS_WARN_IF(!uri)) {
    return;
  }

  nsCOMPtr<nsIURL> url(do_QueryInterface(uri));
  if (url) {
    nsresult rv = url->GetQuery(search);
    if (NS_FAILED(rv)) {
      NS_WARNING("Failed to get the query from a nsIURL.");
    }
  }

  mSearchParams->ParseInput(search, this);
}

void
nsLocation::RemoveURLSearchParams()
{
  if (mSearchParams) {
    mSearchParams->RemoveObserver(this);
    mSearchParams = nullptr;

    nsRefPtr<URLSearchParams> docShellSearchParams = GetDocShellSearchParams();
    if (docShellSearchParams) {
      docShellSearchParams->RemoveObserver(this);
    }
  }
}
