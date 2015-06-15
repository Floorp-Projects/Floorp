/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check that the timeline-based UI is displayed instead of the playerwidget-
// based UI when the "devtools.inspector.animationInspectorV3" is set.

add_task(function*() {
  yield addTab(TEST_URL_ROOT + "doc_simple_animation.html");
  let {inspector, panel} = yield openAnimationInspectorNewUI();

  info("Selecting the test node");
  yield selectNode(".animated", inspector);

  let timeline = panel.animationsTimelineComponent;

  ok(timeline, "The timeline components was created");
  is(timeline.rootWrapperEl.parentNode, panel.playersEl,
    "The timeline component was appended in the DOM");
  is(panel.playersEl.querySelectorAll(".player-widget").length, 0,
    "There are no playerWidgets in the DOM");
});
