/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsQueryActor_h
#define nsQueryActor_h

#include "nsCOMPtr.h"
#include "mozilla/RefPtr.h"
#include "mozilla/dom/JSWindowActorChild.h"
#include "mozilla/dom/WindowGlobalChild.h"

class MOZ_STACK_CLASS nsQueryActor final : public nsCOMPtr_helper {
 public:
  nsQueryActor(const nsDependentString aActorName, nsPIDOMWindowOuter* aWindow)
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
  const nsDependentString mActorName;
  nsPIDOMWindowOuter* mWindow;
};

inline nsQueryActor do_QueryActor(const char16_t* aActorName,
                                  nsPIDOMWindowOuter* aWindow) {
  return nsQueryActor(nsDependentString(aActorName), aWindow);
}

#endif  // defined nsQueryActor_h
