/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test css properties that are inactive in :visited rule.

const TEST_URI = URL_ROOT + "doc_visited.html";

const TEST_DATA = [
  {
    selector: "#visited",
    inactiveDeclarations: [
      {
        declaration: { "font-size": "100px" },
        ruleIndex: 2,
      },
      {
        declaration: { "margin-left": "50px" },
        ruleIndex: 2,
      },
    ],
    activeDeclarations: [
      {
        declarations: {
          "background-color": "transparent",
          "border-color": "lime",
          color: "rgba(0, 255, 0, 0.8)",
          "text-decoration-color": "lime",
          "text-emphasis-color": "seagreen",
        },
        ruleIndex: 2,
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
          "text-decoration-color": "lime",
          "text-emphasis-color": "seagreen",
        },
        ruleIndex: 1,
      },
    ],
  },
];

add_task(async () => {
  info("Open a url which has visited links");
  const tab = await addTab(TEST_URI);

  info("Wait until the visited links are available");
  const selectors = TEST_DATA.map(t => t.selector);
  await waitUntilVisitedState(tab, selectors);

  info("Open the inspector");
  const { inspector, view } = await openRuleView();

  await runInactiveCSSTests(view, inspector, TEST_DATA);
});
