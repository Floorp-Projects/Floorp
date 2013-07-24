/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

typedef long myLong;
typedef TestInterface AnotherNameForTestInterface;
typedef TestInterface? NullableTestInterface;

interface TestExternalInterface;

interface TestRenamedInterface {
};

callback interface TestCallbackInterface {
  readonly attribute long foo;
  attribute DOMString bar;
  void doSomething();
  long doSomethingElse(DOMString arg, TestInterface otherArg);
  void doSequenceLongArg(sequence<long> arg);
  void doSequenceStringArg(sequence<DOMString> arg);
  sequence<long> getSequenceOfLong();
  sequence<TestInterface> getSequenceOfInterfaces();
  sequence<TestInterface>? getNullableSequenceOfInterfaces();
  sequence<TestInterface?> getSequenceOfNullableInterfaces();
  sequence<TestInterface?>? getNullableSequenceOfNullableInterfaces();
  sequence<TestCallbackInterface> getSequenceOfCallbackInterfaces();
  sequence<TestCallbackInterface>? getNullableSequenceOfCallbackInterfaces();
  sequence<TestCallbackInterface?> getSequenceOfNullableCallbackInterfaces();
  sequence<TestCallbackInterface?>? getNullableSequenceOfNullableCallbackInterfaces();
};

callback interface TestSingleOperationCallbackInterface {
  TestInterface doSomething(short arg, sequence<double> anotherArg);
};

enum TestEnum {
  "1",
  "a",
  "b"
};

callback TestCallback = void();
[TreatNonCallableAsNull] callback TestTreatAsNullCallback = void();

// Callback return value tests
callback TestIntegerReturn = long();
callback TestNullableIntegerReturn = long?();
callback TestBooleanReturn = boolean();
callback TestFloatReturn = float();
callback TestStringReturn = DOMString(long arg);
callback TestEnumReturn = TestEnum();
callback TestInterfaceReturn = TestInterface();
callback TestNullableInterfaceReturn = TestInterface?();
callback TestExternalInterfaceReturn = TestExternalInterface();
callback TestNullableExternalInterfaceReturn = TestExternalInterface?();
callback TestCallbackInterfaceReturn = TestCallbackInterface();
callback TestNullableCallbackInterfaceReturn = TestCallbackInterface?();
callback TestCallbackReturn = TestCallback();
callback TestNullableCallbackReturn = TestCallback?();
callback TestObjectReturn = object();
callback TestNullableObjectReturn = object?();
callback TestTypedArrayReturn = ArrayBuffer();
callback TestNullableTypedArrayReturn = ArrayBuffer?();
callback TestSequenceReturn = sequence<boolean>();
callback TestNullableSequenceReturn = sequence<boolean>?();
// Callback argument tests
callback TestIntegerArguments = sequence<long>(long arg1, long? arg2,
                                               sequence<long> arg3,
                                               sequence<long?>? arg4);
callback TestInterfaceArguments = void(TestInterface arg1, TestInterface? arg2,
                                       TestExternalInterface arg3,
                                       TestExternalInterface? arg4,
                                       TestCallbackInterface arg5,
                                       TestCallbackInterface? arg6,
                                       sequence<TestInterface> arg7,
                                       sequence<TestInterface?>? arg8,
                                       sequence<TestExternalInterface> arg9,
                                       sequence<TestExternalInterface?>? arg10,
                                       sequence<TestCallbackInterface> arg11,
                                       sequence<TestCallbackInterface?>? arg12);
callback TestStringEnumArguments = void(DOMString myString, DOMString? nullString,
                                        TestEnum myEnum);
callback TestObjectArguments = void(object anObj, object? anotherObj,
                                    ArrayBuffer buf, ArrayBuffer? buf2);
callback TestOptionalArguments = void(optional DOMString aString,
                                      optional object something,
                                      optional sequence<TestInterface> aSeq,
                                      optional TestInterface? anInterface,
                                      optional TestInterface anotherInterface,
                                      optional long aLong);

TestInterface implements ImplementedInterface;

// This interface is only for use in the constructor below
interface OnlyForUseInConstructor {
};

