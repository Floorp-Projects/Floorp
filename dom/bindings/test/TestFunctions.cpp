/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/TestFunctions.h"
#include "mozilla/dom/TestFunctionsBinding.h"
#include "mozilla/dom/WindowBinding.h"
#include "mozilla/dom/WrapperCachedNonISupportsTestInterface.h"
#include "nsStringBuffer.h"
#include "mozITestInterfaceJS.h"
#include "nsComponentManagerUtils.h"
#include "nsGlobalWindowInner.h"

namespace mozilla {
namespace dom {

/* static */
TestFunctions* TestFunctions::Constructor(GlobalObject& aGlobal) {
  return new TestFunctions;
}

/* static */
void TestFunctions::ThrowUncatchableException(GlobalObject& aGlobal,
                                              ErrorResult& aRv) {
  aRv.ThrowUncatchableException();
}

/* static */
Promise* TestFunctions::PassThroughPromise(GlobalObject& aGlobal,
                                           Promise& aPromise) {
  return &aPromise;
}

/* static */
already_AddRefed<Promise> TestFunctions::PassThroughCallbackPromise(
    GlobalObject& aGlobal, PromiseReturner& aCallback, ErrorResult& aRv) {
  return aCallback.Call(aRv);
}

void TestFunctions::SetStringData(const nsAString& aString) {
  mStringData = aString;
}

void TestFunctions::GetStringDataAsAString(nsAString& aString) {
  aString = mStringData;
}

void TestFunctions::GetStringDataAsAString(uint32_t aLength,
                                           nsAString& aString) {
  MOZ_RELEASE_ASSERT(aLength <= mStringData.Length(),
                     "Bogus test passing in a too-big length");
  aString.Assign(mStringData.BeginReading(), aLength);
}

void TestFunctions::GetStringDataAsDOMString(const Optional<uint32_t>& aLength,
                                             DOMString& aString) {
  uint32_t length;
  if (aLength.WasPassed()) {
    length = aLength.Value();
    MOZ_RELEASE_ASSERT(length <= mStringData.Length(),
                       "Bogus test passing in a too-big length");
  } else {
    length = mStringData.Length();
  }

  nsStringBuffer* buf = nsStringBuffer::FromString(mStringData);
  if (buf) {
    aString.SetKnownLiveStringBuffer(buf, length);
    return;
  }

  // We better have an empty mStringData; otherwise why did we not have a string
  // buffer?
  MOZ_RELEASE_ASSERT(length == 0, "Why no stringbuffer?");
  // No need to do anything here; aString is already empty.
}

void TestFunctions::GetShortLiteralString(nsAString& aString) {
  // JS inline strings can hold 2 * sizeof(void*) chars, which on 32-bit means 8
  // chars.  Return fewer than that.
  aString.AssignLiteral(u"012345");
}

void TestFunctions::GetMediumLiteralString(nsAString& aString) {
  // JS inline strings are at most 2 * sizeof(void*) chars, so at most 16 on
  // 64-bit.  FakeString can hold 63 chars in its inline buffer (plus the null
  // terminator).  Let's return 40 chars; that way if we ever move to 128-bit
  // void* or something this test will still be valid.
  aString.AssignLiteral(u"0123456789012345678901234567890123456789");
}

void TestFunctions::GetLongLiteralString(nsAString& aString) {
  // Need more than 64 chars.
  aString.AssignLiteral(
      u"0123456789012345678901234567890123456789"  // 40
      "0123456789012345678901234567890123456789"   // 80
  );
}

void TestFunctions::GetStringbufferString(const nsAString& aInput,
                                          nsAString& aRetval) {
  // We have to be a bit careful: if aRetval is an autostring, if we just assign
  // it won't cause stringbuffer allocation.  So we have to round-trip through
  // something that definitely causes a stringbuffer allocation.
  nsString str;
  // Can't use operator= here, because if aInput is a literal string then str
  // would end up the same way.
  str.Assign(aInput.BeginReading(), aInput.Length());

  // Now we might end up hitting our external string cache and getting the wrong
  // sort of external string, so replace the last char by a different value
  // (replacing, not just appending, to preserve the length).  If we have an
  // empty string, our caller screwed up and there's not much we can do for
  // them.
  if (str.Length() > 1) {
    char16_t last = str[str.Length() - 1];
    str.Truncate(str.Length() - 1);
    if (last == 'x') {
      str.Append('y');
    } else {
      str.Append('x');
    }
  }

  // Here we use operator= to preserve stringbufferness.
  aRetval = str;
}

StringType TestFunctions::GetStringType(const nsAString& aString) {
  if (aString.IsLiteral()) {
    return StringType::Literal;
  }

  if (nsStringBuffer::FromString(aString)) {
    return StringType::Stringbuffer;
  }

  if (aString.GetDataFlags() & nsAString::DataFlags::INLINE) {
    return StringType::Inline;
  }

  return StringType::Other;
}

bool TestFunctions::StringbufferMatchesStored(const nsAString& aString) {
  return nsStringBuffer::FromString(aString) &&
         nsStringBuffer::FromString(aString) ==
             nsStringBuffer::FromString(mStringData);
}

void TestFunctions::TestThrowNsresult(ErrorResult& aError) {
  nsCOMPtr<mozITestInterfaceJS> impl =
      do_CreateInstance("@mozilla.org/dom/test-interface-js;1");
  aError = impl->TestThrowNsresult();
}

void TestFunctions::TestThrowNsresultFromNative(ErrorResult& aError) {
  nsCOMPtr<mozITestInterfaceJS> impl =
      do_CreateInstance("@mozilla.org/dom/test-interface-js;1");
  aError = impl->TestThrowNsresultFromNative();
}

already_AddRefed<Promise> TestFunctions::ThrowToRejectPromise(
    GlobalObject& aGlobal, ErrorResult& aError) {
  aError.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
  return nullptr;
}

int32_t TestFunctions::One() const { return 1; }

int32_t TestFunctions::Two() const { return 2; }

void TestFunctions::SetClampedNullableOctet(const Nullable<uint8_t>& aOctet) {
  mClampedNullableOctet = aOctet;
}

Nullable<uint8_t> TestFunctions::GetClampedNullableOctet() const {
  return mClampedNullableOctet;
}

void TestFunctions::SetEnforcedNullableOctet(const Nullable<uint8_t>& aOctet) {
  mEnforcedNullableOctet = aOctet;
}

Nullable<uint8_t> TestFunctions::GetEnforcedNullableOctet() const {
  return mEnforcedNullableOctet;
}

void TestFunctions::SetArrayBufferView(const ArrayBufferView& aBuffer) {}

void TestFunctions::GetArrayBufferView(JSContext* aCx,
                                       JS::Handle<JSObject*> aObj,
                                       JS::MutableHandle<JSObject*> aRetval,
                                       ErrorResult& aError) {
  aError.Throw(NS_ERROR_NOT_IMPLEMENTED);
}

void TestFunctions::SetAllowSharedArrayBufferView(
    const ArrayBufferView& aBuffer) {}

void TestFunctions::GetAllowSharedArrayBufferView(
    JSContext* aCx, JS::Handle<JSObject*> aObj,
    JS::MutableHandle<JSObject*> aRetval, ErrorResult& aError) {
  aError.Throw(NS_ERROR_NOT_IMPLEMENTED);
}

void TestFunctions::SetSequenceOfArrayBufferView(
    const Sequence<ArrayBufferView>& aBuffers) {}

void TestFunctions::GetSequenceOfArrayBufferView(JSContext* aCx,
                                                 JS::Handle<JSObject*> aObj,
                                                 nsTArray<JSObject*>& aRetval,
                                                 ErrorResult& aError) {
  aError.Throw(NS_ERROR_NOT_IMPLEMENTED);
}

void TestFunctions::SetSequenceOfAllowSharedArrayBufferView(
    const Sequence<ArrayBufferView>& aBuffers) {}

void TestFunctions::GetSequenceOfAllowSharedArrayBufferView(
    JSContext* aCx, JS::Handle<JSObject*> aObj, nsTArray<JSObject*>& aRetval,
    ErrorResult& aError) {
  aError.Throw(NS_ERROR_NOT_IMPLEMENTED);
}

void TestFunctions::SetArrayBuffer(const ArrayBuffer& aBuffer) {}

void TestFunctions::GetArrayBuffer(JSContext* aCx, JS::Handle<JSObject*> aObj,
                                   JS::MutableHandle<JSObject*> aRetval,
                                   ErrorResult& aError) {
  aError.Throw(NS_ERROR_NOT_IMPLEMENTED);
}

void TestFunctions::SetAllowSharedArrayBuffer(const ArrayBuffer& aBuffer) {}

void TestFunctions::GetAllowSharedArrayBuffer(
    JSContext* aCx, JS::Handle<JSObject*> aObj,
    JS::MutableHandle<JSObject*> aRetval, ErrorResult& aError) {
  aError.Throw(NS_ERROR_NOT_IMPLEMENTED);
}

void TestFunctions::SetSequenceOfArrayBuffer(
    const Sequence<ArrayBuffer>& aBuffers) {}

void TestFunctions::GetSequenceOfArrayBuffer(JSContext* aCx,
                                             JS::Handle<JSObject*> aObj,
                                             nsTArray<JSObject*>& aRetval,
                                             ErrorResult& aError) {
  aError.Throw(NS_ERROR_NOT_IMPLEMENTED);
}

void TestFunctions::SetSequenceOfAllowSharedArrayBuffer(
    const Sequence<ArrayBuffer>& aBuffers) {}

void TestFunctions::GetSequenceOfAllowSharedArrayBuffer(
    JSContext* aCx, JS::Handle<JSObject*> aObj, nsTArray<JSObject*>& aRetval,
    ErrorResult& aError) {
  aError.Throw(NS_ERROR_NOT_IMPLEMENTED);
}

void TestFunctions::TestNotAllowShared(const ArrayBufferView& aBuffer) {}

void TestFunctions::TestNotAllowShared(const ArrayBuffer& aBuffer) {}

void TestFunctions::TestNotAllowShared(const nsAString& aBuffer) {}

void TestFunctions::TestAllowShared(const ArrayBufferView& aBuffer) {}

void TestFunctions::TestAllowShared(const ArrayBuffer& aBuffer) {}

void TestFunctions::TestDictWithAllowShared(
    const DictWithAllowSharedBufferSource& aDict) {}

void TestFunctions::TestUnionOfBuffferSource(
    const ArrayBufferOrArrayBufferViewOrString& aUnion) {}

void TestFunctions::TestUnionOfAllowSharedBuffferSource(
    const MaybeSharedArrayBufferOrMaybeSharedArrayBufferView& aUnion) {}

bool TestFunctions::ObjectFromAboutBlank(JSContext* aCx, JSObject* aObj) {
  // We purposefully don't use WindowOrNull here, because we want to
  // demonstrate the incorrect behavior we get, not just fail some asserts.
  RefPtr<nsGlobalWindowInner> win;
  UNWRAP_MAYBE_CROSS_ORIGIN_OBJECT(Window, aObj, win, aCx);
  if (!win) {
    return false;
  }

  Document* doc = win->GetDoc();
  if (!doc) {
    return false;
  }

  return doc->GetDocumentURI()->GetSpecOrDefault().EqualsLiteral("about:blank");
}

WrapperCachedNonISupportsTestInterface*
TestFunctions::WrapperCachedNonISupportsObject() {
  if (!mWrapperCachedNonISupportsTestInterface) {
    mWrapperCachedNonISupportsTestInterface =
        new WrapperCachedNonISupportsTestInterface();
  }
  return mWrapperCachedNonISupportsTestInterface;
}

bool TestFunctions::WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto,
                               JS::MutableHandle<JSObject*> aWrapper) {
  return TestFunctions_Binding::Wrap(aCx, this, aGivenProto, aWrapper);
}

}  // namespace dom
}  // namespace mozilla
