/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the selector highlighter is shown when clicking on a selector icon
// in the rule-view

const TEST_URI = `
  <style type="text/css">
    body {
      background: red;
    }
    p {
      color: white;
    }
  </style>
  <p>Testing the selector highlighter</p>
`;

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, view } = await openRuleView();
  let data;

  info("Clicking once on the body selector highlighter icon");
  data = await clickSelectorIcon(view, "body");
  ok(data.isShown, "The highlighter is shown");

  info("Clicking once again on the body selector highlighter icon");
  data = await clickSelectorIcon(view, "body");
  ok(!data.isShown, "The highlighter is hidden");

  info("Checking that the right NodeFront reference and options are passed");
  await selectNode("p", inspector);
  data = await clickSelectorIcon(view, "p");

  is(
    data.nodeFront.tagName,
    "P",
    "The right NodeFront is passed to the highlighter"
  );
  is(
    data.options.selector,
    "p",
    "The right selector option is passed to the highlighter"
  );
});
