/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests adding a new rule using the add rule button.

const TEST_URI = `
  <style type="text/css">
    .testclass {
      text-align: center;
    }
  </style>
  <div id="testid" class="testclass">Styled Node</div>
  <span class="testclass2">This is a span</span>
  <span class="class1 class2">Multiple classes</span>
  <span class="class3      class4">Multiple classes</span>
  <p>Empty<p>
  <h1 class="asd@@@@a!!!!:::@asd">Invalid characters in class</h1>
  <h2 id="asd@@@a!!2a">Invalid characters in id</h2>
  <svg viewBox="0 0 10 10">
    <circle cx="5" cy="5" r="5" fill="blue"></circle>
  </svg>
`;

const TEST_DATA = [
  { node: "#testid", expected: "#testid" },
  { node: ".testclass2", expected: ".testclass2" },
  { node: ".class1.class2", expected: ".class1.class2" },
  { node: ".class3.class4", expected: ".class3.class4" },
  { node: "p", expected: "p" },
  { node: "h1", expected: ".asd\\@\\@\\@\\@a\\!\\!\\!\\!\\:\\:\\:\\@asd" },
  { node: "h2", expected: "#asd\\@\\@\\@a\\!\\!2a" },
  { node: "circle", expected: "circle" }
];

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const {inspector, view} = await openRuleView();

  for (const data of TEST_DATA) {
    const {node, expected} = data;
    await selectNode(node, inspector);
    await addNewRuleAndDismissEditor(inspector, view, expected, 1);
  }
});
