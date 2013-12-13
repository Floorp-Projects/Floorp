/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef TestBindingHeader_h
#define TestBindingHeader_h

#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/Date.h"
#include "mozilla/dom/TypedArray.h"
#include "mozilla/dom/UnionTypes.h"
#include "mozilla/ErrorResult.h"
#include "nsCOMPtr.h"
#include "nsGenericHTMLElement.h"
#include "nsWrapperCache.h"

// Forward declare this before we include TestCodeGenBinding.h, because that header relies on including
// this one for it, for ParentDict. Hopefully it won't begin to rely on it in more fundamental ways.
namespace mozilla {
namespace dom {
class TestExternalInterface;
} // namespace dom
} // namespace mozilla

// We don't export TestCodeGenBinding.h, but it's right in our parent dir.
#include "../TestCodeGenBinding.h"

extern bool TestFuncControlledMember(JSContext*, JSObject*);

namespace mozilla {
namespace dom {

// IID for nsRenamedInterface
#define NS_RENAMED_INTERFACE_IID \
{ 0xd4b19ef3, 0xe68b, 0x4e3f, \
 { 0x94, 0xbc, 0xc9, 0xde, 0x3a, 0x69, 0xb0, 0xe8 } }

class nsRenamedInterface : public nsISupports,
                           public nsWrapperCache
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_RENAMED_INTERFACE_IID)
  NS_DECL_ISUPPORTS

  // We need a GetParentObject to make binding codegen happy
  virtual nsISupports* GetParentObject();
};

// IID for the IndirectlyImplementedInterface
#define NS_INDIRECTLY_IMPLEMENTED_INTERFACE_IID \
{ 0xfed55b69, 0x7012, 0x4849, \
 { 0xaf, 0x56, 0x4b, 0xa9, 0xee, 0x41, 0x30, 0x89 } }

class IndirectlyImplementedInterface : public nsISupports,
                                       public nsWrapperCache
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_INDIRECTLY_IMPLEMENTED_INTERFACE_IID)
  NS_DECL_ISUPPORTS

  // We need a GetParentObject to make binding codegen happy
  virtual nsISupports* GetParentObject();

  bool IndirectlyImplementedProperty();
  void IndirectlyImplementedProperty(bool);
  void IndirectlyImplementedMethod();
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

class TestNonWrapperCacheInterface : public nsISupports
{
public:
  NS_DECL_ISUPPORTS

  virtual JSObject* WrapObject(JSContext* cx, JS::Handle<JSObject*> scope);
};

