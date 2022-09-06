/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_JSWindowActorChild_h
#define mozilla_dom_JSWindowActorChild_h

#include "js/RootingAPI.h"
#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/RefPtr.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/JSActor.h"
#include "mozilla/dom/WindowGlobalChild.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIGlobalObject.h"
#include "nsISupports.h"
#include "nsStringFwd.h"

class nsIDocShell;

namespace mozilla {
class ErrorResult;

namespace dom {

template <typename>
struct Nullable;

class BrowsingContext;
class Document;
class WindowProxyHolder;

}  // namespace dom
}  // namespace mozilla

namespace mozilla::dom {

class JSWindowActorChild final : public JSActor {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(JSWindowActorChild, JSActor)

  explicit JSWindowActorChild(nsISupports* aGlobal = nullptr)
      : JSActor(aGlobal) {}

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  static already_AddRefed<JSWindowActorChild> Constructor(
      GlobalObject& aGlobal) {
    return MakeAndAddRef<JSWindowActorChild>(aGlobal.GetAsSupports());
  }

  WindowGlobalChild* GetManager() const;
  WindowContext* GetWindowContext() const;
  void Init(const nsACString& aName, WindowGlobalChild* aManager);
  void ClearManager() override;
  Document* GetDocument(ErrorResult& aRv);
  BrowsingContext* GetBrowsingContext(ErrorResult& aRv);
  nsIDocShell* GetDocShell(ErrorResult& aRv);
  Nullable<WindowProxyHolder> GetContentWindow(ErrorResult& aRv);

 protected:
  void SendRawMessage(const JSActorMessageMeta& aMeta,
                      Maybe<ipc::StructuredCloneData>&& aData,
                      Maybe<ipc::StructuredCloneData>&& aStack,
                      ErrorResult& aRv) override;

 private:
  ~JSWindowActorChild();

  RefPtr<WindowGlobalChild> mManager;

  nsCOMPtr<nsIGlobalObject> mGlobal;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_JSWindowActorChild_h
