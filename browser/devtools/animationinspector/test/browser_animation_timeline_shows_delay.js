/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check that animation delay is visualized in the timeline-based UI when the
// animation is delayed.
// Also check that negative delays do not overflow the UI, and are shown like
// positive delays.

add_task(function*() {
  yield addTab(TEST_URL_ROOT + "doc_simple_animation.html");
  let {inspector, panel} = yield openAnimationInspectorNewUI();

  info("Selecting a delayed animated node");
  yield selectNode(".delayed", inspector);
  let timelineEl = panel.animationsTimelineComponent.rootWrapperEl;
  checkDelayAndName(timelineEl, true);

  info("Selecting a no-delay animated node");
  yield selectNode(".animated", inspector);
  checkDelayAndName(timelineEl, false);

  info("Selecting a negative-delay animated node");
  yield selectNode(".negative-delay", inspector);
  checkDelayAndName(timelineEl, true);
});

function checkDelayAndName(timelineEl, hasDelay) {
  let delay = timelineEl.querySelector(".delay");

  is(!!delay, hasDelay, "The timeline " +
                        (hasDelay ? "contains" : "does not contain") +
                        " a delay element, as expected");

  if (hasDelay) {
    let name = timelineEl.querySelector(".name");
    let targetNode = timelineEl.querySelector(".target");

    // Check that the delay element does not cause the timeline to overflow.
    let delayRect = delay.getBoundingClientRect();
    let sidebarWidth = targetNode.getBoundingClientRect().width;
    ok(delayRect.x >= sidebarWidth,
       "The delay element isn't displayed over the sidebar");

    // Check that the delay is not displayed on top of the name.
    let nameLeft = name.getBoundingClientRect().left;
    ok(delayRect.right <= nameLeft,
       "The delay element does not span over the name element");
  }
}