[Constructor,
 Constructor(DOMString str),
 Constructor(unsigned long num, boolean? boolArg),
 Constructor(TestInterface? iface),
 Constructor(long arg1, IndirectlyImplementedInterface iface),
 Constructor(Date arg1),
 // Constructor(long arg1, long arg2, (TestInterface or OnlyForUseInConstructor) arg3),
 NamedConstructor=Test,
 NamedConstructor=Test(DOMString str),
 NamedConstructor=Test2(DictForConstructor dict, any any1, object obj1,
                        object? obj2, sequence<Dict> seq, optional any any2,
                        optional object obj3, optional object? obj4)
 ]
interface TestInterface {
  // Integer types
  // XXXbz add tests for throwing versions of all the integer stuff
  readonly attribute byte readonlyByte;
  attribute byte writableByte;
  void passByte(byte arg);
  byte receiveByte();
  void passOptionalByte(optional byte arg);
  void passOptionalUndefinedMissingByte([TreatUndefinedAs=Missing] optional byte arg);
  void passOptionalByteWithDefault(optional byte arg = 0);
  void passOptionalUndefinedMissingByteWithDefault([TreatUndefinedAs=Missing] optional byte arg = 0);
  void passNullableByte(byte? arg);
  void passOptionalNullableByte(optional byte? arg);
  void passVariadicByte(byte... arg);

  readonly attribute short readonlyShort;
  attribute short writableShort;
  void passShort(short arg);
  short receiveShort();
  void passOptionalShort(optional short arg);
  void passOptionalShortWithDefault(optional short arg = 5);

  readonly attribute long readonlyLong;
  attribute long writableLong;
  void passLong(long arg);
  long receiveLong();
  void passOptionalLong(optional long arg);
  void passOptionalLongWithDefault(optional long arg = 7);

  readonly attribute long long readonlyLongLong;
  attribute long long writableLongLong;
  void passLongLong(long long arg);
  long long receiveLongLong();
  void passOptionalLongLong(optional long long arg);
  void passOptionalLongLongWithDefault(optional long long arg = -12);

  readonly attribute octet readonlyOctet;
  attribute octet writableOctet;
  void passOctet(octet arg);
  octet receiveOctet();
  void passOptionalOctet(optional octet arg);
  void passOptionalOctetWithDefault(optional octet arg = 19);

  readonly attribute unsigned short readonlyUnsignedShort;
  attribute unsigned short writableUnsignedShort;
  void passUnsignedShort(unsigned short arg);
  unsigned short receiveUnsignedShort();
  void passOptionalUnsignedShort(optional unsigned short arg);
  void passOptionalUnsignedShortWithDefault(optional unsigned short arg = 2);

  readonly attribute unsigned long readonlyUnsignedLong;
  attribute unsigned long writableUnsignedLong;
  void passUnsignedLong(unsigned long arg);
  unsigned long receiveUnsignedLong();
  void passOptionalUnsignedLong(optional unsigned long arg);
  void passOptionalUnsignedLongWithDefault(optional unsigned long arg = 6);

  readonly attribute unsigned long long readonlyUnsignedLongLong;
  attribute unsigned long long  writableUnsignedLongLong;
  void passUnsignedLongLong(unsigned long long arg);
  unsigned long long receiveUnsignedLongLong();
  void passOptionalUnsignedLongLong(optional unsigned long long arg);
  void passOptionalUnsignedLongLongWithDefault(optional unsigned long long arg = 17);

  attribute float writableFloat;
  attribute unrestricted float writableUnrestrictedFloat;
  attribute float? writableNullableFloat;
  attribute unrestricted float? writableNullableUnrestrictedFloat;
  attribute double writableDouble;
  attribute unrestricted double writableUnrestrictedDouble;
  attribute double? writableNullableDouble;
  attribute unrestricted double? writableNullableUnrestrictedDouble;
  void passFloat(float arg1, unrestricted float arg2,
                 float? arg3, unrestricted float? arg4,
                 double arg5, unrestricted double arg6,
                 double? arg7, unrestricted double? arg8,
                 sequence<float> arg9, sequence<unrestricted float> arg10,
                 sequence<float?> arg11, sequence<unrestricted float?> arg12,
                 sequence<double> arg13, sequence<unrestricted double> arg14,
                 sequence<double?> arg15, sequence<unrestricted double?> arg16);
  [LenientFloat]
  void passLenientFloat(float arg1, unrestricted float arg2,
                        float? arg3, unrestricted float? arg4,
                        double arg5, unrestricted double arg6,
                        double? arg7, unrestricted double? arg8,
                        sequence<float> arg9,
                        sequence<unrestricted float> arg10,
                        sequence<float?> arg11,
                        sequence<unrestricted float?> arg12,
                        sequence<double> arg13,
                        sequence<unrestricted double> arg14,
                        sequence<double?> arg15,
                        sequence<unrestricted double?> arg16);
  [LenientFloat]
  attribute float lenientFloatAttr;
  [LenientFloat]
  attribute double lenientDoubleAttr;

