/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* global toggleFilter */

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
      toolbar: [true, false, false, false],
    },
  },
  {
    desc: "Toggle first filter (all) to activate.",
    setup: async ({ doc }) => {
      await toggleFilter(doc, 1);
    },
    expected: {
      toolbar: [false, true, true, true],
    },
  },
  {
    desc: "Click on the filter again.",
    setup: async ({ doc }) => {
      await toggleFilter(doc, 1);
    },
    expected: {
      toolbar: [true, false, false, false],
    },
  },
  {
    desc: "Toggle first custom filter to activate.",
    setup: async ({ doc }) => {
      await toggleFilter(doc, 2);
    },
    expected: {
      toolbar: [false, false, true, false],
    },
  },
  {
    desc: "Click on the filter again.",
    setup: async ({ doc }) => {
      await toggleFilter(doc, 2);
    },
    expected: {
      toolbar: [true, false, false, false],
    },
  },
  {
    desc: "Toggle first custom filter to activate.",
    setup: async ({ doc }) => {
      await toggleFilter(doc, 2);
    },
    expected: {
      toolbar: [false, false, true, false],
    },
  },
  {
    desc: "Toggle second custom filter to activate.",
    setup: async ({ doc }) => {
      await toggleFilter(doc, 3);
    },
    expected: {
      toolbar: [false, true, true, true],
    },
  },
  {
    desc: "Click on the none filter to de-activate all.",
    setup: async ({ doc }) => {
      await toggleFilter(doc, 0);
    },
    expected: {
      toolbar: [true, false, false, false],
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
