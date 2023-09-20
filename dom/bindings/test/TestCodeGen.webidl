/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

typedef long myLong;
typedef TestInterface AnotherNameForTestInterface;
typedef TestInterface? NullableTestInterface;
typedef CustomEventInit TestDictionaryTypedef;
typedef ArrayBufferView ArrayBufferViewTypedef;
typedef [AllowShared] ArrayBufferView AllowSharedArrayBufferViewTypedef;

interface TestExternalInterface;

// We need a pref name that's in StaticPrefList.h here.
[Pref="dom.webidl.test1",
 Exposed=Window]
interface TestRenamedInterface {
};

[Exposed=Window]
callback interface TestCallbackInterface {
  readonly attribute long foo;
  attribute DOMString bar;
  undefined doSomething();
  long doSomethingElse(DOMString arg, TestInterface otherArg);
  undefined doSequenceLongArg(sequence<long> arg);
  undefined doSequenceStringArg(sequence<DOMString> arg);
  undefined doRecordLongArg(record<DOMString, long> arg);
  sequence<long> getSequenceOfLong();
  sequence<TestInterface> getSequenceOfInterfaces();
  sequence<TestInterface>? getNullableSequenceOfInterfaces();
  sequence<TestInterface?> getSequenceOfNullableInterfaces();
  sequence<TestInterface?>? getNullableSequenceOfNullableInterfaces();
  sequence<TestCallbackInterface> getSequenceOfCallbackInterfaces();
  sequence<TestCallbackInterface>? getNullableSequenceOfCallbackInterfaces();
  sequence<TestCallbackInterface?> getSequenceOfNullableCallbackInterfaces();
  sequence<TestCallbackInterface?>? getNullableSequenceOfNullableCallbackInterfaces();
  record<DOMString, long> getRecordOfLong();
  Dict? getDictionary();
  undefined passArrayBuffer(ArrayBuffer arg);
  undefined passNullableArrayBuffer(ArrayBuffer? arg);
  undefined passOptionalArrayBuffer(optional ArrayBuffer arg);
  undefined passOptionalNullableArrayBuffer(optional ArrayBuffer? arg);
  undefined passOptionalNullableArrayBufferWithDefaultValue(optional ArrayBuffer? arg= null);
  undefined passArrayBufferView(ArrayBufferView arg);
  undefined passInt8Array(Int8Array arg);
  undefined passInt16Array(Int16Array arg);
  undefined passInt32Array(Int32Array arg);
  undefined passUint8Array(Uint8Array arg);
  undefined passUint16Array(Uint16Array arg);
  undefined passUint32Array(Uint32Array arg);
  undefined passUint8ClampedArray(Uint8ClampedArray arg);
  undefined passFloat32Array(Float32Array arg);
  undefined passFloat64Array(Float64Array arg);
  undefined passSequenceOfArrayBuffers(sequence<ArrayBuffer> arg);
  undefined passSequenceOfNullableArrayBuffers(sequence<ArrayBuffer?> arg);
  undefined passVariadicTypedArray(Float32Array... arg);
  undefined passVariadicNullableTypedArray(Float32Array?... arg);
  Uint8Array receiveUint8Array();
  attribute Uint8Array uint8ArrayAttr;
  Promise<undefined> receivePromise();
};

[Exposed=Window]
callback interface TestSingleOperationCallbackInterface {
  TestInterface doSomething(short arg, sequence<double> anotherArg);
};

enum TestEnum {
  "1",
  "a",
  "b",
  "1-2",
  "2d-array"
};

