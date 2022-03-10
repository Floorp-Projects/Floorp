/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/TestInterfaceObservableArray.h"
#include "mozilla/dom/TestInterfaceObservableArrayBinding.h"
#include "nsPIDOMWindow.h"
#include "mozilla/dom/BindingUtils.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(TestInterfaceObservableArray, mParent,
                                      mSetBooleanCallback,
                                      mDeleteBooleanCallback,
                                      mSetObjectCallback, mDeleteObjectCallback,
                                      mSetInterfaceCallback,
                                      mDeleteInterfaceCallback)

NS_IMPL_CYCLE_COLLECTING_ADDREF(TestInterfaceObservableArray)
NS_IMPL_CYCLE_COLLECTING_RELEASE(TestInterfaceObservableArray)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(TestInterfaceObservableArray)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

TestInterfaceObservableArray::TestInterfaceObservableArray(
    nsPIDOMWindowInner* aParent, const ObservableArrayCallbacks& aCallbacks)
    : mParent(aParent) {
  if (aCallbacks.mSetBooleanCallback.WasPassed()) {
    mSetBooleanCallback = &aCallbacks.mSetBooleanCallback.Value();
  }
  if (aCallbacks.mDeleteBooleanCallback.WasPassed()) {
    mDeleteBooleanCallback = &aCallbacks.mDeleteBooleanCallback.Value();
  }
  if (aCallbacks.mSetObjectCallback.WasPassed()) {
    mSetObjectCallback = &aCallbacks.mSetObjectCallback.Value();
  }
  if (aCallbacks.mDeleteObjectCallback.WasPassed()) {
    mDeleteObjectCallback = &aCallbacks.mDeleteObjectCallback.Value();
  }
  if (aCallbacks.mSetInterfaceCallback.WasPassed()) {
    mSetInterfaceCallback = &aCallbacks.mSetInterfaceCallback.Value();
  }
  if (aCallbacks.mDeleteInterfaceCallback.WasPassed()) {
    mDeleteInterfaceCallback = &aCallbacks.mDeleteInterfaceCallback.Value();
  }
}

// static
already_AddRefed<TestInterfaceObservableArray>
TestInterfaceObservableArray::Constructor(
    const GlobalObject& aGlobal, const ObservableArrayCallbacks& aCallbacks,
    ErrorResult& aRv) {
  nsCOMPtr<nsPIDOMWindowInner> window =
      do_QueryInterface(aGlobal.GetAsSupports());
  if (!window) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<TestInterfaceObservableArray> r =
      new TestInterfaceObservableArray(window, aCallbacks);
  return r.forget();
}

JSObject* TestInterfaceObservableArray::WrapObject(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return TestInterfaceObservableArray_Binding::Wrap(aCx, this, aGivenProto);
}

nsPIDOMWindowInner* TestInterfaceObservableArray::GetParentObject() const {
  return mParent;
}

void TestInterfaceObservableArray::OnSetObservableArrayObject(
    JSContext* aCx, JS::Handle<JSObject*> aValue, uint32_t aIndex,
    ErrorResult& aRv) {
  if (mSetObjectCallback) {
    MOZ_KnownLive(mSetObjectCallback)
        ->Call(aValue, aIndex, aRv, "OnSetObservableArrayObject",
               CallbackFunction::eRethrowExceptions);
  }
}

void TestInterfaceObservableArray::OnDeleteObservableArrayObject(
    JSContext* aCx, JS::Handle<JSObject*> aValue, uint32_t aIndex,
    ErrorResult& aRv) {
  if (mDeleteObjectCallback) {
    MOZ_KnownLive(mDeleteObjectCallback)
        ->Call(aValue, aIndex, aRv, "OnDeleteObservableArrayObject",
               CallbackFunction::eRethrowExceptions);
  }
}

void TestInterfaceObservableArray::OnSetObservableArrayBoolean(
    bool aValue, uint32_t aIndex, ErrorResult& aRv) {
  if (mSetBooleanCallback) {
    MOZ_KnownLive(mSetBooleanCallback)
        ->Call(aValue, aIndex, aRv, "OnSetObservableArrayBoolean",
               CallbackFunction::eRethrowExceptions);
  }
}

void TestInterfaceObservableArray::OnDeleteObservableArrayBoolean(
    bool aValue, uint32_t aIndex, ErrorResult& aRv) {
  if (mDeleteBooleanCallback) {
    MOZ_KnownLive(mDeleteBooleanCallback)
        ->Call(aValue, aIndex, aRv, "OnDeleteObservableArrayBoolean",
               CallbackFunction::eRethrowExceptions);
  }
}

void TestInterfaceObservableArray::OnSetObservableArrayInterface(
    TestInterfaceObservableArray* aValue, uint32_t aIndex, ErrorResult& aRv) {
  if (mSetInterfaceCallback && aValue) {
    MOZ_KnownLive(mSetInterfaceCallback)
        ->Call(*aValue, aIndex, aRv, "OnSetObservableArrayInterface",
               CallbackFunction::eRethrowExceptions);
  }
}

