/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the panel content refreshes when animations are removed.

add_task(function*() {
  yield addTab(TEST_URL_ROOT + "doc_simple_animation.html");
  let {toolbox, inspector, panel} = yield openAnimationInspector();

  info("Select a animated node");
  yield selectNode(".animated", inspector);

  is(panel.playersEl.querySelectorAll(".player-widget").length, 1,
    "There is one player widget in the panel");

  info("Listen to the next UI update event");
  let onPanelUpdated = panel.once(panel.UI_UPDATED_EVENT);

  info("Remove the animation on the node by removing the class");
  yield executeInContent("devtools:test:setAttribute", {
    selector: ".animated",
    attributeName: "class",
    attributeValue: "ball still test-node"
  });

  yield onPanelUpdated;
  ok(true, "The panel update event was fired");

  is(panel.playersEl.querySelectorAll(".player-widget").length, 0,
    "There are no player widgets in the panel anymore");

  info("Add an finite animation on the node again, and wait for it to appear");
  onPanelUpdated = panel.once(panel.UI_UPDATED_EVENT);
  yield executeInContent("devtools:test:setAttribute", {
    selector: ".test-node",
    attributeName: "class",
    attributeValue: "ball short"
  });
  yield onPanelUpdated;
  is(panel.playersEl.querySelectorAll(".player-widget").length, 1,
    "There is one player widget in the panel again");

  info("Now wait until the animation finishes");
  let widget = panel.playerWidgets[0];
  yield waitForPlayState(widget.player, "finished")

  is(panel.playersEl.querySelectorAll(".player-widget").length, 1,
    "There is still a player widget in the panel after the animation finished");

  info("Checking that the animation's currentTime can still be set");
  info("Click at the center of the slider input");

  let onPaused = waitForPlayState(widget.player, "paused");
  let input = widget.currentTimeEl;
  let win = input.ownerDocument.defaultView;
  EventUtils.synthesizeMouseAtCenter(input, {type: "mousedown"}, win);
  yield onPaused;
  ok(widget.el.classList.contains("paused"), "The widget is in paused mode");
});
