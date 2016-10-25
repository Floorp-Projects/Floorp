/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

requestLongerTimeout(2);

// Check that animation endDelay is visualized in the timeline when the
// animation is delayed.
// Also check that negative endDelays do not overflow the UI, and are shown
// like positive endDelays.

add_task(function* () {
  yield addTab(URL_ROOT + "doc_end_delay.html");
  let {inspector, panel} = yield openAnimationInspector();

  let selectors = ["#target1", "#target2", "#target3", "#target4"];
  for (let i = 0; i < selectors.length; i++) {
    let selector = selectors[i];
    yield selectNode(selector, inspector);
    let timelineEl = panel.animationsTimelineComponent.rootWrapperEl;
    let animationEl = timelineEl.querySelectorAll(".animation")[0];
    checkEndDelayAndName(animationEl);
  }
});

function checkEndDelayAndName(animationEl) {
  let endDelay = animationEl.querySelector(".end-delay");
  let name = animationEl.querySelector(".name");
  let targetNode = animationEl.querySelector(".target");

  // Check that the endDelay element does not cause the timeline to overflow.
  let endDelayLeft = Math.round(endDelay.getBoundingClientRect().x);
  let sidebarWidth = Math.round(targetNode.getBoundingClientRect().width);
  ok(endDelayLeft >= sidebarWidth,
     "The endDelay element isn't displayed over the sidebar");

  // Check that the endDelay is not displayed on top of the name.
  let endDelayRight = Math.round(endDelay.getBoundingClientRect().right);
  let nameLeft = Math.round(name.getBoundingClientRect().left);
  ok(endDelayRight >= nameLeft,
     "The endDelay element does not span over the name element");
}
