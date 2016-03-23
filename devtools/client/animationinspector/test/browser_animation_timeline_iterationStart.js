/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check iteration start.

add_task(function*() {
  yield addTab(URL_ROOT + "doc_script_animation.html");
  let {panel, controller} = yield openAnimationInspector();

  info("Getting the animation element by script");
  let timelineEl = panel.animationsTimelineComponent.rootWrapperEl;
  let timeBlockNameEl = timelineEl.querySelector(".time-block .name");

  let state = controller.animationPlayers[0].state;
  let title = timeBlockNameEl.getAttribute("title");
  if (state.iterationStart !== 0) {
    let iterationStartTime = state.iterationStart * state.duration / 1000;
    let iterationStartTimeString =
      iterationStartTime.toLocaleString(undefined,
                                        { maximumFractionDigits: 2,
                                          minimumFractionDigits: 2 })
                                       .replace(".", "\\.");
    let iterationStartString =
      state.iterationStart.toString().replace(".", "\\.");
    let regex = new RegExp("Iteration start: " + iterationStartString +
                           " \\(" + iterationStartTimeString + "s\\)");
    ok(title.match(regex), "The tooltip shows the iteration start");
  } else {
    ok(!title.match(/Iteration start:/),
      "The tooltip doesn't show the iteration start");
  }
});

