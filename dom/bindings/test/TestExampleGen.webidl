/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */
[Constructor,
 Constructor(DOMString str),
 Constructor(unsigned long num, boolean? boolArg),
 Constructor(TestInterface? iface),
 Constructor(long arg1, IndirectlyImplementedInterface iface),
 // Constructor(long arg1, long arg2, (TestInterface or OnlyForUseInConstructor) arg3),
 NamedConstructor=Example,
 NamedConstructor=Example(DOMString str)
 ]
interface TestExampleInterface {
  // Integer types
  // XXXbz add tests for throwing versions of all the integer stuff
  readonly attribute byte readonlyByte;
  attribute byte writableByte;
  void passByte(byte arg);
  byte receiveByte();
  void passOptionalByte(optional byte arg);
  void passOptionalByteWithDefault(optional byte arg = 0);
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

  // Castable interface types
  // XXXbz add tests for throwing versions of all the castable interface stuff
  TestInterface receiveSelf();
  TestInterface? receiveNullableSelf();
  TestInterface receiveWeakSelf();
  TestInterface? receiveWeakNullableSelf();
  // A verstion to test for casting to TestInterface&
  void passSelf(TestInterface arg);
  // A version we can use to test for the exact type passed in
  void passSelf2(TestInterface arg);
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
  // A verstion to test for casting to IndirectlyImplementedInterface&
  void passOther(IndirectlyImplementedInterface arg);
  // A version we can use to test for the exact type passed in
  void passOther2(IndirectlyImplementedInterface arg);
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
  // A verstion to test for casting to TestExternalInterface&
  void passExternal(TestExternalInterface arg);
  // A version we can use to test for the exact type passed in
  void passExternal2(TestExternalInterface arg);
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
  // A verstion to test for casting to TestCallbackInterface&
  void passCallbackInterface(TestCallbackInterface arg);
  // A version we can use to test for the exact type passed in
  void passCallbackInterface2(TestCallbackInterface arg);
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

  sequence<any> receiveAnySequence();
  sequence<any>? receiveNullableAnySequence();

  void passSequenceOfSequences(sequence<sequence<long>> arg);

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

  // String types
  void passString(DOMString arg);
  void passNullableString(DOMString? arg);
  void passOptionalString(optional DOMString arg);
  void passOptionalStringWithDefaultValue(optional DOMString arg = "abc");
  void passOptionalNullableString(optional DOMString? arg);
  void passOptionalNullableStringWithDefaultValue(optional DOMString? arg = null);
  void passVariadicString(DOMString... arg);

  // Enumerated types
  void passEnum(TestEnum arg);
  // No support for nullable enums yet
  // void passNullableEnum(TestEnum? arg);
  void passOptionalEnum(optional TestEnum arg);
  void passEnumWithDefault(optional TestEnum arg = "a");
  // void passOptionalNullableEnum(optional TestEnum? arg);
  // void passOptionalNullableEnumWithDefaultValue(optional TestEnum? arg = null);
  TestEnum receiveEnum();
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

  // Any types
  void passAny(any arg);
  void passOptionalAny(optional any arg);
  void passAnyDefaultNull(optional any arg = null);
  any receiveAny();

  // object types
  void passObject(object arg);
  void passNullableObject(object? arg);
  void passOptionalObject(optional object arg);
  void passOptionalNullableObject(optional object? arg);
  void passOptionalNullableObjectWithDefaultValue(optional object? arg = null);
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
  //  void passUnionWithCallback((TestCallback or long) arg);
  void passUnionWithObject((object or long) arg);
  //void passUnionWithDict((Dict or long) arg);

  // binaryNames tests
  void methodRenamedFrom();
  void methodRenamedFrom(byte argument);
  readonly attribute byte attributeGetterRenamedFrom;
  attribute byte attributeRenamedFrom;

  void passDictionary(optional Dict x);
  //UNSUPPORTED  Dict receiveDictionary();
  void passOtherDictionary(optional GrandparentDict x);
  void passSequenceOfDictionaries(sequence<Dict> x);
  void passDictionaryOrLong(optional Dict x);
  void passDictionaryOrLong(long x);

  void passDictContainingDict(optional DictContainingDict arg);
  void passDictContainingSequence(optional DictContainingSequence arg);
  //UNSUPPORTED DictContainingSequence receiveDictContainingSequence();

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
  [PutForwards=writableByte] readonly attribute TestExampleInterface putForwardsAttr;
  [PutForwards=writableByte, LenientThis] readonly attribute TestExampleInterface putForwardsAttr2;
  [PutForwards=writableByte, ChromeOnly] readonly attribute TestExampleInterface putForwardsAttr3;
  [Throws] void throwingMethod();
  [Throws] attribute boolean throwingAttr;
  [GetterThrows] attribute boolean throwingGetterAttr;
  [SetterThrows] attribute boolean throwingSetterAttr;
  legacycaller short(unsigned long arg1, TestInterface arg2);

  // If you add things here, add them to TestCodeGen and TestJSImplGen as well
};

interface TestExampleProxyInterface {
  getter long longIndexedGetter(unsigned long ix);
  deleter void (unsigned long ix);
  setter creator void longIndexedSetter(unsigned long y, long z);
  stringifier DOMString myStringifier();
  getter short shortNameGetter(DOMString nom);
  deleter void (DOMString nomnom);
  setter creator void shortNamedSetter(DOMString me, short value);
};
