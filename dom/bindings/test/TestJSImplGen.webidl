/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

typedef TestJSImplInterface AnotherNameForTestJSImplInterface;
typedef TestJSImplInterface YetAnotherNameForTestJSImplInterface;
typedef TestJSImplInterface? NullableTestJSImplInterface;

callback MyTestCallback = void();

TestInterface implements ImplementedInterface;

enum MyTestEnum {
  "a",
  "b"
};

// We don't support multiple constructors (bug 869268) or named constructors
// for JS-implemented WebIDL.
[Constructor(DOMString str, unsigned long num, boolean? boolArg,
             TestInterface? iface, long arg1,
             DictForConstructor dict, any any1,
             object obj1,
             object? obj2, sequence<Dict> seq, optional any any2 = null,
             optional object obj3,
             optional object? obj4),
 JSImplementation="@mozilla.org/test-js-impl-interface;1"]
interface TestJSImplInterface {
  // Integer types
  // XXXbz add tests for throwing versions of all the integer stuff
  readonly attribute byte readonlyByte;
  attribute byte writableByte;
  void passByte(byte arg);
  byte receiveByte();
  void passOptionalByte(optional byte arg);
  void passOptionalByteBeforeRequired(optional byte arg1, byte arg2);
  void passOptionalByteWithDefault(optional byte arg = 0);
  void passOptionalByteWithDefaultBeforeRequired(optional byte arg1 = 0, byte arg2);
  void passNullableByte(byte? arg);
  void passOptionalNullableByte(optional byte? arg);
  void passVariadicByte(byte... arg);
  [Cached, Pure]
  readonly attribute byte cachedByte;
  [Cached, Constant]
  readonly attribute byte cachedConstantByte;
  [Cached, Pure]
  attribute byte cachedWritableByte;

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
  TestJSImplInterface receiveSelf();
  TestJSImplInterface? receiveNullableSelf();

  // Callback interface ignores 'resultNotAddRefed'. See bug 843272.
  //TestJSImplInterface receiveWeakSelf();
  //TestJSImplInterface? receiveWeakNullableSelf();

  // A version to test for casting to TestJSImplInterface&
  void passSelf(TestJSImplInterface arg);
  void passNullableSelf(TestJSImplInterface? arg);
  attribute TestJSImplInterface nonNullSelf;
  attribute TestJSImplInterface? nullableSelf;
  [Cached, Pure]
  readonly attribute TestJSImplInterface cachedSelf;
  // Optional arguments
  void passOptionalSelf(optional TestJSImplInterface? arg);
  void passOptionalNonNullSelf(optional TestJSImplInterface arg);
  void passOptionalSelfWithDefault(optional TestJSImplInterface? arg = null);

  // Non-wrapper-cache interface types
  [NewObject]
  TestNonWrapperCacheInterface receiveNonWrapperCacheInterface();
  [NewObject]
  TestNonWrapperCacheInterface? receiveNullableNonWrapperCacheInterface();

  [NewObject]
  sequence<TestNonWrapperCacheInterface> receiveNonWrapperCacheInterfaceSequence();
  [NewObject]
  sequence<TestNonWrapperCacheInterface?> receiveNullableNonWrapperCacheInterfaceSequence();
  [NewObject]
  sequence<TestNonWrapperCacheInterface>? receiveNonWrapperCacheInterfaceNullableSequence();
  [NewObject]
  sequence<TestNonWrapperCacheInterface?>? receiveNullableNonWrapperCacheInterfaceNullableSequence();

