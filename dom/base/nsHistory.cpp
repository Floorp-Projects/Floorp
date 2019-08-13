/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsHistory.h"

#include "jsapi.h"
#include "nsCOMPtr.h"
#include "nsPIDOMWindow.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/DocumentInlines.h"
#include "nsIDocShell.h"
#include "nsIWebNavigation.h"
#include "nsIURI.h"
#include "nsReadableUtils.h"
#include "nsContentUtils.h"
#include "nsISHistory.h"
#include "mozilla/dom/Location.h"
#include "mozilla/Preferences.h"
#include "mozilla/RefPtr.h"

using namespace mozilla;
using namespace mozilla::dom;

//
//  History class implementation
//
NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_0(nsHistory)
NS_IMPL_CYCLE_COLLECTING_ADDREF(nsHistory)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsHistory)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsHistory)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

nsHistory::nsHistory(nsPIDOMWindowInner* aInnerWindow)
    : mInnerWindow(do_GetWeakReference(aInnerWindow)) {}

nsHistory::~nsHistory() {}

nsPIDOMWindowInner* nsHistory::GetParentObject() const {
  nsCOMPtr<nsPIDOMWindowInner> win(do_QueryReferent(mInnerWindow));
  return win;
}

JSObject* nsHistory::WrapObject(JSContext* aCx,
                                JS::Handle<JSObject*> aGivenProto) {
  return History_Binding::Wrap(aCx, this, aGivenProto);
}

uint32_t nsHistory::GetLength(ErrorResult& aRv) const {
  nsCOMPtr<nsPIDOMWindowInner> win(do_QueryReferent(mInnerWindow));
  if (!win || !win->HasActiveDocument()) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);

    return 0;
  }

  // Get session History from docshell
  RefPtr<ChildSHistory> sHistory = GetSessionHistory();
  if (!sHistory) {
    aRv.Throw(NS_ERROR_FAILURE);

    return 0;
  }

  int32_t len = sHistory->Count();
  ;
  return len >= 0 ? len : 0;
}

ScrollRestoration nsHistory::GetScrollRestoration(mozilla::ErrorResult& aRv) {
  nsCOMPtr<nsPIDOMWindowInner> win(do_QueryReferent(mInnerWindow));
  if (!win || !win->HasActiveDocument() || !win->GetDocShell()) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return mozilla::dom::ScrollRestoration::Auto;
  }

  bool currentScrollRestorationIsManual = false;
  win->GetDocShell()->GetCurrentScrollRestorationIsManual(
      &currentScrollRestorationIsManual);
  return currentScrollRestorationIsManual
             ? mozilla::dom::ScrollRestoration::Manual
             : mozilla::dom::ScrollRestoration::Auto;
}

void nsHistory::SetScrollRestoration(mozilla::dom::ScrollRestoration aMode,
                                     mozilla::ErrorResult& aRv) {
  nsCOMPtr<nsPIDOMWindowInner> win(do_QueryReferent(mInnerWindow));
  if (!win || !win->HasActiveDocument() || !win->GetDocShell()) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  win->GetDocShell()->SetCurrentScrollRestorationIsManual(
      aMode == mozilla::dom::ScrollRestoration::Manual);
}

void nsHistory::GetState(JSContext* aCx, JS::MutableHandle<JS::Value> aResult,
                         ErrorResult& aRv) const {
  nsCOMPtr<nsPIDOMWindowInner> win(do_QueryReferent(mInnerWindow));
  if (!win) {
    aRv.Throw(NS_ERROR_NOT_AVAILABLE);
    return;
  }

  if (!win->HasActiveDocument()) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  nsCOMPtr<Document> doc = win->GetExtantDoc();
  if (!doc) {
    aRv.Throw(NS_ERROR_NOT_AVAILABLE);
    return;
  }

  nsCOMPtr<nsIVariant> variant;
  doc->GetStateObject(getter_AddRefs(variant));

  if (variant) {
    aRv = variant->GetAsJSVal(aResult);

    if (aRv.Failed()) {
      return;
    }

    if (!JS_WrapValue(aCx, aResult)) {
      aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    }

    return;
  }

  aResult.setNull();
}

