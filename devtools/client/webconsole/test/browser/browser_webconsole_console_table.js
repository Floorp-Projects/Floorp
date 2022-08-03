/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check console.table calls with all the test cases shown
// in the MDN doc (https://developer.mozilla.org/en-US/docs/Web/API/Console/table)

const TEST_URI =
  "http://example.com/browser/devtools/client/webconsole/" +
  "test/browser/test-console-table.html";

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);

  function Person(firstName, lastName) {
    this.firstName = firstName;
    this.lastName = lastName;
  }

  const holeyArray = [];
  holeyArray[1] = "apples";
  holeyArray[3] = "oranges";
  holeyArray[6] = "bananas";

  const testCases = [
    {
      info: "Testing when data argument is an array",
      input: ["apples", "oranges", "bananas"],
      expected: {
        columns: ["(index)", "Values"],
        rows: [
          ["0", "apples"],
          ["1", "oranges"],
          ["2", "bananas"],
        ],
      },
    },
    {
      info: "Testing when data argument is an holey array",
      input: holeyArray,
      expected: {
        columns: ["(index)", "Values"],
        rows: [
          ["0", ""],
          ["1", "apples"],
          ["2", ""],
          ["3", "oranges"],
          ["4", ""],
          ["5", ""],
          ["6", "bananas"],
        ],
      },
    },
    {
      info: "Testing when data argument has holey array",
      // eslint-disable-next-line no-sparse-arrays
      input: [[1, , 2]],
      expected: {
        columns: ["(index)", "0", "1", "2"],
        rows: [["0", "1", "", "2"]],
      },
    },
    {
      info: "Testing when data argument is an object",
      input: new Person("John", "Smith"),
      expected: {
        columns: ["(index)", "Values"],
        rows: [
          ["firstName", "John"],
          ["lastName", "Smith"],
        ],
      },
    },
    {
      info: "Testing when data argument is an array of arrays",
      input: [
        ["Jane", "Doe"],
        ["Emily", "Jones"],
      ],
      expected: {
        columns: ["(index)", "0", "1"],
        rows: [
          ["0", "Jane", "Doe"],
          ["1", "Emily", "Jones"],
        ],
      },
    },
    {
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
        ],
      },
    },
    {
      info:
        "Testing when data argument is an object whose properties are objects",
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
        ],
      },
    },
    {
      info: "Testing when data argument is a Set",
      input: new Set(["a", "b", "c"]),
      expected: {
        columns: ["(iteration index)", "Values"],
        rows: [
          ["0", "a"],
          ["1", "b"],
          ["2", "c"],
        ],
      },
    },
    {
      info: "Testing when data argument is a Map",
      input: new Map([
        ["key-a", "value-a"],
        ["key-b", "value-b"],
      ]),
      expected: {
        columns: ["(iteration index)", "Key", "Values"],
        rows: [
          ["0", "key-a", "value-a"],
          ["1", "key-b", "value-b"],
        ],
      },
    },
    {
      info: "Testing when data argument is a Int8Array",
      input: new Int8Array([1, 2, 3, 4]),
      expected: {
        columns: ["(index)", "Values"],
        rows: [
          ["0", "1"],
          ["1", "2"],
          ["2", "3"],
          ["3", "4"],
        ],
      },
    },
    {
      info: "Testing when data argument is a Uint8Array",
      input: new Uint8Array([1, 2, 3, 4]),
      expected: {
        columns: ["(index)", "Values"],
        rows: [
          ["0", "1"],
          ["1", "2"],
          ["2", "3"],
          ["3", "4"],
        ],
      },
    },
    {
      info: "Testing when data argument is a Int16Array",
      input: new Int16Array([1, 2, 3, 4]),
      expected: {
        columns: ["(index)", "Values"],
        rows: [
          ["0", "1"],
          ["1", "2"],
          ["2", "3"],
          ["3", "4"],
        ],
      },
    },
    {
      info: "Testing when data argument is a Uint16Array",
      input: new Uint16Array([1, 2, 3, 4]),
      expected: {
        columns: ["(index)", "Values"],
        rows: [
          ["0", "1"],
          ["1", "2"],
          ["2", "3"],
          ["3", "4"],
        ],
      },
    },
    {
      info: "Testing when data argument is a Int32Array",
      input: new Int32Array([1, 2, 3, 4]),
      expected: {
        columns: ["(index)", "Values"],
        rows: [
          ["0", "1"],
          ["1", "2"],
          ["2", "3"],
          ["3", "4"],
        ],
      },
    },
    {
      info: "Testing when data argument is a Uint32Array",
      input: new Uint32Array([1, 2, 3, 4]),
      expected: {
        columns: ["(index)", "Values"],
        rows: [
          ["0", "1"],
          ["1", "2"],
          ["2", "3"],
          ["3", "4"],
        ],
      },
    },
    {
      info: "Testing when data argument is a Float32Array",
      input: new Float32Array([1, 2, 3, 4]),
      expected: {
        columns: ["(index)", "Values"],
        rows: [
          ["0", "1"],
          ["1", "2"],
          ["2", "3"],
          ["3", "4"],
        ],
      },
    },
    {
      info: "Testing when data argument is a Float64Array",
      input: new Float64Array([1, 2, 3, 4]),
      expected: {
        columns: ["(index)", "Values"],
        rows: [
          ["0", "1"],
          ["1", "2"],
          ["2", "3"],
          ["3", "4"],
        ],
      },
    },
    {
      info: "Testing when data argument is a Uint8ClampedArray",
      input: new Uint8ClampedArray([1, 2, 3, 4]),
      expected: {
        columns: ["(index)", "Values"],
        rows: [
          ["0", "1"],
          ["1", "2"],
          ["2", "3"],
          ["3", "4"],
        ],
      },
    },
    {
      info: "Testing when data argument is a BigInt64Array",
      // eslint-disable-next-line no-undef
      input: new BigInt64Array([1n, 2n, 3n, 4n]),
      expected: {
        columns: ["(index)", "Values"],
        rows: [
          ["0", "1n"],
          ["1", "2n"],
          ["2", "3n"],
          ["3", "4n"],
        ],
      },
    },
    {
      info: "Testing when data argument is a BigUint64Array",
      // eslint-disable-next-line no-undef
      input: new BigUint64Array([1n, 2n, 3n, 4n]),
      expected: {
        columns: ["(index)", "Values"],
        rows: [
          ["0", "1n"],
          ["1", "2n"],
          ["2", "3n"],
          ["3", "4n"],
        ],
      },
    },
    {
      info: "Testing restricting the columns displayed",
      input: [new Person("Sam", "Wright"), new Person("Elena", "Bartz")],
      headers: ["firstName"],
      expected: {
        columns: ["(index)", "firstName"],
        rows: [
          ["0", "Sam"],
          ["1", "Elena"],
        ],
      },
    },
    {
      info: "Testing nested object with falsy values",
      input: [
        { a: null, b: false, c: undefined, d: 0 },
        { b: null, c: false, d: undefined, e: 0 },
      ],
      expected: {
        columns: ["(index)", "a", "b", "c", "d", "e"],
        rows: [
          ["0", "null", "false", "undefined", "0", ""],
          ["1", "", "null", "false", "undefined", "0"],
        ],
      },
    },
    {
      info: "Testing invalid headers",
      input: ["apples", "oranges", "bananas"],
      headers: [[]],
      expected: {
        columns: ["(index)", "Values"],
        rows: [
          ["0", "apples"],
          ["1", "oranges"],
          ["2", "bananas"],
        ],
      },
    },
    {
      info: "Testing overflow-y",
      input: Array.from({ length: 50 }, (_, i) => `item-${i}`),
      expected: {
        columns: ["(index)", "Values"],
        rows: Array.from({ length: 50 }, (_, i) => [i.toString(), `item-${i}`]),
        overflow: true,
      },
    },
    {
      info: "Testing table with expandable objects",
      input: [{ a: { b: 34 } }],
      expected: {
        columns: ["(index)", "a"],
        rows: [["0", "Object { b: 34 }"]],
      },
      async additionalTest(node) {
        info("Check that object in a cell can be expanded");
        const objectNode = node.querySelector(".tree .node");
        objectNode.click();
        await waitFor(() => node.querySelectorAll(".tree .node").length === 3);
        const nodes = node.querySelectorAll(".tree .node");
        ok(nodes[1].textContent.includes("b: 34"));
        ok(nodes[2].textContent.includes("<prototype>"));
      },
    },
    {
      info: "Testing max columns",
      input: [
        Array.from({ length: 30 }).reduce((acc, _, i) => {
          return {
            ...acc,
            ["item" + i]: i,
          };
        }, {}),
      ],
      expected: {
        // We show 21 columns at most
        columns: [
          "(index)",
          ...Array.from({ length: 20 }, (_, i) => `item${i}`),
        ],
        rows: [[0, ...Array.from({ length: 20 }, (_, i) => i)]],
      },
    },
    {
      info: "Testing performance entries",
      input: "PERFORMANCE_ENTRIES",
      headers: [
        "name",
        "entryType",
        "initiatorType",
        "connectStart",
        "connectEnd",
        "fetchStart",
      ],
      expected: {
        columns: [
          "(index)",
          "initiatorType",
          "fetchStart",
          "connectStart",
          "connectEnd",
          "name",
          "entryType",
        ],
        rows: [[0, "navigation", /\d+/, /\d+/, /\d+/, TEST_URI, "navigation"]],
      },
    },
  ];

  await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [testCases.map(({ input, headers }) => ({ input, headers }))],
    function(tests) {
      tests.forEach(test => {
        let { input, headers } = test;
        if (input === "PERFORMANCE_ENTRIES") {
          input = content.wrappedJSObject.performance.getEntriesByType(
            "navigation"
          );
        }
        content.wrappedJSObject.doConsoleTable(input, headers);
      });
    }
  );
  const messages = await waitFor(async () => {
    const msgs = await findAllMessagesVirtualized(hud);
    if (msgs.length === testCases.length) {
      return msgs;
    }
    return null;
  });
  for (const [index, testCase] of testCases.entries()) {
    // Refresh the reference to the message, as it may have been scrolled out of existence.
    const node = await findMessageVirtualizedById({
      hud,
      messageId: messages[index].getAttribute("data-message-id"),
    });
    await testItem(testCase, node.querySelector(".consoletable"));
  }
});

