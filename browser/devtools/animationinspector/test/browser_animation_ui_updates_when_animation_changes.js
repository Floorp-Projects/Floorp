/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Verify that if the animation object changes in content, then the widget
// reflects that change.

add_task(function*() {
  yield addTab(TEST_URL_ROOT + "doc_simple_animation.html");
  let {panel, inspector} = yield openAnimationInspector();

  info("Select the test node");
  yield selectNode(".animated", inspector);

  info("Get the player widget");
  let widget = panel.playerWidgets[0];
  let player = widget.player;

  info("Wait for paused playState");
  let onPaused = waitForPlayState(player, "paused");

  info("Pause the animation via the content DOM");
  yield executeInContent("Test:ToggleAnimationPlayer", {
    selector: ".animated",
    animationIndex: 0,
    pause: true
  });

  yield onPaused;

  is(player.state.playState, "paused", "The AnimationPlayerFront is paused");
  ok(widget.el.classList.contains("paused"), "The button's state has changed");
  ok(!widget.rafID, "The smooth timeline animation has been stopped");

  info("Wait for running playState");
  let onRunning = waitForPlayState(player, "running");

  info("Play the animation via the content DOM");
  yield executeInContent("Test:ToggleAnimationPlayer", {
    selector: ".animated",
    animationIndex: 0,
    pause: false
  });

  yield onRunning;

  is(player.state.playState, "running", "The AnimationPlayerFront is running");
  ok(widget.el.classList.contains("running"), "The button's state has changed");
  ok(widget.rafID, "The smooth timeline animation has been started");
});