  void passUnrestricted(optional unrestricted float arg1 = 0,
                        optional unrestricted float arg2 = Infinity,
                        optional unrestricted float arg3 = -Infinity,
                        optional unrestricted float arg4 = NaN,
                        optional unrestricted double arg5 = 0,
                        optional unrestricted double arg6 = Infinity,
                        optional unrestricted double arg7 = -Infinity,
                        optional unrestricted double arg8 = NaN);

  // Castable interface types
  // XXXbz add tests for throwing versions of all the castable interface stuff
  TestInterface receiveSelf();
  TestInterface? receiveNullableSelf();
  TestInterface receiveWeakSelf();
  TestInterface? receiveWeakNullableSelf();
  void passSelf(TestInterface arg);
  void passNullableSelf(TestInterface? arg);
  attribute TestInterface nonNullSelf;
  attribute TestInterface? nullableSelf;
  // Optional arguments
  void passOptionalSelf(optional TestInterface? arg);
  void passOptionalNonNullSelf(optional TestInterface arg);
  void passOptionalSelfWithDefault(optional TestInterface? arg = null);

  // Non-wrapper-cache interface types
  [Creator]
  TestNonWrapperCacheInterface receiveNonWrapperCacheInterface();
  [Creator]
  TestNonWrapperCacheInterface? receiveNullableNonWrapperCacheInterface();
  [Creator]
  sequence<TestNonWrapperCacheInterface> receiveNonWrapperCacheInterfaceSequence();
  [Creator]
  sequence<TestNonWrapperCacheInterface?> receiveNullableNonWrapperCacheInterfaceSequence();
  [Creator]
  sequence<TestNonWrapperCacheInterface>? receiveNonWrapperCacheInterfaceNullableSequence();
  [Creator]
  sequence<TestNonWrapperCacheInterface?>? receiveNullableNonWrapperCacheInterfaceNullableSequence();

  // Non-castable interface types
  IndirectlyImplementedInterface receiveOther();
  IndirectlyImplementedInterface? receiveNullableOther();
  IndirectlyImplementedInterface receiveWeakOther();
  IndirectlyImplementedInterface? receiveWeakNullableOther();
  void passOther(IndirectlyImplementedInterface arg);
  void passNullableOther(IndirectlyImplementedInterface? arg);
  attribute IndirectlyImplementedInterface nonNullOther;
  attribute IndirectlyImplementedInterface? nullableOther;
  // Optional arguments
  void passOptionalOther(optional IndirectlyImplementedInterface? arg);
  void passOptionalNonNullOther(optional IndirectlyImplementedInterface arg);
  void passOptionalOtherWithDefault(optional IndirectlyImplementedInterface? arg = null);

  // External interface types
  TestExternalInterface receiveExternal();
  TestExternalInterface? receiveNullableExternal();
  TestExternalInterface receiveWeakExternal();
  TestExternalInterface? receiveWeakNullableExternal();
  void passExternal(TestExternalInterface arg);
  void passNullableExternal(TestExternalInterface? arg);
  attribute TestExternalInterface nonNullExternal;
  attribute TestExternalInterface? nullableExternal;
  // Optional arguments
  void passOptionalExternal(optional TestExternalInterface? arg);
  void passOptionalNonNullExternal(optional TestExternalInterface arg);
  void passOptionalExternalWithDefault(optional TestExternalInterface? arg = null);

