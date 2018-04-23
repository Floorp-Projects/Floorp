/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

requestLongerTimeout(2);

// Test that the panel shows no animation data for invalid or not animated nodes

add_task(async function() {
  await addTab(URL_ROOT + "doc_simple_animation.html");
  let {inspector, panel, window} = await openAnimationInspector();
  let {document} = window;

  info("Select node .still and check that the panel is empty");
  let stillNode = await getNodeFront(".still", inspector);
  await selectNodeAndWaitForAnimations(stillNode, inspector);

  is(panel.animationsTimelineComponent.animations.length, 0,
     "No animation players stored in the timeline component for a still node");
  is(panel.animationsTimelineComponent.animationsEl.childNodes.length, 0,
     "No animation displayed in the timeline component for a still node");
  is(document.querySelector("#error-type").textContent,
     ANIMATION_L10N.getStr("panel.invalidElementSelected"),
     "The correct error message is displayed");

  info("Select the comment text node and check that the panel is empty");
  let commentNode = await inspector.walker.previousSibling(stillNode);
  await selectNodeAndWaitForAnimations(commentNode, inspector);

  is(panel.animationsTimelineComponent.animations.length, 0,
     "No animation players stored in the timeline component for a text node");
  is(panel.animationsTimelineComponent.animationsEl.childNodes.length, 0,
     "No animation displayed in the timeline component for a text node");
  is(document.querySelector("#error-type").textContent,
     ANIMATION_L10N.getStr("panel.invalidElementSelected"),
     "The correct error message is displayed");
});
