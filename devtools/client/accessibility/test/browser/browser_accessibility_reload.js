/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI_1 = `<html>
  <head>
    <meta charset="utf-8"/>
    <title>Accessibility Panel Test</title>
  </head>
  <body>
    <h1>Top level header</h1>
    <p>This is a paragraph.</p>
  </body>
</html>`;

const TEST_URI_2 = `<html>
  <head>
    <meta charset="utf-8"/>
    <title>Navigation Accessibility Panel</title>
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
    desc:
      "Test the initial accessibility tree state after first row is expanded.",
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
      sidebar: {
        name: "Accessibility Panel Test",
        role: "document",
      },
    },
  },
  {
    desc: "Reload the page.",
    setup: async ({ panel }) => {
      const onReloaded = panel.once("reloaded");
      panel.accessibilityProxy.commands.targetCommand.reloadTopLevelTarget();
      await onReloaded;
    },
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
      },
    },
  },
  {
    desc: "Navigate to a new page.",
    setup: async () => {
      // `navigate` waits for the "reloaded" event so we don't need to do it explicitly here
      await navigateTo(buildURL(TEST_URI_2));
    },
    expected: {
      tree: [
        {
          role: "document",
          name: `"Navigation Accessibility Panel"`,
        },
      ],
      sidebar: {
        name: "Navigation Accessibility Panel",
        role: "document",
      },
    },
  },
];

/**
 * Simple test that checks content of the Accessibility panel tree on reload.
 */
addA11yPanelTestsTask(
  tests,
  TEST_URI_1,
  "Test Accessibility panel tree on reload."
);