  // Non-castable interface types
  IndirectlyImplementedInterface receiveOther();
  IndirectlyImplementedInterface? receiveNullableOther();
  // Callback interface ignores 'resultNotAddRefed'. See bug 843272.
  //IndirectlyImplementedInterface receiveWeakOther();
  //IndirectlyImplementedInterface? receiveWeakNullableOther();

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
  // Callback interface ignores 'resultNotAddRefed'. See bug 843272.
  //TestExternalInterface receiveWeakExternal();
  //TestExternalInterface? receiveWeakNullableExternal();
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
  // Callback interface ignores 'resultNotAddRefed'. See bug 843272.
  //TestCallbackInterface receiveWeakCallbackInterface();
  //TestCallbackInterface? receiveWeakNullableCallbackInterface();
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
  [Cached, Pure]
  readonly attribute sequence<long> readonlySequence;
  [Cached, Pure]
  readonly attribute sequence<Dict> readonlySequenceOfDictionaries;
  [Cached, Pure]
  readonly attribute sequence<Dict>? readonlyNullableSequenceOfDictionaries;
  [Cached, Pure, Frozen]
  readonly attribute sequence<long> readonlyFrozenSequence;
  [Cached, Pure, Frozen]
  readonly attribute sequence<long>? readonlyFrozenNullableSequence;
  sequence<long> receiveSequence();
  sequence<long>? receiveNullableSequence();
  sequence<long?> receiveSequenceOfNullableInts();
  sequence<long?>? receiveNullableSequenceOfNullableInts();
  void passSequence(sequence<long> arg);
  void passNullableSequence(sequence<long>? arg);
  void passSequenceOfNullableInts(sequence<long?> arg);
  void passOptionalSequenceOfNullableInts(optional sequence<long?> arg);
  void passOptionalNullableSequenceOfNullableInts(optional sequence<long?>? arg);
  sequence<TestJSImplInterface> receiveCastableObjectSequence();
  sequence<TestCallbackInterface> receiveCallbackObjectSequence();
  sequence<TestJSImplInterface?> receiveNullableCastableObjectSequence();
  sequence<TestCallbackInterface?> receiveNullableCallbackObjectSequence();
  sequence<TestJSImplInterface>? receiveCastableObjectNullableSequence();
  sequence<TestJSImplInterface?>? receiveNullableCastableObjectNullableSequence();
  sequence<TestJSImplInterface> receiveWeakCastableObjectSequence();
  sequence<TestJSImplInterface?> receiveWeakNullableCastableObjectSequence();
  sequence<TestJSImplInterface>? receiveWeakCastableObjectNullableSequence();
  sequence<TestJSImplInterface?>? receiveWeakNullableCastableObjectNullableSequence();
  void passCastableObjectSequence(sequence<TestJSImplInterface> arg);
  void passNullableCastableObjectSequence(sequence<TestJSImplInterface?> arg);
  void passCastableObjectNullableSequence(sequence<TestJSImplInterface>? arg);
  void passNullableCastableObjectNullableSequence(sequence<TestJSImplInterface?>? arg);
  void passOptionalSequence(optional sequence<long> arg);
  void passOptionalNullableSequence(optional sequence<long>? arg);
  void passOptionalNullableSequenceWithDefaultValue(optional sequence<long>? arg = null);
  void passOptionalObjectSequence(optional sequence<TestJSImplInterface> arg);
  void passExternalInterfaceSequence(sequence<TestExternalInterface> arg);
  void passNullableExternalInterfaceSequence(sequence<TestExternalInterface?> arg);

  sequence<DOMString> receiveStringSequence();
  sequence<ByteString> receiveByteStringSequence();
  // Callback interface problem.  See bug 843261.
  //void passStringSequence(sequence<DOMString> arg);
  sequence<any> receiveAnySequence();
  sequence<any>? receiveNullableAnySequence();
  //XXXbz No support for sequence of sequence return values yet.
  //sequence<sequence<any>> receiveAnySequenceSequence();

  sequence<object> receiveObjectSequence();
  sequence<object?> receiveNullableObjectSequence();

  void passSequenceOfSequences(sequence<sequence<long>> arg);
  //sequence<sequence<long>> receiveSequenceOfSequences();

  // ArrayBuffer is handled differently in callback interfaces and the example generator.
  // Need to figure out what should be done there.  Seems like other typed array stuff is
  // similarly not working in the JS implemented generator.  Probably some other issues
  // here as well.
  // Typed array types
  //void passArrayBuffer(ArrayBuffer arg);
  //void passNullableArrayBuffer(ArrayBuffer? arg);
  //void passOptionalArrayBuffer(optional ArrayBuffer arg);
  //void passOptionalNullableArrayBuffer(optional ArrayBuffer? arg);
  //void passOptionalNullableArrayBufferWithDefaultValue(optional ArrayBuffer? arg= null);
  //void passArrayBufferView(ArrayBufferView arg);
  //void passInt8Array(Int8Array arg);
  //void passInt16Array(Int16Array arg);
  //void passInt32Array(Int32Array arg);
  //void passUint8Array(Uint8Array arg);
  //void passUint16Array(Uint16Array arg);
  //void passUint32Array(Uint32Array arg);
  //void passUint8ClampedArray(Uint8ClampedArray arg);
  //void passFloat32Array(Float32Array arg);
  //void passFloat64Array(Float64Array arg);
  //void passSequenceOfArrayBuffers(sequence<ArrayBuffer> arg);
  //void passSequenceOfNullableArrayBuffers(sequence<ArrayBuffer?> arg);
  //void passVariadicTypedArray(Float32Array... arg);
  //void passVariadicNullableTypedArray(Float32Array?... arg);
  //Uint8Array receiveUint8Array();

