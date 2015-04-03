/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the currentTime timeline widget actually progresses with the
// animation itself.

add_task(function*() {
  yield addTab(TEST_URL_ROOT + "doc_simple_animation.html");
  let {inspector, panel} = yield openAnimationInspector();

  info("Select the animated node");
  yield selectNode(".animated", inspector);

  info("Get the player widget's timeline element and its current position");
  let widget = panel.playerWidgets[0];
  let timeline = widget.currentTimeEl;

  yield onceNextPlayerRefresh(widget.player);
  ok(widget.rafID, "The widget is updating the timeline with a rAF loop");

  info("Pause the animation");
  yield togglePlayPauseButton(widget);

  ok(!widget.rafID, "The rAF loop has been stopped after the animation was paused");
});
