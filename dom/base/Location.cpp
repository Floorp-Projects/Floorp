/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Location.h"
#include "nsIScriptSecurityManager.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsIScriptContext.h"
#include "nsDocShellLoadState.h"
#include "nsIWebNavigation.h"
#include "nsIURIFixup.h"
#include "nsIURL.h"
#include "nsIURIMutator.h"
#include "nsIJARURI.h"
#include "nsNetUtil.h"
#include "nsCOMPtr.h"
#include "nsEscape.h"
#include "nsIDOMWindow.h"
#include "nsPresContext.h"
#include "nsError.h"
#include "nsReadableUtils.h"
#include "nsITextToSubURI.h"
#include "nsJSUtils.h"
#include "nsContentUtils.h"
#include "nsDocShell.h"
#include "nsGlobalWindow.h"
#include "mozilla/Likely.h"
#include "nsCycleCollectionParticipant.h"
#include "mozilla/Components.h"
#include "mozilla/NullPrincipal.h"
#include "mozilla/Unused.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/DocumentInlines.h"
#include "mozilla/dom/LocationBinding.h"
#include "mozilla/dom/ScriptSettings.h"
#include "ReferrerInfo.h"

namespace mozilla {
namespace dom {

Location::Location(nsPIDOMWindowInner* aWindow, nsIDocShell* aDocShell)
    : mInnerWindow(aWindow) {
  // aDocShell can be null if it gets called after nsDocShell::Destory().
  mDocShell = do_GetWeakReference(aDocShell);
}

Location::~Location() {}

// QueryInterface implementation for Location
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(Location)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(Location, mInnerWindow)

NS_IMPL_CYCLE_COLLECTING_ADDREF(Location)
NS_IMPL_CYCLE_COLLECTING_RELEASE(Location)

already_AddRefed<nsDocShellLoadState> Location::CheckURL(
    nsIURI* aURI, nsIPrincipal& aSubjectPrincipal, ErrorResult& aRv) {
  nsCOMPtr<nsIDocShell> docShell(do_QueryReferent(mDocShell));
  if (NS_WARN_IF(!docShell)) {
    aRv.Throw(NS_ERROR_NOT_AVAILABLE);
    return nullptr;
  }

  nsCOMPtr<nsIPrincipal> triggeringPrincipal;
  nsCOMPtr<nsIURI> sourceURI;
  ReferrerPolicy referrerPolicy = ReferrerPolicy::_empty;

  // Get security manager.
  nsIScriptSecurityManager* ssm = nsContentUtils::GetSecurityManager();
  if (NS_WARN_IF(!ssm)) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  // Check to see if URI is allowed.
  nsresult rv = ssm->CheckLoadURIWithPrincipal(
      &aSubjectPrincipal, aURI, nsIScriptSecurityManager::STANDARD);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    nsAutoCString spec;
    aURI->GetSpec(spec);
    aRv.ThrowTypeError<MSG_URL_NOT_LOADABLE>(NS_ConvertUTF8toUTF16(spec));
    return nullptr;
  }

  // Make the load's referrer reflect changes to the document's URI caused by
  // push/replaceState, if possible.  First, get the document corresponding to
  // fp.  If the document's original URI (i.e. its URI before
  // push/replaceState) matches the principal's URI, use the document's
  // current URI as the referrer.  If they don't match, use the principal's
  // URI.
  //
  // The triggering principal for this load should be the principal of the
  // incumbent document (which matches where the referrer information is
  // coming from) when there is an incumbent document, and the subject
  // principal otherwise.  Note that the URI in the triggering principal
  // may not match the referrer URI in various cases, notably including
  // the cases when the incumbent document's document URI was modified
  // after the document was loaded.

  nsCOMPtr<nsPIDOMWindowInner> incumbent =
      do_QueryInterface(mozilla::dom::GetIncumbentGlobal());
  nsCOMPtr<Document> doc = incumbent ? incumbent->GetDoc() : nullptr;

  if (doc) {
    nsCOMPtr<nsIURI> docOriginalURI, docCurrentURI, principalURI;
    docOriginalURI = doc->GetOriginalURI();
    docCurrentURI = doc->GetDocumentURI();
    rv = doc->NodePrincipal()->GetURI(getter_AddRefs(principalURI));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      aRv.Throw(rv);
      return nullptr;
    }

