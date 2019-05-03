/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* global toggleFilter */

const header = "<h1 style=\"color:rgba(255,0,0,0.1); " +
               "background-color:rgba(255,255,255,1);\">header</h1>";

const TEST_URI = `<html>
  <head>
    <meta charset="utf-8"/>
    <title>Accessibility Panel Test</title>
  </head>
  <body>
    ${header.repeat(20)}
  </body>
</html>`;

const docRow = {
  role: "document",
  name: `"Accessibility Panel Test"`,
};
const headingRow = {
  role: "heading",
  name: `"header"`,
};
const textLeafRow = {
  role: "text leaf",
  name: `"header"contrast`,
  badges: [ "contrast" ],
};
const audit = new Array(20).fill(textLeafRow);

const auditInitial = audit.map(check => ({ ...check }));
auditInitial[0].selected = true;

const auditSecondLastSelected = audit.map(check => ({ ...check }));
auditSecondLastSelected[19].selected = true;

const resetAfterAudit = [docRow];
for (let i = 0; i < 20; i++) {
  resetAfterAudit.push(headingRow);
  resetAfterAudit.push({ ...textLeafRow, selected: i === 19 });
}

/**
* Test data has the format of:
* {
*   desc     {String}    description for better logging
*   setup    {Function}  An optional setup that needs to be performed before
*                        the state of the tree and the sidebar can be checked.
*   expected {JSON}      An expected states for the tree and the sidebar.
* }
*/
const tests = [{
  desc: "Check initial state.",
  expected: {
    tree: [{ ...docRow, selected: true }],
  },
}, {
  desc: "Run an audit from a11y panel toolbar by activating a filter.",
  setup: async ({ doc }) => {
    await toggleFilter(doc, 0);
  },
  expected: {
    tree: auditInitial,
  },
}, {
  desc: "Select a row that is guaranteed to have to be scrolled into view.",
  setup: async ({ doc }) => {
    selectRow(doc, 0);
    EventUtils.synthesizeKey("VK_END", {}, doc.defaultView);
  },
  expected: {
    tree: auditSecondLastSelected,
  },
}, {
  desc: "Click on the filter again.",
  setup: async ({ doc }) => {
    await toggleFilter(doc, 0);
  },
  expected: {
    tree: resetAfterAudit,
  },
}];

/**
* Simple test that checks content of the Accessibility panel tree when the
* audit is activated via the panel's toolbar and the selection persists when
* the filter is toggled off.
*/
addA11yPanelTestsTask(tests, TEST_URI,
  "Test Accessibility panel tree with persistent selected row.");
