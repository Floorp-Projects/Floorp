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
  void doSomething();
  long doSomethingElse(DOMString arg, TestInterface otherArg);
  void doSequenceLongArg(sequence<long> arg);
  void doSequenceStringArg(sequence<DOMString> arg);
  void doRecordLongArg(record<DOMString, long> arg);
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
  void passSequenceOfArrayBuffers(sequence<ArrayBuffer> arg);
  void passSequenceOfNullableArrayBuffers(sequence<ArrayBuffer?> arg);
  void passVariadicTypedArray(Float32Array... arg);
  void passVariadicNullableTypedArray(Float32Array?... arg);
  Uint8Array receiveUint8Array();
  attribute Uint8Array uint8ArrayAttr;
  Promise<void> receivePromise();
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
// Callback constructor return value tests
callback constructor TestVoidConstruction = void(TestDictionaryTypedef arg);
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
  void passByte(byte arg);
  byte receiveByte();
  void passOptionalByte(optional byte arg);
  void passOptionalByteBeforeRequired(optional byte arg1, byte arg2);
  void passOptionalByteWithDefault(optional byte arg = 0);
  void passOptionalByteWithDefaultBeforeRequired(optional byte arg1 = 0, byte arg2);
  void passNullableByte(byte? arg);
  void passOptionalNullableByte(optional byte? arg);
  void passVariadicByte(byte... arg);
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
  [Cached, Pure]
  readonly attribute TestInterface cachedSelf;
  // Optional arguments
  void passOptionalSelf(optional TestInterface? arg);
  void passOptionalNonNullSelf(optional TestInterface arg);
  void passOptionalSelfWithDefault(optional TestInterface? arg = null);

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
  void passOptionalSequenceWithDefaultValue(optional sequence<long> arg = []);
  void passOptionalNullableSequence(optional sequence<long>? arg);
  void passOptionalNullableSequenceWithDefaultValue(optional sequence<long>? arg = null);
  void passOptionalNullableSequenceWithDefaultValue2(optional sequence<long>? arg = []);
  void passOptionalObjectSequence(optional sequence<TestInterface> arg);
  void passExternalInterfaceSequence(sequence<TestExternalInterface> arg);
  void passNullableExternalInterfaceSequence(sequence<TestExternalInterface?> arg);

  sequence<DOMString> receiveStringSequence();
  void passStringSequence(sequence<DOMString> arg);

  sequence<ByteString> receiveByteStringSequence();
  void passByteStringSequence(sequence<ByteString> arg);

  sequence<UTF8String> receiveUTF8StringSequence();
  void passUTF8StringSequence(sequence<UTF8String> arg);

  sequence<any> receiveAnySequence();
  sequence<any>? receiveNullableAnySequence();
  sequence<sequence<any>> receiveAnySequenceSequence();

  sequence<object> receiveObjectSequence();
  sequence<object?> receiveNullableObjectSequence();

  void passSequenceOfSequences(sequence<sequence<long>> arg);
  void passSequenceOfSequencesOfSequences(sequence<sequence<sequence<long>>> arg);
  sequence<sequence<long>> receiveSequenceOfSequences();
  sequence<sequence<sequence<long>>> receiveSequenceOfSequencesOfSequences();

  // record types
  void passRecord(record<DOMString, long> arg);
  void passNullableRecord(record<DOMString, long>? arg);
  void passRecordOfNullableInts(record<DOMString, long?> arg);
  void passOptionalRecordOfNullableInts(optional record<DOMString, long?> arg);
  void passOptionalNullableRecordOfNullableInts(optional record<DOMString, long?>? arg);
  void passCastableObjectRecord(record<DOMString, TestInterface> arg);
  void passNullableCastableObjectRecord(record<DOMString, TestInterface?> arg);
  void passCastableObjectNullableRecord(record<DOMString, TestInterface>? arg);
  void passNullableCastableObjectNullableRecord(record<DOMString, TestInterface?>? arg);
  void passOptionalRecord(optional record<DOMString, long> arg);
  void passOptionalNullableRecord(optional record<DOMString, long>? arg);
  void passOptionalNullableRecordWithDefaultValue(optional record<DOMString, long>? arg = null);
  void passOptionalObjectRecord(optional record<DOMString, TestInterface> arg);
  void passExternalInterfaceRecord(record<DOMString, TestExternalInterface> arg);
  void passNullableExternalInterfaceRecord(record<DOMString, TestExternalInterface?> arg);
  void passStringRecord(record<DOMString, DOMString> arg);
  void passByteStringRecord(record<DOMString, ByteString> arg);
  void passUTF8StringRecord(record<DOMString, UTF8String> arg);
  void passRecordOfRecords(record<DOMString, record<DOMString, long>> arg);
  record<DOMString, long> receiveRecord();
  record<DOMString, long>? receiveNullableRecord();
  record<DOMString, long?> receiveRecordOfNullableInts();
  record<DOMString, long?>? receiveNullableRecordOfNullableInts();
  record<DOMString, record<DOMString, long>> receiveRecordOfRecords();
  record<DOMString, any> receiveAnyRecord();

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
  void passSequenceOfArrayBuffers(sequence<ArrayBuffer> arg);
  void passSequenceOfNullableArrayBuffers(sequence<ArrayBuffer?> arg);
  void passRecordOfArrayBuffers(record<DOMString, ArrayBuffer> arg);
  void passRecordOfNullableArrayBuffers(record<DOMString, ArrayBuffer?> arg);
  void passVariadicTypedArray(Float32Array... arg);
  void passVariadicNullableTypedArray(Float32Array?... arg);
  Uint8Array receiveUint8Array();
  attribute Uint8Array uint8ArrayAttr;

  // DOMString types
  void passString(DOMString arg);
  void passNullableString(DOMString? arg);
  void passOptionalString(optional DOMString arg);
  void passOptionalStringWithDefaultValue(optional DOMString arg = "abc");
  void passOptionalNullableString(optional DOMString? arg);
  void passOptionalNullableStringWithDefaultValue(optional DOMString? arg = null);
  void passVariadicString(DOMString... arg);
  DOMString receiveString();

  // ByteString types
  void passByteString(ByteString arg);
  void passNullableByteString(ByteString? arg);
  void passOptionalByteString(optional ByteString arg);
  void passOptionalByteStringWithDefaultValue(optional ByteString arg = "abc");
  void passOptionalNullableByteString(optional ByteString? arg);
  void passOptionalNullableByteStringWithDefaultValue(optional ByteString? arg = null);
  void passVariadicByteString(ByteString... arg);
  void passOptionalUnionByteString(optional (ByteString or long) arg);
  void passOptionalUnionByteStringWithDefaultValue(optional (ByteString or long) arg = "abc");

  // UTF8String types
  void passUTF8String(UTF8String arg);
  void passNullableUTF8String(UTF8String? arg);
  void passOptionalUTF8String(optional UTF8String arg);
  void passOptionalUTF8StringWithDefaultValue(optional UTF8String arg = "abc");
  void passOptionalNullableUTF8String(optional UTF8String? arg);
  void passOptionalNullableUTF8StringWithDefaultValue(optional UTF8String? arg = null);
  void passVariadicUTF8String(UTF8String... arg);
  void passOptionalUnionUTF8String(optional (UTF8String or long) arg);
  void passOptionalUnionUTF8StringWithDefaultValue(optional (UTF8String or long) arg = "abc");

  // USVString types
  void passUSVS(USVString arg);
  void passNullableUSVS(USVString? arg);
  void passOptionalUSVS(optional USVString arg);
  void passOptionalUSVSWithDefaultValue(optional USVString arg = "abc");
  void passOptionalNullableUSVS(optional USVString? arg);
  void passOptionalNullableUSVSWithDefaultValue(optional USVString? arg = null);
  void passVariadicUSVS(USVString... arg);
  USVString receiveUSVS();

  // JSString types
  void passJSString(JSString arg);
  // void passNullableJSString(JSString? arg); // NOT SUPPORTED YET
  // void passOptionalJSString(optional JSString arg); // NOT SUPPORTED YET
  void passOptionalJSStringWithDefaultValue(optional JSString arg = "abc");
  // void passOptionalNullableJSString(optional JSString? arg); // NOT SUPPORTED YET
  // void passOptionalNullableJSStringWithDefaultValue(optional JSString? arg = null); // NOT SUPPORTED YET
  // void passVariadicJSString(JSString... arg); // NOT SUPPORTED YET
  // void passRecordOfJSString(record<DOMString, JSString> arg); // NOT SUPPORTED YET
  // void passSequenceOfJSString(sequence<JSString> arg); // NOT SUPPORTED YET
  // void passUnionJSString((JSString or long) arg); // NOT SUPPORTED YET
  JSString receiveJSString();
  // sequence<JSString> receiveJSStringSequence(); // NOT SUPPORTED YET
  // (JSString or long) receiveJSStringUnion(); // NOT SUPPORTED YET
  // record<DOMString, JSString> receiveJSStringRecord(); // NOT SUPPORTED YET
  readonly attribute JSString readonlyJSStringAttr;
  attribute JSString jsStringAttr;

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

  // Force code generation of the various test callbacks we have.
  void forceCallbackGeneration(TestIntegerReturn arg1,
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
                               TestVoidConstruction arg26,
                               TestIntegerConstruction arg27,
                               TestBooleanConstruction arg28,
                               TestFloatConstruction arg29,
                               TestStringConstruction arg30,
                               TestEnumConstruction arg31,
                               TestInterfaceConstruction arg32,
                               TestExternalInterfaceConstruction arg33,
                               TestCallbackInterfaceConstruction arg34,
                               TestCallbackConstruction arg35,
                               TestObjectConstruction arg36,
                               TestTypedArrayConstruction arg37,
                               TestSequenceConstruction arg38);

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
  void passRecordOfAny(record<DOMString, any> arg);
  void passNullableRecordOfAny(record<DOMString, any>? arg);
  void passOptionalRecordOfAny(optional record<DOMString, any> arg);
  void passOptionalNullableRecordOfAny(optional record<DOMString, any>? arg);
  void passOptionalRecordOfAnyWithDefaultValue(optional record<DOMString, any>? arg = null);
  void passRecordOfRecordOfAny(record<DOMString, record<DOMString, any>> arg);
  void passRecordOfNullableRecordOfAny(record<DOMString, record<DOMString, any>?> arg);
  void passNullableRecordOfNullableRecordOfAny(record<DOMString, record<DOMString, any>?>? arg);
  void passOptionalNullableRecordOfNullableRecordOfAny(optional record<DOMString, record<DOMString, any>?>? arg);
  void passOptionalNullableRecordOfNullableSequenceOfAny(optional record<DOMString, sequence<any>?>? arg);
  void passOptionalNullableSequenceOfNullableRecordOfAny(optional sequence<record<DOMString, any>?>? arg);
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
  void passNullableSequenceOfObject(sequence<object>? arg);
  void passOptionalNullableSequenceOfNullableSequenceOfObject(optional sequence<sequence<object>?>? arg);
  void passOptionalNullableSequenceOfNullableSequenceOfNullableObject(optional sequence<sequence<object?>?>? arg);
  void passRecordOfObject(record<DOMString, object> arg);
  object receiveObject();
  object? receiveNullableObject();

  // Union types
  void passUnion((object or long) arg);
  // Some  union tests are debug-only to avoid creating all those
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
  void passUnion10(optional (EventInit or long) arg = {});
  void passUnion11(optional (CustomEventInit or long) arg = {});
  void passUnion12(optional (EventInit or long) arg = 5);
  void passUnion13(optional (object or long?) arg = null);
  void passUnion14(optional (object or long?) arg = 5);
  void passUnion15((sequence<long> or long) arg);
  void passUnion16(optional (sequence<long> or long) arg);
  void passUnion17(optional (sequence<long>? or long) arg = 5);
  void passUnion18((sequence<object> or long) arg);
  void passUnion19(optional (sequence<object> or long) arg);
  void passUnion20(optional (sequence<object> or long) arg = []);
  void passUnion21((record<DOMString, long> or long) arg);
  void passUnion22((record<DOMString, object> or long) arg);
  void passUnion23((sequence<ImageData> or long) arg);
  void passUnion24((sequence<ImageData?> or long) arg);
  void passUnion25((sequence<sequence<ImageData>> or long) arg);
  void passUnion26((sequence<sequence<ImageData?>> or long) arg);
  void passUnion27(optional (sequence<DOMString> or EventInit) arg = {});
  void passUnion28(optional (EventInit or sequence<DOMString>) arg = {});
  void passUnionWithCallback((EventHandler or long) arg);
  void passUnionWithByteString((ByteString or long) arg);
  void passUnionWithUTF8String((UTF8String or long) arg);
  void passUnionWithRecord((record<DOMString, DOMString> or DOMString) arg);
  void passUnionWithRecordAndSequence((record<DOMString, DOMString> or sequence<DOMString>) arg);
  void passUnionWithSequenceAndRecord((sequence<DOMString> or record<DOMString, DOMString>) arg);
  void passUnionWithUSVS((USVString or long) arg);
