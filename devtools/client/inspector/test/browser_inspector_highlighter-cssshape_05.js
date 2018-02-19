/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test hovering over shape points in the rule-view and shapes highlighter.

const TEST_URL = URL_ROOT + "doc_inspector_highlighter_cssshapes.html";
const HIGHLIGHTER_TYPE = "ShapesHighlighter";

add_task(function* () {
  let env = yield openInspectorForURL(TEST_URL);
  let helper = yield getHighlighterHelperFor(HIGHLIGHTER_TYPE)(env);
  let { testActor, inspector } = env;
  let view = selectRuleView(inspector);
  let highlighters = view.highlighters;
  let config = { inspector, view, highlighters, testActor, helper };

  yield highlightFromRuleView(config);
  yield highlightFromHighlighter(config);
});

function* setup(config) {
  const { view, selector, property, inspector } = config;
  info(`Turn on shapes highlighter for ${selector}`);
  yield selectNode(selector, inspector);
  return yield toggleShapesHighlighter(view, selector, property, true);
}

function* teardown(config) {
  const { view, selector, property } = config;
  info(`Turn off shapes highlighter for ${selector}`);
  return yield toggleShapesHighlighter(view, selector, property, false);
}
/*
* Test that points hovered in the rule view will highlight corresponding points
* in the shapes highlighter on the page.
*/
function* highlightFromRuleView(config) {
  const { view, highlighters, testActor } = config;
  const selector = "#polygon";
  const property = "clip-path";

  yield setup({ selector, property, ...config });

  let container = getRuleViewProperty(view, selector, property).valueSpan;
  let shapesToggle = container.querySelector(".ruleview-shapeswatch");

  let highlighterFront = highlighters.highlighters[HIGHLIGHTER_TYPE];
  let markerHidden = yield testActor.getHighlighterNodeAttribute(
    "shapes-marker-hover", "hidden", highlighterFront);
  ok(markerHidden, "Hover marker on highlighter is not visible");

  info("Hover over point 0 in rule view");
  let pointSpan = container.querySelector(".ruleview-shape-point[data-point='0']");
  let onHighlighterShown = highlighters.once("shapes-highlighter-shown");
  EventUtils.synthesizeMouseAtCenter(pointSpan, {type: "mousemove"}, view.styleWindow);
  yield onHighlighterShown;

  info("Point in shapes highlighter is marked when same point in rule view is hovered");
  markerHidden = yield testActor.getHighlighterNodeAttribute(
    "shapes-marker-hover", "hidden", highlighterFront);
  ok(!markerHidden, "Marker on highlighter is visible");

  info("Move mouse off point");
  onHighlighterShown = highlighters.once("shapes-highlighter-shown");
  EventUtils.synthesizeMouseAtCenter(shapesToggle, {type: "mousemove"}, view.styleWindow);
  yield onHighlighterShown;

  markerHidden = yield testActor.getHighlighterNodeAttribute(
    "shapes-marker-hover", "hidden", highlighterFront);
  ok(markerHidden, "Marker on highlighter is not visible");

  yield teardown({selector, property, ...config});
}

/*
* Test that points hovered in the shapes highlighter on the page will highlight
* corresponding points in the rule view.
*/
function* highlightFromHighlighter(config) {
  const { view, highlighters, testActor, helper } = config;
  const selector = "#polygon";
  const property = "clip-path";

  yield setup({ selector, property, ...config });

  let highlighterFront = highlighters.highlighters[HIGHLIGHTER_TYPE];
  let { mouse } = helper;
  let container = getRuleViewProperty(view, selector, property).valueSpan;

  info("Hover over first point in highlighter");
  let onEventHandled = highlighters.once("highlighter-event-handled");
  yield mouse.move(0, 0);
  yield onEventHandled;
  let markerHidden = yield testActor.getHighlighterNodeAttribute(
    "shapes-marker-hover", "hidden", highlighterFront);
  ok(!markerHidden, "Marker on highlighter is visible");

  info("Point in rule view is marked when same point in shapes highlighter is hovered");
  let pointSpan = container.querySelector(".ruleview-shape-point[data-point='0']");
  ok(pointSpan.classList.contains("active"), "Span for point 0 is active");

  info("Move mouse off point");
  onEventHandled = highlighters.once("highlighter-event-handled");
  yield mouse.move(100, 100);
  yield onEventHandled;
  markerHidden = yield testActor.getHighlighterNodeAttribute(
    "shapes-marker-hover", "hidden", highlighterFront);
  ok(markerHidden, "Marker on highlighter is no longer visible");
  ok(!pointSpan.classList.contains("active"), "Span for point 0 is no longer active");

  yield teardown({ selector, property, ...config });
}
