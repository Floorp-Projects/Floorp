/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check that no inherited properties appear when the property does not apply
// to the nested element.

var {ELEMENT_STYLE} = require("devtools/server/actors/styles");

const TEST_URI = `
  <style type="text/css">
    #test2 {
      background-color: green;
    }
  </style>
  <div id="test2"><div id="test1">Styled Node</div></div>
`;

add_task(function*() {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {inspector, view} = yield openRuleView();
  yield selectNode("#test1", inspector);
  yield emptyInherit(inspector, view);
});

function* emptyInherit(inspector, view) {
  // No inheritable styles, this rule shouldn't show up.
  let elementStyle = view._elementStyle;
  is(elementStyle.rules.length, 1, "Should have 1 rule.");

  let elementRule = elementStyle.rules[0];
  ok(!elementRule.inherited,
    "Element style attribute should not consider itself inherited.");
}
