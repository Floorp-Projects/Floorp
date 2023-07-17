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
#include "nsReadableUtils.h"
#include "nsContentUtils.h"
#include "mozilla/dom/WindowContext.h"
#include "mozilla/dom/Location.h"
#include "mozilla/RefPtr.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/BasePrincipal.h"

using namespace mozilla;
using namespace mozilla::dom;

extern LazyLogModule gSHistoryLog;

#define LOG(format) MOZ_LOG(gSHistoryLog, mozilla::LogLevel::Debug, format)

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

nsHistory::~nsHistory() = default;

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
    return 1;
  }

  int32_t len = sHistory->Count();
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

  aRv = doc->GetStateObject(aResult);
}

void nsHistory::Go(int32_t aDelta, nsIPrincipal& aSubjectPrincipal,
                   ErrorResult& aRv) {
  LOG(("nsHistory::Go(%d)", aDelta));
  nsCOMPtr<nsPIDOMWindowInner> win(do_QueryReferent(mInnerWindow));
  if (!win || !win->HasActiveDocument()) {
    return aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
  }

  if (!aDelta) {
    // https://html.spec.whatwg.org/multipage/history.html#the-history-interface
    // "When the go(delta) method is invoked, if delta is zero, the user agent
    // must act as if the location.reload() method was called instead."
    RefPtr<Location> location = win->Location();
    return location->Reload(false, aSubjectPrincipal, aRv);
  }

  RefPtr<ChildSHistory> session_history = GetSessionHistory();
  if (!session_history) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  bool userActivation =
      win->GetWindowContext()
          ? win->GetWindowContext()->HasValidTransientUserGestureActivation()
          : false;

  CallerType callerType = aSubjectPrincipal.IsSystemPrincipal()
                              ? CallerType::System
                              : CallerType::NonSystem;

  // AsyncGo throws if we hit the location change rate limit.
  session_history->AsyncGo(aDelta, /* aRequireUserInteraction = */ false,
                           userActivation, callerType, aRv);
}

void nsHistory::Back(CallerType aCallerType, ErrorResult& aRv) {
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

  bool userActivation =
      win->GetWindowContext()
          ? win->GetWindowContext()->HasValidTransientUserGestureActivation()
          : false;

  sHistory->AsyncGo(-1, /* aRequireUserInteraction = */ false, userActivation,
                    aCallerType, aRv);
}

void nsHistory::Forward(CallerType aCallerType, ErrorResult& aRv) {
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

  bool userActivation =
      win->GetWindowContext()
          ? win->GetWindowContext()->HasValidTransientUserGestureActivation()
          : false;

  sHistory->AsyncGo(1, /* aRequireUserInteraction = */ false, userActivation,
                    aCallerType, aRv);
}

void nsHistory::PushState(JSContext* aCx, JS::Handle<JS::Value> aData,
                          const nsAString& aTitle, const nsAString& aUrl,
                          CallerType aCallerType, ErrorResult& aRv) {
  PushOrReplaceState(aCx, aData, aTitle, aUrl, aCallerType, aRv, false);
}

void nsHistory::ReplaceState(JSContext* aCx, JS::Handle<JS::Value> aData,
                             const nsAString& aTitle, const nsAString& aUrl,
                             CallerType aCallerType, ErrorResult& aRv) {
  PushOrReplaceState(aCx, aData, aTitle, aUrl, aCallerType, aRv, true);
}

void nsHistory::PushOrReplaceState(JSContext* aCx, JS::Handle<JS::Value> aData,
                                   const nsAString& aTitle,
                                   const nsAString& aUrl,
                                   CallerType aCallerType, ErrorResult& aRv,
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

  BrowsingContext* bc = win->GetBrowsingContext();
  if (bc) {
    nsresult rv = bc->CheckLocationChangeRateLimit(aCallerType);
    if (NS_FAILED(rv)) {
      aRv.Throw(rv);
      return;
    }
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

already_AddRefed<ChildSHistory> nsHistory::GetSessionHistory() const {
  nsCOMPtr<nsPIDOMWindowInner> win = do_QueryReferent(mInnerWindow);
  NS_ENSURE_TRUE(win, nullptr);

  BrowsingContext* bc = win->GetBrowsingContext();
  NS_ENSURE_TRUE(bc, nullptr);

  RefPtr<ChildSHistory> childSHistory = bc->Top()->GetChildSessionHistory();
  return childSHistory.forget();
}
