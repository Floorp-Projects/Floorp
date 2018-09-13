/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

typedef any StructuredClonable;

[ChromeOnly]
interface MozSharedMapChangeEvent : Event {
  [Cached, Constant]
  readonly attribute sequence<DOMString> changedKeys;
};

dictionary MozSharedMapChangeEventInit : EventInit {
  required sequence<DOMString> changedKeys;
};

[ChromeOnly]
interface MozSharedMap : EventTarget {
  boolean has(DOMString name);

  [Throws]
  StructuredClonable get(DOMString name);

  iterable<DOMString, StructuredClonable>;
};

[ChromeOnly]
interface MozWritableSharedMap : MozSharedMap {
  /**
   * Sets the given key to the given structured-clonable value. The value is
   * synchronously structured cloned, and the serialized value is saved in the
   * map.
   *
   * Unless flush() is called, the new value will be broadcast to content
   * processes after a short delay.
   */
  [Throws]
  void set(DOMString name, StructuredClonable value);

  /**
   * Removes the given key from the map.
   *
   * Unless flush() is called, the removal will be broadcast to content
   * processes after a short delay.
   */
  void delete(DOMString name);

  /**
   * Broadcasts any pending changes to all content processes.
   */
  void flush();
};
