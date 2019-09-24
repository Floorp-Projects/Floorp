/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test css properties that are inactive in :visited rule.

const VISISTED_URI = URL_ROOT + "doc_variables_1.html";

const TEST_URI = `
  <style type='text/css'>
    a:visited, #visited-and-other-matched-selector {
      background-color: transparent;
      border-color: lime;
      color: rgba(0, 255, 0, 0.8);
      font-size: 100px;
      margin-left: 50px;
    }
  </style>
  <a href="${VISISTED_URI}" id="visited-only">link1</a>
  <a href="${VISISTED_URI}" id="visited-and-other-matched-selector">link2</a>
`;

const TEST_DATA = [
  {
    selector: "#visited-only",
    inactiveDeclarations: [
      {
        declaration: { "font-size": "100px" },
        ruleIndex: 1,
      },
      {
        declaration: { "margin-left": "50px" },
        ruleIndex: 1,
      },
    ],
    activeDeclarations: [
      {
        declarations: {
          "background-color": "transparent",
          "border-color": "lime",
          color: "rgba(0, 255, 0, 0.8)",
        },
        ruleIndex: 1,
      },
    ],
  },
  {
    selector: "#visited-and-other-matched-selector",
    activeDeclarations: [
      {
        declarations: {
          "background-color": "transparent",
          "border-color": "lime",
          color: "rgba(0, 255, 0, 0.8)",
          "font-size": "100px",
          "margin-left": "50px",
        },
        ruleIndex: 1,
      },
    ],
  },
];

add_task(async () => {
  info("Open a particular url to make a visited link");
  const tab = await addTab(VISISTED_URI);

  info("Open tested page in the same tab");
  tab.linkedBrowser.loadURI(
    "data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI),
    {
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
    }
  );

  info("Open the inspector");
  const { inspector, view } = await openRuleView();

  await runInactiveCSSTests(view, inspector, TEST_DATA);
});
