/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test that shapes in iframes are updated correctly on mouse events.

const TEST_URL = URL_ROOT + "doc_inspector_highlighter_cssshapes_iframe.html";
const HIGHLIGHTER_TYPE = "ShapesHighlighter";

add_task(async function () {
  const env = await openInspectorForURL(TEST_URL);
  const helper = await getHighlighterHelperFor(HIGHLIGHTER_TYPE)(env);
  const { inspector } = env;
  const view = selectRuleView(inspector);
  const highlighters = view.highlighters;
  const config = { inspector, view, highlighters, helper };

  await testPolygonIframeMovePoint(config);
});

async function testPolygonIframeMovePoint(config) {
  const { inspector, view, helper } = config;
  const selector = "#polygon";
  const property = "clip-path";

  info(`Turn on shapes highlighter for ${selector}`);
  // Get a reference to the highlighter's target node inside the iframe.
  const highlightedNode = await getNodeFrontInFrames(
    ["#frame", selector],
    inspector
  );
  // Select the nested node so toggling of the shapes highlighter works from the rule view
  await selectNode(highlightedNode, inspector);
  await toggleShapesHighlighter(view, selector, property, true);
  const { mouse } = helper;

  // We expect two ruleview-changed events:
  // - one for previewing the change (DomRule::previewPropertyValue)
  // - one for applying the change (DomRule::applyProperties)
  let onRuleViewChanged = waitForNEvents(view, "ruleview-changed", 2);

  info("Moving polygon point visible in iframe");
  // Iframe has 10px margin. Element in iframe is 800px by 800px. First point is at 0 0%
  is(
    await getClipPathPoint(highlightedNode, 0),
    "0px 0%",
    `First point is at 0 0%`
  );

  await mouse.down(10, 10);
  await mouse.move(20, 20);
  await mouse.up();
  await reflowContentPage();
  await onRuleViewChanged;

  // point moved from y 0 to 10px, 10/800 (iframe height) = 1,25%
  is(
    await getClipPathPoint(highlightedNode, 0),
    "10px 1.25%",
    `Point moved to 10px 1.25%`
  );

  onRuleViewChanged = waitForNEvents(view, "ruleview-changed", 2);

  info("Dragging mouse out of the iframe");
  // Iframe has 10px margin. Element in iframe is 800px by 800px. Second point is at 100px 50%
  is(
    await getClipPathPoint(highlightedNode, 1),
    "100px 50%",
    `Second point is at 100px 50%`
  );

  await mouse.down(110, 410);
  await mouse.move(120, 510);
  await mouse.up();
  await reflowContentPage();
  await onRuleViewChanged;

  // The point can't be moved out of the iframe boundary, so we can't really assert the
  // y point here (as it depends on the horizontal scrollbar size + the shape control point size).
  ok(
    (await getClipPathPoint(highlightedNode, 1)).startsWith("110px"),
    `Point moved to 110px`
  );

  info(`Turn off shapes highlighter for ${selector}`);
  await toggleShapesHighlighter(view, selector, property, false);
}

async function getClipPathPoint(node, pointIndex) {
  const computedStyle = await node.inspectorFront.pageStyle.getComputed(node);
  const definition = computedStyle["clip-path"].value;
  const points = definition.replaceAll(/(^polygon\()|(\)$)/g, "").split(", ");
  return points[pointIndex];
}
