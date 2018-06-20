/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI = `<html>
  <head>
    <meta charset="utf-8"/>
    <title>Accessibility Panel Test</title>
  </head>
  <body>
    <h1 id="h1">Top level header</h1>
    <p id="p">This is a paragraph.</p>
  </body>
</html>`;

/**
 * Test data has the format of:
 * {
 *   desc     {String}    description for better logging
 *   action   {Function}  An optional action that needs to be performed before
 *                        the state of the tree and the sidebar can be checked.
 *   expected {JSON}      An expected states for the tree and the sidebar.
 * }
 */
const tests = [{
  desc: "Expand first and second rows, select third row.",
  action: async ({ doc }) => {
    await toggleRow(doc, 0);
    await toggleRow(doc, 1);
    selectRow(doc, 3);
  },
  expected: {
    tree: [{
      role: "document",
      name: `"Accessibility Panel Test"`
    }, {
      role: "heading",
      name: `"Top level header"`
    }, {
      role: "text leaf",
      name: `"Top level header"`
    }, {
      role: "paragraph",
      name: `""`
    }],
    sidebar: {
      name: null,
      role: "paragraph",
      actions: [],
      value: "",
      description: "",
      keyboardShortcut: "",
      childCount: 1,
      indexInParent: 1,
      states: ["selectable text", "opaque", "enabled", "sensitive"]
    }
  }
}, {
  desc: "Remove a child from a document.",
  action: async ({ doc, browser }) => {
    is(doc.querySelectorAll(".treeRow").length, 4, "Tree size is correct.");
    await ContentTask.spawn(browser, {}, () =>
      content.document.getElementById("p").remove());
    await BrowserTestUtils.waitForCondition(() =>
      doc.querySelectorAll(".treeRow").length === 3, "Tree updated.");
  },
  expected: {
    tree: [{
      role: "document",
      name: `"Accessibility Panel Test"`
    }, {
      role: "heading",
      name: `"Top level header"`
    }, {
      role: "text leaf",
      name: `"Top level header"`
    }],
    sidebar: {
      name: "Top level header",
      role: "text leaf"
    }
  }
}, {
  desc: "Update child's text content.",
  action: async ({ browser }) => {
    await ContentTask.spawn(browser, {}, () => {
      content.document.getElementById("h1").textContent = "New Header";
    });
  },
  expected: {
    tree: [{
      role: "document",
      name: `"Accessibility Panel Test"`
    }, {
      role: "heading",
      name: `"New Header"`
    }, {
      role: "text leaf",
      name: `"New Header"`
    }]
  }
}, {
  desc: "Select third row in the tree.",
  action: ({ doc }) => selectRow(doc, 1),
  expected: {
    sidebar: {
      name: "New Header"
    }
  }
}];

/**
 * Test that checks the Accessibility panel after DOM tree mutations.
 */
addA11yPanelTestsTask(tests, TEST_URI,
  "Test Accessibility panel after DOM tree mutations.");