class OnlyForUseInConstructor : public nsISupports,
                                public nsWrapperCache
{
public:
  NS_DECL_ISUPPORTS
  // We need a GetParentObject to make binding codegen happy
  virtual nsISupports* GetParentObject();
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
  already_AddRefed<TestInterface>
    Constructor(const GlobalObject&, ErrorResult&);
  static
  already_AddRefed<TestInterface>
    Constructor(const GlobalObject&, const nsAString&, ErrorResult&);
  static
  already_AddRefed<TestInterface>
    Constructor(const GlobalObject&, uint32_t, const Nullable<bool>&,
                ErrorResult&);
  static
  already_AddRefed<TestInterface>
    Constructor(const GlobalObject&, TestInterface*, ErrorResult&);
  static
  already_AddRefed<TestInterface>
    Constructor(const GlobalObject&, uint32_t, IndirectlyImplementedInterface&, ErrorResult&);

  static
  already_AddRefed<TestInterface>
    Constructor(const GlobalObject&, Date&, ErrorResult&);
  /*  static
  already_AddRefed<TestInterface>
    Constructor(const GlobalObject&, uint32_t, uint32_t,
                const TestInterfaceOrOnlyForUseInConstructor&, ErrorResult&);
  */

  static
  already_AddRefed<TestInterface> Test(const GlobalObject&, ErrorResult&);
  static
  already_AddRefed<TestInterface> Test(const GlobalObject&, const nsAString&,
                                       ErrorResult&);
  static
  already_AddRefed<TestInterface> Test(const GlobalObject&, const nsACString&,
                                       ErrorResult&);

  static
  already_AddRefed<TestInterface> Test2(const GlobalObject&,
                                        JSContext*,
                                        const DictForConstructor&,
                                        JS::Value,
                                        JS::Handle<JSObject*>,
                                        JS::Handle<JSObject*>,
                                        const Sequence<Dict>&,
                                        const Optional<JS::Handle<JS::Value> >&,
                                        const Optional<JS::Handle<JSObject*> >&,
                                        const Optional<JS::Handle<JSObject*> >&,
                                        ErrorResult&);

  // Integer types
  int8_t ReadonlyByte();
  int8_t WritableByte();
  void SetWritableByte(int8_t);
  void PassByte(int8_t);
  int8_t ReceiveByte();
  void PassOptionalByte(const Optional<int8_t>&);
  void PassOptionalByteBeforeRequired(const Optional<int8_t>&, int8_t);
  void PassOptionalByteWithDefault(int8_t);
  void PassOptionalByteWithDefaultBeforeRequired(int8_t, int8_t);
  void PassNullableByte(const Nullable<int8_t>&);
  void PassOptionalNullableByte(const Optional< Nullable<int8_t> >&);
  void PassVariadicByte(const Sequence<int8_t>&);
  int8_t CachedByte();

  int16_t ReadonlyShort();
  int16_t WritableShort();
  void SetWritableShort(int16_t);
  void PassShort(int16_t);
  int16_t ReceiveShort();
  void PassOptionalShort(const Optional<int16_t>&);
  void PassOptionalShortWithDefault(int16_t);

  int32_t ReadonlyLong();
  int32_t WritableLong();
  void SetWritableLong(int32_t);
  void PassLong(int32_t);
  int16_t ReceiveLong();
  void PassOptionalLong(const Optional<int32_t>&);
  void PassOptionalLongWithDefault(int32_t);

  int64_t ReadonlyLongLong();
  int64_t WritableLongLong();
  void SetWritableLongLong(int64_t);
  void PassLongLong(int64_t);
  int64_t ReceiveLongLong();
  void PassOptionalLongLong(const Optional<int64_t>&);
  void PassOptionalLongLongWithDefault(int64_t);

  uint8_t ReadonlyOctet();
  uint8_t WritableOctet();
  void SetWritableOctet(uint8_t);
  void PassOctet(uint8_t);
  uint8_t ReceiveOctet();
  void PassOptionalOctet(const Optional<uint8_t>&);
  void PassOptionalOctetWithDefault(uint8_t);

  uint16_t ReadonlyUnsignedShort();
  uint16_t WritableUnsignedShort();
  void SetWritableUnsignedShort(uint16_t);
  void PassUnsignedShort(uint16_t);
  uint16_t ReceiveUnsignedShort();
  void PassOptionalUnsignedShort(const Optional<uint16_t>&);
  void PassOptionalUnsignedShortWithDefault(uint16_t);

  uint32_t ReadonlyUnsignedLong();
  uint32_t WritableUnsignedLong();
  void SetWritableUnsignedLong(uint32_t);
  void PassUnsignedLong(uint32_t);
  uint32_t ReceiveUnsignedLong();
  void PassOptionalUnsignedLong(const Optional<uint32_t>&);
  void PassOptionalUnsignedLongWithDefault(uint32_t);

  uint64_t ReadonlyUnsignedLongLong();
  uint64_t WritableUnsignedLongLong();
  void SetWritableUnsignedLongLong(uint64_t);
  void PassUnsignedLongLong(uint64_t);
  uint64_t ReceiveUnsignedLongLong();
  void PassOptionalUnsignedLongLong(const Optional<uint64_t>&);
  void PassOptionalUnsignedLongLongWithDefault(uint64_t);

  float WritableFloat() const;
  void SetWritableFloat(float);
  float WritableUnrestrictedFloat() const;
  void SetWritableUnrestrictedFloat(float);
  Nullable<float> GetWritableNullableFloat() const;
  void SetWritableNullableFloat(Nullable<float>);
  Nullable<float> GetWritableNullableUnrestrictedFloat() const;
  void SetWritableNullableUnrestrictedFloat(Nullable<float>);
  double WritableDouble() const;
  void SetWritableDouble(double);
  double WritableUnrestrictedDouble() const;
  void SetWritableUnrestrictedDouble(double);
  Nullable<double> GetWritableNullableDouble() const;
  void SetWritableNullableDouble(Nullable<double>);
  Nullable<double> GetWritableNullableUnrestrictedDouble() const;
  void SetWritableNullableUnrestrictedDouble(Nullable<double>);
  void PassFloat(float, float, Nullable<float>, Nullable<float>,
                 double, double, Nullable<double>, Nullable<double>,
                 const Sequence<float>&, const Sequence<float>&,
                 const Sequence<Nullable<float> >&,
                 const Sequence<Nullable<float> >&,
                 const Sequence<double>&, const Sequence<double>&,
                 const Sequence<Nullable<double> >&,
                 const Sequence<Nullable<double> >&);
  void PassLenientFloat(float, float, Nullable<float>, Nullable<float>,
                        double, double, Nullable<double>, Nullable<double>,
                        const Sequence<float>&, const Sequence<float>&,
                        const Sequence<Nullable<float> >&,
                        const Sequence<Nullable<float> >&,
                        const Sequence<double>&, const Sequence<double>&,
                        const Sequence<Nullable<double> >&,
                        const Sequence<Nullable<double> >&);
  float LenientFloatAttr() const;
  void SetLenientFloatAttr(float);
  double LenientDoubleAttr() const;
  void SetLenientDoubleAttr(double);

  void PassUnrestricted(float arg1,
                        float arg2,
                        float arg3,
                        float arg4,
                        double arg5,
                        double arg6,
                        double arg7,
                        double arg8);

  // Interface types
  already_AddRefed<TestInterface> ReceiveSelf();
  already_AddRefed<TestInterface> ReceiveNullableSelf();
  TestInterface* ReceiveWeakSelf();
  TestInterface* ReceiveWeakNullableSelf();
  void PassSelf(TestInterface&);
  void PassNullableSelf(TestInterface*);
  already_AddRefed<TestInterface> NonNullSelf();
  void SetNonNullSelf(TestInterface&);
  already_AddRefed<TestInterface> GetNullableSelf();
  void SetNullableSelf(TestInterface*);
  void PassOptionalSelf(const Optional<TestInterface*> &);
  void PassOptionalNonNullSelf(const Optional<NonNull<TestInterface> >&);
  void PassOptionalSelfWithDefault(TestInterface*);

  already_AddRefed<TestNonWrapperCacheInterface> ReceiveNonWrapperCacheInterface();
  already_AddRefed<TestNonWrapperCacheInterface> ReceiveNullableNonWrapperCacheInterface();
  void ReceiveNonWrapperCacheInterfaceSequence(nsTArray<nsRefPtr<TestNonWrapperCacheInterface> >&);
  void ReceiveNullableNonWrapperCacheInterfaceSequence(nsTArray<nsRefPtr<TestNonWrapperCacheInterface> >&);
  void ReceiveNonWrapperCacheInterfaceNullableSequence(Nullable<nsTArray<nsRefPtr<TestNonWrapperCacheInterface> > >&);
  void ReceiveNullableNonWrapperCacheInterfaceNullableSequence(Nullable<nsTArray<nsRefPtr<TestNonWrapperCacheInterface> > >&);

  already_AddRefed<IndirectlyImplementedInterface> ReceiveOther();
  already_AddRefed<IndirectlyImplementedInterface> ReceiveNullableOther();
  IndirectlyImplementedInterface* ReceiveWeakOther();
  IndirectlyImplementedInterface* ReceiveWeakNullableOther();
  void PassOther(IndirectlyImplementedInterface&);
  void PassNullableOther(IndirectlyImplementedInterface*);
  already_AddRefed<IndirectlyImplementedInterface> NonNullOther();
  void SetNonNullOther(IndirectlyImplementedInterface&);
  already_AddRefed<IndirectlyImplementedInterface> GetNullableOther();
  void SetNullableOther(IndirectlyImplementedInterface*);
  void PassOptionalOther(const Optional<IndirectlyImplementedInterface*>&);
  void PassOptionalNonNullOther(const Optional<NonNull<IndirectlyImplementedInterface> >&);
  void PassOptionalOtherWithDefault(IndirectlyImplementedInterface*);

  already_AddRefed<TestExternalInterface> ReceiveExternal();
  already_AddRefed<TestExternalInterface> ReceiveNullableExternal();
  TestExternalInterface* ReceiveWeakExternal();
  TestExternalInterface* ReceiveWeakNullableExternal();
  void PassExternal(TestExternalInterface*);
  void PassNullableExternal(TestExternalInterface*);
  already_AddRefed<TestExternalInterface> NonNullExternal();
  void SetNonNullExternal(TestExternalInterface*);
  already_AddRefed<TestExternalInterface> GetNullableExternal();
  void SetNullableExternal(TestExternalInterface*);
  void PassOptionalExternal(const Optional<TestExternalInterface*>&);
  void PassOptionalNonNullExternal(const Optional<TestExternalInterface*>&);
  void PassOptionalExternalWithDefault(TestExternalInterface*);

  already_AddRefed<TestCallbackInterface> ReceiveCallbackInterface();
  already_AddRefed<TestCallbackInterface> ReceiveNullableCallbackInterface();
  TestCallbackInterface* ReceiveWeakCallbackInterface();
  TestCallbackInterface* ReceiveWeakNullableCallbackInterface();
  void PassCallbackInterface(TestCallbackInterface&);
  void PassNullableCallbackInterface(TestCallbackInterface*);
  already_AddRefed<TestCallbackInterface> NonNullCallbackInterface();
  void SetNonNullCallbackInterface(TestCallbackInterface&);
  already_AddRefed<TestCallbackInterface> GetNullableCallbackInterface();
  void SetNullableCallbackInterface(TestCallbackInterface*);
  void PassOptionalCallbackInterface(const Optional<nsRefPtr<TestCallbackInterface> >&);
  void PassOptionalNonNullCallbackInterface(const Optional<OwningNonNull<TestCallbackInterface> >&);
  void PassOptionalCallbackInterfaceWithDefault(TestCallbackInterface*);

  already_AddRefed<IndirectlyImplementedInterface> ReceiveConsequentialInterface();
  void PassConsequentialInterface(IndirectlyImplementedInterface&);

  // Sequence types
  void GetReadonlySequence(nsTArray<int32_t>&);
  void GetReadonlySequenceOfDictionaries(JSContext*, nsTArray<Dict>&);
  void ReceiveSequence(nsTArray<int32_t>&);
  void ReceiveNullableSequence(Nullable< nsTArray<int32_t> >&);
  void ReceiveSequenceOfNullableInts(nsTArray< Nullable<int32_t> >&);
  void ReceiveNullableSequenceOfNullableInts(Nullable< nsTArray< Nullable<int32_t> > >&);
  void PassSequence(const Sequence<int32_t> &);
  void PassNullableSequence(const Nullable< Sequence<int32_t> >&);
  void PassSequenceOfNullableInts(const Sequence<Nullable<int32_t> >&);
  void PassOptionalSequenceOfNullableInts(const Optional<Sequence<Nullable<int32_t> > > &);
  void PassOptionalNullableSequenceOfNullableInts(const Optional<Nullable<Sequence<Nullable<int32_t> > > > &);
  void ReceiveCastableObjectSequence(nsTArray< nsRefPtr<TestInterface> > &);
  void ReceiveCallbackObjectSequence(nsTArray< nsRefPtr<TestCallbackInterface> > &);
  void ReceiveNullableCastableObjectSequence(nsTArray< nsRefPtr<TestInterface> > &);
  void ReceiveNullableCallbackObjectSequence(nsTArray< nsRefPtr<TestCallbackInterface> > &);
  void ReceiveCastableObjectNullableSequence(Nullable< nsTArray< nsRefPtr<TestInterface> > >&);
  void ReceiveNullableCastableObjectNullableSequence(Nullable< nsTArray< nsRefPtr<TestInterface> > >&);
  void ReceiveWeakCastableObjectSequence(nsTArray<TestInterface*> &);
  void ReceiveWeakNullableCastableObjectSequence(nsTArray<TestInterface*> &);
  void ReceiveWeakCastableObjectNullableSequence(Nullable< nsTArray<TestInterface*> >&);
  void ReceiveWeakNullableCastableObjectNullableSequence(Nullable< nsTArray<TestInterface*> >&);
  void PassCastableObjectSequence(const Sequence< OwningNonNull<TestInterface> >&);
  void PassNullableCastableObjectSequence(const Sequence< nsRefPtr<TestInterface> > &);
  void PassCastableObjectNullableSequence(const Nullable< Sequence< OwningNonNull<TestInterface> > >&);
  void PassNullableCastableObjectNullableSequence(const Nullable< Sequence< nsRefPtr<TestInterface> > >&);
  void PassOptionalSequence(const Optional<Sequence<int32_t> >&);
  void PassOptionalNullableSequence(const Optional<Nullable<Sequence<int32_t> > >&);
  void PassOptionalNullableSequenceWithDefaultValue(const Nullable< Sequence<int32_t> >&);
  void PassOptionalObjectSequence(const Optional<Sequence<OwningNonNull<TestInterface> > >&);
  void PassExternalInterfaceSequence(const Sequence<nsRefPtr<TestExternalInterface> >&);
  void PassNullableExternalInterfaceSequence(const Sequence<nsRefPtr<TestExternalInterface> >&);

  void ReceiveStringSequence(nsTArray<nsString>&);
  void PassStringSequence(const Sequence<nsString>&);

  void ReceiveByteStringSequence(nsTArray<nsCString>&);
  void PassByteStringSequence(const Sequence<nsCString>&);

  void ReceiveAnySequence(JSContext*, nsTArray<JS::Value>&);
  void ReceiveNullableAnySequence(JSContext*, Nullable<nsTArray<JS::Value> >&);
  void ReceiveAnySequenceSequence(JSContext*, nsTArray<nsTArray<JS::Value> >&);

  void ReceiveObjectSequence(JSContext*, nsTArray<JSObject*>&);
  void ReceiveNullableObjectSequence(JSContext*, nsTArray<JSObject*>&);

  void PassSequenceOfSequences(const Sequence< Sequence<int32_t> >&);
  void ReceiveSequenceOfSequences(nsTArray< nsTArray<int32_t> >&);

  // Typed array types
  void PassArrayBuffer(const ArrayBuffer&);
  void PassNullableArrayBuffer(const Nullable<ArrayBuffer>&);
  void PassOptionalArrayBuffer(const Optional<ArrayBuffer>&);
  void PassOptionalNullableArrayBuffer(const Optional<Nullable<ArrayBuffer> >&);
  void PassOptionalNullableArrayBufferWithDefaultValue(const Nullable<ArrayBuffer>&);
  void PassArrayBufferView(const ArrayBufferView&);
  void PassInt8Array(const Int8Array&);
  void PassInt16Array(const Int16Array&);
  void PassInt32Array(const Int32Array&);
  void PassUint8Array(const Uint8Array&);
  void PassUint16Array(const Uint16Array&);
  void PassUint32Array(const Uint32Array&);
  void PassUint8ClampedArray(const Uint8ClampedArray&);
  void PassFloat32Array(const Float32Array&);
  void PassFloat64Array(const Float64Array&);
  void PassSequenceOfArrayBuffers(const Sequence<ArrayBuffer>&);
  void PassSequenceOfNullableArrayBuffers(const Sequence<Nullable<ArrayBuffer> >&);
  void PassVariadicTypedArray(const Sequence<Float32Array>&);
  void PassVariadicNullableTypedArray(const Sequence<Nullable<Float32Array> >&);
  JSObject* ReceiveUint8Array(JSContext*);

  // DOMString types
  void PassString(const nsAString&);
  void PassNullableString(const nsAString&);
  void PassOptionalString(const Optional<nsAString>&);
  void PassOptionalStringWithDefaultValue(const nsAString&);
  void PassOptionalNullableString(const Optional<nsAString>&);
  void PassOptionalNullableStringWithDefaultValue(const nsAString&);
  void PassVariadicString(const Sequence<nsString>&);

  // ByteString types
  void PassByteString(const nsCString&);
  void PassNullableByteString(const nsCString&);
  void PassOptionalByteString(const Optional<nsCString>&);
  void PassOptionalNullableByteString(const Optional<nsCString>&);
  void PassVariadicByteString(const Sequence<nsCString>&);

  // Enumerated types
  void PassEnum(TestEnum);
  void PassNullableEnum(const Nullable<TestEnum>&);
  void PassOptionalEnum(const Optional<TestEnum>&);
  void PassEnumWithDefault(TestEnum);
  void PassOptionalNullableEnum(const Optional<Nullable<TestEnum> >&);
  void PassOptionalNullableEnumWithDefaultValue(const Nullable<TestEnum>&);
  void PassOptionalNullableEnumWithDefaultValue2(const Nullable<TestEnum>&);
  TestEnum ReceiveEnum();
  Nullable<TestEnum> ReceiveNullableEnum();
  TestEnum EnumAttribute();
  TestEnum ReadonlyEnumAttribute();
  void SetEnumAttribute(TestEnum);

  // Callback types
  void PassCallback(TestCallback&);
  void PassNullableCallback(TestCallback*);
  void PassOptionalCallback(const Optional<OwningNonNull<TestCallback> >&);
  void PassOptionalNullableCallback(const Optional<nsRefPtr<TestCallback> >&);
  void PassOptionalNullableCallbackWithDefaultValue(TestCallback*);
  already_AddRefed<TestCallback> ReceiveCallback();
  already_AddRefed<TestCallback> ReceiveNullableCallback();
  void PassNullableTreatAsNullCallback(TestTreatAsNullCallback*);
  void PassOptionalNullableTreatAsNullCallback(const Optional<nsRefPtr<TestTreatAsNullCallback> >&);
  void PassOptionalNullableTreatAsNullCallbackWithDefaultValue(TestTreatAsNullCallback*);
  void SetTreatAsNullCallback(TestTreatAsNullCallback&);
  already_AddRefed<TestTreatAsNullCallback> TreatAsNullCallback();
  void SetNullableTreatAsNullCallback(TestTreatAsNullCallback*);
  already_AddRefed<TestTreatAsNullCallback> GetNullableTreatAsNullCallback();

  // Any types
  void PassAny(JSContext*, JS::Handle<JS::Value>);
  void PassVariadicAny(JSContext*, const Sequence<JS::Value>&);
  void PassOptionalAny(JSContext*, const Optional<JS::Handle<JS::Value> >&);
  void PassAnyDefaultNull(JSContext*, JS::Handle<JS::Value>);
  void PassSequenceOfAny(JSContext*, const Sequence<JS::Value>&);
  void PassNullableSequenceOfAny(JSContext*, const Nullable<Sequence<JS::Value> >&);
  void PassOptionalSequenceOfAny(JSContext*, const Optional<Sequence<JS::Value> >&);
  void PassOptionalNullableSequenceOfAny(JSContext*, const Optional<Nullable<Sequence<JS::Value> > >&);
  void PassOptionalSequenceOfAnyWithDefaultValue(JSContext*, const Nullable<Sequence<JS::Value> >&);
  void PassSequenceOfSequenceOfAny(JSContext*, const Sequence<Sequence<JS::Value> >&);
  void PassSequenceOfNullableSequenceOfAny(JSContext*, const Sequence<Nullable<Sequence<JS::Value> > >&);
  void PassNullableSequenceOfNullableSequenceOfAny(JSContext*, const Nullable<Sequence<Nullable<Sequence<JS::Value> > > >&);
  void PassOptionalNullableSequenceOfNullableSequenceOfAny(JSContext*, const Optional<Nullable<Sequence<Nullable<Sequence<JS::Value> > > > >&);
  JS::Value ReceiveAny(JSContext*);

  // object types
  void PassObject(JSContext*, JS::Handle<JSObject*>);
  void PassVariadicObject(JSContext*, const Sequence<JSObject*>&);
  void PassNullableObject(JSContext*, JS::Handle<JSObject*>);
  void PassVariadicNullableObject(JSContext*, const Sequence<JSObject*>&);
  void PassOptionalObject(JSContext*, const Optional<JS::Handle<JSObject*> >&);
  void PassOptionalNullableObject(JSContext*, const Optional<JS::Handle<JSObject*> >&);
  void PassOptionalNullableObjectWithDefaultValue(JSContext*, JS::Handle<JSObject*>);
  void PassSequenceOfObject(JSContext*, const Sequence<JSObject*>&);
  void PassSequenceOfNullableObject(JSContext*, const Sequence<JSObject*>&);
  void PassNullableSequenceOfObject(JSContext*, const Nullable<Sequence<JSObject*> >&);
  void PassOptionalNullableSequenceOfNullableSequenceOfObject(JSContext*, const Optional<Nullable<Sequence<Nullable<Sequence<JSObject*> > > > >&);
  void PassOptionalNullableSequenceOfNullableSequenceOfNullableObject(JSContext*, const Optional<Nullable<Sequence<Nullable<Sequence<JSObject*> > > > >&);
  JSObject* ReceiveObject(JSContext*);
  JSObject* ReceiveNullableObject(JSContext*);

  // Union types
  void PassUnion(JSContext*, const ObjectOrLong& arg);
  void PassUnionWithNullable(JSContext* cx, const ObjectOrNullOrLong& arg)
  {
    OwningObjectOrLong returnValue;
    if (arg.IsNull()) {
    } else if (arg.IsObject()) {
      JS::Rooted<JSObject*> obj(cx, arg.GetAsObject());
      JS_GetClass(obj);
      returnValue.SetAsObject() = obj;
    } else {
      int32_t i = arg.GetAsLong();
      i += 1;
      returnValue.SetAsLong() = i;
    }
  }
#ifdef DEBUG
  void PassUnion2(const LongOrBoolean& arg);
  void PassUnion3(JSContext*, const ObjectOrLongOrBoolean& arg);
  void PassUnion4(const NodeOrLongOrBoolean& arg);
  void PassUnion5(JSContext*, const ObjectOrBoolean& arg);
  void PassUnion6(JSContext*, const ObjectOrString& arg);
  void PassUnion7(JSContext*, const ObjectOrStringOrLong& arg);
  void PassUnion8(JSContext*, const ObjectOrStringOrBoolean& arg);
  void PassUnion9(JSContext*, const ObjectOrStringOrLongOrBoolean& arg);
  void PassUnion10(const EventInitOrLong& arg);
  void PassUnion11(JSContext*, const CustomEventInitOrLong& arg);
  void PassUnion12(const EventInitOrLong& arg);
  void PassUnion13(JSContext*, const ObjectOrLongOrNull& arg);
  void PassUnion14(JSContext*, const ObjectOrLongOrNull& arg);
#endif
  void PassNullableUnion(JSContext*, const Nullable<ObjectOrLong>&);
  void PassOptionalUnion(JSContext*, const Optional<ObjectOrLong>&);
  void PassOptionalNullableUnion(JSContext*, const Optional<Nullable<ObjectOrLong> >&);
  void PassOptionalNullableUnionWithDefaultValue(JSContext*, const Nullable<ObjectOrLong>&);
  //void PassUnionWithInterfaces(const TestInterfaceOrTestExternalInterface& arg);
  //void PassUnionWithInterfacesAndNullable(const TestInterfaceOrNullOrTestExternalInterface& arg);
  void PassUnionWithArrayBuffer(const ArrayBufferOrLong&);
  void PassUnionWithString(JSContext*, const StringOrObject&);
  //void PassUnionWithEnum(JSContext*, const TestEnumOrObject&);
  //void PassUnionWithCallback(JSContext*, const TestCallbackOrLong&);
  void PassUnionWithObject(JSContext*, const ObjectOrLong&);

  void PassUnionWithDefaultValue1(const DoubleOrString& arg);
  void PassUnionWithDefaultValue2(const DoubleOrString& arg);
  void PassUnionWithDefaultValue3(const DoubleOrString& arg);
  void PassUnionWithDefaultValue4(const FloatOrString& arg);
  void PassUnionWithDefaultValue5(const FloatOrString& arg);
  void PassUnionWithDefaultValue6(const FloatOrString& arg);
  void PassUnionWithDefaultValue7(const UnrestrictedDoubleOrString& arg);
  void PassUnionWithDefaultValue8(const UnrestrictedDoubleOrString& arg);
  void PassUnionWithDefaultValue9(const UnrestrictedDoubleOrString& arg);
  void PassUnionWithDefaultValue10(const UnrestrictedDoubleOrString& arg);
  void PassUnionWithDefaultValue11(const UnrestrictedFloatOrString& arg);
  void PassUnionWithDefaultValue12(const UnrestrictedFloatOrString& arg);
  void PassUnionWithDefaultValue13(const UnrestrictedFloatOrString& arg);
  void PassUnionWithDefaultValue14(const UnrestrictedFloatOrString& arg);

  void PassNullableUnionWithDefaultValue1(const Nullable<DoubleOrString>& arg);
  void PassNullableUnionWithDefaultValue2(const Nullable<DoubleOrString>& arg);
  void PassNullableUnionWithDefaultValue3(const Nullable<DoubleOrString>& arg);
  void PassNullableUnionWithDefaultValue4(const Nullable<FloatOrString>& arg);
  void PassNullableUnionWithDefaultValue5(const Nullable<FloatOrString>& arg);
  void PassNullableUnionWithDefaultValue6(const Nullable<FloatOrString>& arg);
  void PassNullableUnionWithDefaultValue7(const Nullable<UnrestrictedDoubleOrString>& arg);
  void PassNullableUnionWithDefaultValue8(const Nullable<UnrestrictedDoubleOrString>& arg);
  void PassNullableUnionWithDefaultValue9(const Nullable<UnrestrictedDoubleOrString>& arg);
  void PassNullableUnionWithDefaultValue10(const Nullable<UnrestrictedFloatOrString>& arg);
  void PassNullableUnionWithDefaultValue11(const Nullable<UnrestrictedFloatOrString>& arg);
  void PassNullableUnionWithDefaultValue12(const Nullable<UnrestrictedFloatOrString>& arg);

  void PassSequenceOfUnions(const Sequence<OwningCanvasPatternOrCanvasGradient>&);
  void PassVariadicUnion(const Sequence<OwningCanvasPatternOrCanvasGradient>&);

  void ReceiveUnion(OwningCanvasPatternOrCanvasGradient&);
  void ReceiveUnion2(JSContext*, OwningObjectOrLong&);
  void ReceiveUnionContainingNull(OwningCanvasPatternOrNullOrCanvasGradient&);
  void ReceiveNullableUnion(Nullable<OwningCanvasPatternOrCanvasGradient>&);
  void ReceiveNullableUnion2(JSContext*, Nullable<OwningObjectOrLong>&);
  void GetWritableUnion(OwningCanvasPatternOrCanvasGradient&);
  void SetWritableUnion(const CanvasPatternOrCanvasGradient&);
  void GetWritableUnionContainingNull(OwningCanvasPatternOrNullOrCanvasGradient&);
  void SetWritableUnionContainingNull(const CanvasPatternOrNullOrCanvasGradient&);
  void GetWritableNullableUnion(Nullable<OwningCanvasPatternOrCanvasGradient>&);
  void SetWritableNullableUnion(const Nullable<CanvasPatternOrCanvasGradient>&);

  // Date types
  void PassDate(Date);
  void PassNullableDate(const Nullable<Date>&);
  void PassOptionalDate(const Optional<Date>&);
  void PassOptionalNullableDate(const Optional<Nullable<Date> >&);
  void PassOptionalNullableDateWithDefaultValue(const Nullable<Date>&);
  void PassDateSequence(const Sequence<Date>&);
  void PassNullableDateSequence(const Sequence<Nullable<Date> >&);
  Date ReceiveDate();
  Nullable<Date> ReceiveNullableDate();

  // binaryNames tests
  void MethodRenamedTo();
  void MethodRenamedTo(int8_t);
  int8_t AttributeGetterRenamedTo();
  int8_t AttributeRenamedTo();
  void SetAttributeRenamedTo(int8_t);

  // Dictionary tests
  void PassDictionary(JSContext*, const Dict&);
  void ReceiveDictionary(JSContext*, Dict&);
  void ReceiveNullableDictionary(JSContext*, Nullable<Dict>&);
  void PassOtherDictionary(const GrandparentDict&);
  void PassSequenceOfDictionaries(JSContext*, const Sequence<Dict>&);
  void PassDictionaryOrLong(JSContext*, const Dict&);
  void PassDictionaryOrLong(int32_t);
  void PassDictContainingDict(JSContext*, const DictContainingDict&);
  void PassDictContainingSequence(JSContext*, const DictContainingSequence&);
  void ReceiveDictContainingSequence(JSContext*, DictContainingSequence&);

  // Typedefs
  void ExerciseTypedefInterfaces1(TestInterface&);
  already_AddRefed<TestInterface> ExerciseTypedefInterfaces2(TestInterface*);
  void ExerciseTypedefInterfaces3(TestInterface&);

  // Static methods and attributes
  static void StaticMethod(const GlobalObject&, bool);
  static void StaticMethodWithContext(const GlobalObject&, JSContext*,
                                      JS::Value);
  static bool StaticAttribute(const GlobalObject&);
  static void SetStaticAttribute(const GlobalObject&, bool);

  // Overload resolution tests
  bool Overload1(TestInterface&);
  TestInterface* Overload1(const nsAString&, TestInterface&);
  void Overload2(TestInterface&);
  void Overload2(JSContext*, const Dict&);
  void Overload2(bool);
  void Overload2(const nsAString&);
  void Overload2(Date);
  void Overload3(TestInterface&);
  void Overload3(const TestCallback&);
  void Overload3(bool);
  void Overload4(TestInterface&);
  void Overload4(TestCallbackInterface&);
  void Overload4(const nsAString&);
  void Overload5(int32_t);
  void Overload5(TestEnum);
  void Overload6(int32_t);
  void Overload6(bool);
  void Overload7(int32_t);
  void Overload7(bool);
  void Overload7(const nsCString&);
  void Overload8(int32_t);
  void Overload8(TestInterface&);
  void Overload9(const Nullable<int32_t>&);
  void Overload9(const nsAString&);
  void Overload10(const Nullable<int32_t>&);
  void Overload10(JSContext*, JS::Handle<JSObject*>);
  void Overload11(int32_t);
  void Overload11(const nsAString&);
  void Overload12(int32_t);
  void Overload12(const Nullable<bool>&);
  void Overload13(const Nullable<int32_t>&);
  void Overload13(bool);
  void Overload14(const Optional<int32_t>&);
  void Overload14(TestInterface&);
  void Overload15(int32_t);
  void Overload15(const Optional<NonNull<TestInterface> >&);
  void Overload16(int32_t);
  void Overload16(const Optional<TestInterface*>&);

  // Variadic handling
  void PassVariadicThirdArg(const nsAString&, int32_t,
                            const Sequence<OwningNonNull<TestInterface> >&);

  // Conditionally exposed methods/attributes
  bool Prefable1();
  bool Prefable2();
  bool Prefable3();
  bool Prefable4();
  bool Prefable5();
  bool Prefable6();
  bool Prefable7();
  bool Prefable8();
  bool Prefable9();
  void Prefable10();
  void Prefable11();
  bool Prefable12();
  void Prefable13();
  bool Prefable14();
  bool Prefable15();
  bool Prefable16();
  void Prefable17();
  void Prefable18();
  void Prefable19();

  // Miscellania
  int32_t AttrWithLenientThis();
  void SetAttrWithLenientThis(int32_t);
  uint32_t UnforgeableAttr();
  uint32_t UnforgeableAttr2();
  void Stringify(nsString&);
  void PassRenamedInterface(nsRenamedInterface&);
  TestInterface* PutForwardsAttr();
  TestInterface* PutForwardsAttr2();
  TestInterface* PutForwardsAttr3();
  JS::Value JsonifierShouldSkipThis(JSContext*);
  void SetJsonifierShouldSkipThis(JSContext*, JS::Rooted<JS::Value>&);
  TestParentInterface* JsonifierShouldSkipThis2();
  void SetJsonifierShouldSkipThis2(TestParentInterface&);
  TestCallbackInterface* JsonifierShouldSkipThis3();
  void SetJsonifierShouldSkipThis3(TestCallbackInterface&);
  void ThrowingMethod(ErrorResult& aRv);
  bool GetThrowingAttr(ErrorResult& aRv) const;
  void SetThrowingAttr(bool arg, ErrorResult& aRv);
  bool GetThrowingGetterAttr(ErrorResult& aRv) const;
  void SetThrowingGetterAttr(bool arg);
  bool ThrowingSetterAttr() const;
  void SetThrowingSetterAttr(bool arg, ErrorResult& aRv);
  int16_t LegacyCall(JS::Value, uint32_t, TestInterface&);
  void PassArgsWithDefaults(JSContext*, const Optional<int32_t>&,
                            TestInterface*, const Dict&, double,
                            const Optional<float>&);

  // Methods and properties imported via "implements"
  bool ImplementedProperty();
  void SetImplementedProperty(bool);
  void ImplementedMethod();
  bool ImplementedParentProperty();
  void SetImplementedParentProperty(bool);
  void ImplementedParentMethod();
  bool IndirectlyImplementedProperty();
  void SetIndirectlyImplementedProperty(bool);
  void IndirectlyImplementedMethod();
  uint32_t DiamondImplementedProperty();

  // Test EnforceRange/Clamp
  void DontEnforceRangeOrClamp(int8_t);
  void DoEnforceRange(int8_t);
  void DoClamp(int8_t);
  void SetEnforcedByte(int8_t);
  int8_t EnforcedByte();
  void SetClampedByte(int8_t);
  int8_t ClampedByte();

private:
  // We add signatures here that _could_ start matching if the codegen
  // got data types wrong.  That way if it ever does we'll have a call
  // to these private deleted methods and compilation will fail.
  void SetReadonlyByte(int8_t) MOZ_DELETE;
  template<typename T>
  void SetWritableByte(T) MOZ_DELETE;
  template<typename T>
  void PassByte(T) MOZ_DELETE;
  void PassNullableByte(Nullable<int8_t>&) MOZ_DELETE;
  template<typename T>
  void PassOptionalByte(const Optional<T>&) MOZ_DELETE;
  template<typename T>
  void PassOptionalByteWithDefault(T) MOZ_DELETE;
  void PassVariadicByte(Sequence<int8_t>&) MOZ_DELETE;

  void SetReadonlyShort(int16_t) MOZ_DELETE;
  template<typename T>
  void SetWritableShort(T) MOZ_DELETE;
  template<typename T>
  void PassShort(T) MOZ_DELETE;
  template<typename T>
  void PassOptionalShort(const Optional<T>&) MOZ_DELETE;
  template<typename T>
  void PassOptionalShortWithDefault(T) MOZ_DELETE;

  void SetReadonlyLong(int32_t) MOZ_DELETE;
  template<typename T>
  void SetWritableLong(T) MOZ_DELETE;
  template<typename T>
  void PassLong(T) MOZ_DELETE;
  template<typename T>
  void PassOptionalLong(const Optional<T>&) MOZ_DELETE;
  template<typename T>
  void PassOptionalLongWithDefault(T) MOZ_DELETE;

  void SetReadonlyLongLong(int64_t) MOZ_DELETE;
  template<typename T>
  void SetWritableLongLong(T) MOZ_DELETE;
  template<typename T>
  void PassLongLong(T) MOZ_DELETE;
  template<typename T>
  void PassOptionalLongLong(const Optional<T>&) MOZ_DELETE;
  template<typename T>
  void PassOptionalLongLongWithDefault(T) MOZ_DELETE;

  void SetReadonlyOctet(uint8_t) MOZ_DELETE;
  template<typename T>
  void SetWritableOctet(T) MOZ_DELETE;
  template<typename T>
  void PassOctet(T) MOZ_DELETE;
  template<typename T>
  void PassOptionalOctet(const Optional<T>&) MOZ_DELETE;
  template<typename T>
  void PassOptionalOctetWithDefault(T) MOZ_DELETE;

  void SetReadonlyUnsignedShort(uint16_t) MOZ_DELETE;
  template<typename T>
  void SetWritableUnsignedShort(T) MOZ_DELETE;
  template<typename T>
  void PassUnsignedShort(T) MOZ_DELETE;
  template<typename T>
  void PassOptionalUnsignedShort(const Optional<T>&) MOZ_DELETE;
  template<typename T>
  void PassOptionalUnsignedShortWithDefault(T) MOZ_DELETE;

  void SetReadonlyUnsignedLong(uint32_t) MOZ_DELETE;
  template<typename T>
  void SetWritableUnsignedLong(T) MOZ_DELETE;
  template<typename T>
  void PassUnsignedLong(T) MOZ_DELETE;
  template<typename T>
  void PassOptionalUnsignedLong(const Optional<T>&) MOZ_DELETE;
  template<typename T>
  void PassOptionalUnsignedLongWithDefault(T) MOZ_DELETE;

  void SetReadonlyUnsignedLongLong(uint64_t) MOZ_DELETE;
  template<typename T>
  void SetWritableUnsignedLongLong(T) MOZ_DELETE;
  template<typename T>
  void PassUnsignedLongLong(T) MOZ_DELETE;
  template<typename T>
  void PassOptionalUnsignedLongLong(const Optional<T>&) MOZ_DELETE;
  template<typename T>
  void PassOptionalUnsignedLongLongWithDefault(T) MOZ_DELETE;

  // Enforce that only const things are passed for sequences
  void PassSequence(Sequence<int32_t> &) MOZ_DELETE;
  void PassNullableSequence(Nullable< Sequence<int32_t> >&) MOZ_DELETE;
  void PassOptionalNullableSequenceWithDefaultValue(Nullable< Sequence<int32_t> >&) MOZ_DELETE;
  void PassSequenceOfAny(JSContext*, Sequence<JS::Value>&) MOZ_DELETE;
  void PassNullableSequenceOfAny(JSContext*, Nullable<Sequence<JS::Value> >&) MOZ_DELETE;
  void PassOptionalSequenceOfAny(JSContext*, Optional<Sequence<JS::Value> >&) MOZ_DELETE;
  void PassOptionalNullableSequenceOfAny(JSContext*, Optional<Nullable<Sequence<JS::Value> > >&) MOZ_DELETE;
  void PassOptionalSequenceOfAnyWithDefaultValue(JSContext*, Nullable<Sequence<JS::Value> >&) MOZ_DELETE;
  void PassSequenceOfSequenceOfAny(JSContext*, Sequence<Sequence<JS::Value> >&) MOZ_DELETE;
  void PassSequenceOfNullableSequenceOfAny(JSContext*, Sequence<Nullable<Sequence<JS::Value> > >&) MOZ_DELETE;
  void PassNullableSequenceOfNullableSequenceOfAny(JSContext*, Nullable<Sequence<Nullable<Sequence<JS::Value> > > >&) MOZ_DELETE;
  void PassOptionalNullableSequenceOfNullableSequenceOfAny(JSContext*, Optional<Nullable<Sequence<Nullable<Sequence<JS::Value> > > > >&) MOZ_DELETE;
  void PassSequenceOfObject(JSContext*, Sequence<JSObject*>&) MOZ_DELETE;
  void PassSequenceOfNullableObject(JSContext*, Sequence<JSObject*>&) MOZ_DELETE;
  void PassOptionalNullableSequenceOfNullableSequenceOfObject(JSContext*, Optional<Nullable<Sequence<Nullable<Sequence<JSObject*> > > > >&) MOZ_DELETE;
  void PassOptionalNullableSequenceOfNullableSequenceOfNullableObject(JSContext*, Optional<Nullable<Sequence<Nullable<Sequence<JSObject*> > > > >&) MOZ_DELETE;

  // Enforce that only const things are passed for optional
  void PassOptionalByte(Optional<int8_t>&) MOZ_DELETE;
  void PassOptionalNullableByte(Optional<Nullable<int8_t> >&) MOZ_DELETE;
  void PassOptionalShort(Optional<int16_t>&) MOZ_DELETE;
  void PassOptionalLong(Optional<int32_t>&) MOZ_DELETE;
  void PassOptionalLongLong(Optional<int64_t>&) MOZ_DELETE;
  void PassOptionalOctet(Optional<uint8_t>&) MOZ_DELETE;
  void PassOptionalUnsignedShort(Optional<uint16_t>&) MOZ_DELETE;
  void PassOptionalUnsignedLong(Optional<uint32_t>&) MOZ_DELETE;
  void PassOptionalUnsignedLongLong(Optional<uint64_t>&) MOZ_DELETE;
  void PassOptionalSelf(Optional<TestInterface*> &) MOZ_DELETE;
  void PassOptionalNonNullSelf(Optional<NonNull<TestInterface> >&) MOZ_DELETE;
  void PassOptionalOther(Optional<IndirectlyImplementedInterface*>&);
  void PassOptionalNonNullOther(Optional<NonNull<IndirectlyImplementedInterface> >&);
  void PassOptionalExternal(Optional<TestExternalInterface*>&) MOZ_DELETE;
  void PassOptionalNonNullExternal(Optional<TestExternalInterface*>&) MOZ_DELETE;
  void PassOptionalSequence(Optional<Sequence<int32_t> >&) MOZ_DELETE;
  void PassOptionalNullableSequence(Optional<Nullable<Sequence<int32_t> > >&) MOZ_DELETE;
  void PassOptionalObjectSequence(Optional<Sequence<OwningNonNull<TestInterface> > >&) MOZ_DELETE;
  void PassOptionalArrayBuffer(Optional<ArrayBuffer>&) MOZ_DELETE;
  void PassOptionalNullableArrayBuffer(Optional<ArrayBuffer*>&) MOZ_DELETE;
  void PassOptionalEnum(Optional<TestEnum>&) MOZ_DELETE;
  void PassOptionalCallback(JSContext*, Optional<OwningNonNull<TestCallback> >&) MOZ_DELETE;
  void PassOptionalNullableCallback(JSContext*, Optional<nsRefPtr<TestCallback> >&) MOZ_DELETE;
  void PassOptionalAny(Optional<JS::Handle<JS::Value> >&) MOZ_DELETE;

  // And test that string stuff is always const
  void PassString(nsAString&) MOZ_DELETE;
  void PassNullableString(nsAString&) MOZ_DELETE;
  void PassOptionalString(Optional<nsAString>&) MOZ_DELETE;
  void PassOptionalStringWithDefaultValue(nsAString&) MOZ_DELETE;
  void PassOptionalNullableString(Optional<nsAString>&) MOZ_DELETE;
  void PassOptionalNullableStringWithDefaultValue(nsAString&) MOZ_DELETE;
  void PassVariadicString(Sequence<nsString>&) MOZ_DELETE;

  // cstrings should be const as well
  void PassByteString(nsCString&) MOZ_DELETE;
  void PassNullableByteString(nsCString&) MOZ_DELETE;
  void PassOptionalByteString(Optional<nsCString>&) MOZ_DELETE;
  void PassOptionalNullableByteString(Optional<nsCString>&) MOZ_DELETE;
  void PassVariadicByteString(Sequence<nsCString>&) MOZ_DELETE;

  // Make sure dictionary arguments are always const
  void PassDictionary(JSContext*, Dict&) MOZ_DELETE;
  void PassOtherDictionary(GrandparentDict&) MOZ_DELETE;
  void PassSequenceOfDictionaries(JSContext*, Sequence<Dict>&) MOZ_DELETE;
  void PassDictionaryOrLong(JSContext*, Dict&) MOZ_DELETE;
  void PassDictContainingDict(JSContext*, DictContainingDict&) MOZ_DELETE;
  void PassDictContainingSequence(DictContainingSequence&) MOZ_DELETE;

  // Make sure various nullable things are always const
  void PassNullableEnum(Nullable<TestEnum>&) MOZ_DELETE;

  // Make sure unions are always const
  void PassUnion(JSContext*, ObjectOrLong& arg) MOZ_DELETE;
  void PassUnionWithNullable(JSContext*, ObjectOrNullOrLong& arg) MOZ_DELETE;
  void PassNullableUnion(JSContext*, Nullable<ObjectOrLong>&) MOZ_DELETE;
  void PassOptionalUnion(JSContext*, Optional<ObjectOrLong>&) MOZ_DELETE;
  void PassOptionalNullableUnion(JSContext*, Optional<Nullable<ObjectOrLong> >&) MOZ_DELETE;
  void PassOptionalNullableUnionWithDefaultValue(JSContext*, Nullable<ObjectOrLong>&) MOZ_DELETE;

  // Make sure various date stuff is const as needed
  void PassNullableDate(Nullable<Date>&) MOZ_DELETE;
  void PassOptionalDate(Optional<Date>&) MOZ_DELETE;
  void PassOptionalNullableDate(Optional<Nullable<Date> >&) MOZ_DELETE;
  void PassOptionalNullableDateWithDefaultValue(Nullable<Date>&) MOZ_DELETE;
  void PassDateSequence(Sequence<Date>&) MOZ_DELETE;
  void PassNullableDateSequence(Sequence<Nullable<Date> >&) MOZ_DELETE;

  // Make sure variadics are const as needed
  void PassVariadicAny(JSContext*, Sequence<JS::Value>&) MOZ_DELETE;
  void PassVariadicObject(JSContext*, Sequence<JSObject*>&) MOZ_DELETE;
  void PassVariadicNullableObject(JSContext*, Sequence<JSObject*>&) MOZ_DELETE;

  // Ensure NonNull does not leak in
  void PassSelf(NonNull<TestInterface>&) MOZ_DELETE;
  void PassSelf(OwningNonNull<TestInterface>&) MOZ_DELETE;
  void PassSelf(const NonNull<TestInterface>&) MOZ_DELETE;
  void PassSelf(const OwningNonNull<TestInterface>&) MOZ_DELETE;
  void PassOther(NonNull<IndirectlyImplementedInterface>&) MOZ_DELETE;
  void PassOther(const NonNull<IndirectlyImplementedInterface>&) MOZ_DELETE;
  void PassOther(OwningNonNull<IndirectlyImplementedInterface>&) MOZ_DELETE;
  void PassOther(const OwningNonNull<IndirectlyImplementedInterface>&) MOZ_DELETE;
  void PassCallbackInterface(OwningNonNull<TestCallbackInterface>&) MOZ_DELETE;
  void PassCallbackInterface(const OwningNonNull<TestCallbackInterface>&) MOZ_DELETE;
  void PassCallbackInterface(NonNull<TestCallbackInterface>&) MOZ_DELETE;
  void PassCallbackInterface(const NonNull<TestCallbackInterface>&) MOZ_DELETE;
  void PassCallback(OwningNonNull<TestCallback>&) MOZ_DELETE;
  void PassCallback(const OwningNonNull<TestCallback>&) MOZ_DELETE;
  void PassCallback(NonNull<TestCallback>&) MOZ_DELETE;
  void PassCallback(const NonNull<TestCallback>&) MOZ_DELETE;
  void PassString(const NonNull<nsAString>&) MOZ_DELETE;
  void PassString(NonNull<nsAString>&) MOZ_DELETE;
  void PassString(const OwningNonNull<nsAString>&) MOZ_DELETE;
  void PassString(OwningNonNull<nsAString>&) MOZ_DELETE;
};