  // DOMString types
  void passString(DOMString arg);
  void passNullableString(DOMString? arg);
  void passOptionalString(optional DOMString arg);
  void passOptionalStringWithDefaultValue(optional DOMString arg = "abc");
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
  void passEnum(MyTestEnum arg);
  void passNullableEnum(MyTestEnum? arg);
  void passOptionalEnum(optional MyTestEnum arg);
  void passEnumWithDefault(optional MyTestEnum arg = "a");
  void passOptionalNullableEnum(optional MyTestEnum? arg);
  void passOptionalNullableEnumWithDefaultValue(optional MyTestEnum? arg = null);
  void passOptionalNullableEnumWithDefaultValue2(optional MyTestEnum? arg = "a");
  MyTestEnum receiveEnum();
  MyTestEnum? receiveNullableEnum();
  attribute MyTestEnum enumAttribute;
  readonly attribute MyTestEnum readonlyEnumAttribute;

  // Callback types
  void passCallback(MyTestCallback arg);
  void passNullableCallback(MyTestCallback? arg);
  void passOptionalCallback(optional MyTestCallback arg);
  void passOptionalNullableCallback(optional MyTestCallback? arg);
  void passOptionalNullableCallbackWithDefaultValue(optional MyTestCallback? arg = null);
  MyTestCallback receiveCallback();
  MyTestCallback? receiveNullableCallback();
  // Hmm. These two don't work, I think because I need a locally modified version of TestTreatAsNullCallback.
  //void passNullableTreatAsNullCallback(TestTreatAsNullCallback? arg);
  //void passOptionalNullableTreatAsNullCallback(optional TestTreatAsNullCallback? arg);
  void passOptionalNullableTreatAsNullCallbackWithDefaultValue(optional TestTreatAsNullCallback? arg = null);

  // Any types
  void passAny(any arg);
  void passVariadicAny(any... arg);
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

  void passObject(object arg);
  void passVariadicObject(object... arg);
  void passNullableObject(object? arg);
  void passVariadicNullableObject(object... arg);
  void passOptionalObject(optional object arg);
  void passOptionalNullableObject(optional object? arg);
  void passOptionalNullableObjectWithDefaultValue(optional object? arg = null);
  void passSequenceOfObject(sequence<object> arg);
  void passSequenceOfNullableObject(sequence<object?> arg);
  void passNullableSequenceOfObject(sequence<object>? arg);
  void passOptionalNullableSequenceOfNullableSequenceOfObject(optional sequence<sequence<object>?>? arg);
  void passOptionalNullableSequenceOfNullableSequenceOfNullableObject(optional sequence<sequence<object?>?>? arg);
  object receiveObject();
  object? receiveNullableObject();

  // Union types
  void passUnion((object or long) arg);
  // Some union tests are debug-only to avoid creating all those
  // unused union types in opt builds.
#ifdef DEBUG
  void passUnion2((long or boolean) arg);
  void passUnion3((object or long or boolean) arg);
  void passUnion4((Node or long or boolean) arg);
  void passUnion5((object or boolean) arg);
  void passUnion6((object or DOMString) arg);
  void passUnion7((object or DOMString or long) arg);
  void passUnion8((object or DOMString or boolean) arg);
  void passUnion9((object or DOMString or long or boolean) arg);
  void passUnion10(optional (EventInit or long) arg);
  void passUnion11(optional (CustomEventInit or long) arg);
  void passUnion12(optional (EventInit or long) arg = 5);
  void passUnion13(optional (object or long?) arg = null);
  void passUnion14(optional (object or long?) arg = 5);
  void passUnionWithCallback((EventHandler or long) arg);
#endif
  void passUnionWithNullable((object? or long) arg);
  void passNullableUnion((object or long)? arg);
  void passOptionalUnion(optional (object or long) arg);
  void passOptionalNullableUnion(optional (object or long)? arg);
  void passOptionalNullableUnionWithDefaultValue(optional (object or long)? arg = null);
  //void passUnionWithInterfaces((TestJSImplInterface or TestExternalInterface) arg);
  //void passUnionWithInterfacesAndNullable((TestJSImplInterface? or TestExternalInterface) arg);
  //void passUnionWithSequence((sequence<object> or long) arg);
  void passUnionWithArrayBuffer((ArrayBuffer or long) arg);
  void passUnionWithString((DOMString or object) arg);
  //void passUnionWithEnum((MyTestEnum or object) arg);
  // Trying to use a callback in a union won't include the test
  // headers, unfortunately, so won't compile.
  //  void passUnionWithCallback((MyTestCallback or long) arg);
  void passUnionWithObject((object or long) arg);
  //void passUnionWithDict((Dict or long) arg);

