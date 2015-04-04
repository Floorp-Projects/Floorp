/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that setting an animation's playbackRate via the WebAnimations API (from
// content), actually changes the rate in the corresponding widget too.

add_task(function*() {
  yield addTab(TEST_URL_ROOT + "doc_simple_animation.html");
  let {inspector, panel} = yield openAnimationInspector();

  info("Selecting the test node");
  yield selectNode(".animated", inspector);

  info("Get the player widget");
  let widget = panel.playerWidgets[0];

  info("Change the animation's playbackRate via the content DOM");
  let onRateChanged = waitForStateCondition(widget.player, state => {
    return state.playbackRate === 2;
  }, "playbackRate === 2");
  yield executeInContent("Test:SetAnimationPlayerPlaybackRate", {
    selector: ".animated",
    animationIndex: 0,
    playbackRate: 2
  });
  yield onRateChanged;

  is(widget.rateComponent.el.value, "2",
    "The playbackRate select value was changed");

  info("Change the animation's playbackRate again via the content DOM");
  onRateChanged = waitForStateCondition(widget.player, state => {
    return state.playbackRate === 0.3;
  }, "playbackRate === 0.3");
  yield executeInContent("Test:SetAnimationPlayerPlaybackRate", {
    selector: ".animated",
    animationIndex: 0,
    playbackRate: 0.3
  });
  yield onRateChanged;

  is(widget.rateComponent.el.value, "0.3",
    "The playbackRate select value was changed again");
});
