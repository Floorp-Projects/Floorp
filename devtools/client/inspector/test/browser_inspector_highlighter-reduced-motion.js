/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Check that the boxmodel highlighter is styled differently when
// ui.prefersReducedMotion is enabled.

const TEST_URL =
  "data:text/html;charset=utf-8,<h1>test1</h1><h2>test2</h2><h3>test3</h3>";

add_task(async function () {
  info("Disable ui.prefersReducedMotion");
  await pushPref("ui.prefersReducedMotion", 0);

  info("Enable simple highlighters");
  await pushPref("devtools.inspector.simple-highlighters-reduced-motion", true);

  const { highlighterTestFront, inspector } = await openInspectorForURL(
    TEST_URL
  );
  const HIGHLIGHTER_TYPE = inspector.highlighters.TYPES.BOXMODEL;

  const front = inspector.inspectorFront;
  const highlighterFront = await front.getHighlighterByType(HIGHLIGHTER_TYPE);

  // Small helper to retrieve the computed style of a specific highlighter
  // element.
  const getElementComputedStyle = async (id, property) => {
    info(`Retrieve computed style for property ${property} on element ${id}`);
    return highlighterTestFront.getHighlighterComputedStyle(
      id,
      property,
      highlighterFront
    );
  };

  info("Highlight a node and check the highlighter is filled");
  await selectAndHighlightNode("h1", inspector);
  let stroke = await getElementComputedStyle("box-model-content", "stroke");
  let fill = await getElementComputedStyle("box-model-content", "fill");
  is(
    stroke,
    "none",
    "If prefersReducedMotion is disabled, stroke style is none"
  );
  ok(
    InspectorUtils.isValidCSSColor(fill),
    "If prefersReducedMotion is disabled, fill style is a valid color"
  );
  await inspector.highlighters.hideHighlighterType(HIGHLIGHTER_TYPE);

  info("Enable ui.prefersReducedMotion");
  await pushPref("ui.prefersReducedMotion", 1);

  info("Highlight a node and check the highlighter uses stroke and not fill");
  await selectAndHighlightNode("h2", inspector);
  stroke = await getElementComputedStyle("box-model-content", "stroke");
  fill = await getElementComputedStyle("box-model-content", "fill");
  ok(
    InspectorUtils.isValidCSSColor(stroke),
    "If prefersReducedMotion is enabled, stroke style is a valid color"
  );
  is(fill, "none", "If prefersReducedMotion is enabled, fill style is none");

  await inspector.highlighters.hideHighlighterType(HIGHLIGHTER_TYPE);

  info("Disable simple highlighters");
  await pushPref(
    "devtools.inspector.simple-highlighters-reduced-motion",
    false
  );

  info("Highlight a node and check the highlighter is filled again");
  await selectAndHighlightNode("h3", inspector);
  stroke = await getElementComputedStyle("box-model-content", "stroke");
  fill = await getElementComputedStyle("box-model-content", "fill");
  is(
    stroke,
    "none",
    "If simple highlighters are disabled, stroke style is none"
  );
  ok(
    InspectorUtils.isValidCSSColor(fill),
    "If simple highlighters are disabled, fill style is a valid color"
  );
  await inspector.highlighters.hideHighlighterType(HIGHLIGHTER_TYPE);
});
