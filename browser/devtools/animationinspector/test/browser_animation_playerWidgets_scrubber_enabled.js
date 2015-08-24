/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the currentTime timeline widget is enabled and that the associated
// player front supports setting the current time.

add_task(function*() {
  yield addTab(TEST_URL_ROOT + "doc_simple_animation.html");
  let {controller, inspector, panel} = yield openAnimationInspector();

  info("Select the animated node");
  yield selectNode(".animated", inspector);

  info("Get the player widget's timeline element");
  let widget = panel.playerWidgets[0];
  let timeline = widget.currentTimeEl;

  ok(!timeline.hasAttribute("disabled"), "The timeline input[range] is enabled");
  ok(widget.setCurrentTime, "The widget has the setCurrentTime method");
  ok(widget.player.setCurrentTime, "The associated player front has the setCurrentTime method");

  info("Faking an older server version by setting " +
    "AnimationsController.traits.hasSetCurrentTime to false");

  yield selectNode("body", inspector);
  controller.traits.hasSetCurrentTime = false;

  yield selectNode(".animated", inspector);

  info("Get the player widget's timeline element");
  widget = panel.playerWidgets[0];
  timeline = widget.currentTimeEl;

  ok(timeline.hasAttribute("disabled"), "The timeline input[range] is disabled");

  yield selectNode("body", inspector);
  controller.traits.hasSetCurrentTime = true;
});
