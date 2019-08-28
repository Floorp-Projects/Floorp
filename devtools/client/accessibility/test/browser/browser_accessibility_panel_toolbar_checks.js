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
    desc: "Check initial state.",
    expected: {
      activeToolbarFilters: [true, false, false, false, false],
    },
  },
  {
    desc: "Toggle first filter (all) to activate.",
    setup: async ({ doc }) => {
      await toggleMenuItem(doc, 0, 1);
    },
    expected: {
      activeToolbarFilters: [false, true, true, true, true],
    },
  },
  {
    desc: "Click on the filter again.",
    setup: async ({ doc }) => {
      await toggleMenuItem(doc, 0, 1);
    },
    expected: {
      activeToolbarFilters: [true, false, false, false, false],
    },
  },
  {
    desc: "Toggle first custom filter to activate.",
    setup: async ({ doc }) => {
      await toggleMenuItem(doc, 0, 2);
    },
    expected: {
      activeToolbarFilters: [false, false, true, false, false],
    },
  },
  {
    desc: "Click on the filter again.",
    setup: async ({ doc }) => {
      await toggleMenuItem(doc, 0, 2);
    },
    expected: {
      activeToolbarFilters: [true, false, false, false, false],
    },
  },
  {
    desc: "Toggle first custom filter to activate.",
    setup: async ({ doc }) => {
      await toggleMenuItem(doc, 0, 2);
    },
    expected: {
      activeToolbarFilters: [false, false, true, false, false],
    },
  },
  {
    desc: "Toggle second custom filter to activate.",
    setup: async ({ doc }) => {
      await toggleMenuItem(doc, 0, 3);
    },
    expected: {
      activeToolbarFilters: [false, false, true, true, false],
    },
  },
  {
    desc: "Toggle third custom filter to activate.",
    setup: async ({ doc }) => {
      await toggleMenuItem(doc, 0, 4);
    },
    expected: {
      activeToolbarFilters: [false, true, true, true, true],
    },
  },
  {
    desc: "Click on the none filter to de-activate all.",
    setup: async ({ doc }) => {
      await toggleMenuItem(doc, 0, 0);
    },
    expected: {
      activeToolbarFilters: [true, false, false, false, false],
    },
  },
];

/**
 * Simple test that checks toggle states for filters in the Accessibility panel
 * toolbar.
 */
addA11yPanelTestsTask(
  tests,
  TEST_URI,
  "Test Accessibility panel filter toggle states."
);
