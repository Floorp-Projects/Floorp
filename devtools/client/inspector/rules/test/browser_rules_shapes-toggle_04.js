/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test toggling the shapes highlighter in the rule view and modifying the 'clip-path'
// declaration.

const TEST_URI = `
  <style type='text/css'>
    #shape {
      width: 800px;
      height: 800px;
      clip-path: circle(25%);
    }
  </style>
  <div id="shape"></div>
`;

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const {inspector, view} = await openRuleView();
  const highlighters = view.highlighters;

  info("Select a node with a shape value");
  await selectNode("#shape", inspector);
  const container = getRuleViewProperty(view, "#shape", "clip-path").valueSpan;
  let shapeToggle = container.querySelector(".ruleview-shapeswatch");

  info("Toggling ON the CSS shape highlighter from the rule-view.");
  const onHighlighterShown = highlighters.once("shapes-highlighter-shown");
  shapeToggle.click();
  await onHighlighterShown;

  info("Edit the clip-path property to ellipse.");
  const editor = await focusEditableField(view, container, 30);
  const onDone = view.once("ruleview-changed");
  editor.input.value = "ellipse(30% 20%);";
  EventUtils.synthesizeKey("VK_RETURN", {}, view.styleWindow);
  await onDone;

  info("Check the shape highlighter and shape toggle button are still visible.");
  shapeToggle = container.querySelector(".ruleview-shapeswatch");
  ok(shapeToggle, "Shape highlighter toggle is visible.");
  ok(highlighters.shapesHighlighterShown, "CSS shape highlighter is shown.");
});
