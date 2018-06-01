/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test toggling transform mode of the shapes highlighter

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

const HIGHLIGHTER_TYPE = "ShapesHighlighter";

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const {inspector, view} = await openRuleView();
  const highlighters = view.highlighters;

  info("Select a node with a shape value");
  await selectNode("#shape", inspector);
  const container = getRuleViewProperty(view, "#shape", "clip-path").valueSpan;
  const shapesToggle = container.querySelector(".ruleview-shapeswatch");

  info("Toggling ON the CSS shapes highlighter with transform mode on.");
  let onHighlighterShown = highlighters.once("shapes-highlighter-shown");
  EventUtils.sendMouseEvent({type: "click", metaKey: true, ctrlKey: true},
    shapesToggle, view.styleWindow);
  await onHighlighterShown;

  info("Checking the CSS shapes highlighter is created and transform mode is on");
  ok(highlighters.highlighters[HIGHLIGHTER_TYPE],
    "CSS shapes highlighter created in the rule-view.");
  ok(highlighters.shapesHighlighterShown, "CSS shapes highlighter is shown.");
  ok(highlighters.state.shapes.options.transformMode, "Transform mode is on.");

  info("Toggling OFF the CSS shapes highlighter from the rule-view.");
  const onHighlighterHidden = highlighters.once("shapes-highlighter-hidden");
  EventUtils.sendMouseEvent({type: "click"},
    shapesToggle, view.styleWindow);
  await onHighlighterHidden;

  info("Checking the CSS shapes highlighter is not shown.");
  ok(!highlighters.shapesHighlighterShown, "No CSS shapes highlighter is shown.");

  info("Toggling ON the CSS shapes highlighter with transform mode off.");
  onHighlighterShown = highlighters.once("shapes-highlighter-shown");
  EventUtils.sendMouseEvent({type: "click"}, shapesToggle, view.styleWindow);
  await onHighlighterShown;

  info("Checking the CSS shapes highlighter is created and transform mode is off");
  ok(highlighters.highlighters[HIGHLIGHTER_TYPE],
    "CSS shapes highlighter created in the rule-view.");
  ok(highlighters.shapesHighlighterShown, "CSS shapes highlighter is shown.");
  ok(!highlighters.state.shapes.options.transformMode, "Transform mode is off.");

  info("Clicking shapes toggle to turn on transform mode while highlighter is shown.");
  onHighlighterShown = highlighters.once("shapes-highlighter-shown");
  EventUtils.sendMouseEvent({type: "click", metaKey: true, ctrlKey: true},
    shapesToggle, view.styleWindow);
  await onHighlighterShown;

  info("Checking the CSS shapes highlighter is created and transform mode is on");
  ok(highlighters.highlighters[HIGHLIGHTER_TYPE],
    "CSS shapes highlighter created in the rule-view.");
  ok(highlighters.shapesHighlighterShown, "CSS shapes highlighter is shown.");
  ok(highlighters.state.shapes.options.transformMode, "Transform mode is on.");
});
