/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_UnderlyingSinkCallbackHelpers_h
#define mozilla_dom_UnderlyingSinkCallbackHelpers_h

#include "mozilla/HoldDropJSObjects.h"
#include "mozilla/dom/ModuleMapKey.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/UnderlyingSinkBinding.h"
#include "nsISupports.h"
#include "nsISupportsImpl.h"

namespace mozilla::dom {
class WritableStreamDefaultController;
}

/*
 * See the comment in UnderlyingSourceCallbackHelpers.h!
 *
 * A native implementation of these callbacks is however currently not required.
 */
namespace mozilla::dom {
class UnderlyingSinkStartCallbackHelper : public nsISupports {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(
      UnderlyingSinkStartCallbackHelper)

  UnderlyingSinkStartCallbackHelper(UnderlyingSinkStartCallback* aCallback,
                                    JS::Handle<JSObject*> aUnderlyingSink)
      : mUnderlyingSink(aUnderlyingSink), mCallback(aCallback) {
    mozilla::HoldJSObjects(this);
  }

  MOZ_CAN_RUN_SCRIPT
  void StartCallback(JSContext* aCx,
                     WritableStreamDefaultController& aController,
                     JS::MutableHandle<JS::Value> aRetVal, ErrorResult& aRv);

 protected:
  virtual ~UnderlyingSinkStartCallbackHelper() {
    mozilla::DropJSObjects(this);
  };

 private:
  JS::Heap<JSObject*> mUnderlyingSink;
  RefPtr<UnderlyingSinkStartCallback> mCallback;
};

class UnderlyingSinkWriteCallbackHelper : public nsISupports {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(
      UnderlyingSinkWriteCallbackHelper)

  explicit UnderlyingSinkWriteCallbackHelper(
      UnderlyingSinkWriteCallback* aCallback,
      JS::Handle<JSObject*> aUnderlyingSink)
      : mUnderlyingSink(aUnderlyingSink), mCallback(aCallback) {
    MOZ_ASSERT(mCallback);
    mozilla::HoldJSObjects(this);
  }

  MOZ_CAN_RUN_SCRIPT
  already_AddRefed<Promise> WriteCallback(
      JSContext* aCx, JS::Handle<JS::Value> aChunk,
      WritableStreamDefaultController& aController, ErrorResult& aRv);

 protected:
  virtual ~UnderlyingSinkWriteCallbackHelper() { mozilla::DropJSObjects(this); }

 private:
  JS::Heap<JSObject*> mUnderlyingSink;
  RefPtr<UnderlyingSinkWriteCallback> mCallback;
};

class UnderlyingSinkCloseCallbackHelper : public nsISupports {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(
      UnderlyingSinkCloseCallbackHelper)

  UnderlyingSinkCloseCallbackHelper(UnderlyingSinkCloseCallback* aCallback,
                                    JS::Handle<JSObject*> aUnderlyingSink)
      : mUnderlyingSink(aUnderlyingSink), mCallback(aCallback) {
    MOZ_ASSERT(mCallback);
    mozilla::HoldJSObjects(this);
  }

  MOZ_CAN_RUN_SCRIPT
  already_AddRefed<Promise> CloseCallback(JSContext* aCx, ErrorResult& aRv);

 protected:
  virtual ~UnderlyingSinkCloseCallbackHelper() { mozilla::DropJSObjects(this); }

 private:
  JS::Heap<JSObject*> mUnderlyingSink;
  RefPtr<UnderlyingSinkCloseCallback> mCallback;
};

// Abstract over the implementation details for the UnderlyinSinkAbortCallback
class UnderlyingSinkAbortCallbackHelper : public nsISupports {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(
      UnderlyingSinkAbortCallbackHelper)

  UnderlyingSinkAbortCallbackHelper(UnderlyingSinkAbortCallback* aCallback,
                                    JS::Handle<JSObject*> aUnderlyingSink)
      : mUnderlyingSink(aUnderlyingSink), mCallback(aCallback) {
    MOZ_ASSERT(mCallback);
    mozilla::HoldJSObjects(this);
  }

  MOZ_CAN_RUN_SCRIPT
  already_AddRefed<Promise> AbortCallback(
      JSContext* aCx, const Optional<JS::Handle<JS::Value>>& aReason,
      ErrorResult& aRv);

 protected:
  virtual ~UnderlyingSinkAbortCallbackHelper() { mozilla::DropJSObjects(this); }

 private:
  JS::Heap<JSObject*> mUnderlyingSink;
  RefPtr<UnderlyingSinkAbortCallback> mCallback;
};

}  // namespace mozilla::dom

#endif
