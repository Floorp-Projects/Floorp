/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_JSProcessActorParent_h
#define mozilla_dom_JSProcessActorParent_h

#include "js/TypeDecls.h"
#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/JSActor.h"
#include "mozilla/extensions/WebExtensionContentScript.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"

namespace mozilla {
namespace dom {

class JSProcessActorParent final : public JSActor {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(JSProcessActorParent,
                                                         JSActor)

  nsIGlobalObject* GetParentObject() const override {
    return xpc::NativeGlobal(xpc::PrivilegedJunkScope());
  }

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  static already_AddRefed<JSProcessActorParent> Constructor(
      GlobalObject& aGlobal) {
    return MakeAndAddRef<JSProcessActorParent>();
  }

  ContentParent* Manager() const { return mManager; }

  void Init(const nsACString& aName, ContentParent* aManager);
  void AfterDestroy();

 protected:
  // Send the message described by the structured clone data |aData|, and the
  // message metadata |aMetadata|. The underlying transport should call the
  // |ReceiveMessage| method on the other side asynchronously.
  virtual void SendRawMessage(const JSActorMessageMeta& aMetadata,
                              ipc::StructuredCloneData&& aData,
                              ipc::StructuredCloneData&& aStack,
                              ErrorResult& aRv) override;

 private:
  ~JSProcessActorParent();

  bool mCanSend = true;
  RefPtr<ContentParent> mManager;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_JSProcessActorParent_h
