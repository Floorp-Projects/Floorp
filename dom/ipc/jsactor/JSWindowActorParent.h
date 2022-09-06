/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_JSWindowActorParent_h
#define mozilla_dom_JSWindowActorParent_h

#include "js/TypeDecls.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/JSActor.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"

namespace mozilla {
class ErrorResult;

namespace dom {

class WindowGlobalParent;

}  // namespace dom
}  // namespace mozilla

namespace mozilla::dom {

class JSWindowActorParent final : public JSActor {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(JSWindowActorParent, JSActor)

  explicit JSWindowActorParent(nsISupports* aGlobal = nullptr)
      : JSActor(aGlobal) {}

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  static already_AddRefed<JSWindowActorParent> Constructor(
      GlobalObject& aGlobal) {
    return MakeAndAddRef<JSWindowActorParent>(aGlobal.GetAsSupports());
  }

  WindowGlobalParent* GetManager() const;
  WindowContext* GetWindowContext() const;
  void Init(const nsACString& aName, WindowGlobalParent* aManager);
  void ClearManager() override;
  CanonicalBrowsingContext* GetBrowsingContext(ErrorResult& aRv);

 protected:
  void SendRawMessage(const JSActorMessageMeta& aMeta,
                      Maybe<ipc::StructuredCloneData>&& aData,
                      Maybe<ipc::StructuredCloneData>&& aStack,
                      ErrorResult& aRv) override;

 private:
  ~JSWindowActorParent();

  RefPtr<WindowGlobalParent> mManager;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_JSWindowActorParent_h
