/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TestFunctions_h
#define mozilla_dom_TestFunctions_h

#include "mozilla/ErrorResult.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/NonRefcountedDOMObject.h"
#include "mozilla/dom/TestFunctionsBinding.h"
#include "nsString.h"

namespace mozilla {
namespace dom {

class Promise;
class PromiseReturner;
class WrapperCachedNonISupportsTestInterface;

class TestFunctions : public NonRefcountedDOMObject {
 public:
  static UniquePtr<TestFunctions> Constructor(GlobalObject& aGlobal);

  static void ThrowUncatchableException(GlobalObject& aGlobal,
                                        ErrorResult& aRv);

  static Promise* PassThroughPromise(GlobalObject& aGlobal, Promise& aPromise);

  MOZ_CAN_RUN_SCRIPT
  static already_AddRefed<Promise> PassThroughCallbackPromise(
      GlobalObject& aGlobal, PromiseReturner& aCallback, ErrorResult& aRv);

  void SetStringData(const nsAString& aString);

  void GetStringDataAsAString(nsAString& aString);
  void GetStringDataAsAString(uint32_t aLength, nsAString& aString);
  void GetStringDataAsDOMString(const Optional<uint32_t>& aLength,
                                DOMString& aString);

  void GetShortLiteralString(nsAString& aString);
  void GetMediumLiteralString(nsAString& aString);
  void GetLongLiteralString(nsAString& aString);

  void GetStringbufferString(const nsAString& aInput, nsAString& aRetval);

  StringType GetStringType(const nsAString& aString);

  bool StringbufferMatchesStored(const nsAString& aString);

  void TestThrowNsresult(ErrorResult& aError);
  void TestThrowNsresultFromNative(ErrorResult& aError);
  static already_AddRefed<Promise> ThrowToRejectPromise(GlobalObject& aGlobal,
                                                        ErrorResult& aError);

  int32_t One() const;
  int32_t Two() const;

  void SetClampedNullableOctet(const Nullable<uint8_t>& aOctet);
  Nullable<uint8_t> GetClampedNullableOctet() const;
  void SetEnforcedNullableOctet(const Nullable<uint8_t>& aOctet);
  Nullable<uint8_t> GetEnforcedNullableOctet() const;

  void SetArrayBufferView(const ArrayBufferView& aBuffer);
  void GetArrayBufferView(JSContext* aCx, JS::Handle<JSObject*> aObj,
                          JS::MutableHandle<JSObject*> aRetval,
                          ErrorResult& aError);
  void SetAllowSharedArrayBufferView(const ArrayBufferView& aBuffer);
  void GetAllowSharedArrayBufferView(JSContext* aCx, JS::Handle<JSObject*> aObj,
                                     JS::MutableHandle<JSObject*> aRetval,
                                     ErrorResult& aError);
  void SetSequenceOfArrayBufferView(const Sequence<ArrayBufferView>& aBuffers);
  void GetSequenceOfArrayBufferView(JSContext* aCx, JS::Handle<JSObject*> aObj,
                                    nsTArray<JSObject*>& aRetval,
                                    ErrorResult& aError);
  void SetSequenceOfAllowSharedArrayBufferView(
      const Sequence<ArrayBufferView>& aBuffers);
  void GetSequenceOfAllowSharedArrayBufferView(JSContext* aCx,
                                               JS::Handle<JSObject*> aObj,
                                               nsTArray<JSObject*>& aRetval,
                                               ErrorResult& aError);
  void SetArrayBuffer(const ArrayBuffer& aBuffer);
  void GetArrayBuffer(JSContext* aCx, JS::Handle<JSObject*> aObj,
                      JS::MutableHandle<JSObject*> aRetval,
                      ErrorResult& aError);
  void SetAllowSharedArrayBuffer(const ArrayBuffer& aBuffer);
  void GetAllowSharedArrayBuffer(JSContext* aCx, JS::Handle<JSObject*> aObj,
                                 JS::MutableHandle<JSObject*> aRetval,
                                 ErrorResult& aError);
  void SetSequenceOfArrayBuffer(const Sequence<ArrayBuffer>& aBuffers);
  void GetSequenceOfArrayBuffer(JSContext* aCx, JS::Handle<JSObject*> aObj,
                                nsTArray<JSObject*>& aRetval,
                                ErrorResult& aError);
  void SetSequenceOfAllowSharedArrayBuffer(
      const Sequence<ArrayBuffer>& aBuffers);
  void GetSequenceOfAllowSharedArrayBuffer(JSContext* aCx,
                                           JS::Handle<JSObject*> aObj,
                                           nsTArray<JSObject*>& aRetval,
                                           ErrorResult& aError);
  void TestNotAllowShared(const ArrayBufferView& aBuffer);
  void TestNotAllowShared(const ArrayBuffer& aBuffer);
  void TestNotAllowShared(const nsAString& aBuffer);
  void TestAllowShared(const ArrayBufferView& aBuffer);
  void TestAllowShared(const ArrayBuffer& aBuffer);
  void TestDictWithAllowShared(const DictWithAllowSharedBufferSource& aDict);
  void TestUnionOfBuffferSource(
      const ArrayBufferOrArrayBufferViewOrString& aUnion);
  void TestUnionOfAllowSharedBuffferSource(
      const MaybeSharedArrayBufferOrMaybeSharedArrayBufferView& aUnion);

  bool StaticAndNonStaticOverload() { return false; }
  static bool StaticAndNonStaticOverload(GlobalObject& aGlobal,
                                         const Optional<uint32_t>& aLength) {
    return true;
  }

  static bool ObjectFromAboutBlank(JSContext* aCx, JSObject* aObj);

  WrapperCachedNonISupportsTestInterface* WrapperCachedNonISupportsObject();

  bool WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto,
                  JS::MutableHandle<JSObject*> aWrapper);

 private:
  nsString mStringData;
  RefPtr<WrapperCachedNonISupportsTestInterface>
      mWrapperCachedNonISupportsTestInterface;

  Nullable<uint8_t> mClampedNullableOctet;
  Nullable<uint8_t> mEnforcedNullableOctet;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_TestFunctions_h
