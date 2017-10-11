/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test hovering over shape points in the rule-view and shapes highlighter.

const TEST_URL = URL_ROOT + "doc_inspector_highlighter_cssshapes.html";

const HIGHLIGHTER_TYPE = "ShapesHighlighter";
const CSS_SHAPES_ENABLED_PREF = "devtools.inspector.shapesHighlighter.enabled";

add_task(function* () {
  yield pushPref(CSS_SHAPES_ENABLED_PREF, true);
  let env = yield openInspectorForURL(TEST_URL);
  let helper = yield getHighlighterHelperFor(HIGHLIGHTER_TYPE)(env);
  let { testActor, inspector } = env;
  let view = selectRuleView(inspector);
  let highlighters = view.highlighters;

  yield highlightFromRuleView(inspector, view, highlighters, testActor);
  yield highlightFromHighlighter(view, highlighters, testActor, helper);
});

function* highlightFromRuleView(inspector, view, highlighters, testActor) {
  yield selectNode("#polygon", inspector);
  yield toggleShapesHighlighter(view, highlighters, "#polygon", "clip-path", true);
  let container = getRuleViewProperty(view, "#polygon", "clip-path").valueSpan;
  let shapesToggle = container.querySelector(".ruleview-shape");

  let highlighterFront = highlighters.highlighters[HIGHLIGHTER_TYPE];
  let markerHidden = yield testActor.getHighlighterNodeAttribute(
    "shapes-marker-hover", "hidden", highlighterFront);
  ok(markerHidden, "Hover marker on highlighter is not visible");

  info("Hover over point 0 in rule view");
  let pointSpan = container.querySelector(".ruleview-shape-point[data-point='0']");
  let onHighlighterShown = highlighters.once("shapes-highlighter-shown");
  EventUtils.synthesizeMouseAtCenter(pointSpan, {type: "mousemove"}, view.styleWindow);
  yield onHighlighterShown;

  ok(pointSpan.classList.contains("active"), "Hovered span is active");
  is(highlighters.state.shapes.options.hoverPoint, "0",
     "Hovered point is saved to state");

  markerHidden = yield testActor.getHighlighterNodeAttribute(
    "shapes-marker-hover", "hidden", highlighterFront);
  ok(!markerHidden, "Marker on highlighter is visible");

  info("Move mouse off point");
  onHighlighterShown = highlighters.once("shapes-highlighter-shown");
  EventUtils.synthesizeMouseAtCenter(shapesToggle, {type: "mousemove"}, view.styleWindow);
  yield onHighlighterShown;

  ok(!pointSpan.classList.contains("active"), "Hovered span is no longer active");
  is(highlighters.state.shapes.options.hoverPoint, null, "Hovered point is null");

  markerHidden = yield testActor.getHighlighterNodeAttribute(
    "shapes-marker-hover", "hidden", highlighterFront);
  ok(markerHidden, "Marker on highlighter is not visible");

  info("Hide shapes highlighter");
  yield toggleShapesHighlighter(view, highlighters, "#polygon", "clip-path", false);
}

function* highlightFromHighlighter(view, highlighters, testActor, helper) {
  let highlighterFront = highlighters.highlighters[HIGHLIGHTER_TYPE];
  let { mouse } = helper;

  yield toggleShapesHighlighter(view, highlighters, "#polygon", "clip-path", true);
  let container = getRuleViewProperty(view, "#polygon", "clip-path").valueSpan;

  info("Hover over first point in highlighter");
  let onEventHandled = highlighters.once("highlighter-event-handled");
  yield mouse.move(0, 0);
  yield onEventHandled;
  let markerHidden = yield testActor.getHighlighterNodeAttribute(
    "shapes-marker-hover", "hidden", highlighterFront);
  ok(!markerHidden, "Marker on highlighter is visible");

  let pointSpan = container.querySelector(".ruleview-shape-point[data-point='0']");
  ok(pointSpan.classList.contains("active"), "Span for point 0 is active");
  is(highlighters.state.shapes.hoverPoint, "0", "Hovered point is saved to state");

  info("Check that point is still highlighted after moving it");
  yield mouse.down(0, 0);
  yield mouse.move(10, 10);
  yield mouse.up(10, 10);
  markerHidden = yield testActor.getHighlighterNodeAttribute(
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
  yield mouse.move(100, 100);
  yield onEventHandled;
  markerHidden = yield testActor.getHighlighterNodeAttribute(
    "shapes-marker-hover", "hidden", highlighterFront);
  ok(markerHidden, "Marker on highlighter is no longer visible");
  ok(!pointSpan.classList.contains("active"), "Span for point 0 is no longer active");
  is(highlighters.state.shapes.hoverPoint, null, "Hovered point is null");
}