  void passUnionWithDefaultValue1(optional (double or DOMString) arg = "");
  void passUnionWithDefaultValue2(optional (double or DOMString) arg = 1);
  void passUnionWithDefaultValue3(optional (double or DOMString) arg = 1.5);
  void passUnionWithDefaultValue4(optional (float or DOMString) arg = "");
  void passUnionWithDefaultValue5(optional (float or DOMString) arg = 1);
  void passUnionWithDefaultValue6(optional (float or DOMString) arg = 1.5);
  void passUnionWithDefaultValue7(optional (unrestricted double or DOMString) arg = "");
  void passUnionWithDefaultValue8(optional (unrestricted double or DOMString) arg = 1);
  void passUnionWithDefaultValue9(optional (unrestricted double or DOMString) arg = 1.5);
  void passUnionWithDefaultValue10(optional (unrestricted double or DOMString) arg = Infinity);
  void passUnionWithDefaultValue11(optional (unrestricted float or DOMString) arg = "");
  void passUnionWithDefaultValue12(optional (unrestricted float or DOMString) arg = 1);
  void passUnionWithDefaultValue13(optional (unrestricted float or DOMString) arg = Infinity);

  void passNullableUnionWithDefaultValue1(optional (double or DOMString)? arg = "");
  void passNullableUnionWithDefaultValue2(optional (double or DOMString)? arg = 1);
  void passNullableUnionWithDefaultValue3(optional (double or DOMString)? arg = null);
  void passNullableUnionWithDefaultValue4(optional (float or DOMString)? arg = "");
  void passNullableUnionWithDefaultValue5(optional (float or DOMString)? arg = 1);
  void passNullableUnionWithDefaultValue6(optional (float or DOMString)? arg = null);
  void passNullableUnionWithDefaultValue7(optional (unrestricted double or DOMString)? arg = "");
  void passNullableUnionWithDefaultValue8(optional (unrestricted double or DOMString)? arg = 1);
  void passNullableUnionWithDefaultValue9(optional (unrestricted double or DOMString)? arg = null);
  void passNullableUnionWithDefaultValue10(optional (unrestricted float or DOMString)? arg = "");
  void passNullableUnionWithDefaultValue11(optional (unrestricted float or DOMString)? arg = 1);
  void passNullableUnionWithDefaultValue12(optional (unrestricted float or DOMString)? arg = null);

  void passSequenceOfUnions(sequence<(CanvasPattern or CanvasGradient)> arg);
  void passSequenceOfUnions2(sequence<(object or long)> arg);
  void passVariadicUnion((CanvasPattern or CanvasGradient)... arg);

  void passSequenceOfNullableUnions(sequence<(CanvasPattern or CanvasGradient)?> arg);
  void passVariadicNullableUnion((CanvasPattern or CanvasGradient)?... arg);

  //(CanvasPattern or CanvasGradient) receiveUnion();
  //(object or long) receiveUnion2();
  //(CanvasPattern? or CanvasGradient) receiveUnionContainingNull();
  //(CanvasPattern or CanvasGradient)? receiveNullableUnion();
  //(object or long)? receiveNullableUnion2();

  //attribute (CanvasPattern or CanvasGradient) writableUnion;
  //attribute (CanvasPattern? or CanvasGradient) writableUnionContainingNull;
  //attribute (CanvasPattern or CanvasGradient)? writableNullableUnion;

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
  [Cached, Pure]
  readonly attribute Dict readonlyDictionary;
  [Cached, Pure]
  readonly attribute Dict? readonlyNullableDictionary;
  [Cached, Pure]
  attribute Dict writableDictionary;
  [Cached, Pure, Frozen]
  readonly attribute Dict readonlyFrozenDictionary;
  [Cached, Pure, Frozen]
  readonly attribute Dict? readonlyFrozenNullableDictionary;
  [Cached, Pure, Frozen]
  attribute Dict writableFrozenDictionary;
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
  [EnforceRange] attribute byte enforcedByte;
  [Clamp] attribute byte clampedByte;

  // Typedefs
  const myLong myLongConstant = 5;
  void exerciseTypedefInterfaces1(AnotherNameForTestJSImplInterface arg);
  AnotherNameForTestJSImplInterface exerciseTypedefInterfaces2(NullableTestJSImplInterface arg);
  void exerciseTypedefInterfaces3(YetAnotherNameForTestJSImplInterface arg);

