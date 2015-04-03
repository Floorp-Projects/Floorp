/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that pressing the main toggle button also updates the displayed
// player widgets.

add_task(function*() {
  yield addTab(TEST_URL_ROOT + "doc_simple_animation.html");
  let {inspector, panel} = yield openAnimationInspector();

  info("Select an animated node");
  yield selectNode(".animated", inspector);
  let widget = panel.playerWidgets[0];
  let player = widget.player;

  info("Listen to animation state changes and click the toggle button to " +
    "pause all animations");
  let onPaused = waitForPlayState(player, "paused");
  yield panel.toggleAll();
  yield onPaused;

  info("Checking the selected node's animation player widget's state");
  is(player.state.playState, "paused", "The player front's state is paused");
  ok(widget.el.classList.contains("paused"), "The widget's UI is in paused state");

  info("Listen to animation state changes and click the toggle button to " +
    "play all animations");
  let onRunning = waitForPlayState(player, "running");
  yield panel.toggleAll();
  yield onRunning;

  info("Checking the selected node's animation player widget's state again");
  is(player.state.playState, "running", "The player front's state is running");
  ok(widget.el.classList.contains("running"), "The widget's UI is in running state");
});
