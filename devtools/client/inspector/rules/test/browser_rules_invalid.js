/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that an invalid property still lets us display the rule view
// Bug 1235603.

const TEST_URI = `
  <style>
    div {
        background: #fff;
        font-family: sans-serif;
        url(display-table.min.htc);
    }
 </style>
 <body>
    <div id="testid" class="testclass">Styled Node</div>
 </body>
`;

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));

  const { inspector, view } = await openRuleView();
  await selectNode("#testid", inspector);

  is(view._elementStyle.rules.length, 2, "Should have 2 rules.");
  // Have to actually get the rule in order to ensure that the
  // elements were created.
  ok(getRuleViewRule(view, "div"), "Rule with div selector exists");
});
