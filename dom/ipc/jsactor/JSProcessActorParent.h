/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_JSProcessActorParent_h
#define mozilla_dom_JSProcessActorParent_h

#include "js/TypeDecls.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/JSActor.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIDOMProcessParent.h"
#include "nsWrapperCache.h"

namespace mozilla {
class ErrorResult;

namespace dom {

class JSProcessActorParent final : public JSActor {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(JSProcessActorParent, JSActor)

  explicit JSProcessActorParent(nsISupports* aGlobal = nullptr)
      : JSActor(aGlobal) {}

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  static already_AddRefed<JSProcessActorParent> Constructor(
      GlobalObject& aGlobal) {
    return MakeAndAddRef<JSProcessActorParent>(aGlobal.GetAsSupports());
  }

  nsIDOMProcessParent* Manager() const { return mManager; }

  void Init(const nsACString& aName, nsIDOMProcessParent* aManager);
  void ClearManager() override;

 protected:
  // Send the message described by the structured clone data |aData|, and the
  // message metadata |aMetadata|. The underlying transport should call the
  // |ReceiveMessage| method on the other side asynchronously.
  virtual void SendRawMessage(const JSActorMessageMeta& aMetadata,
                              Maybe<ipc::StructuredCloneData>&& aData,
                              Maybe<ipc::StructuredCloneData>&& aStack,
                              ErrorResult& aRv) override;

 private:
  ~JSProcessActorParent();

  nsCOMPtr<nsIDOMProcessParent> mManager;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_JSProcessActorParent_h