#endif
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
  // Using an enum in a union.  Note that we use some enum not declared in our
  // binding file, because UnionTypes.h will need to include the binding header
  // for this enum.  Pick an enum from an interface that won't drag in too much
  // stuff.
  void passUnionWithEnum((SupportedType or object) arg);

  // Trying to use a callback in a union won't include the test
  // headers, unfortunately, so won't compile.
  //void passUnionWithCallback((TestCallback or long) arg);
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
  void passUnionWithDefaultValue14(optional (double or ByteString) arg = "");
  void passUnionWithDefaultValue15(optional (double or ByteString) arg = 1);
  void passUnionWithDefaultValue16(optional (double or ByteString) arg = 1.5);
  void passUnionWithDefaultValue17(optional (double or SupportedType) arg = "text/html");
  void passUnionWithDefaultValue18(optional (double or SupportedType) arg = 1);
  void passUnionWithDefaultValue19(optional (double or SupportedType) arg = 1.5);
  void passUnionWithDefaultValue20(optional (double or USVString) arg = "abc");
  void passUnionWithDefaultValue21(optional (double or USVString) arg = 1);
  void passUnionWithDefaultValue22(optional (double or USVString) arg = 1.5);
  void passUnionWithDefaultValue23(optional (double or UTF8String) arg = "");
  void passUnionWithDefaultValue24(optional (double or UTF8String) arg = 1);
  void passUnionWithDefaultValue25(optional (double or UTF8String) arg = 1.5);

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
  void passNullableUnionWithDefaultValue13(optional (double or ByteString)? arg = "");
  void passNullableUnionWithDefaultValue14(optional (double or ByteString)? arg = 1);
  void passNullableUnionWithDefaultValue15(optional (double or ByteString)? arg = 1.5);
  void passNullableUnionWithDefaultValue16(optional (double or ByteString)? arg = null);
  void passNullableUnionWithDefaultValue17(optional (double or SupportedType)? arg = "text/html");
  void passNullableUnionWithDefaultValue18(optional (double or SupportedType)? arg = 1);
  void passNullableUnionWithDefaultValue19(optional (double or SupportedType)? arg = 1.5);
  void passNullableUnionWithDefaultValue20(optional (double or SupportedType)? arg = null);
  void passNullableUnionWithDefaultValue21(optional (double or USVString)? arg = "abc");
  void passNullableUnionWithDefaultValue22(optional (double or USVString)? arg = 1);
  void passNullableUnionWithDefaultValue23(optional (double or USVString)? arg = 1.5);
  void passNullableUnionWithDefaultValue24(optional (double or USVString)? arg = null);

  void passNullableUnionWithDefaultValue25(optional (double or UTF8String)? arg = "abc");
  void passNullableUnionWithDefaultValue26(optional (double or UTF8String)? arg = 1);
  void passNullableUnionWithDefaultValue27(optional (double or UTF8String)? arg = 1.5);
  void passNullableUnionWithDefaultValue28(optional (double or UTF8String)? arg = null);

  void passSequenceOfUnions(sequence<(CanvasPattern or CanvasGradient)> arg);
  void passSequenceOfUnions2(sequence<(object or long)> arg);
  void passVariadicUnion((CanvasPattern or CanvasGradient)... arg);

  void passSequenceOfNullableUnions(sequence<(CanvasPattern or CanvasGradient)?> arg);
  void passVariadicNullableUnion((CanvasPattern or CanvasGradient)?... arg);
  void passRecordOfUnions(record<DOMString, (CanvasPattern or CanvasGradient)> arg);
  // XXXbz no move constructor on some unions
  // void passRecordOfUnions2(record<DOMString, (object or long)> arg);

  (CanvasPattern or CanvasGradient) receiveUnion();
  (object or long) receiveUnion2();
  (CanvasPattern? or CanvasGradient) receiveUnionContainingNull();
  (CanvasPattern or CanvasGradient)? receiveNullableUnion();
  (object or long)? receiveNullableUnion2();

  attribute (CanvasPattern or CanvasGradient) writableUnion;
  attribute (CanvasPattern? or CanvasGradient) writableUnionContainingNull;
  attribute (CanvasPattern or CanvasGradient)? writableNullableUnion;

  // Promise types
  void passPromise(Promise<any> arg);
  void passOptionalPromise(optional Promise<any> arg);
  void passPromiseSequence(sequence<Promise<any>> arg);
  Promise<any> receivePromise();
  Promise<any> receiveAddrefedPromise();

  // binaryNames tests
  [BinaryName="methodRenamedTo"]
  void methodRenamedFrom();
  [BinaryName="methodRenamedTo"]
  void methodRenamedFrom(byte argument);
  [BinaryName="attributeGetterRenamedTo"]
  readonly attribute byte attributeGetterRenamedFrom;
  [BinaryName="attributeRenamedTo"]
  attribute byte attributeRenamedFrom;

  void passDictionary(optional Dict x = {});
  void passDictionary2(Dict x);
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
  void passOtherDictionary(optional GrandparentDict x = {});
  void passSequenceOfDictionaries(sequence<Dict> x);
  void passRecordOfDictionaries(record<DOMString, GrandparentDict> x);
  // No support for nullable dictionaries inside a sequence (nor should there be)
  //  void passSequenceOfNullableDictionaries(sequence<Dict?> x);
  void passDictionaryOrLong(optional Dict x = {});
  void passDictionaryOrLong(long x);

  void passDictContainingDict(optional DictContainingDict arg = {});
  void passDictContainingSequence(optional DictContainingSequence arg = {});
  DictContainingSequence receiveDictContainingSequence();
  void passVariadicDictionary(Dict... arg);

  // EnforceRange/Clamp tests
  void dontEnforceRangeOrClamp(byte arg);
  void doEnforceRange([EnforceRange] byte arg);
  void doEnforceRangeNullable([EnforceRange] byte? arg);
  void doClamp([Clamp] byte arg);
  void doClampNullable([Clamp] byte? arg);
  attribute [EnforceRange] byte enforcedByte;
  attribute [EnforceRange] byte? enforcedNullableByte;
  attribute [Clamp] byte clampedByte;
  attribute [Clamp] byte? clampedNullableByte;

  // Typedefs
  const myLong myLongConstant = 5;
  void exerciseTypedefInterfaces1(AnotherNameForTestInterface arg);
  AnotherNameForTestInterface exerciseTypedefInterfaces2(NullableTestInterface arg);
  void exerciseTypedefInterfaces3(YetAnotherNameForTestInterface arg);

  // Deprecated methods and attributes
  [Deprecated="Components"]
  attribute byte deprecatedAttribute;
  [Deprecated="Components"]
  byte deprecatedMethod();
  [Deprecated="Components"]
  byte deprecatedMethodWithContext(any arg);

  // Static methods and attributes
  static attribute boolean staticAttribute;
  static void staticMethod(boolean arg);
  static void staticMethodWithContext(any arg);

  // Testing static method with a reserved C++ keyword as the name
  static void assert(boolean arg);

  // Deprecated static methods and attributes
  [Deprecated="Components"]
  static attribute byte staticDeprecatedAttribute;
  [Deprecated="Components"]
  static void staticDeprecatedMethod();
  [Deprecated="Components"]
  static void staticDeprecatedMethodWithContext(any arg);

  // Overload resolution tests
  //void overload1(DOMString... strs);
  boolean overload1(TestInterface arg);
  TestInterface overload1(DOMString strs, TestInterface arg);
  void overload2(TestInterface arg);
  void overload2(optional Dict arg = {});
  void overload2(boolean arg);
  void overload2(DOMString arg);
  void overload3(TestInterface arg);
  void overload3(TestCallback arg);
  void overload3(boolean arg);
  void overload4(TestInterface arg);
  void overload4(TestCallbackInterface arg);
  void overload4(DOMString arg);
  void overload5(long arg);
  void overload5(TestEnum arg);
  void overload6(long arg);
  void overload6(boolean arg);
  void overload7(long arg);
  void overload7(boolean arg);
  void overload7(ByteString arg);
  void overload8(long arg);
  void overload8(TestInterface arg);
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
  void overload17(sequence<long> arg);
  void overload17(record<DOMString, long> arg);
  void overload18(record<DOMString, DOMString> arg);
  void overload18(sequence<DOMString> arg);
  void overload19(sequence<long> arg);
  void overload19(optional Dict arg = {});
  void overload20(optional Dict arg = {});
  void overload20(sequence<long> arg);

  // Variadic handling
  void passVariadicThirdArg(DOMString arg1, long arg2, TestInterface... arg3);

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
  void prefable10();
  [Pref="dom.webidl.test1", Func="nsGenericHTMLElement::LegacyTouchAPIEnabled"]
  void prefable11();
  [Pref="dom.webidl.test1", Func="TestFuncControlledMember"]
  readonly attribute boolean prefable12;
  [Pref="dom.webidl.test1", Func="nsGenericHTMLElement::LegacyTouchAPIEnabled"]
  void prefable13();
  [Pref="dom.webidl.test1", Func="TestFuncControlledMember"]
  readonly attribute boolean prefable14;
  [Func="TestFuncControlledMember"]
  readonly attribute boolean prefable15;
  [Func="TestFuncControlledMember"]
  readonly attribute boolean prefable16;
  [Pref="dom.webidl.test1", Func="TestFuncControlledMember"]
  void prefable17();
  [Func="TestFuncControlledMember"]
  void prefable18();
  [Func="TestFuncControlledMember"]
  void prefable19();
  [Pref="dom.webidl.test1", Func="TestFuncControlledMember", ChromeOnly]
  void prefable20();

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
  void conditionalOnSecureContext5();
  [SecureContext, Pref="dom.webidl.test1"]
  void conditionalOnSecureContext6();
  [SecureContext, Pref="dom.webidl.test1", Func="nsGenericHTMLElement::LegacyTouchAPIEnabled"]
  void conditionalOnSecureContext7();
  [SecureContext, Pref="dom.webidl.test1", Func="TestFuncControlledMember"]
  void conditionalOnSecureContext8();

  // Miscellania
  [LegacyLenientThis] attribute long attrWithLenientThis;
  [LegacyUnforgeable] readonly attribute long unforgeableAttr;
  [LegacyUnforgeable, ChromeOnly] readonly attribute long unforgeableAttr2;
  [LegacyUnforgeable] long unforgeableMethod();
  [LegacyUnforgeable, ChromeOnly] long unforgeableMethod2();
  stringifier;
  void passRenamedInterface(TestRenamedInterface arg);
  [PutForwards=writableByte] readonly attribute TestInterface putForwardsAttr;
  [PutForwards=writableByte, LegacyLenientThis] readonly attribute TestInterface putForwardsAttr2;
  [PutForwards=writableByte, ChromeOnly] readonly attribute TestInterface putForwardsAttr3;
  [Throws] void throwingMethod();
  [Throws] attribute boolean throwingAttr;
  [GetterThrows] attribute boolean throwingGetterAttr;
  [SetterThrows] attribute boolean throwingSetterAttr;
  [CanOOM] void canOOMMethod();
  [CanOOM] attribute boolean canOOMAttr;
  [GetterCanOOM] attribute boolean canOOMGetterAttr;
  [SetterCanOOM] attribute boolean canOOMSetterAttr;
  [NeedsSubjectPrincipal] void needsSubjectPrincipalMethod();
  [NeedsSubjectPrincipal] attribute boolean needsSubjectPrincipalAttr;
  [NeedsCallerType] void needsCallerTypeMethod();
  [NeedsCallerType] attribute boolean needsCallerTypeAttr;
  [NeedsSubjectPrincipal=NonSystem] void needsNonSystemSubjectPrincipalMethod();
  [NeedsSubjectPrincipal=NonSystem] attribute boolean needsNonSystemSubjectPrincipalAttr;
  [CEReactions] void ceReactionsMethod();
  [CEReactions] void ceReactionsMethodOverload();
  [CEReactions] void ceReactionsMethodOverload(DOMString bar);
  [CEReactions] attribute boolean ceReactionsAttr;
  legacycaller short(unsigned long arg1, TestInterface arg2);
  void passArgsWithDefaults(optional long arg1,
                            optional TestInterface? arg2 = null,
                            optional Dict arg3 = {}, optional double arg4 = 5.0,
                            optional float arg5);

  attribute any toJSONShouldSkipThis;
  attribute TestParentInterface toJSONShouldSkipThis2;
  attribute TestCallbackInterface toJSONShouldSkipThis3;
  [Default] object toJSON();

  attribute byte dashed-attribute;
  void dashed-method();

  // [NonEnumerable] tests
  [NonEnumerable]
  attribute boolean nonEnumerableAttr;
  [NonEnumerable]
  const boolean nonEnumerableConst = true;
  [NonEnumerable]
  void nonEnumerableMethod();

  // [AllowShared] tests
  attribute [AllowShared] ArrayBufferViewTypedef allowSharedArrayBufferViewTypedef;
  attribute [AllowShared] ArrayBufferView allowSharedArrayBufferView;
  attribute [AllowShared] ArrayBufferView? allowSharedNullableArrayBufferView;
  attribute [AllowShared] ArrayBuffer allowSharedArrayBuffer;
  attribute [AllowShared] ArrayBuffer? allowSharedNullableArrayBuffer;

  void passAllowSharedArrayBufferViewTypedef(AllowSharedArrayBufferViewTypedef foo);
  void passAllowSharedArrayBufferView([AllowShared] ArrayBufferView foo);
  void passAllowSharedNullableArrayBufferView([AllowShared] ArrayBufferView? foo);
  void passAllowSharedArrayBuffer([AllowShared] ArrayBuffer foo);
  void passAllowSharedNullableArrayBuffer([AllowShared] ArrayBuffer? foo);
  void passUnionArrayBuffer((DOMString or ArrayBuffer) foo);
  void passUnionAllowSharedArrayBuffer((DOMString or [AllowShared] ArrayBuffer) foo);

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
  void mixedInMethod();
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

  Promise<void> promise;
  sequence<Promise<void>> promiseSequence;

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

