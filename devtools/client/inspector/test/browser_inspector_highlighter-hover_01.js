/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that when first hovering over a node and immediately after selecting it
// by clicking on it leaves the highlighter visible for as long as the mouse is
// over the node

const TEST_URL = "data:text/html;charset=utf-8,<p>It's going to be legen....</p>";

add_task(function* () {
  let {toolbox, inspector, testActor} = yield openInspectorForURL(TEST_URL);

  info("hovering over the <p> line in the markup-view");
  yield hoverContainer("p", inspector);
  let isVisible = yield testActor.isHighlighting();
  ok(isVisible, "the highlighter is still visible");

  info("selecting the <p> line by clicking in the markup-view");
  yield clickContainer("p", inspector);

  yield testActor.setProperty("p", "textContent", "wait for it ....");
  info("wait and see if the highlighter stays visible even after the node was selected");
  yield waitForTheBrieflyShowBoxModelTimeout();

  yield testActor.setProperty("p", "textContent", "dary!!!!");
  isVisible = yield testActor.isHighlighting();
  ok(isVisible, "the highlighter is still visible");
});

function waitForTheBrieflyShowBoxModelTimeout() {
  let deferred = promise.defer();
  // Note that the current timeout is 1 sec and is neither configurable nor
  // exported anywhere we can access, so hard-coding the timeout
  setTimeout(deferred.resolve, 1500);
  return deferred.promise;
}