callback TestCallback = undefined();
[TreatNonCallableAsNull] callback TestTreatAsNullCallback = undefined();

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
callback TestInterfaceArguments = undefined(TestInterface arg1,
                                            TestInterface? arg2,
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
callback TestStringEnumArguments = undefined(DOMString myString, DOMString? nullString,
                                        TestEnum myEnum);
callback TestObjectArguments = undefined(object anObj, object? anotherObj,
                                    ArrayBuffer buf, ArrayBuffer? buf2);
callback TestOptionalArguments = undefined(optional DOMString aString,
                                           optional object something,
                                           optional sequence<TestInterface> aSeq,
                                           optional TestInterface? anInterface,
                                           optional TestInterface anotherInterface,
                                           optional long aLong);
// Callback constructor return value tests
callback constructor TestUndefinedConstruction = undefined(TestDictionaryTypedef arg);
callback constructor TestIntegerConstruction = unsigned long();
callback constructor TestBooleanConstruction = boolean(any arg1,
                                                       optional any arg2);
callback constructor TestFloatConstruction = unrestricted float(optional object arg1,
                                                                optional TestDictionaryTypedef arg2);
callback constructor TestStringConstruction = DOMString(long? arg);
callback constructor TestEnumConstruction = TestEnum(any... arg);
callback constructor TestInterfaceConstruction = TestInterface();
callback constructor TestExternalInterfaceConstruction = TestExternalInterface();
callback constructor TestCallbackInterfaceConstruction = TestCallbackInterface();
callback constructor TestCallbackConstruction = TestCallback();
callback constructor TestObjectConstruction = object();
callback constructor TestTypedArrayConstruction = ArrayBuffer();
callback constructor TestSequenceConstruction = sequence<boolean>();
// If you add a new test callback, add it to the forceCallbackGeneration
// method on TestInterface so it actually gets tested.

TestInterface includes InterfaceMixin;

// This interface is only for use in the constructor below
[Exposed=Window]
interface OnlyForUseInConstructor {
};

// This enum is only for use in inner unions below
enum OnlyForUseInInnerUnion {
  "1",
};

[LegacyFactoryFunction=Test,
 LegacyFactoryFunction=Test(DOMString str),
 LegacyFactoryFunction=Test2(DictForConstructor dict, any any1, object obj1,
                        object? obj2, sequence<Dict> seq, optional any any2,
                        optional object obj3, optional object? obj4),
 LegacyFactoryFunction=Test3((long or record<DOMString, any>) arg1),
 LegacyFactoryFunction=Test4(record<DOMString, record<DOMString, any>> arg1),
 LegacyFactoryFunction=Test5(record<DOMString, sequence<record<DOMString, record<DOMString, sequence<sequence<any>>>>>> arg1),
 LegacyFactoryFunction=Test6(sequence<record<ByteString, sequence<sequence<record<ByteString, record<USVString, any>>>>>> arg1),
 Exposed=Window]
interface TestInterface {
  constructor();
  constructor(DOMString str);
  constructor(unsigned long num, boolean? boolArg);
  constructor(TestInterface? iface);
  constructor(unsigned long arg1, TestInterface iface);
  constructor(ArrayBuffer arrayBuf);
  constructor(Uint8Array typedArr);
  // constructor(long arg1, long arg2, (TestInterface or OnlyForUseInConstructor) arg3);

  // Integer types
  // XXXbz add tests for throwing versions of all the integer stuff
  readonly attribute byte readonlyByte;
  attribute byte writableByte;
  undefined passByte(byte arg);
  byte receiveByte();
  undefined passOptionalByte(optional byte arg);
  undefined passOptionalByteBeforeRequired(optional byte arg1, byte arg2);
  undefined passOptionalByteWithDefault(optional byte arg = 0);
  undefined passOptionalByteWithDefaultBeforeRequired(optional byte arg1 = 0, byte arg2);
  undefined passNullableByte(byte? arg);
  undefined passOptionalNullableByte(optional byte? arg);
  undefined passVariadicByte(byte... arg);
  [StoreInSlot, Pure]
  readonly attribute byte cachedByte;
  [StoreInSlot, Constant]
  readonly attribute byte cachedConstantByte;
  [StoreInSlot, Pure]
  attribute byte cachedWritableByte;
  [Affects=Nothing]
  attribute byte sideEffectFreeByte;
  [Affects=Nothing, DependsOn=DOMState]
  attribute byte domDependentByte;
  [Affects=Nothing, DependsOn=Nothing]
  readonly attribute byte constantByte;
  [DependsOn=DeviceState, Affects=Nothing]
  readonly attribute byte deviceStateDependentByte;
  [Affects=Nothing]
  byte returnByteSideEffectFree();
  [Affects=Nothing, DependsOn=DOMState]
  byte returnDOMDependentByte();
  [Affects=Nothing, DependsOn=Nothing]
  byte returnConstantByte();
  [DependsOn=DeviceState, Affects=Nothing]
  byte returnDeviceStateDependentByte();

  readonly attribute short readonlyShort;
  attribute short writableShort;
  undefined passShort(short arg);
  short receiveShort();
  undefined passOptionalShort(optional short arg);
  undefined passOptionalShortWithDefault(optional short arg = 5);

  readonly attribute long readonlyLong;
  attribute long writableLong;
  undefined passLong(long arg);
  long receiveLong();
  undefined passOptionalLong(optional long arg);
  undefined passOptionalLongWithDefault(optional long arg = 7);

  readonly attribute long long readonlyLongLong;
  attribute long long writableLongLong;
  undefined passLongLong(long long arg);
  long long receiveLongLong();
  undefined passOptionalLongLong(optional long long arg);
  undefined passOptionalLongLongWithDefault(optional long long arg = -12);

  readonly attribute octet readonlyOctet;
  attribute octet writableOctet;
  undefined passOctet(octet arg);
  octet receiveOctet();
  undefined passOptionalOctet(optional octet arg);
  undefined passOptionalOctetWithDefault(optional octet arg = 19);

  readonly attribute unsigned short readonlyUnsignedShort;
  attribute unsigned short writableUnsignedShort;
  undefined passUnsignedShort(unsigned short arg);
  unsigned short receiveUnsignedShort();
  undefined passOptionalUnsignedShort(optional unsigned short arg);
  undefined passOptionalUnsignedShortWithDefault(optional unsigned short arg = 2);

  readonly attribute unsigned long readonlyUnsignedLong;
  attribute unsigned long writableUnsignedLong;
  undefined passUnsignedLong(unsigned long arg);
  unsigned long receiveUnsignedLong();
  undefined passOptionalUnsignedLong(optional unsigned long arg);
  undefined passOptionalUnsignedLongWithDefault(optional unsigned long arg = 6);

  readonly attribute unsigned long long readonlyUnsignedLongLong;
  attribute unsigned long long  writableUnsignedLongLong;
  undefined passUnsignedLongLong(unsigned long long arg);
  unsigned long long receiveUnsignedLongLong();
  undefined passOptionalUnsignedLongLong(optional unsigned long long arg);
  undefined passOptionalUnsignedLongLongWithDefault(optional unsigned long long arg = 17);

  attribute float writableFloat;
  attribute unrestricted float writableUnrestrictedFloat;
  attribute float? writableNullableFloat;
  attribute unrestricted float? writableNullableUnrestrictedFloat;
  attribute double writableDouble;
  attribute unrestricted double writableUnrestrictedDouble;
  attribute double? writableNullableDouble;
  attribute unrestricted double? writableNullableUnrestrictedDouble;
  undefined passFloat(float arg1, unrestricted float arg2,
                      float? arg3, unrestricted float? arg4,
                      double arg5, unrestricted double arg6,
                      double? arg7, unrestricted double? arg8,
                      sequence<float> arg9, sequence<unrestricted float> arg10,
                      sequence<float?> arg11, sequence<unrestricted float?> arg12,
                      sequence<double> arg13, sequence<unrestricted double> arg14,
                      sequence<double?> arg15, sequence<unrestricted double?> arg16);
  [LenientFloat]
  undefined passLenientFloat(float arg1, unrestricted float arg2,
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

  undefined passUnrestricted(optional unrestricted float arg1 = 0,
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
  undefined passSelf(TestInterface arg);
  undefined passNullableSelf(TestInterface? arg);
  attribute TestInterface nonNullSelf;
  attribute TestInterface? nullableSelf;
  [Cached, Pure]
  readonly attribute TestInterface cachedSelf;
  // Optional arguments
  undefined passOptionalSelf(optional TestInterface? arg);
  undefined passOptionalNonNullSelf(optional TestInterface arg);
  undefined passOptionalSelfWithDefault(optional TestInterface? arg = null);

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

  // External interface types
  TestExternalInterface receiveExternal();
  TestExternalInterface? receiveNullableExternal();
  TestExternalInterface receiveWeakExternal();
  TestExternalInterface? receiveWeakNullableExternal();
  undefined passExternal(TestExternalInterface arg);
  undefined passNullableExternal(TestExternalInterface? arg);
  attribute TestExternalInterface nonNullExternal;
  attribute TestExternalInterface? nullableExternal;
  // Optional arguments
  undefined passOptionalExternal(optional TestExternalInterface? arg);
  undefined passOptionalNonNullExternal(optional TestExternalInterface arg);
  undefined passOptionalExternalWithDefault(optional TestExternalInterface? arg = null);

  // Callback interface types
  TestCallbackInterface receiveCallbackInterface();
  TestCallbackInterface? receiveNullableCallbackInterface();
  TestCallbackInterface receiveWeakCallbackInterface();
  TestCallbackInterface? receiveWeakNullableCallbackInterface();
  undefined passCallbackInterface(TestCallbackInterface arg);
  undefined passNullableCallbackInterface(TestCallbackInterface? arg);
  attribute TestCallbackInterface nonNullCallbackInterface;
  attribute TestCallbackInterface? nullableCallbackInterface;
  // Optional arguments
  undefined passOptionalCallbackInterface(optional TestCallbackInterface? arg);
  undefined passOptionalNonNullCallbackInterface(optional TestCallbackInterface arg);
  undefined passOptionalCallbackInterfaceWithDefault(optional TestCallbackInterface? arg = null);

  // Sequence types
  [Cached, Pure]
  readonly attribute sequence<long> readonlySequence;
  [Cached, Pure]
  readonly attribute sequence<Dict> readonlySequenceOfDictionaries;
  [Cached, Pure]
  readonly attribute sequence<Dict>? readonlyNullableSequenceOfDictionaries;
  [Cached, Pure, Frozen]
  readonly attribute sequence<Dict> readonlyFrozenSequence;
  [Cached, Pure, Frozen]
  readonly attribute sequence<Dict>? readonlyFrozenNullableSequence;
  sequence<long> receiveSequence();
  sequence<long>? receiveNullableSequence();
  sequence<long?> receiveSequenceOfNullableInts();
  sequence<long?>? receiveNullableSequenceOfNullableInts();
  undefined passSequence(sequence<long> arg);
  undefined passNullableSequence(sequence<long>? arg);
  undefined passSequenceOfNullableInts(sequence<long?> arg);
  undefined passOptionalSequenceOfNullableInts(optional sequence<long?> arg);
  undefined passOptionalNullableSequenceOfNullableInts(optional sequence<long?>? arg);
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
  undefined passCastableObjectSequence(sequence<TestInterface> arg);
  undefined passNullableCastableObjectSequence(sequence<TestInterface?> arg);
  undefined passCastableObjectNullableSequence(sequence<TestInterface>? arg);
  undefined passNullableCastableObjectNullableSequence(sequence<TestInterface?>? arg);
  undefined passOptionalSequence(optional sequence<long> arg);
  undefined passOptionalSequenceWithDefaultValue(optional sequence<long> arg = []);
  undefined passOptionalNullableSequence(optional sequence<long>? arg);
  undefined passOptionalNullableSequenceWithDefaultValue(optional sequence<long>? arg = null);
  undefined passOptionalNullableSequenceWithDefaultValue2(optional sequence<long>? arg = []);
  undefined passOptionalObjectSequence(optional sequence<TestInterface> arg);
  undefined passExternalInterfaceSequence(sequence<TestExternalInterface> arg);
  undefined passNullableExternalInterfaceSequence(sequence<TestExternalInterface?> arg);

  sequence<DOMString> receiveStringSequence();
  undefined passStringSequence(sequence<DOMString> arg);

  sequence<ByteString> receiveByteStringSequence();
  undefined passByteStringSequence(sequence<ByteString> arg);

  sequence<UTF8String> receiveUTF8StringSequence();
  undefined passUTF8StringSequence(sequence<UTF8String> arg);

  sequence<any> receiveAnySequence();
  sequence<any>? receiveNullableAnySequence();
  sequence<sequence<any>> receiveAnySequenceSequence();

  sequence<object> receiveObjectSequence();
  sequence<object?> receiveNullableObjectSequence();

  undefined passSequenceOfSequences(sequence<sequence<long>> arg);
  undefined passSequenceOfSequencesOfSequences(sequence<sequence<sequence<long>>> arg);
  sequence<sequence<long>> receiveSequenceOfSequences();
  sequence<sequence<sequence<long>>> receiveSequenceOfSequencesOfSequences();

  // record types
  undefined passRecord(record<DOMString, long> arg);
  undefined passNullableRecord(record<DOMString, long>? arg);
  undefined passRecordOfNullableInts(record<DOMString, long?> arg);
  undefined passOptionalRecordOfNullableInts(optional record<DOMString, long?> arg);
  undefined passOptionalNullableRecordOfNullableInts(optional record<DOMString, long?>? arg);
  undefined passCastableObjectRecord(record<DOMString, TestInterface> arg);
  undefined passNullableCastableObjectRecord(record<DOMString, TestInterface?> arg);
  undefined passCastableObjectNullableRecord(record<DOMString, TestInterface>? arg);
  undefined passNullableCastableObjectNullableRecord(record<DOMString, TestInterface?>? arg);
  undefined passOptionalRecord(optional record<DOMString, long> arg);
  undefined passOptionalNullableRecord(optional record<DOMString, long>? arg);
  undefined passOptionalNullableRecordWithDefaultValue(optional record<DOMString, long>? arg = null);
  undefined passOptionalObjectRecord(optional record<DOMString, TestInterface> arg);
  undefined passExternalInterfaceRecord(record<DOMString, TestExternalInterface> arg);
  undefined passNullableExternalInterfaceRecord(record<DOMString, TestExternalInterface?> arg);
  undefined passStringRecord(record<DOMString, DOMString> arg);
  undefined passByteStringRecord(record<DOMString, ByteString> arg);
  undefined passUTF8StringRecord(record<DOMString, UTF8String> arg);
  undefined passRecordOfRecords(record<DOMString, record<DOMString, long>> arg);
  record<DOMString, long> receiveRecord();
  record<DOMString, long>? receiveNullableRecord();
  record<DOMString, long?> receiveRecordOfNullableInts();
  record<DOMString, long?>? receiveNullableRecordOfNullableInts();
  record<DOMString, record<DOMString, long>> receiveRecordOfRecords();
  record<DOMString, any> receiveAnyRecord();

  // Typed array types
  undefined passArrayBuffer(ArrayBuffer arg);
  undefined passNullableArrayBuffer(ArrayBuffer? arg);
  undefined passOptionalArrayBuffer(optional ArrayBuffer arg);
  undefined passOptionalNullableArrayBuffer(optional ArrayBuffer? arg);
  undefined passOptionalNullableArrayBufferWithDefaultValue(optional ArrayBuffer? arg= null);
  undefined passArrayBufferView(ArrayBufferView arg);
  undefined passInt8Array(Int8Array arg);
  undefined passInt16Array(Int16Array arg);
  undefined passInt32Array(Int32Array arg);
  undefined passUint8Array(Uint8Array arg);
  undefined passUint16Array(Uint16Array arg);
  undefined passUint32Array(Uint32Array arg);
  undefined passUint8ClampedArray(Uint8ClampedArray arg);
  undefined passFloat32Array(Float32Array arg);
  undefined passFloat64Array(Float64Array arg);
  undefined passSequenceOfArrayBuffers(sequence<ArrayBuffer> arg);
  undefined passSequenceOfNullableArrayBuffers(sequence<ArrayBuffer?> arg);
  undefined passRecordOfArrayBuffers(record<DOMString, ArrayBuffer> arg);
  undefined passRecordOfNullableArrayBuffers(record<DOMString, ArrayBuffer?> arg);
  undefined passVariadicTypedArray(Float32Array... arg);
  undefined passVariadicNullableTypedArray(Float32Array?... arg);
  Uint8Array receiveUint8Array();
  attribute Uint8Array uint8ArrayAttr;

  // DOMString types
  undefined passString(DOMString arg);
  undefined passNullableString(DOMString? arg);
  undefined passOptionalString(optional DOMString arg);
  undefined passOptionalStringWithDefaultValue(optional DOMString arg = "abc");
  undefined passOptionalNullableString(optional DOMString? arg);
  undefined passOptionalNullableStringWithDefaultValue(optional DOMString? arg = null);
  undefined passVariadicString(DOMString... arg);
  DOMString receiveString();

  // ByteString types
  undefined passByteString(ByteString arg);
  undefined passNullableByteString(ByteString? arg);
  undefined passOptionalByteString(optional ByteString arg);
  undefined passOptionalByteStringWithDefaultValue(optional ByteString arg = "abc");
  undefined passOptionalNullableByteString(optional ByteString? arg);
  undefined passOptionalNullableByteStringWithDefaultValue(optional ByteString? arg = null);
  undefined passVariadicByteString(ByteString... arg);
  undefined passOptionalUnionByteString(optional (ByteString or long) arg);
  undefined passOptionalUnionByteStringWithDefaultValue(optional (ByteString or long) arg = "abc");

  // UTF8String types
  undefined passUTF8String(UTF8String arg);
  undefined passNullableUTF8String(UTF8String? arg);
  undefined passOptionalUTF8String(optional UTF8String arg);
  undefined passOptionalUTF8StringWithDefaultValue(optional UTF8String arg = "abc");
  undefined passOptionalNullableUTF8String(optional UTF8String? arg);
  undefined passOptionalNullableUTF8StringWithDefaultValue(optional UTF8String? arg = null);
  undefined passVariadicUTF8String(UTF8String... arg);
  undefined passOptionalUnionUTF8String(optional (UTF8String or long) arg);
  undefined passOptionalUnionUTF8StringWithDefaultValue(optional (UTF8String or long) arg = "abc");

  // USVString types
  undefined passUSVS(USVString arg);
  undefined passNullableUSVS(USVString? arg);
  undefined passOptionalUSVS(optional USVString arg);
  undefined passOptionalUSVSWithDefaultValue(optional USVString arg = "abc");
  undefined passOptionalNullableUSVS(optional USVString? arg);
  undefined passOptionalNullableUSVSWithDefaultValue(optional USVString? arg = null);
  undefined passVariadicUSVS(USVString... arg);
  USVString receiveUSVS();

  // JSString types
  undefined passJSString(JSString arg);
  // undefined passNullableJSString(JSString? arg); // NOT SUPPORTED YET
  // undefined passOptionalJSString(optional JSString arg); // NOT SUPPORTED YET
  undefined passOptionalJSStringWithDefaultValue(optional JSString arg = "abc");
  // undefined passOptionalNullableJSString(optional JSString? arg); // NOT SUPPORTED YET
  // undefined passOptionalNullableJSStringWithDefaultValue(optional JSString? arg = null); // NOT SUPPORTED YET
  // undefined passVariadicJSString(JSString... arg); // NOT SUPPORTED YET
  // undefined passRecordOfJSString(record<DOMString, JSString> arg); // NOT SUPPORTED YET
  // undefined passSequenceOfJSString(sequence<JSString> arg); // NOT SUPPORTED YET
  // undefined passUnionJSString((JSString or long) arg); // NOT SUPPORTED YET
  JSString receiveJSString();
  // sequence<JSString> receiveJSStringSequence(); // NOT SUPPORTED YET
  // (JSString or long) receiveJSStringUnion(); // NOT SUPPORTED YET
  // record<DOMString, JSString> receiveJSStringRecord(); // NOT SUPPORTED YET
  readonly attribute JSString readonlyJSStringAttr;
  attribute JSString jsStringAttr;

  // Enumerated types
  undefined passEnum(TestEnum arg);
  undefined passNullableEnum(TestEnum? arg);
  undefined passOptionalEnum(optional TestEnum arg);
  undefined passEnumWithDefault(optional TestEnum arg = "a");
  undefined passOptionalNullableEnum(optional TestEnum? arg);
  undefined passOptionalNullableEnumWithDefaultValue(optional TestEnum? arg = null);
  undefined passOptionalNullableEnumWithDefaultValue2(optional TestEnum? arg = "a");
  TestEnum receiveEnum();
  TestEnum? receiveNullableEnum();
  attribute TestEnum enumAttribute;
  readonly attribute TestEnum readonlyEnumAttribute;

  // Callback types
  undefined passCallback(TestCallback arg);
  undefined passNullableCallback(TestCallback? arg);
  undefined passOptionalCallback(optional TestCallback arg);
  undefined passOptionalNullableCallback(optional TestCallback? arg);
  undefined passOptionalNullableCallbackWithDefaultValue(optional TestCallback? arg = null);
  TestCallback receiveCallback();
  TestCallback? receiveNullableCallback();
  undefined passNullableTreatAsNullCallback(TestTreatAsNullCallback? arg);
  undefined passOptionalNullableTreatAsNullCallback(optional TestTreatAsNullCallback? arg);
  undefined passOptionalNullableTreatAsNullCallbackWithDefaultValue(optional TestTreatAsNullCallback? arg = null);
  attribute TestTreatAsNullCallback treatAsNullCallback;
  attribute TestTreatAsNullCallback? nullableTreatAsNullCallback;

  // Force code generation of the various test callbacks we have.
  undefined forceCallbackGeneration(TestIntegerReturn arg1,
                                    TestNullableIntegerReturn arg2,
                                    TestBooleanReturn arg3,
                                    TestFloatReturn arg4,
                                    TestStringReturn arg5,
                                    TestEnumReturn arg6,
                                    TestInterfaceReturn arg7,
                                    TestNullableInterfaceReturn arg8,
                                    TestExternalInterfaceReturn arg9,
                                    TestNullableExternalInterfaceReturn arg10,
                                    TestCallbackInterfaceReturn arg11,
                                    TestNullableCallbackInterfaceReturn arg12,
                                    TestCallbackReturn arg13,
                                    TestNullableCallbackReturn arg14,
                                    TestObjectReturn arg15,
                                    TestNullableObjectReturn arg16,
                                    TestTypedArrayReturn arg17,
                                    TestNullableTypedArrayReturn arg18,
                                    TestSequenceReturn arg19,
                                    TestNullableSequenceReturn arg20,
                                    TestIntegerArguments arg21,
                                    TestInterfaceArguments arg22,
                                    TestStringEnumArguments arg23,
                                    TestObjectArguments arg24,
                                    TestOptionalArguments arg25,
                                    TestUnionArguments arg26,
                                    TestUndefinedConstruction arg27,
                                    TestIntegerConstruction arg28,
                                    TestBooleanConstruction arg29,
                                    TestFloatConstruction arg30,
                                    TestStringConstruction arg31,
                                    TestEnumConstruction arg32,
                                    TestInterfaceConstruction arg33,
                                    TestExternalInterfaceConstruction arg34,
                                    TestCallbackInterfaceConstruction arg35,
                                    TestCallbackConstruction arg36,
                                    TestObjectConstruction arg37,
                                    TestTypedArrayConstruction arg38,
                                    TestSequenceConstruction arg39);

  // Any types
  undefined passAny(any arg);
  undefined passVariadicAny(any... arg);
  undefined passOptionalAny(optional any arg);
  undefined passAnyDefaultNull(optional any arg = null);
  undefined passSequenceOfAny(sequence<any> arg);
  undefined passNullableSequenceOfAny(sequence<any>? arg);
  undefined passOptionalSequenceOfAny(optional sequence<any> arg);
  undefined passOptionalNullableSequenceOfAny(optional sequence<any>? arg);
  undefined passOptionalSequenceOfAnyWithDefaultValue(optional sequence<any>? arg = null);
  undefined passSequenceOfSequenceOfAny(sequence<sequence<any>> arg);
  undefined passSequenceOfNullableSequenceOfAny(sequence<sequence<any>?> arg);
  undefined passNullableSequenceOfNullableSequenceOfAny(sequence<sequence<any>?>? arg);
  undefined passOptionalNullableSequenceOfNullableSequenceOfAny(optional sequence<sequence<any>?>? arg);
  undefined passRecordOfAny(record<DOMString, any> arg);
  undefined passNullableRecordOfAny(record<DOMString, any>? arg);
  undefined passOptionalRecordOfAny(optional record<DOMString, any> arg);
  undefined passOptionalNullableRecordOfAny(optional record<DOMString, any>? arg);
  undefined passOptionalRecordOfAnyWithDefaultValue(optional record<DOMString, any>? arg = null);
  undefined passRecordOfRecordOfAny(record<DOMString, record<DOMString, any>> arg);
  undefined passRecordOfNullableRecordOfAny(record<DOMString, record<DOMString, any>?> arg);
  undefined passNullableRecordOfNullableRecordOfAny(record<DOMString, record<DOMString, any>?>? arg);
  undefined passOptionalNullableRecordOfNullableRecordOfAny(optional record<DOMString, record<DOMString, any>?>? arg);
  undefined passOptionalNullableRecordOfNullableSequenceOfAny(optional record<DOMString, sequence<any>?>? arg);
  undefined passOptionalNullableSequenceOfNullableRecordOfAny(optional sequence<record<DOMString, any>?>? arg);
  any receiveAny();

  // object types
  undefined passObject(object arg);
  undefined passVariadicObject(object... arg);
  undefined passNullableObject(object? arg);
  undefined passVariadicNullableObject(object... arg);
  undefined passOptionalObject(optional object arg);
  undefined passOptionalNullableObject(optional object? arg);
  undefined passOptionalNullableObjectWithDefaultValue(optional object? arg = null);
  undefined passSequenceOfObject(sequence<object> arg);
  undefined passSequenceOfNullableObject(sequence<object?> arg);
  undefined passNullableSequenceOfObject(sequence<object>? arg);
  undefined passOptionalNullableSequenceOfNullableSequenceOfObject(optional sequence<sequence<object>?>? arg);
  undefined passOptionalNullableSequenceOfNullableSequenceOfNullableObject(optional sequence<sequence<object?>?>? arg);
  undefined passRecordOfObject(record<DOMString, object> arg);
  object receiveObject();
  object? receiveNullableObject();

  // Union types
  undefined passUnion((object or long) arg);
  // Some  union tests are debug-only to avoid creating all those
  // unused union types in opt builds.
#ifdef DEBUG
  undefined passUnion2((long or boolean) arg);
  undefined passUnion3((object or long or boolean) arg);
  undefined passUnion4((Node or long or boolean) arg);
  undefined passUnion5((object or boolean) arg);
  undefined passUnion6((object or DOMString) arg);
  undefined passUnion7((object or DOMString or long) arg);
  undefined passUnion8((object or DOMString or boolean) arg);
  undefined passUnion9((object or DOMString or long or boolean) arg);
  undefined passUnion10(optional (EventInit or long) arg = {});
  undefined passUnion11(optional (CustomEventInit or long) arg = {});
  undefined passUnion12(optional (EventInit or long) arg = 5);
  undefined passUnion13(optional (object or long?) arg = null);
  undefined passUnion14(optional (object or long?) arg = 5);
  undefined passUnion15((sequence<long> or long) arg);
  undefined passUnion16(optional (sequence<long> or long) arg);
  undefined passUnion17(optional (sequence<long>? or long) arg = 5);
  undefined passUnion18((sequence<object> or long) arg);
  undefined passUnion19(optional (sequence<object> or long) arg);
  undefined passUnion20(optional (sequence<object> or long) arg = []);
  undefined passUnion21((record<DOMString, long> or long) arg);
  undefined passUnion22((record<DOMString, object> or long) arg);
  undefined passUnion23((sequence<ImageData> or long) arg);
  undefined passUnion24((sequence<ImageData?> or long) arg);
  undefined passUnion25((sequence<sequence<ImageData>> or long) arg);
  undefined passUnion26((sequence<sequence<ImageData?>> or long) arg);
  undefined passUnion27(optional (sequence<DOMString> or EventInit) arg = {});
  undefined passUnion28(optional (EventInit or sequence<DOMString>) arg = {});
  undefined passUnionWithCallback((EventHandler or long) arg);
  undefined passUnionWithByteString((ByteString or long) arg);
  undefined passUnionWithUTF8String((UTF8String or long) arg);
  undefined passUnionWithRecord((record<DOMString, DOMString> or DOMString) arg);
  undefined passUnionWithRecordAndSequence((record<DOMString, DOMString> or sequence<DOMString>) arg);
  undefined passUnionWithSequenceAndRecord((sequence<DOMString> or record<DOMString, DOMString>) arg);
  undefined passUnionWithUSVS((USVString or long) arg);
#endif
  undefined passUnionWithNullable((object? or long) arg);
  undefined passNullableUnion((object or long)? arg);
  undefined passOptionalUnion(optional (object or long) arg);
  undefined passOptionalNullableUnion(optional (object or long)? arg);
  undefined passOptionalNullableUnionWithDefaultValue(optional (object or long)? arg = null);
  //undefined passUnionWithInterfaces((TestInterface or TestExternalInterface) arg);
  //undefined passUnionWithInterfacesAndNullable((TestInterface? or TestExternalInterface) arg);
  //undefined passUnionWithSequence((sequence<object> or long) arg);
  undefined passUnionWithArrayBuffer((UTF8String or ArrayBuffer) arg);
  undefined passUnionWithArrayBufferOrNull((UTF8String or ArrayBuffer?) arg);
  undefined passUnionWithTypedArrays((ArrayBufferView or ArrayBuffer) arg);
  undefined passUnionWithTypedArraysOrNull((ArrayBufferView or ArrayBuffer?) arg);
  undefined passUnionWithString((DOMString or object) arg);
  // Using an enum in a union.  Note that we use some enum not declared in our
  // binding file, because UnionTypes.h will need to include the binding header
  // for this enum.  Pick an enum from an interface that won't drag in too much
  // stuff.
  undefined passUnionWithEnum((SupportedType or object) arg);

  // Trying to use a callback in a union won't include the test
  // headers, unfortunately, so won't compile.
  //undefined passUnionWithCallback((TestCallback or long) arg);
  undefined passUnionWithObject((object or long) arg);
  //undefined passUnionWithDict((Dict or long) arg);

  undefined passUnionWithDefaultValue1(optional (double or DOMString) arg = "");
  undefined passUnionWithDefaultValue2(optional (double or DOMString) arg = 1);
  undefined passUnionWithDefaultValue3(optional (double or DOMString) arg = 1.5);
  undefined passUnionWithDefaultValue4(optional (float or DOMString) arg = "");
  undefined passUnionWithDefaultValue5(optional (float or DOMString) arg = 1);
  undefined passUnionWithDefaultValue6(optional (float or DOMString) arg = 1.5);
  undefined passUnionWithDefaultValue7(optional (unrestricted double or DOMString) arg = "");
  undefined passUnionWithDefaultValue8(optional (unrestricted double or DOMString) arg = 1);
  undefined passUnionWithDefaultValue9(optional (unrestricted double or DOMString) arg = 1.5);
  undefined passUnionWithDefaultValue10(optional (unrestricted double or DOMString) arg = Infinity);
  undefined passUnionWithDefaultValue11(optional (unrestricted float or DOMString) arg = "");
  undefined passUnionWithDefaultValue12(optional (unrestricted float or DOMString) arg = 1);
  undefined passUnionWithDefaultValue13(optional (unrestricted float or DOMString) arg = Infinity);
  undefined passUnionWithDefaultValue14(optional (double or ByteString) arg = "");
  undefined passUnionWithDefaultValue15(optional (double or ByteString) arg = 1);
  undefined passUnionWithDefaultValue16(optional (double or ByteString) arg = 1.5);
  undefined passUnionWithDefaultValue17(optional (double or SupportedType) arg = "text/html");
  undefined passUnionWithDefaultValue18(optional (double or SupportedType) arg = 1);
  undefined passUnionWithDefaultValue19(optional (double or SupportedType) arg = 1.5);
  undefined passUnionWithDefaultValue20(optional (double or USVString) arg = "abc");
  undefined passUnionWithDefaultValue21(optional (double or USVString) arg = 1);
  undefined passUnionWithDefaultValue22(optional (double or USVString) arg = 1.5);
  undefined passUnionWithDefaultValue23(optional (double or UTF8String) arg = "");
  undefined passUnionWithDefaultValue24(optional (double or UTF8String) arg = 1);
  undefined passUnionWithDefaultValue25(optional (double or UTF8String) arg = 1.5);

  undefined passNullableUnionWithDefaultValue1(optional (double or DOMString)? arg = "");
  undefined passNullableUnionWithDefaultValue2(optional (double or DOMString)? arg = 1);
  undefined passNullableUnionWithDefaultValue3(optional (double or DOMString)? arg = null);
  undefined passNullableUnionWithDefaultValue4(optional (float or DOMString)? arg = "");
  undefined passNullableUnionWithDefaultValue5(optional (float or DOMString)? arg = 1);
  undefined passNullableUnionWithDefaultValue6(optional (float or DOMString)? arg = null);
  undefined passNullableUnionWithDefaultValue7(optional (unrestricted double or DOMString)? arg = "");
  undefined passNullableUnionWithDefaultValue8(optional (unrestricted double or DOMString)? arg = 1);
  undefined passNullableUnionWithDefaultValue9(optional (unrestricted double or DOMString)? arg = null);
  undefined passNullableUnionWithDefaultValue10(optional (unrestricted float or DOMString)? arg = "");
  undefined passNullableUnionWithDefaultValue11(optional (unrestricted float or DOMString)? arg = 1);
  undefined passNullableUnionWithDefaultValue12(optional (unrestricted float or DOMString)? arg = null);
  undefined passNullableUnionWithDefaultValue13(optional (double or ByteString)? arg = "");
  undefined passNullableUnionWithDefaultValue14(optional (double or ByteString)? arg = 1);
  undefined passNullableUnionWithDefaultValue15(optional (double or ByteString)? arg = 1.5);
  undefined passNullableUnionWithDefaultValue16(optional (double or ByteString)? arg = null);
  undefined passNullableUnionWithDefaultValue17(optional (double or SupportedType)? arg = "text/html");
  undefined passNullableUnionWithDefaultValue18(optional (double or SupportedType)? arg = 1);
  undefined passNullableUnionWithDefaultValue19(optional (double or SupportedType)? arg = 1.5);
  undefined passNullableUnionWithDefaultValue20(optional (double or SupportedType)? arg = null);
  undefined passNullableUnionWithDefaultValue21(optional (double or USVString)? arg = "abc");
  undefined passNullableUnionWithDefaultValue22(optional (double or USVString)? arg = 1);
  undefined passNullableUnionWithDefaultValue23(optional (double or USVString)? arg = 1.5);
  undefined passNullableUnionWithDefaultValue24(optional (double or USVString)? arg = null);

  undefined passNullableUnionWithDefaultValue25(optional (double or UTF8String)? arg = "abc");
  undefined passNullableUnionWithDefaultValue26(optional (double or UTF8String)? arg = 1);
  undefined passNullableUnionWithDefaultValue27(optional (double or UTF8String)? arg = 1.5);
  undefined passNullableUnionWithDefaultValue28(optional (double or UTF8String)? arg = null);

  undefined passSequenceOfUnions(sequence<(CanvasPattern or CanvasGradient)> arg);
  undefined passSequenceOfUnions2(sequence<(object or long)> arg);
  undefined passVariadicUnion((CanvasPattern or CanvasGradient)... arg);

  undefined passSequenceOfNullableUnions(sequence<(CanvasPattern or CanvasGradient)?> arg);
  undefined passVariadicNullableUnion((CanvasPattern or CanvasGradient)?... arg);
  undefined passRecordOfUnions(record<DOMString, (CanvasPattern or CanvasGradient)> arg);

  // Each inner union in the following APIs should have a unique set
  // of union member types, not used in any other API.
  undefined passUnionWithSequenceOfUnions((DOMString or sequence<(OnlyForUseInInnerUnion or CanvasPattern)>) arg);
  //undefined passUnionWithFrozenArrayOfUnions((DOMString or FrozenArray<(OnlyForUseInInnerUnion or CanvasGradient)>) arg);
  undefined passUnionWithRecordOfUnions((sequence<long> or record<DOMString, (OnlyForUseInInnerUnion or sequence<long>)>) arg);

  // XXXbz no move constructor on some unions
  // undefined passRecordOfUnions2(record<DOMString, (object or long)> arg);

  (CanvasPattern or CanvasGradient) receiveUnion();
  (object or long) receiveUnion2();
  (CanvasPattern? or CanvasGradient) receiveUnionContainingNull();
  (CanvasPattern or CanvasGradient)? receiveNullableUnion();
  (object or long)? receiveNullableUnion2();
  (undefined or CanvasPattern) receiveUnionWithUndefined();
  (undefined? or CanvasPattern) receiveUnionWithNullableUndefined();
  (undefined or CanvasPattern?) receiveUnionWithUndefinedAndNullable();
  (undefined or CanvasPattern)? receiveNullableUnionWithUndefined();

  attribute (CanvasPattern or CanvasGradient) writableUnion;
  attribute (CanvasPattern? or CanvasGradient) writableUnionContainingNull;
  attribute (CanvasPattern or CanvasGradient)? writableNullableUnion;
  attribute (undefined or CanvasPattern) writableUnionWithUndefined;
  attribute (undefined? or CanvasPattern) writableUnionWithNullableUndefined;
  attribute (undefined or CanvasPattern?) writableUnionWithUndefinedAndNullable;
  attribute (undefined or CanvasPattern)? writableNullableUnionWithUndefined;

  // Promise types
  undefined passPromise(Promise<any> arg);
  undefined passOptionalPromise(optional Promise<any> arg);
  undefined passPromiseSequence(sequence<Promise<any>> arg);
  Promise<any> receivePromise();
  Promise<any> receiveAddrefedPromise();

  // ObservableArray types
  attribute ObservableArray<boolean> booleanObservableArray;
  attribute ObservableArray<object> objectObservableArray;
  attribute ObservableArray<any> anyObservableArray;
  attribute ObservableArray<TestInterface> interfaceObservableArray;
  attribute ObservableArray<long?> nullableObservableArray;

  // binaryNames tests
  [BinaryName="methodRenamedTo"]
  undefined methodRenamedFrom();
  [BinaryName="methodRenamedTo"]
  undefined methodRenamedFrom(byte argument);
  [BinaryName="attributeGetterRenamedTo"]
  readonly attribute byte attributeGetterRenamedFrom;
  [BinaryName="attributeRenamedTo"]
  attribute byte attributeRenamedFrom;

  undefined passDictionary(optional Dict x = {});
  undefined passDictionary2(Dict x);
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
  undefined passOtherDictionary(optional GrandparentDict x = {});
  undefined passSequenceOfDictionaries(sequence<Dict> x);
  undefined passRecordOfDictionaries(record<DOMString, GrandparentDict> x);
  // No support for nullable dictionaries inside a sequence (nor should there be)
  //  undefined passSequenceOfNullableDictionaries(sequence<Dict?> x);
  undefined passDictionaryOrLong(optional Dict x = {});
  undefined passDictionaryOrLong(long x);

  undefined passDictContainingDict(optional DictContainingDict arg = {});
  undefined passDictContainingSequence(optional DictContainingSequence arg = {});
  DictContainingSequence receiveDictContainingSequence();
  undefined passVariadicDictionary(Dict... arg);

  // EnforceRange/Clamp tests
  undefined dontEnforceRangeOrClamp(byte arg);
  undefined doEnforceRange([EnforceRange] byte arg);
  undefined doEnforceRangeNullable([EnforceRange] byte? arg);
  undefined doClamp([Clamp] byte arg);
  undefined doClampNullable([Clamp] byte? arg);
  attribute [EnforceRange] byte enforcedByte;
  attribute [EnforceRange] byte? enforcedNullableByte;
  attribute [Clamp] byte clampedByte;
  attribute [Clamp] byte? clampedNullableByte;

  // Typedefs
  const myLong myLongConstant = 5;
  undefined exerciseTypedefInterfaces1(AnotherNameForTestInterface arg);
  AnotherNameForTestInterface exerciseTypedefInterfaces2(NullableTestInterface arg);
  undefined exerciseTypedefInterfaces3(YetAnotherNameForTestInterface arg);

  // Deprecated methods and attributes
  [Deprecated="Components"]
  attribute byte deprecatedAttribute;
  [Deprecated="Components"]
  byte deprecatedMethod();
  [Deprecated="Components"]
  byte deprecatedMethodWithContext(any arg);

  // Static methods and attributes
  static attribute boolean staticAttribute;
  static undefined staticMethod(boolean arg);
  static undefined staticMethodWithContext(any arg);

  // Testing static method with a reserved C++ keyword as the name
  static undefined assert(boolean arg);

  // Deprecated static methods and attributes
  [Deprecated="Components"]
  static attribute byte staticDeprecatedAttribute;
  [Deprecated="Components"]
  static undefined staticDeprecatedMethod();
  [Deprecated="Components"]
  static undefined staticDeprecatedMethodWithContext(any arg);

  // Overload resolution tests
  //undefined overload1(DOMString... strs);
  boolean overload1(TestInterface arg);
  TestInterface overload1(DOMString strs, TestInterface arg);
  undefined overload2(TestInterface arg);
  undefined overload2(optional Dict arg = {});
  undefined overload2(boolean arg);
  undefined overload2(DOMString arg);
  undefined overload3(TestInterface arg);
  undefined overload3(TestCallback arg);
  undefined overload3(boolean arg);
  undefined overload4(TestInterface arg);
  undefined overload4(TestCallbackInterface arg);
  undefined overload4(DOMString arg);
  undefined overload5(long arg);
  undefined overload5(TestEnum arg);
  undefined overload6(long arg);
  undefined overload6(boolean arg);
  undefined overload7(long arg);
  undefined overload7(boolean arg);
  undefined overload7(ByteString arg);
  undefined overload8(long arg);
  undefined overload8(TestInterface arg);
  undefined overload9(long? arg);
  undefined overload9(DOMString arg);
  undefined overload10(long? arg);
  undefined overload10(object arg);
  undefined overload11(long arg);
  undefined overload11(DOMString? arg);
  undefined overload12(long arg);
  undefined overload12(boolean? arg);
  undefined overload13(long? arg);
  undefined overload13(boolean arg);
  undefined overload14(optional long arg);
  undefined overload14(TestInterface arg);
  undefined overload15(long arg);
  undefined overload15(optional TestInterface arg);
  undefined overload16(long arg);
  undefined overload16(optional TestInterface? arg);
  undefined overload17(sequence<long> arg);
  undefined overload17(record<DOMString, long> arg);
  undefined overload18(record<DOMString, DOMString> arg);
  undefined overload18(sequence<DOMString> arg);
  undefined overload19(sequence<long> arg);
  undefined overload19(optional Dict arg = {});
  undefined overload20(optional Dict arg = {});
  undefined overload20(sequence<long> arg);

  // Variadic handling
  undefined passVariadicThirdArg(DOMString arg1, long arg2, TestInterface... arg3);

  // Conditionally exposed methods/attributes
  [Pref="dom.webidl.test1"]
  readonly attribute boolean prefable1;
  [Pref="dom.webidl.test1"]
  readonly attribute boolean prefable2;
  [Pref="dom.webidl.test2"]
  readonly attribute boolean prefable3;
  [Pref="dom.webidl.test2"]
  readonly attribute boolean prefable4;
  [Pref="dom.webidl.test1"]
  readonly attribute boolean prefable5;
  [Pref="dom.webidl.test1", Func="nsGenericHTMLElement::LegacyTouchAPIEnabled"]
  readonly attribute boolean prefable6;
  [Pref="dom.webidl.test1", Func="nsGenericHTMLElement::LegacyTouchAPIEnabled"]
  readonly attribute boolean prefable7;
  [Pref="dom.webidl.test2", Func="nsGenericHTMLElement::LegacyTouchAPIEnabled"]
  readonly attribute boolean prefable8;
  [Pref="dom.webidl.test1", Func="nsGenericHTMLElement::LegacyTouchAPIEnabled"]
  readonly attribute boolean prefable9;
  [Pref="dom.webidl.test1"]
  undefined prefable10();
  [Pref="dom.webidl.test1", Func="nsGenericHTMLElement::LegacyTouchAPIEnabled"]
  undefined prefable11();
  [Pref="dom.webidl.test1", Func="TestFuncControlledMember"]
  readonly attribute boolean prefable12;
  [Pref="dom.webidl.test1", Func="nsGenericHTMLElement::LegacyTouchAPIEnabled"]
  undefined prefable13();
  [Pref="dom.webidl.test1", Func="TestFuncControlledMember"]
  readonly attribute boolean prefable14;
  [Func="TestFuncControlledMember"]
  readonly attribute boolean prefable15;
  [Func="TestFuncControlledMember"]
  readonly attribute boolean prefable16;
  [Pref="dom.webidl.test1", Func="TestFuncControlledMember"]
  undefined prefable17();
  [Func="TestFuncControlledMember"]
  undefined prefable18();
  [Func="TestFuncControlledMember"]
  undefined prefable19();
  [Pref="dom.webidl.test1", Func="TestFuncControlledMember", ChromeOnly]
  undefined prefable20();
  [Trial="TestTrial"]
  undefined prefable21();
  [Pref="dom.webidl.test1", Trial="TestTrial"]
  undefined prefable22();

  // Conditionally exposed methods/attributes involving [SecureContext]
  [SecureContext]
  readonly attribute boolean conditionalOnSecureContext1;
  [SecureContext, Pref="dom.webidl.test1"]
  readonly attribute boolean conditionalOnSecureContext2;
  [SecureContext, Pref="dom.webidl.test1", Func="nsGenericHTMLElement::LegacyTouchAPIEnabled"]
  readonly attribute boolean conditionalOnSecureContext3;
  [SecureContext, Pref="dom.webidl.test1", Func="TestFuncControlledMember"]
  readonly attribute boolean conditionalOnSecureContext4;
  [SecureContext]
  undefined conditionalOnSecureContext5();
  [SecureContext, Pref="dom.webidl.test1"]
  undefined conditionalOnSecureContext6();
  [SecureContext, Pref="dom.webidl.test1", Func="nsGenericHTMLElement::LegacyTouchAPIEnabled"]
  undefined conditionalOnSecureContext7();
  [SecureContext, Pref="dom.webidl.test1", Func="TestFuncControlledMember"]
  undefined conditionalOnSecureContext8();
  [SecureContext, Trial="TestTrial"]
  readonly attribute boolean conditionalOnSecureContext9;
  [SecureContext, Pref="dom.webidl.test1", Func="TestFuncControlledMember", Trial="TestTrial"]
  undefined conditionalOnSecureContext10();

  // Miscellania
  [LegacyLenientThis] attribute long attrWithLenientThis;
  [LegacyUnforgeable] readonly attribute long unforgeableAttr;
  [LegacyUnforgeable, ChromeOnly] readonly attribute long unforgeableAttr2;
  [LegacyUnforgeable] long unforgeableMethod();
  [LegacyUnforgeable, ChromeOnly] long unforgeableMethod2();
  stringifier;
  undefined passRenamedInterface(TestRenamedInterface arg);
  [PutForwards=writableByte] readonly attribute TestInterface putForwardsAttr;
  [PutForwards=writableByte, LegacyLenientThis] readonly attribute TestInterface putForwardsAttr2;
  [PutForwards=writableByte, ChromeOnly] readonly attribute TestInterface putForwardsAttr3;
  [Throws] undefined throwingMethod();
  [Throws] attribute boolean throwingAttr;
  [GetterThrows] attribute boolean throwingGetterAttr;
  [SetterThrows] attribute boolean throwingSetterAttr;
  [CanOOM] undefined canOOMMethod();
  [CanOOM] attribute boolean canOOMAttr;
  [GetterCanOOM] attribute boolean canOOMGetterAttr;
  [SetterCanOOM] attribute boolean canOOMSetterAttr;
  [NeedsSubjectPrincipal] undefined needsSubjectPrincipalMethod();
  [NeedsSubjectPrincipal] attribute boolean needsSubjectPrincipalAttr;
  [NeedsCallerType] undefined needsCallerTypeMethod();
  [NeedsCallerType] attribute boolean needsCallerTypeAttr;
  [NeedsSubjectPrincipal=NonSystem] undefined needsNonSystemSubjectPrincipalMethod();
  [NeedsSubjectPrincipal=NonSystem] attribute boolean needsNonSystemSubjectPrincipalAttr;
  [CEReactions] undefined ceReactionsMethod();
  [CEReactions] undefined ceReactionsMethodOverload();
  [CEReactions] undefined ceReactionsMethodOverload(DOMString bar);
  [CEReactions] attribute boolean ceReactionsAttr;
  legacycaller short(unsigned long arg1, TestInterface arg2);
  undefined passArgsWithDefaults(optional long arg1,
                                 optional TestInterface? arg2 = null,
                                 optional Dict arg3 = {}, optional double arg4 = 5.0,
                                 optional float arg5);

  attribute any toJSONShouldSkipThis;
  attribute TestParentInterface toJSONShouldSkipThis2;
  attribute TestCallbackInterface toJSONShouldSkipThis3;
  [Default] object toJSON();

  attribute byte dashed-attribute;
  undefined dashed-method();

  // [NonEnumerable] tests
  [NonEnumerable]
  attribute boolean nonEnumerableAttr;
  [NonEnumerable]
  const boolean nonEnumerableConst = true;
  [NonEnumerable]
  undefined nonEnumerableMethod();

  // [AllowShared] tests
  attribute [AllowShared] ArrayBufferViewTypedef allowSharedArrayBufferViewTypedef;
  attribute [AllowShared] ArrayBufferView allowSharedArrayBufferView;
  attribute [AllowShared] ArrayBufferView? allowSharedNullableArrayBufferView;
  attribute [AllowShared] ArrayBuffer allowSharedArrayBuffer;
  attribute [AllowShared] ArrayBuffer? allowSharedNullableArrayBuffer;

  undefined passAllowSharedArrayBufferViewTypedef(AllowSharedArrayBufferViewTypedef foo);
  undefined passAllowSharedArrayBufferView([AllowShared] ArrayBufferView foo);
  undefined passAllowSharedNullableArrayBufferView([AllowShared] ArrayBufferView? foo);
  undefined passAllowSharedArrayBuffer([AllowShared] ArrayBuffer foo);
  undefined passAllowSharedNullableArrayBuffer([AllowShared] ArrayBuffer? foo);
  undefined passUnionArrayBuffer((DOMString or ArrayBuffer) foo);
  undefined passUnionAllowSharedArrayBuffer((DOMString or [AllowShared] ArrayBuffer) foo);

  // If you add things here, add them to TestExampleGen and TestJSImplGen as well
};

[Exposed=Window]
interface TestParentInterface {
};

[Exposed=Window]
interface TestChildInterface : TestParentInterface {
};

[Exposed=Window]
interface TestNonWrapperCacheInterface {
};

interface mixin InterfaceMixin {
  undefined mixedInMethod();
  attribute boolean mixedInProperty;

  const long mixedInConstant = 5;
};

dictionary Dict : ParentDict {
  TestEnum someEnum;
  long x;
  long a;
  long b = 8;
  long z = 9;
  [EnforceRange] unsigned long enforcedUnsignedLong;
  [Clamp] unsigned long clampedUnsignedLong;
  DOMString str;
  DOMString empty = "";
  TestEnum otherEnum = "b";
  DOMString otherStr = "def";
  DOMString? yetAnotherStr = null;
  DOMString template;
  ByteString byteStr;
  ByteString emptyByteStr = "";
  ByteString otherByteStr = "def";
  // JSString jsStr; // NOT SUPPORTED YET
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

  (float or DOMString) floatOrString = "str";
  (float or DOMString)? nullableFloatOrString = "str";
  (object or long) objectOrLong;
#ifdef DEBUG
  (EventInit or long) eventInitOrLong;
  (EventInit or long)? nullableEventInitOrLong;
  (HTMLElement or long)? nullableHTMLElementOrLong;
  // CustomEventInit is useful to test because it needs rooting.
  (CustomEventInit or long) eventInitOrLong2;
  (CustomEventInit or long)? nullableEventInitOrLong2;
  (EventInit or long) eventInitOrLongWithDefaultValue = {};
  (CustomEventInit or long) eventInitOrLongWithDefaultValue2 = {};
  (EventInit or long) eventInitOrLongWithDefaultValue3 = 5;
  (CustomEventInit or long) eventInitOrLongWithDefaultValue4 = 5;
  (EventInit or long)? nullableEventInitOrLongWithDefaultValue = null;
  (CustomEventInit or long)? nullableEventInitOrLongWithDefaultValue2 = null;
  (EventInit or long)? nullableEventInitOrLongWithDefaultValue3 = 5;
  (CustomEventInit or long)? nullableEventInitOrLongWithDefaultValue4 = 5;
  (sequence<object> or long) objectSequenceOrLong;
  (sequence<object> or long) objectSequenceOrLongWithDefaultValue1 = 1;
  (sequence<object> or long) objectSequenceOrLongWithDefaultValue2 = [];
  (sequence<object> or long)? nullableObjectSequenceOrLong;
  (sequence<object> or long)? nullableObjectSequenceOrLongWithDefaultValue1 = 1;
  (sequence<object> or long)? nullableObjectSequenceOrLongWithDefaultValue2 = [];
#endif

  ArrayBuffer arrayBuffer;
  ArrayBuffer? nullableArrayBuffer;
  Uint8Array uint8Array;
  Float64Array? float64Array = null;

  sequence<long> seq1;
  sequence<long> seq2 = [];
  sequence<long>? seq3;
  sequence<long>? seq4 = null;
  sequence<long>? seq5 = [];

  long dashed-name;

  required long requiredLong;
  required object requiredObject;

  CustomEventInit customEventInit;
  TestDictionaryTypedef dictionaryTypedef;

  Promise<undefined> promise;
  sequence<Promise<undefined>> promiseSequence;

  record<DOMString, long> recordMember;
  record<DOMString, long>? nullableRecord;
  record<DOMString, DOMString>? nullableRecordWithDefault = null;
  record<USVString, long> usvStringRecord;
  record<USVString, long>? nullableUSVStringRecordWithDefault = null;
  record<ByteString, long> byteStringRecord;
  record<ByteString, long>? nullableByteStringRecordWithDefault = null;
  record<UTF8String, long> utf8StringRecord;
  record<UTF8String, long>? nullableUTF8StringRecordWithDefault = null;
  required record<DOMString, TestInterface> requiredRecord;
  required record<USVString, TestInterface> requiredUSVRecord;
  required record<ByteString, TestInterface> requiredByteRecord;
  required record<UTF8String, TestInterface> requiredUTF8Record;
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
  sequence<(float or DOMString)> ourSequence10;
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

dictionary DictWithConditionalMembers {
  [ChromeOnly]
  long chromeOnlyMember;
  [Func="TestFuncControlledMember"]
  long funcControlledMember;
  [ChromeOnly, Func="nsGenericHTMLElement::LegacyTouchAPIEnabled"]
  long chromeOnlyFuncControlledMember;
  // We need a pref name that's in StaticPrefList.h here.
  [Pref="dom.webidl.test1"]
  long prefControlledMember;
  [Pref="dom.webidl.test1", ChromeOnly, Func="TestFuncControlledMember"]
  long chromeOnlyFuncAndPrefControlledMember;
};

dictionary DictWithAllowSharedMembers {
  [AllowShared] ArrayBufferView a;
  [AllowShared] ArrayBufferView? b;
  [AllowShared] ArrayBuffer c;
  [AllowShared] ArrayBuffer? d;
  [AllowShared] ArrayBufferViewTypedef e;
  AllowSharedArrayBufferViewTypedef f;
};

dictionary DictWithBinaryType {
  [BinaryType="nsAutoString"]
  DOMString otherTypeOfStorageStr = "";
};

[Exposed=Window]
interface TestIndexedGetterInterface {
  getter long item(unsigned long idx);
  readonly attribute unsigned long length;
  legacycaller undefined();
  [Cached, Pure] readonly attribute long cachedAttr;
  [StoreInSlot, Pure] readonly attribute long storeInSlotAttr;
};

[Exposed=Window]
interface TestNamedGetterInterface {
  getter DOMString (DOMString name);
};

[Exposed=Window]
interface TestIndexedGetterAndSetterAndNamedGetterInterface {
  getter DOMString (DOMString myName);
  getter long (unsigned long index);
  setter undefined (unsigned long index, long arg);
  readonly attribute unsigned long length;
};

[Exposed=Window]
interface TestIndexedAndNamedGetterInterface {
  getter long (unsigned long index);
  getter DOMString namedItem(DOMString name);
  readonly attribute unsigned long length;
};

[Exposed=Window]
interface TestIndexedSetterInterface {
  setter undefined setItem(unsigned long idx, DOMString item);
  getter DOMString (unsigned long idx);
  readonly attribute unsigned long length;
};

[Exposed=Window]
interface TestNamedSetterInterface {
  setter undefined (DOMString myName, TestIndexedSetterInterface item);
  getter TestIndexedSetterInterface (DOMString name);
};

[Exposed=Window]
interface TestIndexedAndNamedSetterInterface {
  setter undefined (unsigned long index, TestIndexedSetterInterface item);
  getter TestIndexedSetterInterface (unsigned long index);
  readonly attribute unsigned long length;
  setter undefined setNamedItem(DOMString name, TestIndexedSetterInterface item);
  getter TestIndexedSetterInterface (DOMString name);
};

[Exposed=Window]
interface TestIndexedAndNamedGetterAndSetterInterface : TestIndexedSetterInterface {
  getter long item(unsigned long index);
  getter DOMString namedItem(DOMString name);
  setter undefined (unsigned long index, long item);
  setter undefined (DOMString name, DOMString item);
  stringifier DOMString ();
  readonly attribute unsigned long length;
};

[Exposed=Window]
interface TestNamedDeleterInterface {
  deleter undefined (DOMString name);
  getter long (DOMString name);
};

[Exposed=Window]
interface TestNamedDeleterWithRetvalInterface {
  deleter boolean delNamedItem(DOMString name);
  getter long (DOMString name);
};

[Exposed=Window]
interface TestCppKeywordNamedMethodsInterface {
  boolean continue();
  boolean delete();
  long volatile();
};

[Deprecated="Components",
 Exposed=Window]
interface TestDeprecatedInterface {
  constructor();

  static undefined alsoDeprecated();
};


[Exposed=Window]
interface TestInterfaceWithPromiseConstructorArg {
  constructor(Promise<undefined> promise);
};

[Exposed=Window]
namespace TestNamespace {
  readonly attribute boolean foo;
  long bar();
};

partial namespace TestNamespace {
  undefined baz();
};

[ClassString="RenamedNamespaceClassName",
 Exposed=Window]
namespace TestRenamedNamespace {
};

[ProtoObjectHack,
 Exposed=Window]
namespace TestProtoObjectHackedNamespace {
};

[SecureContext,
 Exposed=Window]
interface TestSecureContextInterface {
  static undefined alsoSecureContext();
};

[Exposed=(Window,Worker)]
interface TestWorkerExposedInterface {
  [NeedsSubjectPrincipal] undefined needsSubjectPrincipalMethod();
  [NeedsSubjectPrincipal] attribute boolean needsSubjectPrincipalAttr;
  [NeedsCallerType] undefined needsCallerTypeMethod();
  [NeedsCallerType] attribute boolean needsCallerTypeAttr;
  [NeedsSubjectPrincipal=NonSystem] undefined needsNonSystemSubjectPrincipalMethod();
  [NeedsSubjectPrincipal=NonSystem] attribute boolean needsNonSystemSubjectPrincipalAttr;
};

[Exposed=Window]
interface TestHTMLConstructorInterface {
  [HTMLConstructor] constructor();
};

[Exposed=Window]
interface TestThrowingConstructorInterface {
  [Throws]
  constructor();
  [Throws]
  constructor(DOMString str);
  [Throws]
  constructor(unsigned long num, boolean? boolArg);
  [Throws]
  constructor(TestInterface? iface);
  [Throws]
  constructor(unsigned long arg1, TestInterface iface);
  [Throws]
  constructor(ArrayBuffer arrayBuf);
  [Throws]
  constructor(Uint8Array typedArr);
  // [Throws] constructor(long arg1, long arg2, (TestInterface or OnlyForUseInConstructor) arg3);
};

[Exposed=Window]
interface TestCEReactionsInterface {
  [CEReactions] setter undefined (unsigned long index, long item);
  [CEReactions] setter undefined (DOMString name, DOMString item);
  [CEReactions] deleter undefined (DOMString name);
  getter long item(unsigned long index);
  getter DOMString (DOMString name);
  readonly attribute unsigned long length;
};

typedef [EnforceRange] octet OctetRange;
typedef [Clamp] octet OctetClamp;
typedef [LegacyNullToEmptyString] DOMString NullEmptyString;
// typedef [TreatNullAs=EmptyString] JSString NullEmptyJSString;

dictionary TestAttributesOnDictionaryMembers {
  [Clamp] octet a;
  [ChromeOnly, Clamp] octet b;
  required [Clamp] octet c;
  [ChromeOnly] octet d;
  // ChromeOnly doesn't mix with required, so we can't
  // test [ChromeOnly] required [Clamp] octet e
};

[Exposed=Window]
interface TestAttributesOnTypes {
  undefined foo(OctetClamp thingy);
  undefined bar(OctetRange thingy);
  undefined baz(NullEmptyString thingy);
  // undefined qux(NullEmptyJSString thingy);
  attribute [Clamp] octet someAttr;
  undefined argWithAttr([Clamp] octet arg0, optional [Clamp] octet arg1);
  // There aren't any argument-only attributes that we can test here,
  // TreatNonCallableAsNull isn't compatible with Clamp-able types
};

[Exposed=Window]
interface TestPrefConstructorForInterface {
  // Since only the constructor is under a pref,
  // the generated constructor should check for the pref.
  [Pref="dom.webidl.test1"] constructor();
};

[Exposed=Window, Pref="dom.webidl.test1"]
interface TestConstructorForPrefInterface {
  // Since the interface itself is under a Pref, there should be no
  // check for the pref in the generated constructor.
  constructor();
};

[Exposed=Window, Pref="dom.webidl.test1"]
interface TestPrefConstructorForDifferentPrefInterface {
  // Since the constructor's pref is different than the interface pref
  // there should still be a check for the pref in the generated constructor.
  [Pref="dom.webidl.test2"] constructor();
};

[Exposed=Window, SecureContext]
interface TestConstructorForSCInterface {
  // Since the interface itself is SecureContext, there should be no
  // runtime check for SecureContext in the generated constructor.
  constructor();
};

[Exposed=Window]
interface TestSCConstructorForInterface {
  // Since the interface context is unspecified but the constructor is
  // SecureContext, the generated constructor should check for SecureContext.
  [SecureContext] constructor();
};

[Exposed=Window, Func="Document::IsWebAnimationsGetAnimationsEnabled"]
interface TestConstructorForFuncInterface {
  // Since the interface has a Func attribute, but the constructor does not,
  // the generated constructor should not check for the Func.
  constructor();
};

[Exposed=Window]
interface TestFuncConstructorForInterface {
  // Since the constructor has a Func attribute, but the interface does not,
  // the generated constructor should check for the Func.
  [Func="Document::IsWebAnimationsGetAnimationsEnabled"]
  constructor();
};

[Exposed=Window, Func="Document::AreWebAnimationsTimelinesEnabled"]
interface TestFuncConstructorForDifferentFuncInterface {
  // Since the constructor has a different Func attribute from the interface,
  // the generated constructor should still check for its conditional func.
  [Func="Document::IsWebAnimationsGetAnimationsEnabled"]
  constructor();
};

[Exposed=Window]
interface TestPrefChromeOnlySCFuncConstructorForInterface {
  [Pref="dom.webidl.test1", ChromeOnly, SecureContext, Func="Document::IsWebAnimationsGetAnimationsEnabled"]
  // There should be checks for all Pref/ChromeOnly/SecureContext/Func
  // in the generated constructor.
  constructor();
};
