/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsQueryActor_h
#define nsQueryActor_h

#include <type_traits>

#include "nsCOMPtr.h"
#include "nsPIDOMWindow.h"
#include "mozilla/RefPtr.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/JSActor.h"
#include "mozilla/dom/JSActorManager.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/WindowGlobalChild.h"

class nsIDOMProcessChild;
class nsIDOMProcessParent;

// This type is used to get an XPCOM interface implemented by a JSActor from its
// native manager.
class MOZ_STACK_CLASS nsQueryJSActor final : public nsCOMPtr_helper {
 public:
  nsQueryJSActor(const nsLiteralCString aActorName,
                 mozilla::dom::JSActorManager* aManager)
      : mActorName(aActorName), mManager(aManager) {}

  nsresult NS_FASTCALL operator()(const nsIID& aIID,
                                  void** aResult) const override {
    if (!mManager) {
      return NS_ERROR_NO_INTERFACE;
    }

    mozilla::dom::AutoJSAPI jsapi;
    jsapi.Init();

    RefPtr<mozilla::dom::JSActor> actor =
        mManager->GetActor(jsapi.cx(), mActorName, mozilla::IgnoreErrors());
    if (!actor) {
      return NS_ERROR_NO_INTERFACE;
    }

    return actor->QueryInterfaceActor(aIID, aResult);
  }

 private:
  const nsLiteralCString mActorName;
  mozilla::dom::JSActorManager* mManager;
};

// Request an XPCOM interface from a JSActor managed by `aManager`.
//
// These APIs will work with both JSWindowActors and JSProcessActors.
template <size_t length>
inline nsQueryJSActor do_QueryActor(const char (&aActorName)[length],
                                    mozilla::dom::JSActorManager* aManager) {
  return nsQueryJSActor(nsLiteralCString(aActorName), aManager);
}

// Overload for directly querying a JSWindowActorChild from an inner window.
template <size_t length>
inline nsQueryJSActor do_QueryActor(const char (&aActorName)[length],
                                    nsPIDOMWindowInner* aWindow) {
  return nsQueryJSActor(nsLiteralCString(aActorName),
                        aWindow ? aWindow->GetWindowGlobalChild() : nullptr);
}

// Overload for directly querying a JSWindowActorChild from a document.
template <size_t length>
inline nsQueryJSActor do_QueryActor(const char (&aActorName)[length],
                                    mozilla::dom::Document* aDoc) {
  return nsQueryJSActor(nsLiteralCString(aActorName),
                        aDoc ? aDoc->GetWindowGlobalChild() : nullptr);
}

// Overload for directly querying from a nsIDOMProcess{Parent,Child} without
// confusing overload selection for types inheriting from both
// nsIDOMProcess{Parent,Child} and JSActorManager.
template <size_t length, typename T,
          typename = std::enable_if_t<std::is_same_v<T, nsIDOMProcessParent> ||
                                      std::is_same_v<T, nsIDOMProcessChild>>>
inline nsQueryJSActor do_QueryActor(const char (&aActorName)[length],
                                    T* aManager) {
  return nsQueryJSActor(nsLiteralCString(aActorName),
                        aManager ? aManager->AsJSActorManager() : nullptr);
}

#endif  // defined nsQueryActor_h
