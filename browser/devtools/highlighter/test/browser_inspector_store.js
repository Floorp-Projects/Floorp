/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Inspector Store Tests.
 *
 * The Initial Developer of the Original Code is
 * The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mihai Șucan <mihai.sucan@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

function test()
{
  let InspectorStore = InspectorUI.store;

  is(InspectorStore.length, 0, "InspectorStore is empty");
  ok(InspectorStore.isEmpty(), "InspectorStore is empty (confirmed)");
  is(typeof InspectorStore.store, "object",
    "InspectorStore.store is an object");

  ok(InspectorStore.addStore("foo"), "addStore('foo') returns true");

  is(InspectorStore.length, 1, "InspectorStore.length = 1");
  ok(!InspectorStore.isEmpty(), "InspectorStore is not empty");
  is(typeof InspectorStore.store.foo, "object", "store.foo is an object");

  ok(InspectorStore.addStore("fooBar"), "addStore('fooBar') returns true");

  is(InspectorStore.length, 2, "InspectorStore.length = 2");
  is(typeof InspectorStore.store.fooBar, "object", "store.fooBar is an object");

  ok(!InspectorStore.addStore("fooBar"), "addStore('fooBar') returns false");

  ok(InspectorStore.deleteStore("fooBar"),
    "deleteStore('fooBar') returns true");

  is(InspectorStore.length, 1, "InspectorStore.length = 1");
  ok(!InspectorStore.store.fooBar, "store.fooBar is deleted");

  ok(!InspectorStore.deleteStore("fooBar"),
    "deleteStore('fooBar') returns false");

  ok(!InspectorStore.hasID("fooBar"), "hasID('fooBar') returns false");

  ok(InspectorStore.hasID("foo"), "hasID('foo') returns true");

  ok(InspectorStore.setValue("foo", "key1", "val1"), "setValue() returns true");

  ok(!InspectorStore.setValue("fooBar", "key1", "val1"),
    "setValue() returns false");

  is(InspectorStore.getValue("foo", "key1"), "val1",
    "getValue() returns the correct value");

  is(InspectorStore.store.foo.key1, "val1", "store.foo.key1 = 'val1'");

  ok(!InspectorStore.getValue("fooBar", "key1"),
    "getValue() returns null for unknown store");

  ok(!InspectorStore.getValue("fooBar", "key1"),
    "getValue() returns null for unknown store");

  ok(InspectorStore.deleteValue("foo", "key1"),
    "deleteValue() returns true for known value");

  ok(!InspectorStore.store.foo.key1, "deleteValue() removed the value.");

  ok(!InspectorStore.deleteValue("fooBar", "key1"),
    "deleteValue() returns false for unknown store.");

  ok(!InspectorStore.deleteValue("foo", "key1"),
    "deleteValue() returns false for unknown value.");

  ok(InspectorStore.deleteStore("foo"), "deleteStore('foo') returns true");

  ok(InspectorStore.isEmpty(), "InspectorStore is empty");
}

