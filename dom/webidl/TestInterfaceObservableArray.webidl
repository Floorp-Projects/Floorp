/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

callback SetDeleteObjectCallback = undefined (object value, unsigned long index);
callback SetDeleteBooleanCallback = undefined (boolean value, unsigned long index);
callback SetDeleteInterfaceCallback = undefined (TestInterfaceObservableArray value, unsigned long index);

dictionary ObservableArrayCallbacks {
  SetDeleteObjectCallback setObjectCallback;
  SetDeleteObjectCallback deleteObjectCallback;
  SetDeleteBooleanCallback setBooleanCallback;
  SetDeleteBooleanCallback deleteBooleanCallback;
  SetDeleteInterfaceCallback setInterfaceCallback;
  SetDeleteInterfaceCallback deleteInterfaceCallback;
};

[Pref="dom.expose_test_interfaces",
 Exposed=Window]
interface TestInterfaceObservableArray {
  [Throws]
  constructor(optional ObservableArrayCallbacks callbacks = {});

  // Testing for ObservableArray
  attribute ObservableArray<boolean> observableArrayBoolean;
  attribute ObservableArray<object> observableArrayObject;
  attribute ObservableArray<TestInterfaceObservableArray> observableArrayInterface;

  // Tests for C++ helper function
  [Throws]
  boolean booleanElementAtInternal(unsigned long index);
  [Throws]
  TestInterfaceObservableArray interfaceElementAtInternal(unsigned long index);
  [Throws]
  object objectElementAtInternal(unsigned long index);

  [Throws]
  undefined booleanReplaceElementAtInternal(unsigned long index, boolean value);
  [Throws]
  undefined interfaceReplaceElementAtInternal(unsigned long index, TestInterfaceObservableArray value);
  [Throws]
  undefined objectReplaceElementAtInternal(unsigned long index, object value);

  [Throws]
  undefined booleanAppendElementInternal(boolean value);
  [Throws]
  undefined interfaceAppendElementInternal(TestInterfaceObservableArray value);
  [Throws]
  undefined objectAppendElementInternal(object value);

  [Throws]
  undefined booleanRemoveLastElementInternal();
  [Throws]
  undefined interfaceRemoveLastElementInternal();
  [Throws]
  undefined objectRemoveLastElementInternal();

  [Throws]
  unsigned long booleanLengthInternal();
  [Throws]
  unsigned long interfaceLengthInternal();
  [Throws]
  unsigned long objectLengthInternal();
};