    triggeringPrincipal = doc->NodePrincipal();
    referrerPolicy = doc->GetReferrerPolicy();

    bool urisEqual = false;
    if (docOriginalURI && docCurrentURI && principalURI) {
      principalURI->Equals(docOriginalURI, &urisEqual);
    }
    if (urisEqual) {
      sourceURI = docCurrentURI;
    } else {
      // Use principalURI as long as it is not an NullPrincipalURI.  We
      // could add a method such as GetReferrerURI to principals to make this
      // cleaner, but given that we need to start using Source Browsing
      // Context for referrer (see Bug 960639) this may be wasted effort at
      // this stage.
      if (principalURI && !principalURI->SchemeIs(NS_NULLPRINCIPAL_SCHEME)) {
        sourceURI = principalURI;
      }
    }
  } else {
    // No document; just use our subject principal as the triggering principal.
    triggeringPrincipal = &aSubjectPrincipal;
  }

  // Create load info
  RefPtr<nsDocShellLoadState> loadState = new nsDocShellLoadState(aURI);

  loadState->SetTriggeringPrincipal(triggeringPrincipal);
  if (doc) {
    loadState->SetCsp(doc->GetCsp());
  }

  if (sourceURI) {
    nsCOMPtr<nsIReferrerInfo> referrerInfo =
        new ReferrerInfo(sourceURI, referrerPolicy);
    loadState->SetReferrerInfo(referrerInfo);
  }

  return loadState.forget();
}

nsresult Location::GetURI(nsIURI** aURI, bool aGetInnermostURI) {
  *aURI = nullptr;

  nsCOMPtr<nsIDocShell> docShell(do_QueryReferent(mDocShell));
  if (!docShell) {
    return NS_OK;
  }

  nsresult rv;
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

  nsCOMPtr<nsIURIFixup> urifixup(components::URIFixup::Service());

  return urifixup->CreateExposableURI(uri, aURI);
}

void Location::SetURI(nsIURI* aURI, nsIPrincipal& aSubjectPrincipal,
                      ErrorResult& aRv, bool aReplace) {
  nsCOMPtr<nsIDocShell> docShell(do_QueryReferent(mDocShell));
  if (docShell) {
    RefPtr<nsDocShellLoadState> loadState =
        CheckURL(aURI, aSubjectPrincipal, aRv);
    if (aRv.Failed()) {
      return;
    }

    if (aReplace) {
      loadState->SetLoadType(LOAD_STOP_CONTENT_AND_REPLACE);
    } else {
      loadState->SetLoadType(LOAD_STOP_CONTENT);
    }

    // Get the incumbent script's browsing context to set as source.
    nsCOMPtr<nsPIDOMWindowInner> sourceWindow =
        do_QueryInterface(mozilla::dom::GetIncumbentGlobal());
    if (sourceWindow) {
      loadState->SetSourceDocShell(sourceWindow->GetDocShell());
    }

    loadState->SetLoadFlags(nsIWebNavigation::LOAD_FLAGS_NONE);
    loadState->SetFirstParty(true);

    nsresult rv = docShell->LoadURI(loadState);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      aRv.Throw(rv);
    }
  }
}

void Location::GetHash(nsAString& aHash, nsIPrincipal& aSubjectPrincipal,
                       ErrorResult& aRv) {
  if (!CallerSubsumes(&aSubjectPrincipal)) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  aHash.SetLength(0);

  nsCOMPtr<nsIURI> uri;
  aRv = GetURI(getter_AddRefs(uri));
  if (NS_WARN_IF(aRv.Failed()) || !uri) {
    return;
  }

  nsAutoCString ref;
  nsAutoString unicodeRef;

  aRv = uri->GetRef(ref);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  if (!ref.IsEmpty()) {
    aHash.Assign(char16_t('#'));
    AppendUTF8toUTF16(ref, aHash);
  }

  if (aHash == mCachedHash) {
    // Work around ShareThis stupidly polling location.hash every
    // 5ms all the time by handing out the same exact string buffer
    // we handed out last time.
    aHash = mCachedHash;
  } else {
    mCachedHash = aHash;
  }
}

