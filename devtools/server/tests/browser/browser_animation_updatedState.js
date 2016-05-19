/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check the animation player's updated state

add_task(function* () {
  let {client, walker, animations} =
    yield initAnimationsFrontForUrl(MAIN_DOMAIN + "animation.html");

  yield playStateIsUpdatedDynamically(walker, animations);

  yield closeDebuggerClient(client);
  gBrowser.removeCurrentTab();
});

function* playStateIsUpdatedDynamically(walker, animations) {
  info("Getting the test node (which runs a very long animation)");
  // The animation lasts for 100s, to avoid intermittents.
  let node = yield walker.querySelector(walker.rootNode, ".long-animation");

  info("Getting the animation player front for this node");
  let [player] = yield animations.getAnimationPlayersForNode(node);
  yield player.ready();

  let state = yield player.getCurrentState();
  is(state.playState, "running",
    "The playState is running while the animation is running");

  info("Change the animation's currentTime to be near the end and wait for " +
       "it to finish");
  let onFinished = waitForAnimationPlayState(player, "finished");
  // Set the currentTime to 98s, knowing that the animation lasts for 100s.
  yield player.setCurrentTime(98 * 1000);
  state = yield onFinished;
  is(state.playState, "finished",
    "The animation has ended and the state has been updated");
  ok(state.currentTime > player.initialState.currentTime,
    "The currentTime has been updated");
}

function* waitForAnimationPlayState(player, playState) {
  let state = {};
  while (state.playState !== playState) {
    state = yield player.getCurrentState();
    yield wait(500);
  }
  return state;
}

function wait(ms) {
  return new Promise(r => setTimeout(r, ms));
}