class TestIndexedGetterInterface : public nsISupports,
                                   public nsWrapperCache
{
public:
  NS_DECL_ISUPPORTS

  // We need a GetParentObject to make binding codegen happy
  virtual nsISupports* GetParentObject();

  uint32_t IndexedGetter(uint32_t, bool&);
  uint32_t IndexedGetter(uint32_t&) MOZ_DELETE;
  uint32_t Item(uint32_t&);
  uint32_t Item(uint32_t, bool&) MOZ_DELETE;
  uint32_t Length();
};

class TestNamedGetterInterface : public nsISupports,
                                 public nsWrapperCache
{
public:
  NS_DECL_ISUPPORTS

  // We need a GetParentObject to make binding codegen happy
  virtual nsISupports* GetParentObject();

  void NamedGetter(const nsAString&, bool&, nsAString&);
  void GetSupportedNames(nsTArray<nsString>&);
};

class TestIndexedGetterAndSetterAndNamedGetterInterface : public nsISupports,
                                                          public nsWrapperCache
{
public:
  NS_DECL_ISUPPORTS

  // We need a GetParentObject to make binding codegen happy
  virtual nsISupports* GetParentObject();

  void NamedGetter(const nsAString&, bool&, nsAString&);
  void GetSupportedNames(nsTArray<nsString>&);
  int32_t IndexedGetter(uint32_t, bool&);
  void IndexedSetter(uint32_t, int32_t);
  uint32_t Length();
};

