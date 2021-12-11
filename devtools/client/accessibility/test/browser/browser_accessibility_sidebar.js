/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI = `<html>
  <head>
    <meta charset="utf-8"/>
    <title>Accessibility Panel Test</title>
  </head>
  <body></body>
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
    desc: "Test the initial accessibility sidebar state.",
    expected: {
      sidebar: {
        name: "Accessibility Panel Test",
        role: "document",
        actions: [],
        value: "",
        description: "",
        keyboardShortcut: "",
        childCount: 0,
        indexInParent: 0,
        states: [
          // The focused state is an outdated state, since the toolbox should now
          // have the focus and not the content page. See Bug 1702709.
          "focused",
          "readonly",
          "focusable",
          "opaque",
          "enabled",
          "sensitive",
        ],
      },
    },
  },
  {
    desc: "Mark document as disabled for accessibility.",
    setup: async ({ browser }) =>
      SpecialPowers.spawn(browser, [], () =>
        content.document.body.setAttribute("aria-disabled", true)
      ),
    expected: {
      sidebar: {
        states: ["unavailable", "readonly", "focusable", "opaque"],
      },
    },
  },
  {
    desc: "Append a new child to the document.",
    setup: async ({ browser }) =>
      SpecialPowers.spawn(browser, [], () => {
        const doc = content.document;
        const button = doc.createElement("button");
        button.textContent = "Press Me!";
        doc.body.appendChild(button);
      }),
    expected: {
      sidebar: {
        childCount: 1,
      },
    },
  },
];

/**
 * Test that checks the Accessibility panel sidebar.
 */
addA11yPanelTestsTask(tests, TEST_URI, "Test Accessibility panel sidebar.");