  // Callback interface types
  TestCallbackInterface receiveCallbackInterface();
  TestCallbackInterface? receiveNullableCallbackInterface();
  TestCallbackInterface receiveWeakCallbackInterface();
  TestCallbackInterface? receiveWeakNullableCallbackInterface();
  void passCallbackInterface(TestCallbackInterface arg);
  void passNullableCallbackInterface(TestCallbackInterface? arg);
  attribute TestCallbackInterface nonNullCallbackInterface;
  attribute TestCallbackInterface? nullableCallbackInterface;
  // Optional arguments
  void passOptionalCallbackInterface(optional TestCallbackInterface? arg);
  void passOptionalNonNullCallbackInterface(optional TestCallbackInterface arg);
  void passOptionalCallbackInterfaceWithDefault(optional TestCallbackInterface? arg = null);

  // Miscellaneous interface tests
  IndirectlyImplementedInterface receiveConsequentialInterface();
  void passConsequentialInterface(IndirectlyImplementedInterface arg);

  // Sequence types
  sequence<long> receiveSequence();
  sequence<long>? receiveNullableSequence();
  sequence<long?> receiveSequenceOfNullableInts();
  sequence<long?>? receiveNullableSequenceOfNullableInts();
  void passSequence(sequence<long> arg);
  void passNullableSequence(sequence<long>? arg);
  void passSequenceOfNullableInts(sequence<long?> arg);
  void passOptionalSequenceOfNullableInts(optional sequence<long?> arg);
  void passOptionalNullableSequenceOfNullableInts(optional sequence<long?>? arg);
  sequence<TestInterface> receiveCastableObjectSequence();
  sequence<TestCallbackInterface> receiveCallbackObjectSequence();
  sequence<TestInterface?> receiveNullableCastableObjectSequence();
  sequence<TestCallbackInterface?> receiveNullableCallbackObjectSequence();
  sequence<TestInterface>? receiveCastableObjectNullableSequence();
  sequence<TestInterface?>? receiveNullableCastableObjectNullableSequence();
  sequence<TestInterface> receiveWeakCastableObjectSequence();
  sequence<TestInterface?> receiveWeakNullableCastableObjectSequence();
  sequence<TestInterface>? receiveWeakCastableObjectNullableSequence();
  sequence<TestInterface?>? receiveWeakNullableCastableObjectNullableSequence();
  void passCastableObjectSequence(sequence<TestInterface> arg);
  void passNullableCastableObjectSequence(sequence<TestInterface?> arg);
  void passCastableObjectNullableSequence(sequence<TestInterface>? arg);
  void passNullableCastableObjectNullableSequence(sequence<TestInterface?>? arg);
  void passOptionalSequence(optional sequence<long> arg);
  void passOptionalNullableSequence(optional sequence<long>? arg);
  void passOptionalNullableSequenceWithDefaultValue(optional sequence<long>? arg = null);
  void passOptionalObjectSequence(optional sequence<TestInterface> arg);
  void passExternalInterfaceSequence(sequence<TestExternalInterface> arg);
  void passNullableExternalInterfaceSequence(sequence<TestExternalInterface?> arg);

  sequence<DOMString> receiveStringSequence();
  void passStringSequence(sequence<DOMString> arg);

  sequence<ByteString> receiveByteStringSequence();
  void passByteStringSequence(sequence<ByteString> arg);

  sequence<any> receiveAnySequence();
  sequence<any>? receiveNullableAnySequence();
  sequence<sequence<any>> receiveAnySequenceSequence();

  sequence<object> receiveObjectSequence();
  sequence<object?> receiveNullableObjectSequence();

  void passSequenceOfSequences(sequence<sequence<long>> arg);
  sequence<sequence<long>> receiveSequenceOfSequences();

  // Typed array types
  void passArrayBuffer(ArrayBuffer arg);
  void passNullableArrayBuffer(ArrayBuffer? arg);
  void passOptionalArrayBuffer(optional ArrayBuffer arg);
  void passOptionalNullableArrayBuffer(optional ArrayBuffer? arg);
  void passOptionalNullableArrayBufferWithDefaultValue(optional ArrayBuffer? arg= null);
  void passArrayBufferView(ArrayBufferView arg);
  void passInt8Array(Int8Array arg);
  void passInt16Array(Int16Array arg);
  void passInt32Array(Int32Array arg);
  void passUint8Array(Uint8Array arg);
  void passUint16Array(Uint16Array arg);
  void passUint32Array(Uint32Array arg);
  void passUint8ClampedArray(Uint8ClampedArray arg);
  void passFloat32Array(Float32Array arg);
  void passFloat64Array(Float64Array arg);
  Uint8Array receiveUint8Array();

