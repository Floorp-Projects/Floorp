/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_JSProcessActorChild_h
#define mozilla_dom_JSProcessActorChild_h

#include "mozilla/dom/JSActor.h"
#include "nsIDOMProcessChild.h"

namespace mozilla::dom {

// Placeholder implementation.
class JSProcessActorChild final : public JSActor {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(JSProcessActorChild, JSActor)

  explicit JSProcessActorChild(nsISupports* aGlobal = nullptr)
      : JSActor(aGlobal) {}

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  static already_AddRefed<JSProcessActorChild> Constructor(
      GlobalObject& aGlobal) {
    return MakeAndAddRef<JSProcessActorChild>(aGlobal.GetAsSupports());
  }

  nsIDOMProcessChild* Manager() const { return mManager; }

  void Init(const nsACString& aName, nsIDOMProcessChild* aManager);
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
  ~JSProcessActorChild() { MOZ_ASSERT(!mManager); }

  nsCOMPtr<nsIDOMProcessChild> mManager;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_JSProcessActorChild_h