void Location::SetHash(const nsAString& aHash, nsIPrincipal& aSubjectPrincipal,
                       ErrorResult& aRv) {
  if (!CallerSubsumes(&aSubjectPrincipal)) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  NS_ConvertUTF16toUTF8 hash(aHash);
  if (hash.IsEmpty() || hash.First() != char16_t('#')) {
    hash.Insert(char16_t('#'), 0);
  }

  nsCOMPtr<nsIURI> uri;
  aRv = GetURI(getter_AddRefs(uri));
  if (NS_WARN_IF(aRv.Failed()) || !uri) {
    return;
  }

  aRv = NS_MutateURI(uri).SetRef(hash).Finalize(uri);
  if (NS_WARN_IF(aRv.Failed()) || !uri) {
    return;
  }

  SetURI(uri, aSubjectPrincipal, aRv);
}

void Location::GetHost(nsAString& aHost, nsIPrincipal& aSubjectPrincipal,
                       ErrorResult& aRv) {
  if (!CallerSubsumes(&aSubjectPrincipal)) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

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
}

void Location::SetHost(const nsAString& aHost, nsIPrincipal& aSubjectPrincipal,
                       ErrorResult& aRv) {
  if (!CallerSubsumes(&aSubjectPrincipal)) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  nsCOMPtr<nsIURI> uri;
  aRv = GetURI(getter_AddRefs(uri));
  if (NS_WARN_IF(aRv.Failed()) || !uri) {
    return;
  }

  aRv =
      NS_MutateURI(uri).SetHostPort(NS_ConvertUTF16toUTF8(aHost)).Finalize(uri);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  SetURI(uri, aSubjectPrincipal, aRv);
}

void Location::GetHostname(nsAString& aHostname,
                           nsIPrincipal& aSubjectPrincipal, ErrorResult& aRv) {
  if (!CallerSubsumes(&aSubjectPrincipal)) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  aHostname.Truncate();

  nsCOMPtr<nsIURI> uri;
  GetURI(getter_AddRefs(uri), true);
  if (uri) {
    nsContentUtils::GetHostOrIPv6WithBrackets(uri, aHostname);
  }
}

void Location::SetHostname(const nsAString& aHostname,
                           nsIPrincipal& aSubjectPrincipal, ErrorResult& aRv) {
  if (!CallerSubsumes(&aSubjectPrincipal)) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  nsCOMPtr<nsIURI> uri;
  aRv = GetURI(getter_AddRefs(uri));
  if (NS_WARN_IF(aRv.Failed()) || !uri) {
    return;
  }

  aRv =
      NS_MutateURI(uri).SetHost(NS_ConvertUTF16toUTF8(aHostname)).Finalize(uri);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  SetURI(uri, aSubjectPrincipal, aRv);
}

nsresult Location::GetHref(nsAString& aHref) {
  aHref.Truncate();

  nsCOMPtr<nsIURI> uri;
  nsresult rv = GetURI(getter_AddRefs(uri));
  if (NS_WARN_IF(NS_FAILED(rv)) || !uri) {
    return rv;
  }

  nsAutoCString uriString;
  rv = uri->GetSpec(uriString);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  AppendUTF8toUTF16(uriString, aHref);
  return NS_OK;
}

void Location::SetHref(const nsAString& aHref, nsIPrincipal& aSubjectPrincipal,
                       ErrorResult& aRv) {
  DoSetHref(aHref, aSubjectPrincipal, false, aRv);
}

void Location::DoSetHref(const nsAString& aHref,
                         nsIPrincipal& aSubjectPrincipal, bool aReplace,
                         ErrorResult& aRv) {
  // Get the source of the caller
  nsCOMPtr<nsIURI> base = GetSourceBaseURL();
  SetHrefWithBase(aHref, base, aSubjectPrincipal, aReplace, aRv);
}

