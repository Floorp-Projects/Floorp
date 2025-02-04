/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test displaying the nodes that caused issues.

const TEST_URI = `
  <style>
  body {
    user-modify: read-only;
  }
  div {
    user-modify: read-only;
    scrollbar-color: auto;
  }
  </style>
  <body>
    <div>div</div>
  </body>
`;

const TEST_DATA_ALL = [
  {
    property: "user-modify",
    nodes: ["body", "div"],
  },
  {
    property: "scrollbar-color",
    nodes: ["div"],
  },
];

add_task(async function () {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));

  const { allElementsPane, selectedElementPane } =
    await openCompatibilityView();

  info("Check nodes that caused issues on the selected element");
  is(
    selectedElementPane.querySelectorAll(".compatibility-node-item").length,
    0,
    "Nodes are not displayed on the selected element"
  );

  info("Check nodes that caused issues on all elements");
  await assertNodeList(allElementsPane, TEST_DATA_ALL);
});
