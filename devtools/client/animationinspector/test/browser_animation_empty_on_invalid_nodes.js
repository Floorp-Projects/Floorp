/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the panel shows no animation data for invalid or not animated nodes

add_task(function*() {
  yield addTab(TEST_URL_ROOT + "doc_simple_animation.html");

  let {inspector, panel} = yield openAnimationInspector();
  yield testEmptyPanel(inspector, panel);
});

function* testEmptyPanel(inspector, panel) {
  info("Select node .still and check that the panel is empty");
  let stillNode = yield getNodeFront(".still", inspector);
  let onUpdated = panel.once(panel.UI_UPDATED_EVENT);
  yield selectNode(stillNode, inspector);
  yield onUpdated;

  is(panel.animationsTimelineComponent.animations.length, 0,
     "No animation players stored in the timeline component for a still node");
  is(panel.animationsTimelineComponent.animationsEl.childNodes.length, 0,
     "No animation displayed in the timeline component for a still node");

  info("Select the comment text node and check that the panel is empty");
  let commentNode = yield inspector.walker.previousSibling(stillNode);
  onUpdated = panel.once(panel.UI_UPDATED_EVENT);
  yield selectNode(commentNode, inspector);
  yield onUpdated;

  is(panel.animationsTimelineComponent.animations.length, 0,
     "No animation players stored in the timeline component for a text node");
  is(panel.animationsTimelineComponent.animationsEl.childNodes.length, 0,
     "No animation displayed in the timeline component for a text node");}
