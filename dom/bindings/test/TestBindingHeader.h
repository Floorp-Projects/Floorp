/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef TestBindingHeader_h
#define TestBindingHeader_h

#include "nsWrapperCache.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/TypedArray.h"
#include "nsCOMPtr.h"
// We don't export TestCodeGenBinding.h, but it's right in our parent dir.
#include "../TestCodeGenBinding.h"

namespace mozilla {
namespace dom {

// IID for the TestNonCastableInterface
#define NS_TEST_NONCASTABLE_INTERFACE_IID \
{ 0x7c9f8ee2, 0xc9bf, 0x46ca, \
 { 0xa0, 0xa9, 0x03, 0xa8, 0xd6, 0x30, 0x0e, 0xde } }

class TestNonCastableInterface : public nsISupports,
                                 public nsWrapperCache
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_TEST_NONCASTABLE_INTERFACE_IID)
  NS_DECL_ISUPPORTS

  // We need a GetParentObject to make binding codegen happy
  virtual nsISupports* GetParentObject();
};

// IID for the TestExternalInterface
#define NS_TEST_EXTERNAL_INTERFACE_IID \
{ 0xd5ba0c99, 0x9b1d, 0x4e71, \
 { 0x8a, 0x94, 0x56, 0x38, 0x6c, 0xa3, 0xda, 0x3d } }
class TestExternalInterface : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_TEST_EXTERNAL_INTERFACE_IID)
  NS_DECL_ISUPPORTS
};

