/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

[Pref="dom.expose_test_interfaces",
 Exposed=Window]
interface TestInterfaceMaplike {
  [Throws]
  constructor();

  maplike<DOMString, long>;
  void setInternal(DOMString aKey, long aValue);
  void clearInternal();
  boolean deleteInternal(DOMString aKey);
  boolean hasInternal(DOMString aKey);
  [Throws]
  long getInternal(DOMString aKey);
};

[Pref="dom.expose_test_interfaces",
 Exposed=Window]
interface TestInterfaceMaplikeObject {
  [Throws]
  constructor();

  readonly maplike<DOMString, TestInterfaceMaplike>;
  void setInternal(DOMString aKey);
  void clearInternal();
  boolean deleteInternal(DOMString aKey);
  boolean hasInternal(DOMString aKey);
  [Throws]
  TestInterfaceMaplike? getInternal(DOMString aKey);
};

[Pref="dom.expose_test_interfaces",
 Exposed=Window]
interface TestInterfaceMaplikeJSObject {
  [Throws]
  constructor();

  readonly maplike<DOMString, object>;
  void setInternal(DOMString aKey, object aObject);
  void clearInternal();
  boolean deleteInternal(DOMString aKey);
  boolean hasInternal(DOMString aKey);
  [Throws]
  object? getInternal(DOMString aKey);
};

[Pref="dom.expose_test_interfaces",
 Exposed=Window]
interface TestInterfaceSetlike {
  [Throws]
  constructor();

  setlike<DOMString>;
};

[Pref="dom.expose_test_interfaces",
 Exposed=Window]
interface TestInterfaceSetlikeNode {
  [Throws]
  constructor();

  setlike<Node>;
};

[Pref="dom.expose_test_interfaces",
 Exposed=Window]
interface TestInterfaceIterableSingle {
  [Throws]
  constructor();

  iterable<long>;
  getter long(unsigned long index);
  readonly attribute unsigned long length;
};

[Pref="dom.expose_test_interfaces",
 Exposed=Window]
interface TestInterfaceIterableDouble {
  [Throws]
  constructor();

  iterable<DOMString, DOMString>;
};

[Pref="dom.expose_test_interfaces",
 Exposed=Window]
interface TestInterfaceIterableDoubleUnion {
  [Throws]
  constructor();

  iterable<DOMString, (DOMString or long)>;
};

[Pref="dom.expose_test_interfaces",
 Exposed=Window]
interface TestInterfaceAsyncIterableSingle {
  [Throws]
  constructor();

  async iterable<long>;
};

[Pref="dom.expose_test_interfaces",
 Exposed=Window]
interface TestInterfaceAsyncIterableDouble {
  [Throws]
  constructor();

  async iterable<DOMString, DOMString>;
};

[Pref="dom.expose_test_interfaces",
 Exposed=Window]
interface TestInterfaceAsyncIterableDoubleUnion {
  [Throws]
  constructor();

  async iterable<DOMString, (DOMString or long)>;
};
