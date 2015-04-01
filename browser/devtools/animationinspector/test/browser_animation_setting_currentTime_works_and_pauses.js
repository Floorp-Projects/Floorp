/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that setting an animation's current time by clicking in the input[range]
// or rewind or fast-forward buttons pauses the animation and sets it to the
// right time.

add_task(function*() {
  yield addTab(TEST_URL_ROOT + "doc_simple_animation.html");
  let {toolbox, inspector, panel} = yield openAnimationInspector();

  info("Select an animated node");
  yield selectNode(".animated", inspector);

  info("Get the player widget for this node");
  let widget = panel.playerWidgets[0];
  let input = widget.currentTimeEl;
  let rwBtn = widget.rewindBtnEl;
  let ffBtn = widget.fastForwardBtnEl;
  let win = input.ownerDocument.defaultView;

  info("Click at the center of the input")
  EventUtils.synthesizeMouseAtCenter(input, {type: "mousedown"}, win);

  yield checkPausedAt(widget, 1000);

  info("Resume the player and wait for an auto-refresh event");
  yield widget.player.play();
  yield onceNextPlayerRefresh(widget.player);

  info("Click on the rewind button");
  EventUtils.sendMouseEvent({type: "click"}, rwBtn, win);

  yield checkPausedAt(widget, 0);

  info("Click on the fast-forward button");
  EventUtils.sendMouseEvent({type: "click"}, ffBtn, win);

  yield checkPausedAt(widget, 2000);
});
