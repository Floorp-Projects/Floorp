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

const documentRow = {
  role: "document",
  name: `"Accessibility Panel Test"`,
};
const documentRowOOP = {
  role: "document",
  name: `""text label`,
  badges: ["text label"],
};
const subtree = [
  {
    role: "heading",
    name: `"Top level header"`,
  },
  {
    role: "text leaf",
    name: `"Top level header"`,
  },
  {
    role: "paragraph",
    name: `""`,
  },
];
const frameSubtree = [
  { role: "internal frame", name: `"Accessibility Panel Test (OOP)"` },
  {
    role: "document",
    name: `"Accessibility Panel Test (OOP)"`,
  },
];
const subtreeOOP = [...frameSubtree, ...subtree];
const renamed = [
  {
    role: "heading",
    name: `"New Header"`,
  },
  {
    role: "text leaf",
    name: `"New Header"`,
  },
];
const paragraphSidebar = {
  name: null,
  role: "paragraph",
  actions: [],
  value: "",
  description: "",
  keyboardShortcut: "",
  childCount: 1,
  indexInParent: 1,
  states: ["selectable text", "opaque", "enabled", "sensitive"],
};
const headerSidebar = {
  name: "Top level header",
  role: "text leaf",
};
const newHeaderSidebar = {
  name: "New Header",
};

function removeRow(rowNumber) {
  return async ({ doc, browser }) => {
    is(
      doc.querySelectorAll(".treeRow").length,
      rowNumber,
      "Tree size is correct."
    );
    await SpecialPowers.spawn(browser, [], async () => {
      const iframe = content.document.getElementsByTagName("iframe")[0];
      if (iframe) {
        await SpecialPowers.spawn(iframe, [], () =>
          content.document.getElementById("p").remove()
        );
        return;
      }

      content.document.getElementById("p").remove();
    });
    await BrowserTestUtils.waitForCondition(
      () => doc.querySelectorAll(".treeRow").length === rowNumber - 1,
      "Tree updated."
    );
  };
}

async function rename({ browser }) {
  await SpecialPowers.spawn(browser, [], async () => {
    const iframe = content.document.getElementsByTagName("iframe")[0];
    if (iframe) {
      await SpecialPowers.spawn(
        iframe,
        [],
        () => (content.document.getElementById("h1").textContent = "New Header")
      );
      return;
    }

    content.document.getElementById("h1").textContent = "New Header";
  });
}

/**
 * Test data has the format of:
 * {
 *   desc     {String}    description for better logging
 *   setup   {Function}  An optional setup that needs to be performed before
 *                        the state of the tree and the sidebar can be checked.
 *   expected {JSON}      An expected states for the tree and the sidebar.
 * }
 */
const testsTopLevel = [
  {
    desc: "Expand first and second rows, select third row.",
    setup: async ({ doc }) => {
      await toggleRow(doc, 0);
      await toggleRow(doc, 1);
      selectRow(doc, 3);
    },
    expected: {
      tree: [documentRow, ...subtree],
      sidebar: paragraphSidebar,
    },
  },
  {
    desc: "Remove a child from a document.",
    setup: removeRow(4),
    expected: {
      tree: [documentRow, ...subtree.slice(0, -1)],
      sidebar: headerSidebar,
    },
  },
  {
    desc: "Update child's text content.",
    setup: rename,
    expected: {
      tree: [documentRow, ...renamed],
    },
  },
  {
    desc: "Select third row in the tree.",
    setup: ({ doc }) => selectRow(doc, 1),
    expected: {
      sidebar: newHeaderSidebar,
    },
  },
];

const testsOOP = [
  {
    desc: "Expand rows until we reach an internal OOP frame.",
    setup: async ({ doc }) => {
      await toggleRow(doc, 0);
      await toggleRow(doc, 1);
      await toggleRow(doc, 2);
      await toggleRow(doc, 3);
      selectRow(doc, 5);
    },
    expected: {
      tree: [documentRowOOP, ...subtreeOOP],
      sidebar: paragraphSidebar,
    },
  },
  {
    desc: "Remove a child from a document.",
    setup: removeRow(6),
    expected: {
      tree: [documentRowOOP, ...subtreeOOP.slice(0, -1)],
      sidebar: headerSidebar,
    },
  },
  {
    desc: "Update child's text content.",
    setup: rename,
    expected: {
      tree: [documentRowOOP, ...frameSubtree, ...renamed],
    },
  },
  {
    desc: "Select third row in the tree.",
    setup: ({ doc }) => selectRow(doc, 1),
    expected: {
      sidebar: newHeaderSidebar,
    },
  },
];

/**
 * Tests that checks the Accessibility panel after DOM tree mutations.
 */
addA11yPanelTestsTask(
  testsTopLevel,
  TEST_URI,
  "Test Accessibility panel after DOM tree mutations."
);

addA11yPanelTestsTask(
  testsOOP,
  TEST_URI,
  "Test Accessibility panel after DOM tree mutations in the OOP frame.",
  { remoteIframe: true }
);
