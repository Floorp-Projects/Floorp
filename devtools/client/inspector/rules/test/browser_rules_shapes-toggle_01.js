/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test toggling the shapes highlighter in the rule view and the display of the
// shapes highlighter.

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

add_task(function* () {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {inspector, view} = yield openRuleView();
  let highlighters = view.highlighters;

  yield selectNode("#shape", inspector);
  let container = getRuleViewProperty(view, "#shape", "clip-path").valueSpan;
  let shapesToggle = container.querySelector(".ruleview-shape");

  info("Checking the initial state of the CSS shape toggle in the rule-view.");
  ok(shapesToggle, "Shapes highlighter toggle is visible.");
  ok(!shapesToggle.classList.contains("active"),
    "Shapes highlighter toggle button is not active.");
  ok(!highlighters.highlighters[HIGHLIGHTER_TYPE],
    "No CSS shapes highlighter exists in the rule-view.");
  ok(!highlighters.shapesHighlighterShown, "No CSS shapes highlighter is shown.");

  info("Toggling ON the CSS shapes highlighter from the rule-view.");
  let onHighlighterShown = highlighters.once("shapes-highlighter-shown");
  shapesToggle.click();
  yield onHighlighterShown;

  info("Checking the CSS shapes highlighter is created and toggle button is active in " +
    "the rule-view.");
  ok(shapesToggle.classList.contains("active"),
    "Shapes highlighter toggle is active.");
  ok(highlighters.highlighters[HIGHLIGHTER_TYPE],
    "CSS shapes highlighter created in the rule-view.");
  ok(highlighters.shapesHighlighterShown, "CSS shapes highlighter is shown.");

  info("Toggling OFF the CSS shapes highlighter from the rule-view.");
  let onHighlighterHidden = highlighters.once("shapes-highlighter-hidden");
  shapesToggle.click();
  yield onHighlighterHidden;

  info("Checking the CSS shapes highlighter is not shown and toggle button is not " +
    "active in the rule-view.");
  ok(!shapesToggle.classList.contains("active"),
    "shapes highlighter toggle button is not active.");
  ok(!highlighters.shapesHighlighterShown, "No CSS shapes highlighter is shown.");
});
