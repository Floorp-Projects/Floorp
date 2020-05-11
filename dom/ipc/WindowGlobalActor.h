/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_WindowGlobalActor_h
#define mozilla_dom_WindowGlobalActor_h

#include "nsWrapperCache.h"
#include "nsISupports.h"
#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/ErrorResult.h"
#include "nsIURI.h"
#include "nsString.h"
#include "mozilla/dom/JSActor.h"

namespace mozilla {
namespace dom {

// Common base class for WindowGlobal{Parent, Child}.
class WindowGlobalActor : public nsISupports {
 public:
  // Called to determine initial state for a window global actor created for an
  // initial about:blank document.
  static WindowGlobalInit AboutBlankInitializer(
      dom::BrowsingContext* aBrowsingContext, nsIPrincipal* aPrincipal);

  // Called to determine initial state for a window global actor created for a
  // specific existing nsGlobalWindowInner.
  static WindowGlobalInit WindowInitializer(nsGlobalWindowInner* aWindow);

 protected:
  virtual ~WindowGlobalActor() = default;

  // Load the module for the named Window Actor and contruct it.
  // This method will not initialize the actor or set its manager,
  // which is handled by callers.
  void ConstructActor(const nsACString& aName, JS::MutableHandleObject aActor,
                      ErrorResult& aRv);
  virtual nsIURI* GetDocumentURI() = 0;
  virtual const nsAString& GetRemoteType() = 0;
  virtual JSActor::Type GetSide() = 0;
  virtual dom::BrowsingContext* BrowsingContext() = 0;

  static WindowGlobalInit BaseInitializer(
      dom::BrowsingContext* aBrowsingContext, uint64_t aInnerWindowId,
      uint64_t aOuterWindowId);
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_WindowGlobalActor_h
