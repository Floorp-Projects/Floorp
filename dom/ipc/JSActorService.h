/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_JSActorService_h
#define mozilla_dom_JSActorService_h

#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/ErrorResult.h"
#include "nsIURI.h"
#include "nsRefPtrHashtable.h"
#include "nsString.h"
#include "nsTArray.h"
#include "mozilla/dom/JSActor.h"

#include "nsIObserver.h"
#include "nsIDOMEventListener.h"
#include "mozilla/EventListenerManager.h"
#include "mozilla/extensions/WebExtensionContentScript.h"

namespace mozilla {
namespace dom {
struct WindowActorOptions;
class JSWindowActorInfo;
class EventTarget;

class JSActorService final {
 public:
  NS_INLINE_DECL_REFCOUNTING(JSActorService)

  static already_AddRefed<JSActorService> GetSingleton();

  // Register or unregister a chrome event target.
  void RegisterChromeEventTarget(EventTarget* aTarget);

  // NOTE: This method is static, as it may be called during shutdown.
  static void UnregisterChromeEventTarget(EventTarget* aTarget);

  // Register child's Actor for content process.
  void LoadJSActorInfos(nsTArray<JSProcessActorInfo>& aProcess,
                        nsTArray<JSWindowActorInfo>& aWindow);

  // --- Window Actor

  void RegisterWindowActor(const nsACString& aName,
                           const WindowActorOptions& aOptions,
                           ErrorResult& aRv);

  void UnregisterWindowActor(const nsACString& aName);

  // Register child's Window Actor from JSWindowActorInfos for content process.
  void LoadJSWindowActorInfos(nsTArray<JSWindowActorInfo>& aInfos);

  // Get the named of Window Actor and the child's WindowActorOptions
  // from mDescriptors to JSWindowActorInfos.
  void GetJSWindowActorInfos(nsTArray<JSWindowActorInfo>& aInfos);

  already_AddRefed<JSWindowActorProtocol> GetJSWindowActorProtocol(
      const nsACString& aName);

 private:
  JSActorService();
  ~JSActorService();

  nsTArray<EventTarget*> mChromeEventTargets;

  // -- Window Actor
  nsRefPtrHashtable<nsCStringHashKey, JSWindowActorProtocol>
      mWindowActorDescriptors;

  // -- Content Actor
  nsRefPtrHashtable<nsCStringHashKey, JSProcessActorProtocol>
      mProcessActorDescriptors;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_JSActorService_h