  // DOMString types
  void passString(DOMString arg);
  void passNullableString(DOMString? arg);
  void passOptionalString(optional DOMString arg);
  void passOptionalUndefinedMissingString([TreatUndefinedAs=Missing] optional DOMString arg);
  void passOptionalStringWithDefaultValue(optional DOMString arg = "abc");
  void passOptionalUndefinedMissingStringWithDefaultValue([TreatUndefinedAs=Missing] optional DOMString arg = "abc");
  void passOptionalNullableString(optional DOMString? arg);
  void passOptionalNullableStringWithDefaultValue(optional DOMString? arg = null);
  void passVariadicString(DOMString... arg);

  // ByteString types
  void passByteString(ByteString arg);
  void passNullableByteString(ByteString? arg);
  void passOptionalByteString(optional ByteString arg);
  void passOptionalNullableByteString(optional ByteString? arg);
  void passVariadicByteString(ByteString... arg);

  // Enumerated types
  void passEnum(TestEnum arg);
  void passNullableEnum(TestEnum? arg);
  void passOptionalEnum(optional TestEnum arg);
  void passEnumWithDefault(optional TestEnum arg = "a");
  void passOptionalNullableEnum(optional TestEnum? arg);
  void passOptionalNullableEnumWithDefaultValue(optional TestEnum? arg = null);
  void passOptionalNullableEnumWithDefaultValue2(optional TestEnum? arg = "a");
  TestEnum receiveEnum();
  TestEnum? receiveNullableEnum();
  attribute TestEnum enumAttribute;
  readonly attribute TestEnum readonlyEnumAttribute;

  // Callback types
  void passCallback(TestCallback arg);
  void passNullableCallback(TestCallback? arg);
  void passOptionalCallback(optional TestCallback arg);
  void passOptionalNullableCallback(optional TestCallback? arg);
  void passOptionalNullableCallbackWithDefaultValue(optional TestCallback? arg = null);
  TestCallback receiveCallback();
  TestCallback? receiveNullableCallback();
  void passNullableTreatAsNullCallback(TestTreatAsNullCallback? arg);
  void passOptionalNullableTreatAsNullCallback(optional TestTreatAsNullCallback? arg);
  void passOptionalNullableTreatAsNullCallbackWithDefaultValue(optional TestTreatAsNullCallback? arg = null);
  attribute TestTreatAsNullCallback treatAsNullCallback;
  attribute TestTreatAsNullCallback? nullableTreatAsNullCallback;

  // Any types
  void passAny(any arg);
  void passVariadicAny(any... arg);
  void passOptionalAny(optional any arg);
  void passAnyDefaultNull(optional any arg = null);
  void passSequenceOfAny(sequence<any> arg);
  void passNullableSequenceOfAny(sequence<any>? arg);
  void passOptionalSequenceOfAny(optional sequence<any> arg);
  void passOptionalNullableSequenceOfAny(optional sequence<any>? arg);
  void passOptionalSequenceOfAnyWithDefaultValue(optional sequence<any>? arg = null);
  void passSequenceOfSequenceOfAny(sequence<sequence<any>> arg);
  void passSequenceOfNullableSequenceOfAny(sequence<sequence<any>?> arg);
  void passNullableSequenceOfNullableSequenceOfAny(sequence<sequence<any>?>? arg);
  void passOptionalNullableSequenceOfNullableSequenceOfAny(optional sequence<sequence<any>?>? arg);
  any receiveAny();

  // object types
  void passObject(object arg);
  void passVariadicObject(object... arg);
  void passNullableObject(object? arg);
  void passVariadicNullableObject(object... arg);
  void passOptionalObject(optional object arg);
  void passOptionalNullableObject(optional object? arg);
  void passOptionalNullableObjectWithDefaultValue(optional object? arg = null);
  void passSequenceOfObject(sequence<object> arg);
  void passSequenceOfNullableObject(sequence<object?> arg);
  void passOptionalNullableSequenceOfNullableSequenceOfObject(optional sequence<sequence<object>?>? arg);
  void passOptionalNullableSequenceOfNullableSequenceOfNullableObject(optional sequence<sequence<object?>?>? arg);
  object receiveObject();
  object? receiveNullableObject();

