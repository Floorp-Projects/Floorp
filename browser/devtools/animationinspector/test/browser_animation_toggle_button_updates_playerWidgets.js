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

  info("Click the toggle button to pause all animations");
  let onRefresh = widget.player.once(widget.player.AUTO_REFRESH_EVENT);
  yield panel.toggleAll();
  yield onRefresh;

  info("Checking the selected node's animation player widget's state");
  is(widget.player.state.playState, "paused", "The player front's state is paused");
  ok(widget.el.classList.contains("paused"), "The widget's UI is in paused state");

  info("Click the toggle button to play all animations");
  onRefresh = widget.player.once(widget.player.AUTO_REFRESH_EVENT);
  yield panel.toggleAll();
  yield onRefresh;

  info("Checking the selected node's animation player widget's state again");
  is(widget.player.state.playState, "running", "The player front's state is running");
  ok(widget.el.classList.contains("running"), "The widget's UI is in running state");
});
