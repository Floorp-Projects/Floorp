/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_JSWindowActorChild_h
#define mozilla_dom_JSWindowActorChild_h

#include "js/TypeDecls.h"
#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/JSActor.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"
#include "nsIContentChild.h"

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

class JSWindowActorChild final : public JSActor {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(JSWindowActorChild,
                                                         JSActor)

  explicit JSWindowActorChild(nsIGlobalObject* aGlobal = nullptr);

  nsIGlobalObject* GetParentObject() const override { return mGlobal; }

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  static already_AddRefed<JSWindowActorChild> Constructor(
      GlobalObject& aGlobal) {
    nsCOMPtr<nsIGlobalObject> global(
        do_QueryInterface(aGlobal.GetAsSupports()));
    return MakeAndAddRef<JSWindowActorChild>(global);
  }

  WindowGlobalChild* GetManager() const;
  void Init(const nsACString& aName, WindowGlobalChild* aManager);
  void StartDestroy();
  void AfterDestroy();
  Document* GetDocument(ErrorResult& aRv);
  BrowsingContext* GetBrowsingContext(ErrorResult& aRv);
  nsIDocShell* GetDocShell(ErrorResult& aRv);
  Nullable<WindowProxyHolder> GetContentWindow(ErrorResult& aRv);

 protected:
  void SendRawMessage(const JSActorMessageMeta& aMeta,
                      ipc::StructuredCloneData&& aData,
                      ipc::StructuredCloneData&& aStack,
                      ErrorResult& aRv) override;

 private:
  ~JSWindowActorChild();

  bool mCanSend = true;
  RefPtr<WindowGlobalChild> mManager;

  nsCOMPtr<nsIGlobalObject> mGlobal;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_JSWindowActorChild_h
