/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that when first hovering over a node and immediately after selecting it
// by clicking on it leaves the highlighter visible for as long as the mouse is
// over the node

let test = asyncTest(function*() {
  let {inspector} = yield addTab("data:text/html,<p>It's going to be legen....</p>").then(openInspector);
  let p = getNode("p");

  info("hovering over the <p> line in the markup-view");
  yield hoverContainer("p", inspector);
  ok(isHighlighterVisible(), "the highlighter is still visible");

  info("selecting the <p> line by clicking in the markup-view");
  yield clickContainer("p", inspector);

  p.textContent = "wait for it ....";
  info("wait and see if the highlighter stays visible even after the node was selected");
  yield waitForTheBrieflyShowBoxModelTimeout();

  let updated = inspector.once("inspector-updated");
  p.textContent = "dary!!!!";
  ok(isHighlighterVisible(), "the highlighter is still visible");
  yield updated;
});

function waitForTheBrieflyShowBoxModelTimeout() {
  let deferred = promise.defer();
  // Note that the current timeout is 1 sec and is neither configurable nor
  // exported anywhere we can access, so hard-coding the timeout
  content.setTimeout(deferred.resolve, 1500);
  return deferred.promise;
}
