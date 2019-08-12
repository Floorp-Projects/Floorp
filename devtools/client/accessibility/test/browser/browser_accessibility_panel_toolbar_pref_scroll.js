/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* global toggleMenuItem */

const TEST_URI = `<html>
  <head>
    <meta charset="utf-8"/>
    <title>Accessibility Panel Test</title>
  </head>
  <body></body>
</html>`;

const {
  PREFS: { SCROLL_INTO_VIEW },
} = require("devtools/client/accessibility/constants");

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
    desc:
      "Check initial state. All filters are disabled (except none). Scroll into view pref disabled.",
    expected: {
      activeToolbarFilters: [true, false, false, false],
      toolbarPrefValues: {
        [SCROLL_INTO_VIEW]: false,
      },
    },
  },
  {
    desc:
      "Toggle scroll into view checkbox to set the pref. Scroll into view pref should be enabled.",
    setup: async ({ doc }) => {
      await toggleMenuItem(doc, 1, 0);
    },
    expected: {
      activeToolbarFilters: [true, false, false, false],
      toolbarPrefValues: {
        [SCROLL_INTO_VIEW]: true,
      },
    },
  },
  {
    desc:
      "Toggle off scroll into view checkbox to unset the pref. Scroll into view pref disabled.",
    setup: async ({ doc }) => {
      await toggleMenuItem(doc, 1, 0);
    },
    expected: {
      activeToolbarFilters: [true, false, false, false],
      toolbarPrefValues: {
        [SCROLL_INTO_VIEW]: false,
      },
    },
  },
];

/**
 * Simple test that checks toggle state and pref set for automatic scroll into
 * view setting.
 */
addA11yPanelTestsTask(
  tests,
  TEST_URI,
  "Test Accessibility panel scroll into view pref."
);