void Location::SetHrefWithBase(const nsAString& aHref, nsIURI* aBase,
                               nsIPrincipal& aSubjectPrincipal, bool aReplace,
                               ErrorResult& aRv) {
  nsresult result;
  nsCOMPtr<nsIURI> newUri;

  nsCOMPtr<nsIDocShell> docShell(do_QueryReferent(mDocShell));

  if (Document* doc = GetEntryDocument()) {
    result = NS_NewURI(getter_AddRefs(newUri), aHref,
                       doc->GetDocumentCharacterSet(), aBase);
  } else {
    result = NS_NewURI(getter_AddRefs(newUri), aHref, nullptr, aBase);
  }

  if (newUri) {
    /* Check with the scriptContext if it is currently processing a script tag.
     * If so, this must be a <script> tag with a location.href in it.
     * we want to do a replace load, in such a situation.
     * In other cases, for example if a event handler or a JS timer
     * had a location.href in it, we want to do a normal load,
     * so that the new url will be appended to Session History.
     * This solution is tricky. Hopefully it isn't going to bite
     * anywhere else. This is part of solution for bug # 39938, 72197
     */
    bool inScriptTag = false;
    nsIScriptContext* scriptContext = nullptr;
    nsCOMPtr<nsPIDOMWindowInner> win = do_QueryInterface(GetEntryGlobal());
    if (win) {
      scriptContext = nsGlobalWindowInner::Cast(win)->GetContextInternal();
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

    SetURI(newUri, aSubjectPrincipal, aRv, aReplace || inScriptTag);
    return;
  }

  aRv.Throw(result);
}

void Location::GetOrigin(nsAString& aOrigin, nsIPrincipal& aSubjectPrincipal,
                         ErrorResult& aRv) {
  if (!CallerSubsumes(&aSubjectPrincipal)) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  aOrigin.Truncate();

  nsCOMPtr<nsIURI> uri;
  aRv = GetURI(getter_AddRefs(uri), true);
  if (NS_WARN_IF(aRv.Failed()) || !uri) {
    return;
  }

  nsAutoString origin;
  aRv = nsContentUtils::GetUTFOrigin(uri, origin);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  aOrigin = origin;
}

void Location::GetPathname(nsAString& aPathname,
                           nsIPrincipal& aSubjectPrincipal, ErrorResult& aRv) {
  if (!CallerSubsumes(&aSubjectPrincipal)) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  aPathname.Truncate();

  nsCOMPtr<nsIURI> uri;
  aRv = GetURI(getter_AddRefs(uri));
  if (NS_WARN_IF(aRv.Failed()) || !uri) {
    return;
  }

  nsAutoCString file;

  aRv = uri->GetFilePath(file);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  AppendUTF8toUTF16(file, aPathname);
}

void Location::SetPathname(const nsAString& aPathname,
                           nsIPrincipal& aSubjectPrincipal, ErrorResult& aRv) {
  if (!CallerSubsumes(&aSubjectPrincipal)) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  nsCOMPtr<nsIURI> uri;
  aRv = GetURI(getter_AddRefs(uri));
  if (NS_WARN_IF(aRv.Failed()) || !uri) {
    return;
  }

  nsresult rv = NS_MutateURI(uri)
                    .SetFilePath(NS_ConvertUTF16toUTF8(aPathname))
                    .Finalize(uri);
  if (NS_FAILED(rv)) {
    return;
  }

  SetURI(uri, aSubjectPrincipal, aRv);
}

void Location::GetPort(nsAString& aPort, nsIPrincipal& aSubjectPrincipal,
                       ErrorResult& aRv) {
  if (!CallerSubsumes(&aSubjectPrincipal)) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  aPort.SetLength(0);

  nsCOMPtr<nsIURI> uri;
  aRv = GetURI(getter_AddRefs(uri), true);
  if (NS_WARN_IF(aRv.Failed()) || !uri) {
    return;
  }

  int32_t port;
  nsresult result = uri->GetPort(&port);

  // Don't propagate this exception to caller
  if (NS_SUCCEEDED(result) && -1 != port) {
    nsAutoString portStr;
    portStr.AppendInt(port);
    aPort.Append(portStr);
  }
}

void Location::SetPort(const nsAString& aPort, nsIPrincipal& aSubjectPrincipal,
                       ErrorResult& aRv) {
  if (!CallerSubsumes(&aSubjectPrincipal)) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  nsCOMPtr<nsIURI> uri;
  aRv = GetURI(getter_AddRefs(uri));
  if (NS_WARN_IF(aRv.Failed() || !uri)) {
    return;
  }

  // perhaps use nsReadingIterators at some point?
  NS_ConvertUTF16toUTF8 portStr(aPort);
  const char* buf = portStr.get();
  int32_t port = -1;

  if (!portStr.IsEmpty() && buf) {
    if (*buf == ':') {
      port = atol(buf + 1);
    } else {
      port = atol(buf);
    }
  }

  aRv = NS_MutateURI(uri).SetPort(port).Finalize(uri);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  SetURI(uri, aSubjectPrincipal, aRv);
}

void Location::GetProtocol(nsAString& aProtocol,
                           nsIPrincipal& aSubjectPrincipal, ErrorResult& aRv) {
  if (!CallerSubsumes(&aSubjectPrincipal)) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  aProtocol.SetLength(0);

  nsCOMPtr<nsIURI> uri;
  aRv = GetURI(getter_AddRefs(uri));
  if (NS_WARN_IF(aRv.Failed()) || !uri) {
    return;
  }

  nsAutoCString protocol;

  aRv = uri->GetScheme(protocol);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  CopyASCIItoUTF16(protocol, aProtocol);
  aProtocol.Append(char16_t(':'));
}

void Location::SetProtocol(const nsAString& aProtocol,
                           nsIPrincipal& aSubjectPrincipal, ErrorResult& aRv) {
  if (!CallerSubsumes(&aSubjectPrincipal)) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  nsCOMPtr<nsIURI> uri;
  aRv = GetURI(getter_AddRefs(uri));
  if (NS_WARN_IF(aRv.Failed()) || !uri) {
    return;
  }

  nsAString::const_iterator start, end;
  aProtocol.BeginReading(start);
  aProtocol.EndReading(end);
  nsAString::const_iterator iter(start);
  Unused << FindCharInReadable(':', iter, end);

  nsresult rv = NS_MutateURI(uri)
                    .SetScheme(NS_ConvertUTF16toUTF8(Substring(start, iter)))
                    .Finalize(uri);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    // Oh, I wish nsStandardURL returned NS_ERROR_MALFORMED_URI for _all_ the
    // malformed cases, not just some of them!
    aRv.Throw(NS_ERROR_DOM_SYNTAX_ERR);
    return;
  }

  nsAutoCString newSpec;
  aRv = uri->GetSpec(newSpec);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }
  // We may want a new URI class for the new URI, so recreate it:
  rv = NS_NewURI(getter_AddRefs(uri), newSpec);
  if (NS_FAILED(rv)) {
    if (rv == NS_ERROR_MALFORMED_URI) {
      rv = NS_ERROR_DOM_SYNTAX_ERR;
    }

    aRv.Throw(rv);
    return;
  }

  if (!uri->SchemeIs("http") && !uri->SchemeIs("https")) {
    // No-op, per spec.
    return;
  }

  SetURI(uri, aSubjectPrincipal, aRv);
}