class TestIndexedAndNamedGetterInterface : public nsISupports,
                                           public nsWrapperCache
{
public:
  NS_DECL_ISUPPORTS

  // We need a GetParentObject to make binding codegen happy
  virtual nsISupports* GetParentObject();

  uint32_t IndexedGetter(uint32_t, bool&);
  void NamedGetter(const nsAString&, bool&, nsAString&);
  void NamedItem(const nsAString&, nsAString&);
  uint32_t Length();
  void GetSupportedNames(nsTArray<nsString>&);
};

class TestIndexedSetterInterface : public nsISupports,
                                   public nsWrapperCache
{
public:
  NS_DECL_ISUPPORTS

  // We need a GetParentObject to make binding codegen happy
  virtual nsISupports* GetParentObject();

  void IndexedSetter(uint32_t, const nsAString&);
  void IndexedGetter(uint32_t, bool&, nsString&);
  uint32_t Length();
  void SetItem(uint32_t, const nsAString&);
};

class TestNamedSetterInterface : public nsISupports,
                                 public nsWrapperCache
{
public:
  NS_DECL_ISUPPORTS

  // We need a GetParentObject to make binding codegen happy
  virtual nsISupports* GetParentObject();

  void NamedSetter(const nsAString&, TestIndexedSetterInterface&);
  TestIndexedSetterInterface* NamedGetter(const nsAString&, bool&);
  void GetSupportedNames(nsTArray<nsString>&);
};

