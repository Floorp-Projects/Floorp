/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Test to verify that the keys shown in sidebar are correct

// Format of the test cases: {
//   action: Either "selectTreeItem" to select a tree item or
//     "assertTableItem" to select a table item,
//   ids: ID array for tree item to select if `action` is "selectTreeItem",
//   id: ID of the table item if `action` is "assertTableItem",
//   keyValuePairs: Array of key value pair objects which will be asserted
//     to exist in the storage sidebar (optional)
// }

"use strict";

const LONG_WORD = "a".repeat(1000);

add_task(async function() {
  await openTabAndSetupStorage(MAIN_DOMAIN + "storage-complex-keys.html");

  gUI.tree.expandAll();

  await testLocalStorage();
  await testSessionStorage();
  await testIndexedDB();
});

async function testLocalStorage() {
  const tests = [
    {
      action: "selectTreeItem",
      ids: ["localStorage", "http://test1.example.org"],
    },
    {
      action: "assertTableItem",
      id: "",
      value: "1",
    },
    {
      action: "assertTableItem",
      id: "键",
      value: "2",
    },
  ];

  await makeTests(tests);
}

async function testSessionStorage() {
  const tests = [
    {
      action: "selectTreeItem",
      ids: ["sessionStorage", "http://test1.example.org"],
    },
    {
      action: "assertTableItem",
      id: "Key with spaces",
      value: "3",
    },
    {
      action: "assertTableItem",
      id: "Key#with~special$characters",
      value: "4",
    },
    {
      action: "assertTableItem",
      id: LONG_WORD,
      value: "5",
    },
  ];

  await makeTests(tests);
}

async function testIndexedDB() {
  const tests = [
    {
      action: "selectTreeItem",
      ids: ["indexedDB", "http://test1.example.org", "idb (default)", "obj"],
    },
    {
      action: "assertTableItem",
      id: "",
      value: JSON.stringify({ id: "", name: "foo" }),
      keyValuePairs: [
        { name: ".id", value: "" },
        { name: ".name", value: "foo" },
      ],
    },
    {
      action: "assertTableItem",
      id: "键",
      value: JSON.stringify({ id: "键", name: "foo2" }),
      keyValuePairs: [
        { name: "键.id", value: "键" },
        { name: "键.name", value: "foo2" },
      ],
    },
    {
      action: "assertTableItem",
      id: "Key with spaces",
      value: JSON.stringify({ id: "Key with spaces", name: "foo3" }),
      keyValuePairs: [
        { name: "Key with spaces.id", value: "Key with spaces" },
        { name: "Key with spaces.name", value: "foo3" },
      ],
    },
    {
      action: "assertTableItem",
      id: "Key#with~special$characters",
      value: JSON.stringify({
        id: "Key#with~special$characters",
        name: "foo4",
      }),
      keyValuePairs: [
        {
          name: "Key#with~special$characters.id",
          value: "Key#with~special$characters",
        },
        { name: "Key#with~special$characters.name", value: "foo4" },
      ],
    },
    {
      action: "assertTableItem",
      id: LONG_WORD,
      value: JSON.stringify({ id: LONG_WORD, name: "foo5" }),
      keyValuePairs: [
        { name: `${LONG_WORD}.id`, value: LONG_WORD },
        { name: `${LONG_WORD}.name`, value: "foo5" },
      ],
    },
  ];

  await makeTests(tests);
}

async function makeTests(tests) {
  for (const item of tests) {
    info(`Selecting item ${JSON.stringify(item)}`);

    switch (item.action) {
      case "selectTreeItem":
        await selectTreeItem(item.ids);
        break;

      case "assertTableItem":
        await selectTableItem(item.id);
        // Check the ID and value in the data section
        await findVariableViewProperties([
          { name: item.id, value: item.value },
        ]);
        // If there are key value pairs defined, check those in the
        // parsed value section
        if (item.keyValuePairs) {
          await findVariableViewProperties(item.keyValuePairs, true);
        }
        break;
    }
  }
}
