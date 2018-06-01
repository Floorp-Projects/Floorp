/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test hovering over shape points in the rule-view and shapes highlighter.

const TEST_URL = URL_ROOT + "doc_inspector_highlighter_cssshapes.html";
const HIGHLIGHTER_TYPE = "ShapesHighlighter";

add_task(async function() {
  const env = await openInspectorForURL(TEST_URL);
  const helper = await getHighlighterHelperFor(HIGHLIGHTER_TYPE)(env);
  const { testActor, inspector } = env;
  const view = selectRuleView(inspector);
  const highlighters = view.highlighters;
  const config = { inspector, view, highlighters, testActor, helper };

  await highlightFromRuleView(config);
  await highlightFromHighlighter(config);
});

async function setup(config) {
  const { view, selector, property, inspector } = config;
  info(`Turn on shapes highlighter for ${selector}`);
  await selectNode(selector, inspector);
  await toggleShapesHighlighter(view, selector, property, true);
}

async function teardown(config) {
  const { view, selector, property } = config;
  info(`Turn off shapes highlighter for ${selector}`);
  await toggleShapesHighlighter(view, selector, property, false);
}
/*
* Test that points hovered in the rule view will highlight corresponding points
* in the shapes highlighter on the page.
*/
async function highlightFromRuleView(config) {
  const { view, highlighters, testActor } = config;
  const selector = "#polygon";
  const property = "clip-path";

  await setup({ selector, property, ...config });

  const container = getRuleViewProperty(view, selector, property).valueSpan;
  const shapesToggle = container.querySelector(".ruleview-shapeswatch");

  const highlighterFront = highlighters.highlighters[HIGHLIGHTER_TYPE];
  let markerHidden = await testActor.getHighlighterNodeAttribute(
    "shapes-marker-hover", "hidden", highlighterFront);
  ok(markerHidden, "Hover marker on highlighter is not visible");

  info("Hover over point 0 in rule view");
  const pointSpan = container.querySelector(".ruleview-shape-point[data-point='0']");
  let onHighlighterShown = highlighters.once("shapes-highlighter-shown");
  EventUtils.synthesizeMouseAtCenter(pointSpan, {type: "mousemove"}, view.styleWindow);
  await onHighlighterShown;

  info("Point in shapes highlighter is marked when same point in rule view is hovered");
  markerHidden = await testActor.getHighlighterNodeAttribute(
    "shapes-marker-hover", "hidden", highlighterFront);
  ok(!markerHidden, "Marker on highlighter is visible");

  info("Move mouse off point");
  onHighlighterShown = highlighters.once("shapes-highlighter-shown");
  EventUtils.synthesizeMouseAtCenter(shapesToggle, {type: "mousemove"}, view.styleWindow);
  await onHighlighterShown;

  markerHidden = await testActor.getHighlighterNodeAttribute(
    "shapes-marker-hover", "hidden", highlighterFront);
  ok(markerHidden, "Marker on highlighter is not visible");

  await teardown({selector, property, ...config});
}

/*
* Test that points hovered in the shapes highlighter on the page will highlight
* corresponding points in the rule view.
*/
async function highlightFromHighlighter(config) {
  const { view, highlighters, testActor, helper } = config;
  const selector = "#polygon";
  const property = "clip-path";

  await setup({ selector, property, ...config });

  const highlighterFront = highlighters.highlighters[HIGHLIGHTER_TYPE];
  const { mouse } = helper;
  const container = getRuleViewProperty(view, selector, property).valueSpan;

  info("Hover over first point in highlighter");
  let onEventHandled = highlighters.once("highlighter-event-handled");
  await mouse.move(0, 0);
  await onEventHandled;
  let markerHidden = await testActor.getHighlighterNodeAttribute(
    "shapes-marker-hover", "hidden", highlighterFront);
  ok(!markerHidden, "Marker on highlighter is visible");

  info("Point in rule view is marked when same point in shapes highlighter is hovered");
  const pointSpan = container.querySelector(".ruleview-shape-point[data-point='0']");
  ok(pointSpan.classList.contains("active"), "Span for point 0 is active");

  info("Move mouse off point");
  onEventHandled = highlighters.once("highlighter-event-handled");
  await mouse.move(100, 100);
  await onEventHandled;
  markerHidden = await testActor.getHighlighterNodeAttribute(
    "shapes-marker-hover", "hidden", highlighterFront);
  ok(markerHidden, "Marker on highlighter is no longer visible");
  ok(!pointSpan.classList.contains("active"), "Span for point 0 is no longer active");

  await teardown({ selector, property, ...config });
}
