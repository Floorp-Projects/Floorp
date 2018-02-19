/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test that shapes in iframes are updated correctly on mouse events.

const TEST_URL = URL_ROOT + "doc_inspector_highlighter_cssshapes_iframe.html";
const HIGHLIGHTER_TYPE = "ShapesHighlighter";

add_task(function* () {
  let env = yield openInspectorForURL(TEST_URL);
  let helper = yield getHighlighterHelperFor(HIGHLIGHTER_TYPE)(env);
  let {testActor, inspector} = env;
  let view = selectRuleView(inspector);
  let highlighters = view.highlighters;
  let config = {inspector, view, highlighters, testActor, helper};

  yield testPolygonIframeMovePoint(config);
});

function* testPolygonIframeMovePoint(config) {
  const { inspector, view, testActor, helper } = config;
  const selector = "#polygon";
  const property = "clip-path";

  info(`Turn on shapes highlighter for ${selector}`);
  // Get a reference to the highlighter's target node inside the iframe.
  let highlightedNode = yield getNodeFrontInFrame(selector, "#frame", inspector);
  // Select the nested node so toggling of the shapes highlighter works from the rule view
  yield selectNode(highlightedNode, inspector);
  yield toggleShapesHighlighter(view, selector, property, true);
  let { mouse } = helper;

  let onRuleViewChanged = view.once("ruleview-changed");
  info("Moving polygon point visible in iframe");
  yield mouse.down(10, 10);
  yield mouse.move(20, 20);
  yield mouse.up();
  yield testActor.reflow();
  yield onRuleViewChanged;

  let computedStyle = yield inspector.pageStyle.getComputed(highlightedNode);
  let definition = computedStyle["clip-path"].value;
  ok(definition.includes("10px 10px"), "Point moved to 10px 10px");

  onRuleViewChanged = view.once("ruleview-changed");
  info("Moving polygon point not visible in iframe");
  yield mouse.down(110, 410);
  yield mouse.move(120, 420);
  yield mouse.up();
  yield testActor.reflow();
  yield onRuleViewChanged;

  computedStyle = yield inspector.pageStyle.getComputed(highlightedNode);
  definition = computedStyle["clip-path"].value;
  ok(definition.includes("110px 51.25%"), "Point moved to 110px 51.25%");
}
