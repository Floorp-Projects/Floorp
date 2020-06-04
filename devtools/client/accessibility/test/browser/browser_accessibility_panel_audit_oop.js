/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* global toggleMenuItem, TREE_FILTERS_MENU_ID */

const TEST_URI = `<html>
   <head>
     <meta charset="utf-8"/>
     <title>Accessibility Panel Test</title>
   </head>
   <body>
     <h1 style="color:rgba(255,0,0,0.1); background-color:rgba(255,255,255,1);">
       Top level header
     </h1>
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
    desc: "Initial state.",
    expected: {
      tree: [
        {
          role: "document",
          name: `""text label`,
          badges: ["text label"],
          level: 1,
        },
      ],
    },
  },
  {
    desc: "Click on the Check for issues - all.",
    setup: async ({ doc, toolbox }) => {
      await toggleMenuItem(doc, toolbox.doc, TREE_FILTERS_MENU_ID, 1);
    },
    expected: {
      tree: [
        {
          role: "document",
          name: `""text label`,
          badges: ["text label"],
          level: 1,
        },
        {
          role: "text leaf",
          name: `"Top level header "contrast`,
          badges: ["contrast"],
          level: 1,
        },
      ],
    },
  },
  {
    desc: "Click on the Check for issues - all again.",
    setup: async ({ doc, toolbox }) => {
      await toggleMenuItem(doc, toolbox.doc, TREE_FILTERS_MENU_ID, 1);
    },
    expected: {
      tree: [
        {
          role: "document",
          name: `""text label`,
          badges: ["text label"],
          level: 1,
        },
        {
          role: "internal frame",
          name: `"Accessibility Panel Test (OOP)"`,
          level: 2,
        },
        {
          role: "document",
          name: `"Accessibility Panel Test (OOP)"`,
          level: 3,
        },
        {
          role: "heading",
          name: `"Top level header"`,
          level: 4,
        },
        {
          role: "text leaf",
          name: `"Top level header "contrast`,
          badges: ["contrast"],
          level: 5,
        },
      ],
    },
  },
];

addA11yPanelTestsTask(
  tests,
  TEST_URI,
  "Test Accessibility panel tree audit on a page with an OOP document.",
  { remoteIframe: true }
);
