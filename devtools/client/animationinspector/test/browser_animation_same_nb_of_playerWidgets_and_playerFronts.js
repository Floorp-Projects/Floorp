/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

requestLongerTimeout(2);

// Check that when playerFronts are updated, the same number of playerWidgets
// are created in the panel.

add_task(function*() {
  yield addTab(TEST_URL_ROOT + "doc_simple_animation.html");
  let {inspector, panel, controller} = yield openAnimationInspector();
  let timeline = panel.animationsTimelineComponent;

  info("Selecting the test animated node again");
  yield selectNode(".multi", inspector);

  is(controller.animationPlayers.length,
    timeline.animationsEl.querySelectorAll(".animation").length,
    "As many timeline elements were created as there are playerFronts");
});
