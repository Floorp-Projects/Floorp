/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

requestLongerTimeout(2);

// Check that when playerFronts are updated, the same number of playerWidgets
// are created in the panel.

add_task(async function() {
  await addTab(URL_ROOT + "doc_simple_animation.html");
  const {inspector, panel, controller} = await openAnimationInspector();
  const timeline = panel.animationsTimelineComponent;

  info("Selecting the test animated node again");
  await selectNodeAndWaitForAnimations(".multi", inspector);

  is(controller.animationPlayers.length,
    timeline.animationsEl.querySelectorAll(".animation").length,
    "As many timeline elements were created as there are playerFronts");
});