[Exposed=Window]
interface TestIndexedGetterInterface {
  getter long item(unsigned long idx);
  readonly attribute unsigned long length;
  legacycaller void();
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
  setter void (unsigned long index, long arg);
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
  setter void setItem(unsigned long idx, DOMString item);
  getter DOMString (unsigned long idx);
  readonly attribute unsigned long length;
};

[Exposed=Window]
interface TestNamedSetterInterface {
  setter void (DOMString myName, TestIndexedSetterInterface item);
  getter TestIndexedSetterInterface (DOMString name);
};

[Exposed=Window]
interface TestIndexedAndNamedSetterInterface {
  setter void (unsigned long index, TestIndexedSetterInterface item);
  getter TestIndexedSetterInterface (unsigned long index);
  readonly attribute unsigned long length;
  setter void setNamedItem(DOMString name, TestIndexedSetterInterface item);
  getter TestIndexedSetterInterface (DOMString name);
};

[Exposed=Window]
interface TestIndexedAndNamedGetterAndSetterInterface : TestIndexedSetterInterface {
  getter long item(unsigned long index);
  getter DOMString namedItem(DOMString name);
  setter void (unsigned long index, long item);
  setter void (DOMString name, DOMString item);
  stringifier DOMString ();
  readonly attribute unsigned long length;
};