  // Static methods and attributes
  // FIXME: Bug 863952 Static things are not supported yet
  /*
  static attribute boolean staticAttribute;
  static void staticMethod(boolean arg);
  static void staticMethodWithContext(any arg);
  */

  // Overload resolution tests
  //void overload1(DOMString... strs);
  boolean overload1(TestJSImplInterface arg);
  TestJSImplInterface overload1(DOMString strs, TestJSImplInterface arg);
  void overload2(TestJSImplInterface arg);
  void overload2(optional Dict arg);
  void overload2(boolean arg);
  void overload2(DOMString arg);
  void overload2(Date arg);
  void overload3(TestJSImplInterface arg);
  void overload3(MyTestCallback arg);
  void overload3(boolean arg);
  void overload4(TestJSImplInterface arg);
  void overload4(TestCallbackInterface arg);
  void overload4(DOMString arg);
  void overload5(long arg);
  void overload5(MyTestEnum arg);
  void overload6(long arg);
  void overload6(boolean arg);
  void overload7(long arg);
  void overload7(boolean arg);
  void overload7(ByteString arg);
  void overload8(long arg);
  void overload8(TestJSImplInterface arg);
  void overload9(long? arg);
  void overload9(DOMString arg);
  void overload10(long? arg);
  void overload10(object arg);
  void overload11(long arg);
  void overload11(DOMString? arg);
  void overload12(long arg);
  void overload12(boolean? arg);
  void overload13(long? arg);
  void overload13(boolean arg);
  void overload14(optional long arg);
  void overload14(TestInterface arg);
  void overload15(long arg);
  void overload15(optional TestInterface arg);
  void overload16(long arg);
  void overload16(optional TestInterface? arg);

  // Variadic handling
  void passVariadicThirdArg(DOMString arg1, long arg2, TestJSImplInterface... arg3);

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
  [Pref="abc.def", Func="TestFuncControlledMember", ChromeOnly]
  void prefable20();
  [Func="TestFuncControlledMember", AvailableIn=CertifiedApps]
  void prefable21();
  [Func="TestFuncControlledMember", AvailableIn=CertifiedApps]
  void prefable22();
  [Pref="abc.def", Func="TestFuncControlledMember", AvailableIn=CertifiedApps]
  void prefable23();
  [Pref="abc.def", Func="TestFuncControlledMember", AvailableIn=PrivilegedApps]
  void prefable24();

  // Miscellania
  [LenientThis] attribute long attrWithLenientThis;
  // FIXME: Bug 863954 Unforgeable things get all confused when
  // non-JS-implemented interfaces inherit from JS-implemented ones or vice
  // versa.
  //   [Unforgeable] readonly attribute long unforgeableAttr;
  //   [Unforgeable, ChromeOnly] readonly attribute long unforgeableAttr2;
  // FIXME: Bug 863955 No stringifiers yet
  //   stringifier;
  void passRenamedInterface(TestRenamedInterface arg);
  [PutForwards=writableByte] readonly attribute TestJSImplInterface putForwardsAttr;
  [PutForwards=writableByte, LenientThis] readonly attribute TestJSImplInterface putForwardsAttr2;
  [PutForwards=writableByte, ChromeOnly] readonly attribute TestJSImplInterface putForwardsAttr3;
  [Throws] void throwingMethod();
  [Throws] attribute boolean throwingAttr;
  [GetterThrows] attribute boolean throwingGetterAttr;
  [SetterThrows] attribute boolean throwingSetterAttr;
  // legacycaller short(unsigned long arg1, TestInterface arg2);
  void passArgsWithDefaults(optional long arg1,
                            optional TestInterface? arg2 = null,
                            optional Dict arg3, optional double arg4 = 5.0,
                            optional float arg5);
  attribute any jsonifierShouldSkipThis;
  attribute TestParentInterface jsonifierShouldSkipThis2;
  attribute TestCallbackInterface jsonifierShouldSkipThis3;
  jsonifier;

  // If you add things here, add them to TestCodeGen as well
};

[NavigatorProperty="TestNavigator", JSImplementation="@mozilla.org/test;1"]
interface TestNavigator {
};

[Constructor, NavigatorProperty="TestNavigatorWithConstructor", JSImplementation="@mozilla.org/test;1"]
interface TestNavigatorWithConstructor {
};

interface TestCImplementedInterface : TestJSImplInterface {
};

interface TestCImplementedInterface2 {
};
