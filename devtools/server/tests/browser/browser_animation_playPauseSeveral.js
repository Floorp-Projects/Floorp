/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check that the AnimationsActor can pause/play all animations at once, and
// check that it can also pause/play a given list of animations at once.

// List of selectors that match "all" animated nodes in the test page.
// This list misses a bunch of animated nodes on purpose. Only the ones that
// have infinite animations are listed. This is done to avoid intermittents
// caused when finite animations are already done playing by the time the test
// runs.
const ALL_ANIMATED_NODES = [".simple-animation", ".multiple-animations",
                            ".delayed-animation"];

add_task(function* () {
  let {client, walker, animations} =
    yield initAnimationsFrontForUrl(MAIN_DOMAIN + "animation.html");

  info("Pause all animations in the test document");
  yield animations.pauseAll();
  yield checkStates(walker, animations, ALL_ANIMATED_NODES, "paused");

  info("Play all animations in the test document");
  yield animations.playAll();
  yield checkStates(walker, animations, ALL_ANIMATED_NODES, "running");

  info("Pause all animations in the test document using toggleAll");
  yield animations.toggleAll();
  yield checkStates(walker, animations, ALL_ANIMATED_NODES, "paused");

  info("Play all animations in the test document using toggleAll");
  yield animations.toggleAll();
  yield checkStates(walker, animations, ALL_ANIMATED_NODES, "running");

  info("Play all animations from multiple animated node using toggleSeveral");
  let players = yield getPlayersFor(walker, animations,
                                   [".multiple-animations"]);
  is(players.length, 2, "Node has 2 animation players");
  yield animations.toggleSeveral(players, false);
  let state1 = yield players[0].getCurrentState();
  is(state1.playState, "running",
    "The playState of the first player is running");
  let state2 = yield players[1].getCurrentState();
  is(state2.playState, "running",
    "The playState of the second player is running");

  info("Pause one animation from a multiple animated node using toggleSeveral");
  yield animations.toggleSeveral([players[0]], true);
  state1 = yield players[0].getCurrentState();
  is(state1.playState, "paused", "The playState of the first player is paused");
  state2 = yield players[1].getCurrentState();
  is(state2.playState, "running",
    "The playState of the second player is running");

  info("Play the same animation");
  yield animations.toggleSeveral([players[0]], false);
  state1 = yield players[0].getCurrentState();
  is(state1.playState, "running",
    "The playState of the first player is running");
  state2 = yield players[1].getCurrentState();
  is(state2.playState, "running",
    "The playState of the second player is running");

  yield closeDebuggerClient(client);
  gBrowser.removeCurrentTab();
});

function* checkStates(walker, animations, selectors, playState) {
  info("Checking the playState of all the nodes that have infinite running " +
       "animations");

  for (let selector of selectors) {
    info("Getting the AnimationPlayerFront for node " + selector);
    let [player] = yield getPlayersFor(walker, animations, selector);
    yield player.ready();
    yield checkPlayState(player, selector, playState);
  }
}

function* getPlayersFor(walker, animations, selector) {
  let node = yield walker.querySelector(walker.rootNode, selector);
  return animations.getAnimationPlayersForNode(node);
}

function* checkPlayState(player, selector, expectedState) {
  let state = yield player.getCurrentState();
  is(state.playState, expectedState,
    "The playState of node " + selector + " is " + expectedState);
}