class TestIndexedAndNamedSetterInterface : public nsISupports,
                                           public nsWrapperCache
{
public:
  NS_DECL_ISUPPORTS

  // We need a GetParentObject to make binding codegen happy
  virtual nsISupports* GetParentObject();

  void IndexedSetter(uint32_t, TestIndexedSetterInterface&);
  TestIndexedSetterInterface* IndexedGetter(uint32_t, bool&);
  uint32_t Length();
  void NamedSetter(const nsAString&, TestIndexedSetterInterface&);
  TestIndexedSetterInterface* NamedGetter(const nsAString&, bool&);
  void SetNamedItem(const nsAString&, TestIndexedSetterInterface&);
  void GetSupportedNames(nsTArray<nsString>&);
};

class TestIndexedAndNamedGetterAndSetterInterface : public TestIndexedSetterInterface
{
public:
  uint32_t IndexedGetter(uint32_t, bool&);
  uint32_t Item(uint32_t);
  void NamedGetter(const nsAString&, bool&, nsAString&);
  void NamedItem(const nsAString&, nsAString&);
  void IndexedSetter(uint32_t, int32_t&);
  void IndexedSetter(uint32_t, const nsAString&) MOZ_DELETE;
  void NamedSetter(const nsAString&, const nsAString&);
  void Stringify(nsAString&);
  uint32_t Length();
  void GetSupportedNames(nsTArray<nsString>&);
};