void nsHistory::Go(int32_t aDelta, ErrorResult& aRv) {
  nsCOMPtr<nsPIDOMWindowInner> win(do_QueryReferent(mInnerWindow));
  if (!win || !win->HasActiveDocument()) {
    return aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
  }

  if (!aDelta) {
    // https://html.spec.whatwg.org/multipage/history.html#the-history-interface
    // "When the go(delta) method is invoked, if delta is zero, the user agent
    // must act as if the location.reload() method was called instead."
    RefPtr<Location> location = win->Location();
    return location->Reload(false, aRv);
  }

  RefPtr<ChildSHistory> session_history = GetSessionHistory();
  if (!session_history) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  // Ignore the return value from Go(), since returning errors from Go() can
  // lead to exceptions and a possible leak of history length
  session_history->Go(aDelta, IgnoreErrors());
}

void nsHistory::Back(ErrorResult& aRv) {
  nsCOMPtr<nsPIDOMWindowInner> win(do_QueryReferent(mInnerWindow));
  if (!win || !win->HasActiveDocument()) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);

    return;
  }

  RefPtr<ChildSHistory> sHistory = GetSessionHistory();
  if (!sHistory) {
    aRv.Throw(NS_ERROR_FAILURE);

    return;
  }

  sHistory->Go(-1, IgnoreErrors());
}

void nsHistory::Forward(ErrorResult& aRv) {
  nsCOMPtr<nsPIDOMWindowInner> win(do_QueryReferent(mInnerWindow));
  if (!win || !win->HasActiveDocument()) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);

    return;
  }

  RefPtr<ChildSHistory> sHistory = GetSessionHistory();
  if (!sHistory) {
    aRv.Throw(NS_ERROR_FAILURE);

    return;
  }

  sHistory->Go(1, IgnoreErrors());
}

void nsHistory::PushState(JSContext* aCx, JS::Handle<JS::Value> aData,
                          const nsAString& aTitle, const nsAString& aUrl,
                          ErrorResult& aRv) {
  PushOrReplaceState(aCx, aData, aTitle, aUrl, aRv, false);
}

void nsHistory::ReplaceState(JSContext* aCx, JS::Handle<JS::Value> aData,
                             const nsAString& aTitle, const nsAString& aUrl,
                             ErrorResult& aRv) {
  PushOrReplaceState(aCx, aData, aTitle, aUrl, aRv, true);
}

void nsHistory::PushOrReplaceState(JSContext* aCx, JS::Handle<JS::Value> aData,
                                   const nsAString& aTitle,
                                   const nsAString& aUrl, ErrorResult& aRv,
                                   bool aReplace) {
  nsCOMPtr<nsPIDOMWindowInner> win(do_QueryReferent(mInnerWindow));
  if (!win) {
    aRv.Throw(NS_ERROR_NOT_AVAILABLE);

    return;
  }

  if (!win->HasActiveDocument()) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);

    return;
  }

  // AddState might run scripts, so we need to hold a strong reference to the
  // docShell here to keep it from going away.
  nsCOMPtr<nsIDocShell> docShell = win->GetDocShell();

  if (!docShell) {
    aRv.Throw(NS_ERROR_FAILURE);

    return;
  }

  // The "replace" argument tells the docshell to whether to add a new
  // history entry or modify the current one.

  aRv = docShell->AddState(aData, aTitle, aUrl, aReplace, aCx);
}

nsIDocShell* nsHistory::GetDocShell() const {
  nsCOMPtr<nsPIDOMWindowInner> win = do_QueryReferent(mInnerWindow);
  if (!win) {
    return nullptr;
  }
  return win->GetDocShell();
}

already_AddRefed<ChildSHistory> nsHistory::GetSessionHistory() const {
  nsIDocShell* docShell = GetDocShell();
  NS_ENSURE_TRUE(docShell, nullptr);

  // Get the root DocShell from it
  nsCOMPtr<nsIDocShellTreeItem> root;
  docShell->GetInProcessSameTypeRootTreeItem(getter_AddRefs(root));
  nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(root));
  NS_ENSURE_TRUE(webNav, nullptr);

  // Get SH from nsIWebNavigation
  return webNav->GetSessionHistory();
}
