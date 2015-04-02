/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that setting an animation's playback rate by selecting a rate in the
// presets drop-down sets the rate accordingly.

add_task(function*() {
  yield addTab(TEST_URL_ROOT + "doc_simple_animation.html");
  let {toolbox, inspector, panel} = yield openAnimationInspector();

  info("Select an animated node");
  yield selectNode(".animated", inspector);

  info("Get the player widget for this node");
  let widget = panel.playerWidgets[0];
  let select = widget.rateComponent.el;
  let win = select.ownerDocument.defaultView;

  info("Click on the rate drop-down");
  EventUtils.synthesizeMouseAtCenter(select, {type: "mousedown"}, win);

  info("Click on a rate option");
  let option = select.options[select.options.length - 1];
  EventUtils.synthesizeMouseAtCenter(option, {type: "mouseup"}, win);
  let selectedRate = parseFloat(option.value);

  info("Check that the rate was changed on the player at the next update");
  yield waitForStateCondition(widget.player, ({playbackRate}) => playbackRate === selectedRate);
  is(widget.player.state.playbackRate, selectedRate,
    "The rate was changed successfully");
});
