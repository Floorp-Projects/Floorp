/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for following timeline tick items.
// * animation list header elements existence
// * timeline tick item elements existence
// * count and label of timeline tick elements changing by the sidebar width

const TimeScale = require("devtools/client/inspector/animation/utils/timescale");
const { findOptimalTimeInterval } =
  require("devtools/client/inspector/animation/utils/utils");

// Should be kept in sync with TIME_GRADUATION_MIN_SPACING in
// AnimationTimeTickList component.
const TIME_GRADUATION_MIN_SPACING = 40;

add_task(async function() {
  await pushPref("devtools.inspector.three-pane-enabled", false);
  await addTab(URL_ROOT + "doc_simple_animation.html");
  await removeAnimatedElementsExcept([".end-delay", ".negative-delay"]);
  const { animationInspector, inspector, panel } = await openAnimationInspector();
  const timeScale = new TimeScale(animationInspector.state.animations);

  info("Checking animation list header element existence");
  const listContainerEl = panel.querySelector(".animation-list-container");
  const listHeaderEl = listContainerEl.querySelector(".devtools-toolbar");
  ok(listHeaderEl, "The header element should be in animation list container element");

  info("Checking time tick item elements existence");
  assertTimelineTickItems(timeScale, listContainerEl);
  const timelineTickItemLength =
    listContainerEl.querySelectorAll(".animation-timeline-tick-item").length;

  info("Checking timeline tick item elements after enlarge sidebar width");
  await setSidebarWidth("100%", inspector);
  assertTimelineTickItems(timeScale, listContainerEl);
  ok(timelineTickItemLength <
     listContainerEl.querySelectorAll(".animation-timeline-tick-item").length,
     "The timeline tick item elements should increase");
});

/**
 * Assert timeline tick item's position and label.
 *
 * @param {TimeScale} - timeScale
 * @param {Element} - listContainerEl
 */
function assertTimelineTickItems(timeScale, listContainerEl) {
  const timelineTickListEl =
    listContainerEl.querySelector(".animation-timeline-tick-list");
  ok(timelineTickListEl,
    "The animation timeline tick list element should be in header");

  const width = timelineTickListEl.offsetWidth;
  const animationDuration = timeScale.getDuration();
  const minTimeInterval = TIME_GRADUATION_MIN_SPACING * animationDuration / width;
  const interval = findOptimalTimeInterval(minTimeInterval);
  const expectedTickItem = Math.ceil(animationDuration / interval);

  const timelineTickItemEls =
    timelineTickListEl.querySelectorAll(".animation-timeline-tick-item");
  is(timelineTickItemEls.length, expectedTickItem,
    "The expected number of timeline ticks were found");

  info("Make sure graduations are evenly distributed and show the right times");
  for (const [index, tickEl] of timelineTickItemEls.entries()) {
    const left = parseFloat(tickEl.style.left);
    const expectedPos = index * interval * 100 / animationDuration;
    is(Math.round(left), Math.round(expectedPos),
      `Graduation ${ index } is positioned correctly`);

    // Note that the distancetoRelativeTime and formatTime functions are tested
    // separately in xpcshell test test_timeScale.js, so we assume that they
    // work here.
    const formattedTime =
      timeScale.formatTime(timeScale.distanceToRelativeTime(expectedPos, width));
    is(tickEl.textContent, formattedTime,
      `Graduation ${ index } has the right text content`);
  }
}