class TestCppKeywordNamedMethodsInterface : public nsISupports,
                                            public nsWrapperCache
{
public:
  NS_DECL_ISUPPORTS

  // We need a GetParentObject to make binding codegen happy
  virtual nsISupports* GetParentObject();

  bool Continue();
  bool Delete();
  int32_t Volatile();
};

class TestIndexedDeleterInterface : public nsISupports,
                                    public nsWrapperCache
{
public:
  NS_DECL_ISUPPORTS

  // We need a GetParentObject to make binding codegen happy
  virtual nsISupports* GetParentObject();

  void IndexedDeleter(uint32_t, bool&);
  void IndexedDeleter(uint32_t) MOZ_DELETE;
  long IndexedGetter(uint32_t, bool&);
  uint32_t Length();
  void DelItem(uint32_t);
  void DelItem(uint32_t, bool&) MOZ_DELETE;
};

class TestIndexedDeleterWithRetvalInterface : public nsISupports,
                                              public nsWrapperCache
{
public:
  NS_DECL_ISUPPORTS

  // We need a GetParentObject to make binding codegen happy
  virtual nsISupports* GetParentObject();

  bool IndexedDeleter(uint32_t, bool&);
  bool IndexedDeleter(uint32_t) MOZ_DELETE;
  long IndexedGetter(uint32_t, bool&);
  uint32_t Length();
  bool DelItem(uint32_t);
  bool DelItem(uint32_t, bool&) MOZ_DELETE;
};

