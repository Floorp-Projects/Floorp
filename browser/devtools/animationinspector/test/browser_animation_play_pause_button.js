/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check that the play/pause button actually plays and pauses the player.

add_task(function*() {
  yield addTab(TEST_URL_ROOT + "doc_simple_animation.html");
  let {inspector, panel, controller} = yield openAnimationInspector();

  info("Selecting an animated node");
  yield selectNode(".animated", inspector);

  let player = controller.animationPlayers[0];
  let widget = panel.playerWidgets[0];

  info("Click the pause button");
  yield togglePlayPauseButton(widget);

  is(player.state.playState, "paused", "The AnimationPlayerFront is paused");
  ok(widget.el.classList.contains("paused"), "The button's state has changed");
  ok(!widget.rafID, "The smooth timeline animation has been stopped");

  info("Click on the play button");
  yield togglePlayPauseButton(widget);

  is(player.state.playState, "running", "The AnimationPlayerFront is running");
  ok(widget.el.classList.contains("running"), "The button's state has changed");
  ok(widget.rafID, "The smooth timeline animation has been started");
});
