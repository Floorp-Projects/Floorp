/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI = `<html>
  <head>
    <meta charset="utf-8"/>
    <title>Accessibility Panel Test</title>
  </head>
  <body>
    <h1>Top level header</h1>
    <p>This is a paragraph.</p>
  </body>
</html>`;

/**
 * Test data has the format of:
 * {
 *   desc     {String}    description for better logging
 *   setup   {Function}  An optional setup that needs to be performed before
 *                        the state of the tree and the sidebar can be checked.
 *   expected {JSON}      An expected states for the tree and the sidebar.
 * }
 */
const tests = [
  {
    desc: "Test the initial accessibility tree and sidebar states.",
    expected: {
      tree: [
        {
          role: "document",
          name: `"Accessibility Panel Test"`,
        },
      ],
      sidebar: {
        name: "Accessibility Panel Test",
        role: "document",
        actions: [],
        value: "",
        description: "",
        keyboardShortcut: "",
        childCount: 2,
        indexInParent: 0,
        states: ["readonly", "focusable", "opaque", "enabled", "sensitive"],
      },
    },
  },
  {
    desc: "Expand first tree node.",
    setup: async ({ doc }) => toggleRow(doc, 0),
    expected: {
      tree: [
        {
          role: "document",
          name: `"Accessibility Panel Test"`,
        },
        {
          role: "heading",
          name: `"Top level header"`,
        },
        {
          role: "paragraph",
          name: `""`,
        },
      ],
    },
  },
  {
    desc: "Select second tree node.",
    setup: async ({ doc }) => selectRow(doc, 1),
    expected: {
      sidebar: {
        name: "Top level header",
        role: "heading",
        actions: [],
        value: "",
        description: "",
        keyboardShortcut: "",
        childCount: 1,
        indexInParent: 0,
        relations: {
          "containing document": {
            role: "document",
            name: "Accessibility Panel Test",
          },
        },
        states: ["selectable text", "opaque", "enabled", "sensitive"],
      },
    },
  },
  {
    desc: "Select containing document.",
    setup: async ({ doc, win }) => {
      const relations = await selectProperty(doc, "/relations");
      EventUtils.sendMouseEvent(
        { type: "click" },
        relations.querySelector(".arrow"),
        win
      );
      const containingDocRelation = await selectProperty(
        doc,
        "/relations/containing document"
      );
      EventUtils.sendMouseEvent(
        { type: "click" },
        containingDocRelation.querySelector(".open-accessibility-inspector"),
        win
      );
    },
    expected: {
      sidebar: {
        name: "Accessibility Panel Test",
        role: "document",
        actions: [],
        value: "",
        description: "",
        keyboardShortcut: "",
        childCount: 2,
        indexInParent: 0,
        states: ["readonly", "focusable", "opaque", "enabled", "sensitive"],
      },
    },
  },
];

/**
 * Check navigation within the tree.
 */
addA11yPanelTestsTask(
  tests,
  TEST_URI,
  "Test Accessibility panel relation navigation."
);
