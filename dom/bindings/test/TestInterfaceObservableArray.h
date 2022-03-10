/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TestInterfaceObservableArray_h
#define mozilla_dom_TestInterfaceObservableArray_h

#include "nsCOMPtr.h"
#include "nsWrapperCache.h"
#include "nsTArray.h"

class nsPIDOMWindowInner;

namespace mozilla {

class ErrorResult;

namespace dom {

class GlobalObject;
class SetDeleteBooleanCallback;
class SetDeleteInterfaceCallback;
class SetDeleteObjectCallback;
struct ObservableArrayCallbacks;

// Implementation of test binding for webidl ObservableArray type, using
// primitives for value type
class TestInterfaceObservableArray final : public nsISupports,
                                           public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(TestInterfaceObservableArray)

  nsPIDOMWindowInner* GetParentObject() const;
  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;
  static already_AddRefed<TestInterfaceObservableArray> Constructor(
      const GlobalObject& aGlobal, const ObservableArrayCallbacks& aCallbacks,
      ErrorResult& rv);

  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  void OnSetObservableArrayObject(JSContext* aCx, JS::Handle<JSObject*> aValue,
                                  uint32_t aIndex, ErrorResult& aRv);

  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  void OnDeleteObservableArrayObject(JSContext* aCx,
                                     JS::Handle<JSObject*> aValue,
                                     uint32_t aIndex, ErrorResult& aRv);

  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  void OnSetObservableArrayBoolean(bool aValue, uint32_t aIndex,
                                   ErrorResult& aRv);

  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  void OnDeleteObservableArrayBoolean(bool aValue, uint32_t aIndex,
                                      ErrorResult& aRv);

  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  void OnSetObservableArrayInterface(TestInterfaceObservableArray* aValue,
                                     uint32_t aIndex, ErrorResult& aRv);

  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  void OnDeleteObservableArrayInterface(TestInterfaceObservableArray* aValue,
                                        uint32_t aIndex, ErrorResult& aRv);

 private:
  explicit TestInterfaceObservableArray(
      nsPIDOMWindowInner* aParent, const ObservableArrayCallbacks& aCallbacks);
  virtual ~TestInterfaceObservableArray() = default;

  nsCOMPtr<nsPIDOMWindowInner> mParent;
  RefPtr<SetDeleteBooleanCallback> mSetBooleanCallback;
  RefPtr<SetDeleteBooleanCallback> mDeleteBooleanCallback;
  RefPtr<SetDeleteObjectCallback> mSetObjectCallback;
  RefPtr<SetDeleteObjectCallback> mDeleteObjectCallback;
  RefPtr<SetDeleteInterfaceCallback> mSetInterfaceCallback;
  RefPtr<SetDeleteInterfaceCallback> mDeleteInterfaceCallback;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_TestInterfaceObservableArray_h
