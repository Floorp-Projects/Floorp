/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_JSWindowActorChild_h
#define mozilla_dom_JSWindowActorChild_h

#include "js/TypeDecls.h"
#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"

namespace mozilla {
namespace dom {

template <typename>
struct Nullable;

class Document;
class WindowGlobalChild;
class WindowProxyHolder;

}  // namespace dom
}  // namespace mozilla

namespace mozilla {
namespace dom {

class JSWindowActorChild final : public nsISupports, public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(JSWindowActorChild)

 protected:
  ~JSWindowActorChild() = default;

 public:
  nsISupports* GetParentObject() const;

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  static already_AddRefed<JSWindowActorChild> Constructor(GlobalObject& aGlobal,
                                                          ErrorResult& aRv) {
    return MakeAndAddRef<JSWindowActorChild>();
  }

  WindowGlobalChild* Manager() const;
  void Init(const nsAString& aName, WindowGlobalChild* aManager);
  void SendAsyncMessage(JSContext* aCx, const nsAString& aMessageName,
                        JS::Handle<JS::Value> aObj,
                        JS::Handle<JS::Value> aTransfers, ErrorResult& aRv);

  Document* GetDocument(ErrorResult& aRv);
  BrowsingContext* GetBrowsingContext(ErrorResult& aRv);
  Nullable<WindowProxyHolder> GetContentWindow(ErrorResult& aRv);

 private:
  nsString mName;
  RefPtr<WindowGlobalChild> mManager;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_JSWindowActorChild_h
