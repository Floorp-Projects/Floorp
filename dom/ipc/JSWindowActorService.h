/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_JSWindowActorService_h
#define mozilla_dom_JSWindowActorService_h

#include "nsClassHashtable.h"
#include "nsString.h"

namespace mozilla {
namespace dom {
struct WindowActorOptions;
class JSWindowActorInfo;

class JSWindowActorService final {
 public:
  NS_INLINE_DECL_REFCOUNTING(JSWindowActorService)

  static already_AddRefed<JSWindowActorService> GetSingleton();

  void RegisterWindowActor(const nsAString& aName,
                           const WindowActorOptions& aOptions,
                           ErrorResult& aRv);

  void UnregisterWindowActor(const nsAString& aName);

  // Register child's Window Actor from JSWindowActorInfos for content process.
  void LoadJSWindowActorInfos(nsTArray<JSWindowActorInfo>& aInfos);

  // Get the named of Window Actor and the child's WindowActorOptions
  // from mDescriptors to JSWindowActorInfos.
  void GetJSWindowActorInfos(nsTArray<JSWindowActorInfo>& aInfos);

  // Load the module for the named Window Actor and contruct it.
  // This method will not initialize the actor or set its manager,
  // which is handled by callers.
  void ConstructActor(const nsAString& aName, bool aParentSide,
                      JS::MutableHandleObject aActor, ErrorResult& aRv);

  void ReceiveMessage(JS::RootedObject& aObj, const nsString& aMessageName,
                      ipc::StructuredCloneData& aData);

 private:
  JSWindowActorService();
  ~JSWindowActorService();

  nsClassHashtable<nsStringHashKey, WindowActorOptions> mDescriptors;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_JSWindowActorService_h