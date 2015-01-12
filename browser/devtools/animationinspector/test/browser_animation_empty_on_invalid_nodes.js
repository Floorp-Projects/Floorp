/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the panel shows no animation data for invalid or not animated nodes

add_task(function*() {
  yield addTab(TEST_URL_ROOT + "doc_simple_animation.html");
  let {inspector, panel} = yield openAnimationInspector();

  info("Select node .still and check that the panel is empty");
  let stillNode = yield getNodeFront(".still", inspector);
  yield selectNode(stillNode, inspector);
  ok(!panel.playerWidgets || !panel.playerWidgets.length,
    "No player widgets displayed for a still node");

  info("Select the comment text node and check that the panel is empty");
  let commentNode = yield inspector.walker.previousSibling(stillNode);
  yield selectNode(commentNode, inspector);
  ok(!panel.playerWidgets || !panel.playerWidgets.length,
    "No player widgets displayed for a text node");
});
