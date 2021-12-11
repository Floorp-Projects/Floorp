/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable mozilla/no-arbitrary-setTimeout */

"use strict";

// Test that when first hovering over a node and immediately after selecting it
// by clicking on it, the highlighter stays visible

const TEST_URL =
  "data:text/html;charset=utf-8," + "<p>It's going to be legen....</p>";

add_task(async function() {
  const { inspector, highlighterTestFront } = await openInspectorForURL(
    TEST_URL
  );

  info("hovering over the <p> line in the markup-view");
  await hoverContainer("p", inspector);
  let isVisible = await highlighterTestFront.isHighlighting();
  ok(isVisible, "the highlighter is still visible");

  info("selecting the <p> line by clicking in the markup-view");
  await clickContainer("p", inspector);

  info(
    "wait and see if the highlighter stays visible even after the node " +
      "was selected"
  );

  await setContentPageElementProperty("p", "textContent", "dary!!!!");
  isVisible = await highlighterTestFront.isHighlighting();
  ok(isVisible, "the highlighter is still visible");
});
