/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that when after an element is selected and highlighted on hover, if the
// mouse leaves the markup-view and comes back again on the same element, that
// the highlighter is shown again on the node

const TEST_URL = "data:text/html;charset=utf-8,<p>Select me!</p>";

add_task(async function() {
  const { inspector, highlighterTestFront } = await openInspectorForURL(
    TEST_URL
  );
  const { waitForHighlighterTypeHidden } = getHighlighterTestHelpers(inspector);

  info(
    "hover over the <p> line in the markup-view so that it's the " +
      "currently hovered node"
  );
  await hoverContainer("p", inspector);

  info("select the <p> markup-container line by clicking");
  await clickContainer("p", inspector);
  let isVisible = await highlighterTestFront.isHighlighting();
  ok(isVisible, "the highlighter is shown");

  const onHidden = waitForHighlighterTypeHidden(
    inspector.highlighters.TYPES.BOXMODEL
  );
  info("mouse-leave the markup-view");
  await mouseLeaveMarkupView(inspector);
  info("listen to the highlighter's hidden event");
  await onHidden;
  info("check that the highlighter is no longer visible");
  isVisible = await highlighterTestFront.isHighlighting();
  ok(!isVisible, "the highlighter is hidden after mouseleave");

  info("hover over the <p> line again, which is still selected");
  await hoverContainer("p", inspector);
  isVisible = await highlighterTestFront.isHighlighting();
  ok(isVisible, "the highlighter is visible again");
});
