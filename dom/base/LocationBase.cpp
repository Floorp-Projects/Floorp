/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/LocationBase.h"
#include "nsIScriptSecurityManager.h"
#include "nsIScriptContext.h"
#include "nsDocShellLoadState.h"
#include "nsIWebNavigation.h"
#include "nsNetUtil.h"
#include "nsCOMPtr.h"
#include "nsError.h"
#include "nsContentUtils.h"
#include "nsGlobalWindow.h"
#include "mozilla/dom/Document.h"

namespace mozilla {
namespace dom {

already_AddRefed<nsDocShellLoadState> LocationBase::CheckURL(
    nsIURI* aURI, nsIPrincipal& aSubjectPrincipal, ErrorResult& aRv) {
  RefPtr<BrowsingContext> bc(GetBrowsingContext());
  if (NS_WARN_IF(!bc)) {
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

void LocationBase::SetURI(nsIURI* aURI, nsIPrincipal& aSubjectPrincipal,
                          ErrorResult& aRv, bool aReplace) {
  RefPtr<BrowsingContext> bc = GetBrowsingContext();
  if (bc && !bc->IsDiscarded()) {
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
    RefPtr<BrowsingContext> accessingBC;
    if (sourceWindow) {
      accessingBC = sourceWindow->GetBrowsingContext();
      loadState->SetSourceDocShell(sourceWindow->GetDocShell());
    }

    loadState->SetLoadFlags(nsIWebNavigation::LOAD_FLAGS_NONE);
    loadState->SetFirstParty(true);

    nsresult rv = bc->LoadURI(accessingBC, loadState);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      aRv.Throw(rv);
    }
  }
}

void LocationBase::SetHref(const nsAString& aHref,
                           nsIPrincipal& aSubjectPrincipal, ErrorResult& aRv) {
  DoSetHref(aHref, aSubjectPrincipal, false, aRv);
}

void LocationBase::DoSetHref(const nsAString& aHref,
                             nsIPrincipal& aSubjectPrincipal, bool aReplace,
                             ErrorResult& aRv) {
  // Get the source of the caller
  nsCOMPtr<nsIURI> base = GetSourceBaseURL();
  SetHrefWithBase(aHref, base, aSubjectPrincipal, aReplace, aRv);
}

void LocationBase::SetHrefWithBase(const nsAString& aHref, nsIURI* aBase,
                                   nsIPrincipal& aSubjectPrincipal,
                                   bool aReplace, ErrorResult& aRv) {
  nsresult result;
  nsCOMPtr<nsIURI> newUri;

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
        nsCOMPtr<nsIDocShell> docShell(GetDocShell());
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

void LocationBase::Replace(const nsAString& aUrl,
                           nsIPrincipal& aSubjectPrincipal, ErrorResult& aRv) {
  DoSetHref(aUrl, aSubjectPrincipal, true, aRv);
}

nsIURI* LocationBase::GetSourceBaseURL() {
  Document* doc = GetEntryDocument();

  // If there's no entry document, we either have no Script Entry Point or one
  // that isn't a DOM Window.  This doesn't generally happen with the DOM, but
  // can sometimes happen with extension code in certain IPC configurations.  If
  // this happens, try falling back on the current document associated with the
  // docshell. If that fails, just return null and hope that the caller passed
  // an absolute URI.
  if (!doc) {
    if (nsCOMPtr<nsIDocShell> docShell = GetDocShell()) {
      nsCOMPtr<nsPIDOMWindowOuter> docShellWin =
          do_QueryInterface(docShell->GetScriptGlobalObject());
      if (docShellWin) {
        doc = docShellWin->GetDoc();
      }
    }
  }
  return doc ? doc->GetBaseURI() : nullptr;
}

}  // namespace dom
}  // namespace mozilla
