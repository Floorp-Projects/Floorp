/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI = `<h1>Top level header</h1><p>This is a paragraph.</p>`;

/**
 * Test data has the format of:
 * {
 *   desc     {String}    description for better logging
 *   setup    {Function}  An optional setup that needs to be performed before
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
          name: `""text label`,
          badges: ["text label"],
        },
      ],
      sidebar: {
        name: "",
        role: "document",
        actions: [],
        value: "",
        description: "",
        keyboardShortcut: "",
        childCount: 1,
        indexInParent: 0,
        states: ["readonly", "focusable", "opaque", "enabled", "sensitive"],
      },
    },
  },
  {
    desc: "Expand first tree node.",
    setup: ({ doc }) => toggleRow(doc, 0),
    expected: {
      tree: [
        {
          role: "document",
          name: `""text label`,
          badges: ["text label"],
        },
        {
          role: "internal frame",
          name: `"Accessibility Panel Test (OOP)"`,
        },
      ],
    },
  },
  {
    desc: "Expand second tree node. Display OOP document.",
    setup: ({ doc }) => toggleRow(doc, 1),
    expected: {
      tree: [
        {
          role: "document",
          name: `""text label`,
          badges: ["text label"],
        },
        {
          role: "internal frame",
          name: `"Accessibility Panel Test (OOP)"`,
        },
        {
          role: "document",
          name: `"Accessibility Panel Test (OOP)"`,
        },
      ],
      sidebar: {
        name: "Accessibility Panel Test (OOP)",
        role: "internal frame",
        actions: [],
        value: "",
        description: "",
        keyboardShortcut: "",
        childCount: 1,
        indexInParent: 0,
        states: ["focusable", "opaque", "enabled", "sensitive"],
      },
    },
  },
  {
    desc: "Expand third tree node. Display OOP frame content.",
    setup: ({ doc }) => toggleRow(doc, 2),
    expected: {
      tree: [
        {
          role: "document",
          name: `""text label`,
          badges: ["text label"],
        },
        {
          role: "internal frame",
          name: `"Accessibility Panel Test (OOP)"`,
        },
        {
          role: "document",
          name: `"Accessibility Panel Test (OOP)"`,
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
      sidebar: {
        name: "Accessibility Panel Test (OOP)",
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
  "Test Accessibility panel tree navigation with OOP frame.",
  { remoteIframe: true }
);
