/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

interface TestInterface {
  // Integer types
  // XXXbz add tests for infallible versions of all the integer stuff
  readonly attribute byte readonlyByte;
  attribute byte writableByte;
  void passByte(byte arg);
  byte receiveByte();

  readonly attribute short readonlyShort;
  attribute short writableShort;
  void passShort(short arg);
  short receiveShort();

  readonly attribute long readonlyLong;
  attribute long writableLong;
  void passLong(long arg);
  long receiveLong();

  readonly attribute long long readonlyLongLong;
  attribute long long  writableLongLong;
  void passLongLong(long long arg);
  long long receiveLongLong();

  readonly attribute octet readonlyOctet;
  attribute octet writableOctet;
  void passOctet(octet arg);
  octet receiveOctet();

  readonly attribute unsigned short readonlyUnsignedShort;
  attribute unsigned short writableUnsignedShort;
  void passUnsignedShort(unsigned short arg);
  unsigned short receiveUnsignedShort();

  readonly attribute unsigned long readonlyUnsignedLong;
  attribute unsigned long writableUnsignedLong;
  void passUnsignedLong(unsigned long arg);
  unsigned long receiveUnsignedLong();

  readonly attribute unsigned long long readonlyUnsignedLongLong;
  attribute unsigned long long  writableUnsignedLongLong;
  void passUnsignedLongLong(unsigned long long arg);
  unsigned long long receiveUnsignedLongLong();

  // Castable interface types
  // XXXbz add tests for infallible versions of all the castable interface stuff
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

  // Sequence types
  sequence<long> receiveSequence();
  sequence<long>? receiveNullableSequence();
  void passSequence(sequence<long> arg);
  void passNullableSequence(sequence<long>? arg);
  sequence<TestInterface> receiveCastableObjectSequence();
  sequence<TestInterface?> receiveNullableCastableObjectSequence();
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
};