void Location::GetSearch(nsAString& aSearch, nsIPrincipal& aSubjectPrincipal,
                         ErrorResult& aRv) {
  if (!CallerSubsumes(&aSubjectPrincipal)) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

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
}

void Location::SetSearch(const nsAString& aSearch,
                         nsIPrincipal& aSubjectPrincipal, ErrorResult& aRv) {
  if (!CallerSubsumes(&aSubjectPrincipal)) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  nsCOMPtr<nsIURI> uri;
  aRv = GetURI(getter_AddRefs(uri));
  nsCOMPtr<nsIURL> url(do_QueryInterface(uri));
  if (NS_WARN_IF(aRv.Failed()) || !url) {
    return;
  }

  if (Document* doc = GetEntryDocument()) {
    aRv = NS_MutateURI(uri)
              .SetQueryWithEncoding(NS_ConvertUTF16toUTF8(aSearch),
                                    doc->GetDocumentCharacterSet())
              .Finalize(uri);
  } else {
    aRv = NS_MutateURI(uri)
              .SetQuery(NS_ConvertUTF16toUTF8(aSearch))
              .Finalize(uri);
  }
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  SetURI(uri, aSubjectPrincipal, aRv);
}

void Location::Reload(bool aForceget, ErrorResult& aRv) {
  nsCOMPtr<nsIDocShell> docShell(do_QueryReferent(mDocShell));
  if (!docShell) {
    return aRv.Throw(NS_ERROR_FAILURE);
  }

  if (StaticPrefs::dom_block_reload_from_resize_event_handler()) {
    nsCOMPtr<nsPIDOMWindowOuter> window = docShell->GetWindow();
    if (window && window->IsHandlingResizeEvent()) {
      // location.reload() was called on a window that is handling a
      // resize event. Sites do this since Netscape 4.x needed it, but
      // we don't, and it's a horrible experience for nothing. In stead
      // of reloading the page, just clear style data and reflow the
      // page since some sites may use this trick to work around gecko
      // reflow bugs, and this should have the same effect.
      RefPtr<Document> doc = window->GetExtantDoc();

      nsPresContext* pcx;
      if (doc && (pcx = doc->GetPresContext())) {
        pcx->RebuildAllStyleData(NS_STYLE_HINT_REFLOW,
                                 RestyleHint::RestyleSubtree());
      }
      return;
    }
  }

  uint32_t reloadFlags = nsIWebNavigation::LOAD_FLAGS_NONE;

  if (aForceget) {
    reloadFlags = nsIWebNavigation::LOAD_FLAGS_BYPASS_CACHE |
                  nsIWebNavigation::LOAD_FLAGS_BYPASS_PROXY;
  }

  nsresult rv = nsDocShell::Cast(docShell)->Reload(reloadFlags);
  if (NS_FAILED(rv) && rv != NS_BINDING_ABORTED) {
    // NS_BINDING_ABORTED is returned when we attempt to reload a POST result
    // and the user says no at the "do you want to reload?" prompt.  Don't
    // propagate this one back to callers.
    return aRv.Throw(rv);
  }
}