  // Union types
  void passUnion((object or long) arg);
  void passUnionWithNullable((object? or long) arg);
  void passNullableUnion((object or long)? arg);
  void passOptionalUnion(optional (object or long) arg);
  void passOptionalNullableUnion(optional (object or long)? arg);
  void passOptionalNullableUnionWithDefaultValue(optional (object or long)? arg = null);
  //void passUnionWithInterfaces((TestInterface or TestExternalInterface) arg);
  //void passUnionWithInterfacesAndNullable((TestInterface? or TestExternalInterface) arg);
  //void passUnionWithSequence((sequence<object> or long) arg);
  void passUnionWithArrayBuffer((ArrayBuffer or long) arg);
  void passUnionWithString((DOMString or object) arg);
  //void passUnionWithEnum((TestEnum or object) arg);
  // Trying to use a callback in a union won't include the test
  // headers, unfortunately, so won't compile.
  //void passUnionWithCallback((TestCallback or long) arg);
  void passUnionWithObject((object or long) arg);
  //void passUnionWithDict((Dict or long) arg);

  // Date types
  void passDate(Date arg);
  void passNullableDate(Date? arg);
  void passOptionalDate(optional Date arg);
  void passOptionalNullableDate(optional Date? arg);
  void passOptionalNullableDateWithDefaultValue(optional Date? arg = null);
  void passDateSequence(sequence<Date> arg);
  void passNullableDateSequence(sequence<Date?> arg);
  Date receiveDate();
  Date? receiveNullableDate();

  // binaryNames tests
  void methodRenamedFrom();
  void methodRenamedFrom(byte argument);
  readonly attribute byte attributeGetterRenamedFrom;
  attribute byte attributeRenamedFrom;

  void passDictionary(optional Dict x);
  Dict receiveDictionary();
  Dict? receiveNullableDictionary();
  void passOtherDictionary(optional GrandparentDict x);
  void passSequenceOfDictionaries(sequence<Dict> x);
  // No support for nullable dictionaries inside a sequence (nor should there be)
  //  void passSequenceOfNullableDictionaries(sequence<Dict?> x);
  void passDictionaryOrLong(optional Dict x);
  void passDictionaryOrLong(long x);

  void passDictContainingDict(optional DictContainingDict arg);
  void passDictContainingSequence(optional DictContainingSequence arg);
  DictContainingSequence receiveDictContainingSequence();

  // EnforceRange/Clamp tests
  void dontEnforceRangeOrClamp(byte arg);
  void doEnforceRange([EnforceRange] byte arg);
  void doClamp([Clamp] byte arg);

  // Typedefs
  const myLong myLongConstant = 5;
  void exerciseTypedefInterfaces1(AnotherNameForTestInterface arg);
  AnotherNameForTestInterface exerciseTypedefInterfaces2(NullableTestInterface arg);
  void exerciseTypedefInterfaces3(YetAnotherNameForTestInterface arg);

  // Static methods and attributes
  static attribute boolean staticAttribute;
  static void staticMethod(boolean arg);
  static void staticMethodWithContext(any arg);

  // Overload resolution tests
  //void overload1(DOMString... strs);
  boolean overload1(TestInterface arg);
  TestInterface overload1(DOMString strs, TestInterface arg);
  void overload2(TestInterface arg);
  void overload2(optional Dict arg);
  void overload2(DOMString arg);
  void overload2(Date arg);
  void overload3(TestInterface arg);
  void overload3(TestCallback arg);
  void overload3(DOMString arg);
  void overload4(TestInterface arg);
  void overload4(TestCallbackInterface arg);
  void overload4(DOMString arg);

  // Variadic handling
  void passVariadicThirdArg(DOMString arg1, long arg2, TestInterface... arg3);

