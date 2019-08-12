/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* global toggleRow, toggleMenuItem */

const TEST_URI = `<html>
  <head>
    <meta charset="utf-8"/>
    <title>Accessibility Panel Test</title>
  </head>
  <body>
    <h1 style="color:rgba(255,0,0,0.1); background-color:rgba(255,255,255,1);">
      Top level header
    </h1>
    <h2 style="color:rgba(0,255,0,0.1); background-color:rgba(255,255,255,1);">
      Second level header
    </h2>
  </body>
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
    desc: "Expand first and second tree nodes.",
    setup: async ({ doc }) => {
      await toggleRow(doc, 0);
      await toggleRow(doc, 1);
    },
    expected: {
      tree: [
        {
          role: "document",
          name: `"Accessibility Panel Test"`,
          level: 1,
        },
        {
          role: "heading",
          name: `"Top level header"`,
          level: 2,
        },
        {
          role: "text leaf",
          name: `"Top level header "contrast`,
          badges: ["contrast"],
          level: 3,
        },
        {
          role: "heading",
          name: `"Second level header"`,
          level: 2,
        },
      ],
    },
  },
  {
    desc: "Click on the all filter.",
    setup: async ({ doc }) => {
      await toggleMenuItem(doc, 0, 1);
    },
    expected: {
      tree: [
        {
          role: "text leaf",
          name: `"Top level header "contrast`,
          badges: ["contrast"],
          level: 1,
        },
        {
          role: "text leaf",
          name: `"Second level header "contrast`,
          badges: ["contrast"],
          selected: true,
          level: 1,
        },
      ],
    },
  },
  {
    desc: "Click on the all filter again.",
    setup: async ({ doc }) => {
      await toggleMenuItem(doc, 0, 1);
    },
    expected: {
      tree: [
        {
          role: "document",
          name: `"Accessibility Panel Test"`,
          level: 1,
        },
        {
          role: "heading",
          name: `"Top level header"`,
          level: 2,
        },
        {
          role: "text leaf",
          name: `"Top level header "contrast`,
          badges: ["contrast"],
          level: 3,
        },
        {
          role: "heading",
          name: `"Second level header"`,
          level: 2,
        },
        {
          role: "text leaf",
          name: `"Second level header "contrast`,
          badges: ["contrast"],
          selected: true,
          level: 3,
        },
      ],
    },
  },
];

/**
 * Simple test that checks content of the Accessibility panel tree when one of
 * the tree rows has a "contrast" badge and auditing is activated via toolbar
 * filter.
 */
addA11yPanelTestsTask(
  tests,
  TEST_URI,
  "Test Accessibility panel tree with contrast badge present."
);
