/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the selector highlighter is correctly shown when clicking on a
// inherited element

const TEST_URI = `
<div style="cursor:pointer">
  A
  <div style="cursor:pointer">
    B<a>Cursor</a>
  </div>
</div>
`;

add_task(async function () {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, view } = await openRuleView();
  let data;

  info("Checking that the right NodeFront reference and options are passed");
  await selectNode("a", inspector);

  data = await clickSelectorIcon(view, "element");
  is(
    data.options.selector,
    "body > div:nth-child(1) > div:nth-child(1) > a:nth-child(1)",
    "The right selector option is passed to the highlighter (1)"
  );

  data = await clickSelectorIcon(view, "element", 1);
  is(
    data.options.selector,
    "body > div:nth-child(1) > div:nth-child(1)",
    "The right selector option is passed to the highlighter (1)"
  );

  data = await clickSelectorIcon(view, "element", 2);
  is(
    data.options.selector,
    "body > div:nth-child(1)",
    "The right selector option is passed to the highlighter (1)"
  );
});
