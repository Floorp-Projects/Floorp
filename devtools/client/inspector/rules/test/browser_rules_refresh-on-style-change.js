/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the rule view refreshes when the current node has its style
// changed

const TEST_URI = "<div id='testdiv' style='font-size: 10px;''>Test div!</div>";

add_task(async function() {
  Services.prefs.setCharPref("devtools.defaultColorUnit", "name");

  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const {inspector, view, testActor} = await openRuleView();
  await selectNode("#testdiv", inspector);

  let fontSize = getRuleViewPropertyValue(view, "element", "font-size");
  is(fontSize, "10px", "The rule view shows the right font-size");

  info("Changing the node's style and waiting for the update");
  const onUpdated = inspector.once("rule-view-refreshed");
  await testActor.setAttribute("#testdiv", "style",
    "font-size: 3em; color: lightgoldenrodyellow; " +
    "text-align: right; text-transform: uppercase");
  await onUpdated;

  const textAlign = getRuleViewPropertyValue(view, "element", "text-align");
  is(textAlign, "right", "The rule view shows the new text align.");
  const color = getRuleViewPropertyValue(view, "element", "color");
  is(color, "lightgoldenrodyellow", "The rule view shows the new color.");
  fontSize = getRuleViewPropertyValue(view, "element", "font-size");
  is(fontSize, "3em", "The rule view shows the new font size.");
  const textTransform = getRuleViewPropertyValue(view, "element",
    "text-transform");
  is(textTransform, "uppercase", "The rule view shows the new text transform.");
});
