/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_JSWindowActorParent_h
#define mozilla_dom_JSWindowActorParent_h

#include "js/TypeDecls.h"
#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/JSWindowActor.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"

namespace mozilla {
namespace dom {

class WindowGlobalParent;

}  // namespace dom
}  // namespace mozilla

namespace mozilla {
namespace dom {

class JSWindowActorParent final : public JSWindowActor {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(JSWindowActorParent,
                                                         JSWindowActor)

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  static already_AddRefed<JSWindowActorParent> Constructor(
      GlobalObject& aGlobal, ErrorResult& aRv) {
    return MakeAndAddRef<JSWindowActorParent>();
  }

  WindowGlobalParent* Manager() const;
  void Init(const nsAString& aName, WindowGlobalParent* aManager);
  void StartDestroy();
  void AfterDestroy();

 protected:
  void SendRawMessage(const JSWindowActorMessageMeta& aMeta,
                      ipc::StructuredCloneData&& aData,
                      ErrorResult& aRv) override;

 private:
  ~JSWindowActorParent();

  bool mCanSend = true;
  RefPtr<WindowGlobalParent> mManager;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_JSWindowActorParent_h
