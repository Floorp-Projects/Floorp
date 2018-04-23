/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

requestLongerTimeout(2);

// Check that the AnimationPlayerFront objects lifecycle is managed by the
// AnimationController.

add_task(async function() {
  await addTab(URL_ROOT + "doc_simple_animation.html");
  let {controller, inspector} = await openAnimationInspector();

  info("Selecting an animated node");
  // selectNode waits for the inspector-updated event before resolving, which
  // means the controller.PLAYERS_UPDATED_EVENT event has been emitted before
  // and players are ready.
  await selectNodeAndWaitForAnimations(".animated", inspector);

  is(controller.animationPlayers.length, 1,
    "One AnimationPlayerFront has been created");

  info("Selecting a node with mutliple animations");
  await selectNodeAndWaitForAnimations(".multi", inspector);

  is(controller.animationPlayers.length, 2,
    "2 AnimationPlayerFronts have been created");

  info("Selecting a node with no animations");
  await selectNodeAndWaitForAnimations(".still", inspector);

  is(controller.animationPlayers.length, 0,
    "There are no more AnimationPlayerFront objects");
});