class TestNamedDeleterInterface : public nsISupports,
                                  public nsWrapperCache
{
public:
  NS_DECL_ISUPPORTS

  // We need a GetParentObject to make binding codegen happy
  virtual nsISupports* GetParentObject();

  void NamedDeleter(const nsAString&, bool&);
  long NamedGetter(const nsAString&, bool&);
  void GetSupportedNames(nsTArray<nsString>&);
};

class TestNamedDeleterWithRetvalInterface : public nsISupports,
                                            public nsWrapperCache
{
public:
  NS_DECL_ISUPPORTS

  // We need a GetParentObject to make binding codegen happy
  virtual nsISupports* GetParentObject();

  bool NamedDeleter(const nsAString&, bool&);
  bool NamedDeleter(const nsAString&) MOZ_DELETE;
  long NamedGetter(const nsAString&, bool&);
  bool DelNamedItem(const nsAString&);
  bool DelNamedItem(const nsAString&, bool&) MOZ_DELETE;
  void GetSupportedNames(nsTArray<nsString>&);
};

class TestIndexedAndNamedDeleterInterface : public nsISupports,
                                            public nsWrapperCache
{
public:
  NS_DECL_ISUPPORTS

  // We need a GetParentObject to make binding codegen happy
  virtual nsISupports* GetParentObject();

  void IndexedDeleter(uint32_t, bool&);
  long IndexedGetter(uint32_t, bool&);
  uint32_t Length();

  void NamedDeleter(const nsAString&, bool&);
  void NamedDeleter(const nsAString&) MOZ_DELETE;
  long NamedGetter(const nsAString&, bool&);
  void DelNamedItem(const nsAString&);
  void DelNamedItem(const nsAString&, bool&) MOZ_DELETE;
  void GetSupportedNames(nsTArray<nsString>&);
};

class TestParentInterface : public nsISupports,
                            public nsWrapperCache
{
public:
  NS_DECL_ISUPPORTS

  // We need a GetParentObject to make binding codegen happy
  virtual nsISupports* GetParentObject();
};

class TestChildInterface : public TestParentInterface
{
};

} // namespace dom
} // namespace mozilla

#endif /* TestBindingHeader_h */
