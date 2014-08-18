/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the rule view refreshes when the current node has its style
// changed

const TESTCASE_URI = 'data:text/html;charset=utf-8,' +
                     '<div id="testdiv" style="font-size:10px;">Test div!</div>';

let test = asyncTest(function*() {
  yield addTab(TESTCASE_URI);

  Services.prefs.setCharPref("devtools.defaultColorUnit", "name");

  info("Getting the test node");
  let div = getNode("#testdiv");

  info("Opening the rule view and selecting the test node");
  let {toolbox, inspector, view} = yield openRuleView();
  yield selectNode("#testdiv", inspector);

  let fontSize = getRuleViewPropertyValue(view, "element", "font-size");
  is(fontSize, "10px", "The rule view shows the right font-size");

  info("Changing the node's style and waiting for the update");
  let onUpdated = inspector.once("rule-view-refreshed");
  div.style.cssText = "font-size: 3em; color: lightgoldenrodyellow; text-align: right; text-transform: uppercase";
  yield onUpdated;

  let textAlign = getRuleViewPropertyValue(view, "element", "text-align");
  is(textAlign, "right", "The rule view shows the new text align.");
  let color = getRuleViewPropertyValue(view, "element", "color");
  is(color, "lightgoldenrodyellow", "The rule view shows the new color.")
  let fontSize = getRuleViewPropertyValue(view, "element", "font-size");
  is(fontSize, "3em", "The rule view shows the new font size.");
  let textTransform = getRuleViewPropertyValue(view, "element", "text-transform");
  is(textTransform, "uppercase", "The rule view shows the new text transform.");
});