[Exposed=Window]
interface TestNamedDeleterInterface {
  deleter void (DOMString name);
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

  static void alsoDeprecated();
};


[Exposed=Window]
interface TestInterfaceWithPromiseConstructorArg {
  constructor(Promise<void> promise);
};

[Exposed=Window]
namespace TestNamespace {
  readonly attribute boolean foo;
  long bar();
};

partial namespace TestNamespace {
  void baz();
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
  static void alsoSecureContext();
};

[Exposed=(Window,Worker)]
interface TestWorkerExposedInterface {
  [NeedsSubjectPrincipal] void needsSubjectPrincipalMethod();
  [NeedsSubjectPrincipal] attribute boolean needsSubjectPrincipalAttr;
  [NeedsCallerType] void needsCallerTypeMethod();
  [NeedsCallerType] attribute boolean needsCallerTypeAttr;
  [NeedsSubjectPrincipal=NonSystem] void needsNonSystemSubjectPrincipalMethod();
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
  [CEReactions] setter void (unsigned long index, long item);
  [CEReactions] setter void (DOMString name, DOMString item);
  [CEReactions] deleter void (DOMString name);
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
  void foo(OctetClamp thingy);
  void bar(OctetRange thingy);
  void baz(NullEmptyString thingy);
  // void qux(NullEmptyJSString thingy);
  attribute [Clamp] octet someAttr;
  void argWithAttr([Clamp] octet arg0, optional [Clamp] octet arg1);
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

[Exposed=Window, Func="Document::IsWebAnimationsEnabled"]
interface TestConstructorForFuncInterface {
  // Since the interface has a Func attribute, but the constructor does not,
  // the generated constructor should not check for the Func.
  constructor();
};

[Exposed=Window]
interface TestFuncConstructorForInterface {
  // Since the constructor has a Func attribute, but the interface does not,
  // the generated constructor should check for the Func.
  [Func="Document::IsWebAnimationsEnabled"]
  constructor();
};

[Exposed=Window, Func="Document::AreWebAnimationsTimelinesEnabled"]
interface TestFuncConstructorForDifferentFuncInterface {
  // Since the constructor has a different Func attribute from the interface,
  // the generated constructor should still check for its conditional func.
  [Func="Document::IsWebAnimationsEnabled"]
  constructor();
};

[Exposed=Window]
interface TestPrefChromeOnlySCFuncConstructorForInterface {
  [Pref="dom.webidl.test1", ChromeOnly, SecureContext, Func="Document::IsWebAnimationsEnabled"]
  // There should be checks for all Pref/ChromeOnly/SecureContext/Func
  // in the generated constructor.
  constructor();
};
