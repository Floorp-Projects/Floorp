/* Any copyright", " is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the rule-view displays correctly on MathML elements

waitForExplicitFinish();

const TEST_URL = [
  "data:text/html,",
  "<div>",
  "  <math xmlns=\"http://www.w3.org/1998/Math/MathML\">",
  "    <mfrac>",
  "      <msubsup>",
  "        <mi>a</mi>",
  "        <mi>i</mi>",
  "        <mi>j</mi>",
  "      </msubsup>",
  "      <msub>",
  "        <mi>x</mi>",
  "        <mn>0</mn>",
  "      </msub>",
  "    </mfrac>",
  "  </math>",
  "</div>"
].join("");

function test() {
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function onload(evt) {
    gBrowser.selectedBrowser.removeEventListener("load", onload, true);
    waitForFocus(runTests, content);
  }, true);
  content.location = TEST_URL;
}

function runTests() {
  openRuleView((inspector, ruleView) => {
    Task.spawn(function() {
      info("Select the DIV node and verify the rule-view shows rules");
      yield selectNode("div", inspector);
      ok(ruleView.element.querySelectorAll(".ruleview-rule").length,
        "The rule-view shows rules for the div element");

      info("Select various MathML nodes and verify the rule-view is empty");
      yield selectNode("math", inspector);
      ok(!ruleView.element.querySelectorAll(".ruleview-rule").length,
        "The rule-view is empty for the math element");

      yield selectNode("msubsup", inspector);
      ok(!ruleView.element.querySelectorAll(".ruleview-rule").length,
        "The rule-view is empty for the msubsup element");

      yield selectNode("mn", inspector);
      ok(!ruleView.element.querySelectorAll(".ruleview-rule").length,
        "The rule-view is empty for the mn element");

      info("Select again the DIV node and verify the rule-view shows rules");
      yield selectNode("div", inspector);
      ok(ruleView.element.querySelectorAll(".ruleview-rule").length,
        "The rule-view shows rules for the div element");
    }).then(null, ok.bind(null, false)).then(finishUp);
  });
}

function finishUp() {
  gBrowser.removeCurrentTab();
  finish();
}