  // Conditionally exposed methods/attributes
  [Pref="abc.def"]
  readonly attribute boolean prefable1;
  [Pref="abc.def"]
  readonly attribute boolean prefable2;
  [Pref="ghi.jkl"]
  readonly attribute boolean prefable3;
  [Pref="ghi.jkl"]
  readonly attribute boolean prefable4;
  [Pref="abc.def"]
  readonly attribute boolean prefable5;
  [Pref="abc.def", Func="nsGenericHTMLElement::TouchEventsEnabled"]
  readonly attribute boolean prefable6;
  [Pref="abc.def", Func="nsGenericHTMLElement::TouchEventsEnabled"]
  readonly attribute boolean prefable7;
  [Pref="ghi.jkl", Func="nsGenericHTMLElement::TouchEventsEnabled"]
  readonly attribute boolean prefable8;
  [Pref="abc.def", Func="nsGenericHTMLElement::TouchEventsEnabled"]
  readonly attribute boolean prefable9;
  [Pref="abc.def"]
  void prefable10();
  [Pref="abc.def", Func="nsGenericHTMLElement::TouchEventsEnabled"]
  void prefable11();
  [Pref="abc.def", Func="TestFuncControlledMember"]
  readonly attribute boolean prefable12;
  [Pref="abc.def", Func="nsGenericHTMLElement::TouchEventsEnabled"]
  void prefable13();
  [Pref="abc.def", Func="TestFuncControlledMember"]
  readonly attribute boolean prefable14;
  [Func="TestFuncControlledMember"]
  readonly attribute boolean prefable15;
  [Func="TestFuncControlledMember"]
  readonly attribute boolean prefable16;
  [Pref="abc.def", Func="TestFuncControlledMember"]
  void prefable17();
  [Func="TestFuncControlledMember"]
  void prefable18();
  [Func="TestFuncControlledMember"]
  void prefable19();

  // Miscellania
  [LenientThis] attribute long attrWithLenientThis;
  [Unforgeable] readonly attribute long unforgeableAttr;
  [Unforgeable, ChromeOnly] readonly attribute long unforgeableAttr2;
  stringifier;
  void passRenamedInterface(TestRenamedInterface arg);
  [PutForwards=writableByte] readonly attribute TestInterface putForwardsAttr;
  [PutForwards=writableByte, LenientThis] readonly attribute TestInterface putForwardsAttr2;
  [PutForwards=writableByte, ChromeOnly] readonly attribute TestInterface putForwardsAttr3;
  [Throws] void throwingMethod();
  [Throws] attribute boolean throwingAttr;
  [GetterThrows] attribute boolean throwingGetterAttr;
  [SetterThrows] attribute boolean throwingSetterAttr;
  legacycaller short(unsigned long arg1, TestInterface arg2);
  void passArgsWithDefaults(optional long arg1,
                            optional TestInterface? arg2 = null,
                            optional Dict arg3, optional double arg4 = 5.0,
                            optional float arg5);

  // If you add things here, add them to TestExampleGen and TestJSImplGen as well
};

interface TestParentInterface {
};

interface TestChildInterface : TestParentInterface {
};

interface TestNonWrapperCacheInterface {
};

[NoInterfaceObject]
interface ImplementedInterfaceParent {
  void implementedParentMethod();
  attribute boolean implementedParentProperty;

  const long implementedParentConstant = 8;
};

ImplementedInterfaceParent implements IndirectlyImplementedInterface;

[NoInterfaceObject]
interface IndirectlyImplementedInterface {
  void indirectlyImplementedMethod();
  attribute boolean indirectlyImplementedProperty;

  const long indirectlyImplementedConstant = 9;
};

[NoInterfaceObject]
interface ImplementedInterface : ImplementedInterfaceParent {
  void implementedMethod();
  attribute boolean implementedProperty;

  const long implementedConstant = 5;
};

[NoInterfaceObject]
interface DiamondImplements {
  readonly attribute long diamondImplementedProperty;
};
[NoInterfaceObject]
interface DiamondBranch1A {
};
[NoInterfaceObject]
interface DiamondBranch1B {
};
[NoInterfaceObject]
interface DiamondBranch2A : DiamondImplements {
};
[NoInterfaceObject]
interface DiamondBranch2B : DiamondImplements {
};
TestInterface implements DiamondBranch1A;
TestInterface implements DiamondBranch1B;
TestInterface implements DiamondBranch2A;
TestInterface implements DiamondBranch2B;
DiamondBranch1A implements DiamondImplements;
DiamondBranch1B implements DiamondImplements;

