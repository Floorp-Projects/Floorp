/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that when after an element is selected and highlighted on hover, if the
// mouse leaves the markup-view and comes back again on the same element, that
// the highlighter is shown again on the node

let test = asyncTest(function*() {
  let {inspector} = yield addTab("data:text/html,<p>Select me!</p>").then(openInspector);

  info("hover over the <p> line in the markup-view so that it's the currently hovered node");
  yield hoverContainer("p", inspector);

  info("select the <p> markup-container line by clicking");
  yield clickContainer("p", inspector);
  ok(isHighlighterVisible(), "the highlighter is shown");

  info("mouse-leave the markup-view");
  yield mouseLeaveMarkupView(inspector);
  ok(!isHighlighterVisible(), "the highlighter is hidden after mouseleave");

  info("hover over the <p> line again, which is still selected");
  yield hoverContainer("p", inspector);
  ok(isHighlighterVisible(), "the highlighter is visible again");
});