async function testItem(testCase, node) {
  info(testCase.info);

  const columns = Array.from(node.querySelectorAll("[role=columnheader]"));
  const columnsNumber = columns.length;
  const cells = Array.from(node.querySelectorAll("[role=gridcell]"));

  is(
    JSON.stringify(columns.map(column => column.textContent)),
    JSON.stringify(testCase.expected.columns),
    `${testCase.info} | table has the expected columns`
  );

  // We don't really have rows since we are using a CSS grid in order to have a sticky
  // header on the table. So we check the "rows" by dividing the number of cells by the
  // number of columns.
  is(
    cells.length / columnsNumber,
    testCase.expected.rows.length,
    `${testCase.info} | table has the expected number of rows`
  );

  testCase.expected.rows.forEach((expectedRow, rowIndex) => {
    const startIndex = rowIndex * columnsNumber;
    // Slicing the cells array so we can get the current "row".
    const rowCells = cells
      .slice(startIndex, startIndex + columnsNumber)
      .map(x => x.textContent);

    const isRegex = x => x && x.constructor.name === "RegExp";
    const hasRegExp = expectedRow.find(isRegex);
    if (hasRegExp) {
      is(
        rowCells.length,
        expectedRow.length,
        `${testCase.info} | row ${rowIndex} has the expected number of cell`
      );
      rowCells.forEach((cell, i) => {
        const expected = expectedRow[i];
        const info = `${testCase.info} | row ${rowIndex} cell ${i} has the expected content`;

        if (isRegex(expected)) {
          ok(expected.test(cell), info);
        } else {
          is(cell, `${expected}`, info);
        }
      });
    } else {
      is(
        rowCells.join(" | "),
        expectedRow.join(" | "),
        `${testCase.info} | row has the expected content`
      );
    }
  });

  if (testCase.expected.overflow) {
    ok(
      node.isConnected,
      "Node must be connected to test overflow. It is likely scrolled out of view."
    );
    ok(
      node.scrollHeight > node.clientHeight,
      testCase.info + " table overflows"
    );
    ok(getComputedStyle(node).overflowY !== "hidden", "table can be scrolled");
  }

  if (typeof testCase.additionalTest === "function") {
    await testCase.additionalTest(node);
  }
}