dictionary Dict : ParentDict {
  TestEnum someEnum;
  long x;
  long a;
  long b = 8;
  long z = 9;
  DOMString str;
  DOMString empty = "";
  TestEnum otherEnum = "b";
  DOMString otherStr = "def";
  DOMString? yetAnotherStr = null;
  DOMString template;
  object someObj;
  boolean prototype;
  object? anotherObj = null;
  TestCallback? someCallback = null;
  any someAny;
  any anotherAny = null;

  unrestricted float  urFloat = 0;
  unrestricted float  urFloat2 = 1.1;
  unrestricted float  urFloat3 = -1.1;
  unrestricted float? urFloat4 = null;
  unrestricted float  infUrFloat = Infinity;
  unrestricted float  negativeInfUrFloat = -Infinity;
  unrestricted float  nanUrFloat = NaN;

  unrestricted double  urDouble = 0;
  unrestricted double  urDouble2 = 1.1;
  unrestricted double  urDouble3 = -1.1;
  unrestricted double? urDouble4 = null;
  unrestricted double  infUrDouble = Infinity;
  unrestricted double  negativeInfUrDouble = -Infinity;
  unrestricted double  nanUrDouble = NaN;

};

dictionary ParentDict : GrandparentDict {
  long c = 5;
  TestInterface someInterface;
  TestInterface? someNullableInterface = null;
  TestExternalInterface someExternalInterface;
  any parentAny;
};

dictionary DictContainingDict {
  Dict memberDict;
};

dictionary DictContainingSequence {
  sequence<long> ourSequence;
  sequence<TestInterface> ourSequence2;
  sequence<any> ourSequence3;
  sequence<object> ourSequence4;
  sequence<object?> ourSequence5;
  sequence<object>? ourSequence6;
  sequence<object?>? ourSequence7;
  sequence<object>? ourSequence8 = null;
  sequence<object?>? ourSequence9 = null;
};

dictionary DictForConstructor {
  Dict dict;
  DictContainingDict dict2;
  sequence<Dict> seq1;
  sequence<sequence<Dict>>? seq2;
  sequence<sequence<Dict>?> seq3;
  sequence<any> seq4;
  sequence<any> seq5;
  sequence<DictContainingSequence> seq6;
  object obj1;
  object? obj2;
  any any1 = null;
};

interface TestIndexedGetterInterface {
  getter long item(unsigned long idx);
  readonly attribute unsigned long length;
};

interface TestNamedGetterInterface {
  getter DOMString (DOMString name);
};

interface TestIndexedGetterAndSetterAndNamedGetterInterface {
  getter DOMString (DOMString myName);
  getter long (unsigned long index);
  setter creator void (unsigned long index, long arg);
};

interface TestIndexedAndNamedGetterInterface {
  getter long (unsigned long index);
  getter DOMString namedItem(DOMString name);
  readonly attribute unsigned long length;
};

interface TestIndexedSetterInterface {
  setter creator void setItem(unsigned long idx, DOMString item);
  getter DOMString (unsigned long idx);
};

interface TestNamedSetterInterface {
  setter creator void (DOMString myName, TestIndexedSetterInterface item);
  getter TestIndexedSetterInterface (DOMString name);
};

interface TestIndexedAndNamedSetterInterface {
  setter creator void (unsigned long index, TestIndexedSetterInterface item);
  getter TestIndexedSetterInterface (unsigned long index);
  setter creator void setNamedItem(DOMString name, TestIndexedSetterInterface item);
  getter TestIndexedSetterInterface (DOMString name);
};

interface TestIndexedAndNamedGetterAndSetterInterface : TestIndexedSetterInterface {
  getter long item(unsigned long index);
  getter DOMString namedItem(DOMString name);
  setter creator void (unsigned long index, long item);
  setter creator void (DOMString name, DOMString item);
  stringifier DOMString ();
  readonly attribute unsigned long length;
};

interface TestIndexedDeleterInterface {
  deleter void delItem(unsigned long idx);
  getter long (unsigned long index);
};

interface TestIndexedDeleterWithRetvalInterface {
  deleter boolean delItem(unsigned long index);
  getter long (unsigned long index);
};

interface TestNamedDeleterInterface {
  deleter void (DOMString name);
  getter long (DOMString name);
};

interface TestNamedDeleterWithRetvalInterface {
  deleter boolean delNamedItem(DOMString name);
  getter long (DOMString name);
};

interface TestIndexedAndNamedDeleterInterface {
  deleter void (unsigned long index);
  getter long (unsigned long index);
  deleter void delNamedItem(DOMString name);
  getter long (DOMString name);
};

interface TestCppKeywordNamedMethodsInterface {
  boolean continue();
  boolean delete();
  long volatile();
};

