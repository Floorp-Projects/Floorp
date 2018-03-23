/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test hovering over shape points in the rule-view and shapes highlighter.

const TEST_URL = URL_ROOT + "doc_inspector_highlighter_cssshapes.html";

const HIGHLIGHTER_TYPE = "ShapesHighlighter";
const CSS_SHAPES_ENABLED_PREF = "devtools.inspector.shapesHighlighter.enabled";

add_task(async function() {
  await pushPref(CSS_SHAPES_ENABLED_PREF, true);
  let env = await openInspectorForURL(TEST_URL);
  let helper = await getHighlighterHelperFor(HIGHLIGHTER_TYPE)(env);
  let { testActor, inspector } = env;
  let view = selectRuleView(inspector);
  let highlighters = view.highlighters;

  await highlightFromRuleView(inspector, view, highlighters, testActor);
  await highlightFromHighlighter(view, highlighters, testActor, helper);
});

async function highlightFromRuleView(inspector, view, highlighters, testActor) {
  await selectNode("#polygon", inspector);
  await toggleShapesHighlighter(view, highlighters, "#polygon", "clip-path", true);
  let container = getRuleViewProperty(view, "#polygon", "clip-path").valueSpan;
  let shapesToggle = container.querySelector(".ruleview-shapeswatch");

  let highlighterFront = highlighters.highlighters[HIGHLIGHTER_TYPE];
  let markerHidden = await testActor.getHighlighterNodeAttribute(
    "shapes-marker-hover", "hidden", highlighterFront);
  ok(markerHidden, "Hover marker on highlighter is not visible");

  info("Hover over point 0 in rule view");
  let pointSpan = container.querySelector(".ruleview-shape-point[data-point='0']");
  let onHighlighterShown = highlighters.once("shapes-highlighter-shown");
  EventUtils.synthesizeMouseAtCenter(pointSpan, {type: "mousemove"}, view.styleWindow);
  await onHighlighterShown;

  ok(pointSpan.classList.contains("active"), "Hovered span is active");
  is(highlighters.state.shapes.options.hoverPoint, "0",
     "Hovered point is saved to state");

  markerHidden = await testActor.getHighlighterNodeAttribute(
    "shapes-marker-hover", "hidden", highlighterFront);
  ok(!markerHidden, "Marker on highlighter is visible");

  info("Move mouse off point");
  onHighlighterShown = highlighters.once("shapes-highlighter-shown");
  EventUtils.synthesizeMouseAtCenter(shapesToggle, {type: "mousemove"}, view.styleWindow);
  await onHighlighterShown;

  ok(!pointSpan.classList.contains("active"), "Hovered span is no longer active");
  is(highlighters.state.shapes.options.hoverPoint, null, "Hovered point is null");

  markerHidden = await testActor.getHighlighterNodeAttribute(
    "shapes-marker-hover", "hidden", highlighterFront);
  ok(markerHidden, "Marker on highlighter is not visible");

  info("Hide shapes highlighter");
  await toggleShapesHighlighter(view, highlighters, "#polygon", "clip-path", false);
}

async function highlightFromHighlighter(view, highlighters, testActor, helper) {
  let highlighterFront = highlighters.highlighters[HIGHLIGHTER_TYPE];
  let { mouse } = helper;

  await toggleShapesHighlighter(view, highlighters, "#polygon", "clip-path", true);
  let container = getRuleViewProperty(view, "#polygon", "clip-path").valueSpan;

  info("Hover over first point in highlighter");
  let onEventHandled = highlighters.once("highlighter-event-handled");
  await mouse.move(0, 0);
  await onEventHandled;
  let markerHidden = await testActor.getHighlighterNodeAttribute(
    "shapes-marker-hover", "hidden", highlighterFront);
  ok(!markerHidden, "Marker on highlighter is visible");

  let pointSpan = container.querySelector(".ruleview-shape-point[data-point='0']");
  ok(pointSpan.classList.contains("active"), "Span for point 0 is active");
  is(highlighters.state.shapes.hoverPoint, "0", "Hovered point is saved to state");

  info("Check that point is still highlighted after moving it");
  await mouse.down(0, 0);
  await mouse.move(10, 10);
  await mouse.up(10, 10);
  markerHidden = await testActor.getHighlighterNodeAttribute(
    "shapes-marker-hover", "hidden", highlighterFront);
  ok(!markerHidden, "Marker on highlighter is visible after moving point");

  container = getRuleViewProperty(view, "element", "clip-path").valueSpan;
  pointSpan = container.querySelector(".ruleview-shape-point[data-point='0']");
  ok(pointSpan.classList.contains("active"),
     "Span for point 0 is active after moving point");
  is(highlighters.state.shapes.hoverPoint, "0",
     "Hovered point is saved to state after moving point");

  info("Move mouse off point");
  onEventHandled = highlighters.once("highlighter-event-handled");
  await mouse.move(100, 100);
  await onEventHandled;
  markerHidden = await testActor.getHighlighterNodeAttribute(
    "shapes-marker-hover", "hidden", highlighterFront);
  ok(markerHidden, "Marker on highlighter is no longer visible");
  ok(!pointSpan.classList.contains("active"), "Span for point 0 is no longer active");
  is(highlighters.state.shapes.hoverPoint, null, "Hovered point is null");
}
