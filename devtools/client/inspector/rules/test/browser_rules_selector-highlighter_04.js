/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the selector highlighter is shown when clicking on a selector icon
// for the 'element {}' rule

const TEST_URI = `
<p>Testing the selector highlighter for the 'element {}' rule</p>
`;

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, view } = await openRuleView();
  let data;

  info("Checking that the right NodeFront reference and options are passed");
  await selectNode("p", inspector);
  data = await clickSelectorIcon(view, "element");
  is(
    data.nodeFront.tagName,
    "P",
    "The right NodeFront is passed to the highlighter (1)"
  );
  is(
    data.options.selector,
    "body > p:nth-child(1)",
    "The right selector option is passed to the highlighter (1)"
  );
  ok(data.isShown, "The toggle event says the highlighter is visible");

  data = await clickSelectorIcon(view, "element");
  ok(!data.isShown, "The toggle event says the highlighter is not visible");
});
