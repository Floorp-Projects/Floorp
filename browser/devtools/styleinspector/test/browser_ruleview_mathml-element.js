/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the rule-view displays correctly on MathML elements.

const TEST_URI = `
  <div>
    <math xmlns=\http://www.w3.org/1998/Math/MathML\>
      <mfrac>
        <msubsup>
          <mi>a</mi>
          <mi>i</mi>
          <mi>j</mi>
        </msubsup>
        <msub>
          <mi>x</mi>
          <mn>0</mn>
        </msub>
      </mfrac>
    </math>
  </div>
`;

add_task(function*() {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {inspector, view} = yield openRuleView();

  info("Select the DIV node and verify the rule-view shows rules");
  yield selectNode("div", inspector);
  ok(view.element.querySelectorAll(".ruleview-rule").length,
    "The rule-view shows rules for the div element");

  info("Select various MathML nodes and verify the rule-view is empty");
  yield selectNode("math", inspector);
  ok(!view.element.querySelectorAll(".ruleview-rule").length,
    "The rule-view is empty for the math element");

  yield selectNode("msubsup", inspector);
  ok(!view.element.querySelectorAll(".ruleview-rule").length,
    "The rule-view is empty for the msubsup element");

  yield selectNode("mn", inspector);
  ok(!view.element.querySelectorAll(".ruleview-rule").length,
    "The rule-view is empty for the mn element");

  info("Select again the DIV node and verify the rule-view shows rules");
  yield selectNode("div", inspector);
  ok(view.element.querySelectorAll(".ruleview-rule").length,
    "The rule-view shows rules for the div element");
});