class TestInterface : public nsISupports,
                      public nsWrapperCache
{
public:
  NS_DECL_ISUPPORTS

  // We need a GetParentObject to make binding codegen happy
  virtual nsISupports* GetParentObject();

  // And now our actual WebIDL API
  // Constructors
  static
  already_AddRefed<TestInterface> Constructor(nsISupports*, ErrorResult&);
  static
  already_AddRefed<TestInterface> Constructor(nsISupports*, const nsAString&,
                                              ErrorResult&);
  static
  already_AddRefed<TestInterface> Constructor(nsISupports*, uint32_t,
                                              Nullable<bool>&, ErrorResult&);
  static
  already_AddRefed<TestInterface> Constructor(nsISupports*, TestInterface*,
                                              ErrorResult&);
  static
  already_AddRefed<TestInterface> Constructor(nsISupports*,
                                              NonNull<TestNonCastableInterface>&,
                                              ErrorResult&);

  // Integer types
  int8_t GetReadonlyByte(ErrorResult&);
  int8_t GetWritableByte(ErrorResult&);
  void SetWritableByte(int8_t, ErrorResult&);
  void PassByte(int8_t, ErrorResult&);
  int8_t ReceiveByte(ErrorResult&);
  void PassOptionalByte(const Optional<int8_t>&, ErrorResult&);
  void PassOptionalByteWithDefault(int8_t, ErrorResult&);
  void PassNullableByte(Nullable<int8_t>&, ErrorResult&);
  void PassOptionalNullableByte(const Optional< Nullable<int8_t> >&,
                                ErrorResult&);

  int16_t GetReadonlyShort(ErrorResult&);
  int16_t GetWritableShort(ErrorResult&);
  void SetWritableShort(int16_t, ErrorResult&);
  void PassShort(int16_t, ErrorResult&);
  int16_t ReceiveShort(ErrorResult&);
  void PassOptionalShort(const Optional<int16_t>&, ErrorResult&);
  void PassOptionalShortWithDefault(int16_t, ErrorResult&);

  int32_t GetReadonlyLong(ErrorResult&);
  int32_t GetWritableLong(ErrorResult&);
  void SetWritableLong(int32_t, ErrorResult&);
  void PassLong(int32_t, ErrorResult&);
  int16_t ReceiveLong(ErrorResult&);
  void PassOptionalLong(const Optional<int32_t>&, ErrorResult&);
  void PassOptionalLongWithDefault(int32_t, ErrorResult&);

  int64_t GetReadonlyLongLong(ErrorResult&);
  int64_t GetWritableLongLong(ErrorResult&);
  void SetWritableLongLong(int64_t, ErrorResult&);
  void PassLongLong(int64_t, ErrorResult&);
  int64_t ReceiveLongLong(ErrorResult&);
  void PassOptionalLongLong(const Optional<int64_t>&, ErrorResult&);
  void PassOptionalLongLongWithDefault(int64_t, ErrorResult&);

  uint8_t GetReadonlyOctet(ErrorResult&);
  uint8_t GetWritableOctet(ErrorResult&);
  void SetWritableOctet(uint8_t, ErrorResult&);
  void PassOctet(uint8_t, ErrorResult&);
  uint8_t ReceiveOctet(ErrorResult&);
  void PassOptionalOctet(const Optional<uint8_t>&, ErrorResult&);
  void PassOptionalOctetWithDefault(uint8_t, ErrorResult&);

  uint16_t GetReadonlyUnsignedShort(ErrorResult&);
  uint16_t GetWritableUnsignedShort(ErrorResult&);
  void SetWritableUnsignedShort(uint16_t, ErrorResult&);
  void PassUnsignedShort(uint16_t, ErrorResult&);
  uint16_t ReceiveUnsignedShort(ErrorResult&);
  void PassOptionalUnsignedShort(const Optional<uint16_t>&, ErrorResult&);
  void PassOptionalUnsignedShortWithDefault(uint16_t, ErrorResult&);

  uint32_t GetReadonlyUnsignedLong(ErrorResult&);
  uint32_t GetWritableUnsignedLong(ErrorResult&);
  void SetWritableUnsignedLong(uint32_t, ErrorResult&);
  void PassUnsignedLong(uint32_t, ErrorResult&);
  uint32_t ReceiveUnsignedLong(ErrorResult&);
  void PassOptionalUnsignedLong(const Optional<uint32_t>&, ErrorResult&);
  void PassOptionalUnsignedLongWithDefault(uint32_t, ErrorResult&);

  uint64_t GetReadonlyUnsignedLongLong(ErrorResult&);
  uint64_t GetWritableUnsignedLongLong(ErrorResult&);
  void SetWritableUnsignedLongLong(uint64_t, ErrorResult&);
  void PassUnsignedLongLong(uint64_t, ErrorResult&);
  uint64_t ReceiveUnsignedLongLong(ErrorResult&);
  void PassOptionalUnsignedLongLong(const Optional<uint64_t>&, ErrorResult&);
  void PassOptionalUnsignedLongLongWithDefault(uint64_t, ErrorResult&);

  // Interface types
  already_AddRefed<TestInterface> ReceiveSelf(ErrorResult&);
  already_AddRefed<TestInterface> ReceiveNullableSelf(ErrorResult&);
  TestInterface* ReceiveWeakSelf(ErrorResult&);
  TestInterface* ReceiveWeakNullableSelf(ErrorResult&);
  void PassSelf(TestInterface&, ErrorResult&);
  void PassSelf2(NonNull<TestInterface>&, ErrorResult&);
  void PassNullableSelf(TestInterface*, ErrorResult&);
  already_AddRefed<TestInterface> GetNonNullSelf(ErrorResult&);
  void SetNonNullSelf(TestInterface&, ErrorResult&);
  already_AddRefed<TestInterface> GetNullableSelf(ErrorResult&);
  void SetNullableSelf(TestInterface*, ErrorResult&);
  void PassOptionalSelf(const Optional<TestInterface*> &, ErrorResult&);
  void PassOptionalNonNullSelf(const Optional<NonNull<TestInterface> >&, ErrorResult&);
  void PassOptionalSelfWithDefault(TestInterface*, ErrorResult&);

  already_AddRefed<TestNonCastableInterface> ReceiveOther(ErrorResult&);
  already_AddRefed<TestNonCastableInterface> ReceiveNullableOther(ErrorResult&);
  TestNonCastableInterface* ReceiveWeakOther(ErrorResult&);
  TestNonCastableInterface* ReceiveWeakNullableOther(ErrorResult&);
  void PassOther(TestNonCastableInterface&, ErrorResult&);
  void PassOther2(NonNull<TestNonCastableInterface>&, ErrorResult&);
  void PassNullableOther(TestNonCastableInterface*, ErrorResult&);
  already_AddRefed<TestNonCastableInterface> GetNonNullOther(ErrorResult&);
  void SetNonNullOther(TestNonCastableInterface&, ErrorResult&);
  already_AddRefed<TestNonCastableInterface> GetNullableOther(ErrorResult&);
  void SetNullableOther(TestNonCastableInterface*, ErrorResult&);
  void PassOptionalOther(const Optional<TestNonCastableInterface*>&, ErrorResult&);
  void PassOptionalNonNullOther(const Optional<NonNull<TestNonCastableInterface> >&, ErrorResult&);
  void PassOptionalOtherWithDefault(TestNonCastableInterface*, ErrorResult&);

  already_AddRefed<TestExternalInterface> ReceiveExternal(ErrorResult&);
  already_AddRefed<TestExternalInterface> ReceiveNullableExternal(ErrorResult&);
  TestExternalInterface* ReceiveWeakExternal(ErrorResult&);
  TestExternalInterface* ReceiveWeakNullableExternal(ErrorResult&);
  void PassExternal(TestExternalInterface*, ErrorResult&);
  void PassExternal2(TestExternalInterface*, ErrorResult&);
  void PassNullableExternal(TestExternalInterface*, ErrorResult&);
  already_AddRefed<TestExternalInterface> GetNonNullExternal(ErrorResult&);
  void SetNonNullExternal(TestExternalInterface*, ErrorResult&);
  already_AddRefed<TestExternalInterface> GetNullableExternal(ErrorResult&);
  void SetNullableExternal(TestExternalInterface*, ErrorResult&);
  void PassOptionalExternal(const Optional<TestExternalInterface*>&, ErrorResult&);
  void PassOptionalNonNullExternal(const Optional<TestExternalInterface*>&, ErrorResult&);
  void PassOptionalExternalWithDefault(TestExternalInterface*, ErrorResult&);

  // Sequence types
  void ReceiveSequence(nsTArray<int32_t>&, ErrorResult&);
  void ReceiveNullableSequence(Nullable< nsTArray<int32_t> >&, ErrorResult&);
  void ReceiveSequenceOfNullableInts(nsTArray< Nullable<int32_t> >&, ErrorResult&);
  void ReceiveNullableSequenceOfNullableInts(Nullable< nsTArray< Nullable<int32_t> > >&, ErrorResult&);
  void PassSequence(const Sequence<int32_t> &, ErrorResult&);
  void PassNullableSequence(const Nullable< Sequence<int32_t> >&, ErrorResult&);
  void PassSequenceOfNullableInts(const Sequence<Nullable<int32_t> >&, ErrorResult&);
  void PassOptionalSequenceOfNullableInts(const Optional<Sequence<Nullable<int32_t> > > &,
                                          ErrorResult&);
  void PassOptionalNullableSequenceOfNullableInts(const Optional<Nullable<Sequence<Nullable<int32_t> > > > &,
                                                  ErrorResult&);
  void ReceiveCastableObjectSequence(nsTArray< nsRefPtr<TestInterface> > &,
                                     ErrorResult&);
  void ReceiveNullableCastableObjectSequence(nsTArray< nsRefPtr<TestInterface> > &,
                                             ErrorResult&);
  void ReceiveCastableObjectNullableSequence(Nullable< nsTArray< nsRefPtr<TestInterface> > >&,
                                             ErrorResult&);
  void ReceiveWeakNullableCastableObjectNullableSequence(Nullable< nsTArray< nsRefPtr<TestInterface> > >&,
                                                         ErrorResult&);
  void ReceiveNullableCastableObjectNullableSequence(Nullable< nsTArray< nsRefPtr<TestInterface> > >&,
                                             ErrorResult&);
  void ReceiveWeakCastableObjectSequence(nsTArray<TestInterface*> &,
                                         ErrorResult&);
  void ReceiveWeakNullableCastableObjectSequence(nsTArray<TestInterface*> &,
                                                 ErrorResult&);
  void ReceiveWeakCastableObjectNullableSequence(Nullable< nsTArray<TestInterface*> >&,
                                                 ErrorResult&);
  void ReceiveWeakNullableCastableObjectNullableSequence(Nullable< nsTArray<TestInterface*> >&,
                                                         ErrorResult&);
  void PassCastableObjectSequence(const Sequence< OwningNonNull<TestInterface> >&,
                                  ErrorResult&);
  void PassNullableCastableObjectSequence(const Sequence< nsRefPtr<TestInterface> > &,
                                          ErrorResult&);
  void PassCastableObjectNullableSequence(const Nullable< Sequence< OwningNonNull<TestInterface> > >&,
                                          ErrorResult&);
  void PassNullableCastableObjectNullableSequence(const Nullable< Sequence< nsRefPtr<TestInterface> > >&,
                                                  ErrorResult&);
  void PassOptionalSequence(const Optional<Sequence<int32_t> >&,
                            ErrorResult&);
  void PassOptionalNullableSequence(const Optional<Nullable<Sequence<int32_t> > >&,
                                    ErrorResult&);
  void PassOptionalNullableSequenceWithDefaultValue(const Nullable< Sequence<int32_t> >&,
                                                    ErrorResult&);
  void PassOptionalObjectSequence(const Optional<Sequence<OwningNonNull<TestInterface> > >&,
                                  ErrorResult&);

  // Typed array types
  void PassArrayBuffer(ArrayBuffer&, ErrorResult&);
  void PassNullableArrayBuffer(ArrayBuffer*, ErrorResult&);
  void PassOptionalArrayBuffer(const Optional<ArrayBuffer>&, ErrorResult&);
  void PassOptionalNullableArrayBuffer(const Optional<ArrayBuffer*>&, ErrorResult&);
  void PassOptionalNullableArrayBufferWithDefaultValue(ArrayBuffer*, ErrorResult&);
  void PassArrayBufferView(ArrayBufferView&, ErrorResult&);
  void PassInt8Array(Int8Array&, ErrorResult&);
  void PassInt16Array(Int16Array&, ErrorResult&);
  void PassInt32Array(Int32Array&, ErrorResult&);
  void PassUint8Array(Uint8Array&, ErrorResult&);
  void PassUint16Array(Uint16Array&, ErrorResult&);
  void PassUint32Array(Uint32Array&, ErrorResult&);
  void PassUint8ClampedArray(Uint8ClampedArray&, ErrorResult&);
  void PassFloat32Array(Float32Array&, ErrorResult&);
  void PassFloat64Array(Float64Array&, ErrorResult&);

  // String types
  void PassString(const nsAString&, ErrorResult&);
  void PassNullableString(const nsAString&, ErrorResult&);
  void PassOptionalString(const Optional<nsAString>&, ErrorResult&);
  void PassOptionalNullableString(const Optional<nsAString>&, ErrorResult&);
  void PassOptionalNullableStringWithDefaultValue(const nsAString&, ErrorResult&);

  // Enumarated types
  void PassEnum(TestEnum, ErrorResult&);
  void PassOptionalEnum(const Optional<TestEnum>&, ErrorResult&);
  TestEnum ReceiveEnum(ErrorResult&);

  // Callback types
  void PassCallback(JSContext*, JSObject*, ErrorResult&);
  void PassNullableCallback(JSContext*, JSObject*, ErrorResult&);
  void PassOptionalCallback(JSContext*, const Optional<JSObject*>&,
                            ErrorResult&);
  void PassOptionalNullableCallback(JSContext*, const Optional<JSObject*>&,
                                    ErrorResult&);
  void PassOptionalNullableCallbackWithDefaultValue(JSContext*, JSObject*,
                                                    ErrorResult&);
  JSObject* ReceiveCallback(JSContext*, ErrorResult&);
  JSObject* ReceiveNullableCallback(JSContext*, ErrorResult&);

  // Any types
  void PassAny(JSContext*, JS::Value, ErrorResult&);
  void PassOptionalAny(JSContext*, const Optional<JS::Value>&, ErrorResult&);
  JS::Value ReceiveAny(JSContext*, ErrorResult&);

  void PassObject(JSContext*, JSObject&, ErrorResult&);
  void PassNullableObject(JSContext*, JSObject*, ErrorResult&);
  void PassOptionalObject(JSContext*, const Optional<NonNull<JSObject> >&, ErrorResult&);
  void PassOptionalNullableObject(JSContext*, const Optional<JSObject*>&, ErrorResult&);
  void PassOptionalNullableObjectWithDefaultValue(JSContext*, JSObject*, ErrorResult&);
  JSObject* ReceiveObject(JSContext*, ErrorResult&);
  JSObject* ReceiveNullableObject(JSContext*, ErrorResult&);

  // binaryNames tests
  void MethodRenamedTo(ErrorResult&);
  void MethodRenamedTo(int8_t, ErrorResult&);
  int8_t GetAttributeGetterRenamedTo(ErrorResult&);
  int8_t GetAttributeRenamedTo(ErrorResult&);
  void SetAttributeRenamedTo(int8_t, ErrorResult&);

private:
  // We add signatures here that _could_ start matching if the codegen
  // got data types wrong.  That way if it ever does we'll have a call
  // to these private deleted methods and compilation will fail.
  void SetReadonlyByte(int8_t, ErrorResult&) MOZ_DELETE;
  template<typename T>
  void SetWritableByte(T, ErrorResult&) MOZ_DELETE;
  template<typename T>
  void PassByte(T, ErrorResult&) MOZ_DELETE;
  template<typename T>
  void PassOptionalByte(const Optional<T>&, ErrorResult&) MOZ_DELETE;
  template<typename T>
  void PassOptionalByteWithDefault(T, ErrorResult&) MOZ_DELETE;

  void SetReadonlyShort(int16_t, ErrorResult&) MOZ_DELETE;
  template<typename T>
  void SetWritableShort(T, ErrorResult&) MOZ_DELETE;
  template<typename T>
  void PassShort(T, ErrorResult&) MOZ_DELETE;
  template<typename T>
  void PassOptionalShort(const Optional<T>&, ErrorResult&) MOZ_DELETE;
  template<typename T>
  void PassOptionalShortWithDefault(T, ErrorResult&) MOZ_DELETE;

  void SetReadonlyLong(int32_t, ErrorResult&) MOZ_DELETE;
  template<typename T>
  void SetWritableLong(T, ErrorResult&) MOZ_DELETE;
  template<typename T>
  void PassLong(T, ErrorResult&) MOZ_DELETE;
  template<typename T>
  void PassOptionalLong(const Optional<T>&, ErrorResult&) MOZ_DELETE;
  template<typename T>
  void PassOptionalLongWithDefault(T, ErrorResult&) MOZ_DELETE;

  void SetReadonlyLongLong(int64_t, ErrorResult&) MOZ_DELETE;
  template<typename T>
  void SetWritableLongLong(T, ErrorResult&) MOZ_DELETE;
  template<typename T>
  void PassLongLong(T, ErrorResult&) MOZ_DELETE;
  template<typename T>
  void PassOptionalLongLong(const Optional<T>&, ErrorResult&) MOZ_DELETE;
  template<typename T>
  void PassOptionalLongLongWithDefault(T, ErrorResult&) MOZ_DELETE;

  void SetReadonlyOctet(uint8_t, ErrorResult&) MOZ_DELETE;
  template<typename T>
  void SetWritableOctet(T, ErrorResult&) MOZ_DELETE;
  template<typename T>
  void PassOctet(T, ErrorResult&) MOZ_DELETE;
  template<typename T>
  void PassOptionalOctet(const Optional<T>&, ErrorResult&) MOZ_DELETE;
  template<typename T>
  void PassOptionalOctetWithDefault(T, ErrorResult&) MOZ_DELETE;

  void SetReadonlyUnsignedShort(uint16_t, ErrorResult&) MOZ_DELETE;
  template<typename T>
  void SetWritableUnsignedShort(T, ErrorResult&) MOZ_DELETE;
  template<typename T>
  void PassUnsignedShort(T, ErrorResult&) MOZ_DELETE;
  template<typename T>
  void PassOptionalUnsignedShort(const Optional<T>&, ErrorResult&) MOZ_DELETE;
  template<typename T>
  void PassOptionalUnsignedShortWithDefault(T, ErrorResult&) MOZ_DELETE;

  void SetReadonlyUnsignedLong(uint32_t, ErrorResult&) MOZ_DELETE;
  template<typename T>
  void SetWritableUnsignedLong(T, ErrorResult&) MOZ_DELETE;
  template<typename T>
  void PassUnsignedLong(T, ErrorResult&) MOZ_DELETE;
  template<typename T>
  void PassOptionalUnsignedLong(const Optional<T>&, ErrorResult&) MOZ_DELETE;
  template<typename T>
  void PassOptionalUnsignedLongWithDefault(T, ErrorResult&) MOZ_DELETE;

  void SetReadonlyUnsignedLongLong(uint64_t, ErrorResult&) MOZ_DELETE;
  template<typename T>
  void SetWritableUnsignedLongLong(T, ErrorResult&) MOZ_DELETE;
  template<typename T>
  void PassUnsignedLongLong(T, ErrorResult&) MOZ_DELETE;
  template<typename T>
  void PassOptionalUnsignedLongLong(const Optional<T>&, ErrorResult&) MOZ_DELETE;
  template<typename T>
  void PassOptionalUnsignedLongLongWithDefault(T, ErrorResult&) MOZ_DELETE;

  // Enforce that only const things are passed for sequences
  void PassSequence(Sequence<int32_t> &, ErrorResult&) MOZ_DELETE;
  void PassNullableSequence(Nullable< Sequence<int32_t> >&, ErrorResult&) MOZ_DELETE;
  void PassOptionalNullableSequenceWithDefaultValue(Nullable< Sequence<int32_t> >&,
                                                    ErrorResult&) MOZ_DELETE;

  // Enforce that only const things are passed for optional
  void PassOptionalByte(Optional<int8_t>&, ErrorResult&) MOZ_DELETE;
  void PassOptionalNullableByte(Optional<Nullable<int8_t> >&,
                                ErrorResult&) MOZ_DELETE;
  void PassOptionalShort(Optional<int16_t>&, ErrorResult&) MOZ_DELETE;
  void PassOptionalLong(Optional<int32_t>&, ErrorResult&) MOZ_DELETE;
  void PassOptionalLongLong(Optional<int64_t>&, ErrorResult&) MOZ_DELETE;
  void PassOptionalOctet(Optional<uint8_t>&, ErrorResult&) MOZ_DELETE;
  void PassOptionalUnsignedShort(Optional<uint16_t>&, ErrorResult&) MOZ_DELETE;
  void PassOptionalUnsignedLong(Optional<uint32_t>&, ErrorResult&) MOZ_DELETE;
  void PassOptionalUnsignedLongLong(Optional<uint64_t>&, ErrorResult&) MOZ_DELETE;
  void PassOptionalSelf(Optional<TestInterface*> &, ErrorResult&) MOZ_DELETE;
  void PassOptionalNonNullSelf(Optional<NonNull<TestInterface> >&, ErrorResult&) MOZ_DELETE;
  void PassOptionalOther(Optional<TestNonCastableInterface*>&, ErrorResult&);
  void PassOptionalNonNullOther(Optional<NonNull<TestNonCastableInterface> >&, ErrorResult&);
  void PassOptionalExternal(Optional<TestExternalInterface*>&, ErrorResult&) MOZ_DELETE;
  void PassOptionalNonNullExternal(Optional<TestExternalInterface*>&, ErrorResult&) MOZ_DELETE;
  void PassOptionalSequence(Optional<Sequence<int32_t> >&, ErrorResult&) MOZ_DELETE;
  void PassOptionalNullableSequence(Optional<Nullable<Sequence<int32_t> > >&,
                                    ErrorResult&) MOZ_DELETE;
  void PassOptionalObjectSequence(Optional<Sequence<OwningNonNull<TestInterface> > >&,
                                  ErrorResult&) MOZ_DELETE;
  void PassOptionalArrayBuffer(Optional<ArrayBuffer>&, ErrorResult&) MOZ_DELETE;
  void PassOptionalNullableArrayBuffer(Optional<ArrayBuffer*>&, ErrorResult&) MOZ_DELETE;
  void PassOptionalEnum(Optional<TestEnum>&, ErrorResult&) MOZ_DELETE;
  void PassOptionalCallback(JSContext*, Optional<JSObject*>&, ErrorResult&) MOZ_DELETE;
  void PassOptionalNullableCallback(JSContext*, Optional<JSObject*>&, ErrorResult&) MOZ_DELETE;
  void PassOptionalAny(Optional<JS::Value>&, ErrorResult) MOZ_DELETE;

  // And test that string stuff is always const
  void PassString(nsAString&, ErrorResult&) MOZ_DELETE;
  void PassNullableString(nsAString&, ErrorResult&) MOZ_DELETE;
  void PassOptionalString(Optional<nsAString>&, ErrorResult&) MOZ_DELETE;
  void PassOptionalNullableString(Optional<nsAString>&, ErrorResult&) MOZ_DELETE;
  void PassOptionalNullableStringWithDefaultValue(nsAString&, ErrorResult&) MOZ_DELETE;

};

} // namespace dom
} // namespace mozilla

#endif /* TestBindingHeader_h */
