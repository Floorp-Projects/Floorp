/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

requestLongerTimeout(2);

// Check that the timeline shows correct time graduations in the header.

const {findOptimalTimeInterval, TimeScale} = require("devtools/client/inspector/animation-old/utils");

// Should be kept in sync with TIME_GRADUATION_MIN_SPACING in
// animation-timeline.js
const TIME_GRADUATION_MIN_SPACING = 40;

add_task(async function() {
  await pushPref("devtools.inspector.three-pane-enabled", false);
  await addTab(URL_ROOT + "doc_simple_animation.html");

  // System scrollbar is enabled by default on our testing envionment and it
  // would shrink width of inspector and affect number of time-ticks causing
  // unexpected results. So, we set it wider to avoid this kind of edge case.
  await pushPref("devtools.toolsidebar-width.inspector", 350);

  let {panel} = await openAnimationInspector();

  let timeline = panel.animationsTimelineComponent;
  let headerEl = timeline.timeHeaderEl;

  info("Find out how many time graduations should there be");
  let width = headerEl.offsetWidth;

  let animationDuration = TimeScale.maxEndTime - TimeScale.minStartTime;
  let minTimeInterval = TIME_GRADUATION_MIN_SPACING * animationDuration / width;

  // Note that findOptimalTimeInterval is tested separately in xpcshell test
  // test_findOptimalTimeInterval.js, so we assume that it works here.
  let interval = findOptimalTimeInterval(minTimeInterval);
  let nb = Math.ceil(animationDuration / interval);

  is(headerEl.querySelectorAll(".header-item").length, nb,
     "The expected number of time ticks were found");

  info("Make sure graduations are evenly distributed and show the right times");
  [...headerEl.querySelectorAll(".time-tick")].forEach((tick, i) => {
    let left = parseFloat(tick.style.left);
    let expectedPos = i * interval * 100 / animationDuration;
    is(Math.round(left), Math.round(expectedPos),
      `Graduation ${i} is positioned correctly`);

    // Note that the distancetoRelativeTime and formatTime functions are tested
    // separately in xpcshell test test_timeScale.js, so we assume that they
    // work here.
    let formattedTime = TimeScale.formatTime(
      TimeScale.distanceToRelativeTime(expectedPos, width));
    is(tick.textContent, formattedTime,
      `Graduation ${i} has the right text content`);
  });
});