void TestInterfaceObservableArray::OnDeleteObservableArrayInterface(
    TestInterfaceObservableArray* aValue, uint32_t aIndex, ErrorResult& aRv) {
  if (mDeleteInterfaceCallback && aValue) {
    MOZ_KnownLive(mDeleteInterfaceCallback)
        ->Call(*aValue, aIndex, aRv, "OnDeleteObservableArrayInterface",
               CallbackFunction::eRethrowExceptions);
  }
}

bool TestInterfaceObservableArray::BooleanElementAtInternal(uint32_t aIndex,
                                                            ErrorResult& aRv) {
  return TestInterfaceObservableArray_Binding::ObservableArrayBooleanHelpers::
      ElementAt(this, aIndex, aRv);
}

void TestInterfaceObservableArray::ObjectElementAtInternal(
    JSContext* aCx, uint32_t aIndex, JS::MutableHandle<JSObject*> aValue,
    ErrorResult& aRv) {
  TestInterfaceObservableArray_Binding::ObservableArrayObjectHelpers::ElementAt(
      this, aCx, aIndex, aValue, aRv);
}

already_AddRefed<TestInterfaceObservableArray>
TestInterfaceObservableArray::InterfaceElementAtInternal(uint32_t aIndex,
                                                         ErrorResult& aRv) {
  return TestInterfaceObservableArray_Binding::ObservableArrayInterfaceHelpers::
      ElementAt(this, aIndex, aRv);
}

void TestInterfaceObservableArray::BooleanReplaceElementAtInternal(
    uint32_t aIndex, bool aValue, ErrorResult& aRv) {
  TestInterfaceObservableArray_Binding::ObservableArrayBooleanHelpers::
      ReplaceElementAt(this, aIndex, aValue, aRv);
}

void TestInterfaceObservableArray::ObjectReplaceElementAtInternal(
    JSContext* aCx, uint32_t aIndex, JS::Handle<JSObject*> aValue,
    ErrorResult& aRv) {
  TestInterfaceObservableArray_Binding::ObservableArrayObjectHelpers::
      ReplaceElementAt(this, aIndex, aValue, aRv);
}

void TestInterfaceObservableArray::InterfaceReplaceElementAtInternal(
    uint32_t aIndex, TestInterfaceObservableArray& aValue, ErrorResult& aRv) {
  TestInterfaceObservableArray_Binding::ObservableArrayInterfaceHelpers::
      ReplaceElementAt(this, aIndex, aValue, aRv);
}

void TestInterfaceObservableArray::BooleanAppendElementInternal(
    bool aValue, ErrorResult& aRv) {
  TestInterfaceObservableArray_Binding::ObservableArrayBooleanHelpers::
      AppendElement(this, aValue, aRv);
}

void TestInterfaceObservableArray::ObjectAppendElementInternal(
    JSContext* aCx, JS::Handle<JSObject*> aValue, ErrorResult& aRv) {
  TestInterfaceObservableArray_Binding::ObservableArrayObjectHelpers::
      AppendElement(this, aValue, aRv);
}

void TestInterfaceObservableArray::InterfaceAppendElementInternal(
    TestInterfaceObservableArray& aValue, ErrorResult& aRv) {
  TestInterfaceObservableArray_Binding::ObservableArrayInterfaceHelpers::
      AppendElement(this, aValue, aRv);
}

void TestInterfaceObservableArray::BooleanRemoveLastElementInternal(
    ErrorResult& aRv) {
  TestInterfaceObservableArray_Binding::ObservableArrayBooleanHelpers::
      RemoveLastElement(this, aRv);
}

void TestInterfaceObservableArray::ObjectRemoveLastElementInternal(
    ErrorResult& aRv) {
  TestInterfaceObservableArray_Binding::ObservableArrayObjectHelpers::
      RemoveLastElement(this, aRv);
}

void TestInterfaceObservableArray::InterfaceRemoveLastElementInternal(
    ErrorResult& aRv) {
  TestInterfaceObservableArray_Binding::ObservableArrayInterfaceHelpers::
      RemoveLastElement(this, aRv);
}

uint32_t TestInterfaceObservableArray::BooleanLengthInternal(ErrorResult& aRv) {
  return TestInterfaceObservableArray_Binding::ObservableArrayBooleanHelpers::
      Length(this, aRv);
}

uint32_t TestInterfaceObservableArray::ObjectLengthInternal(ErrorResult& aRv) {
  return TestInterfaceObservableArray_Binding::ObservableArrayObjectHelpers::
      Length(this, aRv);
}

uint32_t TestInterfaceObservableArray::InterfaceLengthInternal(
    ErrorResult& aRv) {
  return TestInterfaceObservableArray_Binding::ObservableArrayInterfaceHelpers::
      Length(this, aRv);
}

}  // namespace mozilla::dom
