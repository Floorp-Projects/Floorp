/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsQueryActor_h
#define nsQueryActor_h

#include "nsCOMPtr.h"
#include "mozilla/RefPtr.h"
#include "mozilla/dom/CanonicalBrowsingContext.h"
#include "mozilla/dom/JSWindowActorChild.h"
#include "mozilla/dom/JSWindowActorParent.h"
#include "mozilla/dom/WindowGlobalChild.h"
#include "mozilla/dom/WindowGlobalParent.h"

// This type is used to get an JSWindowActorChild given a window. It
// only has any use within a child process.
class MOZ_STACK_CLASS nsQueryActorChild final : public nsCOMPtr_helper {
 public:
  nsQueryActorChild(const nsLiteralCString aActorName,
                    nsPIDOMWindowOuter* aWindow)
      : mActorName(aActorName), mWindow(aWindow) {}

  virtual nsresult NS_FASTCALL operator()(const nsIID& aIID,
                                          void** aResult) const override {
    if (NS_WARN_IF(!mWindow) || !mWindow->GetCurrentInnerWindow()) {
      return NS_ERROR_NO_INTERFACE;
    }

    RefPtr<mozilla::dom::WindowGlobalChild> wgc(
        mWindow->GetCurrentInnerWindow()->GetWindowGlobalChild());
    if (!wgc) {
      return NS_ERROR_NO_INTERFACE;
    }

    RefPtr<mozilla::dom::JSWindowActorChild> actor =
        wgc->GetActor(mActorName, mozilla::IgnoreErrors());
    if (!actor) {
      return NS_ERROR_NO_INTERFACE;
    }

    return actor->QueryInterfaceActor(aIID, aResult);
  }

 private:
  const nsLiteralCString mActorName;
  nsPIDOMWindowOuter* mWindow;
};

template <size_t length>
inline nsQueryActorChild do_QueryActor(const char (&aActorName)[length],
                                       nsPIDOMWindowOuter* aWindow) {
  return nsQueryActorChild(nsLiteralCString(aActorName), aWindow);
}

// This type is used to get an actor given a CanonicalBrowsingContext. It
// is only useful in the parent process.
class MOZ_STACK_CLASS nsQueryActorParent final : public nsCOMPtr_helper {
 public:
  nsQueryActorParent(nsLiteralCString aActorName,
                     mozilla::dom::CanonicalBrowsingContext* aBrowsingContext)
      : mActorName(aActorName), mBrowsingContext(aBrowsingContext) {}

  virtual nsresult NS_FASTCALL operator()(const nsIID& aIID,
                                          void** aResult) const override {
    if (!mBrowsingContext) {
      return NS_ERROR_NO_INTERFACE;
    }

    RefPtr<mozilla::dom::WindowGlobalParent> wgp =
        mBrowsingContext->GetCurrentWindowGlobal();
    if (!wgp) {
      return NS_ERROR_NO_INTERFACE;
    }

    RefPtr<mozilla::dom::JSWindowActorParent> actor =
        wgp->GetActor(mActorName, mozilla::IgnoreErrors());
    if (!actor) {
      return NS_ERROR_NO_INTERFACE;
    }

    return actor->QueryInterfaceActor(aIID, aResult);
  }

 private:
  const nsLiteralCString mActorName;
  mozilla::dom::CanonicalBrowsingContext* mBrowsingContext;
};

template <size_t length>
inline nsQueryActorParent do_QueryActor(
    const char (&aActorName)[length],
    mozilla::dom::CanonicalBrowsingContext* aBrowsingContext) {
  return nsQueryActorParent(nsLiteralCString(aActorName), aBrowsingContext);
}

#endif  // defined nsQueryActor_h
