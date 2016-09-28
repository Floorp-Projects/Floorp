/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check console.table calls with all the test cases shown
// in the MDN doc (https://developer.mozilla.org/en-US/docs/Web/API/Console/table)

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/new-console-output/test/mochitest/test-console-table.html";

add_task(function* () {
  let toolbox = yield openNewTabAndToolbox(TEST_URI, "webconsole");
  let hud = toolbox.getCurrentPanel().hud;

  function Person(firstName, lastName) {
    this.firstName = firstName;
    this.lastName = lastName;
  }

  const testCases = [{
    info: "Testing when data argument is an array",
    input: ["apples", "oranges", "bananas"],
    expected: {
      columns: ["(index)", "Values"],
      rows: [
        ["0", "apples"],
        ["1", "oranges"],
        ["2", "bananas"],
      ]
    }
  }, {
    info: "Testing when data argument is an object",
    input: new Person("John", "Smith"),
    expected: {
      columns: ["(index)", "Values"],
      rows: [
        ["firstName", "John"],
        ["lastName", "Smith"],
      ]
    }
  }, {
    info: "Testing when data argument is an array of arrays",
    input: [["Jane", "Doe"], ["Emily", "Jones"]],
    expected: {
      columns: ["(index)", "0", "1"],
      rows: [
        ["0", "Jane", "Doe"],
        ["1", "Emily", "Jones"],
      ]
    }
  }, {
    info: "Testing when data argument is an array of objects",
    input: [
      new Person("Jack", "Foo"),
      new Person("Emma", "Bar"),
      new Person("Michelle", "Rax"),
    ],
    expected: {
      columns: ["(index)", "firstName", "lastName"],
      rows: [
        ["0", "Jack", "Foo"],
        ["1", "Emma", "Bar"],
        ["2", "Michelle", "Rax"],
      ]
    }
  }, {
    info: "Testing when data argument is an object whose properties are objects",
    input: {
      father: new Person("Darth", "Vader"),
      daughter: new Person("Leia", "Organa"),
      son: new Person("Luke", "Skywalker"),
    },
    expected: {
      columns: ["(index)", "firstName", "lastName"],
      rows: [
        ["father", "Darth", "Vader"],
        ["daughter", "Leia", "Organa"],
        ["son", "Luke", "Skywalker"],
      ]
    }
  }, {
    info: "Testing when data argument is a Set",
    input: new Set(["a", "b", "c"]),
    expected: {
      columns: ["(iteration index)", "Values"],
      rows: [
        ["0", "a"],
        ["1", "b"],
        ["2", "c"],
      ]
    }
  }, {
    info: "Testing when data argument is a Map",
    input: new Map([["key-a", "value-a"], ["key-b", "value-b"]]),
    expected: {
      columns: ["(iteration index)", "Key", "Values"],
      rows: [
        ["0", "key-a", "value-a"],
        ["1", "key-b", "value-b"],
      ]
    }
  }, {
    info: "Testing restricting the columns displayed",
    input: [
      new Person("Sam", "Wright"),
      new Person("Elena", "Bartz"),
    ],
    headers: ["firstName"],
    expected: {
      columns: ["(index)", "firstName"],
      rows: [
        ["0", "Sam"],
        ["1", "Elena"],
      ]
    }
  }];

  yield ContentTask.spawn(gBrowser.selectedBrowser, testCases, function (tests) {
    tests.forEach((test) => {
      content.wrappedJSObject.doConsoleTable(test.input, test.headers);
    });
  });

  let nodes = [];
  for (let testCase of testCases) {
    let node = yield waitFor(
      () => findConsoleTable(hud.ui.experimentalOutputNode, testCases.indexOf(testCase))
    );
    nodes.push(node);
  }

  let consoleTableNodes = hud.ui.experimentalOutputNode.querySelectorAll(
    ".message .new-consoletable");

  is(consoleTableNodes.length, testCases.length,
    "console has the expected number of consoleTable items");

  testCases.forEach((testCase, index) => {
    info(testCase.info);

    let node = nodes[index];
    let columns = Array.from(node.querySelectorAll("thead th"));
    let rows = Array.from(node.querySelectorAll("tbody tr"));

    is(
      JSON.stringify(testCase.expected.columns),
      JSON.stringify(columns.map(column => column.textContent)),
      "table has the expected columns"
    );

    is(testCase.expected.rows.length, rows.length,
      "table has the expected number of rows");

    testCase.expected.rows.forEach((expectedRow, rowIndex) => {
      let row = rows[rowIndex];
      let cells = row.querySelectorAll("td");
      is(expectedRow.length, cells.length, "row has the expected number of cells");

      expectedRow.forEach((expectedCell, cellIndex) => {
        let cell = cells[cellIndex];
        is(expectedCell, cell.textContent, "cell has the expected content");
      });
    });
  });
});

function findConsoleTable(node, index) {
  let condition = node.querySelector(
    `.message:nth-of-type(${index + 1}) .new-consoletable`);
  return condition;
}
