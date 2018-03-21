/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test progress indicator in animated properties.
// Since this indicator works with the timeline, after selecting each animation,
// click the timeline header to change the current time and check the change.

add_task(async function() {
  await addTab(URL_ROOT + "doc_multiple_property_types.html");
  const { panel } = await openAnimationInspector();
  const timelineComponent = panel.animationsTimelineComponent;
  const detailsComponent = timelineComponent.details;

  info("Click to select the animation");
  await clickOnAnimation(panel, 0);
  let progressIndicatorEl = detailsComponent.progressIndicatorEl;
  ok(progressIndicatorEl, "The progress indicator should be exist");
  await clickOnTimelineHeader(panel, 0);
  is(progressIndicatorEl.style.left, "0%",
     "The left style of progress indicator element should be 0% at 0ms");
  await clickOnTimelineHeader(panel, 0.5);
  approximate(progressIndicatorEl.style.left, "50%",
              "The left style of progress indicator element should be "
              + "approximately 50% at 500ms");
  await clickOnTimelineHeader(panel, 1);
  is(progressIndicatorEl.style.left, "100%",
     "The left style of progress indicator element should be 100% at 1000ms");

  info("Click to select the steps animation");
  await clickOnAnimation(panel, 4);
  // Re-get progressIndicatorEl since this element re-create
  // in case of select the animation.
  progressIndicatorEl = detailsComponent.progressIndicatorEl;
  // Use indicateProgess directly from here since
  // MouseEvent.clientX may not be able to indicate finely
  // in case of the width of header element * xPositionRate has a fraction.
  detailsComponent.indicateProgress(499);
  is(progressIndicatorEl.style.left, "0%",
     "The left style of progress indicator element should be 0% at 0ms");
  detailsComponent.indicateProgress(499);
  is(progressIndicatorEl.style.left, "0%",
     "The left style of progress indicator element should be 0% at 499ms");
  detailsComponent.indicateProgress(500);
  is(progressIndicatorEl.style.left, "50%",
     "The left style of progress indicator element should be 50% at 500ms");
  detailsComponent.indicateProgress(999);
  is(progressIndicatorEl.style.left, "50%",
     "The left style of progress indicator element should be 50% at 999ms");
  await clickOnTimelineHeader(panel, 1);
  is(progressIndicatorEl.style.left, "100%",
     "The left style of progress indicator element should be 100% at 1000ms");

  info("Change the playback rate");
  await changeTimelinePlaybackRate(panel, 2);
  await clickOnAnimation(panel, 0);
  progressIndicatorEl = detailsComponent.progressIndicatorEl;
  await clickOnTimelineHeader(panel, 0);
  is(progressIndicatorEl.style.left, "0%",
     "The left style of progress indicator element should be 0% "
     + "at 0ms and playback rate 2");
  detailsComponent.indicateProgress(250);
  is(progressIndicatorEl.style.left, "50%",
     "The left style of progress indicator element should be 50% "
     + "at 250ms and playback rate 2");
  detailsComponent.indicateProgress(500);
  is(progressIndicatorEl.style.left, "100%",
     "The left style of progress indicator element should be 100% "
     + "at 500ms and playback rate 2");

  info("Check the progress indicator position after select another animation");
  await changeTimelinePlaybackRate(panel, 1);
  await clickOnTimelineHeader(panel, 0.5);
  const originalIndicatorPosition = progressIndicatorEl.style.left;
  await clickOnAnimation(panel, 1);
  is(progressIndicatorEl.style.left, originalIndicatorPosition,
     "The animation time should be continued even if another animation selects");
});

function approximate(percentageString1, percentageString2, message) {
  const val1 = Math.round(parseFloat(percentageString1));
  const val2 = Math.round(parseFloat(percentageString2));
  is(val1, val2, message);
}
