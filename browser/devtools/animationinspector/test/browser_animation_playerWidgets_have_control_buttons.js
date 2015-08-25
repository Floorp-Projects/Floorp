/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that playerWidgets have control buttons: play/pause, rewind, fast-forward.

add_task(function*() {
  yield addTab(TEST_URL_ROOT + "doc_simple_animation.html");
  let {controller, inspector, panel} = yield openAnimationInspector();

  info("Select the simple animated node");
  yield selectNode(".animated", inspector);

  let widget = panel.playerWidgets[0];
  let container = widget.el.querySelector(".playback-controls");

  ok(container, "The control buttons container exists");
  is(container.querySelectorAll("button").length, 3,
    "The container contains 3 buttons");
  ok(container.children[0].classList.contains("toggle"),
    "The first button is the play/pause button");
  ok(container.children[1].classList.contains("rw"),
    "The second button is the rewind button");
  ok(container.children[2].classList.contains("ff"),
    "The third button is the fast-forward button");
  ok(container.querySelector("select"),
    "The container contains the playback rate select");

  info("Faking an older server version by setting " +
    "AnimationsController.traits.hasSetCurrentTime to false");

  // Selecting <div.still> to make sure no widgets are displayed in the panel.
  yield selectNode(".still", inspector);
  controller.traits.hasSetCurrentTime = false;

  info("Selecting the animated node again");
  yield selectNode(".animated", inspector);

  widget = panel.playerWidgets[0];
  container = widget.el.querySelector(".playback-controls");

  ok(container, "The control buttons container still exists");
  is(container.querySelectorAll("button").length, 1,
    "The container only contains 1 button");
  ok(container.children[0].classList.contains("toggle"),
    "The first button is the play/pause button");

  yield selectNode(".still", inspector);
  controller.traits.hasSetCurrentTime = true;

  info("Faking an older server version by setting " +
    "AnimationsController.traits.hasSetPlaybackRate to false");

  controller.traits.hasSetPlaybackRate = false;

  info("Selecting the animated node again");
  yield selectNode(".animated", inspector);

  widget = panel.playerWidgets[0];
  container = widget.el.querySelector(".playback-controls");

  ok(container, "The control buttons container still exists");
  ok(!container.querySelector("select"),
    "The playback rate select does not exist");

  yield selectNode(".still", inspector);
  controller.traits.hasSetPlaybackRate = true;
});
