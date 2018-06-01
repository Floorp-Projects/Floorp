/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check that if an animation has had its playbackRate changed via the DOM, then
// the timeline UI shows the right delay and duration.
// Indeed, the header in the timeline UI always shows the unaltered time,
// because there might be multiple animations displayed at the same time, some
// of which may have a different rate than others. Those that have had their
// rate changed have a delay = delay/rate and a duration = duration/rate.

add_task(async function() {
  await addTab(URL_ROOT + "doc_modify_playbackRate.html");

  const {panel} = await openAnimationInspector();

  const timelineEl = panel.animationsTimelineComponent.rootWrapperEl;

  const timeBlocks = timelineEl.querySelectorAll(".time-block");
  is(timeBlocks.length, 2, "2 animations are displayed");

  info("The first animation has its rate set to 1, let's measure it");

  const el = timeBlocks[0];
  const delay = parseInt(el.querySelector(".delay").style.width, 10);
  let duration = null;
  el.querySelectorAll("svg g").forEach(groupEl => {
    const dur = getDuration(groupEl.querySelector("path"));
    if (!duration) {
      duration = dur;
      return;
    }
    is(duration, dur, "The durations shuld be same at all paths in one group");
  });

  info("The second animation has its rate set to 2, so should be shorter");

  const el2 = timeBlocks[1];
  const delay2 = parseInt(el2.querySelector(".delay").style.width, 10);
  let duration2 = null;
  el2.querySelectorAll("svg g").forEach(groupEl => {
    const dur = getDuration(groupEl.querySelector("path"));
    if (!duration2) {
      duration2 = dur;
      return;
    }
    is(duration2, dur, "The durations shuld be same at all paths in one group");
  });

  // The width are calculated by the animation-inspector dynamically depending
  // on the size of the panel, and therefore depends on the test machine/OS.
  // Let's not try to be too precise here and compare numbers.
  const durationDelta = (2 * duration2) - duration;
  ok(durationDelta <= 1, "The duration width is correct");
  const delayDelta = (2 * delay2) - delay;
  ok(delayDelta <= 1, "The delay width is correct");
});

function getDuration(pathEl) {
  const pathSegList = pathEl.pathSegList;
  // Find the index of starting iterations.
  let startingIterationIndex = 0;
  const firstPathSeg = pathSegList.getItem(1);
  for (let i = 2, n = pathSegList.numberOfItems - 2; i < n; i++) {
    // Changing point of the progress acceleration is the time.
    const pathSeg = pathSegList.getItem(i);
    if (firstPathSeg.y != pathSeg.y) {
      startingIterationIndex = i;
      break;
    }
  }
  // Find the index of ending iterations.
  let endingIterationIndex = 0;
  let previousPathSegment = pathSegList.getItem(startingIterationIndex);
  for (let i = startingIterationIndex + 1, n = pathSegList.numberOfItems - 2;
       i < n; i++) {
    // Find forwards fill-mode.
    const pathSeg = pathSegList.getItem(i);
    if (previousPathSegment.y == pathSeg.y) {
      endingIterationIndex = i;
      break;
    }
    previousPathSegment = pathSeg;
  }
  if (endingIterationIndex) {
    // Not forwards fill-mode
    endingIterationIndex = pathSegList.numberOfItems - 2;
  }
  // Return the distance of starting and ending
  const startingIterationPathSegment =
    pathSegList.getItem(startingIterationIndex);
  const endingIterationPathSegment =
    pathSegList.getItem(startingIterationIndex);
  return endingIterationPathSegment.x - startingIterationPathSegment.x;
}
