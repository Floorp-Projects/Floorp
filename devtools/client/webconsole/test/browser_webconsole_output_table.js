 /* vim: set ft=javascript ts=2 et sw=2 tw=80: */
 /* Any copyright is dedicated to the Public Domain.
  http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that console.table() works as intended.

"use strict";

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
                 "test/test-console-table.html";

const TEST_DATA = [
  {
    command: "console.table(languages1)",
    data: [
        { _index: 0, name: "\"JavaScript\"", fileExtension: "Array[1]" },
        { _index: 1, name: "Object", fileExtension: "\".ts\"" },
        { _index: 2, name: "\"CoffeeScript\"", fileExtension: "\".coffee\"" }
    ],
    columns: { _index: "(index)", name: "name", fileExtension: "fileExtension" }
  },
  {
    command: "console.table(languages1, 'name')",
    data: [
        { _index: 0, name: "\"JavaScript\"", fileExtension: "Array[1]" },
        { _index: 1, name: "Object", fileExtension: "\".ts\"" },
        { _index: 2, name: "\"CoffeeScript\"", fileExtension: "\".coffee\"" }
    ],
    columns: { _index: "(index)", name: "name" }
  },
  {
    command: "console.table(languages1, ['name'])",
    data: [
        { _index: 0, name: "\"JavaScript\"", fileExtension: "Array[1]" },
        { _index: 1, name: "Object", fileExtension: "\".ts\"" },
        { _index: 2, name: "\"CoffeeScript\"", fileExtension: "\".coffee\"" }
    ],
    columns: { _index: "(index)", name: "name" }
  },
  {
    command: "console.table(languages2)",
    data: [
      { _index: "csharp", name: "\"C#\"", paradigm: "\"object-oriented\"" },
      { _index: "fsharp", name: "\"F#\"", paradigm: "\"functional\"" }
    ],
    columns: { _index: "(index)", name: "name", paradigm: "paradigm" }
  },
  {
    command: "console.table([[1, 2], [3, 4]])",
    data: [
      { _index: 0, 0: "1", 1: "2" },
      { _index: 1, 0: "3", 1: "4" }
    ],
    columns: { _index: "(index)", 0: "0", 1: "1" }
  },
  {
    command: "console.table({a: [1, 2], b: [3, 4]})",
    data: [
      { _index: "a", 0: "1", 1: "2" },
      { _index: "b", 0: "3", 1: "4" }
    ],
    columns: { _index: "(index)", 0: "0", 1: "1" }
  },
  {
    command: "console.table(family)",
    data: [
      { _index: "mother", firstName: "\"Susan\"", lastName: "\"Doyle\"",
        age: "32" },
      { _index: "father", firstName: "\"John\"", lastName: "\"Doyle\"",
        age: "33" },
      { _index: "daughter", firstName: "\"Lily\"", lastName: "\"Doyle\"",
        age: "5" },
      { _index: "son", firstName: "\"Mike\"", lastName: "\"Doyle\"", age: "8" },
    ],
    columns: { _index: "(index)", firstName: "firstName", lastName: "lastName",
               age: "age" }
  },
  {
    command: "console.table(family, [])",
    data: [
      { _index: "mother", firstName: "\"Susan\"", lastName: "\"Doyle\"",
        age: "32" },
      { _index: "father", firstName: "\"John\"", lastName: "\"Doyle\"",
        age: "33" },
      { _index: "daughter", firstName: "\"Lily\"", lastName: "\"Doyle\"",
        age: "5" },
      { _index: "son", firstName: "\"Mike\"", lastName: "\"Doyle\"", age: "8" },
    ],
    columns: { _index: "(index)" }
  },
  {
    command: "console.table(family, ['firstName', 'lastName'])",
    data: [
      { _index: "mother", firstName: "\"Susan\"", lastName: "\"Doyle\"",
        age: "32" },
      { _index: "father", firstName: "\"John\"", lastName: "\"Doyle\"",
        age: "33" },
      { _index: "daughter", firstName: "\"Lily\"", lastName: "\"Doyle\"",
        age: "5" },
      { _index: "son", firstName: "\"Mike\"", lastName: "\"Doyle\"", age: "8" },
    ],
    columns: { _index: "(index)", firstName: "firstName", lastName: "lastName" }
  },
  {
    command: "console.table(mySet)",
    data: [
      { _index: 0, _value: "1" },
      { _index: 1, _value: "5" },
      { _index: 2, _value: "\"some text\"" },
      { _index: 3, _value: "null" },
      { _index: 4, _value: "undefined" }
    ],
    columns: { _index: "(iteration index)", _value: "Values" }
  },
  {
    command: "console.table(myMap)",
    data: [
      { _index: 0, _key: "\"a string\"",
        _value: "\"value associated with 'a string'\"" },
      { _index: 1, _key: "5", _value: "\"value associated with 5\"" },
    ],
    columns: { _index: "(iteration index)", _key: "Key", _value: "Values" }
  }
];

add_task(function*() {
  const {tab} = yield loadTab(TEST_URI);
  let hud = yield openConsole(tab);

  for (let testdata of TEST_DATA) {
    hud.jsterm.clearOutput();

    info("Executing " + testdata.command);

    let onTableRender = once(hud.ui, "messages-table-rendered");
    hud.jsterm.execute(testdata.command);
    yield onTableRender;

    let [result] = yield waitForMessages({
      webconsole: hud,
      messages: [{
        name: testdata.command + " output",
        consoleTable: true
      }],
    });

    let node = [...result.matched][0];
    ok(node, "found trace log node");

    let obj = node._messageObject;
    ok(obj, "console.trace message object");

    ok(obj._data, "found table data object");

    let data = obj._data.map(entries => {
      let entryResult = {};

      for (let key of Object.keys(entries)) {
        entryResult[key] = entries[key] instanceof HTMLElement ?
          entries[key].textContent : entries[key];
      }

      return entryResult;
    });

    is(data.toSource(), testdata.data.toSource(), "table data is correct");
    ok(obj._columns, "found table column object");
    is(obj._columns.toSource(), testdata.columns.toSource(),
       "table column is correct");
  }
});