void Location::Replace(const nsAString& aUrl, nsIPrincipal& aSubjectPrincipal,
                       ErrorResult& aRv) {
  DoSetHref(aUrl, aSubjectPrincipal, true, aRv);
}

void Location::Assign(const nsAString& aUrl, nsIPrincipal& aSubjectPrincipal,
                      ErrorResult& aRv) {
  if (!CallerSubsumes(&aSubjectPrincipal)) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  DoSetHref(aUrl, aSubjectPrincipal, false, aRv);
}

nsIURI* Location::GetSourceBaseURL() {
  Document* doc = GetEntryDocument();
  // If there's no entry document, we either have no Script Entry Point or one
  // that isn't a DOM Window.  This doesn't generally happen with the DOM, but
  // can sometimes happen with extension code in certain IPC configurations.  If
  // this happens, try falling back on the current document associated with the
  // docshell. If that fails, just return null and hope that the caller passed
  // an absolute URI.
  nsCOMPtr<nsIDocShell> docShell(do_QueryReferent(mDocShell));
  if (!doc && docShell) {
    nsCOMPtr<nsPIDOMWindowOuter> docShellWin =
        do_QueryInterface(docShell->GetScriptGlobalObject());
    if (docShellWin) {
      doc = docShellWin->GetDoc();
    }
  }
  NS_ENSURE_TRUE(doc, nullptr);
  return doc->GetBaseURI();
}

bool Location::CallerSubsumes(nsIPrincipal* aSubjectPrincipal) {
  MOZ_ASSERT(aSubjectPrincipal);

  // Get the principal associated with the location object.  Note that this is
  // the principal of the page which will actually be navigated, not the
  // principal of the Location object itself.  This is why we need this check
  // even though we only allow limited cross-origin access to Location objects
  // in general.
  nsCOMPtr<nsPIDOMWindowOuter> outer = mInnerWindow->GetOuterWindow();
  if (MOZ_UNLIKELY(!outer)) return false;
  nsCOMPtr<nsIScriptObjectPrincipal> sop = do_QueryInterface(outer);
  bool subsumes = false;
  nsresult rv = aSubjectPrincipal->SubsumesConsideringDomain(
      sop->GetPrincipal(), &subsumes);
  NS_ENSURE_SUCCESS(rv, false);
  return subsumes;
}

JSObject* Location::WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) {
  return Location_Binding::Wrap(aCx, this, aGivenProto);
}

}  // namespace dom
}  // namespace mozilla
