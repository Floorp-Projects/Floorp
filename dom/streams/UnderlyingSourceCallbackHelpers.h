/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_UnderlyingSourceCallbackHelpers_h
#define mozilla_dom_UnderlyingSourceCallbackHelpers_h

#include "mozilla/HoldDropJSObjects.h"
#include "mozilla/dom/ModuleMapKey.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/ReadableStreamDefaultController.h"
#include "mozilla/dom/UnderlyingSourceBinding.h"
#include "nsISupports.h"
#include "nsISupportsImpl.h"

/* Since the streams specification has native descriptions of some callbacks
 * (i.e. described in prose, rather than provided by user code), we need to be
 * able to pass around native callbacks. To handle this, we define polymorphic
 * classes That cover the difference between native callback and user-provided.
 *
 * The Streams specification wants us to invoke these callbacks, run through
 * WebIDL as if they were methods. So we have to preserve the underlying object
 * to use as the This value on invocation.
 */
namespace mozilla::dom {

class BodyStreamHolder;

// Note: Until we need to be able to provide a native implementation of start,
// I don't distinguish between UnderlyingSourceStartCallbackHelper and  a
// hypothetical IDLUnderlingSourceStartCallbackHelper
class UnderlyingSourceStartCallbackHelper : public nsISupports {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(
      UnderlyingSourceStartCallbackHelper)

  UnderlyingSourceStartCallbackHelper(UnderlyingSourceStartCallback* aCallback,
                                      JS::HandleObject aThisObj)
      : mThisObj(aThisObj), mCallback(aCallback) {
    mozilla::HoldJSObjects(this);
  }

  // The fundamental Call Primitive
  MOZ_CAN_RUN_SCRIPT
  void StartCallback(JSContext* aCx, ReadableStreamController& aController,
                     JS::MutableHandle<JS::Value> aRetVal, ErrorResult& aRv);

 protected:
  virtual ~UnderlyingSourceStartCallbackHelper() {
    mozilla::DropJSObjects(this);
  };

 private:
  JS::Heap<JSObject*> mThisObj;
  RefPtr<UnderlyingSourceStartCallback> mCallback;
};

// Abstract over the implementation details for the UnderlyingSourcePullCallback
class UnderlyingSourcePullCallbackHelper : public nsISupports {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(
      UnderlyingSourcePullCallbackHelper)

  // The fundamental Call Primitive
  MOZ_CAN_RUN_SCRIPT
  virtual already_AddRefed<Promise> PullCallback(
      JSContext* aCx, ReadableStreamController& aController,
      ErrorResult& aRv) = 0;

 protected:
  virtual ~UnderlyingSourcePullCallbackHelper() = default;
};

// Invoke IDL passed, user provided callback.
class IDLUnderlyingSourcePullCallbackHelper final
    : public UnderlyingSourcePullCallbackHelper {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(
      IDLUnderlyingSourcePullCallbackHelper, UnderlyingSourcePullCallbackHelper)

  explicit IDLUnderlyingSourcePullCallbackHelper(
      UnderlyingSourcePullCallback* aCallback, JS::HandleObject aThisObj)
      : mThisObj(aThisObj), mCallback(aCallback) {
    MOZ_ASSERT(mCallback);
    mozilla::HoldJSObjects(this);
  }

  MOZ_CAN_RUN_SCRIPT
  virtual already_AddRefed<Promise> PullCallback(
      JSContext* aCx, ReadableStreamController& aController,
      ErrorResult& aRv) override;

 protected:
  virtual ~IDLUnderlyingSourcePullCallbackHelper() {
    mozilla::DropJSObjects(this);
  }

 private:
  JS::Heap<JSObject*> mThisObj;
  RefPtr<UnderlyingSourcePullCallback> mCallback;
};

class BodyStreamUnderlyingSourcePullCallbackHelper final
    : public UnderlyingSourcePullCallbackHelper {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(
      BodyStreamUnderlyingSourcePullCallbackHelper,
      UnderlyingSourcePullCallbackHelper)

  explicit BodyStreamUnderlyingSourcePullCallbackHelper(
      BodyStreamHolder* underlyingSource);

  MOZ_CAN_RUN_SCRIPT
  virtual already_AddRefed<Promise> PullCallback(
      JSContext* aCx, ReadableStreamController& aController,
      ErrorResult& aRv) override;

 protected:
  virtual ~BodyStreamUnderlyingSourcePullCallbackHelper() = default;

 private:
  RefPtr<BodyStreamHolder> mUnderlyingSource;
};

class UnderlyingSourceCancelCallbackHelper : public nsISupports {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(
      UnderlyingSourceCancelCallbackHelper)

