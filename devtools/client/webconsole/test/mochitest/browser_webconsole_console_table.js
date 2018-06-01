/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check console.table calls with all the test cases shown
// in the MDN doc (https://developer.mozilla.org/en-US/docs/Web/API/Console/table)

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
                 "test/mochitest/test-console-table.html";

add_task(async function() {
  const toolbox = await openNewTabAndToolbox(TEST_URI, "webconsole");
  const hud = toolbox.getCurrentPanel().hud;

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
  }, {
    info: "Testing nested object with falsy values",
    input: [
      {a: null, b: false, c: undefined, d: 0},
      {b: null, c: false, d: undefined, e: 0}
    ],
    expected: {
      columns: ["(index)", "a", "b", "c", "d", "e"],
      rows: [
        ["0", "null", "false", "undefined", "0", "undefined"],
        ["1", "undefined", "null", "false", "undefined", "0"],
      ]
    }
  }];

  await ContentTask.spawn(gBrowser.selectedBrowser, testCases, function(tests) {
    tests.forEach((test) => {
      content.wrappedJSObject.doConsoleTable(test.input, test.headers);
    });
  });
  const nodes = [];
  for (const testCase of testCases) {
    const node = await waitFor(
      () => findConsoleTable(hud.ui.outputNode, testCases.indexOf(testCase))
    );
    nodes.push(node);
  }
  const consoleTableNodes = hud.ui.outputNode.querySelectorAll(
    ".message .new-consoletable");
  is(consoleTableNodes.length, testCases.length,
    "console has the expected number of consoleTable items");
  testCases.forEach((testCase, index) => testItem(testCase, nodes[index]));
});
function testItem(testCase, node) {
  info(testCase.info);

  const columns = Array.from(node.querySelectorAll("[role=columnheader]"));
  const columnsNumber = columns.length;
  const cells = Array.from(node.querySelectorAll("[role=gridcell]"));

  is(
    JSON.stringify(testCase.expected.columns),
    JSON.stringify(columns.map(column => column.textContent)),
    "table has the expected columns"
  );

  // We don't really have rows since we are using a CSS grid in order to have a sticky
  // header on the table. So we check the "rows" by dividing the number of cells by the
  // number of columns.
  is(testCase.expected.rows.length, cells.length / columnsNumber,
    "table has the expected number of rows");

  testCase.expected.rows.forEach((expectedRow, rowIndex) => {
    const startIndex = rowIndex * columnsNumber;
    // Slicing the cells array so we can get the current "row".
    const rowCells = cells.slice(startIndex, startIndex + columnsNumber);
    is(rowCells.map(x => x.textContent).join(" | "), expectedRow.join(" | "));
  });
}

function findConsoleTable(node, index) {
  const condition = node.querySelector(
    `.message:nth-of-type(${index + 1}) .new-consoletable`);
  return condition;
}
