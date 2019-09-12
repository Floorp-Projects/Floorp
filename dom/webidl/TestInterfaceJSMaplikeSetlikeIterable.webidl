/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

[Pref="dom.expose_test_interfaces"]
interface TestInterfaceMaplike {
  [Throws]
  constructor();

  maplike<DOMString, long>;
  void setInternal(DOMString aKey, long aValue);
  void clearInternal();
  boolean deleteInternal(DOMString aKey);
  boolean hasInternal(DOMString aKey);
};

[Pref="dom.expose_test_interfaces"]
interface TestInterfaceMaplikeObject {
  [Throws]
  constructor();

  readonly maplike<DOMString, TestInterfaceMaplike>;
  void setInternal(DOMString aKey);
  void clearInternal();
  boolean deleteInternal(DOMString aKey);
  boolean hasInternal(DOMString aKey);
};

[Pref="dom.expose_test_interfaces",
 JSImplementation="@mozilla.org/dom/test-interface-js-maplike;1"]
interface TestInterfaceJSMaplike {
  [Throws]
  constructor();

  readonly maplike<DOMString, long>;
  void setInternal(DOMString aKey, long aValue);
  void clearInternal();
  boolean deleteInternal(DOMString aKey);
};

[Pref="dom.expose_test_interfaces"]
interface TestInterfaceSetlike {
  [Throws]
  constructor();

  setlike<DOMString>;
};

[Pref="dom.expose_test_interfaces"]
interface TestInterfaceSetlikeNode {
  [Throws]
  constructor();

  setlike<Node>;
};

[Pref="dom.expose_test_interfaces"]
interface TestInterfaceIterableSingle {
  [Throws]
  constructor();

  iterable<long>;
  getter long(unsigned long index);
  readonly attribute unsigned long length;
};

[Pref="dom.expose_test_interfaces"]
interface TestInterfaceIterableDouble {
  [Throws]
  constructor();

  iterable<DOMString, DOMString>;
};

[Pref="dom.expose_test_interfaces"]
interface TestInterfaceIterableDoubleUnion {
  [Throws]
  constructor();

  iterable<DOMString, (DOMString or long)>;
};

