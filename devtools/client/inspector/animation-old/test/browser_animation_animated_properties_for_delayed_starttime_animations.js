/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for animations that have different starting time.
// We should check progress indicator working well even if the start time is not zero.
// Also, check that there is no duplication display.

add_task(async function() {
  await addTab(URL_ROOT + "doc_delayed_starttime_animations.html");
  const { panel } = await openAnimationInspector();
  await setStyle(null, panel, "animation", "anim 100s", "#target2");
  await setStyle(null, panel, "animation", "anim 100s", "#target3");
  await setStyle(null, panel, "animation", "anim 100s", "#target4");
  await setStyle(null, panel, "animation", "anim 100s", "#target5");

  const timelineComponent = panel.animationsTimelineComponent;
  const detailsComponent = timelineComponent.details;
  const headers =
    detailsComponent.containerEl.querySelectorAll(".animated-properties-header");
  is(headers.length, 1, "There should be only one header in the details panel");

  // Check indicator.
  await clickOnAnimation(panel, 1);
  const progressIndicatorEl = detailsComponent.progressIndicatorEl;
  const startTime = detailsComponent.animation.state.previousStartTime;
  detailsComponent.indicateProgress(0);
  is(progressIndicatorEl.style.left, "0%",
     "The progress indicator position should be 0% at 0ms");
  detailsComponent.indicateProgress(startTime);
  is(progressIndicatorEl.style.left, "0%",
     "The progress indicator position should be 0% at start time");
  detailsComponent.indicateProgress(startTime + 50 * 1000);
  is(progressIndicatorEl.style.left, "50%",
     "The progress indicator position should be 50% at half time of animation");
  detailsComponent.indicateProgress(startTime + 99 * 1000);
  is(progressIndicatorEl.style.left, "99%",
     "The progress indicator position should be 99% at 99s");
  detailsComponent.indicateProgress(startTime + 100 * 1000);
  is(progressIndicatorEl.style.left, "0%",
     "The progress indicator position should be 0% at end of animation");
});