  MOZ_CAN_RUN_SCRIPT
  virtual already_AddRefed<Promise> CancelCallback(
      JSContext* aCx, const Optional<JS::Handle<JS::Value>>& aReason,
      ErrorResult& aRv) = 0;

 protected:
  virtual ~UnderlyingSourceCancelCallbackHelper() = default;
};

// Invoke IDL passed, user provided callback.
class IDLUnderlyingSourceCancelCallbackHelper final
    : public UnderlyingSourceCancelCallbackHelper {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(
      IDLUnderlyingSourceCancelCallbackHelper,
      UnderlyingSourceCancelCallbackHelper)

  explicit IDLUnderlyingSourceCancelCallbackHelper(
      UnderlyingSourceCancelCallback* aCallback, JS::HandleObject aThisObj)
      : mThisObj(aThisObj), mCallback(aCallback) {
    MOZ_ASSERT(mCallback);
    mozilla::HoldJSObjects(this);
  }

  MOZ_CAN_RUN_SCRIPT
  virtual already_AddRefed<Promise> CancelCallback(
      JSContext* aCx, const Optional<JS::Handle<JS::Value>>& aReason,
      ErrorResult& aRv) override;

 protected:
  virtual ~IDLUnderlyingSourceCancelCallbackHelper() {
    mozilla::DropJSObjects(this);
  }

 private:
  JS::Heap<JSObject*> mThisObj;
  RefPtr<UnderlyingSourceCancelCallback> mCallback;
};

class BodyStreamUnderlyingSourceCancelCallbackHelper final
    : public UnderlyingSourceCancelCallbackHelper {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(
      BodyStreamUnderlyingSourceCancelCallbackHelper,
      UnderlyingSourceCancelCallbackHelper)

  explicit BodyStreamUnderlyingSourceCancelCallbackHelper(
      BodyStreamHolder* aUnderlyingSource);

  MOZ_CAN_RUN_SCRIPT
  virtual already_AddRefed<Promise> CancelCallback(
      JSContext* aCx, const Optional<JS::Handle<JS::Value>>& aReason,
      ErrorResult& aRv) override;

 protected:
  virtual ~BodyStreamUnderlyingSourceCancelCallbackHelper() = default;

 private:
  RefPtr<BodyStreamHolder> mUnderlyingSource;
};

// Callback called when erroring a stream.
class UnderlyingSourceErrorCallbackHelper : public nsISupports {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(UnderlyingSourceErrorCallbackHelper)

  virtual void Call() = 0;

 protected:
  virtual ~UnderlyingSourceErrorCallbackHelper() = default;
};

class BodyStreamUnderlyingSourceErrorCallbackHelper final
    : public UnderlyingSourceErrorCallbackHelper {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(
      BodyStreamUnderlyingSourceErrorCallbackHelper,
      UnderlyingSourceErrorCallbackHelper)

  explicit BodyStreamUnderlyingSourceErrorCallbackHelper(
      BodyStreamHolder* aUnderlyingSource);

  virtual void Call() override;

 protected:
  virtual ~BodyStreamUnderlyingSourceErrorCallbackHelper() = default;

 private:
  RefPtr<BodyStreamHolder> mUnderlyingSource;
};

}  // namespace mozilla::dom

#endif
