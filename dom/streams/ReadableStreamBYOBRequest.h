/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ReadableStreamBYOBRequest_h
#define mozilla_dom_ReadableStreamBYOBRequest_h

#include "js/RootingAPI.h"
#include "js/TypeDecls.h"
#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/HoldDropJSObjects.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/TypedArray.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"
#include "nsIGlobalObject.h"

namespace mozilla {
namespace dom {

class ReadableByteStreamController;

class ReadableStreamBYOBRequest final : public nsISupports,
                                        public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(ReadableStreamBYOBRequest)

 public:
  explicit ReadableStreamBYOBRequest(nsIGlobalObject* aGlobal)
      : mGlobal(aGlobal) {
    mozilla::HoldJSObjects(this);
  }

 protected:
  ~ReadableStreamBYOBRequest() { mozilla::DropJSObjects(this); };

 public:
  nsIGlobalObject* GetParentObject() const { return mGlobal; }

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  void GetView(JSContext* cx, JS::MutableHandle<JSObject*> aRetVal) const;

  void Respond(JSContext* aCx, uint64_t bytesWritten, ErrorResult& aRv);

  void RespondWithNewView(JSContext* aCx, const ArrayBufferView& view,
                          ErrorResult& aRv);

  ReadableByteStreamController* Controller() { return mController; }
  void SetController(ReadableByteStreamController* aController) {
    mController = aController;
  }

  JSObject* View() { return mView; }
  void SetView(JS::HandleObject aView) { mView = aView; }

 private:
  // Internal Slots
  nsCOMPtr<nsIGlobalObject> mGlobal;
  RefPtr<ReadableByteStreamController> mController;
  JS::Heap<JSObject*> mView;
};

}  // namespace dom
}  // namespace mozilla

#endif
