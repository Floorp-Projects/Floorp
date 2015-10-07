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
// List of selectors that match some animated nodes in the test page only.
const SOME_ANIMATED_NODES = [".simple-animation", ".delayed-animation"];

add_task(function*() {
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

  info("Pause a given list of animations only");
  let players = [];
  for (let selector of SOME_ANIMATED_NODES) {
    let [player] = yield getPlayersFor(walker, animations, selector);
    players.push(player);
  }
  yield animations.toggleSeveral(players, true);
  yield checkStates(walker, animations, SOME_ANIMATED_NODES, "paused");
  yield checkStates(walker, animations, [".multiple-animations"], "running");

  info("Play the same list of animations");
  yield animations.toggleSeveral(players, false);
  yield checkStates(walker, animations, ALL_ANIMATED_NODES, "running");

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
