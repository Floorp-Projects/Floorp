/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check that the AnimationPlayerFront objects lifecycle is managed by the
// AnimationController.

add_task(function*() {
  yield addTab(TEST_URL_ROOT + "doc_simple_animation.html");
  let {controller, inspector} = yield openAnimationInspector();

  info("Selecting an animated node");
  // selectNode waits for the inspector-updated event before resolving, which
  // means the controller.PLAYERS_UPDATED_EVENT event has been emitted before
  // and players are ready.
  yield selectNode(".animated", inspector);

  is(controller.animationPlayers.length, 1,
    "One AnimationPlayerFront has been created");
  ok(controller.animationPlayers[0].autoRefreshTimer,
    "The AnimationPlayerFront has been set to auto-refresh");

  info("Selecting a node with mutliple animations");
  yield selectNode(".multi", inspector);

  is(controller.animationPlayers.length, 2,
    "2 AnimationPlayerFronts have been created");
  ok(controller.animationPlayers[0].autoRefreshTimer &&
     controller.animationPlayers[1].autoRefreshTimer,
    "The AnimationPlayerFronts have been set to auto-refresh");

  // Hold on to one of the AnimationPlayerFront objects and mock its release
  // method to test that it is released correctly and that its auto-refresh is
  // stopped.
  let retainedFront = controller.animationPlayers[0];
  let oldRelease = retainedFront.release;
  let releaseCalled = false;
  retainedFront.release = () => {
    releaseCalled = true;
  };

  info("Selecting a node with no animations");
  yield selectNode(".still", inspector);

  is(controller.animationPlayers.length, 0,
    "There are no more AnimationPlayerFront objects");

  info("Checking the destroyed AnimationPlayerFront object");
  ok(releaseCalled, "The AnimationPlayerFront has been released");
  ok(!retainedFront.autoRefreshTimer,
    "The released AnimationPlayerFront's auto-refresh mode has been turned off");
  yield oldRelease.call(retainedFront);
});
