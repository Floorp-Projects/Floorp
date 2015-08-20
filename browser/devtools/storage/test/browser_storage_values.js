/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Test to verify that the values shown in sidebar are correct

// Format: [
//   <id of the table item to click> or <id array for tree item to select> or
//     null do click nothing,
//   null to skip checking value in variables view or a key value pair object
//     which will be asserted to exist in the storage sidebar,
//   true if the check is to be made in the parsed value section
// ]

"use strict";

const testCases = [
  ["cs2", [
    {name: "cs2", value: "sessionCookie"},
    {name: "cs2.path", value: "/"},
    {name: "cs2.isDomain", value: "true"},
    {name: "cs2.isHttpOnly", value: "false"},
    {name: "cs2.host", value: ".example.org"},
    {name: "cs2.expires", value: "Session"},
    {name: "cs2.isSecure", value: "false"},
  ]],
  ["c1", [
    {name: "c1", value: JSON.stringify(["foo", "Bar", {foo: "Bar"}])},
    {name: "c1.path", value: "/browser"},
    {name: "c1.isDomain", value: "false"},
    {name: "c1.isHttpOnly", value: "false"},
    {name: "c1.host", value: "test1.example.org"},
    {name: "c1.expires", value: new Date(2000000000000).toLocaleString()},
    {name: "c1.isSecure", value: "false"},
  ]],
  [null, [
    {name: "c1", value: "Array"},
    {name: "c1.0", value: "foo"},
    {name: "c1.1", value: "Bar"},
    {name: "c1.2", value: "Object"},
    {name: "c1.2.foo", value: "Bar"},
  ], true],
  [["localStorage", "http://test1.example.org"]],
  ["ls2", [
    {name: "ls2", value: "foobar-2"}
  ]],
  ["ls1", [
    {name: "ls1", value: JSON.stringify({
      es6: "for", the: "win", baz: [0, 2, 3, {
        deep: "down",
        nobody: "cares"
      }]})}
  ]],
  [null, [
    {name: "ls1", value: "Object"},
    {name: "ls1.es6", value: "for"},
    {name: "ls1.the", value: "win"},
    {name: "ls1.baz", value: "Array"},
    {name: "ls1.baz.0", value: "0"},
    {name: "ls1.baz.1", value: "2"},
    {name: "ls1.baz.2", value: "3"},
    {name: "ls1.baz.3", value: "Object"},
    {name: "ls1.baz.3.deep", value: "down"},
    {name: "ls1.baz.3.nobody", value: "cares"},
  ], true],
  ["ls3", [
    {name: "ls3", "value": "http://foobar.com/baz.php"}
  ]],
  [null, [
    {name: "ls3", "value": "http://foobar.com/baz.php", dontMatch: true}
  ], true],
  [["sessionStorage", "http://test1.example.org"]],
  ["ss1", [
    {name: "ss1", value: "This#is#an#array"}
  ]],
  [null, [
    {name: "ss1", value: "Array"},
    {name: "ss1.0", value: "This"},
    {name: "ss1.1", value: "is"},
    {name: "ss1.2", value: "an"},
    {name: "ss1.3", value: "array"},
  ], true],
  ["ss2", [
    {name: "ss2", value: "Array"},
    {name: "ss2.0", value: "This"},
    {name: "ss2.1", value: "is"},
    {name: "ss2.2", value: "another"},
    {name: "ss2.3", value: "array"},
  ], true],
  ["ss3", [
    {name: "ss3", value: "Object"},
    {name: "ss3.this", value: "is"},
    {name: "ss3.an", value: "object"},
    {name: "ss3.foo", value: "bar"},
  ], true],
  [["indexedDB", "http://test1.example.org", "idb1", "obj1"]],
  [1, [
    {name: 1, value: JSON.stringify({id: 1, name: "foo", email: "foo@bar.com"})}
  ]],
  [null, [
    {name: "1.id", value: "1"},
    {name: "1.name", value: "foo"},
    {name: "1.email", value: "foo@bar.com"},
  ], true],
  [["indexedDB", "http://test1.example.org", "idb1", "obj2"]],
  [1, [
    {name: 1, value: JSON.stringify({
      id2: 1, name: "foo", email: "foo@bar.com", extra: "baz"
    })}
  ]],
  [null, [
    {name: "1.id2", value: "1"},
    {name: "1.name", value: "foo"},
    {name: "1.email", value: "foo@bar.com"},
    {name: "1.extra", value: "baz"},
  ], true]
];

add_task(function*() {
  yield openTabAndSetupStorage(MAIN_DOMAIN + "storage-complex-values.html");

  gUI.tree.expandAll();

  for (let item of testCases) {
    info("clicking for item " + item);

    if (Array.isArray(item[0])) {
      yield selectTreeItem(item[0]);
      continue;
    } else if (item[0]) {
      yield selectTableItem(item[0]);
    }

    yield findVariableViewProperties(item[1], item[2]);
  }

  yield finishTests();
});
